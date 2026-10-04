#include "world/world.h"
#include "world/beta_region.h"
#include "world/beta_discovery.h"
#include "world/block_entity.h"
#include "world/piston.h"
#include "world/door.h"
#include "world/redstone.h"
#include "game/player.h"
#include "game/mining.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

static void geometry(void)
{
    World w; BetaBlockBox b[5]; unsigned id,m;
    assert(world_init(&w,22,1,4)==WORLD_OK && world_get_chunk(&w,0,0));
    for(id=0;id<97;++id) {
        assert(strncmp(world_block_def((uint8_t)id)->name,"Unimplemented",13)!=0);
        if(id==0 || id==36 || id==63 || id==68) continue;
        for(m=0;m<6;++m) {
            int tile=beta_block_terrain_tile((BetaBlockState){(uint8_t)id,0},m);
            assert(tile>=0 && beta_render_source_tile(beta_render_tile(tile))==tile);
        }
    }
    assert(beta_block_terrain_tile((BetaBlockState){41,0},1)==23);
    assert(beta_block_terrain_tile((BetaBlockState){42,0},1)==22);
    assert(beta_block_terrain_tile((BetaBlockState){57,0},1)==24);
    assert(beta_block_terrain_tile((BetaBlockState){19,0},1)==48);
    assert(beta_block_terrain_tile((BetaBlockState){24,0},0)==208);
    assert(beta_block_terrain_tile((BetaBlockState){24,0},1)==176);
    assert(beta_block_terrain_tile((BetaBlockState){24,0},2)==192);
    for(m=0;m<8;++m) {
        assert(world_set_state(&w,8,64,8,(BetaBlockState){78,(uint8_t)m}));
        assert(beta_block_selection_box((BetaBlockState){78,(uint8_t)m},b));
        assert(b[0].max_y==(m+1)/8.0f);
        assert(world_block_collision_boxes(&w,8,64,8,b)==(m>=3));
        if(m>=3) assert(b[0].max_y==.5f); /* Original Beta collision quirk. */
    }
    for(id=53;id<=67;id+=14) for(m=0;m<4;++m) {
        assert(world_set_state(&w,8,64,8,(BetaBlockState){(uint8_t)id,(uint8_t)m}));
        assert(world_block_collision_boxes(&w,8,64,8,b)==2);
        assert(b[0].max_y==.5f && b[1].max_y==1);
        assert((m<2 ? b[0].max_x-b[0].min_x : b[0].max_z-b[0].min_z)==.5f);
        assert((m<2 ? b[1].max_x-b[1].min_x : b[1].max_z-b[1].min_z)==.5f);
        assert(mining_drop((BetaBlockState){(uint8_t)id,(uint8_t)m},278,0).id==(id==53 ? 5 : 4));
    }
    for(m=0;m<8;++m) {
        assert(beta_block_selection_box((BetaBlockState){96,(uint8_t)m},b));
        assert((m<4 ? b[0].max_y : (m&3)<2 ? b[0].max_z-b[0].min_z : b[0].max_x-b[0].min_x)==.1875f);
    }
    assert(world_set_block(&w,8,64,8,85));
    assert(world_block_collision_boxes(&w,8,64,8,b)==1 && b[0].max_y==1.5f);
    assert(!world_block_def(85)->opaque && !world_block_def(53)->opaque);
    for(m=0;m<4;++m) {
        static const int front[4]={3,4,2,5};
        assert(beta_block_terrain_tile((BetaBlockState){86,(uint8_t)m},front[m])==119);
        assert(beta_block_terrain_tile((BetaBlockState){91,(uint8_t)m},front[m])==120);
    }
    assert(world_block_def(91)->emission==15);
    assert(beta_block_item_boxes((BetaBlockState){77,0},b)==1);
    assert(b[0].max_x-b[0].min_x==.375f && b[0].max_y-b[0].min_y==.25f);
    assert(b[0].max_z-b[0].min_z==.125f); /* Button is not a plate. */
    assert(world_close(&w)==WORLD_OK);
}

