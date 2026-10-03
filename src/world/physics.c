#include "world.h"
#include "fluid.h"
#include "../game/bed.h"
#include "../game/sign.h"
#include "redstone.h"
#include "door.h"
#include "rail.h"
#include "ticks.h"
#include "environment.h"
#include "../game/mining.h"
#include <stdio.h>
#include <string.h>
static const int dx[6]={-1,1,0,0,0,0},dy[6]={0,0,-1,1,0,0},dz[6]={0,0,0,0,-1,1};
static Chunk *loaded(const World *w,int x,int y,int z)
{
    int cx=x>=0 ? x/16 : (int)(-((-(int64_t)x+15)/16));
    int cz=z>=0 ? z/16 : (int)(-((-(int64_t)z+15)/16));
    Chunk *c=world_peek_chunk(w,cx,cz);
    return (unsigned)y<128 && c && (!w->beta_format || c->beta_raw) ? c : NULL;
}
static unsigned hash_cell(int x,int y,int z,unsigned id)
{
    uint32_t h=(uint32_t)x*UINT32_C(0x9e3779b1)^(uint32_t)z*UINT32_C(0x85ebca6b)^(uint32_t)(y*257+id);
    h^=h>>16; return h&(WORLD_PHYSICS_QUEUE*2-1);
}
static int earlier(const WorldPhysicsCell *a,const WorldPhysicsCell *b)
{ return a->due<b->due || (a->due==b->due && a->order<b->order); }
static void swap(World *w,unsigned a,unsigned b)
{
    WorldPhysicsCell t=w->physics[a]; w->physics[a]=w->physics[b]; w->physics[b]=t;
    w->physics_hash[w->physics[a].hash_slot]=(uint16_t)(a+1);
    w->physics_hash[w->physics[b].hash_slot]=(uint16_t)(b+1);
}
void world_schedule_tick(World *w,int x,int y,int z,uint8_t id,unsigned delay)
{
    unsigned slot=hash_cell(x,y,z,id),n,i,free_slot=UINT16_MAX;
    Chunk *chunk; SavedTick *saved;
    if (!w || w->network_mode || !(chunk=loaded(w,x,y,z))) return;
    for (n=0;n<WORLD_PHYSICS_QUEUE*2;++n,slot=(slot+1)&(WORLD_PHYSICS_QUEUE*2-1)) {
        unsigned entry=w->physics_hash[slot];
        if (!entry) { if (free_slot==UINT16_MAX) free_slot=slot; break; }
        if (entry==UINT16_MAX) { if (free_slot==UINT16_MAX) free_slot=slot; continue; }
        {
            WorldPhysicsCell *c=&w->physics[entry-1];
            if (c->x==x && c->y==y && c->z==z && c->id==id) return;
        }
    }
    if (w->physics_count==WORLD_PHYSICS_QUEUE || free_slot==UINT16_MAX) {
        world_ticks_remember(w,chunk,x,y,z,id,delay);
        if (!w->physics_overflow) fprintf(stderr,"Block tick queue full; retrying loaded moving blocks later.\n");
        w->physics_overflow=1; return;
    }
    saved=world_ticks_remember(w,chunk,x,y,z,id,delay);
    if (!saved) return;
    saved->queued=1;
    i=w->physics_count++;
    w->physics[i].x=x; w->physics[i].y=(uint8_t)y; w->physics[i].z=z; w->physics[i].id=id;
    w->physics[i].hash_slot=(uint16_t)free_slot;
    w->physics[i].due=saved->due; w->physics[i].order=++w->physics_order;
    w->physics[i].saved_tick=saved;
    w->physics_hash[free_slot]=(uint16_t)(i+1);
    while (i && earlier(&w->physics[i],&w->physics[(i-1)/2])) { unsigned p=(i-1)/2; swap(w,i,p); i=p; }
}
void world_physics_forget_chunk(World *w,Chunk *chunk)
{
    unsigned i,n=0;
    for(i=0;i<w->physics_count;++i) {
        WorldPhysicsCell *c=&w->physics[i];
        if(loaded(w,c->x,c->y,c->z)==chunk) {
            if(c->saved_tick) c->saved_tick->queued=0;
        } else w->physics[n++]=*c;
    }
    w->physics_count=n; memset(w->physics_hash,0,sizeof(w->physics_hash));
    for(i=0;i<n;++i) {
        unsigned slot=hash_cell(w->physics[i].x,w->physics[i].y,w->physics[i].z,w->physics[i].id),p=i;
        while(w->physics_hash[slot]) slot=(slot+1)&(WORLD_PHYSICS_QUEUE*2-1);
        w->physics[i].hash_slot=(uint16_t)slot; w->physics_hash[slot]=(uint16_t)(i+1);
        while(p && earlier(&w->physics[p],&w->physics[(p-1)/2])) { unsigned parent=(p-1)/2; swap(w,p,parent); p=parent; }
    }
}
static WorldPhysicsCell pop(World *w)
{
    WorldPhysicsCell result=w->physics[0]; unsigned i=0;
    w->physics_hash[result.hash_slot]=UINT16_MAX;
    if (--w->physics_count) {
        w->physics[0]=w->physics[w->physics_count];
        w->physics_hash[w->physics[0].hash_slot]=1;
        for (;;) {
            unsigned a=i*2+1,b=a+1,best=i;
            if (a<w->physics_count && earlier(&w->physics[a],&w->physics[best])) best=a;
            if (b<w->physics_count && earlier(&w->physics[b],&w->physics[best])) best=b;
            if (best==i) break;
            swap(w,i,best); i=best;
        }
    }
    if (!w->physics_count) memset(w->physics_hash,0,sizeof(w->physics_hash));
    return result;
}
static void support(int x,int y,int z,int meta,int *sx,int *sy,int *sz);
static void notify_cell(World *w,int x,int y,int z)
{
    Chunk *c=loaded(w,x,y,z); unsigned id;
    if (!c) return;
    id=world_peek_block(w,x,y,z);
    if (id==9 || id==11) {
        chunk_set_block(c,x-c->x*16,y,z-c->z*16,(uint8_t)(id-1)); --id;
    }
    if (id==8 || id==10) world_schedule_tick(w,x,y,z,(uint8_t)id,id==8 ? 5 : 30);
    else if (id==12 || id==13) world_schedule_tick(w,x,y,z,(uint8_t)id,3);
    else if (id==75 || id==76) world_schedule_tick(w,x,y,z,(uint8_t)id,2);
    else if (id==50 || id==81 || id==55 || id==26 || id==64 || id==71 || sign_is_block(id) || rail_is(id)) world_schedule_tick(w,x,y,z,(uint8_t)id,id==55 || rail_is(id) ? 0 : 1);
    else if(id==93 || id==94) {
        unsigned meta=world_peek_metadata(w,x,y,z);
        int input=world_repeater_input(w,x,y,z,meta);
        if(!world_block_def(world_peek_block(w,x,y-1,z))->opaque) {
            world_drop_stack(w,x,y,z,(InventorySlot){356,1,0}); world_set_block(w,x,y,z,0);
        } else if((id==93 && input) || (id==94 && !input)) world_schedule_tick(w,x,y,z,(uint8_t)id,(((meta>>2)&3)+1)*2);
    } else if(id==69 || id==77) {
        int sx,sy,sz; support(x,y,z,world_peek_metadata(w,x,y,z)&7,&sx,&sy,&sz);
        if(loaded(w,sx,sy,sz) && !world_block_def(world_peek_block(w,sx,sy,sz))->opaque) {
            world_drop_stack(w,x,y,z,(InventorySlot){(int)id,1,0}); world_set_block(w,x,y,z,0);
        }
    }
}
void world_physics_notify(World *w,int x,int y,int z)
{
    int i,height;
    if (!w || w->network_mode) return;
    notify_cell(w,x,y,z);
    for (i=0;i<6;++i) notify_cell(w,x+dx[i],y+dy[i],z+dz[i]);
    /* Rails connect diagonally across a one-block elevation change. */
    for(i=0;i<6;++i) if(!dy[i]) for(height=-1;height<=1;height+=2) {
        unsigned id=world_peek_block(w,x+dx[i],y+height,z+dz[i]);
        if(rail_is(id)) world_schedule_tick(w,x+dx[i],y+height,z+dz[i],(uint8_t)id,0);
    }
}
void world_physics_loaded(World *w,Chunk *c)
{
    unsigned i;
    if (!w || !c || w->network_mode || (w->beta_format && !c->beta_raw)) return;
    world_ticks_resume(w,c);
    for (i=0;i<WORLD_CHUNK_VOLUME;++i) {
        unsigned id=c->blocks[i];
        int x=c->x*16+(i&15),y=i>>8,z=c->z*16+((i>>4)&15);
        if(id==50 || id==75 || id==76 || id==81 || id==55) continue;
        /* Supported falling blocks do not require a permanent scheduled tick. */
        if ((id==12 || id==13) && !fluid_kind(world_peek_block(w,x,y-1,z)) &&
            world_peek_block(w,x,y-1,z)!=0 && world_peek_block(w,x,y-1,z)!=51) continue;
        if (id==8 || id==10 || id==75 || id==76 || id==81 || id==50 || id==12 || id==13 || id==55)
            world_schedule_tick(w,x,y,z,(uint8_t)id,
                id==8 ? 5 : id==10 ? 30 : id==12 || id==13 ? 3 : id==75 || id==76 ? 2 : 1);
    }
}
static void support(int x,int y,int z,int meta,int *sx,int *sy,int *sz)
{
    *sx=x; *sy=y-1; *sz=z;
    if (meta==1) { *sx=x-1; *sy=y; }
    else if (meta==2) { *sx=x+1; *sy=y; }
    else if (meta==3) { *sz=z-1; *sy=y; }
    else if (meta==4) { *sz=z+1; *sy=y; }
}
void world_redstone_notify(World *w,int x,int y,int z)
{
    int i,j;
    world_physics_notify(w,x,y,z);
    for(i=0;i<6;++i) door_power_changed(w,x+dx[i],y+dy[i],z+dz[i]);
    for (i=0;i<6;++i) for (j=0;j<6;++j) {
        int nx=x+dx[i]+dx[j],ny=y+dy[i]+dy[j],nz=z+dz[i]+dz[j];
        unsigned id=world_peek_block(w,nx,ny,nz);
        if (id==75 || id==76 || id==55 || id==93 || id==94 || rail_is(id)) notify_cell(w,nx,ny,nz);
        if(id==64 || id==71) door_power_changed(w,nx,ny,nz);
    }
}
static int burned_out(const World *w,int x,int y,int z)
{
    unsigned i,count=0;
    for (i=0;i<128;++i) if (w->torch_toggles[i].tick && w->tick-w->torch_toggles[i].tick<=100 &&
        w->torch_toggles[i].x==x && w->torch_toggles[i].y==y && w->torch_toggles[i].z==z) ++count;
    return count>=8;
}
static int cactus_valid(const World *w,int x,int y,int z)
{
    int i; unsigned below=world_peek_block(w,x,y-1,z);
    if (below!=12 && below!=81) return 0;
    for (i=0;i<6;++i) if (dy[i]==0 && beta_material_solid(world_peek_block(w,x+dx[i],y,z+dz[i]))) return 0;
    return 1;
}
static void step(World *w,WorldPhysicsCell c)
{
    int x=c.x,y=c.y,z=c.z,sx,sy,sz;
    unsigned id=world_peek_block(w,x,y,z),meta=world_peek_metadata(w,x,y,z);
    if (id!=c.id || !loaded(w,x,y,z)) return;
    if(sign_is_block(id)) { sign_neighbor_tick(w,x,y,z); return; }
    if(rail_is(id)) { rail_update(w,x,y,z); return; }
    if(id==26) { bed_neighbor_tick(w,x,y,z); return; }
    if(id==64 || id==71) { door_neighbor_tick(w,x,y,z); return; }
    if(id==77) {
        if(meta&8) {
            world_set_metadata(w,x,y,z,(uint8_t)(meta&7)); world_redstone_notify(w,x,y,z);
            world_sound(w,"random.click",x+.5f,y+.5f,z+.5f,.3f,.5f);
        }
        return;
    }
    if(id==93 || id==94) {
        int input=world_repeater_input(w,x,y,z,meta);
        if(!world_block_def(world_peek_block(w,x,y-1,z))->opaque) {
            world_drop_stack(w,x,y,z,(InventorySlot){356,1,0}); world_set_block(w,x,y,z,0); return;
        }
        if(id==93 || !input) {
            world_set_state(w,x,y,z,(BetaBlockState){(uint8_t)(id==93 ? 94 : 93),(uint8_t)meta});
            if(id==93 && !input) world_schedule_tick(w,x,y,z,94,(((meta>>2)&3)+1)*2);
            world_redstone_notify(w,x,y,z);
        }
        return;
    }
    if (id==8 || id==10) { fluid_tick(w,x,y,z,id); return; }
    if (id==12 || id==13) {
        int bottom=y;
        while (bottom>0 && loaded(w,x,bottom-1,z)) {
            unsigned b=world_peek_block(w,x,bottom-1,z);
            if (b && b!=51 && !fluid_kind(b)) break;
            --bottom;
        }
        /* Resolve the destination atomically; do not duplicate a sand block
         * or move it one cell per frame. Entity animation is rendered separately. */
        if (bottom<y) { world_set_block(w,x,y,z,0); if (bottom>0) world_set_block(w,x,bottom,z,(uint8_t)id); }
        return;
    }
    if (id==81) {
        if (!cactus_valid(w,x,y,z)) {
            world_drop_stack(w,x,y,z,(InventorySlot){81,1,0}); world_set_block(w,x,y,z,0);
        }
        return;
    }
    if (id==50 || id==75 || id==76) {
        support(x,y,z,meta&7,&sx,&sy,&sz);
        if (!loaded(w,sx,sy,sz)) return;
        if (!world_block_def(world_peek_block(w,sx,sy,sz))->opaque) {
            world_drop_stack(w,x,y,z,(InventorySlot){id==50 ? 50 : 76,1,0}); world_set_block(w,x,y,z,0); return;
        }
        if (id!=50) {
            int powered=world_redstone_signal(w,sx,sy,sz,x,y,z,1)>0;
            if ((id==76 && powered) || (id==75 && !powered && !burned_out(w,x,y,z))) {
                world_set_state(w,x,y,z,(BetaBlockState){(uint8_t)(id==76 ? 75 : 76),(uint8_t)meta});
                if (id==76) {
                    unsigned n=w->torch_toggle_head++%128;
                    w->torch_toggles[n].x=x; w->torch_toggles[n].y=y; w->torch_toggles[n].z=z; w->torch_toggles[n].tick=w->tick;
                    if(burned_out(w,x,y,z)) world_sound(w,"random.fizz",x+.5f,y+.5f,z+.5f,.5f,
                        2.6f+((float)world_random(w,16777216)-(float)world_random(w,16777216))/16777216*.8f);
                }
                world_redstone_notify(w,x,y,z);
            }
        }
        return;
    }
    if (id==55) {
        int i,power=world_redstone_power(w,x,y,z,x,y,z,0)>0 ? 15 : 0;
        if (!world_block_def(world_peek_block(w,x,y-1,z))->opaque) {
            world_drop_stack(w,x,y,z,(InventorySlot){331,1,0}); world_set_block(w,x,y,z,0); return;
        }
        for (i=0;i<6;++i) if (!dy[i]) {
            int nx=x+dx[i],nz=z+dz[i],ny=y,n;
            if(world_block_def(world_peek_block(w,nx,y,nz))->opaque) {
                if(world_block_def(world_peek_block(w,x,y+1,z))->opaque) continue;
                ++ny;
            } else if(world_peek_block(w,nx,y,nz)!=55) --ny;
            n=world_peek_metadata(w,nx,ny,nz);
            if(world_peek_block(w,nx,ny,nz)==55 && n-1>power) power=n-1;
        }
        if (power!=(int)meta) { world_set_metadata(w,x,y,z,(uint8_t)power); world_redstone_notify(w,x,y,z); }
    }
}
static void random_cactus(World *w)
{
    size_t c;
    for (c=0;c<w->cache_count;++c) {
        Chunk *chunk=w->cache[c]; unsigned n;
        if (w->beta_format && !chunk->beta_raw) continue;
        for (n=0;n<80;++n) {
            unsigned bits,index; int x,y,z,height;
            w->random_tick=w->random_tick*3u+UINT32_C(1013904223); bits=w->random_tick>>2;
            x=bits&15; z=(bits>>8)&15; y=(bits>>16)&127; index=(unsigned)(x+z*16+y*256);
            x+=chunk->x*16; z+=chunk->z*16;
            if(chunk->blocks[index]==75 || chunk->blocks[index]==76) {
                step(w,(WorldPhysicsCell){x,z,(uint8_t)y,chunk->blocks[index],0,0,0,NULL}); continue;
            }
            if (chunk->blocks[index]!=81 || y==127) continue;
            if (world_peek_block(w,x,y+1,z)!=0 || !cactus_valid(w,x,y,z)) continue;
            for (height=1;world_peek_block(w,x,y-height,z)==81;++height) { }
            if (height<3) {
                unsigned age=world_peek_metadata(w,x,y,z);
                if (age==15) { world_set_block(w,x,y+1,z,81); world_set_metadata(w,x,y,z,0); }
                else world_set_metadata(w,x,y,z,(uint8_t)(age+1));
            }
        }
    }
}
void world_step_physics(World *w,unsigned max_updates)
{
    unsigned done=0;
    if (!w || w->network_mode) return;
    ++w->tick; w->physics_processing=1;
    while (w->physics_count && w->physics[0].due<=w->tick && done++<max_updates) {
        WorldPhysicsCell cell=pop(w);
        world_ticks_consume(w,&cell); step(w,cell);
    }
    if (w->physics_overflow && w->physics_count<WORLD_PHYSICS_QUEUE/2) {
        size_t i; w->physics_overflow=0;
        for (i=0;i<w->cache_count;++i) world_physics_loaded(w,w->cache[i]);
    }
    random_cactus(w);
    w->physics_processing=0; world_finish_light_updates(w);
}
