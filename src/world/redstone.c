#include "redstone.h"
#include <stdlib.h>
#include <string.h>
static const int dx[6]={-1,1,0,0,0,0},dy[6]={0,0,-1,1,0,0},dz[6]={0,0,0,0,-1,1};
static const int bx[4]={0,-1,0,1},bz[4]={1,0,-1,0};
static int opaque(const World *w,int x,int y,int z) { return world_block_def(world_peek_block(w,x,y,z))->opaque; }
static int connected(const World *w,int x,int y,int z,int direction)
{
    unsigned id=world_peek_block(w,x,y,z);
    /* ModelBed.footInvisibleFaceRemap: wire's neighbour direction is the
     * opposite of the repeater metadata direction, including idle repeaters. */
    if(id==93 || id==94) return direction>=0 && (int)(world_peek_metadata(w,x,y,z)&3)==((direction+2)&3);
    return id==55 || id==69 || id==77 || id==75 || id==76 || id==28 || id==70 || id==72;
}
int world_wire_connects(const World *w,int x,int y,int z,int n)
{
    int nx=x+bx[n],nz=z+bz[n],solid=opaque(w,nx,y,nz);
    if(connected(w,nx,y,nz,n)) return 1;
    if(!solid && connected(w,nx,y-1,nz,-1)) return 1;
    return solid && !opaque(w,x,y+1,z) && connected(w,nx,y+1,nz,-1);
}
static int emit(const World *w,int x,int y,int z,int tx,int ty,int tz,int wire,int strong)
{
    unsigned id=world_peek_block(w,x,y,z),meta=world_peek_metadata(w,x,y,z);
    int dir=meta&7,sx=x,sy=y-1,sz=z;
    if(dir==1) { sx=x-1; sy=y; } else if(dir==2) { sx=x+1; sy=y; }
    else if(dir==3) { sz=z-1; sy=y; } else if(dir==4) { sz=z+1; sy=y; }
    if(id==69 || id==77) return (meta&8) && (!strong || (sx==tx && sy==ty && sz==tz)) ? 15 : 0;
    if(id==28) return (meta&8) && (!strong || ty==y-1) ? 15 : 0;
    if(id==70 || id==72) return meta && (!strong || ty==y-1) ? 15 : 0;
    if(id==76) return strong ? (ty==y+1 ? 15 : 0) : sx==tx && sy==ty && sz==tz ? 0 : 15;
    if(id==94) return tx==x-bx[meta&3] && tz==z-bz[meta&3] && ty==y ? 15 : 0;
    if(id==55 && wire && meta) {
        int a=world_wire_connects(w,x,y,z,0),b=world_wire_connects(w,x,y,z,1);
        int c=world_wire_connects(w,x,y,z,2),d=world_wire_connects(w,x,y,z,3);
        if(ty==y-1) return (int)meta;
        if(ty!=y) return 0;
        if(!a && !b && !c && !d) return (int)meta;
        if(tx==x && !b && !d && ((tz<z && a) || (tz>z && c))) return (int)meta;
        if(tz==z && !a && !c && ((tx< x && d) || (tx>x && b))) return (int)meta;
    }
    return 0;
}
/* Resolve a connected wire network within the neighbour callback, as Beta's
 * calculateCurrentChanges does. An explicit graph avoids Java's recursive
 * stack depth; external power is sampled with ALL dust output disabled. No
 * intermediate strengths are published to repeaters/torches. */
