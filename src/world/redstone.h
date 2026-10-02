#ifndef RECRAFT_REDSTONE_H
#define RECRAFT_REDSTONE_H
#include "world.h"
int world_redstone_power(const World *w,int x,int y,int z,int ex,int ey,int ez,int wire);
int world_redstone_signal(const World *w,int x,int y,int z,int tx,int ty,int tz,int wire);
int world_repeater_input(const World *w,int x,int y,int z,unsigned metadata);
int world_redstone_activate(World *w,int x,int y,int z);
#endif
