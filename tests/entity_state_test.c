#include "game/entity_render.h"
#include "game/player.h"
#include "game/modal_input.h"
#include "world/entities.h"
#include "world/collision.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

static void stepping(void)
{
    World w;Player p;PlayerInput in;int dir,t,variant;
    const int dx[4]={1,-1,0,0},dz[4]={0,0,1,-1};
    assert(world_init(&w,42,1,16)==WORLD_OK);
    assert(world_get_chunk(&w,0,0));
    for(variant=0;variant<3;++variant)for(dir=0;dir<4;++dir) {
        memset(&p,0,sizeof(p));memset(&in,0,sizeof(in));
        world_set_state(&w,8,64,8,(BetaBlockState){variant==0 ? 44 : variant==1 ? 53 : 67,(uint8_t)dir});
        p.x=8.5f-dx[dir]*1.5f;p.z=8.5f-dz[dir]*1.5f;p.y=64.00001f;p.health=20;p.air=300;
        /* Slab and stair bottom half approached from all four directions. */
        p.yaw=atan2f((float)dx[dir],(float)-dz[dir]);in.forward=1;
        {float highest=p.y;for(t=0;t<14;++t){player_tick(&p,&w,&in,.05f);highest=fmaxf(highest,p.y);}
        assert(highest>=64.49f && (p.x-8.5f)*dx[dir]+(p.z-8.5f)*dz[dir]>-.35f);}
        world_set_block(&w,8,64,8,0);
    }
    /* A slab under a low ceiling must not pull the player through the roof. */
    world_set_state(&w,8,64,8,(BetaBlockState){44,0});world_set_block(&w,8,66,8,1);
    memset(&p,0,sizeof(p));memset(&in,0,sizeof(in));p.x=7;p.z=8.5f;p.y=64;p.health=20;p.air=300;p.yaw=1.57079633f;in.forward=1;
    for(t=0;t<20;++t)player_tick(&p,&w,&in,.05f);
    assert(p.x<=7.701f && p.y<64.1f);
    world_set_block(&w,8,66,8,0);world_set_block(&w,8,64,8,0);
    /* Landing and a diagonal step across a negative chunk boundary. */
    world_set_state(&w,-1,64,-1,(BetaBlockState){44,0});
    memset(&p,0,sizeof(p));memset(&in,0,sizeof(in));p.x=-2;p.z=-2;p.y=64.1f;p.vy=-2;p.health=20;p.air=300;
    p.yaw=2.35619449f;in.forward=1;
    {float highest=0;for(t=0;t<12;++t){player_tick(&p,&w,&in,.05f);highest=fmaxf(highest,p.y);}assert(highest>=64.49f && p.x>-.7f && p.z>-.7f);}
    assert(world_close(&w)==WORLD_OK);
}
static void lighting(void)
{
    World w;Chunk *c;int x,z,id;
    assert(world_init(&w,42,1,16)==WORLD_OK);
    assert(world_get_chunk(&w,0,0));assert(world_get_chunk(&w,1,0));
    w.physics_processing=1;
    for(x=12;x<20;++x)for(z=5;z<12;++z)world_set_block(&w,x,68,z,1);
    w.physics_processing=0;world_finish_light_updates(&w);
    assert(world_sky_light(&w,15,66,8)>0 && world_sky_light(&w,16,66,8)>0);
    for(id=0;id<5;++id) {
        static const int blocks[5]={44,53,67,55,93};
        world_set_state(&w,15,64,8,(BetaBlockState){(uint8_t)blocks[id],0});
        assert(world_render_light(&w,15,64,8)>0);
    }
    w.sky_subtracted=15;world_set_state(&w,15,65,8,(BetaBlockState){76,5});
    world_color_lighting(&w,1);world_finish_color_updates(&w);
    assert(world_red_light(&w,16,65,8)==6);
    c=world_peek_chunk(&w,1,0);assert(c && c->red_light);
    world_set_block(&w,15,65,8,0);world_finish_color_updates(&w);
    assert(world_red_light(&w,16,65,8)==0);
    world_color_lighting(&w,0);assert(!c->red_light);
    /* Network colour must never overwrite vanilla SkyLight or BlockLight. */
    w.network_mode=1;chunk_set_block(c,0,65,8,76);chunk_set_sky_light(c,0,65,8,4);chunk_set_block_light(c,0,65,8,7);
    world_color_lighting(&w,1);world_finish_color_updates(&w);world_finish_light_updates(&w);
    assert(chunk_get_sky_light(c,0,65,8)==4 && chunk_get_block_light(c,0,65,8)==7);
    assert(world_close(&w)==WORLD_OK);
}
static void poses_and_items(void)
{
    RenderEntity e[3]={{0}};World w;Player p={0};float x,y,z,yaw,roll;int i;
    int press=0;
    assert(gameplay_input_ready(&press,0,0));
    assert(!gameplay_attack_blocks(&press,1,1));
    assert(!gameplay_attack_blocks(&press,1,0)); /* Entity died; still no mining. */
    assert(!gameplay_attack_blocks(&press,0,0));
    assert(gameplay_attack_blocks(&press,1,0)); /* A fresh click can target terrain. */
    RendererCamera camera={0,1.62f,0,0,0,70};
    e[0].active=1;e[0].id=4;e[0].type=54;e[0].z=-2;
    e[1].active=1;e[1].id=7;e[1].type=1001;e[1].z=-3;e[1].y=1.3f;
    assert(entity_pick(e,3,&camera,5,5)==4);assert(entity_pick(e,3,&camera,5,1)==-1);
    assert(entity_pick_except(e,3,&camera,5,5,4)==7);
    entity_render_update(e,3,.01f,0);entity_remote_pose(&e[0],1,0,-2,359,0);
    for(i=0;i<15;++i)entity_render_update(e,3,.01f,0);
    assert(fabsf(e[0].draw_x-1)<.0001f && e[0].walk_amount>0);
    for(i=0;i<150;++i)entity_render_update(e,3,.01f,0);
    assert(e[0].walk_amount<.0001f);
    e[1].x=5;e[1].y=65;e[1].z=9;e[1].yaw=0;
    assert(entity_rider_position(e,3,7,0,&x,&y,&z) && x==5 && fabsf(y-64.2f)<.0001f && z==9);
    e[1].type=1002;entity_rider_position(e,3,7,0,&x,&y,&z);assert(fabsf(x-5.4f)<.0001f);
    for(i=0;i<8;++i){entity_spider_leg_pose(0,0,i,&yaw,&roll);assert((i&1) ? roll>0 : roll<0);}
    p.x=e[0].x+.1f;p.z=e[0].z;p.y=0;p.health=20;entity_network_collide(e,1,&p);assert(p.push_x>0);
    assert(world_init(&w,42,1,8)==WORLD_OK);w.network_mode=1;
    memset(e,0,sizeof(e));e[0].active=1;e[0].type=1008;e[0].item=(InventorySlot){278,3,9};e[0].x=8;e[0].y=65;e[0].z=8;
    entity_network_tick(e,1,&w);assert(e[0].y==65);
    {Chunk *c=world_get_chunk(&w,0,0);assert(c);c->network_received=1;chunk_set_block(c,8,63,8,1);}
    for(i=0;i<50;++i)entity_network_tick(e,1,&w);
    assert(e[0].y>=64.124f && e[0].y<64.14f && e[0].item.count==3 && e[0].item.damage==9);
    assert(world_close(&w)==WORLD_OK);
}
static void mob_health(void)
{
    World w;Player p={0};InventorySlot hand={0};SavedEntity *mob;int health;
    assert(world_init(&w,42,1,16)==WORLD_OK);assert(world_get_chunk(&w,0,0));
    assert(world_mob_spawn(&w,54,8.5f,64,6.5f));mob=w.cache[0]->saved_entities;
    p.x=8.5f;p.y=64;p.z=8.5f;p.health=20;p.creative=1;
    health=mob->mob.health;assert(world_mobs_attack(&w,&p,&hand,3));assert(mob->mob.health==health-1);
    assert(world_mobs_attack(&w,&p,&hand,3));assert(mob->mob.health==health-1);
    {int t;PlayerInput input={0};w.difficulty=2;
     assert(world_mob_hit(&w,mob,100));
     for(t=0;t<20;++t){++w.tick;world_mobs_tick(&w,&p);}
     /* Completed death animations and dead mobs do not remain attack targets. */
     assert(world_mobs_visible(&w,(RenderEntity[8]){{0}},8)==0);
     p.riding=1;w.network_mode=1;input.forward=1;player_tick(&p,&w,&input,.05f);assert(fabsf(p.vz+.392f)<.00001f);
    }
    assert(beta_attack_damage(268)==4 && beta_attack_damage(276)==10 && beta_attack_damage(278)==5);
    assert(world_close(&w)==WORLD_OK);
}
int main(void)
{stepping();lighting();poses_and_items();mob_health();puts("Entity poses, Item physics, riding, half-block stepping and partial light passed");return 0;}
