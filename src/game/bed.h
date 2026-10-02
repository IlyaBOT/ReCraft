#ifndef RECRAFT_BED_H
#define RECRAFT_BED_H
#include "player.h"
/* Local results: 0 success, 1 daytime, 2 too far/unsupported/invalid. */
int player_sleep(Player *player,World *world,int x,int y,int z);
/* Eye position and view in the same coordinate convention as Player. */
void player_eye(const Player *player,const World *world,float *x,float *y,float *z,float *yaw,float *pitch);
void player_wake(Player *player,World *world,int completed);
void player_sleep_tick(Player *player,World *world);
void player_respawn(Player *player,World *world);
int bed_place(World *world,int x,int y,int z,unsigned direction);
void bed_neighbor_tick(World *world,int x,int y,int z);
#endif
