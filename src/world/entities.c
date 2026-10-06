#include "entities.h"
#include "../game/player.h"
#include "environment.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>
static int named(const NbtTag *t,const char *n) { return t->name.size==strlen(n) && !memcmp(t->name.data,n,t->name.size); }
static Chunk *owner(const World *w,int x,int z)
{
    int cx=x>=0 ? x/16 : (int)(-((-(int64_t)x+15)/16)),cz=z>=0 ? z/16 : (int)(-((-(int64_t)z+15)/16));
    return world_peek_chunk(w,cx,cz);
}
static void free_one(SavedEntity *e) { free(e->raw); free(e); }
void world_entities_free(Chunk *chunk)
{ SavedEntity *e,*next; for(e=chunk->saved_entities;e;e=next) { next=e->next; free_one(e); } chunk->saved_entities=NULL; }
typedef struct Read {
    SavedEntity **tail,*current; NbtWriter writer; uint8_t *scratch; size_t size;
    int list,pos,motion,index,item,records,rotation,health_seen,pos_seen,items,slot,fall_time_seen;
    InventorySlot cargo_item;
} Read;
static int read_tag(void *context,NbtEvent event,const NbtTag *t,unsigned depth)
{
    Read *r=(Read *)context;
    if(depth==2 && t->type==NBT_LIST && named(t,"Entities")) r->list=event==NBT_BEGIN;
    if(!r->list) return 1;
    if(event==NBT_BEGIN && depth==3 && t->type==NBT_COMPOUND) {
        if(++r->records>4096) return 0;
        r->current=(SavedEntity *)calloc(1,sizeof(*r->current)); if(!r->current) return 0;
        r->current->item.id=-1; r->current->item.health=5;
        r->health_seen=r->pos_seen=r->fall_time_seen=0; r->current->mob.health=10;r->current->mob.air=300;
        r->current->transport.x_tile=r->current->transport.y_tile=r->current->transport.z_tile=-1;
        nbt_writer_init(&r->writer,r->scratch,r->size,NULL);
    }
    if(!r->current) return 1;
    if((event==NBT_FINISH ? nbt_writer_end(&r->writer) : nbt_writer_tag(&r->writer,t))!=NBT_OK) return 0;
    if(event==NBT_VALUE && depth==4) {
        if(t->type==NBT_STRING && named(t,"id") && t->value.bytes.size==4 && !memcmp(t->value.bytes.data,"Item",4)) r->current->item_entity=1;
        if(t->type==NBT_STRING && named(t,"id")) {
            if(t->value.bytes.size==5 && !memcmp(t->value.bytes.data,"Arrow",5)) r->current->transport.kind=1;
            if(t->value.bytes.size==8 && !memcmp(t->value.bytes.data,"Minecart",8)) r->current->transport.kind=2;
            if(t->value.bytes.size==4 && !memcmp(t->value.bytes.data,"Boat",4)) r->current->transport.kind=3;
            if(t->value.bytes.size==9 && !memcmp(t->value.bytes.data,"PrimedTnt",9)) r->current->transport.kind=4;
            if(t->value.bytes.size==8 && !memcmp(t->value.bytes.data,"Snowball",8)) r->current->transport.kind=5;
            if(t->value.bytes.size==3 && !memcmp(t->value.bytes.data,"Egg",3)) r->current->transport.kind=6;
            if(t->value.bytes.size==11 && !memcmp(t->value.bytes.data,"FallingSand",11)) r->current->transport.kind=7;
            r->current->mob.type=mob_type(t->value.bytes.data,t->value.bytes.size);
            if(!r->health_seen) r->current->mob.health=mob_default_health(r->current->mob.type);
        }
        if(t->type==NBT_SHORT && named(t,"Age")) r->current->item.age=t->value.short_value/20.0f;
        if(t->type==NBT_SHORT && named(t,"AttackTime")) r->current->mob.attack_ticks=t->value.short_value;
        if(t->type==NBT_SHORT && named(t,"DeathTime")) r->current->mob.death_ticks=t->value.short_value;
        if(t->type==NBT_SHORT && named(t,"HurtTime")) r->current->mob.hurt_ticks=t->value.short_value;
        if(t->type==NBT_SHORT && named(t,"Health")) { r->health_seen=1; r->current->item.health=t->value.short_value; r->current->mob.health=t->value.short_value; }
        if(t->type==NBT_SHORT && named(t,"Fire")) r->current->mob.fire=t->value.short_value;
        if(t->type==NBT_SHORT && named(t,"Air")) r->current->mob.air=t->value.short_value;
        if(t->type==NBT_BYTE && named(t,"OnGround")) r->current->mob.on_ground=t->value.byte!=0;
        if(t->type==NBT_BYTE && named(t,"Color")) r->current->mob.color=t->value.byte&15;
        if(t->type==NBT_BYTE && named(t,"Sheared")) r->current->mob.sheared=t->value.byte!=0;
        if(t->type==NBT_BYTE && named(t,"powered")) r->current->mob.powered=t->value.byte!=0;
        {
            TransportState *s=&r->current->transport;
            if(t->type==NBT_SHORT) {
                if(named(t,"xTile")) s->x_tile=t->value.short_value;
                if(named(t,"yTile")) s->y_tile=t->value.short_value;
                if(named(t,"zTile")) s->z_tile=t->value.short_value;
                if(named(t,"Fuel")) s->fuel=t->value.short_value;
            }
            if(t->type==NBT_BYTE) {
                if(named(t,"Fuse")) s->fuse=(uint8_t)t->value.byte;
                if(named(t,"Tile")) s->falling_block=(uint8_t)t->value.byte;
                if(named(t,"Data")) s->fall_data=t->value.byte&15;
                if(named(t,"Time")) { s->fall_time=(uint8_t)t->value.byte; r->fall_time_seen=1; }
                if(named(t,"inTile")) s->in_tile=(uint8_t)t->value.byte;
                if(named(t,"inData")) s->in_data=(uint8_t)t->value.byte;
                if(named(t,"shake")) s->shake=(uint8_t)t->value.byte;
                if(named(t,"inGround")) s->in_ground=t->value.byte!=0;
                if(named(t,"player")) s->player=t->value.byte!=0;
            }
            if(t->type==NBT_INT && named(t,"Type")) s->type=t->value.int_value;
            if(t->type==NBT_DOUBLE && named(t,"PushX")) s->push_x=(float)t->value.double_value;
            if(t->type==NBT_DOUBLE && named(t,"PushZ")) s->push_z=(float)t->value.double_value;
        }
    }
    if(event==NBT_BEGIN && depth==4) {
        if(t->type==NBT_LIST && t->list_type==NBT_DOUBLE && t->count==3) {
            r->pos=named(t,"Pos"); r->motion=named(t,"Motion"); r->index=0;
        }
        if(t->type==NBT_LIST && named(t,"Rotation") && t->list_type==NBT_FLOAT && t->count==2) { r->rotation=1; r->index=0; }
        if(t->type==NBT_COMPOUND && named(t,"Item")) r->item=1;
        if(t->type==NBT_LIST && named(t,"Items")) r->items=1;
    }
    if(event==NBT_VALUE && depth==5) {
        ItemDrop *d=&r->current->item;
        if(t->type==NBT_DOUBLE && r->index<3 && (r->pos || r->motion)) {
            float *p=r->pos ? (r->index==0 ? &d->x : r->index==1 ? &d->y : &d->z) :
                (r->index==0 ? &d->vx : r->index==1 ? &d->vy : &d->vz);
            *p=(float)t->value.double_value*(r->motion ? 20 : 1); ++r->index;
        }
        if(t->type==NBT_FLOAT && r->rotation && r->index<2) {
            if(r->index++==0) r->current->mob.yaw=t->value.float_value; else r->current->mob.pitch=t->value.float_value;
        }
        if(r->item) {
            if(t->type==NBT_SHORT && named(t,"id")) d->id=(uint16_t)t->value.short_value;
            if(t->type==NBT_SHORT && named(t,"Damage")) d->damage=(uint16_t)t->value.short_value;
            if(t->type==NBT_BYTE && named(t,"Count")) d->count=(uint8_t)t->value.byte;
        }
    }
    if(r->items && depth==5 && event==NBT_BEGIN && t->type==NBT_COMPOUND) { r->slot=-1; inventory_clear_slot(&r->cargo_item); }
    if(r->items && depth==6 && event==NBT_VALUE) {
        if(t->type==NBT_BYTE && named(t,"Slot")) r->slot=(uint8_t)t->value.byte;
        {
            InventorySlot *s=&r->cargo_item;
            if(t->type==NBT_SHORT && named(t,"id")) s->id=(uint16_t)t->value.short_value;
            if(t->type==NBT_SHORT && named(t,"Damage")) s->damage=(uint16_t)t->value.short_value;
            if(t->type==NBT_BYTE && named(t,"Count")) s->count=(uint8_t)t->value.byte;
        }
    }
    if(r->items && depth==5 && event==NBT_FINISH && t->type==NBT_COMPOUND && r->slot>=0 && r->slot<27)
        r->current->transport.cargo[r->slot]=r->cargo_item;
    if(event==NBT_FINISH && depth==4) {
        if(t->type==NBT_LIST) { if(r->pos && r->index==3) r->pos_seen=1; r->pos=r->motion=r->rotation=r->items=0; }
        if(t->type==NBT_COMPOUND) r->item=0;
    }
    if(event==NBT_FINISH && depth==3) {
        SavedEntity *e=r->current; size_t size;
        if(nbt_writer_finish(&r->writer,&size)!=NBT_OK) return 0;
        e->raw=(uint8_t *)malloc(size); if(!e->raw) return 0;
        memcpy(e->raw,r->scratch,size); e->raw_size=size;
        if(e->item_entity && (!isfinite(e->item.x) || !isfinite(e->item.y) || !isfinite(e->item.z) ||
            !isfinite(e->item.vx) || !isfinite(e->item.vy) || !isfinite(e->item.vz) ||
            fabsf(e->item.x)>32000000 || fabsf(e->item.z)>32000000 || fabsf(e->item.y)>32000000)) return 0;
        e->item.active=e->item_entity && e->item.id>0 && e->item.count>0;
        e->mob.x=e->item.x; e->mob.y=e->item.y; e->mob.z=e->item.z;
        e->mob.previous_x=e->mob.x;e->mob.previous_y=e->mob.y;e->mob.previous_z=e->mob.z;
        e->mob.vx=e->item.vx; e->mob.vy=e->item.vy; e->mob.vz=e->item.vz;
        if(e->transport.kind==1 || e->transport.kind==5 || e->transport.kind==6) e->mob.yaw=180-e->mob.yaw;
        if(e->transport.kind) {
            /* Beta FallingSand NBT has only Tile. Its source was already
             * removed before saving; never require/re-delete it after load. */
            e->transport.source_removed=e->transport.kind!=7 || !r->fall_time_seen || e->transport.fall_time>0;
            if(e->transport.kind==7 && !r->fall_time_seen) e->transport.fall_time=1;
            e->transport.previous_x=e->mob.x; e->transport.previous_y=e->mob.y; e->transport.previous_z=e->mob.z;
            if(fabsf(e->mob.vx)>200) e->mob.vx=0;
            if(fabsf(e->mob.vy)>200) e->mob.vy=0;
            if(fabsf(e->mob.vz)>200) e->mob.vz=0;
            if(e->transport.kind==2 && (e->transport.type<0 || e->transport.type>2)) e->transport.kind=0;
        }
        if(!r->pos_seen) e->mob.type=0; /* Preserve incomplete reference records verbatim. */
        if(!r->pos_seen) e->transport.kind=0;
        if((e->mob.type || e->transport.kind) && (!isfinite(e->mob.x) || !isfinite(e->mob.y) || !isfinite(e->mob.z) ||
           !isfinite(e->mob.vx) || !isfinite(e->mob.vy) || !isfinite(e->mob.vz) ||
           !isfinite(e->mob.yaw) || !isfinite(e->mob.pitch) || fabsf(e->mob.x)>32000000 ||
           fabsf(e->mob.z)>32000000 || fabsf(e->mob.y)>32000000)) return 0;
        *r->tail=e; r->tail=&e->next; r->current=NULL;
    }
    return 1;
}
int world_entities_read(Chunk *chunk,const uint8_t *raw,size_t size)
{
    Read r; int ok; memset(&r,0,sizeof(r)); r.tail=&chunk->saved_entities; r.size=size+64;
    while(*r.tail) r.tail=&(*r.tail)->next;
    r.scratch=(uint8_t *)malloc(r.size); if(!r.scratch) return 0;
    ok=nbt_read(raw,size,NULL,read_tag,&r,NULL)==NBT_OK; free(r.scratch);
    if(r.current) free_one(r.current);
    if(!ok) world_entities_free(chunk);
    return ok;
}
static int scalar(NbtWriter *w,NbtType type,const char *name,int value)
{
    NbtTag t={0}; t.type=type; t.name=nbt_span(name);
    if(type==NBT_SHORT) t.value.short_value=(int16_t)value; else t.value.byte=(int8_t)value;
    return nbt_writer_tag(w,&t)==NBT_OK;
}
static int numbers(NbtWriter *w,const char *name,float x,float y,float z)
{
    NbtTag t={0}; int i; float p[3]={x,y,z};
    t.type=NBT_LIST; t.name=nbt_span(name); t.list_type=NBT_DOUBLE; t.count=3;
    if(nbt_writer_tag(w,&t)!=NBT_OK) return 0;
    memset(&t,0,sizeof(t)); t.type=NBT_DOUBLE;
    for(i=0;i<3;++i) { t.value.double_value=p[i]; if(nbt_writer_tag(w,&t)!=NBT_OK) return 0; }
    return nbt_writer_end(w)==NBT_OK;
}
static int item_tag(NbtWriter *w,const ItemDrop *d)
{
    NbtTag t={0}; t.type=NBT_COMPOUND; t.name=nbt_span("Item");
    return nbt_writer_tag(w,&t)==NBT_OK && scalar(w,NBT_SHORT,"id",d->id) && scalar(w,NBT_BYTE,"Count",d->count) &&
        scalar(w,NBT_SHORT,"Damage",d->damage) && nbt_writer_end(w)==NBT_OK;
}
typedef struct WriteItem { NbtWriter *writer; const ItemDrop *d; int index,pos,motion,item,in_item; } WriteItem;
static int item_rewrite(void *context,NbtEvent event,const NbtTag *t,unsigned depth)
{
    WriteItem *r=(WriteItem *)context; NbtTag changed=*t;
    if(event==NBT_BEGIN && depth==1 && t->type==NBT_COMPOUND && named(t,"Item")) {
        r->item=r->in_item=1;
    }
    if(event==NBT_BEGIN && depth==1 && t->type==NBT_LIST) {
        r->pos=named(t,"Pos"); r->motion=named(t,"Motion"); r->index=0;
    }
    if(event==NBT_VALUE) {
        if(r->in_item && depth==2) {
            if(t->type==NBT_SHORT && named(t,"id")) changed.value.short_value=(int16_t)r->d->id;
            if(t->type==NBT_BYTE && named(t,"Count")) changed.value.byte=(int8_t)r->d->count;
            if(t->type==NBT_SHORT && named(t,"Damage")) changed.value.short_value=(int16_t)r->d->damage;
        }
        if(depth==1 && t->type==NBT_SHORT && named(t,"Age")) changed.value.short_value=(int16_t)(r->d->age*20);
        if(depth==1 && t->type==NBT_SHORT && named(t,"Health")) changed.value.short_value=(int16_t)r->d->health;
        if(depth==2 && t->type==NBT_DOUBLE && r->index<3 && (r->pos || r->motion)) {
            float p[3]={r->d->x,r->d->y,r->d->z},m[3]={r->d->vx/20,r->d->vy/20,r->d->vz/20};
            changed.value.double_value=r->motion ? m[r->index++] : p[r->index++];
        }
    }
    if(event==NBT_FINISH && depth==1 && t->type==NBT_LIST) r->pos=r->motion=0;
    if(event==NBT_FINISH && depth==1 && t->type==NBT_COMPOUND && named(t,"Item")) r->in_item=0;
    if(event==NBT_FINISH && depth==0 && !r->item && !item_tag(r->writer,r->d)) return 0;
    return (event==NBT_FINISH ? nbt_writer_end(r->writer) : nbt_writer_tag(r->writer,&changed))==NBT_OK;
}
static int copy_tag(void *context,NbtEvent event,const NbtTag *t,unsigned depth)
{ (void)depth; return (event==NBT_FINISH ? nbt_writer_end((NbtWriter *)context) : nbt_writer_tag((NbtWriter *)context,t))==NBT_OK; }
int world_entities_write_list(const Chunk *chunk,NbtWriter *w)
{
    NbtTag t={0}; const SavedEntity *e;
    t.type=NBT_LIST; t.name=nbt_span("Entities"); t.list_type=NBT_COMPOUND;
    for(e=chunk->saved_entities;e;e=e->next) if((!e->item_entity || e->item.active) && !e->transport.dead) ++t.count;
    if(nbt_writer_tag(w,&t)!=NBT_OK) return 0;
    for(e=chunk->saved_entities;e;e=e->next) {
        if((e->item_entity && !e->item.active) || e->transport.dead) continue;
        if(e->transport.kind) { if(!world_transport_write(w,e)) return 0; continue; }
        if(e->mob.type) { if(!world_mob_write(w,e)) return 0; continue; }
        if(e->raw) {
            WriteItem rewrite; memset(&rewrite,0,sizeof(rewrite)); rewrite.writer=w; rewrite.d=&e->item;
            if(nbt_read(e->raw,e->raw_size,NULL,e->item_entity ? item_rewrite : copy_tag,e->item_entity ? (void *)&rewrite : (void *)w,NULL)!=NBT_OK) return 0;
        } else {
            NbtTag record={0}; record.type=NBT_COMPOUND;
            if(nbt_writer_tag(w,&record)!=NBT_OK) return 0;
            record.type=NBT_STRING; record.name=nbt_span("id"); record.value.bytes=nbt_span("Item");
            if(nbt_writer_tag(w,&record)!=NBT_OK || !numbers(w,"Pos",e->item.x,e->item.y,e->item.z) ||
                !numbers(w,"Motion",e->item.vx/20,e->item.vy/20,e->item.vz/20) ||
                !scalar(w,NBT_SHORT,"Age",(int)(e->item.age*20)) || !scalar(w,NBT_SHORT,"Health",e->item.health) ||
                !item_tag(w,&e->item) || nbt_writer_end(w)!=NBT_OK) return 0;
        }
    }
    return nbt_writer_end(w)==NBT_OK;
}
typedef struct Rewrite { const Chunk *chunk; NbtWriter w; int skip,seen; } Rewrite;
static int rewrite_tag(void *context,NbtEvent event,const NbtTag *t,unsigned depth)
{
    Rewrite *r=(Rewrite *)context;
    if(r->skip) { if(event==NBT_FINISH && depth==2) r->skip=0; return 1; }
    if(event==NBT_BEGIN && depth==2 && t->type==NBT_LIST && named(t,"Entities")) {
        r->skip=r->seen=1; return world_entities_write_list(r->chunk,&r->w);
    }
    if(event==NBT_FINISH && depth==1 && t->type==NBT_COMPOUND && named(t,"Level") && !r->seen)
        if(!world_entities_write_list(r->chunk,&r->w)) return 0;
    return (event==NBT_FINISH ? nbt_writer_end(&r->w) : nbt_writer_tag(&r->w,t))==NBT_OK;
}
int world_entities_rewrite(const Chunk *chunk,const uint8_t *in,size_t size,uint8_t **out,size_t *out_size)
{
    Rewrite r; const SavedEntity *e; size_t capacity=size+64;
    for(e=chunk->saved_entities;e;e=e->next) capacity+=e->raw_size+2048;
    if(capacity>16u*1024u*1024u) return 0;
    *out=(uint8_t *)malloc(capacity); if(!*out) return 0;
    memset(&r,0,sizeof(r)); r.chunk=chunk; nbt_writer_init(&r.w,*out,capacity,NULL);
    if(nbt_read(in,size,NULL,rewrite_tag,&r,NULL)!=NBT_OK || nbt_writer_finish(&r.w,out_size)!=NBT_OK) { free(*out); *out=NULL; return 0; }
    return 1;
}
int world_item_spawn_at(World *w,float x,float y,float z,InventorySlot item)
{
    Chunk *c=owner(w,(int)floorf(x),(int)floorf(z)); SavedEntity *e;
    if(!c || item.id<=0 || item.count<=0) return 0;
    e=(SavedEntity *)calloc(1,sizeof(*e)); if(!e) { w->error=WORLD_ERROR_OUT_OF_MEMORY; return 0; }
    e->item_entity=e->item.active=1; e->item.id=item.id; e->item.count=item.count; e->item.damage=item.damage;
    e->item.x=x; e->item.y=y; e->item.z=z;
    e->item.vy=4; e->item.health=5; e->item.pickup_delay=10;
    e->next=c->saved_entities; c->saved_entities=e; c->entities_modified=1; c->dirty_flags|=CHUNK_DIRTY_SAVE|CHUNK_DIRTY_ENTITIES;
    return 1;
}
int world_item_spawn(World *w,int x,int y,int z,InventorySlot item)
{ return world_item_spawn_at(w,x+.5f,y+.35f,z+.5f,item); }
int world_item_throw(World *w,const Player *p,InventorySlot *held)
{
    Chunk *c;ItemDrop *drop;InventorySlot one;float angle,spread;
    if(w->network_mode || !held || held->id<=0 || held->count<=0) return 0;
    one=*held;one.count=1;
    /* EntityPlayer SP: posY + eyeHeight(.12) - .3, with yOffset 1.62. */
    if(!world_item_spawn_at(w,p->x,p->y+PLAYER_BETA_ENTITY_Y_OFFSET+.12f-.3f,p->z,one)) return 0;
    c=owner(w,(int)floorf(p->x),(int)floorf(p->z));drop=&c->saved_entities->item;
    angle=(float)world_random(w,16777216)/16777216*6.283185307f;
    spread=.4f*(float)world_random(w,16777216)/16777216;
    drop->vx=sinf(p->yaw)*cosf(p->pitch)*6+cosf(angle)*spread;
    drop->vz=-cosf(p->yaw)*cosf(p->pitch)*6+sinf(angle)*spread;
    drop->vy=sinf(p->pitch)*6+2+((float)world_random(w,16777216)-(float)world_random(w,16777216))/16777216*2;
    drop->pickup_delay=40;
    if(--held->count==0) inventory_clear_slot(held);
    return 1;
}
void world_items_tick(World *w,const Player *player,InventorySlot *inventory)
{
    size_t i;
    if(w->network_mode) return;
    for(i=0;i<w->cache_count;++i) {
        Chunk *c=w->cache[i]; SavedEntity **link=&c->saved_entities;
        while(*link) {
            SavedEntity *e=*link; ItemDrop *d=&e->item; int bx,by,bz; float dx,dz;
            if(!e->item_entity) { link=&e->next; continue; }
            if(e->last_tick==w->tick) { link=&e->next; continue; }
            e->last_tick=w->tick;
            if(d->active) {
                d->age+=.05f; if(d->pickup_delay>0) --d->pickup_delay;
                d->vy-=.8f; d->y+=d->vy*.05f;
                d->x+=d->vx*.05f; d->z+=d->vz*.05f;
                bx=(int)floorf(d->x); by=(int)floorf(d->y); bz=(int)floorf(d->z);
                if(world_block_def(world_peek_block(w,bx,by,bz))->solid) { d->y=by+1.001f; d->vy*= -.5f; d->vx*=.588f; d->vz*=.588f; }
                d->vx*=.98f; d->vy*=.98f; d->vz*=.98f;
                if(world_peek_block(w,bx,by,bz)==81 || world_peek_block(w,bx,by,bz)==10 || world_peek_block(w,bx,by,bz)==11) d->health=0;
                dx=d->x-player->x; dz=d->z-player->z;
                if(player->health>0 && d->pickup_delay==0 && dx*dx+dz*dz<1 && fabsf(d->y-player->y-0.9f)<1.3f) {
                    InventorySlot stack={d->id,d->count,d->damage}; int added=inventory_add_stack(inventory,36,stack);
                    d->count-=added;
                    if(added) world_sound(w,"random.pop",d->x,d->y,d->z,.2f,
                        2*((float)world_random(w,10000)/10000-(float)world_random(w,10000)/10000)*.7f+2);
                }
                if(d->count<=0 || d->age>=300 || d->y< -64 || d->health<=0) d->active=0;
                c->entities_modified=1; c->dirty_flags|=CHUNK_DIRTY_SAVE|CHUNK_DIRTY_ENTITIES;
            }
            if(!d->active) { *link=e->next; free_one(e); continue; }
            {
                Chunk *target=owner(w,(int)floorf(d->x),(int)floorf(d->z));
                if(target && target!=c && (!w->beta_format || target->beta_raw)) {
                    *link=e->next; e->next=target->saved_entities; target->saved_entities=e;
                    target->entities_modified=1; target->dirty_flags|=CHUNK_DIRTY_SAVE|CHUNK_DIRTY_ENTITIES; continue;
                }
            }
            link=&e->next;
        }
    }
}
int world_items_visible(const World *w,ItemDrop *out,int count)
{
    size_t i; int n=0;
    for(i=0;i<w->cache_count && n<count;++i) {
        const SavedEntity *e;
        for(e=w->cache[i]->saved_entities;e && n<count;e=e->next) if(e->item_entity && e->item.active) out[n++]=e->item;
    }
    return n;
}

