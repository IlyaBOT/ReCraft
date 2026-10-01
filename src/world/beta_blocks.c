#include "beta_blocks.h"
static const uint8_t materials[BETA_BLOCK_COUNT][5]={
#include "beta_materials.def"
};
int beta_material_solid(unsigned id) { return id<BETA_BLOCK_COUNT && materials[id][0]; }
int beta_material_blocks_flow(unsigned id) { return id<BETA_BLOCK_COUNT && materials[id][1]; }
int beta_material_burns(unsigned id) { return id<BETA_BLOCK_COUNT && materials[id][3]; }
int beta_material_wood(unsigned id) { return id<BETA_BLOCK_COUNT && materials[id][4]; }

static const BetaBlockDef beta_blocks[BETA_BLOCK_COUNT] = {
#define BETA_BLOCK(id, token, name, usage) [id] = { id, name, usage },
#include "beta_blocks.def"
#undef BETA_BLOCK
};

const BetaBlockDef *beta_block_find(unsigned id)
{
    if (id >= BETA_BLOCK_COUNT || !beta_blocks[id].name) return 0;
    return &beta_blocks[id];
}

int beta_block_state_valid(BetaBlockState state)
{
    return beta_block_find(state.id) != 0 && state.metadata <= 15;
}

int beta_block_cross_plant(unsigned id)
{
    switch (id) {
    case BETA_BLOCK_SAPLING: case BETA_BLOCK_TALL_GRASS:
    case BETA_BLOCK_DEAD_BUSH: case BETA_BLOCK_DANDELION:
    case BETA_BLOCK_ROSE: case BETA_BLOCK_BROWN_MUSHROOM:
    case BETA_BLOCK_RED_MUSHROOM: case BETA_BLOCK_REEDS:
        return 1;
    default: return 0;
    }
}

int beta_block_selection_box(BetaBlockState state, BetaBlockBox *out)
{
    float inset, height;
    if (!out || !beta_block_state_valid(state)) return 0;
    if (state.id == BETA_BLOCK_SLAB) {
        out->min_x = out->min_y = out->min_z = 0.0f;
        out->max_x = out->max_z = 1.0f;
        out->max_y = 0.5f;
        return 1;
    }
    if (state.id==BETA_BLOCK_CACTUS) {
        *out=(BetaBlockBox){0.0625f,0.0f,0.0625f,0.9375f,1.0f,0.9375f};
        return 1;
    }
    if (state.id==BETA_BLOCK_NETHER_PORTAL) {
        *out=(BetaBlockBox){0.0f,0.0f,0.375f,1.0f,1.0f,0.625f};
        return 1;
    }
    if (state.id == BETA_BLOCK_TORCH ||
        state.id == BETA_BLOCK_UNLIT_REDSTONE_TORCH ||
        state.id == BETA_BLOCK_REDSTONE_TORCH) {
        switch (state.metadata & 7u) {
        case 1:
            *out = (BetaBlockBox){0.0f,0.2f,0.35f,0.3f,0.8f,0.65f}; break;
        case 2:
            *out = (BetaBlockBox){0.7f,0.2f,0.35f,1.0f,0.8f,0.65f}; break;
        case 3:
            *out = (BetaBlockBox){0.35f,0.2f,0.0f,0.65f,0.8f,0.3f}; break;
        case 4:
            *out = (BetaBlockBox){0.35f,0.2f,0.7f,0.65f,0.8f,1.0f}; break;
        default:
            *out = (BetaBlockBox){0.4f,0.0f,0.4f,0.6f,0.6f,0.6f}; break;
        }
        return 1;
    }
    if (!beta_block_cross_plant(state.id)) return 0;
    switch (state.id) {
    case BETA_BLOCK_SAPLING:
    case BETA_BLOCK_TALL_GRASS:
    case BETA_BLOCK_DEAD_BUSH:
        inset = 0.1f; height = 0.8f; break;
    case BETA_BLOCK_BROWN_MUSHROOM:
    case BETA_BLOCK_RED_MUSHROOM:
        inset = 0.3f; height = 0.4f; break;
    case BETA_BLOCK_REEDS:
        inset = 0.125f; height = 1.0f; break;
    default:
        inset = 0.3f; height = 0.6f; break;
    }
    out->min_x = out->min_z = inset;
    out->min_y = 0.0f;
    out->max_x = out->max_z = 1.0f - inset;
    out->max_y = height;
    return 1;
}

