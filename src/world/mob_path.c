#include "mob_path.h"
#include <math.h>
#include <string.h>
typedef struct Node { int x,y,z,parent;float g,h;unsigned char closed; } Node;
static int space(const World *w,int x,int y,int z,int width,int height)
{
    int a,b,c;
    if(y<0 || y+height>128) return 0;
    for(a=x;a<x+width;++a) for(c=z;c<z+width;++c) {
        Chunk *chunk=world_peek_chunk(w,a>>4,c>>4);
        if(!chunk || (w->beta_format&&!chunk->beta_raw)) return 0;
        for(b=y;b<y+height;++b) {
            unsigned id=world_peek_block(w,a,b,c);
            if(id==8||id==9) return -1;
            if(id==10||id==11) return -2;
            if(id==64||id==71) { if(!(world_peek_metadata(w,a,b,c)&4)) return 0; }
            else if(beta_material_solid(id)) return 0;
        }
    }
    return 1;
}
int mob_path_find(const World *w,float width,float height,float x,float y,float z,float tx,float ty,float tz,MobPath *path)
{
    Node nodes[512];int count=1,best=0,iterations,n,chain[512],length=0;
    int sx=(int)floorf(x-width*.5f),sy=(int)floorf(y+.5f),sz=(int)floorf(z-width*.5f);
    int gx=(int)floorf(tx-width*.5f),gy=(int)floorf(ty),gz=(int)floorf(tz-width*.5f);
    int body_width=(int)floorf(width+1),body_height=(int)floorf(height+1);
    static const int dx[4]={0,-1,1,0},dz[4]={1,0,0,-1};
    memset(path,0,sizeof(*path));
    nodes[0]=(Node){sx,sy,sz,-1,0,0,0};
    nodes[0].h=sqrtf((float)((gx-sx)*(gx-sx)+(gy-sy)*(gy-sy)+(gz-sz)*(gz-sz)));
    for(iterations=0;iterations<512;++iterations) {
        int current=-1;float score=1e30f;
        for(n=0;n<count;++n) if(!nodes[n].closed && nodes[n].g+nodes[n].h<score) {current=n;score=nodes[n].g+nodes[n].h;}
        if(current<0) break;
        nodes[current].closed=1;
        if(nodes[current].h<nodes[best].h) best=current;
        if(nodes[current].x==gx && nodes[current].y==gy && nodes[current].z==gz) { best=current;break; }
        for(n=0;n<4;++n) {
            int nx=nodes[current].x+dx[n],ny=nodes[current].y,nz=nodes[current].z+dz[n],drop=0,found,k;
            float cost,heuristic;
            if(space(w,nx,ny,nz,body_width,body_height)!=1) {
                if(space(w,nodes[current].x,ny+1,nodes[current].z,body_width,body_height)!=1 || space(w,nx,ny+1,nz,body_width,body_height)!=1) continue;
                ++ny;
            }
            while(ny>0 && space(w,nx,ny-1,nz,body_width,body_height)==1) {--ny;if(++drop>=4)break;}
            if(drop>=4 || ny<=0 || space(w,nx,ny-1,nz,body_width,body_height)==-2) continue;
            heuristic=sqrtf((float)((gx-nx)*(gx-nx)+(gy-ny)*(gy-ny)+(gz-nz)*(gz-nz)));
            if(heuristic>=16) continue;
            cost=nodes[current].g+sqrtf(1.0f+(ny-nodes[current].y)*(ny-nodes[current].y));
            found=-1;for(k=0;k<count;++k) if(nodes[k].x==nx&&nodes[k].y==ny&&nodes[k].z==nz){found=k;break;}
            if(found>=0) {if(nodes[found].closed||cost>=nodes[found].g)continue;nodes[found].g=cost;nodes[found].parent=current;}
            else if(count<512) nodes[count++]=(Node){nx,ny,nz,current,cost,heuristic,0};
        }
    }
    if(best==0) return 0;
    for(n=best;n>=0 && length<512;n=nodes[n].parent) chain[length++]=n;
    for(n=length-2;n>=0 && path->count<MOB_PATH_POINTS;--n) {
        Node *node=&nodes[chain[n]];int at=path->count++;
        path->x[at]=node->x;path->y[at]=(unsigned char)node->y;path->z[at]=node->z;
    }
    path->ticks=20;return path->count>0;
}
