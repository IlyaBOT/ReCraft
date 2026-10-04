#ifndef RECRAFT_PISTON_H
#define RECRAFT_PISTON_H
#include "world.h"
void world_piston_changed(World *world,int x,int y,int z);
void world_piston_events(World *world);
void world_pistons_tick(World *world);
void world_piston_removed(World *world,int x,int y,int z,unsigned old,unsigned metadata,unsigned replacement);
int world_block_collision_boxes(const World *world,int x,int y,int z,BetaBlockBox boxes[2]);
#endif
