#include "ticks.h"
#include "world.h"
#include "../nbt/nbt.h"
#include <limits.h>
#include <stdlib.h>
#include <string.h>

static int named(const NbtTag *t,const char *name)
{ return t->name.size==strlen(name) && !memcmp(t->name.data,name,t->name.size); }
static void changed(Chunk *c)
{ c->ticks_modified=1; c->dirty_flags|=CHUNK_DIRTY_SAVE|CHUNK_DIRTY_TICKS; }
int world_ticks_supported(unsigned id)
{ return id==8 || id==10 || id==12 || id==13 || id==50 || id==55 || id==75 || id==76 || id==81; }
void world_ticks_free(Chunk *c)
{
    SavedTick *t,*next;
    if (!c) return;
    for (t=c->ticks;t;t=next) { next=t->next; free(t->raw); free(t); }
    c->ticks=NULL;
}
typedef struct TickRead {
    Chunk *chunk;
    SavedTick *current,**tail;
    NbtWriter writer;
    uint8_t *scratch;
    size_t capacity;
    unsigned count,fields;
    int list;
} TickRead;
static int read_tag(void *context,NbtEvent event,const NbtTag *tag,unsigned depth)
{
    TickRead *r=(TickRead *)context;
    if (depth==2 && tag->type==NBT_LIST && named(tag,"TileTicks")) {
        if (event==NBT_BEGIN && tag->list_type!=NBT_COMPOUND && tag->count) return 0;
        r->list=event==NBT_BEGIN;
    }
    if (!r->list) return 1;
    if (depth==3 && event==NBT_BEGIN && tag->type==NBT_COMPOUND) {
        if (++r->count>65536u) return 0;
        r->current=(SavedTick *)calloc(1,sizeof(*r->current));
        if (!r->current) return 0;
        r->fields=0;
        nbt_writer_init(&r->writer,r->scratch,r->capacity,NULL);
    }
    if (!r->current) return 1;
    if ((event==NBT_FINISH ? nbt_writer_end(&r->writer) : nbt_writer_tag(&r->writer,tag))!=NBT_OK) return 0;
    if (depth==4 && event==NBT_VALUE && tag->type==NBT_INT) {
        SavedTick *t=r->current;
        if (named(tag,"i")) { t->id=tag->value.int_value; r->fields|=1; }
        else if (named(tag,"x")) { t->x=tag->value.int_value; r->fields|=2; }
        else if (named(tag,"y")) { t->y=tag->value.int_value; r->fields|=4; }
        else if (named(tag,"z")) { t->z=tag->value.int_value; r->fields|=8; }
        else if (named(tag,"t")) { t->delay=tag->value.int_value; r->fields|=16; }
    }
    if (depth==3 && event==NBT_FINISH) {
        SavedTick *t=r->current; size_t size=0;
        if (nbt_writer_finish(&r->writer,&size)!=NBT_OK) return 0;
        t->raw=(uint8_t *)malloc(size);
        if (!t->raw) return 0;
        memcpy(t->raw,r->scratch,size); t->raw_size=size;
        t->managed=(uint8_t)(r->fields==31 && (unsigned)t->y<WORLD_HEIGHT &&
            (int64_t)t->x>=(int64_t)r->chunk->x*16 && (int64_t)t->x<(int64_t)r->chunk->x*16+16 &&
            (int64_t)t->z>=(int64_t)r->chunk->z*16 && (int64_t)t->z<(int64_t)r->chunk->z*16+16 &&
            world_ticks_supported((unsigned)t->id));
        *r->tail=t; r->tail=&t->next; r->current=NULL;
    }
    return 1;
}
int world_ticks_read(Chunk *chunk,const uint8_t *raw,size_t size)
{
    TickRead r; int ok;
    memset(&r,0,sizeof(r)); r.chunk=chunk; r.capacity=size+64; r.tail=&chunk->ticks;
    while (*r.tail) r.tail=&(*r.tail)->next;
    r.scratch=(uint8_t *)malloc(r.capacity);
    if (!r.scratch) return 0;
    ok=nbt_read(raw,size,NULL,read_tag,&r,NULL)==NBT_OK;
    free(r.scratch);
    if (r.current) { free(r.current->raw); free(r.current); }
    if (!ok) world_ticks_free(chunk);
    return ok;
}
SavedTick *world_ticks_remember(World *w,Chunk *c,int x,int y,int z,uint8_t id,unsigned delay)
{
    SavedTick *t;
    for (t=c->ticks;t;t=t->next) if (t->managed && t->x==x && t->y==y && t->z==z && t->id==id) break;
    if (!t) {
        t=(SavedTick *)calloc(1,sizeof(*t));
        if (!t) { w->error=WORLD_ERROR_OUT_OF_MEMORY; return NULL; }
        t->x=x; t->y=y; t->z=z; t->id=id; t->managed=1;
        t->delay=(int32_t)(delay>INT_MAX ? INT_MAX : delay);
        t->next=c->ticks; c->ticks=t;
    }
    if (!t->resumed) {
        t->due=w->tick+(t->delay>0 ? (unsigned)t->delay : 1u);
        t->resumed=1;
    }
    changed(c); return t;
}
void world_ticks_resume(World *w,Chunk *c)
{
    SavedTick *t;
    for (t=c->ticks;t;t=t->next) if (t->managed && !t->queued)
        world_schedule_tick(w,t->x,t->y,t->z,(uint8_t)t->id,t->delay>0 ? (unsigned)t->delay : 1u);
}
void world_ticks_consume(World *w,const WorldPhysicsCell *cell)
{
    int cx=cell->x>=0 ? cell->x/16 : (int)(-((-(int64_t)cell->x+15)/16));
    int cz=cell->z>=0 ? cell->z/16 : (int)(-((-(int64_t)cell->z+15)/16));
    Chunk *c=world_peek_chunk(w,cx,cz); SavedTick **link;
    if (!c || !cell->saved_tick) return;
    for (link=&c->ticks;*link;link=&(*link)->next) if (*link==cell->saved_tick) {
        SavedTick *t=*link; *link=t->next; free(t->raw); free(t); changed(c); return;
    }
}
void world_ticks_dirty_countdowns(World *w)
{
    size_t i;
    for (i=0;i<w->cache_count;++i) {
        SavedTick *t;
        if(w->cache[i]->ticks_saved_at==w->tick) continue;
        for (t=w->cache[i]->ticks;t;t=t->next) if (t->managed && t->resumed) { changed(w->cache[i]); break; }
    }
}
size_t world_ticks_capacity(const Chunk *c)
{
    const SavedTick *t; size_t capacity=256;
    for (t=c->ticks;t;t=t->next) capacity+=t->raw_size+128;
    return capacity;
}
static int scalar(NbtWriter *w,const char *name,int32_t value)
{
    NbtTag t={0}; t.type=NBT_INT; t.name=nbt_span(name); t.value.int_value=value;
    return nbt_writer_tag(w,&t)==NBT_OK;
}
typedef struct TickWrite { NbtWriter *writer; const SavedTick *tick; int32_t delay; } TickWrite;
static int tick_tag(void *context,NbtEvent event,const NbtTag *tag,unsigned depth)
{
    TickWrite *w=(TickWrite *)context; NbtTag t=*tag;
    if (depth==1 && event==NBT_VALUE && t.type==NBT_INT && named(&t,"t") && w->tick->managed)
        t.value.int_value=w->delay;
    return (event==NBT_FINISH ? nbt_writer_end(w->writer) : nbt_writer_tag(w->writer,&t))==NBT_OK;
}
int world_ticks_write_list(const World *w,const Chunk *c,NbtWriter *writer)
{
    NbtTag list={0}; const SavedTick *t;
    list.type=NBT_LIST; list.list_type=NBT_COMPOUND; list.name=nbt_span("TileTicks");
    for (t=c->ticks;t;t=t->next) ++list.count;
    if (nbt_writer_tag(writer,&list)!=NBT_OK) return 0;
    for (t=c->ticks;t;t=t->next) {
        int32_t delay=t->delay;
        if (t->managed && t->resumed) {
            uint64_t left=t->due>w->tick ? t->due-w->tick : 0;
            delay=(int32_t)(left>INT_MAX ? INT_MAX : left);
        }
        if (t->raw) {
            TickWrite r; r.writer=writer; r.tick=t; r.delay=delay;
            if (nbt_read(t->raw,t->raw_size,NULL,tick_tag,&r,NULL)!=NBT_OK) return 0;
        } else {
            NbtTag item={0}; item.type=NBT_COMPOUND;
            if (nbt_writer_tag(writer,&item)!=NBT_OK || !scalar(writer,"i",t->id) ||
                !scalar(writer,"x",t->x) || !scalar(writer,"y",t->y) || !scalar(writer,"z",t->z) ||
                !scalar(writer,"t",delay) || nbt_writer_end(writer)!=NBT_OK) return 0;
        }
    }
    return nbt_writer_end(writer)==NBT_OK;
}
typedef struct ChunkWrite { NbtWriter writer; const World *world; const Chunk *chunk; int skip,done; } ChunkWrite;
static int chunk_tag(void *context,NbtEvent event,const NbtTag *tag,unsigned depth)
{
    ChunkWrite *c=(ChunkWrite *)context;
    if (depth==2 && tag->type==NBT_LIST && named(tag,"TileTicks")) {
        if (event==NBT_BEGIN) { c->skip=1; c->done=1; return world_ticks_write_list(c->world,c->chunk,&c->writer); }
        if (event==NBT_FINISH) { c->skip=0; return 1; }
    }
    if (c->skip) return 1;
    if (depth==1 && event==NBT_FINISH && tag->type==NBT_COMPOUND && named(tag,"Level") && !c->done)
        if (!world_ticks_write_list(c->world,c->chunk,&c->writer)) return 0;
    return (event==NBT_FINISH ? nbt_writer_end(&c->writer) : nbt_writer_tag(&c->writer,tag))==NBT_OK;
}
int world_ticks_rewrite(const World *w,const Chunk *c,const uint8_t *input,size_t size,uint8_t **output,size_t *output_size)
{
    ChunkWrite writer; size_t capacity=size+world_ticks_capacity(c);
    if (capacity>16u*1024u*1024u) return 0;
    memset(&writer,0,sizeof(writer)); writer.world=w; writer.chunk=c;
    *output=(uint8_t *)malloc(capacity);
    if (!*output) return 0;
    nbt_writer_init(&writer.writer,*output,capacity,NULL);
    if (nbt_read(input,size,NULL,chunk_tag,&writer,NULL)!=NBT_OK ||
        nbt_writer_finish(&writer.writer,output_size)!=NBT_OK) { free(*output); *output=NULL; return 0; }
    return 1;
}
