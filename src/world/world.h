#ifndef RECRAFT_WORLD_H
#define RECRAFT_WORLD_H

#include <stddef.h>
#include <stdint.h>
#include "beta_blocks.h"

#ifdef __cplusplus
extern "C" {
#endif

#define WORLD_CHUNK_SIZE 16
#define WORLD_HEIGHT 128
#define WORLD_CHUNK_VOLUME (WORLD_CHUNK_SIZE * WORLD_CHUNK_SIZE * WORLD_HEIGHT)
#define WORLD_NIBBLE_BYTES (WORLD_CHUNK_VOLUME / 2)
#define WORLD_NAME_MAX 95
#define WORLD_ID_MAX 63
#define WORLD_PATH_MAX 512
#define WORLD_PHYSICS_QUEUE 4096
#define WORLD_DROP_QUEUE 128

typedef enum BlockId {
    BLOCK_AIR = BETA_BLOCK_AIR,
    BLOCK_STONE = BETA_BLOCK_STONE,
    BLOCK_DIRT = BETA_BLOCK_DIRT,
    BLOCK_GRASS = BETA_BLOCK_GRASS,
    BLOCK_SAND = BETA_BLOCK_SAND,
    BLOCK_GRAVEL = BETA_BLOCK_GRAVEL,
    BLOCK_COBBLESTONE = BETA_BLOCK_COBBLESTONE,
    BLOCK_WOOD = BETA_BLOCK_LOG,
    BLOCK_LEAVES = BETA_BLOCK_LEAVES,
    BLOCK_WATER = BETA_BLOCK_STILL_WATER,
    BLOCK_GLASS = BETA_BLOCK_GLASS,
    BLOCK_TORCH = BETA_BLOCK_TORCH,
    BLOCK_COUNT = BETA_BLOCK_COUNT
} BlockId;

typedef enum BlockRenderLayer {
    BLOCK_LAYER_NONE = 0,
    BLOCK_LAYER_OPAQUE = 1,
    BLOCK_LAYER_CUTOUT = 2,
    BLOCK_LAYER_TRANSPARENT = 3
} BlockRenderLayer;

typedef struct BlockDef {
    const char *name;
    uint8_t solid;
    uint8_t opaque;
    uint8_t render_layer;
    uint8_t texture_top;
    uint8_t texture_side;
    uint8_t texture_bottom;
    uint8_t emission;
} BlockDef;

const BlockDef *world_block_def(uint8_t id);

#define CHUNK_DIRTY_MESH 1u
#define CHUNK_DIRTY_SAVE 2u
#define CHUNK_DIRTY_LIGHT 4u
#define CHUNK_DIRTY_SMOOTH_MESH 8u

/* x + z * 16 + y * 256. Block bytes use Beta IDs; metadata is a nibble. */
typedef struct Chunk {
    int32_t x;
    int32_t z;
    uint8_t blocks[WORLD_CHUNK_VOLUME];
    uint8_t metadata[WORLD_NIBBLE_BYTES];
    uint8_t block_light[WORLD_NIBBLE_BYTES];
    uint8_t sky_light[WORLD_NIBBLE_BYTES];
    uint32_t dirty_flags;
    uint64_t last_used;
    uint32_t revision;
    void *render_data;
    uint8_t *beta_raw;
    size_t beta_raw_size;
    size_t beta_offsets[4];  /* Blocks, Data, BlockLight, SkyLight. */
    uint8_t beta_compression;
} Chunk;

typedef enum WorldError {
    WORLD_OK = 0,
    WORLD_ERROR_INVALID_ARGUMENT,
    WORLD_ERROR_OUT_OF_MEMORY,
    WORLD_ERROR_IO,
    WORLD_ERROR_CORRUPT,
    WORLD_ERROR_NOT_FOUND,
    WORLD_ERROR_EXISTS,
    WORLD_ERROR_PATH_TOO_LONG
} WorldError;

typedef struct WorldInfo {
    char id[WORLD_ID_MAX + 1];
    char name[WORLD_NAME_MAX + 1];
    uint64_t seed;
    uint64_t last_played;
    uint8_t flat;
    uint8_t creative;
    uint8_t structures;
} WorldInfo;

typedef struct WorldPhysicsCell {
    int32_t x, z;
    uint8_t y;
} WorldPhysicsCell;

typedef struct WorldDropEvent {
    int32_t x,z;
    uint8_t y,id;
} WorldDropEvent;

typedef struct World {
    uint64_t seed;
    uint64_t clock;
    uint8_t flat;
    uint8_t creative;
    uint8_t structures;
    uint8_t persistent;
    uint8_t network_mode; /* Allocate empty chunks until protocol data arrives. */
    uint8_t beta_format;   /* Original Beta 1.7.3 McRegion storage. */
    int64_t beta_world_time;
    int32_t spawn_x, spawn_y, spawn_z;
    int beta_has_player;
    double beta_player_x,beta_player_y,beta_player_z;
    float beta_player_yaw,beta_player_pitch;
    WorldError error;
    char path[WORLD_PATH_MAX];
    char id[WORLD_ID_MAX + 1];
    char name[WORLD_NAME_MAX + 1];
    Chunk **cache;
    Chunk **lookup;       /* Open-addressed lookup for hot meshing queries. */
    size_t lookup_capacity;
    Chunk *staging;
    size_t cache_count;
    size_t cache_capacity;
    void (*destroy_render_data)(void *);
    WorldError (*read_beta_chunk)(const struct World *, Chunk *);
    WorldError (*write_beta_chunk)(const struct World *, const Chunk *);
    WorldPhysicsCell physics[WORLD_PHYSICS_QUEUE];
    unsigned physics_head, physics_count;
    uint8_t physics_processing;
    WorldDropEvent drops[WORLD_DROP_QUEUE];
    unsigned drop_head,drop_count;
} World;

/* Cache capacity is in whole chunks; 64 chunks occupy about 5 MiB before meshes. */
WorldError world_init(World *world, uint64_t seed, int flat, size_t cache_capacity);
WorldError world_create(World *world, const char *saves_dir, const char *id,
                        const char *name, uint64_t seed, int flat, int creative, int structures,
                        size_t cache_capacity);
WorldError world_open(World *world, const char *saves_dir, const char *id,
                      size_t cache_capacity);
WorldError world_save(World *world);
WorldError world_close(World *world);

/* IDs are portable ASCII names: letters, digits, underscore and hyphen. */
int world_valid_id(const char *id);
size_t world_storage_list(const char *saves_dir, WorldInfo *out, size_t capacity);
WorldError world_storage_delete(const char *saves_dir, const char *id);
WorldError world_storage_rename(const char *saves_dir, const char *id, const char *name);

Chunk *world_get_chunk(World *world, int32_t cx, int32_t cz);
Chunk *world_peek_chunk(const World *world, int32_t cx, int32_t cz);
size_t world_cached_chunk_count(const World *world);
Chunk *world_cached_chunk_at(const World *world, size_t index);
size_t world_dirty_chunk_count(const World *world);
size_t world_memory_bytes(const World *world);
void world_touch_chunk(World *world, Chunk *chunk);
void world_set_render_data_destroy(World *world, void (*destroy)(void *));

uint8_t world_get_block(World *world, int wx, int y, int wz);
uint8_t world_peek_block(const World *world, int wx, int y, int wz);
int world_set_block(World *world, int wx, int y, int wz, uint8_t id);
uint8_t world_get_metadata(World *world, int wx, int y, int wz);
int world_set_metadata(World *world, int wx, int y, int wz, uint8_t value);
/* Bounded local block updates; authoritative multiplayer worlds skip these. */
void world_step_physics(World *world, unsigned max_updates);
int world_take_drop(World *world, WorldDropEvent *drop);
/* Rebuild local sky and block light once after a batch of chunk edits. */
void world_relight_chunk(World *world, Chunk *chunk);

uint8_t chunk_get_block(const Chunk *chunk, int x, int y, int z);
uint8_t chunk_get_metadata(const Chunk *chunk, int x, int y, int z);
uint8_t chunk_get_block_light(const Chunk *chunk, int x, int y, int z);
uint8_t chunk_get_sky_light(const Chunk *chunk, int x, int y, int z);
void chunk_set_block(Chunk *chunk, int x, int y, int z, uint8_t id);
void chunk_set_metadata(Chunk *chunk, int x, int y, int z, uint8_t value);
void chunk_set_block_light(Chunk *chunk, int x, int y, int z, uint8_t value);
void chunk_set_sky_light(Chunk *chunk, int x, int y, int z, uint8_t value);

/* Internal storage hooks are declared here for the two C99 translation units. */
WorldError world_storage_make(const char *saves_dir, const WorldInfo *info,
                              char *path, size_t path_capacity);
WorldError world_storage_open(const char *saves_dir, const char *id,
                              WorldInfo *info, char *path, size_t path_capacity);
WorldError world_storage_write_info(const World *world);
WorldError world_storage_read_chunk(const World *world, Chunk *chunk);
WorldError world_storage_write_chunk(const World *world, const Chunk *chunk);

#ifdef __cplusplus
}
#endif

#endif
