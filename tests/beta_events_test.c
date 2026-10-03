#include "world/foliage.h"
#include "world/fire.h"
#include "world/explosion.h"
#include "world/entities.h"
#include "world/environment.h"
#include "game/player.h"
#include "game/bed.h"
#include "game/mining.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
static void init(World *w)
{int x,z;assert(world_init(w,42,1,16)==WORLD_OK);for(x=-1;x<=1;++x)for(z=-1;z<=1;++z)assert(world_get_chunk(w,x,z));w->difficulty=2;}
static int count_kind(const World *w,int kind)
{size_t i;int n=0;for(i=0;i<w->cache_count;++i){SavedEntity *e;for(e=w->cache[i]->saved_entities;e;e=e->next)if(e->transport.kind==kind&&!e->transport.dead)++n;}return n;}
static void foliage(void)
{
    World w;int i;init(&w);assert(world_set_block(&w,13,70,8,17));
    for(i=14;i<=17;++i)assert(world_set_state(&w,i,70,8,(BetaBlockState){18,9}));
    world_leaf_tick(&w,17,70,8);assert(world_peek_metadata(&w,17,70,8)==1);
    assert(world_set_block(&w,13,70,8,0));assert(world_peek_metadata(&w,17,70,8)&8);
    world_leaf_tick(&w,17,70,8);assert(!world_peek_block(&w,17,70,8));
    assert(world_set_state(&w,31,70,8,(BetaBlockState){18,10}));world_leaf_tick(&w,31,70,8);
    assert(world_peek_block(&w,31,70,8)==18); /* incomplete neighboring chunk: defer */
    for(i=0;i<20;++i){InventorySlot drop=mining_drop((BetaBlockState){18,2},0,(unsigned)i);assert(drop.id!=260);assert(drop.count==(i==0));}
    assert(world_close(&w)==WORLD_OK);
}
static void fire(void)
{
    World w;int n;init(&w);
    assert(!world_ignite(&w,8,80,8));assert(world_set_block(&w,8,79,8,87));assert(world_ignite(&w,8,80,8));
    assert(w.physics_count && w.physics[0].due==w.tick+40);
    w.raining=1;world_fire_tick(&w,8,80,8);assert(world_peek_block(&w,8,80,8)==51);
    assert(world_ignite(&w,4,64,4));world_fire_tick(&w,4,64,4);assert(!world_peek_block(&w,4,64,4));
    w.raining=0;assert(world_set_block(&w,9,80,8,46));
    for(n=0;n<100 && world_peek_block(&w,9,80,8)==46;++n)world_fire_tick(&w,8,80,8);
    assert(world_peek_block(&w,9,80,8)!=46 && count_kind(&w,4)==1);
    assert(fire_encouragement(17)==5&&fire_burn_rate(18)==60&&fire_burn_rate(1)==0);
    /* Stationary lava can ignite wood one block above, with bounded random
     * steps. No neighboring chunks are generated as a side effect. */
    assert(world_set_block(&w,2,80,2,11));
    for(n=0;n<3;++n)assert(world_set_block(&w,1+n,81,4,5));
    for(n=0;n<200 && !world_peek_block(&w,2,81,3);++n)world_lava_ignite_tick(&w,2,80,2);
    assert(world_peek_block(&w,2,81,3)==51);
    assert(world_extinguish_fire(&w,2,81,3)&&world_peek_block(&w,2,81,4)==5);
    assert(beta_material_burns(54)&&!fire_encouragement(54));
    assert(world_set_block(&w,6,80,6,11));
    assert(world_set_block(&w,6,80,7,1));
    for(n=0;n<3;++n)assert(world_set_block(&w,5+n,81,8,54));
    for(n=0;n<200 && !world_peek_block(&w,6,81,7);++n)world_lava_ignite_tick(&w,6,80,6);
    assert(world_peek_block(&w,6,81,7)==51); /* Material.wood, absent in spread table. */
    assert(world_close(&w)==WORLD_OK);
}
static void explosion(void)
{
    World w;Player p={0};InventorySlot inventory[36]={{0}};SavedEntity *e;int i;
    init(&w);p.x=p.z=-8;p.y=64;p.health=20;p.creative=1;
    assert(world_set_block(&w,8,64,8,46));assert(world_set_block(&w,9,64,8,46));
    assert(world_tnt_prime(&w,8.5f,64.5f,8.5f,80));assert(world_set_block(&w,8,64,8,0));
    for(i=0;i<80;++i){world_step_physics(&w,4096);world_transport_tick(&w,&p,inventory);}assert(count_kind(&w,4)==1);
    world_step_physics(&w,4096);world_transport_tick(&w,&p,inventory);
    assert(!world_peek_block(&w,9,64,8)&&count_kind(&w,4)==1); /* chain entity must survive current unlink */
    e=world_peek_chunk(&w,0,0)->saved_entities;while(e&&e->transport.kind!=4)e=e->next;
    assert(e&&e->transport.fuse>=10&&e->transport.fuse<30);
    assert(world_set_block(&w,6,70,6,49));assert(world_set_block(&w,7,70,6,7));
    world_explode(&w,&p,6.5f,70.5f,7.5f,4,0);
    assert(world_peek_block(&w,6,70,6)==49&&world_peek_block(&w,7,70,6)==7);
    assert(beta_blast_resistance(9)==100&&beta_blast_resistance(1)==6);
    assert(beta_blast_resistance(10)==0&&beta_blast_resistance(14)==3&&beta_blast_resistance(84)==6);
    assert(world_close(&w)==WORLD_OK);
    /* Exposure is calculated before block destruction; a solid wall shields
     * the player, while an unobstructed blast leaves a persistent impulse. */
    init(&w);p=(Player){0};p.x=10.5f;p.y=64;p.z=8.5f;p.health=20;
    world_explode(&w,&p,8.5f,65,8.5f,2,0);assert(p.health<20&&p.push_x>0&&p.vy>0);
    assert(world_close(&w)==WORLD_OK);
    init(&w);p=(Player){0};p.x=10.5f;p.y=64;p.z=8.5f;p.health=20;
    for(i=62;i<69;++i){int z;for(z=3;z<14;++z)assert(world_set_block(&w,9,i,z,49));}
    world_explode(&w,&p,8.5f,65,8.5f,2,0);assert(p.health==20&&p.push_x==0);
    assert(world_close(&w)==WORLD_OK);
}
static void entity_nbt(void)
{
    World w;Chunk copy={0};NbtWriter writer;NbtTag tag={0};unsigned char bytes[8192];size_t size;SavedEntity *e;
    init(&w);assert(world_tnt_prime(&w,8,65,8,37));assert(world_mob_spawn(&w,50,10,64,10));
    e=world_peek_chunk(&w,0,0)->saved_entities;e->mob.powered=1;e->mob.air=63;
    nbt_writer_init(&writer,bytes,sizeof(bytes),NULL);tag.type=NBT_COMPOUND;
    assert(nbt_writer_tag(&writer,&tag)==NBT_OK);tag.name=nbt_span("Level");assert(nbt_writer_tag(&writer,&tag)==NBT_OK);
    assert(world_entities_write_list(world_peek_chunk(&w,0,0),&writer));assert(nbt_writer_end(&writer)==NBT_OK&&nbt_writer_end(&writer)==NBT_OK&&nbt_writer_finish(&writer,&size)==NBT_OK);
    assert(world_entities_read(&copy,bytes,size));e=copy.saved_entities;assert(e->mob.type==50&&e->mob.powered&&e->mob.air==63);
    e=e->next;assert(e&&e->transport.kind==4&&e->transport.fuse==37);
    world_entities_free(&copy);assert(world_close(&w)==WORLD_OK);
}
static void drop_and_path(void)
{
    World w;Player p={0};InventorySlot held={278,2,37};SavedEntity *e;MobPath path;int z,y,i;
    init(&w);p.x=p.z=8.5f;p.y=64;p.health=20;
    assert(world_item_throw(&w,&p,&held));e=world_peek_chunk(&w,0,0)->saved_entities;
    assert(held.count==1&&e->item.count==1&&e->item.damage==37&&e->item.pickup_delay==40&&e->item.vz< -5.5f);
    assert(fabsf(e->item.y-(p.y+1.44f))<1e-5f);
    for(z=6;z<=10;++z)for(y=64;y<=65;++y)assert(world_set_block(&w,10,y,z,1));
    assert(mob_path_find(&w,.6f,1.8f,8.5f,64,8.5f,12.5f,64,8.5f,&path));
    assert(path.x[path.count-1]==12&&path.z[path.count-1]==8);
    for(i=0;i<path.count;++i)assert(!(path.x[i]==10&&path.z[i]>=6&&path.z[i]<=10));
    assert(world_close(&w)==WORLD_OK);
}
static void hostiles(void)
{
    World w;Player p={0};InventorySlot inv[36]={{0}};SavedEntity *e;int i;
    init(&w);w.beta_world_time=18000;world_environment_refresh(&w);p.x=p.z=8.5f;p.y=64;p.health=20;
    assert(world_mob_spawn(&w,51,8.5f,64,14.5f));
    for(i=0;i<3;++i){world_step_physics(&w,4096);world_mobs_tick(&w,&p);world_transport_tick(&w,&p,inv);}assert(count_kind(&w,1)>0);
    assert(world_mob_spawn(&w,50,10.5f,64,8.5f));e=world_peek_chunk(&w,0,0)->saved_entities;e->mob.target_player=1;
    for(i=0;i<29;++i){++w.tick;world_mobs_tick(&w,&p);}assert(e->mob.fuse==29&&e->mob.health>0);
    ++w.tick;world_mobs_tick(&w,&p);assert(e->mob.health==0);assert(w.sound_count>0);
    assert(world_close(&w)==WORLD_OK);
    init(&w);p=(Player){0};p.x=p.z=0;p.y=64;p.health=20;w.spawn_x=w.spawn_z=-100;
    w.beta_world_time=6000;world_environment_refresh(&w);
    for(i=0;i<100;++i)assert(!world_mob_can_spawn(&w,54,30,64,30,&p));
    w.beta_world_time=18000;world_environment_refresh(&w);
    for(i=0;i<100&&!world_mob_can_spawn(&w,54,30,64,30,&p);++i){}
    assert(i<100);assert(!world_mob_can_spawn(&w,54,3,64,3,&p));
    assert(world_close(&w)==WORLD_OK);
}
static void player_position(void)
{
    World w;Player p={0};init(&w);w.beta_has_player=1;
    w.beta_player_x=w.beta_player_z=8.5;w.beta_player_y=64+PLAYER_BETA_ENTITY_Y_OFFSET;
    w.beta_player_yaw=180;w.beta_player_pitch=100;
    assert(!player_restore_beta(&p,&w,0)&&fabsf(p.y-64)<1e-5f);
    assert(p.yaw==0&&p.pitch==player_clamp_pitch(-2));
    /* Only saves previously touched by ReCraft use the feet-coordinate
     * recovery. Authentic vanilla positions retain the original convention. */
    w.beta_player_y=64;
    assert(!player_restore_beta(&p,&w,0)&&fabsf(p.y-62.38f)<1e-5f);
    assert(player_restore_beta(&p,&w,1)&&p.y==64);
    w.beta_player_y=80;
    assert(!player_restore_beta(&p,&w,1)&&fabsf(p.y-78.38f)<1e-5f);
    w.beta_player_y=64.9375;w.beta_player_sleeping=1;
    assert(bed_place(&w,8,64,8,0));w.beta_player_z=9.5;
    assert(world_set_metadata(&w,8,64,9,12));
    assert(!player_restore_beta(&p,&w,1)&&!p.sleeping);
    assert(!(world_peek_metadata(&w,8,64,9)&4)&&p.y>64&&p.y<65);
    assert(world_close(&w)==WORLD_OK);
}
int main(void)
{foliage();fire();explosion();entity_nbt();drop_and_path();hostiles();player_position();puts("Beta foliage, fire, TNT, NBT, Q, paths and hostile attacks passed");return 0;}
