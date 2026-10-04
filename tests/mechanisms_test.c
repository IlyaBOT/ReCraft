#include "world/world.h"
#include "world/block_entity.h"
#include "world/mechanisms.h"
#include "world/piston.h"
#include "world/entities.h"
#include "world/redstone.h"
#include "game/player.h"
#include "game/inventory.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>
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
static void ticks(World *w,int n)
{ while(n--) { world_step_physics(w,4096); block_entities_tick(w); } }
static void lever(World *w,int x,int y,int z)
{ world_set_block(w,x,y-1,z,1); world_set_state(w,x,y,z,(BetaBlockState){69,13}); }
static void init(World *w)
{
    int x,z;
    assert(world_init(w,42,1,16)==WORLD_OK);
    for(x=-1;x<=1;++x) for(z=-1;z<=1;++z) assert(world_get_chunk(w,x,z));
}
static void wire_test(void)
{
    World w; int x;
    init(&w);
    for(x=0;x<=20;++x) world_set_state(&w,x,64,4,(BetaBlockState){55,0});
    world_set_state(&w,-1,64,4,(BetaBlockState){76,5});
    /* All strengths settle in the placement callback, not 20 separate ticks. */
    for(x=0;x<=20;++x) assert(world_peek_metadata(&w,x,64,4)==(x<15 ? 15-x : 0));
    world_set_block(&w,-1,64,4,0);
    for(x=0;x<=20;++x) assert(world_peek_metadata(&w,x,64,4)==0);
    assert(world_close(&w)==WORLD_OK);
}
static void clock_init(World *w)
{
    static const int x[4]={5,6,5,4},z[4]={4,5,6,5},dir[4]={1,2,3,0}; int i;
    init(w);
    for(i=0;i<4;++i) world_set_state(w,x[i],64,z[i],(BetaBlockState){93,(uint8_t)(dir[i]|4)});
    world_set_block(w,4,64,4,55); world_set_block(w,6,64,4,55);
    world_set_block(w,6,64,6,55); world_set_block(w,4,64,6,55);
}
static void clock_test(void)
{
    World a,b; int t,changes=0,previous=-1;
    clock_init(&a); clock_init(&b);
    world_set_block(&b,7,64,4,55); /* Output branch from the north-east corner. */
    world_set_state(&a,3,64,4,(BetaBlockState){69,13});
    world_set_state(&b,3,64,4,(BetaBlockState){69,13});
    ticks(&a,1); ticks(&b,1);
    world_set_block(&a,3,64,4,0); world_set_block(&b,3,64,4,0);
    for(t=0;t<160;++t) {
        int state;
        ticks(&a,1); ticks(&b,1); state=world_peek_block(&a,5,64,4);
        assert(state==world_peek_block(&b,5,64,4));
        assert(world_peek_block(&a,6,64,5)==world_peek_block(&b,6,64,5));
        assert(world_peek_block(&a,5,64,6)==world_peek_block(&b,5,64,6));
        assert(world_peek_block(&a,4,64,5)==world_peek_block(&b,4,64,5));
        if(state!=previous) ++changes;
        previous=state;
    }
    assert(changes>=16);
    assert(world_close(&a)==WORLD_OK && world_close(&b)==WORLD_OK);
}
static void plates_buttons_test(void)
{
    World w; Player p={0}; int id;
    init(&w); p.health=20; p.x=p.z=8.5f; p.y=64;
    for(id=70;id<=72;id+=2) {
        world_set_block(&w,8,64,8,(uint8_t)id); world_mechanisms_tick(&w,&p);
        assert(world_peek_metadata(&w,8,64,8)==1);
        assert(world_redstone_signal(&w,8,63,8,9,63,8,1)==15);
        p.x=12; ticks(&w,19); assert(world_peek_metadata(&w,8,64,8)==1);
        ticks(&w,1); assert(world_peek_metadata(&w,8,64,8)==0); p.x=8.5f;
        world_set_block(&w,8,64,8,0);
    }
    p.x=12;
    assert(world_item_spawn_at(&w,8.5f,64.125f,8.5f,(InventorySlot){1,1,0}));
    world_set_block(&w,8,64,8,70); world_mechanisms_tick(&w,&p); assert(!world_peek_metadata(&w,8,64,8));
    world_set_block(&w,8,64,8,72); world_mechanisms_tick(&w,&p); assert(world_peek_metadata(&w,8,64,8)==1);
    world_set_block(&w,8,63,8,0); assert(!world_peek_block(&w,8,64,8));
    world_set_block(&w,8,63,8,1); world_set_state(&w,9,63,8,(BetaBlockState){77,1}); assert(world_redstone_activate(&w,9,63,8));
    ticks(&w,19); assert(world_peek_metadata(&w,9,63,8)&8);
    ticks(&w,1); assert(!(world_peek_metadata(&w,9,63,8)&8));
    assert(world_close(&w)==WORLD_OK);
}
static void dispenser_test(void)
{
    World w; BlockEntity *d; int items[4]={262,332,344,326},i;
    init(&w); world_set_state(&w,8,64,8,(BetaBlockState){23,3}); d=block_entity_get(&w,8,64,8,1); assert(d && d->kind==BLOCK_ENTITY_DISPENSER);
    for(i=0;i<4;++i) {
        SavedEntity *e;
        d->slots[4]=(InventorySlot){items[i],2,0};
        world_set_state(&w,7,64,8,(BetaBlockState){69,13}); ticks(&w,3); assert(d->slots[4].count==2);
        ticks(&w,1); assert(d->slots[4].count==1);
        e=world_peek_chunk(&w,0,0)->saved_entities; assert(e);
        if(i<3) { assert(e->transport.kind==(i==0 ? 1 : i==1 ? 5 : 6)); assert(e->mob.vz>15); }
        else { assert(e->item_entity && e->item.id==326 && e->item.vz>0); assert(world_peek_block(&w,8,64,9)==0); }
        /* Constant power does not create an automatic rapid-fire timer. */
        ticks(&w,10); assert(d->slots[4].count==1);
        world_set_block(&w,7,64,8,0); inventory_clear_slot(&d->slots[4]);
    }
    world_set_block(&w,8,64,8,0); assert(!block_entity_get(&w,8,64,8,0)); assert(world_close(&w)==WORLD_OK);
    for(i=2;i<=5;++i) {
        SavedEntity *e;
        init(&w);world_set_state(&w,8,64,8,(BetaBlockState){23,(uint8_t)i});d=block_entity_get(&w,8,64,8,1);d->slots[0]=(InventorySlot){262,3,0};
        world_set_block(&w,7,64,8,55);world_set_state(&w,6,64,8,(BetaBlockState){76,5});
        assert(world_dispenser_powered(&w,8,64,8));ticks(&w,4);assert(d->slots[0].count==2);
        e=world_peek_chunk(&w,0,0)->saved_entities;assert(e && e->transport.kind==1);
        if(i==2)assert(e->mob.vz< -15);else if(i==3)assert(e->mob.vz>15);else if(i==4)assert(e->mob.vx< -15);else assert(e->mob.vx>15);
        assert(world_close(&w)==WORLD_OK);
    }
}
static void piston_test(void)
{
    World w; int id,n;
    for(id=29;id<=33;id+=4) {
        init(&w); world_set_state(&w,8,65,8,(BetaBlockState){(uint8_t)id,5});
        world_set_state(&w,9,65,8,(BetaBlockState){35,14}); lever(&w,8,65,7);
        ticks(&w,1); assert(world_peek_block(&w,10,65,8)==36); assert(world_peek_metadata(&w,8,65,8)==13);
        ticks(&w,3); assert(world_peek_block(&w,9,65,8)==34 && world_peek_block(&w,10,65,8)==35 && world_peek_metadata(&w,10,65,8)==14);
        world_set_block(&w,8,65,7,0); ticks(&w,4);
        assert(world_peek_block(&w,8,65,8)==id && world_peek_metadata(&w,8,65,8)==5);
        assert(world_peek_block(&w,id==29 ? 9 : 10,65,8)==35 && world_peek_metadata(&w,id==29 ? 9 : 10,65,8)==14);
        assert(world_close(&w)==WORLD_OK);
    }
    init(&w); world_set_state(&w,1,65,8,(BetaBlockState){33,5});
    for(n=1;n<=13;++n) world_set_block(&w,1+n,65,8,1);
    lever(&w,1,65,7); ticks(&w,4); assert(world_peek_metadata(&w,1,65,8)==5);
    world_set_block(&w,14,65,8,0); world_piston_changed(&w,1,65,8); ticks(&w,4);
    assert(world_peek_metadata(&w,1,65,8)==13 && world_peek_block(&w,14,65,8)==1); assert(world_close(&w)==WORLD_OK);
    for(n=0;n<6;++n) {
        static const int dx[6]={0,0,0,0,-1,1},dy[6]={-1,1,0,0,0,0},dz[6]={0,0,-1,1,0,0};
        int powerface=n==4 ? 5 : 4;
        init(&w); world_set_state(&w,8,68,8,(BetaBlockState){33,(uint8_t)n});
        world_set_state(&w,8+dx[n],68+dy[n],8+dz[n],(BetaBlockState){35,3});
        lever(&w,8+dx[powerface],68,8);
        world_piston_changed(&w,8,68,8); ticks(&w,4);
        assert(world_peek_block(&w,8+dx[n]*2,68+dy[n]*2,8+dz[n]*2)==35);
        assert(world_close(&w)==WORLD_OK);
    }
}
static void persistence_test(void)
{
    World w; BlockEntity *e; char root[128];
    snprintf(root,sizeof(root),"build/mechanisms-test-%d",(int)test_pid()); assert(test_mkdir(root)==0);
    assert(world_create(&w,root,"fixture","Mechanisms",42,1,0,0,16)==WORLD_OK);
    world_set_state(&w,8,64,8,(BetaBlockState){23,5}); e=block_entity_get(&w,8,64,8,1); e->slots[8]=(InventorySlot){262,13,0}; block_entity_changed(&w,e);
    world_set_block(&w,12,64,8,25);e=block_entity_get(&w,12,64,8,1);e->note=24;block_entity_changed(&w,e);
    world_set_state(&w,4,65,4,(BetaBlockState){29,5}); world_set_state(&w,5,65,4,(BetaBlockState){35,7}); lever(&w,4,65,3); ticks(&w,1);
    assert(world_peek_block(&w,6,65,4)==36 && block_entity_get(&w,6,65,4,0)->piston_progress==.5f);
    assert(world_save(&w)==WORLD_OK && world_close(&w)==WORLD_OK);
    assert(world_open(&w,root,"fixture",16)==WORLD_OK); assert(world_get_chunk(&w,0,0));
    e=block_entity_get(&w,8,64,8,0); assert(e && e->kind==BLOCK_ENTITY_DISPENSER && e->slots[8].id==262 && e->slots[8].count==13);
    e=block_entity_get(&w,12,64,8,0);assert(e && e->kind==BLOCK_ENTITY_NOTE && e->note==24);
    e=block_entity_get(&w,6,65,4,0); assert(e && e->kind==BLOCK_ENTITY_PISTON && e->piston_id==35 && e->piston_data==7 && e->piston_facing==5 && e->piston_extending);
    ticks(&w,4); assert(world_peek_block(&w,6,65,4)==35 && world_peek_metadata(&w,6,65,4)==7);
    assert(world_close(&w)==WORLD_OK); assert(world_storage_delete(root,"fixture")==WORLD_OK); assert(test_rmdir(root)==0);
}
static void note_test(void)
{
    static const int base[]={3,1,12,20,5,41,89};
    static const char *sound[]={"note.harp","note.bd","note.snare","note.hat","note.bassattack","note.harp","note.bd"};
    World w;BlockEntity *e;int i;
    init(&w);world_set_block(&w,8,64,8,25);e=block_entity_get(&w,8,64,8,1);assert(e && e->kind==BLOCK_ENTITY_NOTE);
    for(i=0;i<7;++i) {
        world_set_block(&w,8,63,8,(uint8_t)base[i]);w.sound_count=0;
        assert(note_block_use(&w,8,64,8,0));assert(w.sound_count==1 && !strcmp(w.sounds[0].key,sound[i]));
        assert(fabsf(w.sounds[0].pitch-.5f)<.001f && w.sounds[0].volume==3);
    }
    for(i=0;i<24;++i)note_block_use(&w,8,64,8,1);
    assert(e->note==24);w.sound_count=0;note_block_use(&w,8,64,8,0);assert(fabsf(w.sounds[0].pitch-2)<.001f);
    note_block_use(&w,8,64,8,1);assert(!e->note);
    world_set_block(&w,8,65,8,1);w.sound_count=0;note_block_use(&w,8,64,8,1);assert(e->note==1 && !w.sound_count);
    world_set_block(&w,8,65,8,0);w.sound_count=0;
    world_set_state(&w,7,64,8,(BetaBlockState){69,13});assert(e->note_powered);
    assert(w.sound_count==1);note_block_changed(&w,8,64,8);assert(w.sound_count==1);
    world_set_block(&w,7,64,8,0);assert(!e->note_powered);
    world_set_state(&w,7,64,8,(BetaBlockState){69,13});assert(w.sound_count==2);
    assert(world_close(&w)==WORLD_OK);
}
int main(void)
{
    wire_test(); clock_test(); plates_buttons_test(); dispenser_test(); piston_test(); note_test(); persistence_test();
    puts("Beta redstone/mechanisms regression tests passed"); return 0;
}
