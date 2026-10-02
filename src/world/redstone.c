#include "redstone.h"
static const int dx[6]={-1,1,0,0,0,0},dy[6]={0,0,-1,1,0,0},dz[6]={0,0,0,0,-1,1};
static const int bx[4]={0,-1,0,1},bz[4]={1,0,-1,0};
static int opaque(const World *w,int x,int y,int z) { return world_block_def(world_peek_block(w,x,y,z))->opaque; }
static int connected(const World *w,int x,int y,int z,int direction)
{
    unsigned id=world_peek_block(w,x,y,z);
    /* ModelBed.footInvisibleFaceRemap: wire's neighbour direction is the
     * opposite of the repeater metadata direction, including idle repeaters. */
    if(id==93 || id==94) return direction>=0 && (int)(world_peek_metadata(w,x,y,z)&3)==((direction+2)&3);
    return id==55 || id==69 || id==77 || id==75 || id==76;
}
static int wire_connect(const World *w,int x,int y,int z,int n)
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
    if(id==76) return strong ? (ty==y+1 ? 15 : 0) : sx==tx && sy==ty && sz==tz ? 0 : 15;
    if(id==94) return tx==x-bx[meta&3] && tz==z-bz[meta&3] && ty==y ? 15 : 0;
    if(id==55 && wire && meta) {
        int a=wire_connect(w,x,y,z,0),b=wire_connect(w,x,y,z,1);
        int c=wire_connect(w,x,y,z,2),d=wire_connect(w,x,y,z,3);
        if(ty==y-1) return (int)meta;
        if(ty!=y) return 0;
        if(!a && !b && !c && !d) return (int)meta;
        if(tx==x && !b && !d && ((tz<z && a) || (tz>z && c))) return (int)meta;
        if(tz==z && !a && !c && ((tx< x && d) || (tx>x && b))) return (int)meta;
    }
    return 0;
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
