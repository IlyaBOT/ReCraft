#include "world.h"
#include "block_entity.h"
#include "entities.h"
#include "ticks.h"
#include "beta_session.h"

#include <stdlib.h>
#include <string.h>

#define WATER_LEVEL 62

/* Tile IDs are compact slots in the renderer's fixed-function terrain atlas. */
static const BlockDef block_defs[BLOCK_COUNT] = {
    [BLOCK_AIR] = {"Air",         0, 0, BLOCK_LAYER_NONE,        0,  0,  0,  0},
    [BLOCK_STONE] = {"Stone",       1, 1, BLOCK_LAYER_OPAQUE,      1,  1,  1,  0},
    [BLOCK_DIRT] = {"Dirt",        1, 1, BLOCK_LAYER_OPAQUE,      2,  2,  2,  0},
    [BLOCK_GRASS] = {"Grass",       1, 1, BLOCK_LAYER_OPAQUE,      3,  4,  2,  0},
    [BLOCK_SAND] = {"Sand",        1, 1, BLOCK_LAYER_OPAQUE,      5,  5,  5,  0},
    [BLOCK_GRAVEL] = {"Gravel",      1, 1, BLOCK_LAYER_OPAQUE,      6,  6,  6,  0},
    [BLOCK_COBBLESTONE] = {"Cobblestone", 1, 1, BLOCK_LAYER_OPAQUE,      7,  7,  7,  0},
    [BLOCK_WOOD] = {"Wood",        1, 1, BLOCK_LAYER_OPAQUE,      8,  9,  8,  0},
    [BLOCK_LEAVES] = {"Leaves",      1, 0, BLOCK_LAYER_CUTOUT,     10, 10, 10,  0},
    [BLOCK_WATER] = {"Water",       0, 0, BLOCK_LAYER_TRANSPARENT,11, 11, 11,  0},
    [BETA_BLOCK_FLOWING_WATER] = {"Water", 0, 0, BLOCK_LAYER_TRANSPARENT,11,11,11,0},
    [BLOCK_GLASS] = {"Glass",       1, 0, BLOCK_LAYER_CUTOUT,     12, 12, 12,  0},
    [BLOCK_TORCH] = {"Torch",       0, 0, BLOCK_LAYER_CUTOUT,     13, 13, 13, 14},
    [BETA_BLOCK_PLANKS] = {"Planks", 1, 1, BLOCK_LAYER_OPAQUE, 17, 17, 17, 0},
    [BETA_BLOCK_WOOL] = {"Wool", 1, 1, BLOCK_LAYER_OPAQUE, 18, 18, 18, 0},
    [BETA_BLOCK_BEDROCK] = {"Bedrock", 1, 1, BLOCK_LAYER_OPAQUE, 34,34,34,0},
    [BETA_BLOCK_GOLD_ORE] = {"Gold ore", 1, 1, BLOCK_LAYER_OPAQUE, 35,35,35,0},
    [BETA_BLOCK_IRON_ORE] = {"Iron ore", 1, 1, BLOCK_LAYER_OPAQUE, 36,36,36,0},
    [BETA_BLOCK_COAL_ORE] = {"Coal ore", 1, 1, BLOCK_LAYER_OPAQUE, 37,37,37,0},
    [BETA_BLOCK_LAPIS_ORE] = {"Lapis ore", 1, 1, BLOCK_LAYER_OPAQUE, 38,38,38,0},
    [BETA_BLOCK_LAPIS_BLOCK] = {"Lapis block", 1, 1, BLOCK_LAYER_OPAQUE, 39,39,39,0},
    [BETA_BLOCK_BRICKS] = {"Bricks", 1, 1, BLOCK_LAYER_OPAQUE, 40,40,40,0},
    [BETA_BLOCK_MOSSY_COBBLESTONE] = {"Mossy cobblestone",1,1,BLOCK_LAYER_OPAQUE,41,41,41,0},
    [BETA_BLOCK_OBSIDIAN] = {"Obsidian",1,1,BLOCK_LAYER_OPAQUE,42,42,42,0},
    [BETA_BLOCK_DIAMOND_ORE] = {"Diamond ore",1,1,BLOCK_LAYER_OPAQUE,43,43,43,0},
    [BETA_BLOCK_REDSTONE_ORE] = {"Redstone ore",1,1,BLOCK_LAYER_OPAQUE,44,44,44,0},
    [BETA_BLOCK_GLOWING_REDSTONE_ORE] = {"Lit redstone ore",1,1,BLOCK_LAYER_OPAQUE,44,44,44,9},
    [BETA_BLOCK_CLAY] = {"Clay",1,1,BLOCK_LAYER_OPAQUE,45,45,45,0},
    [BETA_BLOCK_NETHERRACK] = {"Netherrack",1,1,BLOCK_LAYER_OPAQUE,46,46,46,0},
    [BETA_BLOCK_SOUL_SAND] = {"Soul sand",1,1,BLOCK_LAYER_OPAQUE,47,47,47,0},
    [BETA_BLOCK_GLOWSTONE] = {"Glowstone",1,1,BLOCK_LAYER_OPAQUE,48,48,48,15},
    [BETA_BLOCK_SAPLING] = {"Sapling",0,0,BLOCK_LAYER_CUTOUT,49,49,49,0},
    [BETA_BLOCK_TALL_GRASS] = {"Tall grass",0,0,BLOCK_LAYER_CUTOUT,52,52,52,0},
    [BETA_BLOCK_DEAD_BUSH] = {"Dead bush",0,0,BLOCK_LAYER_CUTOUT,53,53,53,0},
    [BETA_BLOCK_DANDELION] = {"Dandelion",0,0,BLOCK_LAYER_CUTOUT,55,55,55,0},
    [BETA_BLOCK_ROSE] = {"Rose",0,0,BLOCK_LAYER_CUTOUT,56,56,56,0},
    [BETA_BLOCK_BROWN_MUSHROOM] = {"Brown mushroom",0,0,BLOCK_LAYER_CUTOUT,57,57,57,1},
    [BETA_BLOCK_RED_MUSHROOM] = {"Red mushroom",0,0,BLOCK_LAYER_CUTOUT,58,58,58,0},
    [BETA_BLOCK_REEDS] = {"Reeds",0,0,BLOCK_LAYER_CUTOUT,59,59,59,0},
    [BETA_BLOCK_DOUBLE_SLAB] = {"Double slab",1,1,BLOCK_LAYER_OPAQUE,0,60,0,0},
    [BETA_BLOCK_SLAB] = {"Slab",1,0,BLOCK_LAYER_OPAQUE,0,60,0,0},
    [BETA_BLOCK_CRAFTING_TABLE] = {"Crafting Table",1,1,BLOCK_LAYER_OPAQUE,70,71,17,0},
    [BETA_BLOCK_CHEST] = {"Chest",1,1,BLOCK_LAYER_OPAQUE,73,74,73,0},
    [BETA_BLOCK_FURNACE] = {"Furnace",1,1,BLOCK_LAYER_OPAQUE,76,77,76,0},
    [BETA_BLOCK_BURNING_FURNACE] = {"Furnace",1,1,BLOCK_LAYER_OPAQUE,76,77,76,13},
    [BETA_BLOCK_REDSTONE_TORCH] = {"Redstone Torch",0,0,BLOCK_LAYER_CUTOUT,80,80,80,7},
    [BETA_BLOCK_UNLIT_REDSTONE_TORCH] = {"Redstone Torch",0,0,BLOCK_LAYER_CUTOUT,81,81,81,0},
    [BETA_BLOCK_CACTUS] = {"Cactus",1,0,BLOCK_LAYER_OPAQUE,67,68,69,0},
    [BETA_BLOCK_NETHER_PORTAL] = {"Nether Portal",0,0,BLOCK_LAYER_TRANSPARENT,66,66,66,11},
    [BETA_BLOCK_FLOWING_LAVA] = {"Lava",0,0,BLOCK_LAYER_OPAQUE,64,65,64,15},
    [BETA_BLOCK_STILL_LAVA] = {"Lava",0,0,BLOCK_LAYER_OPAQUE,64,65,64,15},
    [BETA_BLOCK_REDSTONE_WIRE] = {"Redstone",0,0,BLOCK_LAYER_CUTOUT,82,82,82,0},
    [BETA_BLOCK_BED] = {"Bed",1,0,BLOCK_LAYER_CUTOUT,89,92,17,0},
    [BETA_BLOCK_UNPOWERED_REPEATER] = {"Redstone Repeater",1,0,BLOCK_LAYER_OPAQUE,95,60,17,0},
    [BETA_BLOCK_POWERED_REPEATER] = {"Redstone Repeater",1,0,BLOCK_LAYER_OPAQUE,96,60,17,0},
    [BETA_BLOCK_LEVER] = {"Lever",0,0,BLOCK_LAYER_CUTOUT,7,7,7,0},
    [BETA_BLOCK_STONE_BUTTON] = {"Stone Button",0,0,BLOCK_LAYER_CUTOUT,1,1,1,0},
    [BETA_BLOCK_SNOW_LAYER] = {"Snow",0,0,BLOCK_LAYER_OPAQUE,97,97,97,0},
    [BETA_BLOCK_SNOW_BLOCK] = {"Snow Block",1,1,BLOCK_LAYER_OPAQUE,97,97,97,0},
    [BETA_BLOCK_ICE] = {"Ice",1,0,BLOCK_LAYER_TRANSPARENT,98,98,98,0},
    [BETA_BLOCK_WOOD_DOOR] = {"Wooden Door",1,0,BLOCK_LAYER_CUTOUT,103,103,103,0},
    [BETA_BLOCK_IRON_DOOR] = {"Iron Door",1,0,BLOCK_LAYER_CUTOUT,105,105,105,0},
    [BETA_BLOCK_WEB] = {"Cobweb",0,0,BLOCK_LAYER_CUTOUT,101,101,101,0},
    [BETA_BLOCK_STANDING_SIGN] = {"Sign",0,0,BLOCK_LAYER_NONE,0,0,0,0},
    [BETA_BLOCK_WALL_SIGN] = {"Sign",0,0,BLOCK_LAYER_NONE,0,0,0,0}
};

