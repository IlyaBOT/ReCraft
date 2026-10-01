#ifndef RECRAFT_WORLD_TICKS_H
#define RECRAFT_WORLD_TICKS_H
#include <stddef.h>
#include <stdint.h>
struct World;
struct Chunk;
struct WorldPhysicsCell;
struct NbtWriter;
/* A tick belongs to its chunk. Unsupported entries retain their original NBT;
 * supported entries retain extra fields while their relative delay changes. */
typedef struct SavedTick {
    struct SavedTick *next;
    int32_t x,y,z,id,delay;
    uint64_t due;
    uint8_t managed,resumed,queued;
    uint8_t *raw;
    size_t raw_size;
} SavedTick;
int world_ticks_supported(unsigned id);
int world_ticks_read(struct Chunk *chunk,const uint8_t *raw,size_t size);
void world_ticks_free(struct Chunk *chunk);
SavedTick *world_ticks_remember(struct World *world,struct Chunk *chunk,
                              int x,int y,int z,uint8_t id,unsigned delay);
void world_ticks_consume(struct World *world,const struct WorldPhysicsCell *cell);
void world_ticks_resume(struct World *world,struct Chunk *chunk);
void world_ticks_dirty_countdowns(struct World *world);
size_t world_ticks_capacity(const struct Chunk *chunk);
int world_ticks_write_list(const struct World *world,const struct Chunk *chunk,struct NbtWriter *writer);
int world_ticks_rewrite(const struct World *world,const struct Chunk *chunk,
                        const uint8_t *input,size_t size,uint8_t **output,size_t *output_size);
#endif
