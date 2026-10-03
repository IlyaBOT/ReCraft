#include "foliage.h"
#include "environment.h"
static int chunks_loaded(const World *w,int x,int z,int radius)
{
    int cx,cz,min_x=(x-radius)>>4,max_x=(x+radius)>>4;
    int min_z=(z-radius)>>4,max_z=(z+radius)>>4;
    for(cx=min_x;cx<=max_x;++cx) for(cz=min_z;cz<=max_z;++cz) {
        Chunk *c=world_peek_chunk(w,cx,cz);
        if(!c || (w->beta_format && !c->beta_raw)) return 0;
    }
    return 1;
}
void world_foliage_removed(World *w,int x,int y,int z,unsigned old_id)
{
    int a,b,c,r=old_id==17?4:1;
    if(w->network_mode || (old_id!=17 && old_id!=18) || !chunks_loaded(w,x,z,r+1)) return;
    for(a=-r;a<=r;++a) for(b=-r;b<=r;++b) for(c=-r;c<=r;++c)
        if(world_peek_block(w,x+a,y+b,z+c)==18)
            world_set_metadata(w,x+a,y+b,z+c,world_peek_metadata(w,x+a,y+b,z+c)|8);
}
void world_leaf_tick(World *w,int x,int y,int z)
{
    signed char distance[729]; unsigned short queue[729]; int head=0,tail=0,a,b,c;
    unsigned meta=world_peek_metadata(w,x,y,z);
    static const int offsets[6]={-81,81,-9,9,-1,1};
    if(w->network_mode || world_peek_block(w,x,y,z)!=18 || !(meta&8) || !chunks_loaded(w,x,z,5)) return;
    for(a=0;a<9;++a) for(b=0;b<9;++b) for(c=0;c<9;++c) {
        int index=a*81+b*9+c; unsigned id=world_peek_block(w,x+a-4,y+b-4,z+c-4);
        distance[index]=id==17?0:id==18?-2:-1;
        if(id==17) queue[tail++]=(unsigned short)index;
    }
    while(head<tail) {
        int index=queue[head++],n,coords[3]={index/81,(index/9)%9,index%9};
        if(distance[index]>=4) continue;
        for(n=0;n<6;++n) {
            int coord=coords[n/2],next=index+offsets[n];
            if((n%2==0 && !coord) || (n%2==1 && coord==8)) continue;
            if(distance[next]==-2) { distance[next]=distance[index]+1; queue[tail++]=(unsigned short)next; }
        }
    }
    if(distance[364]>=0) world_set_metadata(w,x,y,z,(uint8_t)(meta&~8));
    else {
        if(world_random(w,20)==0) world_drop_stack(w,x,y,z,(InventorySlot){6,1,(int)(meta&3)});
        world_set_block(w,x,y,z,0);
    }
}
