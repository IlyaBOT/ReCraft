#include "fire.h"
#include "environment.h"
#include "explosion.h"
static const int dx[6]={1,-1,0,0,0,0},dy[6]={0,0,-1,1,0,0},dz[6]={0,0,0,0,-1,1};
int fire_encouragement(unsigned id)
{
    switch(id) { case 5:case 85:case 53:case 17:return 5;case 18:case 47:case 35:return 30;
    case 46:return 15;case 31:return 60;default:return 0; }
}
int fire_burn_rate(unsigned id)
{
    switch(id) { case 5:case 85:case 53:case 47:return 20;case 17:return 5;case 18:case 35:return 60;
    case 46:case 31:return 100;default:return 0; }
}
static int encouragement(const World *w,int x,int y,int z)
{
    int n,max=0;
    for(n=0;n<6;++n) { int v=fire_encouragement(world_peek_block(w,x+dx[n],y+dy[n],z+dz[n])); if(v>max) max=v; }
    return max;
}
int world_fire_can_stay(const World *w,int x,int y,int z)
{ return world_block_def(world_peek_block(w,x,y-1,z))->opaque || encouragement(w,x,y,z)>0; }
static int rain(World *w,int x,int y,int z)
{
    unsigned kind=0;
    return w->raining && world_precipitation_height(w,x,z,&kind)<=y && kind==1;
}
int world_ignite(World *w,int x,int y,int z)
{
    Chunk *c=world_peek_chunk(w,x>>4,z>>4);
    if(w->network_mode || (unsigned)y>=128 || !c || (w->beta_format && !c->beta_raw) ||
       world_peek_block(w,x,y,z) || !world_fire_can_stay(w,x,y,z)) return 0;
    return world_set_block(w,x,y,z,51);
}
int world_extinguish_fire(World *w,int x,int y,int z)
{
    if(w->network_mode || world_peek_block(w,x,y,z)!=51 || !world_set_block(w,x,y,z,0))return 0;
    world_sound(w,"fire.ignite",x+.5f,y+.5f,z+.5f,.5f,2.6f+((int)world_random(w,100)-(int)world_random(w,100))*.008f);
    return 1;
}
void world_lava_ignite_tick(World *w,int x,int y,int z)
{
    /* Beta stationary lava takes 0..2 upward random-walk steps. It does not
     * use the later three horizontal ignition attempts when steps == 0. */
    unsigned n,steps=world_random(w,3);
    for(n=0;n<steps;++n) {
        unsigned id;Chunk *c;
        x+=(int)world_random(w,3)-1;z+=(int)world_random(w,3)-1;++y;
        c=world_peek_chunk(w,x>>4,z>>4);
        if((unsigned)y>=128||!c||(w->beta_format&&!c->beta_raw))return;
        id=world_peek_block(w,x,y,z);
        if(!id) {
            int side;
            for(side=0;side<6;++side)
                if(beta_material_burns(world_peek_block(w,x+dx[side],y+dy[side],z+dz[side]))) {
                    /* Lava checks Material.canBurn, not the fire-spread table.
                     * It can ignite beside wooden doors/signs, even though
                     * ordinary fire cannot consume those Beta blocks. */
                    world_set_block(w,x,y,z,51);return;
                }
        } else if(beta_material_solid(id))return;
    }
}
static void burn(World *w,int x,int y,int z,unsigned bound,unsigned age)
{
    unsigned id=world_peek_block(w,x,y,z),rate=fire_burn_rate(id);
    if(!rate || world_random(w,bound)>=rate) return;
    if(world_random(w,age+10)<5 && !rain(w,x,y,z)) {
        unsigned next=age+world_random(w,5)/4;
        world_set_state(w,x,y,z,(BetaBlockState){51,(uint8_t)(next>15?15:next)});
    } else world_set_block(w,x,y,z,0);
    if(id==46) world_tnt_prime(w,x+.5f,y+.5f,z+.5f,80);
}
void world_fire_tick(World *w,int x,int y,int z)
{
    unsigned age=world_peek_metadata(w,x,y,z); int eternal,n,a,b,c;
    if(w->network_mode || world_peek_block(w,x,y,z)!=51) return;
    eternal=world_peek_block(w,x,y-1,z)==87;
    if(!world_fire_can_stay(w,x,y,z) || (!eternal &&
        (rain(w,x,y,z)||rain(w,x-1,y,z)||rain(w,x+1,y,z)||rain(w,x,y,z-1)||rain(w,x,y,z+1)))) {
        world_set_block(w,x,y,z,0); return;
    }
    if(age<15) { unsigned next=age+world_random(w,3)/2; world_set_metadata(w,x,y,z,(uint8_t)(next>15?15:next)); }
    world_schedule_tick(w,x,y,z,51,40);
    if(!eternal && !encouragement(w,x,y,z)) {
        if(!world_block_def(world_peek_block(w,x,y-1,z))->opaque || age>3) world_set_block(w,x,y,z,0);
        return;
    }
    if(!eternal && !fire_encouragement(world_peek_block(w,x,y-1,z)) && age==15 && !world_random(w,4)) {
        world_set_block(w,x,y,z,0); return;
    }
    for(n=0;n<6;++n) burn(w,x+dx[n],y+dy[n],z+dz[n],dy[n]?250:300,age);
    for(a=-1;a<=1;++a) for(c=-1;c<=1;++c) for(b=-1;b<=4;++b) {
        int chance,denominator=100+(b>1?(b-1)*100:0),ny=y+b,nx=x+a,nz=z+c;
        Chunk *loaded=world_peek_chunk(w,nx>>4,nz>>4);
        if((!a&&!b&&!c) || (unsigned)ny>=128 || !loaded || (w->beta_format&&!loaded->beta_raw) || world_peek_block(w,nx,ny,nz)) continue;
        chance=encouragement(w,nx,ny,nz); if(!chance) continue;
        chance=(chance+40)/(age+30);
        if(chance>0 && world_random(w,(unsigned)denominator)<=(unsigned)chance &&
           !(rain(w,nx,ny,nz)||rain(w,nx-1,ny,nz)||rain(w,nx+1,ny,nz)||rain(w,nx,ny,nz-1)||rain(w,nx,ny,nz+1))) {
            unsigned next=age+world_random(w,5)/4;
            world_set_state(w,nx,ny,nz,(BetaBlockState){51,(uint8_t)(next>15?15:next)});
        }
    }
    if(world_random(w,24)==0) world_sound(w,"fire.fire",x+.5f,y+.5f,z+.5f,1+(float)world_random(w,100)/100,.3f+(float)world_random(w,70)/100);
}
