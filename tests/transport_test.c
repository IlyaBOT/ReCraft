#include "world/world.h"
#include "world/entities.h"
#include "world/transport.h"
#include "world/rail.h"
#include "world/redstone.h"
#include "world/environment.h"
#include "world/block_entity.h"
#include "game/player.h"
#include "game/bed.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
static void init(World *w)
{
    int x,z; assert(world_init(w,42,1,32)==WORLD_OK);
    for(x=-1;x<=1;++x) for(z=-1;z<=1;++z) assert(world_get_chunk(w,x,z));
}
static void ticks(World *w,Player *p,InventorySlot *inventory,int n)
{ while(n--) { world_step_physics(w,4096); world_transport_tick(w,p,inventory); } }
static void lever_test(void)
{
    World w; Player p={0}; int d; const float pi=3.14159265f;
    const float x[4]={11.5f,5.5f,8.5f,8.5f},z[4]={8.5f,8.5f,11.5f,5.5f};
    const float yaw[4]={-pi/2,pi/2,0,pi};
    const int lx[4]={9,7,8,8},lz[4]={8,8,9,7};
    init(&w); p.health=20; p.creative=1;
    for(d=0;d<4;++d) {
        assert(world_set_block(&w,8,70,8,1)); p.x=x[d]; p.y=68.9f; p.z=z[d]; p.yaw=yaw[d]; p.pitch=0;
        assert(player_place_block(&p,&w,69));
        assert(world_peek_block(&w,lx[d],70,lz[d])==69 && world_peek_metadata(&w,lx[d],70,lz[d])==d+1);
        assert(world_redstone_activate(&w,lx[d],70,lz[d]));
        assert(world_peek_metadata(&w,lx[d],70,lz[d])==d+9);
        assert(world_redstone_signal(&w,lx[d],70,lz[d],8,70,8,1)==15);
        assert(world_set_block(&w,8,70,8,0)); assert(!world_peek_block(&w,lx[d],70,lz[d]));
    }
    for(d=0;d<8;++d) {
        assert(world_set_block(&w,8,70,8,1)); p.x=p.z=8.5f; p.y=73; p.pitch=-1.55f;
        assert(player_place_block(&p,&w,69)); assert(world_peek_metadata(&w,8,71,8)>=5 && world_peek_metadata(&w,8,71,8)<=6);
        world_set_block(&w,8,71,8,0);
    }
    assert(beta_block_terrain_tile((BetaBlockState){69,5},0)==96);
    assert(beta_render_source_tile(beta_render_tile(96))==96);
    assert(world_close(&w)==WORLD_OK);
}
static void jukebox_test(void)
{
    World w; InventorySlot held; int disc; init(&w);
    assert(world_set_block(&w,8,64,8,84));
    for(disc=2256;disc<=2257;++disc) {
        held=(InventorySlot){disc,1,0}; assert(jukebox_use(&w,8,64,8,&held));
        assert(held.count==0 && block_entity_get(&w,8,64,8,0)->record==disc && world_peek_metadata(&w,8,64,8)==1);
        assert(jukebox_use(&w,8,64,8,&held));
        assert(block_entity_get(&w,8,64,8,0)->record==0 && !world_peek_metadata(&w,8,64,8));
    }
    held=(InventorySlot){2257,1,0}; assert(jukebox_use(&w,8,64,8,&held));
    assert(world_set_block(&w,8,64,8,0)); assert(!block_entity_get(&w,8,64,8,0));
    assert(world_close(&w)==WORLD_OK);
}
static void arrow_test(void)
{
    World w; Player p={0}; InventorySlot inventory[36]={{0}}; SavedEntity *arrow; int y;
    init(&w); p.x=p.z=8.5f; p.y=64; p.health=20;
    assert(!world_bow_use(&w,&p,inventory)); inventory[0]=(InventorySlot){262,2,0};
    assert(world_bow_use(&w,&p,inventory) && inventory[0].count==1);
    arrow=world_peek_chunk(&w,0,0)->saved_entities; assert(arrow->transport.kind==1);
    assert(arrow->mob.vz< -29 && arrow->mob.vz> -31);
    for(y=64;y<68;++y) world_set_block(&w,8,y,4,1);
    ticks(&w,&p,inventory,5); assert(arrow->transport.in_ground && arrow->transport.z_tile==4 && arrow->transport.shake>0);
    assert(arrow->mob.z>5 && arrow->mob.z<5.1f);
    p.x=arrow->mob.x; p.y=arrow->mob.y-1; p.z=arrow->mob.z;
    ticks(&w,&p,inventory,8); assert(inventory[0].count==2 && !world_peek_chunk(&w,0,0)->saved_entities);
    p.x=8.5f; p.y=64; p.z=12.5f;
    assert(world_mob_spawn(&w,90,8.5f,64,9.5f));
    assert(world_bow_use(&w,&p,inventory)); ticks(&w,&p,inventory,3);
    assert(world_peek_chunk(&w,0,0)->saved_entities->mob.type==90 && world_peek_chunk(&w,0,0)->saved_entities->mob.health==6);
    assert(world_close(&w)==WORLD_OK);
}
static void rail_test(void)
{
    World w; Player p={0}; InventorySlot inv[36]={{0}}; float x,y,z; int s,k; SavedEntity *cart;
    init(&w); p.x=p.z=14; p.y=64; p.health=20;
    for(k=2;k<=12;++k) assert(world_set_block(&w,k,64,8,66));
    ticks(&w,&p,inv,3);
    assert(world_peek_metadata(&w,8,64,8)==1);
    assert(world_set_block(&w,12,64,9,66)); ticks(&w,&p,inv,3);
    assert(world_peek_metadata(&w,12,64,8)==7);
    assert(rail_path(&w,12.75f,64.5f,8.5f,&x,&y,&z,&s) && s==7 && fabsf(x-z-3.5f)<.0001f);
    assert(world_minecart_spawn(&w,3.5f,64,8.5f,0)); cart=world_peek_chunk(&w,0,0)->saved_entities;
    cart->mob.vx=8; ticks(&w,&p,inv,10); assert(cart->mob.x>6 && fabsf(cart->mob.z-8.5f)<.001f);
    assert(world_set_state(&w,8,64,8,(BetaBlockState){27,1})); ticks(&w,&p,inv,4);
    cart->mob.x=8.2f; cart->mob.z=8.5f; cart->mob.vx=4; cart->mob.vz=0;
    ticks(&w,&p,inv,1); assert(cart->mob.vx<2.1f);
    assert(world_set_state(&w,8,64,7,(BetaBlockState){69,13})); ticks(&w,&p,inv,3);
    assert(world_peek_metadata(&w,8,64,8)&8);
    cart->mob.x=8.2f; cart->mob.vx=4; ticks(&w,&p,inv,1); assert(cart->mob.vx>4);
    assert(world_set_state(&w,9,64,8,(BetaBlockState){28,1})); ticks(&w,&p,inv,3);
    cart->mob.x=9.5f; cart->mob.vx=0; ticks(&w,&p,inv,1);
    assert(world_peek_metadata(&w,9,64,8)&8 && world_redstone_signal(&w,9,64,8,9,64,7,1)==15);
    cart->mob.x=5; ticks(&w,&p,inv,20); assert(!(world_peek_metadata(&w,9,64,8)&8));
    assert(world_set_block(&w,12,64,9,0)); ticks(&w,&p,inv,3); assert(world_peek_metadata(&w,12,64,8)==1);
    assert(world_set_block(&w,5,64,5,1)); assert(world_set_block(&w,4,64,5,66));
    ticks(&w,&p,inv,3);
    assert(world_set_block(&w,5,65,5,66)); ticks(&w,&p,inv,3); assert(world_peek_metadata(&w,4,64,5)==2);
    assert(rail_path(&w,4.75f,64.5f,5.5f,&x,&y,&z,&s) && s==2 && fabsf(y-65.25f)<.001f);
    cart->mob.x=4.2f; cart->mob.y=64.7f; cart->mob.z=5.5f; cart->mob.vx=6; cart->mob.vz=0;
    ticks(&w,&p,inv,5); assert(cart->mob.x>5 && cart->mob.y>65.49f);
    cart->mob.x=12.1f; cart->mob.y=64.5f; cart->mob.z=8.6f; cart->mob.vx=4; cart->mob.vz=0;
    assert(world_set_block(&w,12,64,9,66)); assert(world_set_block(&w,12,64,10,66)); ticks(&w,&p,inv,3);
    ticks(&w,&p,inv,8); assert(cart->mob.z>9 && fabsf(cart->mob.x-12.5f)<.001f);
    assert(world_close(&w)==WORLD_OK);
}
static int saved_pitch(void *context,NbtEvent event,const NbtTag *tag,unsigned depth)
{
    int *index=(int *)context;
    if(event==NBT_BEGIN && depth==1 && tag->name.size==8 && !memcmp(tag->name.data,"Rotation",8)) *index=0;
    if(event==NBT_VALUE && depth==2 && tag->type==NBT_FLOAT && *index>=0) {
        if(++*index==2) { assert(tag->value.float_value==40); *index=-1; }
    }
    return 1;
}
static void entity_roundtrip(void)
{
    World w; Player p={0}; InventorySlot inv[36]={{262,2,0}}; Chunk copy={0}; NbtWriter writer;
    NbtTag tag={0}; uint8_t bytes[8192]; size_t size; SavedEntity *e; int type,index=-1;
    init(&w); p.x=p.z=8.5f; p.y=64; p.health=20;
    assert(world_set_block(&w,8,64,8,66));
    for(type=0;type<3;++type) {
        assert(world_minecart_spawn(&w,8.5f,64,8.5f,type)); e=world_peek_chunk(&w,0,0)->saved_entities;
        e->transport.cargo[26]=(InventorySlot){278,1,37}; e->transport.push_x=-1.5f; e->transport.push_z=3.25f; e->transport.fuel=500;
    }
    assert(world_bow_use(&w,&p,inv)); e=world_peek_chunk(&w,0,0)->saved_entities;
    e->transport.in_ground=1; e->transport.x_tile=7; e->transport.y_tile=64; e->transport.z_tile=8; e->transport.in_tile=1;
    e->mob.yaw=30; e->mob.pitch=40;
    nbt_writer_init(&writer,bytes,sizeof(bytes),NULL); tag.type=NBT_COMPOUND;
    assert(nbt_writer_tag(&writer,&tag)==NBT_OK); tag.name=nbt_span("Level"); assert(nbt_writer_tag(&writer,&tag)==NBT_OK);
    assert(world_entities_write_list(world_peek_chunk(&w,0,0),&writer));
    assert(nbt_writer_end(&writer)==NBT_OK && nbt_writer_end(&writer)==NBT_OK && nbt_writer_finish(&writer,&size)==NBT_OK);
    assert(world_entities_read(&copy,bytes,size)); e=copy.saved_entities;
    assert(e->transport.kind==1 && e->transport.in_ground && e->transport.player && e->transport.x_tile==7);
    assert(fabsf(e->mob.yaw-30)<.001f && fabsf(e->mob.pitch-40)<.001f && e->mob.vz< -29);
    assert(nbt_read(e->raw,e->raw_size,NULL,saved_pitch,&index,NULL)==NBT_OK && index== -1);
    e=e->next; assert(e->transport.kind==2 && e->transport.type==2 && e->transport.fuel==500 && e->transport.push_x== -1.5f && e->transport.push_z==3.25f);
    e=e->next; assert(e->transport.kind==2 && e->transport.type==1 && e->transport.cargo[26].id==278 && e->transport.cargo[26].damage==37);
    e=e->next; assert(e->transport.kind==2 && e->transport.type==0);
    world_entities_free(&copy); assert(world_close(&w)==WORLD_OK);
}
static void rail_forms_test(void)
{
    int id,shape,end;
    for(id=0;id<3;++id) for(shape=0;shape<(id==0 ? 10 : 6);++shape) {
        World w; Player p={0}; InventorySlot inv[36]={{0}};
        unsigned block=id==0 ? 66 : id==1 ? 27 : 28;
        init(&w); p.x=p.z=14; p.y=64; p.health=20;
        world_set_block(&w,8,64,8,(uint8_t)block);
        for(end=0;end<2;++end) {
            int x=8+rail_ends[shape][end][0],z=8+rail_ends[shape][end][2];
            int y=64+(shape>=2 && shape<=5 ? rail_ends[shape][end][1]+1 : 0);
            if(y==65) world_set_block(&w,x,64,z,1);
            world_set_block(&w,x,y,z,(uint8_t)block);
        }
        ticks(&w,&p,inv,5);
        assert((world_peek_metadata(&w,8,64,8)&(id==0 ? 15 : 7))==(unsigned)shape);
        world_set_block(&w,8,63,8,0); ticks(&w,&p,inv,2); assert(!world_peek_block(&w,8,64,8));
        assert(world_close(&w)==WORLD_OK);
    }
    { World w; Player p={0}; InventorySlot inv[36]={{0}}; int k;
      init(&w); p.x=p.z=14; p.y=64; p.health=20;
      for(k=2;k<=12;++k) world_set_block(&w,k,64,8,27);
      world_set_state(&w,2,64,7,(BetaBlockState){69,13}); ticks(&w,&p,inv,6);
      for(k=2;k<=10;++k) assert(world_peek_metadata(&w,k,64,8)&8);
      assert(!(world_peek_metadata(&w,11,64,8)&8));
      world_redstone_activate(&w,2,64,7); ticks(&w,&p,inv,6);
      for(k=2;k<=12;++k) {
          if(world_peek_metadata(&w,k,64,8)&8) fprintf(stderr,"powered rail x=%d stays on; lever=%u queue=%u tick=%llu\n",k,world_peek_metadata(&w,2,64,7),w.physics_count,(unsigned long long)w.tick);
          assert(!(world_peek_metadata(&w,k,64,8)&8));
      }
      assert(world_close(&w)==WORLD_OK); }
}
static void cart_use_test(void)
{
    World w; Player p={0}; InventorySlot inv[36]={{0}},held={263,2,0}; SavedEntity *cart; int type;
    init(&w); p.x=8.5f; p.y=64; p.z=10.5f; p.pitch=-.5f; p.health=20;
    world_set_state(&w,8,64,8,(BetaBlockState){66,0});
    for(type=0;type<3;++type) {
        assert(world_minecart_spawn(&w,8.5f,64,8.5f,type)); cart=world_peek_chunk(&w,0,0)->saved_entities;
        assert(world_transport_interact(&w,&p,&held,0));
        if(type==0) {
            assert(p.riding && cart->transport.ridden); ticks(&w,&p,inv,1);
            assert(fabsf(p.x-cart->mob.x)<.001f && fabsf(p.y-cart->mob.y+.3f)<.001f);
            world_minecart_dismount(&w,&p); assert(!p.riding && !cart->transport.ridden);
        } else if(type==1) { assert(p.cart_inventory==cart->mob.runtime_id); cart->transport.cargo[26]=(InventorySlot){264,3,0}; }
        else { assert(held.count==1 && cart->transport.fuel==1200); ticks(&w,&p,inv,2); assert(cart->mob.vz<0); }
        p.x=8.5f; p.y=64; p.z=10.5f; p.creative=1;
        assert(world_transport_interact(&w,&p,&held,1) && cart->transport.dead);
        ticks(&w,&p,inv,1); p.creative=0; assert(!world_peek_chunk(&w,0,0)->saved_entities || world_peek_chunk(&w,0,0)->saved_entities->item_entity);
    }
    world_set_block(&w,8,64,8,66); assert(world_minecart_spawn(&w,8.5f,64,8.5f,0));
    cart=world_peek_chunk(&w,0,0)->saved_entities;
    world_set_state(&w,8,64,8,(BetaBlockState){44,0}); cart->mob.y=70; cart->mob.vy=-200;
    ticks(&w,&p,inv,1); assert(cart->mob.y>=64.849f && cart->mob.y<64.852f && cart->mob.vy==0);
    assert(world_close(&w)==WORLD_OK);
}
static void arrow_lifetime_test(void)
{
    World w; Player p={0}; InventorySlot inv[36]={{262,2,0}}; SavedEntity *arrow; int i;
    init(&w); p.x=p.z=8.5f; p.y=64; p.health=20;
    world_set_block(&w,8,65,4,1); assert(world_bow_use(&w,&p,inv)); arrow=world_peek_chunk(&w,0,0)->saved_entities;
    ticks(&w,&p,inv,5); assert(arrow->transport.in_ground);
    world_set_block(&w,8,65,4,0); ticks(&w,&p,inv,1); assert(!arrow->transport.in_ground);
    arrow->transport.in_ground=1; arrow->transport.x_tile=8; arrow->transport.y_tile=65; arrow->transport.z_tile=4;
    world_set_block(&w,8,65,4,1); arrow->transport.ground_ticks=1199; ticks(&w,&p,inv,1);
    assert(!world_peek_chunk(&w,0,0)->saved_entities);
    p.x=15.4f; p.y=70; p.z=8.5f; p.yaw=1.57079633f;
    assert(world_bow_use(&w,&p,inv)); ticks(&w,&p,inv,1);
    arrow=world_peek_chunk(&w,1,0)->saved_entities; assert(arrow && arrow->transport.kind==1 && arrow->mob.x>16);
    for(i=0;i<36;++i) inv[i]=(InventorySlot){1,64,0};
    arrow->mob.x=17.5f; arrow->mob.y=70; arrow->mob.z=8.5f;
    arrow->transport.in_ground=1; arrow->transport.shake=0; arrow->transport.x_tile=17;
    arrow->transport.y_tile=70; arrow->transport.z_tile=8; arrow->transport.in_tile=1; arrow->transport.in_data=0;
    world_set_block(&w,17,70,8,1);
    p.x=17.5f; p.y=69; p.z=8.5f; ticks(&w,&p,inv,1); assert(!arrow->transport.dead);
    inventory_clear_slot(&inv[35]); ticks(&w,&p,inv,1); assert(inv[35].id==262 && !world_peek_chunk(&w,1,0)->saved_entities);
    assert(world_close(&w)==WORLD_OK);
}
static void bed_test(void)
{
    World w; Player p={0}; int d; float x,y,z,yaw,pitch; const int dx[4]={0,-1,0,1},dz[4]={1,0,-1,0};
    init(&w); w.beta_world_time=18000; world_environment_refresh(&w); p.health=20;
    for(d=0;d<4;++d) {
        int hx=8+dx[d],hz=8+dz[d]; p.x=8.5f; p.y=64; p.z=8.5f;
        assert(bed_place(&w,8,64,8,(unsigned)d)); assert(!player_sleep(&p,&w,8,64,8));
        player_eye(&p,&w,&x,&y,&z,&yaw,&pitch);
        assert(fabsf(x-hx-.5f-dx[d]*.4f)<.001f && fabsf(z-hz-.5f-dz[d]*.4f)<.001f);
        assert(y>65 && y<65.2f && fabsf(yaw-d*1.57079633f)<.001f);
        player_wake(&p,&w,0); assert(!p.sleeping);
        world_set_block(&w,8,64,8,0); world_set_block(&w,hx,64,hz,0);
    }
    assert(world_close(&w)==WORLD_OK);
}
int main(void)
{
    lever_test(); jukebox_test(); arrow_test(); arrow_lifetime_test(); rail_test(); rail_forms_test();
    cart_use_test(); entity_roundtrip(); bed_test();
    puts("Beta lever, jukebox, arrow, rail topology, minecart NBT and sleeping eyes passed"); return 0;
}
