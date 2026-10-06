#ifndef RECRAFT_COLLISION_H
#define RECRAFT_COLLISION_H
#include "world.h"
typedef struct WorldAabb { double min[3],max[3]; } WorldAabb;
double world_aabb_clip(const WorldAabb *moving,const WorldAabb *obstacle,int axis,double delta);
double world_clip_axis(World *world,const WorldAabb *body,int axis,double delta,int load);
#endif
