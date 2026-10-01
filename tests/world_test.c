#include "../src/world/world.h"
#include "../src/game/player.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#ifdef _WIN32
#include <direct.h>
#include <process.h>
#define test_mkdir(p) _mkdir(p)
#define test_pid() _getpid()
#else
#include <sys/stat.h>
#include <unistd.h>
#define test_mkdir(p) mkdir(p,0700)
#define test_pid() getpid()
#endif

static void put_u32(uint8_t *p, uint32_t n)
{
    p[0]=(uint8_t)n; p[1]=(uint8_t)(n>>8);
    p[2]=(uint8_t)(n>>16); p[3]=(uint8_t)(n>>24);
}

static void write_native_v1_fixture(const char *path)
{
    const size_t raw_size=WORLD_CHUNK_VOLUME+3u*WORLD_NIBBLE_BYTES;
    uint8_t *raw=(uint8_t *)calloc(raw_size,1);
    uint8_t header[26]={0};
    uint32_t hash=UINT32_C(2166136261);
    size_t i, encoded_len=raw_size+(raw_size+127u)/128u;
    FILE *file;
    assert(raw);
    raw[0]=2; /* Old private ID 2 was dirt; Beta ID 2 is grass. */
    raw[1]=3; /* Old private ID 3 was grass; Beta ID 3 is dirt. */
    raw[2]=7; /* Old private ID 7 was a log; Beta ID 7 is bedrock. */
    raw[WORLD_CHUNK_VOLUME]=0x0a; /* First block's metadata nibble. */
    for(i=0;i<raw_size;++i) { hash^=raw[i]; hash*=UINT32_C(16777619); }
    memcpy(header,"RCC1",4); header[4]=1;
    put_u32(header+14,(uint32_t)raw_size);
    put_u32(header+18,(uint32_t)encoded_len);
    put_u32(header+22,hash);
    file=fopen(path,"wb"); assert(file);
    assert(fwrite(header,1,sizeof(header),file)==sizeof(header));
    for(i=0;i<raw_size;i+=128u) {
        uint8_t tag=(uint8_t)(raw_size-i>=128u ? 127u : raw_size-i-1u);
        size_t len=(size_t)tag+1u;
        assert(fwrite(&tag,1,1,file)==1);
        assert(fwrite(raw+i,1,len,file)==len);
    }
    assert(fclose(file)==0);
    free(raw);
}

