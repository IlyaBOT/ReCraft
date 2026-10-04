#include "world/world.h"
#include "world/block_entity.h"
#include "world/fluid.h"
#include "world/entities.h"
#include "world/ticks.h"
#include "game/player.h"
#include "game/commands.h"
#include "game/bed.h"
#include "world/environment.h"
#include "world/redstone.h"
#include "world/door.h"
#include "world/mobs.h"
#include "game/entity_render.h"
#include "nbt/nbt.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef _WIN32
#include <direct.h>
#include <process.h>
#define test_mkdir(p) _mkdir(p)
#define test_rmdir(p) _rmdir(p)
#define test_pid() _getpid()
#else
#include <sys/stat.h>
#include <unistd.h>
#define test_mkdir(p) mkdir(p,0700)
#define test_rmdir(p) rmdir(p)
#define test_pid() getpid()
#endif
static void ticks(World *w,int n) { while (n--) world_step_physics(w,4096); }
static void fluid_tests(void)
{
    World w; int x,z;
    assert(world_init(&w,42,1,32)==WORLD_OK);
    for (x=-1;x<=1;++x) for(z=-1;z<=1;++z) assert(world_get_chunk(&w,x,z));
    assert(world_set_block(&w,0,64,0,8));
    ticks(&w,4); assert(world_peek_block(&w,1,64,0)==0);
    ticks(&w,1); assert(fluid_kind(world_peek_block(&w,1,64,0))==1 && world_peek_metadata(&w,1,64,0)==1);
    ticks(&w,100);
    assert(world_peek_metadata(&w,7,64,0)==7 && fluid_kind(world_peek_block(&w,7,64,0))==1);
    assert(world_peek_block(&w,8,64,0)==0 && world_peek_block(&w,4,64,4)==0);
    assert(world_set_block(&w,0,64,0,0)); ticks(&w,160);
    for(x=-8;x<=8;++x) for(z=-8;z<=8;++z) assert(world_peek_block(&w,x,64,z)==0);
    assert(world_set_block(&w,-1,64,0,8) && world_set_block(&w,1,64,0,8)); ticks(&w,80);
    assert(world_peek_metadata(&w,0,64,0)==0 && fluid_kind(world_peek_block(&w,0,64,0))==1);
    assert(world_close(&w)==WORLD_OK);
    assert(world_init(&w,42,1,32)==WORLD_OK);
    for(x=-1;x<=1;++x) for(z=-1;z<=1;++z) assert(world_get_chunk(&w,x,z));
    assert(world_set_block(&w,0,64,0,10)); ticks(&w,29);
    assert(world_peek_block(&w,1,64,0)==0); ticks(&w,1);
    assert(fluid_kind(world_peek_block(&w,1,64,0))==2 && world_peek_metadata(&w,1,64,0)==2);
    ticks(&w,400); assert(fluid_kind(world_peek_block(&w,3,64,0))==2 && world_peek_metadata(&w,3,64,0)==6);
    assert(world_peek_block(&w,4,64,0)==0);
    assert(world_set_block(&w,0,64,0,0)); ticks(&w,1000);
    for(x=-4;x<=4;++x) for(z=-4;z<=4;++z) assert(world_peek_block(&w,x,64,z)==0);
    assert(world_close(&w)==WORLD_OK);
}
static void regressions(void)
{
    World w; Player p={0}; PlayerInput in={0}; int id,dir,i;
    static const int dx[4]={0,-1,0,1},dz[4]={1,0,-1,0};
    assert(world_init(&w,42,1,32)==WORLD_OK);
    for(i=-1;i<=1;++i) { assert(world_get_chunk(&w,i,0)); assert(world_get_chunk(&w,0,i)); }
    for(id=64;id<=71;id+=7) for(dir=0;dir<4;++dir) {
        assert(door_place(&w,8,64,8,(unsigned)id,dir*1.57079633f));
        ticks(&w,1); assert(world_peek_block(&w,8,64,8)==id && world_peek_block(&w,8,65,8)==id);
        assert(world_peek_metadata(&w,8,65,8)==(world_peek_metadata(&w,8,64,8)|8));
        i=world_peek_metadata(&w,8,64,8);
        assert(door_activate(&w,8,65,8));
        assert(world_peek_metadata(&w,8,64,8)==(id==64 ? (i^4) : i));
        if(id==64) { ticks(&w,5); assert(world_peek_metadata(&w,8,64,8)==(i^4)); }
        world_set_block(&w,9,64,8,69); world_set_metadata(&w,9,64,8,5);
        world_redstone_activate(&w,9,64,8); assert(world_peek_metadata(&w,8,64,8)&4);
        world_redstone_activate(&w,9,64,8); assert(!(world_peek_metadata(&w,8,64,8)&4));
        world_set_block(&w,9,64,8,0); world_set_block(&w,8,65,8,0); ticks(&w,1); assert(!world_peek_block(&w,8,64,8));
    }
    assert(door_place(&w,8,64,8,64,0) && door_place(&w,9,64,8,64,0));
    ticks(&w,5); assert(world_peek_metadata(&w,9,64,8)==6 && world_peek_metadata(&w,9,65,8)==14);
    world_set_block(&w,8,64,8,0); world_set_block(&w,9,64,8,0); ticks(&w,1);
    /* A removed surface source must regenerate over source water, too. */
    for(i=0;i<3;++i) { world_set_state(&w,8+i,63,8,(BetaBlockState){9,0}); world_set_state(&w,8+i,64,8,(BetaBlockState){9,0}); }
    world_set_block(&w,9,64,8,0); ticks(&w,30);
    assert(fluid_kind(world_peek_block(&w,9,64,8))==1 && world_peek_metadata(&w,9,64,8)==0);
    assert(world_close(&w)==WORLD_OK);
    assert(world_init(&w,42,1,32)==WORLD_OK); assert(world_get_chunk(&w,0,0));
    for(i=0;i<3;++i) { world_set_state(&w,8,64+i*2,8,(BetaBlockState){76,5}); if(i<2) world_set_block(&w,8,65+i*2,8,1); }
    ticks(&w,10); assert(world_peek_block(&w,8,64,8)==76 && world_peek_block(&w,8,66,8)==75 && world_peek_block(&w,8,68,8)==76);
    world_set_state(&w,9,63,8,(BetaBlockState){69,9}); ticks(&w,10);
    assert(world_peek_block(&w,8,64,8)==75 && world_peek_block(&w,8,66,8)==76 && world_peek_block(&w,8,68,8)==75);
    world_set_block(&w,9,63,8,0); ticks(&w,10); assert(world_peek_block(&w,8,64,8)==76 && world_peek_block(&w,8,68,8)==76);
    assert(world_close(&w)==WORLD_OK);
    for(dir=0;dir<4;++dir) {
        assert(world_init(&w,42,1,32)==WORLD_OK); assert(world_get_chunk(&w,0,0));
        world_set_state(&w,8,64,8,(BetaBlockState){93,(uint8_t)(dir|12)});
        world_set_state(&w,8+dx[dir],64,8+dz[dir],(BetaBlockState){76,5});
        ticks(&w,7); assert(world_peek_block(&w,8,64,8)==93); ticks(&w,1);
        assert(world_peek_block(&w,8,64,8)==94 && world_peek_metadata(&w,8,64,8)==(dir|12));
        assert(world_redstone_signal(&w,8,64,8,8-dx[dir],64,8-dz[dir],1)==15);
        world_set_block(&w,8+dx[dir],64,8+dz[dir],0); ticks(&w,8); assert(world_peek_block(&w,8,64,8)==93);
        assert(world_close(&w)==WORLD_OK);
    }
    assert(world_init(&w,42,1,16)==WORLD_OK); player_spawn(&p,&w,1); p.flying=0; p.y=64; p.x=p.z=8.5f;
    in.look_dy=-10000; player_tick(&p,&w,&in,.05f); assert(fabsf(p.pitch-1.57079633f)<.00001f);
    assert(player_clamp_pitch(p.pitch+10)==p.pitch);
    in.look_dy=10000; player_tick(&p,&w,&in,.05f); assert(fabsf(p.pitch+1.57079633f)<.00001f);
    assert(world_mob_spawn(&w,92,8.8f,64,8.5f)); world_entities_collide(&w,&p);
    assert(p.push_x<0); in.look_dy=0; i=(int)(p.x*1000); player_tick(&p,&w,&in,.05f); assert(p.x*1000<i);
    assert(world_close(&w)==WORLD_OK);
}
static void container_tests(void)
{
    World w; BlockEntity *a,*b,*f; WorldDropEvent drop;
    char root[128]; int i;
    snprintf(root,sizeof(root),"build/gameplay-test-%d",(int)test_pid()); assert(test_mkdir(root)==0);
    assert(world_create(&w,root,"fixture","Container fixture",42,1,0,0,16)==WORLD_OK);
    assert(world_set_block(&w,4,64,4,54));
    assert(block_chest_halves(&w,4,64,4,&a,&b)==27 && a && !b);
    a->slots[0]=(InventorySlot){35,23,14}; block_entity_changed(&w,a);
    assert(block_chest_can_place(&w,5,64,4)); assert(world_set_block(&w,5,64,4,54));
    assert(block_chest_halves(&w,5,64,4,&a,&b)==54 && a->x==4 && b->x==5);
    b->slots[26]=(InventorySlot){278,1,400}; block_entity_changed(&w,b);
    assert(!block_chest_can_place(&w,6,64,4));
    assert(world_set_block(&w,5,65,4,1)); assert(!block_chest_halves(&w,4,64,4,&a,&b));
    assert(world_set_block(&w,5,65,4,0));
    assert(world_set_block(&w,8,64,4,61)); f=block_entity_get(&w,8,64,4,1); assert(f);
    f->slots[0]=(InventorySlot){17,2,1}; f->slots[1]=(InventorySlot){263,1,1};
    block_entity_changed(&w,f);
    for(i=0;i<199;++i) block_entities_tick(&w);
    assert(f->slots[2].count==0 && f->cook==199 && f->burn>0 && world_peek_block(&w,8,64,4)==62);
    block_entities_tick(&w); assert(f->slots[2].id==263 && f->slots[2].damage==1 && f->slots[0].count==1);
    assert(door_place(&w,11,64,4,64,0) && door_place(&w,12,64,4,64,0));
    assert(world_close(&w)==WORLD_OK);
    assert(world_open(&w,root,"fixture",16)==WORLD_OK);
    assert(world_get_block(&w,12,64,4)==64 && world_get_metadata(&w,12,64,4)==6);
    assert(world_get_block(&w,12,65,4)==64 && world_get_metadata(&w,12,65,4)==14);
    assert(block_chest_halves(&w,4,64,4,&a,&b)==54);
    assert(a->slots[0].id==35 && a->slots[0].count==23 && a->slots[0].damage==14);
    assert(b->slots[26].id==278 && b->slots[26].damage==400);
    f=block_entity_get(&w,8,64,4,0); assert(f && f->cook==0 && f->slots[2].damage==1 && f->burn>0);
    assert(world_set_block(&w,4,64,4,0)); assert(world_take_drop(&w,&drop));
    assert(drop.id==35 && drop.damage==14 && drop.count==23);
    assert(!world_take_drop(&w,&drop)); assert(block_chest_halves(&w,5,64,4,&a,&b)==27);
    assert(world_save(&w)==WORLD_OK);
    assert(world_set_block(&w,8,65,5,20) && world_save(&w)==WORLD_OK);
    assert(world_close(&w)==WORLD_OK);
    assert(world_open(&w,root,"fixture",16)==WORLD_OK);
    assert(block_chest_halves(&w,5,64,4,&a,&b)==27 && a->slots[26].damage==400);
    {
        ItemDrop items[4]; int n=world_items_visible(&w,items,4);
        assert(n==1 && items[0].id==35 && items[0].count==23 && items[0].damage==14);
    }
    assert(world_close(&w)==WORLD_OK);
    assert(world_storage_delete(root,"fixture")==WORLD_OK); assert(test_rmdir(root)==0);
}
static void health_redstone_tests(void)
{
    World w; Player p; PlayerInput in={0}; int i;
    assert(world_init(&w,42,1,16)==WORLD_OK); player_spawn(&p,&w,0);
    assert(p.health==20 && p.air==300);
    p.y=67.2f;
    for(i=0;i<50;++i) player_tick(&p,&w,&in,.05f);
    assert(p.on_ground && p.health==19 && p.fall_distance==0);
    p.health=20; p.hurt_ticks=0; p.y=70; p.vy=0; p.on_ground=0;
    for (i=0;i<100;++i) player_tick(&p,&w,&in,.05f);
    assert(p.health==17); player_damage(&p,2); assert(p.health==15);
    player_damage(&p,2); assert(p.health==15); player_damage(&p,4); assert(p.health==13);
    {
        InventorySlot soup={282,1,0};
        assert(player_use_item(&p,&w,&soup) && p.health==20 && soup.id==281);
    }
    assert(world_set_block(&w,5,64,5,1));
    assert(world_set_block(&w,5,65,5,76)); assert(world_set_metadata(&w,5,65,5,5));
    assert(world_set_block(&w,6,64,5,69)); assert(world_set_metadata(&w,6,64,5,9));
    world_physics_notify(&w,6,64,5); ticks(&w,2);
    assert(world_peek_block(&w,5,65,5)==75);
    world_set_metadata(&w,6,64,5,1); world_physics_notify(&w,6,64,5); ticks(&w,2);
    assert(world_peek_block(&w,5,65,5)==76);
    assert(world_close(&w)==WORLD_OK);
}

