#ifndef RECRAFT_FIRE_H
#define RECRAFT_FIRE_H
#include "world.h"
int fire_encouragement(unsigned id);
int fire_burn_rate(unsigned id);
int world_fire_can_stay(const World *w,int x,int y,int z);
void world_fire_tick(World *w,int x,int y,int z);
void world_lava_ignite_tick(World *w,int x,int y,int z);
int world_ignite(World *w,int x,int y,int z);
int world_extinguish_fire(World *w,int x,int y,int z);
#endif
