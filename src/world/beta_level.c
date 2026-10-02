#include "beta_level.h"
#include "beta_level_io.h"
#include "beta_session.h"
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
    int has_motion,has_health,has_air,has_fire;
    int has_ground,has_fall;
    unsigned environment,bed;
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
    if(event==NBT_VALUE && depth==2) {
        if(tag->type==NBT_INT && named(tag,"rainTime")) capture->environment|=1;
        if(tag->type==NBT_INT && named(tag,"thunderTime")) capture->environment|=2;
        if(tag->type==NBT_BYTE && named(tag,"raining")) capture->environment|=4;
        if(tag->type==NBT_BYTE && named(tag,"thundering")) capture->environment|=8;
    }
    if(capture->player && event==NBT_VALUE && depth==3 && tag->type==NBT_INT) {
        if(named(tag,"SpawnX")) capture->bed|=1;
        if(named(tag,"SpawnY")) capture->bed|=2;
        if(named(tag,"SpawnZ")) capture->bed|=4;
    }
    if (capture->player && event==NBT_BEGIN && depth==3 && tag->type==NBT_LIST) {
        if (named(tag,"Pos") && tag->list_type==NBT_DOUBLE && tag->count==3)
            capture->has_position=1;
        else if (named(tag,"Rotation") && tag->list_type==NBT_FLOAT && tag->count==2)
            capture->has_rotation=1;
        else if (named(tag,"Inventory") && tag->list_type==NBT_COMPOUND) {
            capture->inventory=1;
            capture->has_inventory=1;
        }
        else if (named(tag,"Motion") && tag->list_type==NBT_DOUBLE && tag->count==3)
            capture->has_motion=1;
    }
    if (capture->player && event==NBT_VALUE && depth==3 && tag->type==NBT_SHORT) {
        if (named(tag,"Health")) capture->has_health=1;
        if (named(tag,"Air")) capture->has_air=1;
        if (named(tag,"Fire")) capture->has_fire=1;
    }
    if (capture->player && event==NBT_VALUE && depth==3) {
        if (tag->type==NBT_BYTE && named(tag,"OnGround")) capture->has_ground=1;
        if (tag->type==NBT_FLOAT && named(tag,"FallDistance")) capture->has_fall=1;
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

static int emit_scalar(NbtWriter *w,NbtType type,const char *name,int64_t value)
{
    NbtTag tag; memset(&tag,0,sizeof(tag)); tag.type=type; tag.name=nbt_span(name);
    if (type==NBT_SHORT) tag.value.short_value=(int16_t)value;
    else if (type==NBT_BYTE) tag.value.byte=(int8_t)value;
    else if (type==NBT_FLOAT) tag.value.float_value=(float)value;
    else if (type==NBT_INT) tag.value.int_value=(int32_t)value;
    else if (type==NBT_LONG) tag.value.long_value=value;
    return nbt_writer_tag(w,&tag)==NBT_OK;
}
static int emit_numbers(NbtWriter *w,const char *name,const double *values,unsigned count,int angles)
{
    NbtTag tag; unsigned i;
    memset(&tag,0,sizeof(tag)); tag.type=NBT_LIST; tag.name=nbt_span(name);
    tag.list_type=angles ? NBT_FLOAT : NBT_DOUBLE; tag.count=count;
    if (nbt_writer_tag(w,&tag)!=NBT_OK) return 0;
    memset(&tag,0,sizeof(tag)); tag.type=angles ? NBT_FLOAT : NBT_DOUBLE;
    for (i=0;i<count;++i) {
        if (angles) tag.value.float_value=(float)values[i]; else tag.value.double_value=values[i];
        if (nbt_writer_tag(w,&tag)!=NBT_OK) return 0;
    }
    return nbt_writer_end(w)==NBT_OK;
}
static int emit_missing_player(Rewrite *r)
{
    const BetaLevelState *s=r->state; const Capture *c=r->capture;
    double position[3]={s->x,s->y,s->z},motion[3]={s->motion_x,s->motion_y,s->motion_z},angle[2]={s->yaw,s->pitch};
    NbtWriter *w=&r->writer; NbtTag tag; unsigned i,count=0;
    if (!c->has_position && !emit_numbers(w,"Pos",position,3,0)) return 0;
    if (!c->has_motion && !emit_numbers(w,"Motion",motion,3,0)) return 0;
    if (!c->has_rotation && !emit_numbers(w,"Rotation",angle,2,1)) return 0;
    if (!c->has_inventory) {
        for (i=0;i<36;++i) if (s->inventory[i].id>0 && s->inventory[i].count>0) ++count;
        memset(&tag,0,sizeof(tag)); tag.type=NBT_LIST; tag.name=nbt_span("Inventory");
        tag.list_type=NBT_COMPOUND; tag.count=count;
        if (nbt_writer_tag(w,&tag)!=NBT_OK) return 0;
        for (i=0;i<36;++i) if (s->inventory[i].id>0 && s->inventory[i].count>0 && emit_item(w,(int)i,&s->inventory[i])!=NBT_OK) return 0;
        if (nbt_writer_end(w)!=NBT_OK) return 0;
    }
    if (!c->has_health && !emit_scalar(w,NBT_SHORT,"Health",s->has_vitals ? s->health : 20)) return 0;
    if (!c->has_air && !emit_scalar(w,NBT_SHORT,"Air",s->has_vitals ? s->air : 300)) return 0;
    if (!c->has_fire && !emit_scalar(w,NBT_SHORT,"Fire",s->has_vitals ? s->fire : 0)) return 0;
    if (!c->has_ground && !emit_scalar(w,NBT_BYTE,"OnGround",s->on_ground!=0)) return 0;
    if (!c->has_fall && !emit_scalar(w,NBT_FLOAT,"FallDistance",0)) return 0;
    if (!c->has_player && !emit_scalar(w,NBT_INT,"Dimension",0)) return 0;
    if(s->has_bed) {
        if(!(c->bed&1) && !emit_scalar(w,NBT_INT,"SpawnX",s->bed_x)) return 0;
        if(!(c->bed&2) && !emit_scalar(w,NBT_INT,"SpawnY",s->bed_y)) return 0;
        if(!(c->bed&4) && !emit_scalar(w,NBT_INT,"SpawnZ",s->bed_z)) return 0;
    }
    return 1;
}
static int rewrite_tag(void *context,NbtEvent event,const NbtTag *tag,unsigned depth)
{
    Rewrite *rewrite=(Rewrite *)context;
    NbtWriter *writer=&rewrite->writer;
    NbtTag changed=*tag;
    unsigned i,j;
    if (depth==1 && event==NBT_BEGIN && tag->type==NBT_COMPOUND && named(tag,"Data"))
        rewrite->data=1;
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
        const BetaLevelState *s=rewrite->state;
        if(s->has_environment && depth==2) {
            if(tag->type==NBT_INT && named(tag,"rainTime")) changed.value.int_value=s->rain_time;
            if(tag->type==NBT_INT && named(tag,"thunderTime")) changed.value.int_value=s->thunder_time;
            if(tag->type==NBT_BYTE && named(tag,"raining")) changed.value.byte=(int8_t)(s->raining!=0);
            if(tag->type==NBT_BYTE && named(tag,"thundering")) changed.value.byte=(int8_t)(s->thundering!=0);
        }
        if(rewrite->player && depth==3) {
            if(tag->type==NBT_BYTE && named(tag,"Sleeping")) changed.value.byte=0;
            if(tag->type==NBT_SHORT && named(tag,"SleepTimer")) changed.value.short_value=0;
            if(s->has_bed && tag->type==NBT_INT) {
                if(named(tag,"SpawnX")) changed.value.int_value=s->bed_x;
                if(named(tag,"SpawnY")) changed.value.int_value=s->bed_y;
                if(named(tag,"SpawnZ")) changed.value.int_value=s->bed_z;
            }
        }
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
        if (rewrite->state->has_vitals && rewrite->player && depth==3 && tag->type==NBT_SHORT) {
            if (named(tag,"Health")) changed.value.short_value=(int16_t)rewrite->state->health;
            if (named(tag,"Air")) changed.value.short_value=(int16_t)rewrite->state->air;
            if (named(tag,"Fire")) changed.value.short_value=(int16_t)rewrite->state->fire;
        }
    }
    if (event==NBT_FINISH && depth==3 && tag->type==NBT_LIST) {
        if (named(tag,"Pos")) rewrite->position=0;
        if (named(tag,"Motion")) rewrite->motion=0;
        if (named(tag,"Rotation")) rewrite->rotation=0;
    }
    if (event==NBT_FINISH && depth==2 && tag->type==NBT_COMPOUND && named(tag,"Player")) {
        if (!emit_missing_player(rewrite)) return 0;
        rewrite->player=0;
    }
    if (event==NBT_FINISH && depth==1 && tag->type==NBT_COMPOUND && named(tag,"Data")) {
        if (!rewrite->capture->has_player) {
            NbtTag player; memset(&player,0,sizeof(player)); player.type=NBT_COMPOUND; player.name=nbt_span("Player");
            if (nbt_writer_tag(writer,&player)!=NBT_OK || !emit_missing_player(rewrite) || nbt_writer_end(writer)!=NBT_OK) return 0;
        }
        if (!rewrite->capture->has_time && !emit_scalar(writer,NBT_LONG,"Time",rewrite->state->world_time)) return 0;
        if(rewrite->state->has_environment) {
            const BetaLevelState *s=rewrite->state; unsigned e=rewrite->capture->environment;
            if(!(e&1) && !emit_scalar(writer,NBT_INT,"rainTime",s->rain_time)) return 0;
            if(!(e&2) && !emit_scalar(writer,NBT_INT,"thunderTime",s->thunder_time)) return 0;
            if(!(e&4) && !emit_scalar(writer,NBT_BYTE,"raining",s->raining!=0)) return 0;
            if(!(e&8) && !emit_scalar(writer,NBT_BYTE,"thundering",s->thundering!=0)) return 0;
        }
        rewrite->data=0;
    }
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
    if(state->session && !beta_session_check(world_path,state->session)) return 0;
    for (i=0;i<RECRAFT_INVENTORY_SLOTS;++i) {
        const InventorySlot *slot=&state->inventory[i];
        if (slot->id>32767 || slot->count<0 || slot->count>255 ||
            slot->damage<0 || slot->damage>65535) return 0;
    }
    if (!beta_level_read(world_path,&input,&size,&used_old)) return 0;
    memset(&capture,0,sizeof(capture));
    if (nbt_read(input,size,NULL,capture_tag,&capture,NULL)!=NBT_OK ||
        capture.failed) {
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
    if (!valid || (state->session && !beta_session_check(world_path,state->session)) ||
        !copy_once(used_old?old:path,backup,backup_temp)) { valid=0; goto done; }
    /* Rotate the last valid primary like Beta SaveHandler. When recovering,
     * keep the valid old file instead of replacing it with a damaged primary. */
    if (!used_old && !replace_file(path,old)) { valid=0; goto done; }
    valid=replace_file(temp,path);
done:
    free(input); free(output);
    return valid;
}
