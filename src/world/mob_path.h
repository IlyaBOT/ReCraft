#ifndef RECRAFT_MOB_PATH_H
#define RECRAFT_MOB_PATH_H
#include "world.h"
#define MOB_PATH_POINTS 32
typedef struct MobPath { int x[MOB_PATH_POINTS],z[MOB_PATH_POINTS]; unsigned char y[MOB_PATH_POINTS]; int count,index,ticks; } MobPath;
int mob_path_find(const World *w,float width,float height,float x,float y,float z,float tx,float ty,float tz,MobPath *path);
#endif
