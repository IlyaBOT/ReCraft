#ifndef RECRAFT_PLAYER_H
#define RECRAFT_PLAYER_H

#include "../world/world.h"

typedef struct Player {
    const char *name;           /* Borrowed from persistent UiOptions.player_name. */
    float x, y, z;              /* feet position, Y points upward */
    float vx, vy, vz;
    float yaw, pitch;           /* radians, yaw 0 looks toward -Z */
    int on_ground;
    int flying;
    int creative;
    int selected_slot;
    int health,air,fire,hurt_ticks,last_damage;
    float fall_distance;
    unsigned age;
    int sleeping,sleep_ticks,bed_x,bed_y,bed_z;
    int has_bed_spawn,spawn_x,spawn_y,spawn_z;
    int riding;
    int cart_inventory; /* Runtime interaction target, never a vanilla NBT tag. */
} Player;

typedef struct PlayerInput {
    float forward;
    float strafe;
    float look_dx;
    float look_dy;
    int jump;
    int descend;
    int sprint;
} PlayerInput;

typedef struct BlockHit {
    int hit;
    int x, y, z;
    int place_x, place_y, place_z;
    uint8_t block;
    float distance;             /* Distance along the ray to the selected face. */
} BlockHit;

void player_spawn(Player *player, World *world, int creative);
void player_tick(Player *player, World *world, const PlayerInput *input, float dt);
void player_damage(Player *player,int amount);
void player_mob_damage(Player *player,const World *world,int amount);
BlockHit player_raycast(const Player *player, World *world, float reach);
BlockHit player_raycast_sources(const Player *player,World *world,float reach);
int player_use_item(Player *player,World *world,InventorySlot *item);
int player_break_block(Player *player, World *world);
/* Local placement with an explicit Beta metadata nibble. */
int player_place_block_state(Player *player, World *world, BetaBlockState state);
int player_place_block(Player *player, World *world, uint8_t id);

#endif
