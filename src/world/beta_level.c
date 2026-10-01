#include "beta_level.h"
#include "beta_level_io.h"
#include "../nbt/nbt.h"
#include "../util/game_paths.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <zlib.h>
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif

#define LEVEL_OUTPUT_LIMIT (4u*1024u*1024u)
#define LEVEL_EVENT_LIMIT 8192u
#define LEVEL_ITEM_LIMIT 256u

typedef struct SavedTag {
    NbtEvent event;
    NbtTag tag;
    unsigned depth;
} SavedTag;

typedef struct Capture {
    SavedTag *tags;
    size_t used,capacity,item_start;
    size_t starts[LEVEL_ITEM_LIMIT],lengths[LEVEL_ITEM_LIMIT];
    unsigned preserved;
    int data,player,inventory,item,slot;
    int has_player,has_position,has_rotation,has_inventory,has_time;
    int failed;
} Capture;

typedef struct Rewrite {
    NbtWriter writer;
    const BetaLevelState *state;
    const Capture *capture;
    int data,player,skip_inventory,position,motion,rotation;
    unsigned coordinate,motion_index,angle;
    int64_t last_played;
} Rewrite;

static int named(const NbtTag *tag,const char *name)
{
    size_t n=strlen(name);
    return tag->name.size==n && memcmp(tag->name.data,name,n)==0;
}

static int capture_tag(void *context,NbtEvent event,const NbtTag *tag,unsigned depth)
{
    Capture *capture=(Capture *)context;
    if (depth==1 && tag->type==NBT_COMPOUND && named(tag,"Data"))
        capture->data=event==NBT_BEGIN;
    if (!capture->data) return 1;
    if (event==NBT_BEGIN && depth==2 && tag->type==NBT_COMPOUND && named(tag,"Player")) {
        capture->player=1;
        capture->has_player=1;
    }
    if (event==NBT_VALUE && depth==2 && tag->type==NBT_LONG && named(tag,"Time"))
        capture->has_time=1;
    if (capture->player && event==NBT_BEGIN && depth==3 && tag->type==NBT_LIST) {
        if (named(tag,"Pos") && tag->list_type==NBT_DOUBLE && tag->count==3)
            capture->has_position=1;
        else if (named(tag,"Rotation") && tag->list_type==NBT_FLOAT && tag->count==2)
            capture->has_rotation=1;
        else if (named(tag,"Inventory") && tag->list_type==NBT_COMPOUND) {
            capture->inventory=1;
            capture->has_inventory=1;
        }
    }
    if (capture->inventory && event==NBT_BEGIN && depth==4 && tag->type==NBT_COMPOUND) {
        capture->item=1;
        capture->item_start=capture->used;
        capture->slot=-1;
    }
    if (capture->item) {
        SavedTag *saved;
        if (capture->used==capture->capacity) {
            size_t capacity=capture->capacity ? capture->capacity*2u : 128u;
            SavedTag *grown;
            if (capacity>LEVEL_EVENT_LIMIT) { capture->failed=1; return 0; }
            grown=(SavedTag *)realloc(capture->tags,capacity*sizeof(*grown));
            if (!grown) { capture->failed=1; return 0; }
            capture->tags=grown;
            capture->capacity=capacity;
        }
        saved=&capture->tags[capture->used++];
        saved->event=event;
        saved->tag=*tag;
        saved->depth=depth;
        if (event==NBT_VALUE && depth==5 && tag->type==NBT_BYTE && named(tag,"Slot"))
            capture->slot=(uint8_t)tag->value.byte;
        if (event==NBT_FINISH && depth==4 && tag->type==NBT_COMPOUND) {
            if (capture->slot>=0 && capture->slot<RECRAFT_INVENTORY_SLOTS)
                capture->used=capture->item_start;
            else if (capture->preserved<LEVEL_ITEM_LIMIT) {
                capture->starts[capture->preserved]=capture->item_start;
                capture->lengths[capture->preserved]=capture->used-capture->item_start;
                ++capture->preserved;
            } else { capture->failed=1; return 0; }
            capture->item=0;
        }
    }
    if (event==NBT_FINISH && depth==3 && tag->type==NBT_LIST && named(tag,"Inventory"))
        capture->inventory=0;
    if (event==NBT_FINISH && depth==2 && tag->type==NBT_COMPOUND && named(tag,"Player"))
        capture->player=0;
    return 1;
}