static const BlockDef unknown_solid = {"Unimplemented solid",1,1,BLOCK_LAYER_OPAQUE,1,1,1,0};
static const BlockDef unknown_empty = {"Unimplemented non-solid",0,0,BLOCK_LAYER_NONE,0,0,0,0};

static int beta_empty_proxy(uint8_t id)
{
    switch (id) {
    case BETA_BLOCK_SAPLING: case BETA_BLOCK_POWERED_RAIL: case BETA_BLOCK_DETECTOR_RAIL:
    case BETA_BLOCK_WEB: case BETA_BLOCK_TALL_GRASS: case BETA_BLOCK_DEAD_BUSH:
    case BETA_BLOCK_DANDELION: case BETA_BLOCK_ROSE: case BETA_BLOCK_BROWN_MUSHROOM:
    case BETA_BLOCK_RED_MUSHROOM: case BETA_BLOCK_FIRE: case BETA_BLOCK_REDSTONE_WIRE:
    case BETA_BLOCK_CROPS: case BETA_BLOCK_STANDING_SIGN: case BETA_BLOCK_LADDER:
    case BETA_BLOCK_RAIL: case BETA_BLOCK_WALL_SIGN: case BETA_BLOCK_LEVER:
    case BETA_BLOCK_STONE_PRESSURE_PLATE: case BETA_BLOCK_WOOD_PRESSURE_PLATE:
    case BETA_BLOCK_UNLIT_REDSTONE_TORCH: case BETA_BLOCK_REDSTONE_TORCH:
    case BETA_BLOCK_STONE_BUTTON: case BETA_BLOCK_SNOW_LAYER: case BETA_BLOCK_REEDS:
    case BETA_BLOCK_NETHER_PORTAL:
        return 1;
    default: return 0;
    }
}

const BlockDef *world_block_def(uint8_t id)
{
    if (id >= BLOCK_COUNT) return &unknown_solid;
    if (block_defs[id].name) return &block_defs[id];
    return beta_empty_proxy(id) ? &unknown_empty : &unknown_solid;
}

static int blocks_light(uint8_t id)
{
    /* ys in the Beta client is non-opaque for face culling but calls g(255):
     * the half slab blocks light despite its open upper half. */
    return world_block_def(id)->opaque || id == BETA_BLOCK_SLAB;
}

static int valid_local(int x, int y, int z)
{
    return (unsigned)x < WORLD_CHUNK_SIZE &&
           (unsigned)z < WORLD_CHUNK_SIZE &&
           (unsigned)y < WORLD_HEIGHT;
}

static size_t block_index(int x, int y, int z)
{
    return (size_t)x + (size_t)z * WORLD_CHUNK_SIZE +
           (size_t)y * WORLD_CHUNK_SIZE * WORLD_CHUNK_SIZE;
}

