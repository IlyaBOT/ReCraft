#include "fluid.h"
#include "../game/mining.h"
#include <math.h>
static const int dx[4]={-1,1,0,0},dz[4]={0,0,-1,1};
int fluid_kind(unsigned id) { return id==8 || id==9 ? 1 : id==10 || id==11 ? 2 : 0; }
static int loaded(const World *w,int x,int y,int z)
{
    int cx=x>=0 ? x/16 : (int)(-((-(int64_t)x+15)/16));
    int cz=z>=0 ? z/16 : (int)(-((-(int64_t)z+15)/16));
    Chunk *c=world_peek_chunk(w,cx,cz);
    return (unsigned)y<128 && c && (!w->beta_format || c->beta_raw);
}
static int decay(const World *w,int x,int y,int z,int kind)
{ return fluid_kind(world_peek_block(w,x,y,z))==kind ? world_peek_metadata(w,x,y,z) : -1; }
static int barrier(const World *w,int x,int y,int z)
{
    unsigned id=world_peek_block(w,x,y,z);
    return !loaded(w,x,y,z) || id==64 || id==71 || id==63 || id==65 || id==83 || beta_material_blocks_flow(id);
}
static int displace(const World *w,int x,int y,int z,int kind)
{
    int other=fluid_kind(world_peek_block(w,x,y,z));
    return other!=kind && other!=2 && !barrier(w,x,y,z);
}
static unsigned random4(World *w)
{
    w->random_seed=(w->random_seed*UINT64_C(0x5deece66d)+11)&UINT64_C(0xffffffffffff);
    return (unsigned)(w->random_seed>>46);
}
static void set_flow(World *w,int x,int y,int z,unsigned id,int meta)
{
    if (!loaded(w,x,y,z)) return;
    if (world_set_block(w,x,y,z,(uint8_t)id)) {
        world_set_metadata(w,x,y,z,(uint8_t)meta);
        world_schedule_tick(w,x,y,z,(uint8_t)id,id==8 ? 5 : 30);
    }
}
static void settle(World *w,int x,int y,int z,unsigned id,int meta)
{
    int cx=x>=0 ? x/16 : (int)(-((-(int64_t)x+15)/16));
    int cz=z>=0 ? z/16 : (int)(-((-(int64_t)z+15)/16));
    Chunk *c=world_peek_chunk(w,cx,cz);
    /* setBlockAndMetadata without neighbor notification: notifying our own
     * settled cell would immediately turn it back into moving liquid. */
    if (c) { chunk_set_block(c,x-cx*16,y,z-cz*16,(uint8_t)(id+1)); chunk_set_metadata(c,x-cx*16,y,z-cz*16,(uint8_t)meta); }
}
static int cost(const World *w,int x,int y,int z,int kind,int depth,int previous)
{
    int best=1000,i;
    for (i=0;i<4;++i) {
        int nx=x+dx[i],nz=z+dz[i],n;
        if (i==(previous^1) || barrier(w,nx,y,nz) || decay(w,nx,y,nz,kind)==0) continue;
        if (!barrier(w,nx,y-1,nz)) return depth;
        if (depth<4) { n=cost(w,nx,y,nz,kind,depth+1,i); if (n<best) best=n; }
    }
    return best;
}
void fluid_tick(World *w,int x,int y,int z,unsigned id)
{
    int kind=fluid_kind(id),level=decay(w,x,y,z,kind),step=kind==2 ? 2 : 1;
    int i,sources=0,min=-100,next,above,costs[4],best=1000;
    if (level<0) return;
    /* Beta hardens lava touching water on the sides or above. */
    if (kind==2) {
        int water=fluid_kind(world_peek_block(w,x,y+1,z))==1;
        for (i=0;i<4;++i) if (fluid_kind(world_peek_block(w,x+dx[i],y,z+dz[i]))==1) water=1;
        if (water && level<=4) {
            world_set_block(w,x,y,z,level==0 ? 49 : 4);
            world_sound(w,"random.fizz",x+.5f,y+.5f,z+.5f,.5f,2.6f); return;
        }
    }
    if (level>0) {
        for (i=0;i<4;++i) {
            int n=decay(w,x+dx[i],y,z+dz[i],kind);
            if (n<0) continue;
            if (n==0) ++sources;
            if (n>=8) n=0;
            if (min<0 || n<min) min=n;
        }
        next=min+step;
        if (next>=8 || min<0) next=-1;
        above=decay(w,x,y+1,z,kind);
        if (above>=0) next=above>=8 ? above : above+8;
        if (kind==1 && sources>=2 && (beta_material_solid(world_peek_block(w,x,y-1,z)) ||
            (decay(w,x,y-1,z,kind)>=0 && level==0))) next=0;
        if (kind==2 && level<8 && next<8 && next>level && random4(w)!=0) {
            world_schedule_tick(w,x,y,z,(uint8_t)id,30);
        } else if (next!=level) {
            level=next;
            if (next<0) world_set_block(w,x,y,z,0);
            else {
                world_set_metadata(w,x,y,z,(uint8_t)next);
                world_schedule_tick(w,x,y,z,(uint8_t)id,kind==1 ? 5 : 30);
                world_physics_notify(w,x,y,z);
            }
        } else settle(w,x,y,z,id,level);
    } else settle(w,x,y,z,id,level);
    if (level<0) return;
    if (displace(w,x,y-1,z,kind)) {
        set_flow(w,x,y-1,z,id,level>=8 ? level : level+8);
    } else if (level==0 || barrier(w,x,y-1,z)) {
        next=level>=8 ? 1 : level+step;
        if (next>=8) return;
        for (i=0;i<4;++i) {
            int nx=x+dx[i],nz=z+dz[i];
            costs[i]=1000;
            if (!barrier(w,nx,y,nz) && decay(w,nx,y,nz,kind)!=0)
                costs[i]=!barrier(w,nx,y-1,nz) ? 0 : cost(w,nx,y,nz,kind,1,i);
            if (costs[i]<best) best=costs[i];
        }
        for (i=0;i<4;++i) if (costs[i]==best && displace(w,x+dx[i],y,z+dz[i],kind)) {
            if (kind==1) {
                BetaBlockState b={world_peek_block(w,x+dx[i],y,z+dz[i]),world_peek_metadata(w,x+dx[i],y,z+dz[i])};
                world_drop_stack(w,x+dx[i],y,z+dz[i],mining_drop(b,0,(uint32_t)w->random_seed));
            }
            set_flow(w,x+dx[i],y,z+dz[i],id,next);
        }
    }
}
float fluid_corner_height(const World *w,int x,int y,int z,int kind)
{
    float air=0; int count=0,dx0,dz0;
    for (dz0=-1;dz0<=0;++dz0) for (dx0=-1;dx0<=0;++dx0) {
        int bx=x+dx0,bz=z+dz0,n=decay(w,bx,y,bz,kind);
        if (decay(w,bx,y+1,bz,kind)>=0) return 1;
        if (n>=0) {
            float a=(float)((n>=8 ? 0 : n)+1)/9;
            if (n==0 || n>=8) { air+=a*10; count+=10; }
            air+=a; ++count;
        } else if (!beta_material_solid(world_peek_block(w,bx,y,bz))) { air+=1; ++count; }
    }
    return count ? 1-air/count : 0;
}
void fluid_flow_vector(const World *w,int x,int y,int z,float out[3])
{
    int kind=fluid_kind(world_peek_block(w,x,y,z)),level=decay(w,x,y,z,kind),i;
    float len;
    out[0]=out[1]=out[2]=0;
    if (!kind) return;
    if (level>=8) level=0;
    for (i=0;i<4;++i) {
        int nx=x+dx[i],nz=z+dz[i],n=decay(w,nx,y,nz,kind),gradient;
        if (n<0) {
            if (beta_material_blocks_flow(world_peek_block(w,nx,y,nz))) continue;
            n=decay(w,nx,y-1,nz,kind);
            if (n<0) continue;
            if (n>=8) n=0;
            gradient=n-(level-8);
        } else gradient=(n>=8 ? 0 : n)-level;
        out[0]+=dx[i]*gradient; out[2]+=dz[i]*gradient;
    }
    len=sqrtf(out[0]*out[0]+out[2]*out[2]);
    if (len>0) { out[0]/=len; out[2]/=len; }
    if (world_peek_metadata(w,x,y,z)>=8) for (i=0;i<4;++i)
        if (world_block_def(world_peek_block(w,x+dx[i],y,z+dz[i]))->opaque ||
            world_block_def(world_peek_block(w,x+dx[i],y+1,z+dz[i]))->opaque) { out[1]=-6; break; }
    len=sqrtf(out[0]*out[0]+out[1]*out[1]+out[2]*out[2]);
    if (len>0) for (i=0;i<3;++i) out[i]/=len;
}
