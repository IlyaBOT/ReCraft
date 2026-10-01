#ifndef RECRAFT_FLUID_H
#define RECRAFT_FLUID_H
#include "world.h"
int fluid_kind(unsigned id); /* 1 water, 2 lava, 0 other. */
void fluid_tick(World *world,int x,int y,int z,unsigned id);
float fluid_corner_height(const World *world,int corner_x,int y,int corner_z,int kind);
void fluid_flow_vector(const World *world,int x,int y,int z,float out[3]);
#endif
