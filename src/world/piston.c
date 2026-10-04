#include "piston.h"
#include "block_entity.h"
#include "redstone.h"
#include "entities.h"
#include "environment.h"
#include "mobs.h"
#include "../game/player.h"
#include "../game/mining.h"
#include <math.h>
#include <stdlib.h>
static const int dx[6]={0,0,0,0,-1,1},dy[6]={-1,1,0,0,0,0},dz[6]={0,0,-1,1,0,0};
static int present(const World *w,int x,int y,int z)
{
    Chunk *c=world_peek_chunk(w,(int)floorf(x/16.0f),(int)floorf(z/16.0f));
    return y>0 && y<127 && c && (!w->beta_format || c->beta_raw);
}
static int mobility(unsigned id)
{
    switch(id) {
    case 34: case 36: case 90: return 2;
    case 6: case 8: case 9: case 10: case 11: case 18: case 26: case 27: case 28:
    case 30: case 31: case 32: case 37: case 38: case 39: case 40: case 50: case 51:
    case 55: case 59: case 64: case 65: case 66: case 69: case 70: case 71: case 72:
    case 75: case 76: case 77: case 78: case 81: case 83: case 86: case 91: case 92:
    case 93: case 94: return 1;
    default: return 0;
    }
}
static int can_push(World *w,int x,int y,int z,int destroy)
{
    unsigned id=world_peek_block(w,x,y,z);
    if(!present(w,x,y,z) || id==49 || mining_hardness(id)<0 || mobility(id)==2 || (!destroy && mobility(id)==1)) return 0;
    if((id==29 || id==33) && (world_peek_metadata(w,x,y,z)&8)) return 0;
    if(id==23 || id==25 || id==52 || id==54 || id==61 || id==62 || id==63 || id==68 || id==84) return 0;
    return !block_entity_get(w,x,y,z,0);
}
static int extent(World *w,int x,int y,int z,int face)
{
    int n;
    for(n=1;n<=13;++n) {
        unsigned id; int nx=x+dx[face]*n,ny=y+dy[face]*n,nz=z+dz[face]*n;
        if(!present(w,nx,ny,nz)) return -1;
        id=world_peek_block(w,nx,ny,nz);
        if(!id) return n;
        if(!can_push(w,nx,ny,nz,1)) return -1;
        if(mobility(id)==1) return n;
        if(n==13) return -1;
    }
    return -1;
}
static int powered(const World *w,int x,int y,int z,int face)
{
    int n;
    for(n=0;n<6;++n) if(n!=face && world_redstone_signal(w,x+dx[n],y+dy[n],z+dz[n],x,y,z,1)) return 1;
    /* Beta's quasi-connectivity: neighbours of the block above the piston. */
    if(world_redstone_signal(w,x,y,z,x,y+1,z,1)) return 1;
    for(n=1;n<6;++n) if(world_redstone_signal(w,x+dx[n],y+1+dy[n],z+dz[n],x,y+1,z,1)) return 1;
    return 0;
}
void world_piston_changed(World *w,int x,int y,int z)
{
    unsigned id=world_peek_block(w,x,y,z),meta=world_peek_metadata(w,x,y,z),i; int face=meta&7,on;
    if(w->network_mode || w->piston_updating || face>5) return;
    if(id==34) {
        unsigned base=world_peek_block(w,x-dx[face],y-dy[face],z-dz[face]);
        if((base!=29 && base!=33) || !(world_peek_metadata(w,x-dx[face],y-dy[face],z-dz[face])&8)) world_set_block(w,x,y,z,0);
        return;
    }
    if(id!=29 && id!=33) return;
    on=powered(w,x,y,z,face);
    if(on==!!(meta&8) || (on && extent(w,x,y,z,face)<0)) return;
    if(w->piston_event_count==512) return;
    for(i=0;i<w->piston_event_count;++i) if(w->piston_events[i].x==x && w->piston_events[i].y==y && w->piston_events[i].z==z && w->piston_events[i].action==!on) return;
    w->piston_updating=1; world_set_metadata(w,x,y,z,(uint8_t)(face|(on ? 8 : 0))); w->piston_updating=0;
    i=w->piston_event_count++; w->piston_events[i].x=x; w->piston_events[i].y=y; w->piston_events[i].z=z;
    w->piston_events[i].id=(int)id; w->piston_events[i].action=!on; w->piston_events[i].facing=face;
}
static void moving(World *w,int x,int y,int z,unsigned id,unsigned meta,int face,int extending,int base)
{
    BlockEntity *e;
    world_set_state(w,x,y,z,(BetaBlockState){36,(uint8_t)meta});
    e=block_entity_get(w,x,y,z,1);
    if(!e) { w->error=WORLD_ERROR_OUT_OF_MEMORY; return; }
    e->piston_id=(int)id; e->piston_data=(int)meta; e->piston_facing=face;
    e->piston_extending=extending; e->piston_base=base; e->piston_progress=e->piston_previous=0;
    block_entity_changed(w,e);
}
static void finish(World *w,int x,int y,int z)
{
    BlockEntity *e=block_entity_get(w,x,y,z,0);
    if(e && e->kind==BLOCK_ENTITY_PISTON && e->piston_id>0 && e->piston_id<97)
        world_set_state(w,x,y,z,(BetaBlockState){(uint8_t)e->piston_id,(uint8_t)e->piston_data});
}
void world_piston_events(World *w)
{
    unsigned i=0;
    if(w->network_mode) return;
    while(i<w->piston_event_count && i<512) {
        int x=w->piston_events[i].x,y=w->piston_events[i].y,z=w->piston_events[i].z;
        int face=w->piston_events[i].facing,id=w->piston_events[i].id,retract=w->piston_events[i++].action;
        int fx=x+dx[face],fy=y+dy[face],fz=z+dz[face],n;
        if(world_peek_block(w,x,y,z)!=id) continue;
        w->piston_updating=1;
        if(!retract) {
            n=extent(w,x,y,z,face);
            if(n<0) { world_set_metadata(w,x,y,z,(uint8_t)face); w->piston_updating=0; continue; }
            if(world_peek_block(w,x+dx[face]*n,y+dy[face]*n,z+dz[face]*n)) {
                BetaBlockState broken={world_peek_block(w,x+dx[face]*n,y+dy[face]*n,z+dz[face]*n),world_peek_metadata(w,x+dx[face]*n,y+dy[face]*n,z+dz[face]*n)};
                InventorySlot drop=mining_drop(broken,278,world_random(w,UINT32_MAX));
                world_drop_stack(w,x+dx[face]*n,y+dy[face]*n,z+dz[face]*n,drop);
                world_set_block(w,x+dx[face]*n,y+dy[face]*n,z+dz[face]*n,0);
            }
            for(;n>0;--n) {
                int nx=x+dx[face]*n,ny=y+dy[face]*n,nz=z+dz[face]*n;
                unsigned bid=world_peek_block(w,nx-dx[face],ny-dy[face],nz-dz[face]);
                unsigned meta=world_peek_metadata(w,nx-dx[face],ny-dy[face],nz-dz[face]);
                moving(w,nx,ny,nz,n==1 ? 34 : bid,n==1 ? (unsigned)(face|(id==29 ? 8 : 0)) : meta,face,1,0);
            }
            world_set_metadata(w,x,y,z,(uint8_t)(face|8));
        } else {
            unsigned bid,meta; int pulled_moving=0;
            finish(w,fx,fy,fz);
            moving(w,x,y,z,(unsigned)id,(unsigned)face,face,0,1);
            bid=world_peek_block(w,x+dx[face]*2,y+dy[face]*2,z+dz[face]*2);
            meta=world_peek_metadata(w,x+dx[face]*2,y+dy[face]*2,z+dz[face]*2);
            if(id==29 && bid==36) {
                BlockEntity *e=block_entity_get(w,x+dx[face]*2,y+dy[face]*2,z+dz[face]*2,0);
                if(e && e->kind==BLOCK_ENTITY_PISTON && e->piston_facing==face && e->piston_extending) {
                    bid=(unsigned)e->piston_id; meta=(unsigned)e->piston_data; finish(w,e->x,e->y,e->z); pulled_moving=1;
                }
            }
            world_set_block(w,fx,fy,fz,0);
            if(id==29 && !pulled_moving && bid && can_push(w,x+dx[face]*2,y+dy[face]*2,z+dz[face]*2,0)) {
                world_set_block(w,x+dx[face]*2,y+dy[face]*2,z+dz[face]*2,0);
                moving(w,fx,fy,fz,bid,meta,face,0,0);
            }
        }
        world_sound(w,retract ? "tile.piston.in" : "tile.piston.out",x+.5f,y+.5f,z+.5f,.5f,
                    (retract ? .6f : .6f)+world_random(w,16777216)/16777216.0f*(retract ? .15f : .25f));
        w->piston_updating=0;
        /* Publish the completed block event before any neighbour callbacks. */
        for(n=0;n<=13;++n) if(present(w,x+dx[face]*n,y+dy[face]*n,z+dz[face]*n))
            world_physics_notify(w,x+dx[face]*n,y+dy[face]*n,z+dz[face]*n);
    }
    w->piston_event_count=0;
}
static BlockEntity *tile(const World *w,int x,int y,int z)
{
    Chunk *c=world_peek_chunk(w,(int)floorf(x/16.0f),(int)floorf(z/16.0f)); BlockEntity *e;
    if(!c) return NULL;
    for(e=c->entities;e;e=e->next) if(e->x==x && e->y==y && e->z==z) return e;
    return NULL;
}
int world_block_collision_boxes(const World *w,int x,int y,int z,BetaBlockBox boxes[2])
{
    unsigned id=world_peek_block(w,x,y,z),meta=world_peek_metadata(w,x,y,z);
    if(w->piston_push_active && x==w->piston_push_x && y==w->piston_push_y && z==w->piston_push_z) return 0;
    if(id==36) {
        BlockEntity *e=tile(w,x,y,z); float off; int face;
        if(!e || e->kind!=BLOCK_ENTITY_PISTON || e->piston_id<=0 || e->piston_id>=97 || (face=e->piston_facing)>5 || face<0 || !world_block_def((uint8_t)e->piston_id)->solid) return 0;
        if(!beta_block_selection_box((BetaBlockState){(uint8_t)e->piston_id,(uint8_t)e->piston_data},boxes)) boxes[0]=(BetaBlockBox){0,0,0,1,1,1};
        off=e->piston_extending ? e->piston_progress-1 : 1-e->piston_progress;
        boxes[0].min_x+=off*dx[face]; boxes[0].max_x+=off*dx[face];
        boxes[0].min_y+=off*dy[face]; boxes[0].max_y+=off*dy[face];
        boxes[0].min_z+=off*dz[face]; boxes[0].max_z+=off*dz[face]; return 1;
    }
    if(id==78) {
        if((meta&7)<3) return 0;
        boxes[0]=(BetaBlockBox){0,0,0,1,.5f,1}; return 1;
    }
    if(beta_block_stair_boxes((BetaBlockState){(uint8_t)id,(uint8_t)meta},boxes)) return 2;
    if(!world_block_def((uint8_t)id)->solid) return 0;
    if(id==85) { boxes[0]=(BetaBlockBox){0,0,0,1,1.5f,1}; return 1; }
    if(id==88) { boxes[0]=(BetaBlockBox){0,0,0,1,.875f,1}; return 1; }
    if(id==92) {
        beta_block_selection_box((BetaBlockState){(uint8_t)id,(uint8_t)meta},boxes);
        boxes[0].max_y=.4375f; return 1;
    }
    if(id==44 || id==81 || id==26 || id==64 || id==71 || id==29 || id==33 || id==34 || id==96 || id==65 || id==92) {
        if(!beta_block_selection_box((BetaBlockState){(uint8_t)id,(uint8_t)meta},boxes)) return 0;
    } else boxes[0]=(BetaBlockBox){0,0,0,1,1,1};
    if(id==34) {
        int face=meta&7;
        boxes[1]=(BetaBlockBox){.375f,.375f,.375f,.625f,.625f,.625f};
        if(face<2) { boxes[1].min_y=0; boxes[1].max_y=1; }
        else if(face<4) { boxes[1].min_z=0; boxes[1].max_z=1; }
        else { boxes[1].min_x=0; boxes[1].max_x=1; }
        return 2;
    }
    return 1;
}
static int intersects(const BetaBlockBox *b,float x,float y,float z,float r,float h)
{ return x+r>b->min_x && x-r<b->max_x && y+h>b->min_y && y<b->max_y && z+r>b->min_z && z-r<b->max_z; }
/* Entity.moveEntity clips each axis using the entity's own bounding box.
 * A chicken, an item and a cart must not inherit the player's 1.8-block body. */
