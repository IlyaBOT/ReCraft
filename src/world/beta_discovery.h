#ifndef RECRAFT_BETA_DISCOVERY_H
#define RECRAFT_BETA_DISCOVERY_H

#include <stddef.h>
#include <stdint.h>
#include "../game/inventory.h"

typedef struct BetaWorldInfo {
    char directory[64];
    char name[96];
    int64_t seed;
    int spawn_x, spawn_y, spawn_z;
    int64_t world_time;
    uint64_t last_played;
    unsigned region_files;
    int save_version,dimension;
    int has_player;
    double player_x,player_y,player_z;
    float player_yaw,player_pitch;
} BetaWorldInfo;

/* Metadata only. No chunk allocation, migration, or writes to save files. */
size_t beta_world_discover(const char *saves_dir, BetaWorldInfo *out, size_t capacity);
/* Player Inventory slots 0..35 from level.dat; returns zero on invalid NBT. */
int beta_world_read_inventory(const char *world_path,
                              InventorySlot slots[RECRAFT_INVENTORY_SLOTS]);

#endif