static uint8_t nibble_get(const uint8_t *data, size_t index)
{
    uint8_t byte = data[index >> 1];
    return (uint8_t)((index & 1u) ? (byte >> 4) : (byte & 15u));
}

static void nibble_set(uint8_t *data, size_t index, uint8_t value)
{
    uint8_t *byte = &data[index >> 1];
    if (index & 1u) *byte = (uint8_t)((*byte & 15u) | ((value & 15u) << 4));
    else *byte = (uint8_t)((*byte & 240u) | (value & 15u));
}

uint8_t chunk_get_block(const Chunk *chunk, int x, int y, int z)
{
    return chunk && valid_local(x, y, z) ? chunk->blocks[block_index(x,y,z)] : BLOCK_AIR;
}

uint8_t chunk_get_metadata(const Chunk *chunk, int x, int y, int z)
{
    return chunk && valid_local(x, y, z) ? nibble_get(chunk->metadata, block_index(x,y,z)) : 0;
}

uint8_t chunk_get_block_light(const Chunk *chunk, int x, int y, int z)
{
    return chunk && valid_local(x, y, z) ? nibble_get(chunk->block_light, block_index(x,y,z)) : 0;
}

uint8_t chunk_get_sky_light(const Chunk *chunk, int x, int y, int z)
{
    return chunk && valid_local(x, y, z) ? nibble_get(chunk->sky_light, block_index(x,y,z)) : 0;
}

void chunk_set_block(Chunk *chunk, int x, int y, int z, uint8_t id)
{
    size_t index;
    if (!chunk || !valid_local(x,y,z) || id >= BLOCK_COUNT) return;
    index = block_index(x,y,z);
    if (chunk->blocks[index] == id) return;
    chunk->blocks[index] = id;
    chunk->dirty_flags |= CHUNK_DIRTY_MESH | CHUNK_DIRTY_SAVE;
    ++chunk->revision;
}

void chunk_set_metadata(Chunk *chunk, int x, int y, int z, uint8_t value)
{
    size_t index;
    if (!chunk || !valid_local(x,y,z)) return;
    index = block_index(x,y,z);
    value &= 15u;
    if (nibble_get(chunk->metadata,index) == value) return;
    nibble_set(chunk->metadata,index,value);
    chunk->dirty_flags |= CHUNK_DIRTY_MESH | CHUNK_DIRTY_SAVE;
    ++chunk->revision;
}

void chunk_set_block_light(Chunk *chunk, int x, int y, int z, uint8_t value)
{
    size_t index;
    if (!chunk || !valid_local(x,y,z)) return;
    index = block_index(x,y,z);
    value &= 15u;
    if (nibble_get(chunk->block_light,index) == value) return;
    nibble_set(chunk->block_light,index,value);
    chunk->dirty_flags |= CHUNK_DIRTY_MESH | CHUNK_DIRTY_SAVE;
    ++chunk->revision;
}

void chunk_set_sky_light(Chunk *chunk, int x, int y, int z, uint8_t value)
{
    size_t index;
    if (!chunk || !valid_local(x,y,z)) return;
    index = block_index(x,y,z);
    value &= 15u;
    if (nibble_get(chunk->sky_light,index) == value) return;
    nibble_set(chunk->sky_light,index,value);
    chunk->dirty_flags |= CHUNK_DIRTY_MESH | CHUNK_DIRTY_SAVE;
    ++chunk->revision;
}

static int64_t floor_div(int64_t x, int64_t divisor)
{
    int64_t q = x / divisor;
    if (x % divisor < 0) --q;
    return q;
}

static int local_from_world(int world_coord, int32_t *chunk_coord)
{
    int64_t chunk = floor_div(world_coord,WORLD_CHUNK_SIZE);
    *chunk_coord = (int32_t)chunk;
    return (int)((int64_t)world_coord - chunk * WORLD_CHUNK_SIZE);
}

static uint32_t mix32(uint32_t value)
{
    value ^= value >> 16;
    value *= UINT32_C(0x7feb352d);
    value ^= value >> 15;
    value *= UINT32_C(0x846ca68b);
    value ^= value >> 16;
    return value;
}

static uint32_t hash2(uint64_t seed, int64_t x, int64_t z)
{
    uint32_t h = (uint32_t)seed ^ mix32((uint32_t)(seed >> 32));
    h ^= mix32((uint32_t)x * UINT32_C(0x9e3779b9));
    h ^= mix32((uint32_t)z * UINT32_C(0x85ebca6b));
    return mix32(h);
}

static uint32_t hash3(uint64_t seed, int64_t x, int64_t y, int64_t z)
{
    uint32_t h = hash2(seed,x,z);
    return mix32(h ^ mix32((uint32_t)y * UINT32_C(0xc2b2ae35)));
}

static int smooth_step(int t, int scale)
{
    /* Integer smoothstep; value remains in [0, scale]. */
    return t * t * (3 * scale - 2 * t) / (scale * scale);
}

static int lerp_int(int a, int b, int t, int scale)
{
    return a + (b - a) * t / scale;
}

static int noise2(uint64_t seed, int64_t x, int64_t z, int scale)
{
    int64_t gx = floor_div(x,scale), gz = floor_div(z,scale);
    int tx = smooth_step((int)(x - gx * scale),scale);
    int tz = smooth_step((int)(z - gz * scale),scale);
    int a = (int)(hash2(seed,gx,gz) & 65535u);
    int b = (int)(hash2(seed,gx+1,gz) & 65535u);
    int c = (int)(hash2(seed,gx,gz+1) & 65535u);
    int d = (int)(hash2(seed,gx+1,gz+1) & 65535u);
    return lerp_int(lerp_int(a,b,tx,scale),lerp_int(c,d,tx,scale),tz,scale);
}

static int noise3(uint64_t seed, int64_t x, int64_t y, int64_t z, int scale)
{
    int64_t gx = floor_div(x,scale), gy = floor_div(y,scale), gz = floor_div(z,scale);
    int tx = smooth_step((int)(x - gx*scale),scale);
    int ty = smooth_step((int)(y - gy*scale),scale);
    int tz = smooth_step((int)(z - gz*scale),scale);
    int a = lerp_int((int)(hash3(seed,gx,gy,gz)&65535u),
                     (int)(hash3(seed,gx+1,gy,gz)&65535u),tx,scale);
    int b = lerp_int((int)(hash3(seed,gx,gy+1,gz)&65535u),
                     (int)(hash3(seed,gx+1,gy+1,gz)&65535u),tx,scale);
    int c = lerp_int((int)(hash3(seed,gx,gy,gz+1)&65535u),
                     (int)(hash3(seed,gx+1,gy,gz+1)&65535u),tx,scale);
    int d = lerp_int((int)(hash3(seed,gx,gy+1,gz+1)&65535u),
                     (int)(hash3(seed,gx+1,gy+1,gz+1)&65535u),tx,scale);
    return lerp_int(lerp_int(a,b,ty,scale),lerp_int(c,d,ty,scale),tz,scale);
}