static NbtResult emit_item(NbtWriter *writer,int slot,const InventorySlot *item)
{
    NbtTag tag;
    memset(&tag,0,sizeof(tag));
    tag.type=NBT_COMPOUND;
    if (nbt_writer_tag(writer,&tag)!=NBT_OK) return writer->result;
    tag.type=NBT_BYTE; tag.name=nbt_span("Slot"); tag.value.byte=(int8_t)slot;
    if (nbt_writer_tag(writer,&tag)!=NBT_OK) return writer->result;
    tag.type=NBT_SHORT; tag.name=nbt_span("id"); tag.value.short_value=(int16_t)item->id;
    if (nbt_writer_tag(writer,&tag)!=NBT_OK) return writer->result;
    tag.type=NBT_BYTE; tag.name=nbt_span("Count"); tag.value.byte=(int8_t)item->count;
    if (nbt_writer_tag(writer,&tag)!=NBT_OK) return writer->result;
    tag.type=NBT_SHORT; tag.name=nbt_span("Damage"); tag.value.short_value=(int16_t)item->damage;
    if (nbt_writer_tag(writer,&tag)!=NBT_OK) return writer->result;
    return nbt_writer_end(writer);
}

static int rewrite_tag(void *context,NbtEvent event,const NbtTag *tag,unsigned depth)
{
    Rewrite *rewrite=(Rewrite *)context;
    NbtWriter *writer=&rewrite->writer;
    NbtTag changed=*tag;
    unsigned i,j;
    if (depth==1 && tag->type==NBT_COMPOUND && named(tag,"Data"))
        rewrite->data=event==NBT_BEGIN;
    if (!rewrite->data)
        return (event==NBT_FINISH ? nbt_writer_end(writer) :
                nbt_writer_tag(writer,tag))==NBT_OK;
    if (rewrite->skip_inventory) {
        if (event==NBT_FINISH && depth==3 && tag->type==NBT_LIST && named(tag,"Inventory")) {
            rewrite->skip_inventory=0;
            return nbt_writer_end(writer)==NBT_OK;
        }
        return 1;
    }
    if (event==NBT_BEGIN && depth==2 && tag->type==NBT_COMPOUND && named(tag,"Player"))
        rewrite->player=1;
    if (rewrite->player && event==NBT_BEGIN && depth==3 && tag->type==NBT_LIST) {
        if (named(tag,"Pos")) { rewrite->position=1; rewrite->coordinate=0; }
        else if (named(tag,"Motion")) { rewrite->motion=1; rewrite->motion_index=0; }
        else if (named(tag,"Rotation")) { rewrite->rotation=1; rewrite->angle=0; }
        else if (named(tag,"Inventory")) {
            unsigned count=rewrite->capture->preserved;
            for (i=0;i<RECRAFT_INVENTORY_SLOTS;++i)
                if (rewrite->state->inventory[i].id>0 && rewrite->state->inventory[i].count>0) ++count;
            changed.count=count;
            if (nbt_writer_tag(writer,&changed)!=NBT_OK) return 0;
            for (i=0;i<RECRAFT_INVENTORY_SLOTS;++i)
                if (rewrite->state->inventory[i].id>0 && rewrite->state->inventory[i].count>0 &&
                    emit_item(writer,(int)i,&rewrite->state->inventory[i])!=NBT_OK) return 0;
            for (i=0;i<rewrite->capture->preserved;++i)
                for (j=0;j<rewrite->capture->lengths[i];++j) {
                    const SavedTag *saved=&rewrite->capture->tags[rewrite->capture->starts[i]+j];
                    NbtResult result=saved->event==NBT_FINISH ? nbt_writer_end(writer) :
                        nbt_writer_tag(writer,&saved->tag);
                    if (result!=NBT_OK) return 0;
                }
            rewrite->skip_inventory=1;
            return 1;
        }
    }
    if (event==NBT_VALUE) {
        if (depth==2 && tag->type==NBT_LONG && named(tag,"Time"))
            changed.value.long_value=rewrite->state->world_time;
        else if (depth==2 && tag->type==NBT_LONG && named(tag,"LastPlayed"))
            changed.value.long_value=rewrite->last_played;
        else if (rewrite->position && depth==4 && tag->type==NBT_DOUBLE) {
            double position[3]={rewrite->state->x,rewrite->state->y,rewrite->state->z};
            if (rewrite->coordinate<3) changed.value.double_value=position[rewrite->coordinate++];
        } else if (rewrite->motion && depth==4 && tag->type==NBT_DOUBLE) {
            double motion[3]={rewrite->state->motion_x,rewrite->state->motion_y,
                              rewrite->state->motion_z};
            if (rewrite->motion_index<3)
                changed.value.double_value=motion[rewrite->motion_index++];
        } else if (rewrite->rotation && depth==4 && tag->type==NBT_FLOAT) {
            float rotation[2]={rewrite->state->yaw,rewrite->state->pitch};
            if (rewrite->angle<2) changed.value.float_value=rotation[rewrite->angle++];
        }
        if (rewrite->player && depth==3 && tag->type==NBT_BYTE &&
            named(tag,"OnGround")) changed.value.byte=(int8_t)(rewrite->state->on_ground!=0);
        if (rewrite->player && depth==3 && tag->type==NBT_FLOAT &&
            named(tag,"FallDistance")) changed.value.float_value=0.0f;
    }
    if (event==NBT_FINISH && depth==3 && tag->type==NBT_LIST) {
        if (named(tag,"Pos")) rewrite->position=0;
        if (named(tag,"Motion")) rewrite->motion=0;
        if (named(tag,"Rotation")) rewrite->rotation=0;
    }
    if (event==NBT_FINISH && depth==2 && tag->type==NBT_COMPOUND && named(tag,"Player"))
        rewrite->player=0;
    return (event==NBT_FINISH ? nbt_writer_end(writer) :
            nbt_writer_tag(writer,&changed))==NBT_OK;
}

