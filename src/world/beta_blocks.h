#ifndef RECRAFT_BETA_BLOCKS_H
#define RECRAFT_BETA_BLOCKS_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Beta 1.7.3 stores a numeric block ID and a separate four-bit metadata value.
 * ReCraft's RCC1 version 2 chunk records use the same IDs. */
typedef struct BetaBlockState {
    uint8_t id;
    uint8_t metadata;
} BetaBlockState;

typedef struct BetaBlockDef {
    uint8_t id;
    const char *name;           /* Beta client's unlocalized registry name. */
    const char *metadata_usage; /* Documentation, not a metadata sanitizer. */
} BetaBlockDef;

typedef struct BetaBlockBox {
    float min_x, min_y, min_z;
    float max_x, max_y, max_z;
} BetaBlockBox;

typedef enum BetaBlockId {
#define BETA_BLOCK(id, token, name, usage) BETA_BLOCK_##token = id,
#include "beta_blocks.def"
#undef BETA_BLOCK
    BETA_BLOCK_COUNT = 97
} BetaBlockId;

/* NULL means an ID outside the unmodded Beta 1.7.3 block registry. */
const BetaBlockDef *beta_block_find(unsigned id);
/* Accept every nibble value; old worlds may contain unusual but valid raw data. */
int beta_block_state_valid(BetaBlockState state);
/* Original terrain.png index for the audited metadata variants below.
 * Face numbering follows Beta: 0 bottom, 1 top, 2..5 sides. Returns -1 when
 * this stage has not verified the block's texture selection. */
int beta_block_terrain_tile(BetaBlockState state, unsigned face);
/* Stable compact atlas slots shared by all chunk meshing paths. */
int beta_render_source_tile(unsigned slot);
unsigned beta_render_tile(int terrain_tile);
int beta_material_solid(unsigned id);
int beta_material_blocks_flow(unsigned id);
int beta_material_burns(unsigned id);
int beta_material_wood(unsigned id);
/* Exact crossed-plane plant family audited for this renderer stage. */
int beta_block_cross_plant(unsigned id);
/* Audited selection bounds for crossed plants, torches and the half slab.
 * Returns zero for blocks whose selection box is not registered yet. */
int beta_block_selection_box(BetaBlockState state, BetaBlockBox *out);

#ifdef __cplusplus
}
#endif

#endif