static void placement(void)
{
    World w; Player p={0}; PlayerInput in={0}; BetaBlockBox b[2]; int dir,i;
    static const int dx[4]={0,0,1,-1},dz[4]={1,-1,0,0};
    static const float yaw[4]={0,3.14159265f,-1.57079633f,1.57079633f};
    assert(world_init(&w,22,1,8)==WORLD_OK && world_get_chunk(&w,0,0));
    w.creative=1; p.creative=1; p.health=20; p.air=300;
    for(dir=0;dir<4;++dir) {
        int x=8+dx[dir],z=8+dz[dir];
        assert(world_set_block(&w,8,65,8,1));
        p.x=8.5f+dx[dir]*3; p.z=8.5f+dz[dir]*3; p.y=64; p.yaw=yaw[dir]; p.pitch=0;
        assert(player_place_block(&p,&w,96));
        assert(world_peek_block(&w,x,65,z)==96 && world_peek_metadata(&w,x,65,z)==(dir^1));
        assert(door_activate(&w,x,65,z) && (world_peek_metadata(&w,x,65,z)&4));
        for(i=0;i<3;++i) world_step_physics(&w,4096);
        assert(world_peek_metadata(&w,x,65,z)&4); /* Unrelated ticks do not close a manually opened hatch. */
        assert(door_activate(&w,x,65,z) && !(world_peek_metadata(&w,x,65,z)&4));
        assert(world_set_state(&w,x,65,z,(BetaBlockState){96,(uint8_t)(dir^1)}));
        assert(world_set_state(&w,x,64,z,(BetaBlockState){69,5}));
        assert(world_redstone_activate(&w,x,64,z) && (world_peek_metadata(&w,x,65,z)&4));
        assert(world_redstone_activate(&w,x,64,z) && !(world_peek_metadata(&w,x,65,z)&4));
        world_set_block(&w,x,64,z,0); world_set_block(&w,8,65,8,0);
        world_step_physics(&w,4096); assert(!world_peek_block(&w,x,65,z));
    }
    /* Sponge does not absorb a neighbouring Beta source. */
    assert(world_set_block(&w,8,64,8,19) && world_set_block(&w,9,64,8,9));
    assert(world_peek_block(&w,9,64,8)==9 || world_peek_block(&w,9,64,8)==8);
    assert(world_set_block(&w,8,64,8,0) && world_set_block(&w,9,64,8,0));
    /* Snow melts under block light, and drops nothing when support disappears. */
    assert(world_set_state(&w,8,64,8,(BetaBlockState){78,7}));
    assert(world_block_collision_boxes(&w,8,64,8,b)==1);
    world_set_block(&w,8,63,8,0); assert(world_peek_block(&w,8,64,8)==0);
    world_set_block(&w,8,63,8,1);
    /* Walk up both half steps; a fence remains too tall for stepHeight .5. */
    for(i=0;i<5;++i) { world_set_block(&w,8+i,63,8,1); world_set_block(&w,8+i,64,8,0); }
    world_set_state(&w,9,64,8,(BetaBlockState){53,0});
    world_set_block(&w,10,64,8,1);
    p.x=8.5f; p.z=8.5f; p.y=64; p.yaw=1.57079633f; p.pitch=0; p.on_ground=1;
    in.forward=1;
    for(i=0;i<12;++i) player_tick(&p,&w,&in,.05f);
    assert(p.x>10.2f && p.y>64.99f);
    world_set_block(&w,9,64,8,85); world_set_block(&w,10,64,8,0);
    p.x=8.5f; p.y=64; p.on_ground=1;
    for(i=0;i<10;++i) player_tick(&p,&w,&in,.05f);
    assert(p.x<8.71f); /* Fence cannot be climbed from the ground. */
    assert(world_close(&w)==WORLD_OK);
}

static void fixture(const char *path)
{
    World w; FILE *manifest; char name[1024],line[128]; unsigned id,meta,seen[97]={0}; int x,y,z,pass,count=0;
    snprintf(name,sizeof(name),"%s/block-lab.csv",path);
    for(pass=0;pass<2;++pass) {
        assert(world_init(&w,22,0,512)==WORLD_OK);
        snprintf(w.path,sizeof(w.path),"%s",path);
        w.persistent=1; w.beta_format=1;
        w.read_beta_chunk=beta_region_read_chunk; w.write_beta_chunk=beta_region_write_chunk;
        manifest=fopen(name,"rb"); assert(manifest && fgets(line,sizeof(line),manifest));
        while(fgets(line,sizeof(line),manifest)) {
            assert(sscanf(line,"%u,%u,%d,%d,%d",&id,&meta,&x,&y,&z)==5);
            assert(id<97 && world_get_block(&w,x,y,z)==id && world_get_metadata(&w,x,y,z)==meta);
            if(!pass) { ++seen[id]; ++count; }
            if(id==96 || id==53 || id==67 || id==78 || id==86 || id==91)
                assert(world_set_metadata(&w,x,y,z,(uint8_t)meta));
            if(id==63 || id==68) {
                BlockEntity *e=block_entity_get(&w,x,y,z,0);
                assert(e && e->kind==BLOCK_ENTITY_SIGN && !strcmp(e->sign_text[0],"Beta sign"));
            }
        }
        assert(fclose(manifest)==0 && world_close(&w)==WORLD_OK);
    }
    for(id=0;id<97;++id) assert(seen[id]);
    assert(seen[78]==8 && seen[96]==8 && seen[53]==4 && seen[67]==4);
    printf("Block lab: %d specimens, all 97 IDs, metadata and sign NBT round trip passed\n",count);
}
int main(int argc,char **argv)
{
    geometry(); placement();
    if(argc==2) fixture(argv[1]);
    puts("Beta block textures, shapes, placement, trapdoor power/support and stair movement passed");
    return 0;
}
