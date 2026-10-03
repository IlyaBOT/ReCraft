#ifndef RECRAFT_FOLIAGE_H
#define RECRAFT_FOLIAGE_H
#include "world.h"
void world_foliage_removed(World *w,int x,int y,int z,unsigned old_id);
void world_leaf_tick(World *w,int x,int y,int z);
#endif