typedef struct WireNode { int x,y,z,links[8],count; unsigned char old,power; } WireNode;
typedef struct WireGraph { WireNode *nodes; unsigned *hash; size_t count,capacity; } WireGraph;
static unsigned wire_hash(int x,int y,int z)
{ return (unsigned)x*73856093u^(unsigned)y*19349663u^(unsigned)z*83492791u; }
static int wire_node(WireGraph *g,int x,int y,int z)
{
    size_t slot;
    if(!g->capacity || g->count*2>=g->capacity) {
        size_t cap=g->capacity ? g->capacity*2 : 128,i;
        unsigned *hash=(unsigned *)calloc(cap,sizeof(*hash));
        WireNode *nodes;
        if(!hash) return -1;
        nodes=(WireNode *)realloc(g->nodes,cap/2*sizeof(*nodes));
        if(!nodes) { free(hash); return -1; }
        g->nodes=nodes;
        for(i=0;i<g->count;++i) {
            size_t s=wire_hash(nodes[i].x,nodes[i].y,nodes[i].z)&(cap-1);
            while(hash[s]) s=(s+1)&(cap-1);
            hash[s]=(unsigned)i+1;
        }
        free(g->hash); g->hash=hash; g->capacity=cap;
    }
    slot=wire_hash(x,y,z)&(g->capacity-1);
    while(g->hash[slot]) {
        unsigned n=g->hash[slot]-1;
        if(g->nodes[n].x==x && g->nodes[n].y==y && g->nodes[n].z==z) return (int)n;
        slot=(slot+1)&(g->capacity-1);
    }
    memset(&g->nodes[g->count],0,sizeof(*g->nodes));
    g->nodes[g->count].x=x; g->nodes[g->count].y=y; g->nodes[g->count].z=z;
    g->hash[slot]=(unsigned)g->count+1; return (int)g->count++;
}
void world_wire_update(World *w,int x,int y,int z)
{
    WireGraph g={0}; size_t i; int level,failed=0;
    if(w->network_mode || w->wire_updating || world_peek_block(w,x,y,z)!=55) return;
    if(!opaque(w,x,y-1,z)) {
        world_drop_stack(w,x,y,z,(InventorySlot){331,1,0}); world_set_block(w,x,y,z,0); return;
    }
    if(wire_node(&g,x,y,z)<0) return;
    for(i=0;i<g.count && !failed;++i) {
        int n,cx=g.nodes[i].x,cy=g.nodes[i].y,cz=g.nodes[i].z;
        g.nodes[i].old=world_peek_metadata(w,cx,cy,cz);
        g.nodes[i].power=world_redstone_power(w,cx,cy,cz,cx,cy,cz,0)>0 ? 15 : 0;
        for(n=0;n<4;++n) {
            int nx=cx+bx[n],nz=cz+bz[n],ny=cy,k;
            for(k=0;k<2;++k) {
                if(k) {
                    if(opaque(w,nx,cy,nz)) { if(opaque(w,cx,cy+1,cz)) continue; ny=cy+1; }
                    else ny=cy-1;
                }
                if(world_peek_block(w,nx,ny,nz)==55) {
                    int link=wire_node(&g,nx,ny,nz);
                    if(link<0) { failed=1; break; }
                    g.nodes[i].links[g.nodes[i].count++]=link;
                }
            }
        }
    }
    if(!failed) {
        /* Only 15 signal levels exist; O(15*N), independent of wire length. */
        for(level=15;level>1;--level) for(i=0;i<g.count;++i) if(g.nodes[i].power==level) {
            int n;
            for(n=0;n<g.nodes[i].count;++n) {
                WireNode *to=&g.nodes[g.nodes[i].links[n]];
                if(to->power<level-1) to->power=(unsigned char)(level-1);
            }
        }
        w->wire_updating=1;
        for(i=0;i<g.count;++i) if(g.nodes[i].old!=g.nodes[i].power)
            world_set_metadata(w,g.nodes[i].x,g.nodes[i].y,g.nodes[i].z,g.nodes[i].power);
        for(i=0;i<g.count;++i) if(g.nodes[i].old!=g.nodes[i].power && (!g.nodes[i].old || !g.nodes[i].power))
            world_redstone_notify(w,g.nodes[i].x,g.nodes[i].y,g.nodes[i].z);
        w->wire_updating=0;
    } else w->error=WORLD_ERROR_OUT_OF_MEMORY;
    free(g.nodes); free(g.hash);
}
int world_redstone_signal(const World *w,int x,int y,int z,int tx,int ty,int tz,int wire)
{
    int i,best=0;
    if(!opaque(w,x,y,z)) return emit(w,x,y,z,tx,ty,tz,wire,0);
    for(i=0;i<6;++i) {
        int n=emit(w,x+dx[i],y+dy[i],z+dz[i],x,y,z,wire,1);
        if(n>best) best=n;
    }
    return best;
}
int world_redstone_power(const World *w,int x,int y,int z,int ex,int ey,int ez,int wire)
{
    int i,best=0;
    for(i=0;i<6;++i) {
        int nx=x+dx[i],ny=y+dy[i],nz=z+dz[i],n;
        if(nx==ex && ny==ey && nz==ez) continue;
        n=world_redstone_signal(w,nx,ny,nz,x,y,z,wire); if(n>best) best=n;
    }
    return best;
}
int world_repeater_input(const World *w,int x,int y,int z,unsigned meta)
{
    int nx=x+bx[meta&3],nz=z+bz[meta&3];
    if(world_peek_block(w,nx,y,nz)==55 && world_peek_metadata(w,nx,y,nz)>0) return 1;
    return world_redstone_signal(w,nx,y,nz,x,y,z,1)>0;
}
int world_redstone_activate(World *w,int x,int y,int z)
{
    unsigned id=world_peek_block(w,x,y,z),meta=world_peek_metadata(w,x,y,z);
    if(w->network_mode) return 0;
    if(id==77) {
        if(meta&8) return 1;
        world_set_metadata(w,x,y,z,(uint8_t)(meta|8));
        world_schedule_tick(w,x,y,z,77,20);
        world_sound(w,"random.click",x+.5f,y+.5f,z+.5f,.3f,.6f);
    } else if(id==69) {
        meta^=8; world_set_metadata(w,x,y,z,(uint8_t)meta);
        world_sound(w,"random.click",x+.5f,y+.5f,z+.5f,.3f,(meta&8) ? .6f : .5f);
    } else if(id==93 || id==94) world_set_metadata(w,x,y,z,(uint8_t)(((meta+4)&12)|(meta&3)));
    else return 0;
    world_redstone_notify(w,x,y,z); return 1;
}
