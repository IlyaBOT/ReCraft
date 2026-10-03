#ifndef RECRAFT_DOOR_H
#define RECRAFT_DOOR_H
#include "world.h"
int door_place(World *w,int x,int y,int z,unsigned id,float yaw);
int door_activate(World *w,int x,int y,int z);
void door_neighbor_tick(World *w,int x,int y,int z);
void door_power_changed(World *w,int x,int y,int z);
#endif
