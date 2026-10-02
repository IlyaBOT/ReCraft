#include "rail.h"
#include "redstone.h"
#include "entities.h"
#include <math.h>
#include <stdlib.h>
const int rail_ends[10][2][3]={{{0,0,-1},{0,0,1}},{{-1,0,0},{1,0,0}},
    {{-1,-1,0},{1,0,0}},{{-1,0,0},{1,-1,0}},{{0,0,-1},{0,-1,1}},{{0,-1,-1},{0,0,1}},
    {{0,0,1},{1,0,0}},{{0,0,1},{-1,0,0}},{{0,0,-1},{-1,0,0}},{{0,0,-1},{1,0,0}}};
int rail_is(unsigned id) { return id==66 || id==27 || id==28; }
static int shape_at(const World *w,int x,int y,int z)
{ unsigned id=world_peek_block(w,x,y,z),m=world_peek_metadata(w,x,y,z); return id==66 ? (m<10 ? (int)m : 0) : (int)(m&7)<6 ? (int)(m&7) : 0; }
static int neighbor(const World *w,int x,int *y,int z)
{
    if(rail_is(world_peek_block(w,x,*y,z))) return 1;
    if(rail_is(world_peek_block(w,x,*y+1,z))) { ++*y; return 1; }
    if(rail_is(world_peek_block(w,x,*y-1,z))) { --*y; return 1; }
    return 0;
}
static int accepts(const World *w,int x,int y,int z,int tx,int tz)
{
    int s,n,connections=0;
    if(!neighbor(w,x,&y,z)) return 0;
    s=shape_at(w,x,y,z);
    for(n=0;n<2;++n) {
        int nx=x+rail_ends[s][n][0],nz=z+rail_ends[s][n][2],ny=y;
        if(nx==tx && nz==tz) return 1;
        if(neighbor(w,nx,&ny,nz)) {
            int ns=shape_at(w,nx,ny,nz),k;
            for(k=0;k<2;++k) if(nx+rail_ends[ns][k][0]==x && nz+rail_ends[ns][k][2]==z) { ++connections; break; }
        }
    }
    return connections!=2;
}
static int powered_chain(const World *w,int x,int y,int z,int shape,int end,int depth)
{
    int n,ny,ns,axis=(shape==1 || shape==2 || shape==3),incoming_x=x,incoming_z=z;
    if(depth>=8) return 0;
    x+=rail_ends[shape][end][0]; z+=rail_ends[shape][end][2];
    /* Slope endpoint height follows the upper rail elevation. */
    if(shape>=2 && shape<=5) y+=rail_ends[shape][end][1]+1;
    ny=y;
    if(!neighbor(w,x,&ny,z) || world_peek_block(w,x,ny,z)!=27) return 0;
    ns=shape_at(w,x,ny,z);
    if(axis!=(ns==1 || ns==2 || ns==3)) return 0;
    if(world_redstone_power(w,x,ny,z,x,ny,z,1)) return 1;
    for(n=0;n<2;++n) if(x+rail_ends[ns][n][0]!=incoming_x || z+rail_ends[ns][n][2]!=incoming_z)
        if(powered_chain(w,x,ny,z,ns,n,depth+1)) return 1;
    return 0;
}
static void update(World *w,int x,int y,int z,int depth)
{
    unsigned id=world_peek_block(w,x,y,z),old=world_peek_metadata(w,x,y,z); int s=-1,north,south,west,east,power,k,dy;
    static const int dx[4]={0,0,-1,1},dz[4]={-1,1,0,0};
    if(w->network_mode || !rail_is(id)) return;
    s=shape_at(w,x,y,z);
    if(s>=2 && s<=5) {
        Chunk *support=world_peek_chunk(w,(int)floorf((x+(s==2 ? 1 : s==3 ? -1 : 0))/16.0f),
            (int)floorf((z+(s==4 ? -1 : s==5 ? 1 : 0))/16.0f));
        if(!support || (w->beta_format && !support->beta_raw)) return;
    }
    if(!world_block_def(world_peek_block(w,x,y-1,z))->opaque ||
       (s>=2 && s<=5 && !world_block_def(world_peek_block(w,x+(s==2 ? 1 : s==3 ? -1 : 0),y,z+(s==4 ? -1 : s==5 ? 1 : 0)))->opaque)) {
        world_drop_stack(w,x,y,z,(InventorySlot){(int)id,1,0}); world_set_block(w,x,y,z,0); return;
    }
    north=accepts(w,x,y,z-1,x,z); south=accepts(w,x,y,z+1,x,z);
    west=accepts(w,x-1,y,z,x,z); east=accepts(w,x+1,y,z,x,z);
    s=-1; power=world_redstone_power(w,x,y,z,x,y,z,1)>0;
    if((north || south) && !west && !east) s=0;
    if((west || east) && !north && !south) s=1;
    if(id==66) {
        if(south && east && !north && !west) s=6;
        if(south && west && !north && !east) s=7;
        if(north && west && !south && !east) s=8;
        if(north && east && !south && !west) s=9;
    }
    if(s<0) {
        if(north || south) s=0;
        if(west || east) s=1;
        if(id==66) {
            if(power) { if(south && east) s=6; if(west && south) s=7; if(east && north) s=9; if(north && west) s=8; }
            else { if(north && west) s=8; if(east && north) s=9; if(west && south) s=7; if(south && east) s=6; }
        }
    }
    if(s<0) s=0;
    if(s==0) { if(rail_is(world_peek_block(w,x,y+1,z-1))) s=4; if(rail_is(world_peek_block(w,x,y+1,z+1))) s=5; }
    if(s==1) { if(rail_is(world_peek_block(w,x+1,y+1,z))) s=2; if(rail_is(world_peek_block(w,x-1,y+1,z))) s=3; }
    if(id==27) power=power || powered_chain(w,x,y,z,s,0,0) || powered_chain(w,x,y,z,s,1,0);
    if(id==28) {
        size_t i; power=0;
        for(i=0;i<w->cache_count && !power;++i) { const SavedEntity *e;
            if(abs(w->cache[i]->x-(int)floorf(x/16.0f))>1 || abs(w->cache[i]->z-(int)floorf(z/16.0f))>1) continue;
            for(e=w->cache[i]->saved_entities;e;e=e->next)
                if(e->transport.kind==2 && !e->transport.dead && fabsf(e->mob.x-x-.5f)<.79f &&
                   fabsf(e->mob.z-z-.5f)<.79f && e->mob.y+.35f>y && e->mob.y-.35f<y+.25f) { power=1; break; }
        }
        if(power) world_schedule_tick(w,x,y,z,28,20);
    }
    if(id!=66 && power) s|=8;
    if((unsigned)s!=old) {
        world_set_metadata(w,x,y,z,(uint8_t)s);
        if(id==28) world_redstone_notify(w,x,y,z);
        for(k=0;k<4;++k) for(dy=-1;dy<=1;++dy) {
            unsigned other=world_peek_block(w,x+dx[k],y+dy,z+dz[k]);
            if(rail_is(other)) {
                world_schedule_tick(w,x+dx[k],y+dy,z+dz[k],(uint8_t)other,0);
                /* Power changes notify a powered-rail chain in the same tick,
                 * as BlockRail.onNeighborBlockChange does. Bound recursion to
                 * the vanilla eight-rail reach; queued work handles branches. */
                if(id==27 && other==27 && ((old^(unsigned)s)&8) && depth<8)
                    update(w,x+dx[k],y+dy,z+dz[k],depth+1);
            }
        }
    }
}
void rail_update(World *w,int x,int y,int z) { update(w,x,y,z,0); }
int rail_path(const World *w,float x,float y,float z,float *px,float *py,float *pz,int *shape)
{
    int bx=(int)floorf(x),by=(int)floorf(y),bz=(int)floorf(z),s; float ax,az,dx,dz,t,y0,y1;
    if(rail_is(world_peek_block(w,bx,by-1,bz))) --by;
    if(!rail_is(world_peek_block(w,bx,by,bz))) return 0;
    s=shape_at(w,bx,by,bz); *shape=s;
    ax=bx+.5f+rail_ends[s][0][0]*.5f; az=bz+.5f+rail_ends[s][0][2]*.5f;
    dx=(rail_ends[s][1][0]-rail_ends[s][0][0])*.5f; dz=(rail_ends[s][1][2]-rail_ends[s][0][2])*.5f;
    t=((x-ax)*dx+(z-az)*dz)/(dx*dx+dz*dz);
    *px=ax+dx*t; *pz=az+dz*t;
    y0=by+.5f+rail_ends[s][0][1]*.5f; y1=by+.5f+rail_ends[s][1][1]*.5f;
    *py=y0+(y1-y0)*t*2;
    if(y1<y0) *py+=1;
    if(y1>y0) *py+=.5f;
    return 1;
}
