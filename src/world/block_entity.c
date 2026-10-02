#include "block_entity.h"
#include "world.h"
#include "entities.h"
#include "ticks.h"
#include "../game/crafting.h"
#include "../game/sign.h"
#include "../nbt/nbt.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif

static int named(const NbtTag *tag,const char *s)
{ return tag->name.size==strlen(s) && !memcmp(tag->name.data,s,tag->name.size); }
static int text_is(NbtSpan value,const char *s)
{ return value.size==strlen(s) && !memcmp(value.data,s,value.size); }
static Chunk *owner(World *world,int x,int z)
{
    int cx=x>=0 ? x/16 : (int)(-((-(int64_t)x+15)/16));
    int cz=z>=0 ? z/16 : (int)(-((-(int64_t)z+15)/16));
    return world_get_chunk(world,cx,cz);
}
static void free_entity(BlockEntity *e) { if (e) { free(e->raw); free(e); } }
void block_entities_free(Chunk *chunk)
{
    BlockEntity *e,*next;
    if (!chunk) return;
    for (e=chunk->entities;e;e=next) { next=e->next; free_entity(e); }
    chunk->entities=NULL;
}
BlockEntity *block_entity_get(World *world,int x,int y,int z,int create)
{
    Chunk *chunk;
    BlockEntity *e;
    uint8_t block;
    int i;
    if (!world || (unsigned)y>=WORLD_HEIGHT) return NULL;
    chunk=owner(world,x,z);
    if (!chunk) return NULL;
    for (e=chunk->entities;e;e=e->next) if (e->x==x && e->y==y && e->z==z) return e;
    block=world_peek_block(world,x,y,z);
    if (!create || (world->network_mode && !sign_is_block(block)) ||
        (world->beta_format && !chunk->beta_raw)) return NULL;
    if (block!=54 && block!=61 && block!=62 && block!=84 && !sign_is_block(block)) return NULL;
    e=(BlockEntity *)calloc(1,sizeof(*e));
    if (!e) return NULL;
    e->x=x; e->y=y; e->z=z;
    e->kind=block==54 ? BLOCK_ENTITY_CHEST : block==84 ? BLOCK_ENTITY_JUKEBOX : sign_is_block(block) ? BLOCK_ENTITY_SIGN : BLOCK_ENTITY_FURNACE;
    for (i=0;i<27;++i) inventory_clear_slot(&e->slots[i]);
    e->next=chunk->entities; chunk->entities=e;
    if (!world->network_mode) {
        chunk->entities_modified=1;
        chunk->dirty_flags|=CHUNK_DIRTY_SAVE|CHUNK_DIRTY_ENTITIES;
    }
    return e;
}
void block_entity_changed(World *world,BlockEntity *entity)
{
    Chunk *chunk;
    if (!world || !entity) return;
    chunk=owner(world,entity->x,entity->z);
    if (chunk) { chunk->entities_modified=1; chunk->dirty_flags|=CHUNK_DIRTY_SAVE|CHUNK_DIRTY_ENTITIES; }
}
int jukebox_use(World *w,int x,int y,int z,InventorySlot *held)
{
    BlockEntity *e;
    if(w->network_mode || world_peek_block(w,x,y,z)!=84 || !(e=block_entity_get(w,x,y,z,1))) return 0;
    if(e->record) {
        world_sound(w,"records.stop",x+.5f,y+.5f,z+.5f,1,1);
        world_drop_stack(w,x,y,z,(InventorySlot){e->record,1,0}); e->record=0;
    } else {
        if(!held || held->count<=0 || (held->id!=2256 && held->id!=2257)) return 0;
        e->record=held->id;
        world_sound(w,held->id==2256 ? "records.13" : "records.cat",x+.5f,y+.5f,z+.5f,1,1);
        if(!w->creative && --held->count==0) inventory_clear_slot(held);
    }
    world_set_metadata(w,x,y,z,(uint8_t)(e->record!=0)); block_entity_changed(w,e); return 1;
}
void block_entity_remove(World *world,int x,int y,int z,int drop_contents)
{
    Chunk *chunk=owner(world,x,z);
    BlockEntity **link;
    if (!chunk) return;
    for (link=&chunk->entities;*link;link=&(*link)->next) {
        BlockEntity *e=*link;
        int i;
        if (e->x!=x || e->y!=y || e->z!=z) continue;
        if(e->kind==BLOCK_ENTITY_JUKEBOX && e->record) {
            world_sound(world,"records.stop",x+.5f,y+.5f,z+.5f,1,1);
            if(drop_contents) world_drop_stack(world,x,y,z,(InventorySlot){e->record,1,0});
        }
        if (drop_contents && (e->kind==BLOCK_ENTITY_CHEST || e->kind==BLOCK_ENTITY_FURNACE))
            for (i=0;i<(e->kind==BLOCK_ENTITY_FURNACE ? 3 : 27);++i)
            if (e->slots[i].id>0 && e->slots[i].count>0)
                world_drop_stack(world,x,y,z,e->slots[i]);
        *link=e->next; free_entity(e); chunk->entities_modified=1;
        chunk->dirty_flags|=CHUNK_DIRTY_SAVE|CHUNK_DIRTY_ENTITIES; return;
    }
}
void block_entities_tick(World *world)
{
    size_t c;
    if (!world || world->network_mode) return;
    for (c=0;c<world->cache_count;++c) {
        Chunk *chunk=world->cache[c];
        BlockEntity *e;
        for (e=chunk->entities;e;e=e->next) if (e->kind==BLOCK_ENTITY_FURNACE) {
            InventorySlot result;
            int was=e->burn>0,can= e->slots[0].count>0 &&
                furnace_recipe(e->slots[0].id,&result);
            if (can && e->slots[2].count>0 && ( !inventory_same(&e->slots[2],&result) ||
                e->slots[2].count>=inventory_stack_limit(result.id))) can=0;
            if (e->burn>0) --e->burn;
            if (!e->burn && can && e->slots[1].count>0) {
                e->fuel=e->burn=furnace_fuel_ticks(e->slots[1].id);
                if (e->burn>0 && --e->slots[1].count==0) inventory_clear_slot(&e->slots[1]);
            }
            if (e->burn>0 && can) {
                if (++e->cook==200) {
                    e->cook=0;
                    if (e->slots[2].count<=0) e->slots[2]=result;
                    else ++e->slots[2].count;
                    if (--e->slots[0].count==0) inventory_clear_slot(&e->slots[0]);
                }
            } else e->cook=0;
            if (was!=(e->burn>0)) {
                uint8_t meta=world_get_metadata(world,e->x,e->y,e->z);
                world_set_block(world,e->x,e->y,e->z,(uint8_t)(e->burn>0 ? 62 : 61));
                world_set_metadata(world,e->x,e->y,e->z,meta);
            }
            if (was || e->burn>0 || e->cook>0) {
                chunk->entities_modified=1; chunk->dirty_flags|=CHUNK_DIRTY_SAVE|CHUNK_DIRTY_ENTITIES;
            }
        }
    }
}

