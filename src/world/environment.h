#ifndef RECRAFT_ENVIRONMENT_H
#define RECRAFT_ENVIRONMENT_H
#include "world.h"
/* Beta overworld clock and weather, advanced only by authoritative 20 Hz ticks. */
unsigned world_random(World *world,unsigned bound);
float world_celestial_angle(const World *world,float partial);
float world_daylight(const World *world,float partial);
void world_environment_refresh(World *world);
void world_environment_tick(World *world);
void world_environment_dawn(World *world);
int world_is_daytime(const World *world);
void world_precipitation_prepare(World *world,Chunk *chunk);
int world_precipitation_height(World *world,int x,int z,unsigned *kind);
#endif
