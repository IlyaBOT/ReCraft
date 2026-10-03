#include "door.h"
#include "redstone.h"
#include <math.h>
static int is_door(unsigned id) { return id==64 || id==71; }
static int replaceable(unsigned id) { return id==0 || id==8 || id==9 || id==10 || id==11 || id==78; }
int door_place(World *w,int x,int y,int z,unsigned id,float yaw)
{
    static const int dx[4]={0,-1,0,1},dz[4]={1,0,-1,0};
    int dir=(int)floorf(yaw*.63661977236f+3.5f)&3,a,b,left,right;
    if(w->network_mode || !is_door(id) || y<=0 || y>=127 ||
       !world_block_def(world_get_block(w,x,y-1,z))->opaque ||
       !replaceable(world_get_block(w,x,y,z)) || !replaceable(world_get_block(w,x,y+1,z))) return 0;
    a=dx[dir]; b=dz[dir];
    left=world_block_def(world_get_block(w,x-a,y,z-b))->opaque+world_block_def(world_get_block(w,x-a,y+1,z-b))->opaque;
    right=world_block_def(world_get_block(w,x+a,y,z+b))->opaque+world_block_def(world_get_block(w,x+a,y+1,z+b))->opaque;
    if(((world_get_block(w,x-a,y,z-b)==id || world_get_block(w,x-a,y+1,z-b)==id) &&
        world_get_block(w,x+a,y,z+b)!=id && world_get_block(w,x+a,y+1,z+b)!=id) || right>left)
        dir=((dir-1)&3)+4;
    /* Beta duplicates direction/open bits in the upper half, with bit 8 added.
     * Validation is scheduled, so callbacks never destroy a half-built door. */
    if(!world_set_state(w,x,y,z,(BetaBlockState){(uint8_t)id,(uint8_t)dir})) return 0;
    if(!world_set_state(w,x,y+1,z,(BetaBlockState){(uint8_t)id,(uint8_t)(dir|8)})) {
        world_set_block(w,x,y,z,0); return 0;
    }
    if(world_redstone_power(w,x,y,z,x,y,z,1)>0 || world_redstone_power(w,x,y+1,z,x,y+1,z,1)>0)
        door_power_changed(w,x,y,z);
    return 1;
}
static void set_open(World *w,int x,int y,int z,int open)
{
    unsigned id=world_peek_block(w,x,y,z),meta=world_peek_metadata(w,x,y,z);
    if(!is_door(id) || !!(meta&4)==!!open) return;
    meta^=4; world_set_metadata(w,x,y,z,(uint8_t)meta);
    if(world_peek_block(w,x,y+1,z)==id) world_set_metadata(w,x,y+1,z,(uint8_t)(meta|8));
    world_sound(w,(open ? "random.door_open" : "random.door_close"),x+.5f,y+.5f,z+.5f,1,1);
}
int door_activate(World *w,int x,int y,int z)
{
    unsigned id=world_peek_block(w,x,y,z);
    if(w->network_mode || !is_door(id)) return 0;
    if(id==71) return 1;
    if(world_peek_metadata(w,x,y,z)&8) --y;
    set_open(w,x,y,z,!(world_peek_metadata(w,x,y,z)&4)); return 1;
}
void door_power_changed(World *w,int x,int y,int z)
{
    if(!is_door(world_peek_block(w,x,y,z))) return;
    if(world_peek_metadata(w,x,y,z)&8) --y;
    if(is_door(world_peek_block(w,x,y,z)) && world_peek_block(w,x,y+1,z)==world_peek_block(w,x,y,z))
        set_open(w,x,y,z,world_redstone_power(w,x,y,z,x,y,z,1)>0 || world_redstone_power(w,x,y+1,z,x,y+1,z,1)>0);
}
void door_neighbor_tick(World *w,int x,int y,int z)
{
    unsigned id=world_peek_block(w,x,y,z),meta=world_peek_metadata(w,x,y,z);
    if(!is_door(id)) return;
    if(meta&8) { if(world_peek_block(w,x,y-1,z)!=id) world_set_block(w,x,y,z,0); }
    else if(world_peek_block(w,x,y+1,z)!=id || !world_block_def(world_peek_block(w,x,y-1,z))->opaque) {
        world_set_block(w,x,y,z,0);
        if(world_peek_block(w,x,y+1,z)==id) world_set_block(w,x,y+1,z,0);
        if(!w->creative) world_drop_stack(w,x,y,z,(InventorySlot){id==64 ? 324 : 330,1,0});
    }
}
