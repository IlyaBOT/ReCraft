#ifndef RECRAFT_MECHANISMS_H
#define RECRAFT_MECHANISMS_H
#include "world.h"
struct Player;
void world_mechanisms_tick(World *world,struct Player *player);
void world_plate_update(World *world,int x,int y,int z);
void world_dispenser_tick(World *world,int x,int y,int z);
int world_dispenser_powered(const World *world,int x,int y,int z);
#endif
