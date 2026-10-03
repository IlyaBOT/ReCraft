#ifndef RECRAFT_BETA_LEVEL_H
#define RECRAFT_BETA_LEVEL_H

#include <stdint.h>
#include "../game/inventory.h"

typedef struct BetaLevelState {
    double x,y,z; /* Vanilla Entity Pos; awake SP Y = feet + 1.62. */
    double motion_x,motion_y,motion_z;
    float yaw,pitch; /* Minecraft degrees, not ReCraft camera radians. */
    int on_ground;
    int64_t world_time;
    int has_vitals,health,air,fire;
    int has_environment,rain_time,thunder_time,raining,thundering;
    int has_bed,bed_x,bed_y,bed_z;
    int64_t session; /* Optional play-session token; zero for fixture rewrites. */
    InventorySlot inventory[RECRAFT_INVENTORY_SLOTS];
} BetaLevelState;

/* Rewrite player state/time while preserving unrelated NBT. Read fallback and
 * level.dat_new -> level.dat_old -> level.dat follow Beta's SaveHandler.
 * Also retain the first valid input as level.dat.recraft.bak. */
int beta_level_save(const char *world_path,const BetaLevelState *state);

#endif