static int copy_once(const char *source,const char *backup,const char *temporary)
{
    FILE *in,*out;
    unsigned char bytes[8192];
    size_t count;
    int valid=1;
    in=fopen(backup,"rb");
    if (in) { fclose(in); return 1; }
    in=fopen(source,"rb");
    if (!in) return 0;
    out=fopen(temporary,"wb");
    if (!out) { fclose(in); return 0; }
    while ((count=fread(bytes,1,sizeof(bytes),in))>0)
        if (fwrite(bytes,1,count,out)!=count) { valid=0; break; }
    if (ferror(in)) valid=0;
    if (fclose(in)!=0) valid=0;
    if (fclose(out)!=0) valid=0;
    if (!valid) { remove(temporary); return 0; }
#ifdef _WIN32
    valid=MoveFileExA(temporary,backup,MOVEFILE_WRITE_THROUGH)!=0;
#else
    valid=rename(temporary,backup)==0;
#endif
    return valid;
}

static int replace_file(const char *source,const char *target)
{
#ifdef _WIN32
    return MoveFileExA(source,target,MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH)!=0;
#else
    return rename(source,target)==0;
#endif
}

int beta_level_save(const char *world_path,const BetaLevelState *state)
{
    char path[512],old[512],temp[512],backup[512],backup_temp[512];
    gzFile file;
    unsigned char *input=NULL,*output=NULL;
    size_t size=0,written=0;
    int n,close_status,valid=0,i,used_old=0;
    Capture capture;
    Rewrite rewrite;
    if (!world_path || !state || !isfinite(state->x) || !isfinite(state->y) ||
        !isfinite(state->z) || !isfinite(state->motion_x) ||
        !isfinite(state->motion_y) || !isfinite(state->motion_z) ||
        !isfinite(state->yaw) || !isfinite(state->pitch) ||
        !game_path_join(path,sizeof(path),world_path,"level.dat") ||
        !game_path_join(old,sizeof(old),world_path,"level.dat_old") ||
        !game_path_join(temp,sizeof(temp),world_path,"level.dat_new") ||
        !game_path_join(backup,sizeof(backup),world_path,"level.dat.recraft.bak") ||
        !game_path_join(backup_temp,sizeof(backup_temp),world_path,
                        "level.dat.recraft.bak.tmp")) return 0;
    for (i=0;i<RECRAFT_INVENTORY_SLOTS;++i) {
        const InventorySlot *slot=&state->inventory[i];
        if (slot->id>32767 || slot->count<0 || slot->count>64 ||
            slot->damage<0 || slot->damage>65535) return 0;
    }
    if (!beta_level_read(world_path,&input,&size,&used_old)) return 0;
    memset(&capture,0,sizeof(capture));
    if (nbt_read(input,size,NULL,capture_tag,&capture,NULL)!=NBT_OK ||
        capture.failed || !capture.has_player || !capture.has_position ||
        !capture.has_rotation || !capture.has_inventory || !capture.has_time) {
        free(capture.tags); goto done;
    }
    output=(unsigned char *)malloc(LEVEL_OUTPUT_LIMIT);
    if (!output) { free(capture.tags); goto done; }
    memset(&rewrite,0,sizeof(rewrite));
    rewrite.state=state; rewrite.capture=&capture;
    rewrite.last_played=(int64_t)time(NULL)*1000;
    nbt_writer_init(&rewrite.writer,output,LEVEL_OUTPUT_LIMIT,NULL);
    valid=nbt_read(input,size,NULL,rewrite_tag,&rewrite,NULL)==NBT_OK &&
          nbt_writer_finish(&rewrite.writer,&written)==NBT_OK &&
          nbt_read(output,written,NULL,NULL,NULL,NULL)==NBT_OK;
    free(capture.tags);
    if (!valid) goto done;
    valid=0;
    file=gzopen(temp,"wb6");
    if (!file) goto done;
    n=(written>0 && written<=LEVEL_OUTPUT_LIMIT) ?
        gzwrite(file,output,(unsigned)written) : -1;
    close_status=gzclose(file);
    valid=n==(int)written && close_status==Z_OK;
    if (!valid || !copy_once(used_old?old:path,backup,backup_temp)) { valid=0; goto done; }
    /* Rotate the last valid primary like Beta SaveHandler. When recovering,
     * keep the valid old file instead of replacing it with a damaged primary. */
    if (!used_old && !replace_file(path,old)) { valid=0; goto done; }
    valid=replace_file(temp,path);
done:
    free(input); free(output);
    return valid;
}
