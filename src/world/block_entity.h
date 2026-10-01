#ifndef RECRAFT_BLOCK_ENTITY_H
#define RECRAFT_BLOCK_ENTITY_H
#include "../game/inventory.h"
#include <stddef.h>
#include <stdint.h>
struct World;
struct Chunk;

typedef enum BlockEntityKind { BLOCK_ENTITY_UNKNOWN, BLOCK_ENTITY_CHEST, BLOCK_ENTITY_FURNACE } BlockEntityKind;
typedef struct BlockEntity {
    struct BlockEntity *next;
    BlockEntityKind kind;
    int x,y,z;
    InventorySlot slots[27];
    int burn,cook,fuel;
    /* Preserve other NBT fields, including tile entities we do not simulate. */
    uint8_t *raw;
    size_t raw_size;
} BlockEntity;

BlockEntity *block_entity_get(struct World *world,int x,int y,int z,int create);
void block_entity_changed(struct World *world,BlockEntity *entity);
void block_entity_remove(struct World *world,int x,int y,int z,int drop_contents);
void block_entities_free(struct Chunk *chunk);
void block_entities_tick(struct World *world);
int block_entities_read(struct Chunk *chunk,const uint8_t *raw,size_t size);
int block_entities_rewrite(const struct Chunk *chunk,const uint8_t *input,size_t size,
                           uint8_t **output,size_t *output_size);
int block_entities_native_read(const struct World *world,struct Chunk *chunk);
int block_entities_native_write(const struct World *world,const struct Chunk *chunk);
/* Single or double chest, ordered north/west half first as in Beta. */
int block_chest_halves(struct World *world,int x,int y,int z,BlockEntity **first,BlockEntity **second);
int block_chest_can_place(struct World *world,int x,int y,int z);
int block_chest_texture(const struct World *world,int x,int y,int z,unsigned face);
#endif
