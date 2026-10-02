#include "bed.h"
#include "../world/environment.h"
#include <math.h>
static const int dx[4]={0,-1,0,1},dz[4]={1,0,-1,0};
void player_eye(const Player *p,const World *w,float *x,float *y,float *z,float *yaw,float *pitch)
{
    *x=p->x; *y=p->y+1.62f; *z=p->z; *yaw=p->yaw; *pitch=p->pitch;
    if(p->sleeping) {
        unsigned dir=world_peek_metadata(w,p->bed_x,p->bed_y,p->bed_z)&3;
        *x=p->bed_x+.5f+dx[dir]*.4f; *y=p->bed_y+1.0375f;
        *z=p->bed_z+.5f+dz[dir]*.4f;
        *yaw=dir*1.57079633f; *pitch=.15f;
    }
}
static int empty_near(World *w,int x,int y,int z,int *rx,int *rz)
{
    int dir=world_get_metadata(w,x,y,z)&3,k,a,b;
    for(k=0;k<=1;++k) {
        int xmin=x-dx[dir]*k-1,zmin=z-dz[dir]*k-1;
        for(a=xmin;a<=xmin+2;++a) for(b=zmin;b<=zmin+2;++b)
            if(world_block_def(world_get_block(w,a,y-1,b))->opaque &&
               !world_get_block(w,a,y,b) && !world_get_block(w,a,y+1,b)) {
                *rx=a; *rz=b; return 1;
            }
    }
    return 0;
}
int bed_place(World *w,int x,int y,int z,unsigned dir)
{
    int hx,hz;
    if(w->network_mode || dir>3 || y<=0 || y>=127) return 0;
    hx=x+dx[dir]; hz=z+dz[dir];
    if(world_get_block(w,x,y,z) || world_get_block(w,hx,y,hz) ||
       !world_block_def(world_get_block(w,x,y-1,z))->opaque ||
       !world_block_def(world_get_block(w,hx,y-1,hz))->opaque) return 0;
    if(!world_set_block(w,x,y,z,26)) return 0;
    world_set_metadata(w,x,y,z,(uint8_t)dir);
    if(!world_set_block(w,hx,y,hz,26)) { world_set_block(w,x,y,z,0); return 0; }
    world_set_metadata(w,hx,y,hz,(uint8_t)(dir|8));
    return 1;
}
void bed_neighbor_tick(World *w,int x,int y,int z)
{
    unsigned meta=world_peek_metadata(w,x,y,z),dir=meta&3;
    int nx=x+((meta&8) ? -dx[dir] : dx[dir]),nz=z+((meta&8) ? -dz[dir] : dz[dir]);
    int cx=nx>=0 ? nx/16 : (int)(-((-(int64_t)nx+15)/16));
    int cz=nz>=0 ? nz/16 : (int)(-((-(int64_t)nz+15)/16));
    if(!world_peek_chunk(w,cx,cz)) return; /* Do not destroy an unloaded half. */
    if(world_peek_block(w,nx,y,nz)!=26) {
        if(!(meta&8) && !w->creative) world_drop_stack(w,x,y,z,(InventorySlot){355,1,0});
        world_set_block(w,x,y,z,0);
    }
}
int player_sleep(Player *p,World *w,int x,int y,int z)
{
    unsigned meta;
    if(w->network_mode || p->sleeping || p->health<=0 || world_get_block(w,x,y,z)!=26) return 2;
    meta=world_get_metadata(w,x,y,z);
    if(!(meta&8)) { x+=dx[meta&3]; z+=dz[meta&3]; }
    if(world_get_block(w,x,y,z)!=26) return 2;
    if(world_is_daytime(w)) return 1;
    if(fabsf(p->x-x)>3 || fabsf(p->y-y)>2 || fabsf(p->z-z)>3) return 2;
    p->bed_x=x; p->bed_y=y; p->bed_z=z; p->sleeping=1; p->sleep_ticks=0;
    p->x=x+.5f+dx[meta&3]*.4f; p->y=y+.9375f; p->z=z+.5f+dz[meta&3]*.4f;
    p->vx=p->vy=p->vz=p->fall_distance=0;
    world_set_metadata(w,x,y,z,(uint8_t)(world_get_metadata(w,x,y,z)|4));
    return 0;
}
void player_wake(Player *p,World *w,int completed)
{
    int x=p->bed_x,z=p->bed_z;
    if(!p->sleeping) return;
    if(world_get_block(w,p->bed_x,p->bed_y,p->bed_z)==26) {
        world_set_metadata(w,p->bed_x,p->bed_y,p->bed_z,
            (uint8_t)(world_get_metadata(w,p->bed_x,p->bed_y,p->bed_z)&~4u));
        if(!empty_near(w,x,p->bed_y,z,&x,&z)) { x=p->bed_x; z=p->bed_z; p->y=p->bed_y+1.1f; }
        else p->y=p->bed_y+.1f;
        p->x=x+.5f; p->z=z+.5f;
    }
    p->sleeping=0; p->sleep_ticks=0;
    if(completed) {
        p->has_bed_spawn=1; p->spawn_x=p->bed_x; p->spawn_y=p->bed_y; p->spawn_z=p->bed_z;
    }
    p->vx=p->vy=p->vz=p->fall_distance=0;
}
void player_sleep_tick(Player *p,World *w)
{
    if(!p->sleeping) return;
    if(world_get_block(w,p->bed_x,p->bed_y,p->bed_z)!=26 || p->health<=0) player_wake(p,w,0);
    else if(world_is_daytime(w)) player_wake(p,w,1);
    else if(++p->sleep_ticks>=100) { world_environment_dawn(w); player_wake(p,w,1); }
}
void player_respawn(Player *p,World *w)
{
    int has=p->has_bed_spawn,x=p->spawn_x,y=p->spawn_y,z=p->spawn_z,rx=x,rz=z;
    player_spawn(p,w,w->creative);
    p->has_bed_spawn=has; p->spawn_x=x; p->spawn_y=y; p->spawn_z=z;
    if(has && world_get_block(w,x,y,z)==26 && empty_near(w,x,y,z,&rx,&rz)) {
        p->x=rx+.5f; p->y=y+.1f; p->z=rz+.5f;
    }
}