int block_chest_can_place(World *world,int x,int y,int z)
{
    static const int dx[4]={-1,1,0,0},dz[4]={0,0,-1,1};
    int i,j,count=0;
    for (i=0;i<4;++i) if (world_get_block(world,x+dx[i],y,z+dz[i])==54) {
        ++count;
        for (j=0;j<4;++j) if (world_get_block(world,x+dx[i]+dx[j],y,z+dz[i]+dz[j])==54)
            return 0;
    }
    return count<=1;
}
int block_chest_halves(World *world,int x,int y,int z,BlockEntity **first,BlockEntity **second)
{
    static const int dx[4]={-1,1,0,0},dz[4]={0,0,-1,1};
    int i;
    if (!first || !second || world_get_block(world,x,y,z)!=54 ||
        world_block_def(world_get_block(world,x,y+1,z))->opaque) return 0;
    *first=block_entity_get(world,x,y,z,1); *second=NULL;
    if (!*first) return 0;
    for (i=0;i<4;++i) if (world_get_block(world,x+dx[i],y,z+dz[i])==54) {
        BlockEntity *other;
        if (world_block_def(world_get_block(world,x+dx[i],y+1,z+dz[i]))->opaque) return 0;
        other=block_entity_get(world,x+dx[i],y,z+dz[i],1);
        if (!other) return 0;
        if (i==0 || i==2) { *second=*first; *first=other; }
        else *second=other;
        return 54;
    }
    return 27;
}

