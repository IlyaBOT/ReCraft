#ifndef RECRAFT_WORLD_ENTITIES_H
#define RECRAFT_WORLD_ENTITIES_H
#include "world.h"
#include "../nbt/nbt.h"
#include "mobs.h"
#include "transport.h"
struct Player;
typedef struct SavedEntity {
    struct SavedEntity *next;
    int item_entity;
    ItemDrop item;
    uint8_t *raw;
    size_t raw_size;
    uint64_t last_tick;
    MobState mob;
    TransportState transport;
} SavedEntity;
void world_entities_free(Chunk *chunk);
int world_entities_read(Chunk *chunk,const uint8_t *input,size_t size);
int world_entities_write_list(const Chunk *chunk,NbtWriter *writer);
int world_entities_rewrite(const Chunk *chunk,const uint8_t *input,size_t size,uint8_t **output,size_t *output_size);
int world_item_spawn(World *world,int x,int y,int z,InventorySlot item);
int world_item_spawn_at(World *world,float x,float y,float z,InventorySlot item);
void world_items_tick(World *world,const struct Player *player,InventorySlot *inventory);
int world_items_visible(const World *world,ItemDrop *out,int count);
void world_entities_collide(World *world,struct Player *player);
#endif
