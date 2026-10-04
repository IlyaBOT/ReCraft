#include "mechanisms.h"
#include "block_entity.h"
#include "entities.h"
#include "redstone.h"
#include "transport.h"
#include "environment.h"
#include "../game/player.h"
#include <math.h>
#include <stdlib.h>
static int plate_overlap(int x,int y,int z,float ex,float ey,float ez,float radius,float height)
{
    return ex+radius>x+.125f && ex-radius<x+.875f && ey+height>y && ey<y+.25f &&
           ez+radius>z+.125f && ez-radius<z+.875f;
}
void world_plate_update(World *w,int x,int y,int z)
{
    unsigned id=world_peek_block(w,x,y,z),old=world_peek_metadata(w,x,y,z);
    int pressed=0; size_t i; const Player *p=w->local_player;
    if(w->network_mode || (id!=70 && id!=72)) return;
    if(!world_block_def(world_peek_block(w,x,y-1,z))->opaque) {
        world_drop_stack(w,x,y,z,(InventorySlot){(int)id,1,0}); world_set_block(w,x,y,z,0); return;
    }
    if(p && p->health>0 && plate_overlap(x,y,z,p->x,p->y,p->z,.3f,1.8f)) pressed=1;
    for(i=0;i<w->cache_count && !pressed;++i) {
        const SavedEntity *e;
        if(abs(w->cache[i]->x-(int)floorf(x/16.0f))>1 || abs(w->cache[i]->z-(int)floorf(z/16.0f))>1) continue;
        for(e=w->cache[i]->saved_entities;e && !pressed;e=e->next) {
            if(e->mob.type && e->mob.health>0) {
                float width,height; mob_dimensions(e->mob.type,&width,&height);
                pressed=plate_overlap(x,y,z,e->mob.x,e->mob.y,e->mob.z,width*.5f,height);
            }
            else if(id==72 && e->item_entity && e->item.active)
                pressed=plate_overlap(x,y,z,e->item.x,e->item.y-.125f,e->item.z,.125f,.25f);
            else if(id==72 && e->transport.kind && !e->transport.dead)
                pressed=plate_overlap(x,y,z,e->mob.x,e->mob.y-(e->transport.kind==2 ? .35f : .125f),e->mob.z,.25f,.5f);
        }
    }
    if(pressed!=(old!=0)) {
        world_set_metadata(w,x,y,z,(uint8_t)pressed);
        world_sound(w,"random.click",x+.5f,y+.1f,z+.5f,.3f,pressed ? .6f : .5f);
    }
    if(pressed) world_schedule_tick(w,x,y,z,(uint8_t)id,20);
}
static void collide_plates(World *w,float px,float py,float pz,float radius,float height)
{
    int x,y,z;
    for(x=(int)floorf(px-radius);x<=(int)floorf(px+radius);++x)
    for(z=(int)floorf(pz-radius);z<=(int)floorf(pz+radius);++z)
    for(y=(int)floorf(py);y<=(int)floorf(py+height);++y) {
        unsigned id=world_peek_block(w,x,y,z);
        if((id==70 || id==72) && !world_peek_metadata(w,x,y,z)) world_plate_update(w,x,y,z);
    }
}
void world_mechanisms_tick(World *w,Player *p)
{
    size_t i;
    w->local_player=p;
    if(w->network_mode) return;
    if(p && p->health>0) collide_plates(w,p->x,p->y,p->z,.3f,1.8f);
    for(i=0;i<w->cache_count;++i) {
        const SavedEntity *e;
        for(e=w->cache[i]->saved_entities;e;e=e->next) {
            if(e->item_entity && e->item.active) collide_plates(w,e->item.x,e->item.y-.125f,e->item.z,.125f,.25f);
            else if(e->mob.type && e->mob.health>0) {
                float width,height; mob_dimensions(e->mob.type,&width,&height);
                collide_plates(w,e->mob.x,e->mob.y,e->mob.z,width*.5f,height);
            }
            else if(e->transport.kind && !e->transport.dead) collide_plates(w,e->mob.x,e->mob.y-.35f,e->mob.z,.75f,.7f);
        }
    }
}
int world_dispenser_powered(const World *w,int x,int y,int z)
{ return world_redstone_power(w,x,y,z,x,y,z,1)>0 || world_redstone_power(w,x,y+1,z,x,y+1,z,1)>0; }
void world_dispenser_tick(World *w,int x,int y,int z)
{
    BlockEntity *e; int i,choice=-1,n=1,dx=0,dz=0,success=0; InventorySlot item;
    unsigned meta=world_peek_metadata(w,x,y,z);
    float px,py=y+.5f,pz;
    if(!world_dispenser_powered(w,x,y,z) || !(e=block_entity_get(w,x,y,z,1))) return;
    for(i=0;i<9;++i) if(e->slots[i].count>0 && world_random(w,(unsigned)n++)==0) choice=i;
    if(choice<0) { world_sound(w,"random.click",x+.5f,py,z+.5f,1,1.2f); return; }
    if(meta==2) dz=-1; else if(meta==3) dz=1; else if(meta==5) dx=1; else dx=-1;
    px=x+.5f+dx*.6f; pz=z+.5f+dz*.6f; item=e->slots[choice]; item.count=1;
    if(item.id==262 || item.id==332 || item.id==344)
        success=world_dispenser_projectile(w,px,py,pz,dx,dz,item.id);
    else if(world_item_spawn_at(w,px,py-.3f,pz,item)) {
        Chunk *c=world_peek_chunk(w,(int)floorf(px/16),(int)floorf(pz/16));
        ItemDrop *drop=&c->saved_entities->item; float speed=.2f+world_random(w,16777216)/16777216.0f*.1f;
        drop->vx=dx*speed*20+world_entity_gaussian(w)*.9f;
        drop->vy=4+world_entity_gaussian(w)*.9f; drop->vz=dz*speed*20+world_entity_gaussian(w)*.9f; success=1;
    }
    if(success) {
        if(--e->slots[choice].count==0) inventory_clear_slot(&e->slots[choice]);
        block_entity_changed(w,e);
        world_sound(w,item.id==262 || item.id==332 || item.id==344 ? "random.bow" : "random.click",x+.5f,py,z+.5f,1,1);
    }
}