int main(void)
{
    World a = {0}, b = {0}, c = {0};
    Player player;
    PlayerInput input = {0};
    BlockHit hit;
    WorldInfo infos[4];
    char dir[128], id[64], chunk_path[256];
    int i;
    /* Light crosses both positive and negative chunk seams, reaches a newly
     * loaded neighbour, and disappears again when its source is removed. */
    assert(world_init(&a,42,1,16)==WORLD_OK);
    assert(world_set_block(&a,15,90,15,BLOCK_TORCH));
    assert(chunk_get_block_light(world_get_chunk(&a,1,0),0,90,15)==13);
    assert(chunk_get_block_light(world_get_chunk(&a,1,1),0,90,0)==12);
    assert(world_set_block(&a,15,90,15,BLOCK_AIR));
    assert(chunk_get_block_light(world_peek_chunk(&a,1,0),0,90,15)==0);
    assert(chunk_get_block_light(world_peek_chunk(&a,1,1),0,90,0)==0);
    assert(world_set_block(&a,-1,90,-1,BLOCK_TORCH));
    assert(chunk_get_block_light(world_get_chunk(&a,0,-1),0,90,15)==13);
    assert(world_set_block(&a,0,90,-1,BLOCK_STONE));
    assert(chunk_get_block_light(world_peek_chunk(&a,0,-1),0,90,15)==0);
    assert(world_set_block(&a,0,90,-1,BLOCK_AIR));
    assert(chunk_get_block_light(world_peek_chunk(&a,0,-1),0,90,15)==13);
    assert(world_set_block(&a,-1,90,-1,BLOCK_AIR));
    assert(chunk_get_block_light(world_peek_chunk(&a,0,-1),0,90,15)==0);
    assert(world_close(&a)==WORLD_OK);
    assert(world_init(&a, 42, 1, 2) == WORLD_OK);
    assert(world_get_block(&a, 0, 63, 0) == BLOCK_GRASS);
    assert(world_get_block(&a, -1, 63, -1) == BLOCK_GRASS);
    assert(world_set_block(&a, -1, 64, -1, BLOCK_TORCH));
    assert(world_set_metadata(&a, -1, 64, -1, 11));
    assert(world_get_block(&a, -1, 64, -1) == BLOCK_TORCH);
    assert(world_get_metadata(&a, -1, 64, -1) == 11);
    assert(world_set_block(&a,-1,64,-1,BETA_BLOCK_SLAB));
    assert(world_get_metadata(&a,-1,64,-1)==0);
    assert(world_set_block(&a,-2,66,-2,BETA_BLOCK_SLAB));
    assert(chunk_get_sky_light(world_get_chunk(&a,-1,-1),14,65,14)==0);
    assert(world_set_block(&a,2,64,2,BETA_BLOCK_GLOWSTONE));
    assert(chunk_get_block_light(world_get_chunk(&a,0,0),2,64,2)==15);
    assert(world_set_block(&a,3,64,2,BETA_BLOCK_GLOWING_REDSTONE_ORE));
    assert(world_block_def(BETA_BLOCK_GLOWING_REDSTONE_ORE)->emission==9);
    assert(world_block_def(BETA_BLOCK_BROWN_MUSHROOM)->emission==1);
    assert(!world_block_def(BETA_BLOCK_SAPLING)->solid);
    assert(world_close(&a) == WORLD_OK);

    /* Local event queue updates only changed cells; generated terrain stays
     * untouched until a player edit exposes a falling or fluid block. */
    assert(world_init(&a,42,1,8)==WORLD_OK);
    assert(world_set_block(&a,2,67,2,BLOCK_SAND));
    for (i=0;i<32;++i) world_step_physics(&a,64);
    assert(world_get_block(&a,2,64,2)==BLOCK_SAND);
    assert(world_get_block(&a,2,67,2)==BLOCK_AIR);
    assert(world_set_block(&a,5,64,5,BLOCK_STONE));
    assert(world_set_block(&a,5,65,5,BLOCK_TORCH));
    assert(world_set_metadata(&a,5,65,5,5));
    assert(world_set_block(&a,5,64,5,BLOCK_AIR));
    for (i=0;i<32;++i) world_step_physics(&a,64);
    assert(world_get_block(&a,5,65,5)==BLOCK_AIR);
    {
        WorldDropEvent drop;
        assert(world_take_drop(&a,&drop));
        assert(drop.id==BLOCK_TORCH && drop.x==5 && drop.y==65 && drop.z==5);
        assert(!world_take_drop(&a,&drop));
    }
    assert(world_set_block(&a,9,64,9,BLOCK_WATER));
    for (i=0;i<32;++i) world_step_physics(&a,64);
    assert(world_get_block(&a,10,64,9)==BETA_BLOCK_FLOWING_WATER);
    assert(world_close(&a)==WORLD_OK);

    snprintf(dir,sizeof(dir),"build/world-test-%ld-%d",(long)time(NULL),(int)test_pid());
    assert(test_mkdir(dir) == 0);
    snprintf(id,sizeof(id),"world_%d",(int)test_pid());
    assert(world_create(&a,dir,id,"Persistence Fixture",123,1,1,0,1) == WORLD_OK);
    assert(world_set_block(&a,-1,70,-1,BLOCK_GLASS));
    assert(world_set_metadata(&a,-1,70,-1,13));
    /* Capacity one forces the dirty negative chunk to disk before close. */
    assert(world_get_chunk(&a,2,3));
    assert(world_close(&a) == WORLD_OK);
    assert(world_storage_list(dir,infos,4) == 1);
    assert(!strcmp(infos[0].name,"Persistence Fixture"));
    assert(infos[0].creative == 1 && infos[0].structures == 0);
    assert(world_storage_rename(dir,id,"Renamed") == WORLD_OK);
    assert(world_open(&b,dir,id,2) == WORLD_OK);
    assert(!strcmp(b.name,"Renamed"));
    assert(world_get_block(&b,-1,70,-1) == BLOCK_GLASS);
    assert(world_get_metadata(&b,-1,70,-1) == 13);
    assert(world_close(&b) == WORLD_OK);

    snprintf(chunk_path,sizeof(chunk_path),"%s/%s/chunk_0_0.rcg",dir,id);
    write_native_v1_fixture(chunk_path);
    assert(world_open(&c,dir,id,2)==WORLD_OK);
    assert(world_get_block(&c,0,0,0)==BLOCK_DIRT);
    assert(world_get_block(&c,1,0,0)==BLOCK_GRASS);
    assert(world_get_block(&c,2,0,0)==BLOCK_WOOD);
    assert(world_get_metadata(&c,0,0,0)==10);
    assert(world_peek_chunk(&c,0,0)->dirty_flags&CHUNK_DIRTY_SAVE);
    assert(world_close(&c)==WORLD_OK);
    {
        uint8_t file_header[6];
        FILE *file=fopen(chunk_path,"rb"); assert(file);
        assert(fread(file_header,1,sizeof(file_header),file)==sizeof(file_header));
        assert(fclose(file)==0);
        assert(file_header[4]==2 && file_header[5]==0);
    }

    assert(world_open(&c,dir,id,2)==WORLD_OK);
    assert(world_set_block(&c,4,70,4,BETA_BLOCK_TRAPDOOR));
    assert(world_set_metadata(&c,4,70,4,13));
    assert(world_close(&c)==WORLD_OK);
    assert(world_open(&c,dir,id,2)==WORLD_OK);
    assert(world_get_block(&c,4,70,4)==BETA_BLOCK_TRAPDOOR);
    assert(world_get_metadata(&c,4,70,4)==13);
    assert(world_close(&c)==WORLD_OK);

    assert(world_init(&a,42,1,8) == WORLD_OK);
    player_spawn(&player,&a,0);
    assert(world_set_block(&a,8,65,5,BLOCK_STONE));
    player.yaw=0; player.pitch=0;
    hit=player_raycast(&player,&a,5);
    assert(hit.hit && hit.x==8 && hit.y==65 && hit.z==5 && hit.place_z==6);
    assert(player_break_block(&player,&a));
    assert(world_get_block(&a,8,65,5)==BLOCK_AIR);
    /* A flower occupies only its Beta selection box. A ray above or beside
     * that box reaches the stone behind it, while a centered ray hits it. */
    assert(world_set_block(&a,8,66,5,BETA_BLOCK_DANDELION));
    assert(world_set_block(&a,8,66,4,BLOCK_STONE));
    player.y=65.2f; /* eye at 66.82, above the flower's 0.6 height */
    hit=player_raycast(&player,&a,5);
    assert(hit.hit && hit.block==BLOCK_STONE && hit.z==4);
    player.y=64.5f; /* eye at 66.12, inside the selection height */
    hit=player_raycast(&player,&a,5);
    assert(hit.hit && hit.block==BETA_BLOCK_DANDELION && hit.z==5);
    assert(hit.place_x==8 && hit.place_y==66 && hit.place_z==6);
    hit=player_raycast(&player,&a,2.5f);
    assert(!hit.hit);
    player.z=5.5f; player.y=65.88f; player.pitch=-1.570796f;
    hit=player_raycast(&player,&a,5);
    assert(hit.hit && hit.block==BETA_BLOCK_DANDELION);
    assert(hit.place_x==8 && hit.place_y==67 && hit.place_z==5);
    player.z=8.5f; player.y=64.5f; player.pitch=0;
    player.x=8.05f; /* outside the flower's 0.3..0.7 X inset */
    hit=player_raycast(&player,&a,5);
    assert(hit.hit && hit.block==BLOCK_STONE && hit.z==4);
    player.x=8.5f;
    assert(world_set_block(&a,8,66,5,BLOCK_AIR));
    assert(world_set_block(&a,8,66,4,BLOCK_AIR));
    player_spawn(&player,&a,0);
    player.yaw=0;
    assert(world_set_block(&a,8,64,6,BLOCK_STONE));
    assert(world_set_block(&a,8,65,6,BLOCK_STONE));
    input.forward=1;
    for(i=0;i<30;++i) player_tick(&player,&a,&input,0.05f);
    assert(player.z>=7.29f && player.z<7.32f);
    assert(world_block_def(BETA_BLOCK_SLAB)->solid);
    assert(!world_block_def(BETA_BLOCK_SLAB)->opaque);
    assert(world_block_def(BETA_BLOCK_DOUBLE_SLAB)->opaque);
    assert(world_set_block(&a,12,64,5,BETA_BLOCK_SLAB));
    assert(world_set_block(&a,12,64,4,BLOCK_STONE));
    player.x=12.5f; player.y=63.13f; player.z=8.5f;
    player.yaw=0; player.pitch=0;
    hit=player_raycast(&player,&a,5);
    assert(hit.hit && hit.block==BLOCK_STONE && hit.z==4);
    player.y=62.63f;
    hit=player_raycast(&player,&a,5);
    assert(hit.hit && hit.block==BETA_BLOCK_SLAB && hit.z==5);
    assert(hit.place_z==6);
    assert(world_set_block(&a,10,64,10,BETA_BLOCK_SLAB));
    player.x=10.5f; player.y=65.5f; player.z=10.5f;
    memset(&input,0,sizeof(input));
    for(i=0;i<30;++i) player_tick(&player,&a,&input,0.05f);
    assert(player.on_ground && player.y>=64.49f && player.y<64.51f);
    {
        BetaBlockState wood_slab={BETA_BLOCK_SLAB,2};
        BetaBlockState sand_slab={BETA_BLOCK_SLAB,1};
        assert(world_set_block(&a,13,64,8,BETA_BLOCK_SLAB));
        assert(world_set_metadata(&a,13,64,8,2));
        player.x=13.5f; player.y=65.88f; player.z=8.5f;
        player.pitch=-1.570796f;
        assert(player_place_block_state(&player,&a,wood_slab));
        assert(world_get_block(&a,13,64,8)==BETA_BLOCK_DOUBLE_SLAB);
        assert(world_get_metadata(&a,13,64,8)==2);
        assert(world_get_block(&a,13,65,8)==BLOCK_AIR);

        assert(world_set_block(&a,14,64,8,BETA_BLOCK_SLAB));
        assert(world_set_metadata(&a,14,64,8,1));
        player.x=14.5f;
        assert(player_place_block_state(&player,&a,wood_slab));
        assert(world_get_block(&a,14,64,8)==BETA_BLOCK_SLAB);
        assert(world_get_metadata(&a,14,64,8)==1);
        assert(world_get_block(&a,14,65,8)==BETA_BLOCK_SLAB);
        assert(world_get_metadata(&a,14,65,8)==2);

        assert(world_set_block(&a,11,64,8,BETA_BLOCK_SLAB));
        assert(world_set_metadata(&a,11,64,8,1));
        assert(world_set_block(&a,11,65,8,BLOCK_WATER));
        assert(world_set_metadata(&a,11,65,8,9));
        player.x=11.5f;
        assert(player_place_block_state(&player,&a,sand_slab));
        assert(world_get_block(&a,11,64,8)==BETA_BLOCK_DOUBLE_SLAB);
        assert(world_get_metadata(&a,11,64,8)==1);
        assert(world_get_block(&a,11,65,8)==BLOCK_AIR);
        assert(world_get_metadata(&a,11,65,8)==0);

        assert(world_set_block(&a,15,64,8,BETA_BLOCK_SLAB));
        assert(world_set_metadata(&a,15,64,8,1));
        player.x=15.5f; player.y=64.51f;
        assert(!player_place_block_state(&player,&a,sand_slab));
        assert(world_get_block(&a,15,64,8)==BETA_BLOCK_SLAB);
        assert(world_get_block(&a,15,65,8)==BLOCK_AIR);
        wood_slab.metadata=16;
        assert(!player_place_block_state(&player,&a,wood_slab));
    }
    assert(world_set_block(&a,8,64,8,BLOCK_STONE));
    player.x=8.5f; player.y=65.88f; player.z=8.5f;
    player.yaw=0; player.pitch=-1.570796f;
    assert(player_place_block(&player,&a,BLOCK_TORCH));
    assert(world_get_block(&a,8,65,8)==BLOCK_TORCH);
    assert(world_get_metadata(&a,8,65,8)==5);

    assert(world_set_block(&a,10,65,5,BLOCK_STONE));
    player.x=10.5f; player.y=63.88f; player.z=8.5f;
    player.yaw=0; player.pitch=0;
    assert(player_place_block(&player,&a,BLOCK_TORCH));
    assert(world_get_block(&a,10,65,6)==BLOCK_TORCH);
    assert(world_get_metadata(&a,10,65,6)==3);
    player.y=64.28f; /* eye above wall torch's 0.8-height box */
    hit=player_raycast(&player,&a,5);
    assert(hit.hit && hit.block==BLOCK_STONE && hit.z==5);
    player.y=64.0f;
    hit=player_raycast(&player,&a,5);
    assert(hit.hit && hit.block==BLOCK_TORCH && hit.z==6 && hit.place_z==7);
    assert(world_set_block(&a,9,65,5,BLOCK_GLASS));
    player.x=9.5f; player.y=63.88f; player.z=8.5f;
    assert(!player_place_block(&player,&a,BLOCK_TORCH));
    assert(world_get_block(&a,9,65,6)==BLOCK_AIR);
    assert(world_set_block(&a,9,67,8,BLOCK_STONE));
    player.y=64.0f; player.pitch=1.570796f;
    assert(!player_place_block(&player,&a,BLOCK_TORCH));
    assert(world_get_block(&a,9,66,8)==BLOCK_AIR);
    assert(world_set_block(&a,11,65,5,BETA_BLOCK_REDSTONE_TORCH));
    assert(world_set_metadata(&a,11,65,5,1));
    assert(world_set_block(&a,11,65,4,BLOCK_STONE));
    player.x=11.5f; player.y=63.88f; player.z=8.5f;
    player.yaw=0; player.pitch=0;
    hit=player_raycast(&player,&a,5);
    assert(hit.hit && hit.block==BLOCK_STONE && hit.z==4);
    player.x=11.15f;
    hit=player_raycast(&player,&a,5);
    assert(hit.hit && hit.block==BETA_BLOCK_REDSTONE_TORCH && hit.z==5);
    assert(world_close(&a)==WORLD_OK);
    puts("world persistence, negative coords, raycast, collision: pass");
    return 0;
}
