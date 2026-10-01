#ifndef RECRAFT_BETA_LEVEL_H
#define RECRAFT_BETA_LEVEL_H

#include <stdint.h>
#include "../game/inventory.h"

typedef struct BetaLevelState {
    double x,y,z;
    double motion_x,motion_y,motion_z;
    float yaw,pitch; /* Minecraft degrees, not ReCraft camera radians. */
    int on_ground;
    int64_t world_time;
    InventorySlot inventory[RECRAFT_INVENTORY_SLOTS];
} BetaLevelState;

/* Rewrite player state/time while preserving unrelated NBT. Read fallback and
 * level.dat_new -> level.dat_old -> level.dat follow Beta's SaveHandler.
 * Also retain the first valid input as level.dat.recraft.bak. */
int beta_level_save(const char *world_path,const BetaLevelState *state);

#endif