static float push_axis(const World *w,float x,float y,float z,float radius,float height,int axis,float delta)
{
    float lo[3]={x-radius,y,z-radius},hi[3]={x+radius,y+height,z+radius};
    int a,b,c,i,k;
    for(a=(int)floorf(lo[0]+fminf(0,axis==0 ? delta : 0))-1;a<=(int)floorf(hi[0]+fmaxf(0,axis==0 ? delta : 0))+1;++a)
    for(b=(int)floorf(lo[1]+fminf(0,axis==1 ? delta : 0))-1;b<=(int)floorf(hi[1]+fmaxf(0,axis==1 ? delta : 0))+1;++b)
    for(c=(int)floorf(lo[2]+fminf(0,axis==2 ? delta : 0))-1;c<=(int)floorf(hi[2]+fmaxf(0,axis==2 ? delta : 0))+1;++c) {
        BetaBlockBox boxes[2]; int n=world_block_collision_boxes(w,a,b,c,boxes);
        for(i=0;i<n;++i) {
            float bl[3]={a+boxes[i].min_x,b+boxes[i].min_y,c+boxes[i].min_z};
            float bh[3]={a+boxes[i].max_x,b+boxes[i].max_y,c+boxes[i].max_z};
            for(k=0;k<3;++k) if(k!=axis && (hi[k]<=bl[k] || lo[k]>=bh[k])) break;
            if(k!=3) continue;
            if(delta>0 && hi[axis]<=bl[axis]) delta=fminf(delta,bl[axis]-hi[axis]);
            else if(delta<0 && lo[axis]>=bh[axis]) delta=fmaxf(delta,bh[axis]-lo[axis]);
        }
    }
    return delta;
}
static void push_entities(World *w,BlockEntity *e,float amount)
{
    BetaBlockBox b[2]; int face=e->piston_facing; size_t i; Player *p=w->local_player;
    if(!world_block_collision_boxes(w,e->x,e->y,e->z,b)) return;
    b[0].min_x+=e->x; b[0].max_x+=e->x; b[0].min_y+=e->y; b[0].max_y+=e->y; b[0].min_z+=e->z; b[0].max_z+=e->z;
    w->piston_push_active=1; w->piston_push_x=e->x; w->piston_push_y=e->y; w->piston_push_z=e->z;
    if(p && intersects(b,p->x,p->y,p->z,.3f,1.8f)) player_piston_move(p,w,dx[face]*amount,dy[face]*amount,dz[face]*amount);
    for(i=0;i<w->cache_count;++i) {
        SavedEntity *s;
        for(s=w->cache[i]->saved_entities;s;s=s->next) {
            float *x,*y,*z,width=.25f,height=.25f,y_offset=.125f;
            if(s->item_entity && s->item.active) { x=&s->item.x; y=&s->item.y; z=&s->item.z; }
            else if((s->mob.type && s->mob.health>0) || (s->transport.kind && !s->transport.dead)) {
                x=&s->mob.x; y=&s->mob.y; z=&s->mob.z;
                if(s->transport.kind) {
                    int kind=s->transport.kind;
                    width=kind==3 ? 1.5f : kind==2 || kind==4 || kind==7 ? .98f : .5f;
                    height=kind==3 ? .6f : kind==2 ? .7f : kind==4 || kind==7 ? .98f : .5f;
                    y_offset=kind>=2 && kind<=4 ? height*.5f : kind==7 ? .49f : 0;
                } else { mob_dimensions(s->mob.type,&width,&height); y_offset=0; }
            }
            else continue;
            if(intersects(b,*x,*y-y_offset,*z,width*.5f,height)) {
                int axis=dy[face] ? 1 : dz[face] ? 2 : 0;
                float movement=push_axis(w,*x,*y-y_offset,*z,width*.5f,height,axis,(dx[face]+dy[face]+dz[face])*amount);
                if(axis==0) *x+=movement; else if(axis==1) *y+=movement; else *z+=movement;
                w->cache[i]->entities_modified=1; w->cache[i]->dirty_flags|=CHUNK_DIRTY_SAVE|CHUNK_DIRTY_ENTITIES;
            }
        }
    }
    w->piston_push_active=0;
}
void world_pistons_tick(World *w)
{
    size_t i;
    if(w->network_mode) return;
    /* Resolve progress before finalizing tiles; callbacks may replace/remove
     * another tile, so restart the list after each completed piston. */
    for(i=0;i<w->cache_count;++i) {
        BlockEntity *e=w->cache[i]->entities;
        while(e) {
            if(e->kind!=BLOCK_ENTITY_PISTON) { e=e->next; continue; }
            if(e->piston_tick==w->tick+1) { e=e->next; continue; }
            e->piston_tick=w->tick+1;
            if(e->piston_facing<0 || e->piston_facing>5 || e->piston_id<=0 || e->piston_id>=BETA_BLOCK_COUNT || !isfinite(e->piston_progress)) { e=e->next; continue; }
            e->piston_previous=e->piston_progress;
            if(e->piston_progress>=1) {
                int x=e->x,y=e->y,z=e->z;
                push_entities(w,e,.25f); finish(w,x,y,z); e=w->cache[i]->entities; continue;
            }
            e->piston_progress=fminf(1,e->piston_progress+.5f);
            if(e->piston_extending) push_entities(w,e,e->piston_progress-e->piston_previous+.0625f);
            block_entity_changed(w,e); w->cache[i]->dirty_flags|=CHUNK_DIRTY_MESH; ++w->cache[i]->revision;
            e=e->next;
        }
    }
}
void world_piston_removed(World *w,int x,int y,int z,unsigned old,unsigned meta,unsigned replacement)
{
    unsigned face=meta&7; int nx,ny,nz; unsigned base;
    if(w->network_mode || w->piston_updating || face>5) return;
    if((old==29 || old==33) && (meta&8) && replacement!=36) {
        nx=x+dx[face]; ny=y+dy[face]; nz=z+dz[face];
        if(world_peek_block(w,nx,ny,nz)==34) world_set_block(w,nx,ny,nz,0);
    }
    if(old!=34 || replacement==36) return;
    nx=x-dx[face]; ny=y-dy[face]; nz=z-dz[face]; base=world_peek_block(w,nx,ny,nz);
    if((base==29 || base==33) && (world_peek_metadata(w,nx,ny,nz)&8)) {
        world_drop_stack(w,nx,ny,nz,(InventorySlot){(int)base,1,0}); world_set_block(w,nx,ny,nz,0);
    }
}