static int terrain_height(const World *world, int64_t wx, int64_t wz)
{
    int height;
    if (world->flat) return 63;
    height = 62 + (noise2(world->seed,wx,wz,64)-32768)*30/32768
                + (noise2(world->seed ^ UINT64_C(0x517cc1b727220a95),wx,wz,16)-32768)*9/32768;
    if (height < 20) height = 20;
    if (height > WORLD_HEIGHT-12) height = WORLD_HEIGHT-12;
    return height;
}

static void relight_sky_column(Chunk *chunk, int x, int z)
{
    int y;
    uint8_t light = 15;
    for (y = WORLD_HEIGHT-1; y >= 0; --y) {
        size_t index = block_index(x,y,z);
        uint8_t id = chunk->blocks[index];
        if (blocks_light(id)) light = 0;
        else if (id == BLOCK_WATER || id == BETA_BLOCK_FLOWING_WATER)
            light = light > 1 ? (uint8_t)(light-2) : 0;
        else if (id == BLOCK_LEAVES && light) --light;
        nibble_set(chunk->sky_light,index,light);
    }
}

static void relight_sky(Chunk *chunk)
{
    int x,z;
    memset(chunk->sky_light,0,sizeof(chunk->sky_light));
    for (z=0; z<WORLD_CHUNK_SIZE; ++z)
        for (x=0; x<WORLD_CHUNK_SIZE; ++x)
            relight_sky_column(chunk,x,z);
}

static void queue_light(Chunk *chunk, size_t index, uint8_t value,
                        uint8_t *queued, uint16_t *queue,
                        size_t *tail, size_t *count)
{
    if (blocks_light(chunk->blocks[index])) return;
    if (nibble_get(chunk->block_light,index) >= value) return;
    nibble_set(chunk->block_light,index,value);
    if (!queued[index]) {
        queued[index] = 1;
        queue[*tail] = (uint16_t)index;
        *tail = (*tail + 1) % WORLD_CHUNK_VOLUME;
        ++*count;
    }
}

static void mark_mesh_neighbors(World *world,int32_t x,int32_t z);

static void relight_block(World *world, Chunk *chunk)
{
    static const int dx[4]={-1,1,0,0}, dz[4]={0,0,-1,1};
    uint8_t previous[WORLD_NIBBLE_BYTES];
    uint8_t queued[WORLD_CHUNK_VOLUME];
    uint16_t queue[WORLD_CHUNK_VOLUME];
    size_t head=0, tail=0, count=0, index;
    int side,y,t;
    memcpy(previous,chunk->block_light,sizeof(previous));
    memset(chunk->block_light,0,sizeof(chunk->block_light));
    memset(queued,0,sizeof(queued));
    for (index=0; index<WORLD_CHUNK_VOLUME; ++index) {
        uint8_t value = world_block_def(chunk->blocks[index])->emission;
        if (value) {
            nibble_set(chunk->block_light,index,value);
            queued[index]=1;
            queue[tail]=(uint16_t)index;
            tail=(tail+1)%WORLD_CHUNK_VOLUME;
            ++count;
        }
    }
    /* Loaded neighbours are boundary conditions, never load chunks here.
     * Every crossing loses at least one light level. Repeating changed edges
     * therefore also removes stale light after deleting an emitter. */
    for (side=0;side<4;++side) {
        Chunk *neighbor=world_peek_chunk(world,chunk->x+dx[side],chunk->z+dz[side]);
        if (!neighbor) continue;
        for (y=0;y<WORLD_HEIGHT;++y) for (t=0;t<16;++t) {
            int x=side==0?0:side==1?15:t;
            int z=side==2?0:side==3?15:t;
            int nx=side==0?15:side==1?0:t;
            int nz=side==2?15:side==3?0:t;
            uint8_t value=chunk_get_block_light(neighbor,nx,y,nz);
            if (value>1) queue_light(chunk,block_index(x,y,z),(uint8_t)(value-1),
                                     queued,queue,&tail,&count);
        }
    }
    while (count) {
        int x,z;
        uint8_t current;
        index=queue[head];
        head=(head+1)%WORLD_CHUNK_VOLUME;
        --count;
        queued[index]=0;
        current=nibble_get(chunk->block_light,index);
        if (current <= 1) continue;
        y=(int)(index >> 8);
        z=(int)((index >> 4)&15u);
        x=(int)(index&15u);
        --current;
        if (x>0) queue_light(chunk,index-1,current,queued,queue,&tail,&count);
        if (x<15) queue_light(chunk,index+1,current,queued,queue,&tail,&count);
        if (z>0) queue_light(chunk,index-16,current,queued,queue,&tail,&count);
        if (z<15) queue_light(chunk,index+16,current,queued,queue,&tail,&count);
        if (y>0) queue_light(chunk,index-256,current,queued,queue,&tail,&count);
        if (y<127) queue_light(chunk,index+256,current,queued,queue,&tail,&count);
    }
    if (!memcmp(previous,chunk->block_light,sizeof(previous))) return;
    chunk->dirty_flags|=CHUNK_DIRTY_MESH;
    if (!world->beta_format || chunk->beta_raw) chunk->dirty_flags|=CHUNK_DIRTY_SAVE;
    ++chunk->revision;
    mark_mesh_neighbors(world,chunk->x,chunk->z);
    for (side=0;side<4;++side) {
        Chunk *neighbor=world_peek_chunk(world,chunk->x+dx[side],chunk->z+dz[side]);
        int changed=0;
        if (!neighbor) continue;
        for (y=0;y<WORLD_HEIGHT && !changed;++y) for (t=0;t<16;++t) {
            index=block_index(side==0?0:side==1?15:t,y,side==2?0:side==3?15:t);
            if (nibble_get(previous,index)!=nibble_get(chunk->block_light,index)) {
                changed=1; break;
            }
        }
        if (changed) neighbor->dirty_flags|=CHUNK_DIRTY_LIGHT|CHUNK_DIRTY_MESH;
    }
}

