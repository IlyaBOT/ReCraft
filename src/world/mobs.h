#ifndef RECRAFT_MOBS_H
#define RECRAFT_MOBS_H
#include "world.h"
#include "../nbt/nbt.h"
struct Player; struct SavedEntity; struct RenderEntity;
typedef struct MobState {
    int type,health,fire,on_ground,hurt_ticks,last_damage,attack_ticks,age,walk_ticks;
    float x,y,z,vx,vy,vz,yaw,pitch,walk,fall_distance;
    float push_x,push_z;
    int runtime_id,sound_ticks,color,sheared;
    int egg_ticks;
} MobState;
int mob_type(const void *name,size_t size);
int mob_default_health(int type);
int world_mob_spawn(World *world,int type,float x,float y,float z);
void world_mobs_tick(World *world,struct Player *player);
int world_mobs_attack(World *world,struct Player *player,InventorySlot *held,float reach);
int world_mobs_interact(World *world,struct Player *player,InventorySlot *held,float reach);
int world_mobs_visible(World *world,struct RenderEntity *out,int capacity);
int world_mob_write(NbtWriter *writer,const struct SavedEntity *entity);
int beta_attack_damage(int item);
int world_mob_hit(World *world,struct SavedEntity *entity,int amount);
#endif
