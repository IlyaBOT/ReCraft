#include "world/beta_blocks.h"

#include <assert.h>
#include <string.h>

int main(void)
{
    unsigned id, meta;
    const BetaBlockDef *def;
    /* The stock b1.7.3 registry occupies the contiguous range 0..96. */
    for (id = 0; id < BETA_BLOCK_COUNT; ++id) {
        def = beta_block_find(id);
        assert(def && def->id == id && def->name && def->name[0]);
        for (meta = 0; meta < 16; ++meta) {
            BetaBlockState state;
            state.id = (uint8_t)id;
            state.metadata = (uint8_t)meta;
            assert(beta_block_state_valid(state));
        }
    }
    assert(!beta_block_find(97));
    assert(!beta_block_find(255));
    assert(BETA_BLOCK_GRASS == 2 && BETA_BLOCK_DIRT == 3);
    assert(BETA_BLOCK_WOOL == 35 && BETA_BLOCK_TRAPDOOR == 96);
    assert(strcmp(beta_block_find(BETA_BLOCK_COBBLESTONE)->name, "stonebrick") == 0);
    assert(strcmp(beta_block_find(BETA_BLOCK_WOOL)->metadata_usage, "wool color") == 0);
    {
        BetaBlockState log = { BETA_BLOCK_LOG, 0 };
        BetaBlockState leaves = { BETA_BLOCK_LEAVES, 0 };
        BetaBlockState wool = { BETA_BLOCK_WOOL, 0 };
        assert(beta_block_terrain_tile(log,0)==21);
        assert(beta_block_terrain_tile(log,2)==20);
        log.metadata=1; assert(beta_block_terrain_tile(log,2)==116);
        log.metadata=2; assert(beta_block_terrain_tile(log,2)==117);
        log.metadata=3; assert(beta_block_terrain_tile(log,2)==20);
        leaves.metadata=1; assert(beta_block_terrain_tile(leaves,2)==132);
        leaves.metadata=2; assert(beta_block_terrain_tile(leaves,2)==52);
        assert(beta_block_terrain_tile(wool,2)==64);
        wool.metadata=1; assert(beta_block_terrain_tile(wool,2)==210);
        wool.metadata=15; assert(beta_block_terrain_tile(wool,2)==113);
        assert(beta_block_terrain_tile(wool,6)==-1);
    }
    {
        BetaBlockState plant = { BETA_BLOCK_SAPLING, 0 };
        BetaBlockBox box;
        assert(beta_block_cross_plant(plant.id));
        assert(beta_block_selection_box(plant,&box));
        assert(box.min_x==0.1f && box.max_y==0.8f);
        assert(beta_block_terrain_tile(plant,2)==15);
        plant.metadata=1; assert(beta_block_terrain_tile(plant,2)==63);
        plant.metadata=2; assert(beta_block_terrain_tile(plant,2)==79);
        plant.id=BETA_BLOCK_TALL_GRASS;
        plant.metadata=0; assert(beta_block_terrain_tile(plant,2)==55);
        plant.metadata=1; assert(beta_block_terrain_tile(plant,2)==39);
        plant.metadata=2; assert(beta_block_terrain_tile(plant,2)==56);
        plant.id=BETA_BLOCK_DEAD_BUSH; assert(beta_block_terrain_tile(plant,2)==55);
        plant.id=BETA_BLOCK_DANDELION; assert(beta_block_terrain_tile(plant,2)==13);
        plant.id=BETA_BLOCK_ROSE; assert(beta_block_terrain_tile(plant,2)==12);
        plant.id=BETA_BLOCK_BROWN_MUSHROOM; assert(beta_block_terrain_tile(plant,2)==29);
        assert(beta_block_selection_box(plant,&box) && box.max_y==0.4f);
        plant.id=BETA_BLOCK_RED_MUSHROOM; assert(beta_block_terrain_tile(plant,2)==28);
        plant.id=BETA_BLOCK_REEDS; assert(beta_block_terrain_tile(plant,2)==73);
        assert(beta_block_selection_box(plant,&box) && box.max_y==1.0f);
        assert(!beta_block_cross_plant(BETA_BLOCK_STONE));
        plant.id=BETA_BLOCK_STONE;
        assert(!beta_block_selection_box(plant,&box));
    }
    {
        BetaBlockState slab = { BETA_BLOCK_SLAB, 0 };
        BetaBlockBox box;
        assert(beta_block_selection_box(slab,&box));
        assert(box.min_y==0.0f && box.max_y==0.5f);
        assert(beta_block_terrain_tile(slab,0)==6);
        assert(beta_block_terrain_tile(slab,1)==6);
        assert(beta_block_terrain_tile(slab,2)==5);
        slab.metadata=1;
        assert(beta_block_terrain_tile(slab,0)==208);
        assert(beta_block_terrain_tile(slab,1)==176);
        assert(beta_block_terrain_tile(slab,2)==192);
        slab.metadata=2; assert(beta_block_terrain_tile(slab,2)==4);
        slab.metadata=3; assert(beta_block_terrain_tile(slab,2)==16);
        slab.metadata=15; assert(beta_block_terrain_tile(slab,2)==6);
        slab.id=BETA_BLOCK_DOUBLE_SLAB;
        assert(!beta_block_selection_box(slab,&box));
        slab.metadata=1; assert(beta_block_terrain_tile(slab,0)==208);
    }
    {
        BetaBlockState torch = { BETA_BLOCK_TORCH, 1 };
        BetaBlockBox box;
        assert(beta_block_selection_box(torch,&box));
        assert(box.min_x==0.0f && box.max_x==0.3f && box.min_y==0.2f);
        torch.metadata=2; assert(beta_block_selection_box(torch,&box));
        assert(box.min_x==0.7f && box.max_x==1.0f);
        torch.metadata=3; assert(beta_block_selection_box(torch,&box));
        assert(box.min_z==0.0f && box.max_z==0.3f);
        torch.metadata=4; assert(beta_block_selection_box(torch,&box));
        assert(box.min_z==0.7f && box.max_z==1.0f);
        torch.metadata=5; assert(beta_block_selection_box(torch,&box));
        assert(box.min_y==0.0f && box.max_y==0.6f && box.min_x==0.4f);
        assert(beta_block_terrain_tile(torch,2)==80);
        torch.id=BETA_BLOCK_REDSTONE_TORCH;
        assert(beta_block_selection_box(torch,&box) && box.max_y==0.6f);
        torch.id=BETA_BLOCK_UNLIT_REDSTONE_TORCH;
        assert(beta_block_selection_box(torch,&box) && box.max_y==0.6f);
    }
    {
        static const uint8_t cube_ids[15] = {
            BETA_BLOCK_BEDROCK, BETA_BLOCK_GOLD_ORE, BETA_BLOCK_IRON_ORE,
            BETA_BLOCK_COAL_ORE, BETA_BLOCK_LAPIS_ORE, BETA_BLOCK_LAPIS_BLOCK,
            BETA_BLOCK_BRICKS, BETA_BLOCK_MOSSY_COBBLESTONE, BETA_BLOCK_OBSIDIAN,
            BETA_BLOCK_DIAMOND_ORE, BETA_BLOCK_REDSTONE_ORE, BETA_BLOCK_CLAY,
            BETA_BLOCK_NETHERRACK, BETA_BLOCK_SOUL_SAND, BETA_BLOCK_GLOWSTONE
        };
        static const int tiles[15] = {
            17,32,33,34,160,144,7,36,37,50,51,72,103,104,105
        };
        for (id=0; id<15; ++id) {
            BetaBlockState state = { cube_ids[id], 0 };
            assert(beta_block_terrain_tile(state, 2)==tiles[id]);
        }
    }
    {
        BetaBlockState state = { BETA_BLOCK_STONE, 16 };
        assert(!beta_block_state_valid(state));
    }
    return 0;
}