static void flush_block_light(World *world)
{
    size_t i;
    int pending;
    do {
        pending=0;
        for (i=0;i<world->cache_count;++i) {
            Chunk *chunk=world->cache[i];
            if (!(chunk->dirty_flags&CHUNK_DIRTY_LIGHT)) continue;
            chunk->dirty_flags&=~CHUNK_DIRTY_LIGHT;
            relight_block(world,chunk);
            pending=1;
        }
    } while (pending);
}

static void put_generated(Chunk *chunk, int64_t wx, int y, int64_t wz, uint8_t id)
{
    int64_t left=(int64_t)chunk->x*WORLD_CHUNK_SIZE;
    int64_t front=(int64_t)chunk->z*WORLD_CHUNK_SIZE;
    if (wx>=left && wx<left+WORLD_CHUNK_SIZE &&
        wz>=front && wz<front+WORLD_CHUNK_SIZE &&
        y>=0 && y<WORLD_HEIGHT) {
        chunk->blocks[block_index((int)(wx-left),y,(int)(wz-front))]=id;
    }
}

static void generate_trees(const World *world, Chunk *chunk)
{
    int64_t left=(int64_t)chunk->x*WORLD_CHUNK_SIZE;
    int64_t front=(int64_t)chunk->z*WORLD_CHUNK_SIZE;
    int64_t gx,gz;
    for (gz=floor_div(front-4,6); gz<=floor_div(front+19,6); ++gz) {
        for (gx=floor_div(left-4,6); gx<=floor_div(left+19,6); ++gx) {
            uint32_t h=hash2(world->seed ^ UINT64_C(0xd6e8feb86659fd93),gx,gz);
            int64_t tx,tz;
            int ground,trunk_height,top,dx,dz,dy;
            if (h%7u != 0u) continue;
            tx=gx*6+1+(int)((h>>8)&3u);
            tz=gz*6+1+(int)((h>>10)&3u);
            if (tx<left-2 || tx>left+17 || tz<front-2 || tz>front+17) continue;
            ground=terrain_height(world,tx,tz);
            if (ground<=WATER_LEVEL+1 || ground>WORLD_HEIGHT-9) continue;
            trunk_height=4+(int)((h>>16)&1u);
            top=ground+trunk_height;
            for (dy=1; dy<=trunk_height; ++dy)
                put_generated(chunk,tx,ground+dy,tz,BLOCK_WOOD);
            for (dy=-2; dy<=1; ++dy) {
                for (dz=-2; dz<=2; ++dz) {
                    for (dx=-2; dx<=2; ++dx) {
                        int yy=top+dy;
                        int64_t xx=tx+dx, zz=tz+dz;
                        int64_t lx=xx-left, lz=zz-front;
                        if (dy==1 && (abs(dx)==2 || abs(dz)==2)) continue;
                        if (abs(dx)==2 && abs(dz)==2 && ((h>>(dy+9))&1u)) continue;
                        if (lx<0 || lx>=16 || lz<0 || lz>=16 || yy<0 || yy>=WORLD_HEIGHT) continue;
                        if (chunk->blocks[block_index((int)lx,yy,(int)lz)]==BLOCK_AIR)
                            put_generated(chunk,xx,yy,zz,BLOCK_LEAVES);
                    }
                }
            }
        }
    }
}

static void generate_chunk(const World *world, Chunk *chunk, int32_t cx, int32_t cz)
{
    int x,z,y;
    memset(chunk,0,sizeof(*chunk));
    chunk->x=cx;
    chunk->z=cz;
    for (z=0; z<WORLD_CHUNK_SIZE; ++z) {
        for (x=0; x<WORLD_CHUNK_SIZE; ++x) {
            int64_t wx=(int64_t)cx*WORLD_CHUNK_SIZE+x;
            int64_t wz=(int64_t)cz*WORLD_CHUNK_SIZE+z;
            int height=terrain_height(world,wx,wz);
            for (y=0; y<WORLD_HEIGHT; ++y) {
                uint8_t id=BLOCK_AIR;
                if (y<=height) {
                    if (world->flat) id = y==height ? BLOCK_GRASS : y>=height-3 ? BLOCK_DIRT : BLOCK_STONE;
                    else if (y==height) id = height<=WATER_LEVEL+2 ? BLOCK_SAND : BLOCK_GRASS;
                    else if (y>=height-3) id = height<=WATER_LEVEL+2 ? BLOCK_SAND : BLOCK_DIRT;
                    else id=BLOCK_STONE;
                    if (!world->flat && y>9 && y<height-4 &&
                        noise3(world->seed ^ UINT64_C(0x9e3779b97f4a7c15),wx,y,wz,8)>51500)
                        id=BLOCK_AIR;
                } else if (!world->flat && y<=WATER_LEVEL) id=BLOCK_WATER;
                chunk->blocks[block_index(x,y,z)]=id;
            }
        }
    }
    if (!world->flat && world->structures) generate_trees(world,chunk);
    relight_sky(chunk);
    /* Newly generated terrain has no block-light sources. */
    chunk->dirty_flags=CHUNK_DIRTY_MESH|CHUNK_DIRTY_SAVE;
    chunk->revision=1;
}

WorldError world_init(World *world, uint64_t seed, int flat, size_t cache_capacity)
{
    size_t lookup_capacity;
    if (!world) return WORLD_ERROR_INVALID_ARGUMENT;
    memset(world,0,sizeof(*world));
    if (!cache_capacity) cache_capacity=64;
    if (cache_capacity>4096 || cache_capacity>SIZE_MAX/sizeof(Chunk *))
        return world->error=WORLD_ERROR_INVALID_ARGUMENT;
    world->cache=(Chunk **)calloc(cache_capacity,sizeof(Chunk *));
    if (!world->cache) return world->error=WORLD_ERROR_OUT_OF_MEMORY;
    lookup_capacity=16;
    while (lookup_capacity<cache_capacity*2u) lookup_capacity*=2u;
    world->lookup=(Chunk **)calloc(lookup_capacity,sizeof(Chunk *));
    if (!world->lookup) {
        free(world->cache);
        world->cache=NULL;
        return world->error=WORLD_ERROR_OUT_OF_MEMORY;
    }
    world->lookup_capacity=lookup_capacity;
    world->cache_capacity=cache_capacity;
    world->seed=seed;
    world->random_seed=(seed^UINT64_C(0x5deece66d))&UINT64_C(0xffffffffffff);
    world->random_tick=(uint32_t)seed;
    world->flat=(uint8_t)(flat != 0);
    world->structures=1;
    world->difficulty=2;
    return WORLD_OK;
}