static void bucket_tests(void)
{
    World w; Player p; InventorySlot bucket={325,1,0};
    assert(world_init(&w,42,1,16)==WORLD_OK); player_spawn(&p,&w,0);
    p.x=8.5f; p.y=63; p.z=11.5f; p.yaw=p.pitch=0;
    assert(world_set_block(&w,8,64,7,1));
    assert(world_set_block(&w,8,64,8,8));
    assert(world_set_metadata(&w,8,64,8,1));
    /* Flowing water cannot be collected, even with a solid block behind it. */
    assert(!player_use_item(&p,&w,&bucket) && bucket.id==325);
    assert(world_peek_block(&w,8,64,8)==8 && world_peek_metadata(&w,8,64,8)==1);
    assert(world_set_metadata(&w,8,64,8,0));
    assert(player_use_item(&p,&w,&bucket) && bucket.id==326);
    assert(world_peek_block(&w,8,64,8)==0);
    assert(player_use_item(&p,&w,&bucket) && bucket.id==325);
    assert(world_peek_block(&w,8,64,8)==8 && world_peek_metadata(&w,8,64,8)==0);
    assert(world_set_block(&w,8,64,8,10));
    assert(player_use_item(&p,&w,&bucket) && bucket.id==327);
    assert(player_use_item(&p,&w,&bucket) && bucket.id==325);
    assert(world_peek_block(&w,8,64,8)==10 && world_peek_metadata(&w,8,64,8)==0);
    p.creative=1;
    assert(player_use_item(&p,&w,&bucket) && bucket.id==325);
    assert(world_peek_block(&w,8,64,8)==0);
    /* A block placed against the solid bank can replace either lava ID,
     * just as water; the ray does not treat fluid as the clicked support. */
    assert(world_set_block(&w,8,64,8,10));
    assert(player_place_block(&p,&w,5) && world_peek_block(&w,8,64,8)==5);
    assert(world_set_block(&w,8,64,8,11));
    assert(player_place_block(&p,&w,1) && world_peek_block(&w,8,64,8)==1);
    assert(world_close(&w)==WORLD_OK);
}

