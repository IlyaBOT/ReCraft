#include "mobs.h"
#include "entities.h"
#include "environment.h"
#include "../game/player.h"
#include "../game/entity_render.h"
#include "../game/bed.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>
typedef struct MobDef {
    int type,health,damage,drop; float width,height;
    const char *name,*ambient,*hurt,*death;
} MobDef;
static const MobDef definitions[]={
    {50,20,0,289,.6f,1.8f,"Creeper",NULL,"mob.creeper","mob.creeperdeath"},
    {51,20,0,262,.6f,1.8f,"Skeleton","mob.skeleton","mob.skeletonhurt","mob.skeletonhurt"},
    {52,20,2,287,1.4f,.9f,"Spider","mob.spider","mob.spider","mob.spiderdeath"},
    {54,20,5,288,.6f,1.8f,"Zombie","mob.zombie","mob.zombiehurt","mob.zombiedeath"},
    {90,10,0,319,.9f,.9f,"Pig","mob.pig","mob.pig","mob.pigdeath"},
    {91,10,0,35,.9f,1.3f,"Sheep","mob.sheep","mob.sheep","mob.sheep"},
    {92,10,0,334,.9f,1.3f,"Cow","mob.cow","mob.cowhurt","mob.cowhurt"},
    {93,4,0,288,.3f,.4f,"Chicken","mob.chicken","mob.chickenhurt","mob.chickenhurt"}
};
static const MobDef *definition(int type)
{
    unsigned i; for(i=0;i<sizeof(definitions)/sizeof(definitions[0]);++i) if(definitions[i].type==type) return definitions+i;
    return NULL;
}
int mob_type(const void *name,size_t size)
{
    unsigned i; for(i=0;i<sizeof(definitions)/sizeof(definitions[0]);++i)
        if(strlen(definitions[i].name)==size && !memcmp(name,definitions[i].name,size)) return definitions[i].type;
    return 0;
}
int mob_default_health(int type) { const MobDef *d=definition(type); return d ? d->health : 10; }
static int scalar(NbtWriter *w,NbtType type,const char *name,int value)
{
    NbtTag tag={0}; tag.type=type; tag.name=nbt_span(name);
    if(type==NBT_BYTE) tag.value.byte=(int8_t)value;
    else tag.value.short_value=(int16_t)value;
    return nbt_writer_tag(w,&tag)==NBT_OK;
}
static int list(NbtWriter *w,const char *name,const float *values,unsigned count,int angles)
{
    unsigned i; NbtTag t={0}; t.type=NBT_LIST; t.name=nbt_span(name);
    t.list_type=angles ? NBT_FLOAT : NBT_DOUBLE; t.count=count;
    if(nbt_writer_tag(w,&t)!=NBT_OK) return 0;
    memset(&t,0,sizeof(t)); t.type=angles ? NBT_FLOAT : NBT_DOUBLE;
    for(i=0;i<count;++i) {
        if(angles) t.value.float_value=values[i]; else t.value.double_value=values[i];
        if(nbt_writer_tag(w,&t)!=NBT_OK) return 0;
    }
    return nbt_writer_end(w)==NBT_OK;
}
typedef struct MobWrite { NbtWriter *writer; const MobState *mob; unsigned fields,index; int list; } MobWrite;
static int mob_rewrite(void *context,NbtEvent event,const NbtTag *tag,unsigned depth)
{
    MobWrite *r=(MobWrite *)context; NbtTag t=*tag;
    if(event==NBT_BEGIN && depth==1 && tag->type==NBT_LIST) {
        r->list=0; r->index=0;
        if(tag->name.size==3 && !memcmp(tag->name.data,"Pos",3)) { r->list=1; r->fields|=1; }
        else if(tag->name.size==6 && !memcmp(tag->name.data,"Motion",6)) { r->list=2; r->fields|=2; }
        else if(tag->name.size==8 && !memcmp(tag->name.data,"Rotation",8)) { r->list=3; r->fields|=4; }
    }
    if(event==NBT_VALUE && depth==2 && r->list && r->index<(r->list==3 ? 2u : 3u)) {
        const MobState *m=r->mob;
        float p[3]={m->x,m->y,m->z},v[3]={m->vx/20,m->vy/20,m->vz/20},a[2]={m->yaw,m->pitch};
        if(r->list==3 && t.type==NBT_FLOAT) t.value.float_value=a[r->index++];
        else if(r->list!=3 && t.type==NBT_DOUBLE) t.value.double_value=r->list==1 ? p[r->index++] : v[r->index++];
    }
    if(event==NBT_VALUE && depth==1) {
        if(t.type==NBT_SHORT && t.name.size==6 && !memcmp(t.name.data,"Health",6)) { t.value.short_value=(int16_t)r->mob->health; r->fields|=8; }
        if(t.type==NBT_SHORT && t.name.size==4 && !memcmp(t.name.data,"Fire",4)) { t.value.short_value=(int16_t)r->mob->fire; r->fields|=16; }
        if(t.type==NBT_BYTE && t.name.size==8 && !memcmp(t.name.data,"OnGround",8)) { t.value.byte=(int8_t)r->mob->on_ground; r->fields|=32; }
        if(t.type==NBT_BYTE && t.name.size==5 && !memcmp(t.name.data,"Color",5)) { t.value.byte=(int8_t)r->mob->color; r->fields|=64; }
        if(t.type==NBT_BYTE && t.name.size==7 && !memcmp(t.name.data,"Sheared",7)) { t.value.byte=(int8_t)r->mob->sheared; r->fields|=128; }
    }
    if(event==NBT_FINISH && depth==1 && t.type==NBT_LIST) r->list=0;
    if(event==NBT_FINISH && depth==0) {
        const MobState *m=r->mob; float p[3]={m->x,m->y,m->z},v[3]={m->vx/20,m->vy/20,m->vz/20},a[2]={m->yaw,m->pitch};
        if(!(r->fields&1) && !list(r->writer,"Pos",p,3,0)) return 0;
        if(!(r->fields&2) && !list(r->writer,"Motion",v,3,0)) return 0;
        if(!(r->fields&4) && !list(r->writer,"Rotation",a,2,1)) return 0;
        if(!(r->fields&8) && !scalar(r->writer,NBT_SHORT,"Health",m->health)) return 0;
        if(!(r->fields&16) && !scalar(r->writer,NBT_SHORT,"Fire",m->fire)) return 0;
        if(!(r->fields&32) && !scalar(r->writer,NBT_BYTE,"OnGround",m->on_ground)) return 0;
        if(m->type==91) {
            if(!(r->fields&64) && !scalar(r->writer,NBT_BYTE,"Color",m->color)) return 0;
            if(!(r->fields&128) && !scalar(r->writer,NBT_BYTE,"Sheared",m->sheared)) return 0;
        }
    }
    return (event==NBT_FINISH ? nbt_writer_end(r->writer) : nbt_writer_tag(r->writer,&t))==NBT_OK;
}
int world_mob_write(NbtWriter *w,const SavedEntity *entity)
{
    MobWrite r; const MobDef *d=definition(entity->mob.type); NbtTag t={0};
    if(!d) return 0;
    memset(&r,0,sizeof(r)); r.writer=w; r.mob=&entity->mob;
    if(entity->raw) return nbt_read(entity->raw,entity->raw_size,NULL,mob_rewrite,&r,NULL)==NBT_OK;
    t.type=NBT_COMPOUND; if(nbt_writer_tag(w,&t)!=NBT_OK) return 0;
    t.type=NBT_STRING; t.name=nbt_span("id"); t.value.bytes=nbt_span(d->name);
    if(nbt_writer_tag(w,&t)!=NBT_OK) return 0;
    t.type=NBT_COMPOUND; return mob_rewrite(&r,NBT_FINISH,&t,0);
}
static void dirty(Chunk *c) { c->entities_modified=1; c->dirty_flags|=CHUNK_DIRTY_ENTITIES|CHUNK_DIRTY_SAVE; }
static Chunk *owner(const World *w,float x,float z) { return world_peek_chunk(w,(int)floorf(x/16),(int)floorf(z/16)); }
int world_mob_spawn(World *w,int type,float x,float y,float z)
{
    Chunk *c=owner(w,x,z); SavedEntity *e;
    if(w->network_mode || !definition(type) || !c || (w->beta_format && !c->beta_raw)) return 0;
    e=(SavedEntity *)calloc(1,sizeof(*e)); if(!e) return 0;
    e->mob.type=type; e->mob.health=mob_default_health(type); e->mob.x=x; e->mob.y=y; e->mob.z=z;
    e->mob.runtime_id=++w->next_entity_id; e->next=c->saved_entities; c->saved_entities=e; dirty(c); return 1;
}
static int collides(const World *w,const MobState *m,float x,float y,float z)
{
    const MobDef *d=definition(m->type); int a,b,c;
    float radius=d->width*.5f;
    for(b=(int)floorf(y+.001f);b<=(int)floorf(y+d->height-.001f);++b)
        for(a=(int)floorf(x-radius+.001f);a<=(int)floorf(x+radius-.001f);++a)
            for(c=(int)floorf(z-radius+.001f);c<=(int)floorf(z+radius-.001f);++c) {
                Chunk *chunk=owner(w,(float)a+.5f,(float)c+.5f);
                unsigned id=world_peek_block(w,a,b,c);
                BetaBlockBox box={0,0,0,1,1,1};
                /* Missing Beta chunks are not simulated air. Keep the whole
                 * body in loaded terrain until the streamer brings it in. */
                if(!chunk || (w->beta_format && !chunk->beta_raw)) return 1;
                if(!world_block_def(id)->solid) continue;
                beta_block_selection_box((BetaBlockState){(uint8_t)id,world_peek_metadata(w,a,b,c)},&box);
                if(x+radius>a+box.min_x && x-radius<a+box.max_x &&
                   y+d->height>b+box.min_y && y<b+box.max_y &&
                   z+radius>c+box.min_z && z-radius<c+box.max_z) return 1;
            }
    return 0;
}
static int move_axis(const World *w,MobState *m,int axis,float delta)
{
    float *position=axis==0 ? &m->x : axis==1 ? &m->y : &m->z;
    int steps,i;
    /* Bound work even for a corrupt but finite Motion tag. Normal Beta mob
     * velocities never reach sixteen blocks in one tick. */
    delta=fmaxf(-16,fminf(16,delta)); steps=(int)ceilf(fabsf(delta)*4);
    for(i=0;i<steps;++i) {
        float start=*position,portion=delta/steps,lo=0,hi=1; int j;
        *position=start+portion;
        if(!collides(w,m,m->x,m->y,m->z)) continue;
        for(j=0;j<8;++j) {
            float mid=(lo+hi)*.5f; *position=start+portion*mid;
            if(collides(w,m,m->x,m->y,m->z)) hi=mid; else lo=mid;
        }
        *position=start+portion*lo; return 1;
    }
    return 0;
}
static int can_see(const World *w,const MobState *m,const Player *p)
{
    int i; float dx=p->x-m->x,dy=p->y+1.62f-m->y-definition(m->type)->height*.85f,dz=p->z-m->z;
    int count=(int)(sqrtf(dx*dx+dy*dy+dz*dz)*8)+1;
    for(i=1;i<count;++i) {
        float t=(float)i/count;
        if(world_block_def(world_peek_block(w,(int)floorf(m->x+dx*t),
            (int)floorf(m->y+definition(m->type)->height*.85f+dy*t),(int)floorf(m->z+dz*t)))->solid) return 0;
    }
    return 1;
}
static void damage(World *w,Chunk *c,MobState *m,int amount)
{
    const MobDef *d=definition(m->type); int applied=amount;
    if(m->health<=0 || amount<=0) return;
    if(m->hurt_ticks>10) { if(amount<=m->last_damage) return; applied-=m->last_damage; }
    else m->hurt_ticks=20;
    m->health-=applied; m->last_damage=amount;
    if(m->health<=0) {
        int count=(int)world_random(w,3),drop=d->drop;
        m->health=0;
        if(m->type==91) count=m->sheared ? 0 : 1;
        if(m->type==90 && m->fire>0) drop=320;
        if(count) world_drop_stack(w,(int)floorf(m->x),(int)floorf(m->y),(int)floorf(m->z),(InventorySlot){drop,count,m->type==91 ? m->color : 0});
        if(m->type==51 && (count=(int)world_random(w,3))>0)
            world_drop_stack(w,(int)floorf(m->x),(int)floorf(m->y),(int)floorf(m->z),(InventorySlot){352,count,0});
    }
    world_sound(w,m->health<=0 ? d->death : d->hurt,m->x,m->y,m->z,d->type==92 ? .4f : 1,1);
    dirty(c);
}
void world_mobs_tick(World *w,Player *p)
{
    size_t i; unsigned processed=0;
    if(w->network_mode) return;
    for(i=0;i<w->cache_count;++i) {
        Chunk *c=w->cache[i]; SavedEntity **link=&c->saved_entities;
        while(*link) {
            SavedEntity *e=*link; MobState *m=&e->mob; const MobDef *d=definition(m->type);
            float dx,dz,distance,speed=1.4f; Chunk *target;
            if(!d || e->last_tick==w->tick) { link=&e->next; continue; }
            dx=p->x-m->x; dz=p->z-m->z; distance=sqrtf(dx*dx+dz*dz);
            if((m->type<90 && w->difficulty==0) || m->health<=0) {
                *link=e->next; free(e->raw); free(e); dirty(c); continue;
            }
            if(distance>32 || processed++>=128) { link=&e->next; continue; }
            e->last_tick=w->tick;
            if(m->hurt_ticks>0) --m->hurt_ticks;
            if(m->attack_ticks>0) --m->attack_ticks;
            ++m->age;
            if(m->type==93) {
                if(!m->egg_ticks) m->egg_ticks=6000+(int)world_random(w,6000);
                if(--m->egg_ticks==0) {
                    world_drop_stack(w,(int)floorf(m->x),(int)floorf(m->y),(int)floorf(m->z),(InventorySlot){344,1,0});
                    world_sound(w,"mob.chickenplop",m->x,m->y,m->z,1,
                        ((float)world_random(w,16777216)-(float)world_random(w,16777216))/16777216*.2f+1);
                    m->egg_ticks=6000+(int)world_random(w,6000);
                }
            }
            /* This shared movement is a bounded foundation. Beta pathfinding,
             * skeleton arrows and creeper explosions are separate pending work. */
            if(d->damage && distance<16 && p->health>0 && !p->creative &&
                (m->type!=52 || !world_is_daytime(w)) && can_see(w,m,p)) {
                m->yaw=atan2f(dx,-dz)*57.2957795131f+180;
                m->vx=distance>.01f ? dx/distance*speed : 0; m->vz=distance>.01f ? dz/distance*speed : 0;
                if(distance<2 && fabsf(p->y-m->y)<d->height && !m->attack_ticks) {
                    int health=p->health;
                    player_wake(p,w,0); player_mob_damage(p,w,d->damage); m->attack_ticks=20;
                    if(p->health<health) world_sound(w,"random.hurt",p->x,p->y,p->z,1,1);
                }
            } else {
                if(!m->walk_ticks) {
                    m->walk_ticks=40+(int)world_random(w,100);
                    m->yaw=(float)world_random(w,360);
                    speed=world_random(w,3)==0 ? 0 : .8f;
                    m->vx=-sinf(m->yaw*.01745329252f)*speed; m->vz=cosf(m->yaw*.01745329252f)*speed;
                } else --m->walk_ticks;
            }
            if((m->type==54 || m->type==51) && world_is_daytime(w)) {
                unsigned kind; int roof=world_precipitation_height(w,(int)floorf(m->x),(int)floorf(m->z),&kind);
                if(roof<=m->y+1 && world_random(w,30)==0) m->fire=300;
            }
            if(m->fire>0) { if(m->fire%20==0) damage(w,c,m,1); --m->fire; }
            if(move_axis(w,m,0,m->vx*.05f) && m->on_ground) m->vy=8.4f;
            if(move_axis(w,m,2,m->vz*.05f) && m->on_ground) m->vy=8.4f;
            m->vy=(m->vy-1.6f)*.98f; m->on_ground=0;
            if(m->type==93 && m->vy<0) m->vy*=.6f;
            {
                float before=m->y; int blocked=move_axis(w,m,1,m->vy*.05f);
                if(m->y<before) m->fall_distance+=before-m->y;
                if(blocked) {
                    if(m->vy<0) {
                        m->on_ground=1;
                        if(m->type!=93) damage(w,c,m,(int)ceilf(m->fall_distance-3));
                        m->fall_distance=0;
                    }
                    m->vy=0;
                }
            }
            m->walk+=sqrtf(m->vx*m->vx+m->vz*m->vz)*.3f;
            if((int)world_random(w,1000)<m->sound_ticks++) {
                m->sound_ticks=-80;
                if(d->ambient) world_sound(w,d->ambient,m->x,m->y,m->z,m->type==92 ? .4f : 1,
                    ((float)world_random(w,16777216)/16777216-(float)world_random(w,16777216)/16777216)*.2f+1);
            }
            if(m->y< -64) damage(w,c,m,4);
            dirty(c); target=owner(w,m->x,m->z);
            if(target && target!=c && (!w->beta_format || target->beta_raw)) {
                *link=e->next; e->next=target->saved_entities; target->saved_entities=e; dirty(target); continue;
            }
            link=&e->next;
        }
    }
}
int beta_attack_damage(int item)
{
    static const int swords[5]={268,272,267,276,283},axes[5]={271,275,258,279,286};
    static const int picks[5]={270,274,257,278,285},shovels[5]={269,273,256,277,284};
    int i; for(i=0;i<5;++i) {
        int tier=i==4 ? 0 : i;
        if(item==swords[i]) return 4+tier*2;
        if(item==axes[i]) return 3+tier;
        if(item==picks[i]) return 2+tier;
        if(item==shovels[i]) return 1+tier;
    }
    return 1;
}
static SavedEntity *mob_hit(World *w,Player *p,float reach,Chunk **chunk)
{
    size_t i; SavedEntity *best=NULL;
    float direction[3]={sinf(p->yaw)*cosf(p->pitch),sinf(p->pitch),-cosf(p->yaw)*cosf(p->pitch)};
    float origin[3]={p->x,p->y+1.62f,p->z},nearest=reach;
    BlockHit hit=player_raycast(p,w,reach);
    if(hit.hit) nearest=fminf(nearest,hit.distance);
    for(i=0;i<w->cache_count;++i) {
        SavedEntity *e;
        for(e=w->cache[i]->saved_entities;e;e=e->next) if(e->mob.type && e->mob.health>0) {
            const MobDef *d=definition(e->mob.type); int axis;
            float lo[3]={e->mob.x-d->width*.5f-.1f,e->mob.y-.1f,e->mob.z-d->width*.5f-.1f};
            float hi[3]={e->mob.x+d->width*.5f+.1f,e->mob.y+d->height+.1f,e->mob.z+d->width*.5f+.1f};
            float enter=0,leave=nearest;
            for(axis=0;axis<3;++axis) {
                if(fabsf(direction[axis])<1e-6f) { if(origin[axis]<lo[axis] || origin[axis]>hi[axis]) break; }
                else {
                    float a=(lo[axis]-origin[axis])/direction[axis],b=(hi[axis]-origin[axis])/direction[axis];
                    if(a>b) { float t=a; a=b; b=t; }
                    enter=fmaxf(enter,a); leave=fminf(leave,b); if(enter>leave) break;
                }
            }
            if(axis==3 && enter<nearest) { nearest=enter; best=e; *chunk=w->cache[i]; }
        }
    }
    return best;
}
int world_mobs_interact(World *w,Player *p,InventorySlot *held,float reach)
{
    Chunk *c=NULL; SavedEntity *e;
    if(w->network_mode || !held || held->count<=0 || !(e=mob_hit(w,p,reach,&c))) return 0;
    if(e->mob.type==92 && held->id==325) { *held=(InventorySlot){335,1,0}; return 1; }
    if(e->mob.type==91 && held->id==359 && !e->mob.sheared) {
        e->mob.sheared=1; dirty(c);
        world_drop_stack(w,(int)floorf(e->mob.x),(int)floorf(e->mob.y),(int)floorf(e->mob.z),
            (InventorySlot){35,2+(int)world_random(w,3),e->mob.color});
        if(!p->creative && inventory_damage(held,1))
            world_sound(w,"random.break",p->x,p->y,p->z,.8f,1);
        return 1;
    }
    return 0;
}
int world_mobs_attack(World *w,Player *p,InventorySlot *held,float reach)
{
    Chunk *chunk=NULL; SavedEntity *best;
    if(w->network_mode || !held || !(best=mob_hit(w,p,reach,&chunk))) return 0;
    damage(w,chunk,&best->mob,p->creative ? 1000 : beta_attack_damage(held->id));
    if(!p->creative) {
        int sword=(held->id==268 || held->id==272 || held->id==267 || held->id==276 || held->id==283);
        int tool=(held->id>=256 && held->id<=258) || (held->id>=269 && held->id<=271) ||
            (held->id>=273 && held->id<=275) || (held->id>=277 && held->id<=279) || (held->id>=284 && held->id<=286);
        int wear=sword ? 1 : 2;
        if((sword || tool) && inventory_damage(held,wear))
            world_sound(w,"random.break",p->x,p->y,p->z,.8f,1);
    }
    return 1;
}
int world_mobs_visible(World *w,RenderEntity *out,int capacity)
{
    size_t i; int count=0;
    for(i=0;i<w->cache_count && count<capacity;++i) {
        SavedEntity *e;
        for(e=w->cache[i]->saved_entities;e && count<capacity;e=e->next) if(e->mob.type && e->mob.health>0) {
            RenderEntity *r=&out[count++]; MobState *m=&e->mob;
            if(!m->runtime_id) m->runtime_id=++w->next_entity_id;
            if(r->id!=m->runtime_id) memset(r,0,sizeof(*r));
            r->active=1; r->id=m->runtime_id; r->type=m->type;
            r->x=m->x; r->y=m->y; r->z=m->z; r->yaw=m->yaw; r->pitch=m->pitch; r->color=m->color; r->sheared=m->sheared;
        }
    }
    for(i=(size_t)count;i<(size_t)capacity;++i) out[i].active=0;
    return count;
}