typedef struct EntityRead {
    Chunk *chunk;
    BlockEntity *current;
    NbtWriter writer;
    uint8_t *scratch;
    size_t capacity;
    int list,items,slot;
    InventorySlot item;
    unsigned count;
    BlockEntity **tail;
} EntityRead;
static int read_tag(void *context,NbtEvent event,const NbtTag *tag,unsigned depth)
{
    EntityRead *r=(EntityRead *)context;
    if (depth==2 && tag->type==NBT_LIST && named(tag,"TileEntities"))
        r->list=event==NBT_BEGIN;
    if (!r->list) return 1;
    if (depth==3 && event==NBT_BEGIN && tag->type==NBT_COMPOUND) {
        int i;
        if (++r->count>1024) return 0;
        r->current=(BlockEntity *)calloc(1,sizeof(*r->current));
        if (!r->current) return 0;
        for (i=0;i<27;++i) inventory_clear_slot(&r->current->slots[i]);
        nbt_writer_init(&r->writer,r->scratch,r->capacity,NULL);
    }
    if (!r->current) return 1;
    if ((event==NBT_FINISH ? nbt_writer_end(&r->writer) :
         nbt_writer_tag(&r->writer,tag))!=NBT_OK) return 0;
    if (event==NBT_VALUE && depth==4) {
        BlockEntity *e=r->current;
        if (tag->type==NBT_STRING && named(tag,"id")) {
            if (text_is(tag->value.bytes,"Chest")) e->kind=BLOCK_ENTITY_CHEST;
            else if (text_is(tag->value.bytes,"Furnace")) e->kind=BLOCK_ENTITY_FURNACE;
            else if (text_is(tag->value.bytes,"Sign")) e->kind=BLOCK_ENTITY_SIGN;
            else if (text_is(tag->value.bytes,"RecordPlayer")) e->kind=BLOCK_ENTITY_JUKEBOX;
        } else if (tag->type==NBT_STRING && tag->name.size==5 &&
                   !memcmp(tag->name.data,"Text",4) && tag->name.data[4]>='1' && tag->name.data[4]<='4') {
            sign_line_read_nbt(e->sign_text[tag->name.data[4]-'1'],tag->value.bytes.data,tag->value.bytes.size);
        } else if (tag->type==NBT_INT) {
            if (named(tag,"x")) e->x=tag->value.int_value;
            if (named(tag,"y")) e->y=tag->value.int_value;
            if (named(tag,"z")) e->z=tag->value.int_value;
            if (named(tag,"Record")) e->record=tag->value.int_value;
        } else if (tag->type==NBT_SHORT) {
            if (named(tag,"BurnTime")) e->burn=tag->value.short_value;
            if (named(tag,"CookTime")) e->cook=tag->value.short_value;
        }
    }
    if (depth==4 && tag->type==NBT_LIST && named(tag,"Items")) r->items=event==NBT_BEGIN;
    if (r->items && depth==5 && event==NBT_BEGIN) { r->slot=-1; inventory_clear_slot(&r->item); }
    if (r->items && depth==6 && event==NBT_VALUE) {
        if (tag->type==NBT_BYTE && named(tag,"Slot")) r->slot=(uint8_t)tag->value.byte;
        if (tag->type==NBT_BYTE && named(tag,"Count")) r->item.count=(uint8_t)tag->value.byte;
        if (tag->type==NBT_SHORT && named(tag,"id")) r->item.id=(uint16_t)tag->value.short_value;
        if (tag->type==NBT_SHORT && named(tag,"Damage")) r->item.damage=(uint16_t)tag->value.short_value;
    }
    if (r->items && depth==5 && event==NBT_FINISH && r->slot>=0 && r->slot<27)
        r->current->slots[r->slot]=r->item;
    if (depth==3 && event==NBT_FINISH) {
        BlockEntity *e=r->current;
        size_t size=0;
        if (nbt_writer_finish(&r->writer,&size)!=NBT_OK) return 0;
        e->raw=(uint8_t *)malloc(size);
        if (!e->raw) return 0;
        memcpy(e->raw,r->scratch,size); e->raw_size=size;
        e->fuel=furnace_fuel_ticks(e->slots[1].id);
        if (e->fuel<e->burn) e->fuel=e->burn;
        *r->tail=e; r->tail=&e->next; r->current=NULL;
    }
    return 1;
}
int block_entities_read(Chunk *chunk,const uint8_t *raw,size_t size)
{
    EntityRead read;
    int ok;
    memset(&read,0,sizeof(read)); read.chunk=chunk; read.capacity=size+64;
    read.tail=&chunk->entities;
    while (*read.tail) read.tail=&(*read.tail)->next;
    read.scratch=(uint8_t *)malloc(read.capacity);
    if (!read.scratch) return 0;
    ok=nbt_read(raw,size,NULL,read_tag,&read,NULL)==NBT_OK;
    free(read.scratch); free_entity(read.current);
    if (!ok) block_entities_free(chunk);
    return ok;
}