WorldError world_create(World *world, const char *saves_dir, const char *id,
                        const char *name, uint64_t seed, int flat, int creative, int structures,
                        size_t cache_capacity)
{
    WorldInfo info;
    WorldError err;
    if (!world || !saves_dir || !id || !name || !world_valid_id(id) ||
        !name[0] || strlen(name)>WORLD_NAME_MAX) return WORLD_ERROR_INVALID_ARGUMENT;
    err=world_init(world,seed,flat,cache_capacity);
    if (err!=WORLD_OK) return err;
    memset(&info,0,sizeof(info));
    strcpy(info.id,id);
    strcpy(info.name,name);
    info.seed=seed;
    info.flat=(uint8_t)(flat!=0);
    info.creative=(uint8_t)(creative!=0);
    info.structures=(uint8_t)(structures!=0);
    err=world_storage_make(saves_dir,&info,world->path,sizeof(world->path));
    if (err!=WORLD_OK) {
        free(world->cache);
        free(world->lookup);
        memset(world,0,sizeof(*world));
        return world->error=err;
    }
    strcpy(world->id,id);
    strcpy(world->name,name);
    world->creative=(uint8_t)(creative!=0);
    world->structures=(uint8_t)(structures!=0);
    world->persistent=1;
    return WORLD_OK;
}

WorldError world_open(World *world, const char *saves_dir, const char *id,
                      size_t cache_capacity)
{
    WorldInfo info;
    WorldError err;
    char path[WORLD_PATH_MAX];
    if (!world) return WORLD_ERROR_INVALID_ARGUMENT;
    err=world_storage_open(saves_dir,id,&info,path,sizeof(path));
    if (err!=WORLD_OK) return err;
    err=world_init(world,info.seed,info.flat,cache_capacity);
    if (err!=WORLD_OK) return err;
    strcpy(world->path,path);
    strcpy(world->id,info.id);
    strcpy(world->name,info.name);
    world->creative=info.creative;
    world->structures=info.structures;
    world->beta_world_time=info.world_time;
    world->rain_time=info.rain_time; world->thunder_time=info.thunder_time;
    world->raining=info.raining; world->thundering=info.thundering;
    world->rain_strength=info.raining ? 1 : 0;
    world->thunder_strength=info.thundering ? 1 : 0;
    world->persistent=1;
    return WORLD_OK;
}

static void mark_neighbor(World *world, int32_t x, int32_t z)
{
    Chunk *chunk=world_peek_chunk(world,x,z);
    if (chunk) chunk->dirty_flags|=CHUNK_DIRTY_MESH;
}

static void mark_mesh_neighbors(World *world, int32_t x, int32_t z)
{
    int dx,dz;
    for (dz=-1;dz<=1;++dz) for (dx=-1;dx<=1;++dx) {
        int64_t nx=(int64_t)x+dx,nz=(int64_t)z+dz;
        if ((dx || dz) && nx>=INT32_MIN && nx<=INT32_MAX && nz>=INT32_MIN && nz<=INT32_MAX) {
            if (dx && dz) {
                Chunk *corner=world_peek_chunk(world,(int32_t)nx,(int32_t)nz);
                if (corner) corner->dirty_flags|=CHUNK_DIRTY_SMOOTH_MESH;
            } else mark_neighbor(world,(int32_t)nx,(int32_t)nz);
        }
    }

}

static size_t chunk_hash(int32_t x, int32_t z, size_t capacity)
{
    uint32_t hash=(uint32_t)x*UINT32_C(0x9e3779b1) ^
                  (uint32_t)z*UINT32_C(0x85ebca6b);
    hash ^= hash >> 16;
    return (size_t)hash & (capacity-1u);
}

static void lookup_insert(World *world, Chunk *chunk)
{
    size_t slot=chunk_hash(chunk->x,chunk->z,world->lookup_capacity);
    while (world->lookup[slot]) slot=(slot+1u)&(world->lookup_capacity-1u);
    world->lookup[slot]=chunk;
}

static void lookup_rebuild(World *world)
{
    size_t i;
    memset(world->lookup,0,world->lookup_capacity*sizeof(Chunk *));
    for (i=0;i<world->cache_count;++i) lookup_insert(world,world->cache[i]);
}

Chunk *world_peek_chunk(const World *world, int32_t cx, int32_t cz)
{
    size_t slot;
    if (!world || !world->lookup) return NULL;
    slot=chunk_hash(cx,cz,world->lookup_capacity);
    while (world->lookup[slot]) {
        Chunk *chunk=world->lookup[slot];
        if (chunk && chunk->x==cx && chunk->z==cz) return chunk;
        slot=(slot+1u)&(world->lookup_capacity-1u);
    }
    return NULL;
}

void world_touch_chunk(World *world, Chunk *chunk)
{
    if (world && chunk) chunk->last_used=++world->clock;
}

