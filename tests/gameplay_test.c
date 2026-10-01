#include "world/world.h"
#include "world/block_entity.h"
#include "world/fluid.h"
#include "world/entities.h"
#include "world/ticks.h"
#include "game/player.h"
#include "nbt/nbt.h"
#include <assert.h>
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
    assert(world_close(&w)==WORLD_OK);
    assert(world_open(&w,root,"fixture",16)==WORLD_OK);
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
    assert(p.health==20 && p.air==300); p.y=70;
    for (i=0;i<100;++i) player_tick(&p,&w,&in,.05f);
    assert(p.health==17); player_damage(&p,2); assert(p.health==15);
    player_damage(&p,2); assert(p.health==15); player_damage(&p,4); assert(p.health==13);
    {
        InventorySlot soup={282,1,0};
        assert(player_use_item(&p,&w,&soup) && p.health==20 && soup.id==281);
    }
    assert(world_set_block(&w,5,64,5,1));
    assert(world_set_block(&w,5,65,5,76)); assert(world_set_metadata(&w,5,65,5,5));
    assert(world_set_block(&w,4,64,5,69)); assert(world_set_metadata(&w,4,64,5,9));
    world_physics_notify(&w,4,64,5); ticks(&w,2);
    assert(world_peek_block(&w,5,65,5)==75);
    world_set_metadata(&w,4,64,5,1); world_physics_notify(&w,4,64,5); ticks(&w,2);
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
    tag.type=NBT_INT; tag.name=nbt_span("i"); tag.value.int_value=93; assert(nbt_writer_tag(&writer,&tag)==NBT_OK);
    tag.name=nbt_span("t"); tag.value.int_value=44; assert(nbt_writer_tag(&writer,&tag)==NBT_OK);
    tag.name=nbt_span("custom"); tag.value.int_value=5678; assert(nbt_writer_tag(&writer,&tag)==NBT_OK);
    assert(nbt_writer_end(&writer)==NBT_OK && nbt_writer_finish(&writer,&size)==NBT_OK);
    unknown=(SavedTick *)calloc(1,sizeof(*unknown)); assert(unknown);
    unknown->raw=(uint8_t *)malloc(size); assert(unknown->raw); memcpy(unknown->raw,raw,size);
    unknown->raw_size=size; unknown->id=93; unknown->delay=44; unknown->next=c->ticks; c->ticks=unknown;
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
int main(void)
{
    fluid_tests(); container_tests(); health_redstone_tests(); bucket_tests(); tick_save_tests(); entity_nbt_tests();
    puts("Scheduled Beta water/lava, containers, persistence, health and torch inversion passed"); return 0;
}
