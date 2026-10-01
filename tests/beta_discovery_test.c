#include "world/beta_discovery.h"
#include "util/game_paths.h"
#include <stdio.h>
#include <string.h>

int main(int argc, char **argv)
{
    BetaWorldInfo worlds[128];
    size_t i, count;
    int new_world = 0, world1 = 0;
    if (argc != 2) return 2;
    count = beta_world_discover(argv[1],worlds,128);
    for (i = 0; i < count; ++i) {
        const BetaWorldInfo *world = &worlds[i];
        InventorySlot slots[RECRAFT_INVENTORY_SLOTS];
        char path[512];
        printf("%s | %s | seed %lld | spawn %d,%d,%d | %u .mcr\n",
            world->directory,world->name,(long long)world->seed,
            world->spawn_x,world->spawn_y,world->spawn_z,world->region_files);
        if (!game_path_join(path,sizeof(path),argv[1],world->directory) ||
            !beta_world_read_inventory(path,slots)) {
            fprintf(stderr,"Cannot read Beta player inventory: %s\n",world->directory);
            return 1;
        }
        if (!strcmp(world->directory,"New World") &&
            !strcmp(world->name,"New World") && world->region_files > 0 &&
            world->spawn_x == 72 && world->spawn_y == 64 && world->spawn_z == 19)
            new_world = 1;
        if (!strcmp(world->directory,"World1") &&
            !strcmp(world->name,"World1") && world->region_files > 0 &&
            world->spawn_x == -192 && world->spawn_y == 64 && world->spawn_z == 127)
            world1 = 1;
    }
    if (!new_world || !world1) {
        fprintf(stderr,"The two supplied Beta 1.7.3 fixtures were not discovered.\n");
        return 1;
    }
    return 0;
}
