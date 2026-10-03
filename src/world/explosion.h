#ifndef RECRAFT_EXPLOSION_H
#define RECRAFT_EXPLOSION_H
#include "world.h"
struct Player;
int world_tnt_prime(World *w,float x,float y,float z,int fuse);
float beta_blast_resistance(unsigned id);
void world_explode(World *w,struct Player *player,float x,float y,float z,float power,int flaming);
#endif
