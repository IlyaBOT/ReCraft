#ifndef RECRAFT_RAIL_H
#define RECRAFT_RAIL_H
#include "world.h"
/* Vanilla EntityMinecart.MATRIX: endpoints relative to rail center. */
extern const int rail_ends[10][2][3];
int rail_is(unsigned id);
void rail_update(World *world,int x,int y,int z);
int rail_path(const World *world,float x,float y,float z,float *px,float *py,float *pz,int *shape);
#endif