int beta_block_terrain_tile(BetaBlockState state, unsigned face)
{
    /* Species/color rules follow vg, bk, ee, he and ru in the local Beta
     * client JAR. Simple cube/plant indices come from uu's initializer. */
    unsigned variant;
    if (!beta_block_state_valid(state) || face > 5u) return -1;
    variant = state.metadata & 3u;
    switch (state.id) {
    case BETA_BLOCK_DOUBLE_SLAB:
    case BETA_BLOCK_SLAB:
        switch (state.metadata) {
        case 0: return face < 2u ? 6 : 5;
        case 1: return face == 0u ? 208 : face == 1u ? 176 : 192;
        case 2: return 4;
        case 3: return 16;
        default: return 6;
        }
    case BETA_BLOCK_PLANKS:
        return 4;
    case BETA_BLOCK_LOG:
        if (face < 2u) return 21;
        return variant == 1u ? 116 : variant == 2u ? 117 : 20;
    case BETA_BLOCK_LEAVES:
        return variant == 1u ? 132 : 52;
    case BETA_BLOCK_WOOL:
        if (state.metadata == 0) return 64;
        variant = (~(unsigned)state.metadata) & 15u;
        return (int)(113u + ((variant & 8u) >> 3u) + (variant & 7u) * 16u);
    case BETA_BLOCK_BEDROCK: return 17;
    case BETA_BLOCK_GOLD_ORE: return 32;
    case BETA_BLOCK_IRON_ORE: return 33;
    case BETA_BLOCK_COAL_ORE: return 34;
    case BETA_BLOCK_LAPIS_ORE: return 160;
    case BETA_BLOCK_LAPIS_BLOCK: return 144;
    case BETA_BLOCK_BRICKS: return 7;
    case BETA_BLOCK_MOSSY_COBBLESTONE: return 36;
    case BETA_BLOCK_OBSIDIAN: return 37;
    case BETA_BLOCK_DIAMOND_ORE: return 50;
    case BETA_BLOCK_REDSTONE_ORE:
    case BETA_BLOCK_GLOWING_REDSTONE_ORE: return 51;
    case BETA_BLOCK_CLAY: return 72;
    case BETA_BLOCK_NETHERRACK: return 103;
    case BETA_BLOCK_SOUL_SAND: return 104;
    case BETA_BLOCK_GLOWSTONE: return 105;
    case BETA_BLOCK_SAPLING:
        return variant == 1u ? 63 : variant == 2u ? 79 : 15;
    case BETA_BLOCK_TALL_GRASS:
        return state.metadata == 0 ? 55 : state.metadata == 2 ? 56 : 39;
    case BETA_BLOCK_DEAD_BUSH: return 55;
    case BETA_BLOCK_DANDELION: return 13;
    case BETA_BLOCK_ROSE: return 12;
    case BETA_BLOCK_BROWN_MUSHROOM: return 29;
    case BETA_BLOCK_RED_MUSHROOM: return 28;
    case BETA_BLOCK_REEDS: return 73;
    case BETA_BLOCK_TORCH: return 80;
    case BETA_BLOCK_CRAFTING_TABLE: return face==1 ? 43 : face==0 ? 4 : (face==2 || face==4) ? 60 : 59;
    case BETA_BLOCK_CHEST: return face<2 ? 25 : face==3 ? 27 : 26;
    case BETA_BLOCK_FURNACE:
    case BETA_BLOCK_BURNING_FURNACE:
        return face<2 ? 62 : face==(state.metadata>=2 && state.metadata<=5 ? state.metadata : 3) ?
            (state.id==BETA_BLOCK_BURNING_FURNACE ? 61 : 44) : 45;
    case BETA_BLOCK_UNLIT_REDSTONE_TORCH: return 115;
    case BETA_BLOCK_REDSTONE_TORCH: return 99;
    case BETA_BLOCK_CACTUS: return face==1 ? 69 : face==0 ? 71 : 70;
    case BETA_BLOCK_NETHER_PORTAL: return 14;
    case BETA_BLOCK_FLOWING_LAVA: case BETA_BLOCK_STILL_LAVA: return face<2 ? 237 : 238;
    case BETA_BLOCK_FLOWING_WATER: case BETA_BLOCK_STILL_WATER: return face<2 ? 205 : 206;
    case BETA_BLOCK_REDSTONE_WIRE: return 164;
    case BETA_BLOCK_STONE: return 1;
    case BETA_BLOCK_GRASS: return face==1 ? 0 : face==0 ? 2 : 3;
    case BETA_BLOCK_DIRT: return 2;
    case BETA_BLOCK_COBBLESTONE: return 16;
    case BETA_BLOCK_SAND: return 18;
    case BETA_BLOCK_GRAVEL: return 19;
    case BETA_BLOCK_GLASS: return 49;
    default:
        return -1;
    }
}

int beta_render_source_tile(unsigned slot)
{
    static const int tiles[]={6,1,2,0,3,18,19,16,21,20,52,-1,49,80,
        116,117,132,4,64,210,194,178,162,146,130,114,225,209,193,177,161,145,129,113,
        17,32,33,34,160,144,7,36,37,50,51,72,103,104,105,
        15,63,79,39,55,56,13,12,29,28,73,5,208,176,192,
        237,238,14,69,70,71,43,59,60,25,26,27,62,45,44,61,99,115,164,
        205,206,41,42,57,58};
    return slot<sizeof(tiles)/sizeof(tiles[0]) ? tiles[slot] : -1;
}
unsigned beta_render_tile(int terrain_tile)
{
    unsigned i;
    for (i=0;i<256;++i) if (beta_render_source_tile(i)==terrain_tile) return i;
    return 1;
}