static int scalar(NbtWriter *w,NbtType type,const char *name,int value)
{
    NbtTag t={0}; t.type=type; t.name=nbt_span(name);
    if (type==NBT_INT) t.value.int_value=value;
    else if (type==NBT_SHORT) t.value.short_value=(int16_t)value;
    else t.value.byte=(int8_t)value;
    return nbt_writer_tag(w,&t)==NBT_OK;
}
static int write_items(NbtWriter *w,const BlockEntity *e)
{
    NbtTag t={0}; int i,n=e->kind==BLOCK_ENTITY_FURNACE ? 3 : 27;
    t.type=NBT_LIST; t.list_type=NBT_COMPOUND; t.name=nbt_span("Items");
    for (i=0;i<n;++i) if (e->slots[i].id>0 && e->slots[i].count>0) ++t.count;
    if (nbt_writer_tag(w,&t)!=NBT_OK) return 0;
    for (i=0;i<n;++i) if (e->slots[i].id>0 && e->slots[i].count>0) {
        NbtTag item={0}; item.type=NBT_COMPOUND;
        if (nbt_writer_tag(w,&item)!=NBT_OK || !scalar(w,NBT_BYTE,"Slot",i) ||
            !scalar(w,NBT_SHORT,"id",e->slots[i].id) ||
            !scalar(w,NBT_BYTE,"Count",e->slots[i].count) ||
            !scalar(w,NBT_SHORT,"Damage",e->slots[i].damage) || nbt_writer_end(w)!=NBT_OK) return 0;
    }
    return nbt_writer_end(w)==NBT_OK;
}
static int write_sign_line(NbtWriter *w,const BlockEntity *e,unsigned line)
{
    uint8_t bytes[91]; char name[6]="Text1"; NbtTag t={0};
    name[4]=(char)('1'+line); t.type=NBT_STRING; t.name=nbt_span(name);
    t.value.bytes.data=bytes;
    t.value.bytes.size=sign_line_write_nbt(bytes,sizeof(bytes),e->sign_text[line]);
    return nbt_writer_tag(w,&t)==NBT_OK;
}
typedef struct EntityWrite { NbtWriter *writer; const BlockEntity *entity; int skip,items,burn,cook; unsigned sign_lines; } EntityWrite;
static int entity_tag(void *context,NbtEvent event,const NbtTag *tag,unsigned depth)
{
    EntityWrite *e=(EntityWrite *)context;
    NbtTag t=*tag;
    if(e->entity->kind==BLOCK_ENTITY_JUKEBOX) {
        if(event==NBT_VALUE && depth==1 && named(tag,"Record") && tag->type==NBT_INT) {
            e->items=1; t.value.int_value=e->entity->record;
        }
        if(event==NBT_FINISH && depth==0 && !e->items && e->entity->record &&
           !scalar(e->writer,NBT_INT,"Record",e->entity->record)) return 0;
    } else if (e->entity->kind==BLOCK_ENTITY_SIGN) {
        if(event==NBT_VALUE && depth==1 && tag->type==NBT_STRING && tag->name.size==5 &&
           !memcmp(tag->name.data,"Text",4) && tag->name.data[4]>='1' && tag->name.data[4]<='4') {
            unsigned line=tag->name.data[4]-'1'; e->sign_lines|=1u<<line;
            if(e->entity->sign_text_modified) return write_sign_line(e->writer,e->entity,line);
        }
        if(event==NBT_FINISH && depth==0 && e->entity->sign_text_modified) {
            unsigned line;
            for(line=0;line<4;++line) if(!(e->sign_lines&(1u<<line)) &&
                !write_sign_line(e->writer,e->entity,line)) return 0;
        }
    } else if (e->entity->kind!=BLOCK_ENTITY_UNKNOWN) {
        if (depth==1 && tag->type==NBT_LIST && named(tag,"Items")) {
            if (event==NBT_BEGIN) { e->skip=1; e->items=1; return write_items(e->writer,e->entity); }
            if (event==NBT_FINISH) { e->skip=0; return 1; }
        }
        if (e->skip) return 1;
        if (event==NBT_VALUE && depth==1 && tag->type==NBT_SHORT) {
            if (named(tag,"BurnTime")) { t.value.short_value=(int16_t)e->entity->burn; e->burn=1; }
            if (named(tag,"CookTime")) { t.value.short_value=(int16_t)e->entity->cook; e->cook=1; }
        }
        if (event==NBT_FINISH && depth==0) {
            if (!e->items && !write_items(e->writer,e->entity)) return 0;
            if (e->entity->kind==BLOCK_ENTITY_FURNACE &&
                ((!e->burn && !scalar(e->writer,NBT_SHORT,"BurnTime",e->entity->burn)) ||
                 (!e->cook && !scalar(e->writer,NBT_SHORT,"CookTime",e->entity->cook)))) return 0;
        }
    }
    return (event==NBT_FINISH ? nbt_writer_end(e->writer) : nbt_writer_tag(e->writer,&t))==NBT_OK;
}
static int write_entity(NbtWriter *w,const BlockEntity *entity)
{
    if (entity->raw) {
        EntityWrite e={0}; e.writer=w; e.entity=entity;
        return nbt_read(entity->raw,entity->raw_size,NULL,entity_tag,&e,NULL)==NBT_OK;
    } else {
        NbtTag t={0}; t.type=NBT_COMPOUND;
        if (nbt_writer_tag(w,&t)!=NBT_OK) return 0;
        t.type=NBT_STRING; t.name=nbt_span("id");
        t.value.bytes=nbt_span(entity->kind==BLOCK_ENTITY_CHEST ? "Chest" : entity->kind==BLOCK_ENTITY_SIGN ? "Sign" : entity->kind==BLOCK_ENTITY_JUKEBOX ? "RecordPlayer" : "Furnace");
        if (nbt_writer_tag(w,&t)!=NBT_OK || !scalar(w,NBT_INT,"x",entity->x) ||
            !scalar(w,NBT_INT,"y",entity->y) || !scalar(w,NBT_INT,"z",entity->z)) return 0;
        if(entity->kind==BLOCK_ENTITY_SIGN) {
            unsigned line;
            for(line=0;line<4;++line) if(!write_sign_line(w,entity,line)) return 0;
        } else if(entity->kind==BLOCK_ENTITY_JUKEBOX) {
            if(entity->record && !scalar(w,NBT_INT,"Record",entity->record)) return 0;
        } else if(!write_items(w,entity)) return 0;
        if (entity->kind==BLOCK_ENTITY_FURNACE &&
            (!scalar(w,NBT_SHORT,"BurnTime",entity->burn) || !scalar(w,NBT_SHORT,"CookTime",entity->cook))) return 0;
        return nbt_writer_end(w)==NBT_OK;
    }
}
static int write_list(NbtWriter *w,const Chunk *chunk)
{
    NbtTag t={0}; const BlockEntity *e;
    t.type=NBT_LIST; t.name=nbt_span("TileEntities"); t.list_type=NBT_COMPOUND;
    for (e=chunk->entities;e;e=e->next) ++t.count;
    if (nbt_writer_tag(w,&t)!=NBT_OK) return 0;
    for (e=chunk->entities;e;e=e->next) if (!write_entity(w,e)) return 0;
    return nbt_writer_end(w)==NBT_OK;
}
typedef struct ChunkWrite { NbtWriter writer; const Chunk *chunk; int skip,done; } ChunkWrite;
static int chunk_tag(void *context,NbtEvent event,const NbtTag *tag,unsigned depth)
{
    ChunkWrite *c=(ChunkWrite *)context;
    if (depth==2 && tag->type==NBT_LIST && named(tag,"TileEntities")) {
        if (event==NBT_BEGIN) { c->skip=1; c->done=1; return write_list(&c->writer,c->chunk); }
        if (event==NBT_FINISH) { c->skip=0; return 1; }
    }
    if (c->skip) return 1;
    if (depth==1 && event==NBT_FINISH && tag->type==NBT_COMPOUND && named(tag,"Level") && !c->done)
        if (!write_list(&c->writer,c->chunk)) return 0;
    return (event==NBT_FINISH ? nbt_writer_end(&c->writer) : nbt_writer_tag(&c->writer,tag))==NBT_OK;
}
int block_entities_rewrite(const Chunk *chunk,const uint8_t *input,size_t size,uint8_t **output,size_t *output_size)
{
    ChunkWrite c={0}; const BlockEntity *e; size_t capacity=size+1024;
    for (e=chunk->entities;e;e=e->next) capacity+=e->raw_size+8192;
    if (capacity>16u*1024u*1024u) return 0;
    *output=(uint8_t *)malloc(capacity);
    if (!*output) return 0;
    c.chunk=chunk; nbt_writer_init(&c.writer,*output,capacity,NULL);
    if (nbt_read(input,size,NULL,chunk_tag,&c,NULL)!=NBT_OK ||
        nbt_writer_finish(&c.writer,output_size)!=NBT_OK) { free(*output); *output=NULL; return 0; }
    return 1;
}
static int native_path(const World *world,const Chunk *chunk,char *path,size_t capacity,int temp)
{
    int n=snprintf(path,capacity,"%s/chunk_%ld_%ld.rct%s",world->path,(long)chunk->x,(long)chunk->z,temp ? ".tmp" : "");
    return n>0 && (size_t)n<capacity;
}
int block_entities_native_write(const World *world,const Chunk *chunk)
{
    char path[512],temp[512]; uint8_t *bytes; size_t size,capacity=1024;
    NbtWriter w; NbtTag root={0},level={0}; const BlockEntity *e; const SavedEntity *entity;
    FILE *f; int ok;
    for (e=chunk->entities;e;e=e->next) capacity+=e->raw_size+8192;
    for(entity=chunk->saved_entities;entity;entity=entity->next) capacity+=entity->raw_size+2048;
    capacity+=world_ticks_capacity(chunk);
    if(capacity>16u*1024u*1024u) return 0;
    if (!native_path(world,chunk,path,sizeof(path),0) || !native_path(world,chunk,temp,sizeof(temp),1)) return 0;
    if (!chunk->entities && !chunk->saved_entities && !chunk->ticks &&
        !(chunk->dirty_flags&(CHUNK_DIRTY_ENTITIES|CHUNK_DIRTY_TICKS))) return 1;
    bytes=(uint8_t *)malloc(capacity); if (!bytes) return 0;
    nbt_writer_init(&w,bytes,capacity,NULL); root.type=level.type=NBT_COMPOUND; level.name=nbt_span("Level");
    ok=nbt_writer_tag(&w,&root)==NBT_OK && nbt_writer_tag(&w,&level)==NBT_OK &&
        write_list(&w,chunk) && world_entities_write_list(chunk,&w) && world_ticks_write_list(world,chunk,&w) && nbt_writer_end(&w)==NBT_OK && nbt_writer_end(&w)==NBT_OK &&
        nbt_writer_finish(&w,&size)==NBT_OK;
    if (!ok) { free(bytes); return 0; }
    f=fopen(temp,"wb"); if (!f) { free(bytes); return 0; }
    ok=fwrite(bytes,1,size,f)==size && fflush(f)==0;
    if (fclose(f)!=0) ok=0;
    free(bytes); if (!ok) return 0;
#ifdef _WIN32
    return MoveFileExA(temp,path,MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH)!=0;
#else
    return rename(temp,path)==0;
#endif
}
int block_entities_native_read(const World *world,Chunk *chunk)
{
    char path[512]; FILE *f; long length; uint8_t *bytes; int ok;
    if (!native_path(world,chunk,path,sizeof(path),0)) return 0;
    f=fopen(path,"rb"); if (!f) return errno==ENOENT;
    if (fseek(f,0,SEEK_END)!=0 || (length=ftell(f))<0 || length>16*1024*1024 ||
        fseek(f,0,SEEK_SET)!=0) { fclose(f); return 0; }
    bytes=(uint8_t *)malloc((size_t)length+1); if (!bytes) { fclose(f); return 0; }
    ok=fread(bytes,1,(size_t)length,f)==(size_t)length;
    if (fclose(f)!=0) ok=0;
    if (ok) ok=block_entities_read(chunk,bytes,(size_t)length);
    if(ok) ok=world_entities_read(chunk,bytes,(size_t)length);
    if(ok) ok=world_ticks_read(chunk,bytes,(size_t)length);
    free(bytes); return ok;
}
int block_chest_texture(const World *w,int x,int y,int z,unsigned face)
{
    unsigned north=world_peek_block(w,x,y,z-1),south=world_peek_block(w,x,y,z+1);
    unsigned west=world_peek_block(w,x-1,y,z),east=world_peek_block(w,x+1,y,z);
    int front=3,offset=0;
    if(face<2) return 25;
    if(north!=54 && south!=54) {
        if(west!=54 && east!=54) {
            if(world_block_def(south)->opaque && !world_block_def(north)->opaque) front=2;
            if(world_block_def(west)->opaque && !world_block_def(east)->opaque) front=5;
            if(world_block_def(east)->opaque && !world_block_def(west)->opaque) front=4;
            return face==(unsigned)front ? 27 : 26;
        }
        if(face==4 || face==5) return 26;
        {
            int other_x=west==54 ? x-1 : x+1;
            unsigned on=world_peek_block(w,other_x,y,z-1),os=world_peek_block(w,other_x,y,z+1);
            offset=west==54 ? -1 : 0;
            if(face==3) offset=-1-offset;
            if((world_block_def(south)->opaque || world_block_def(os)->opaque) &&
                !world_block_def(north)->opaque && !world_block_def(on)->opaque) front=2;
        }
    } else {
        unsigned ow,oe; int other_z=north==54 ? z-1 : z+1;
        if(face==2 || face==3) return 26;
        ow=world_peek_block(w,x-1,y,other_z); oe=world_peek_block(w,x+1,y,other_z);
        offset=north==54 ? -1 : 0; if(face==4) offset=-1-offset;
        front=5;
        if((world_block_def(east)->opaque || world_block_def(oe)->opaque) &&
            !world_block_def(west)->opaque && !world_block_def(ow)->opaque) front=4;
    }
    return (face==(unsigned)front ? 42 : 58)+offset;
}