static void tick_save_tests(void)
{
    World w; char root[128]; Chunk *c; SavedTick *unknown; uint8_t raw[256];
    NbtWriter writer; NbtTag tag={0}; size_t size;
    snprintf(root,sizeof(root),"build/tick-test-%d",(int)test_pid()); assert(test_mkdir(root)==0);
    assert(world_create(&w,root,"fixture","Tick fixture",42,1,0,0,2)==WORLD_OK);
    assert(world_set_block(&w,8,64,8,10)); ticks(&w,7);
    c=world_peek_chunk(&w,0,0); assert(c && c->ticks && c->ticks->due-w.tick==23);
    /* Unsupported mechanisms and their private tags survive unchanged. */
    nbt_writer_init(&writer,raw,sizeof(raw),NULL);
    tag.type=NBT_COMPOUND; assert(nbt_writer_tag(&writer,&tag)==NBT_OK);
    tag.type=NBT_INT; tag.name=nbt_span("i"); tag.value.int_value=95; assert(nbt_writer_tag(&writer,&tag)==NBT_OK);
    tag.name=nbt_span("t"); tag.value.int_value=44; assert(nbt_writer_tag(&writer,&tag)==NBT_OK);
    tag.name=nbt_span("custom"); tag.value.int_value=5678; assert(nbt_writer_tag(&writer,&tag)==NBT_OK);
    assert(nbt_writer_end(&writer)==NBT_OK && nbt_writer_finish(&writer,&size)==NBT_OK);
    unknown=(SavedTick *)calloc(1,sizeof(*unknown)); assert(unknown);
    unknown->raw=(uint8_t *)malloc(size); assert(unknown->raw); memcpy(unknown->raw,raw,size);
    unknown->raw_size=size; unknown->id=95; unknown->delay=44; unknown->next=c->ticks; c->ticks=unknown;
    assert(world_save(&w)==WORLD_OK);
    assert(world_set_block(&w,1,65,1,20) && world_save(&w)==WORLD_OK);
    /* Eviction removes queue pointers before freeing the owning records. */
    assert(world_get_chunk(&w,1,0) && world_get_chunk(&w,2,0));
    assert(!world_peek_chunk(&w,0,0) && w.physics_count==0);
    c=world_get_chunk(&w,0,0); assert(c);
    for(unknown=c->ticks;unknown && unknown->managed;unknown=unknown->next) { }
    assert(unknown && unknown->raw_size==size && !memcmp(unknown->raw,raw,size));
    assert(world_close(&w)==WORLD_OK);
    assert(world_open(&w,root,"fixture",2)==WORLD_OK); c=world_get_chunk(&w,0,0); assert(c);
    ticks(&w,22); assert(world_peek_block(&w,9,64,8)==0);
    ticks(&w,1); assert(fluid_kind(world_peek_block(&w,9,64,8))==2);
    assert(world_close(&w)==WORLD_OK);
    assert(world_storage_delete(root,"fixture")==WORLD_OK && test_rmdir(root)==0);
}

