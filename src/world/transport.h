#ifndef RECRAFT_TRANSPORT_H
#define RECRAFT_TRANSPORT_H
#include "world.h"
#include "../nbt/nbt.h"
struct Player; struct SavedEntity; struct RenderEntity;
typedef struct TransportState {
    int kind,dead; /* 1 Arrow, 2 Minecart; position/motion live in SavedEntity.mob. */
    int x_tile,y_tile,z_tile,in_tile,in_data,in_ground,shake,player,ground_ticks,air_ticks;
    int type,fuel,ridden,damage,hit_ticks;
    float push_x,push_z;
    float previous_x,previous_y,previous_z,slope_pitch;
    InventorySlot cargo[27];
} TransportState;
int world_bow_use(World *world,const struct Player *player,InventorySlot *inventory);
int world_minecart_spawn(World *world,float x,float y,float z,int type);
void world_transport_tick(World *world,struct Player *player,InventorySlot *inventory);
int world_transport_interact(World *world,struct Player *player,InventorySlot *held,int attack);
int world_transport_visible(World *world,struct RenderEntity *out,int capacity);
int world_transport_write(NbtWriter *writer,const struct SavedEntity *entity);
void world_minecart_dismount(World *world,struct Player *player);
struct SavedEntity *world_minecart_find(World *world,int runtime_id);
void world_transport_changed(World *world,struct SavedEntity *entity);
#endif