/* Entity.applyEntityCollision pushes horizontally; living mobs are not walls.
 * Store impulses separately so the next movement input cannot discard them. */
void world_entities_collide(World *w,Player *p)
{
    size_t i,j;
    if(w->network_mode || p->health<=0) return;
    for(i=0;i<w->cache_count;++i) {
        SavedEntity *e;
        if(abs(w->cache[i]->x-(int)floorf(p->x/16))>2 || abs(w->cache[i]->z-(int)floorf(p->z/16))>2) continue;
        for(e=w->cache[i]->saved_entities;e;e=e->next) {
          if(e->transport.kind==3 && !e->transport.dead && !e->transport.ridden && !p->riding) {
            float dx=p->x-e->mob.x,dz=p->z-e->mob.z,d=fmaxf(fabsf(dx),fabsf(dz));
            if(p->y<e->mob.y+.3f && p->y+1.8f>e->mob.y-.3f && fabsf(dx)<1.25f && fabsf(dz)<1.25f && d>=.01f) {
                float f=fminf(1,1/sqrtf(d))/sqrtf(d);
                p->push_x+=dx*f; p->push_z+=dz*f;
                e->mob.vx-=dx*f; e->mob.vz-=dz*f; world_transport_changed(w,e);
            }
          }
          if(e->mob.type && e->mob.health>0) {
            MobState *m=&e->mob;
            float half=m->type==52 ? .7f : m->type==93 ? .15f : m->type>=90 ? .45f : .3f;
            float height=m->type==93 ? .4f : m->type==52 || m->type==90 ? .9f : m->type>=90 ? 1.3f : 1.8f;
            float dx=p->x-m->x,dz=p->z-m->z,d=fmaxf(fabsf(dx),fabsf(dz));
            if(!p->riding && p->y<m->y+height && p->y+1.8f>m->y &&
               fabsf(dx)<half+.5f && fabsf(dz)<half+.5f && d>=.01f) {
                float f=fminf(1,1/sqrtf(d))/sqrtf(d);
                p->push_x+=dx*f; p->push_z+=dz*f;
                m->push_x-=dx*f; m->push_z-=dz*f;
            }
            for(j=i;j<w->cache_count;++j) {
                SavedEntity *other;
                if(abs(w->cache[j]->x-w->cache[i]->x)>1 || abs(w->cache[j]->z-w->cache[i]->z)>1) continue;
                for(other=j==i ? e->next : w->cache[j]->saved_entities;other;other=other->next)
                    if(((other->mob.type && other->mob.health>0) || (other->transport.kind==3 && !other->transport.dead)) && fabsf(m->y-other->mob.y)<.9f) {
                        float ox=other->mob.x-m->x,oz=other->mob.z-m->z;
                        float oh=other->transport.kind==3 ? .75f : other->mob.type==52 ? .7f : other->mob.type==93 ? .15f : other->mob.type>=90 ? .45f : .3f;
                        float distance=fmaxf(fabsf(ox),fabsf(oz));
                        if(fabsf(ox)<half+oh+.2f && fabsf(oz)<half+oh+.2f && distance>=.01f) {
                            float f=fminf(1,1/sqrtf(distance))/sqrtf(distance);
                            m->push_x-=ox*f; m->push_z-=oz*f;
                            if(other->transport.kind==3) { other->mob.vx+=ox*f; other->mob.vz+=oz*f; world_transport_changed(w,other); }
                            else { other->mob.push_x+=ox*f; other->mob.push_z+=oz*f; }
                        }
                    }
            }
        }
        }
    }
}