static int entity_canaries(void *context,NbtEvent event,const NbtTag *t,unsigned depth)
{
    int *seen=(int *)context; (void)depth;
    if(event!=NBT_VALUE) return 1;
    if(t->type==NBT_INT && t->name.size==6 && !memcmp(t->name.data,"custom",6)) {
        assert(t->value.int_value==99 || t->value.int_value==77);
        *seen|=t->value.int_value==99 ? 1 : 2;
    }
    if(t->type==NBT_BYTE && t->name.size==5 && !memcmp(t->name.data,"Count",5)) {
        assert(t->value.byte==6); *seen|=4;
    }
    return 1;
}
static void entity_nbt_tests(void)
{
    uint8_t raw[1024],*updated; size_t size,out_size; Chunk *c=(Chunk *)calloc(1,sizeof(*c));
    NbtWriter writer; NbtTag t={0}; SavedEntity *e; int i,seen=0;
    assert(c); nbt_writer_init(&writer,raw,sizeof(raw),NULL);
    t.type=NBT_COMPOUND; assert(nbt_writer_tag(&writer,&t)==NBT_OK);
    t.name=nbt_span("Level"); assert(nbt_writer_tag(&writer,&t)==NBT_OK);
    t.type=NBT_LIST; t.name=nbt_span("Entities"); t.list_type=NBT_COMPOUND; t.count=2;
    assert(nbt_writer_tag(&writer,&t)==NBT_OK);
    for(i=0;i<2;++i) {
        memset(&t,0,sizeof(t)); t.type=NBT_COMPOUND; assert(nbt_writer_tag(&writer,&t)==NBT_OK);
        t.type=NBT_STRING; t.name=nbt_span("id"); t.value.bytes=nbt_span(i ? "Item" : "Pig"); assert(nbt_writer_tag(&writer,&t)==NBT_OK);
        if(i) {
            t.type=NBT_COMPOUND; t.name=nbt_span("Item"); assert(nbt_writer_tag(&writer,&t)==NBT_OK);
            t.type=NBT_SHORT; t.name=nbt_span("id"); t.value.short_value=35; assert(nbt_writer_tag(&writer,&t)==NBT_OK);
            t.name=nbt_span("Damage"); t.value.short_value=14; assert(nbt_writer_tag(&writer,&t)==NBT_OK);
            t.type=NBT_BYTE; t.name=nbt_span("Count"); t.value.byte=1; assert(nbt_writer_tag(&writer,&t)==NBT_OK);
        }
        t.type=NBT_INT; t.name=nbt_span("custom"); t.value.int_value=i ? 77 : 99; assert(nbt_writer_tag(&writer,&t)==NBT_OK);
        if(i) assert(nbt_writer_end(&writer)==NBT_OK);
        assert(nbt_writer_end(&writer)==NBT_OK);
    }
    assert(nbt_writer_end(&writer)==NBT_OK && nbt_writer_end(&writer)==NBT_OK && nbt_writer_end(&writer)==NBT_OK);
    assert(nbt_writer_finish(&writer,&size)==NBT_OK && world_entities_read(c,raw,size));
    for(e=c->saved_entities;e && !e->item_entity;e=e->next) { }
    assert(e && e->item.damage==14); e->item.count=6;
    assert(world_entities_rewrite(c,raw,size,&updated,&out_size));
    assert(nbt_read(updated,out_size,NULL,entity_canaries,&seen,NULL)==NBT_OK && seen==7);
    free(updated); world_entities_free(c); free(c);
}
static void environment_bed_tests(void)
{
    World w; Player p; PlayerInput input={0}; int i; uint64_t tick; int64_t night;
    assert(world_init(&w,42,1,16)==WORLD_OK);
    assert(world_get_chunk(&w,0,0) && world_get_chunk(&w,1,0));
    w.beta_world_time=6000; world_environment_refresh(&w); assert(w.sky_subtracted==0);
    w.beta_world_time=18000; world_environment_refresh(&w); assert(w.sky_subtracted==11);
    w.rain_time=1; w.thunder_time=1; world_environment_tick(&w);
    assert(w.raining && w.thundering && w.rain_strength>0);
    for(i=0;i<100;++i) world_environment_tick(&w);
    assert(w.rain_strength>.999f && w.thunder_strength>.999f && w.rain_time>0);
    assert(bed_place(&w,15,64,8,3));
    assert(world_peek_block(&w,16,64,8)==26 && world_peek_metadata(&w,16,64,8)==11);
    player_spawn(&p,&w,0); p.x=15.5f; p.z=8.5f; p.y=64;
    assert(!player_sleep(&p,&w,15,64,8));
    tick=w.tick; night=w.beta_world_time;
    for(i=0;i<99;++i) player_tick(&p,&w,&input,.05f);
    assert(p.sleeping && p.sleep_ticks==99);
    player_tick(&p,&w,&input,.05f);
    assert(!p.sleeping && p.has_bed_spawn && p.spawn_x==16 && w.beta_world_time%24000==0);
    assert(w.tick==tick+(uint64_t)(24000-night%24000));
    assert(!w.raining && !w.thundering && w.rain_time==0 && w.thunder_time==0);
    p.health=0; player_respawn(&p,&w); assert(p.health==20 && p.x>=14 && p.x<=18);
    for(i=0;i<100;++i) world_environment_tick(&w);
    assert(player_sleep(&p,&w,16,64,8)==1);
    assert(world_set_block(&w,16,64,8,0)); ticks(&w,1);
    assert(world_peek_block(&w,15,64,8)==0);
    p.health=19; w.difficulty=0; p.age=0;
    for(i=0;i<20;++i) player_tick(&p,&w,&input,.05f);
    assert(p.health==20);
    assert(world_close(&w)==WORLD_OK);
}
static void mechanisms_tests(void)
{
    World w;
    assert(world_init(&w,42,1,16)==WORLD_OK); assert(world_get_chunk(&w,0,0));
    assert(world_set_block(&w,7,64,8,1));
    assert(world_set_block(&w,8,64,8,77)); world_set_metadata(&w,8,64,8,1);
    assert(world_redstone_activate(&w,8,64,8)); ticks(&w,19);
    assert(world_peek_metadata(&w,8,64,8)&8);
    assert(world_redstone_activate(&w,8,64,8)); ticks(&w,1); /* Reclick does not restart the timer. */
    assert(!(world_peek_metadata(&w,8,64,8)&8));
    assert(world_set_block(&w,12,64,12,93));
    assert(world_set_block(&w,12,64,13,69)); world_set_metadata(&w,12,64,13,5);
    assert(world_redstone_activate(&w,12,64,13)); ticks(&w,1); assert(world_peek_block(&w,12,64,12)==93);
    ticks(&w,1); assert(world_peek_block(&w,12,64,12)==94);
    /* A side branch into the input of an idle repeater changes wire power
     * direction. This is Beta's remap, not the repeater's output direction. */
    assert(world_set_block(&w,4,64,4,55)); world_set_metadata(&w,4,64,4,15);
    assert(world_set_block(&w,5,64,4,55)); world_set_metadata(&w,5,64,4,14);
    assert(world_set_block(&w,4,64,5,93)); world_set_metadata(&w,4,64,5,2);
    /* This isolated test probes output geometry at an injected strength;
     * production dust now correctly clears unsupported power immediately. */
    chunk_set_metadata(world_get_chunk(&w,0,0),4,64,4,15);
    assert(world_redstone_signal(&w,4,64,4,3,64,4,1)==0);
    world_set_metadata(&w,4,64,5,0);
    chunk_set_metadata(world_get_chunk(&w,0,0),4,64,4,15);
    assert(world_redstone_signal(&w,4,64,4,3,64,4,1)==15);
    assert(world_redstone_signal(&w,12,64,12,12,64,11,1)==15);
    assert(world_redstone_signal(&w,12,64,12,11,64,12,1)==0);
    assert(world_redstone_activate(&w,12,64,13)); ticks(&w,2); assert(world_peek_block(&w,12,64,12)==93);
    assert(world_redstone_activate(&w,12,64,12)); assert(world_peek_metadata(&w,12,64,12)==4);
    assert(world_redstone_activate(&w,12,64,13)); ticks(&w,3); assert(world_peek_block(&w,12,64,12)==93);
    ticks(&w,1); assert(world_peek_block(&w,12,64,12)==94);
    assert(world_close(&w)==WORLD_OK);
}
static void mob_tests(void)
{
    World w; Player p; RenderEntity visible[16]={0}; InventorySlot sword={276,1,0}; SavedEntity *e;
    int difficulty;
    assert(world_init(&w,42,1,16)==WORLD_OK); player_spawn(&p,&w,0);
    for(difficulty=0;difficulty<4;++difficulty) {
        static const int expected[4]={20,18,15,13};
        p.health=20; p.hurt_ticks=0; w.difficulty=difficulty;
        player_mob_damage(&p,&w,5); assert(p.health==expected[difficulty]);
    }
    assert(beta_attack_damage(268)==4 && beta_attack_damage(283)==4 && beta_attack_damage(272)==6);
    assert(beta_attack_damage(267)==8 && beta_attack_damage(276)==10);
    assert(world_mob_spawn(&w,54,8.5f,64,6.5f));
    p.x=8.5f; p.y=64; p.z=9; p.yaw=p.pitch=0; p.health=20; p.hurt_ticks=0; w.difficulty=2;
    assert(world_mobs_attack(&w,&p,&sword,3));
    assert(sword.damage==1);
    e=world_peek_chunk(&w,0,0)->saved_entities; assert(e->mob.health==10);
    assert(world_mobs_visible(&w,visible,16)==1 && visible[0].type==54);
    assert(world_set_block(&w,8,65,7,1));
    assert(!world_mobs_attack(&w,&p,&sword,3) && sword.damage==1);
    assert(world_set_block(&w,8,65,7,0));
    p.z=7.5f;
    ticks(&w,1); world_mobs_tick(&w,&p); assert(p.health==15);
    w.difficulty=0; ticks(&w,1); world_mobs_tick(&w,&p);
    assert(world_mobs_visible(&w,visible,16)==0);
    assert(world_close(&w)==WORLD_OK);
    assert(world_init(&w,42,1,16)==WORLD_OK); player_spawn(&p,&w,0); p.x=8.5f; p.y=64; p.z=10; p.yaw=0; p.pitch=-.4f;
    assert(world_mob_spawn(&w,91,8.5f,64,7.5f));
    {
        InventorySlot shears={359,1,238}; WorldDropEvent drop;
        assert(world_mobs_interact(&w,&p,&shears,3) && shears.count==0);
        e=world_peek_chunk(&w,0,0)->saved_entities;
        while(e && e->mob.type!=91) e=e->next;
        assert(e && e->mob.sheared);
        assert(world_take_drop(&w,&drop) && drop.id==35 && drop.count>=2 && drop.count<=4);
    }
    assert(world_mob_spawn(&w,92,11.5f,64,7.5f)); p.x=11.5f;
    {
        InventorySlot bucket={325,1,0};
        assert(world_mobs_interact(&w,&p,&bucket,3) && bucket.id==335);
    }
    assert(world_mob_spawn(&w,93,13.5f,64,7.5f));
    e=world_peek_chunk(&w,0,0)->saved_entities; assert(e->mob.type==93); e->mob.egg_ticks=1;
    ticks(&w,1); world_mobs_tick(&w,&p);
    {
        WorldDropEvent drop; assert(world_take_drop(&w,&drop) && drop.id==344 && drop.count==1);
        assert(e->mob.egg_ticks>=6000 && e->mob.egg_ticks<12000);
    }
    assert(world_close(&w)==WORLD_OK);
    {
        char root[128]; Chunk *c; unsigned char raw[8192]; size_t size; NbtWriter writer; NbtTag tag={0};
        snprintf(root,sizeof(root),"build/mob-test-%d",(int)test_pid()); assert(test_mkdir(root)==0);
        assert(world_create(&w,root,"fixture","Mob fixture",42,1,0,0,16)==WORLD_OK); assert(world_get_chunk(&w,0,0));
        assert(world_mob_spawn(&w,90,8.5f,64,8.5f)); e=world_peek_chunk(&w,0,0)->saved_entities; e->mob.health=6; e->mob.fire=27;
        assert(world_mob_spawn(&w,91,10.5f,64,8.5f)); e=world_peek_chunk(&w,0,0)->saved_entities; e->mob.color=11; e->mob.sheared=1;
        assert(world_save(&w)==WORLD_OK && world_close(&w)==WORLD_OK);
        assert(world_open(&w,root,"fixture",16)==WORLD_OK); c=world_get_chunk(&w,0,0); assert(c);
        e=c->saved_entities; assert(e && e->mob.type==91 && e->mob.color==11 && e->mob.sheared);
        e=e->next; assert(e && e->mob.type==90 && e->mob.health==6 && e->mob.fire==27 && e->mob.x==8.5f);
        /* Entity scalar order is not fixed in NBT. Health before id must survive. */
        nbt_writer_init(&writer,raw,sizeof(raw),NULL); tag.type=NBT_COMPOUND; assert(nbt_writer_tag(&writer,&tag)==NBT_OK);
        tag.name=nbt_span("Level"); assert(nbt_writer_tag(&writer,&tag)==NBT_OK);
        tag.type=NBT_LIST; tag.name=nbt_span("Entities"); tag.list_type=NBT_COMPOUND; tag.count=1; assert(nbt_writer_tag(&writer,&tag)==NBT_OK);
        tag.type=NBT_COMPOUND; tag.name=nbt_span(""); assert(nbt_writer_tag(&writer,&tag)==NBT_OK);
        tag.type=NBT_SHORT; tag.name=nbt_span("Health"); tag.value.short_value=3; assert(nbt_writer_tag(&writer,&tag)==NBT_OK);
        tag.type=NBT_STRING; tag.name=nbt_span("id"); tag.value.bytes=nbt_span("Pig"); assert(nbt_writer_tag(&writer,&tag)==NBT_OK);
        tag.type=NBT_LIST; tag.name=nbt_span("Pos"); tag.list_type=NBT_DOUBLE; tag.count=3; assert(nbt_writer_tag(&writer,&tag)==NBT_OK);
        tag.type=NBT_DOUBLE; tag.name=nbt_span(""); tag.value.double_value=8.5;
        assert(nbt_writer_tag(&writer,&tag)==NBT_OK && nbt_writer_tag(&writer,&tag)==NBT_OK && nbt_writer_tag(&writer,&tag)==NBT_OK);
        assert(nbt_writer_end(&writer)==NBT_OK && nbt_writer_end(&writer)==NBT_OK && nbt_writer_end(&writer)==NBT_OK && nbt_writer_end(&writer)==NBT_OK && nbt_writer_end(&writer)==NBT_OK && nbt_writer_finish(&writer,&size)==NBT_OK);
        assert(world_entities_read(c,raw,size));
        e=c->saved_entities; while(e->next) e=e->next;
        assert(e->mob.health==3 && e->mob.type==90);
        assert(world_close(&w)==WORLD_OK);
        assert(world_storage_delete(root,"fixture")==WORLD_OK && test_rmdir(root)==0);
    }
}
static void mob_collision_tests(void)
{
    World w; Player p={0}; SavedEntity *e;
    assert(world_init(&w,42,1,16)==WORLD_OK && world_get_chunk(&w,0,0));
    p.x=p.z=8.5f; p.y=82; p.health=20; p.creative=1;
    assert(world_set_block(&w,8,80,8,1));
    assert(world_mob_spawn(&w,90,8.5f,82.2f,8.5f));
    e=world_peek_chunk(&w,0,0)->saved_entities;
    e->mob.walk_ticks=100; e->mob.vy=-100;
    ++w.tick; world_mobs_tick(&w,&p);
    /* A fast fall must hit a one-block platform, even when the next tick's
     * endpoint is empty air below it. */
    assert(e->mob.y>=80.999f && e->mob.y<81.002f && e->mob.on_ground && e->mob.health==10);
    e->mob.x=15.5f; e->mob.y=64; e->mob.vx=20; e->mob.vy=0; e->mob.on_ground=1;
    assert(!world_peek_chunk(&w,1,0));
    ++w.tick; world_mobs_tick(&w,&p);
    assert(e->mob.x>=15.5f && e->mob.x<15.72f && w.cache_count==1);
    assert(world_get_chunk(&w,1,0));
    e->mob.vy=0; e->mob.on_ground=1;
    ++w.tick; world_mobs_tick(&w,&p);
    assert(world_peek_chunk(&w,0,0)->saved_entities==NULL);
    assert(world_peek_chunk(&w,1,0)->saved_entities==e && e->mob.x>16);
    assert(world_close(&w)==WORLD_OK);
}
static void command_tests(void)
{
    World w; Player p={0}; char reply[401];
    assert(world_init(&w,123,1,8)==WORLD_OK); p.name="IlyaBOT";
    p.x=8.5f; p.y=64; p.z=-3.5f; p.vy=-12; p.fall_distance=50;
    assert(game_command(&w,&p,"/tp IlyaBOT ~2 70 -10",reply,sizeof(reply))==1);
    assert(p.x==10.5f && p.y==70 && p.z==-9.5f && !p.vy && !p.fall_distance);
    assert(game_command(&w,&p,"/tp nan 70 0",reply,sizeof(reply))==-1);
    assert(p.x==10.5f && p.y==70 && p.z==-9.5f);
    assert(game_command(&w,&p,"/tp Other 1 2 3",reply,sizeof(reply))==-1);
    assert(game_command(&w,&p,"/gamemode creative IlyaBOT",reply,sizeof(reply))==1);
    assert(p.creative && w.creative); p.flying=1;
    assert(game_command(&w,&p,"/gamemode 0",reply,sizeof(reply))==1);
    assert(!p.creative && !w.creative && !p.flying);
    assert(game_command(&w,&p,"/gamemode adventure",reply,sizeof(reply))==-1);
    assert(game_command(&w,&p,"/time set night",reply,sizeof(reply))==1);
    assert(w.beta_world_time==12500);
    assert(game_command(&w,&p,"/time add 100",reply,sizeof(reply))==1);
    assert(w.beta_world_time==12600);
    assert(game_command(&w,&p,"/timeset day",reply,sizeof(reply))==1 && !w.beta_world_time);
    assert(game_command(&w,&p,"/time set -1",reply,sizeof(reply))==-1 && !w.beta_world_time);
    w.beta_world_time=INT64_MAX;
    assert(game_command(&w,&p,"/time add 1",reply,sizeof(reply))==-1 && w.beta_world_time==INT64_MAX);
    assert(game_command(&w,&p,"/weather thunder 5",reply,sizeof(reply))==1);
    assert(w.raining && w.thundering && w.rain_time==100 && w.thunder_time==100);
    assert(game_command(&w,&p,"/weather clear",reply,sizeof(reply))==1 && !w.raining && !w.thundering);
    assert(game_command(&w,&p,"/seed",reply,sizeof(reply))==1 && strstr(reply,"123"));
    assert(game_command(&w,&p,"/help",reply,sizeof(reply))==1 && strstr(reply,"gamemode"));
    w.network_mode=1;
    assert(game_command(&w,&p,"/gamemode creative",reply,sizeof(reply))==0);
    assert(!p.creative && !w.creative && !reply[0]);
    assert(world_close(&w)==WORLD_OK);
}
int main(void)
{
    command_tests(); regressions(); mob_collision_tests(); mob_tests(); environment_bed_tests(); mechanisms_tests(); fluid_tests(); container_tests(); health_redstone_tests(); bucket_tests(); tick_save_tests(); entity_nbt_tests();
    puts("Scheduled Beta water/lava, containers, persistence, health and torch inversion passed"); return 0;
}