Chunk *world_get_chunk(World *world, int32_t cx, int32_t cz)
{
    Chunk *incoming, *existing;
    WorldError err=WORLD_OK;
    size_t i,victim=0;
    uint64_t oldest=UINT64_MAX;
    if (!world || !world->cache) return NULL;
    existing=world_peek_chunk(world,cx,cz);
    if (existing) {
        world_touch_chunk(world,existing);
        world->error=WORLD_OK;
        return existing;
    }
    incoming=world->cache_count<world->cache_capacity ?
             (Chunk *)malloc(sizeof(Chunk)) : world->staging;
    if (!incoming) incoming=(Chunk *)malloc(sizeof(Chunk));
    if (!incoming) {
        world->error=WORLD_ERROR_OUT_OF_MEMORY;
        return NULL;
    }
    memset(incoming,0,sizeof(*incoming));
    incoming->x=cx;
    incoming->z=cz;
    if (world->persistent)
        err=world->beta_format ? world->read_beta_chunk(world,incoming) :
            world_storage_read_chunk(world,incoming);
    else err=WORLD_ERROR_NOT_FOUND;
    if (err==WORLD_ERROR_NOT_FOUND) {
        if (world->network_mode || world->beta_format) {
            incoming->dirty_flags=CHUNK_DIRTY_MESH;
            incoming->revision=1;
        } else generate_chunk(world,incoming,cx,cz);
        err=WORLD_OK;
    }
    if (err!=WORLD_OK) {
        block_entities_free(incoming);
        world_entities_free(incoming);
        world_ticks_free(incoming);
        free(incoming->beta_raw);
        if (incoming!=world->staging) free(incoming);
        world->error=err;
        return NULL;
    }
    if (world->cache_count<world->cache_capacity) {
        world->cache[world->cache_count++]=incoming;
        lookup_insert(world,incoming);
    } else {
        Chunk *outgoing;
        for (i=0; i<world->cache_count; ++i) {
            if (world->cache[i]->last_used<oldest) {
                oldest=world->cache[i]->last_used;
                victim=i;
            }
        }
        outgoing=world->cache[victim];
        if(outgoing->ticks) world_ticks_dirty_countdowns(world);
        if ((outgoing->dirty_flags&CHUNK_DIRTY_SAVE) && world->persistent &&
            (!world->beta_format || outgoing->beta_raw)) {
            err=world->beta_format ? world->write_beta_chunk(world,outgoing) :
                world_storage_write_chunk(world,outgoing);
            if (err!=WORLD_OK) {
                block_entities_free(incoming);
                world_entities_free(incoming);
                world_ticks_free(incoming);
                free(incoming->beta_raw); incoming->beta_raw=NULL;
                if (incoming!=world->staging) free(incoming);
                world->error=err;
                return NULL;
            }
            outgoing->dirty_flags &= ~CHUNK_DIRTY_SAVE;
            outgoing->ticks_saved_at=world->tick;
        }
        mark_mesh_neighbors(world,outgoing->x,outgoing->z);
        if (outgoing->render_data && world->destroy_render_data)
            world->destroy_render_data(outgoing->render_data);
        outgoing->render_data=NULL;
        world_physics_forget_chunk(world,outgoing);
        block_entities_free(outgoing);
        world_entities_free(outgoing);
        world_ticks_free(outgoing);
        free(outgoing->beta_raw);
        outgoing->beta_raw=NULL;
        world->cache[victim]=incoming;
        lookup_rebuild(world);
        world->staging=outgoing;
    }
    world_touch_chunk(world,incoming);
    world_physics_loaded(world,incoming);
    mark_mesh_neighbors(world,cx,cz);
    /* Original McRegion light is authoritative on initial load. In particular
     * an unloaded neighbour is not proof that an existing light source died.
     * Actual edits still relight all affected cached chunk boundaries. */
    if (!world->network_mode && !world->beta_format) {
        static const int dx[4]={-1,1,0,0},dz[4]={0,0,-1,1};
        static const uint8_t dark[WORLD_NIBBLE_BYTES]={0};
        int side,has_light=memcmp(incoming->block_light,dark,sizeof(dark))!=0;
        for (side=0;side<4;++side) {
            Chunk *neighbor=world_peek_chunk(world,cx+dx[side],cz+dz[side]);
            if (neighbor && memcmp(neighbor->block_light,dark,sizeof(dark))) has_light=1;
        }
        if (has_light) {
            incoming->dirty_flags|=CHUNK_DIRTY_LIGHT;
            for (side=0;side<4;++side) {
                Chunk *neighbor=world_peek_chunk(world,cx+dx[side],cz+dz[side]);
                if (neighbor) neighbor->dirty_flags|=CHUNK_DIRTY_LIGHT;
            }
            flush_block_light(world);
        }
    }
    world->error=WORLD_OK;
    return incoming;
}

size_t world_cached_chunk_count(const World *world)
{
    return world ? world->cache_count : 0;
}

Chunk *world_cached_chunk_at(const World *world, size_t index)
{
    return world && index<world->cache_count ? world->cache[index] : NULL;
}

size_t world_dirty_chunk_count(const World *world)
{
    size_t i, count=0;
    if (!world) return 0;
    for (i=0; i<world->cache_count; ++i)
        if (world->cache[i]->dirty_flags & CHUNK_DIRTY_MESH) ++count;
    return count;
}

size_t world_memory_bytes(const World *world)
{
    size_t i,bytes;
    if (!world) return 0;
    bytes=sizeof(*world) + world->cache_capacity*sizeof(Chunk *) +
           world->lookup_capacity*sizeof(Chunk *) +
           (world->cache_count + (world->staging!=NULL ? 1u : 0u))*sizeof(Chunk);
    for (i=0;i<world->cache_count;++i) {
        const BlockEntity *b; const SavedEntity *e; const Chunk *c=world->cache[i];
        bytes+=c->beta_raw_size;
        for(b=c->entities;b;b=b->next) bytes+=sizeof(*b)+b->raw_size;
        for(e=c->saved_entities;e;e=e->next) bytes+=sizeof(*e)+e->raw_size;
        { const SavedTick *t; for(t=c->ticks;t;t=t->next) bytes+=sizeof(*t)+t->raw_size; }
    }
    return bytes;
}
void world_set_render_data_destroy(World *world, void (*destroy)(void *))
{
    if (world) world->destroy_render_data=destroy;
}

uint8_t world_get_block(World *world, int wx, int y, int wz)
{
    int32_t cx,cz;
    int x,z;
    Chunk *chunk;
    if ((unsigned)y>=WORLD_HEIGHT) return BLOCK_AIR;
    x=local_from_world(wx,&cx);
    z=local_from_world(wz,&cz);
    chunk=world_get_chunk(world,cx,cz);
    return chunk ? chunk_get_block(chunk,x,y,z) : BLOCK_AIR;
}

uint8_t world_peek_block(const World *world, int wx, int y, int wz)
{
    int32_t cx,cz;
    int x,z;
    Chunk *chunk;
    if ((unsigned)y>=WORLD_HEIGHT) return BLOCK_AIR;
    x=local_from_world(wx,&cx);
    z=local_from_world(wz,&cz);
    chunk=world_peek_chunk(world,cx,cz);
    return chunk ? chunk_get_block(chunk,x,y,z) : BLOCK_AIR;
}

