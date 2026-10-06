#include "collision.h"
#include "piston.h"
#include <math.h>

double world_aabb_clip(const WorldAabb *a,const WorldAabb *b,int axis,double delta)
{
    int i;
    for(i=0;i<3;++i) if(i!=axis && (a->max[i]<=b->min[i]+1e-7 || a->min[i]>=b->max[i]-1e-7)) return delta;
    if(delta>0 && a->max[axis]<=b->min[axis]+1e-6) {
        double gap=b->min[axis]-a->max[axis]; if(gap<delta) delta=gap;
    } else if(delta<0 && a->min[axis]>=b->max[axis]-1e-6) {
        double gap=b->max[axis]-a->min[axis]; if(gap>delta) delta=gap;
    }
    return delta;
}
double world_clip_axis(World *w,const WorldAabb *body,int axis,double delta,int load)
{
    int lo[3],hi[3],i,x,y,z;
    if(!delta || !isfinite(delta)) return 0;
    for(i=0;i<3;++i) {
        lo[i]=(int)floor(body->min[i]+(i==axis && delta<0 ? delta : 0))-1;
        hi[i]=(int)floor(body->max[i]+(i==axis && delta>0 ? delta : 0))+1;
    }
    if(lo[1]<-1)lo[1]=-1;
    if(hi[1]>127)hi[1]=127;
    for(y=lo[1];y<=hi[1];++y) for(z=lo[2];z<=hi[2];++z) for(x=lo[0];x<=hi[0];++x) {
        BetaBlockBox boxes[2];int n,j;unsigned id=load ? world_get_block(w,x,y,z) : world_peek_block(w,x,y,z);
        if(!id)continue;
        n=world_block_collision_boxes(w,x,y,z,boxes);
        for(j=0;j<n;++j) {
            WorldAabb b={{x+boxes[j].min_x,y+boxes[j].min_y,z+boxes[j].min_z},
                         {x+boxes[j].max_x,y+boxes[j].max_y,z+boxes[j].max_z}};
            delta=world_aabb_clip(body,&b,axis,delta);
        }
    }
    return delta;
}