int world_set_block(World *world, int wx, int y, int wz, uint8_t id)
{
    int32_t cx,cz;
    int x,z;
    Chunk *chunk;
    if ((unsigned)y>=WORLD_HEIGHT || id>=BLOCK_COUNT) return 0;
    x=local_from_world(wx,&cx);
    z=local_from_world(wz,&cz);
    chunk=world_get_chunk(world,cx,cz);
    if (!chunk) return 0;
    if (world->beta_format && !chunk->beta_raw) return 0;
    if (chunk_get_block(chunk,x,y,z)==id) return 1;
    {
        uint8_t old=chunk_get_block(chunk,x,y,z);
        if ((old==54 || old==61 || old==62 || old==63 || old==68) &&
            !((old==61 || old==62) && (id==61 || id==62)))
            block_entity_remove(world,wx,y,wz,!world->network_mode);
    }
    chunk_set_block(chunk,x,y,z,id);
    /* A newly placed block starts with metadata zero. Network block-change
     * packets set their transmitted nibble immediately afterward. */
    chunk_set_metadata(chunk,x,y,z,0);
    relight_sky_column(chunk,x,z);
    chunk->dirty_flags|=CHUNK_DIRTY_LIGHT;
    if (!world->physics_processing) flush_block_light(world);
    chunk->dirty_flags|=CHUNK_DIRTY_MESH|CHUNK_DIRTY_SAVE;
    mark_mesh_neighbors(world,cx,cz);
    world_physics_notify(world,wx,y,wz);
    return 1;
}

void world_drop_stack(World *world,int x,int y,int z,InventorySlot item)
{
    unsigned index;
    if (!world || world->network_mode || item.id<=0 || item.count<=0) return;
    if(!world_item_spawn(world,x,y,z,item)) return;
    /* The observer queue is separate from the saved entity. A full queue
     * must never erase a stack dropped by a container or on player death. */
    if(world->drop_count>=WORLD_DROP_QUEUE) return;
    index=(world->drop_head+world->drop_count)%WORLD_DROP_QUEUE;
    world->drops[index].x=x;
    world->drops[index].y=(uint8_t)y;
    world->drops[index].z=z;
    world->drops[index].id=item.id;
    world->drops[index].count=item.count;
    world->drops[index].damage=item.damage;
    ++world->drop_count;
}
void world_sound(World *w,const char *key,float x,float y,float z,float volume,float pitch)
{
    unsigned i;
    if(!w || w->sound_count>=64) return;
    i=w->sound_count++;
    w->sounds[i].key=key; w->sounds[i].x=x; w->sounds[i].y=y; w->sounds[i].z=z;
    w->sounds[i].volume=volume; w->sounds[i].pitch=pitch;
}

int world_take_drop(World *world,WorldDropEvent *drop)
{
    if (!world || !drop || !world->drop_count) return 0;
    *drop=world->drops[world->drop_head];
    world->drop_head=(world->drop_head+1u)%WORLD_DROP_QUEUE;
    --world->drop_count;
    return 1;
}

void world_finish_light_updates(World *world) { flush_block_light(world); }
uint8_t world_peek_metadata(const World *world,int wx,int y,int wz)
{
    int32_t cx,cz;
    int x,z;
    Chunk *chunk;
    if ((unsigned)y>=WORLD_HEIGHT) return 0;
    x=local_from_world(wx,&cx); z=local_from_world(wz,&cz);
    chunk=world_peek_chunk(world,cx,cz);
    return chunk ? chunk_get_metadata(chunk,x,y,z) : 0;
}

uint8_t world_get_metadata(World *world, int wx, int y, int wz)
{
    int32_t cx,cz;
    int x,z;
    Chunk *chunk;
    if ((unsigned)y>=WORLD_HEIGHT) return 0;
    x=local_from_world(wx,&cx);
    z=local_from_world(wz,&cz);
    chunk=world_get_chunk(world,cx,cz);
    return chunk ? chunk_get_metadata(chunk,x,y,z) : 0;
}

int world_set_metadata(World *world, int wx, int y, int wz, uint8_t value)
{
    int32_t cx,cz;
    int x,z;
    Chunk *chunk;
    if ((unsigned)y>=WORLD_HEIGHT) return 0;
    x=local_from_world(wx,&cx);
    z=local_from_world(wz,&cz);
    chunk=world_get_chunk(world,cx,cz);
    if (!chunk) return 0;
    if (world->beta_format && !chunk->beta_raw) return 0;
    chunk_set_metadata(chunk,x,y,z,value);
    return 1;
}

void world_relight_chunk(World *world, Chunk *chunk)
{
    if (!world || !chunk) return;
    relight_sky(chunk);
    chunk->dirty_flags|=CHUNK_DIRTY_LIGHT;
    flush_block_light(world);
    chunk->dirty_flags|=CHUNK_DIRTY_MESH|CHUNK_DIRTY_SAVE;
    ++chunk->revision;
    mark_mesh_neighbors(world,chunk->x,chunk->z);
}

WorldError world_save(World *world)
{
    size_t i;
    WorldError err;
    if (!world || !world->cache) return WORLD_ERROR_INVALID_ARGUMENT;
    if (!world->persistent) return world->error=WORLD_OK;
    if(world->beta_format && world->beta_session && !beta_session_check(world->path,world->beta_session))
        return world->error=WORLD_ERROR_SESSION_LOCK;
    world_ticks_dirty_countdowns(world);
    for (i=0; i<world->cache_count; ++i) {
        Chunk *chunk=world->cache[i];
        if (!(chunk->dirty_flags&CHUNK_DIRTY_SAVE)) continue;
        /* Missing Beta chunks are read-only empty cache entries. Derived light
         * updates must never make these unsaveable entries block world exit. */
        if (world->beta_format && !chunk->beta_raw) {
            chunk->dirty_flags &= ~CHUNK_DIRTY_SAVE;
            continue;
        }
        err=world->beta_format ? world->write_beta_chunk(world,chunk) :
            world_storage_write_chunk(world,chunk);
        if (err!=WORLD_OK) return world->error=err;
        chunk->dirty_flags &= ~(CHUNK_DIRTY_SAVE|CHUNK_DIRTY_ENTITIES|CHUNK_DIRTY_TICKS);
        chunk->ticks_saved_at=world->tick;
    }
    err=world->beta_format ? WORLD_OK : world_storage_write_info(world);
    return world->error=err;
}

WorldError world_close(World *world)
{
    size_t i;
    WorldError err;
    if (!world) return WORLD_ERROR_INVALID_ARGUMENT;
    err=world->cache ? world_save(world) : WORLD_OK;
    if (err!=WORLD_OK) return err;
    for (i=0; i<world->cache_count; ++i) {
        Chunk *chunk=world->cache[i];
        if (chunk->render_data && world->destroy_render_data)
            world->destroy_render_data(chunk->render_data);
        block_entities_free(chunk);
        world_entities_free(chunk);
        world_ticks_free(chunk);
        free(chunk->beta_raw);
        free(chunk);
    }
    free(world->staging);
    free(world->cache);
    free(world->lookup);
    free(world->climate);
    memset(world,0,sizeof(*world));
    return WORLD_OK;
}
