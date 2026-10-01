#ifndef RECRAFT_PLAYER_H
#define RECRAFT_PLAYER_H

#include "../world/world.h"

typedef struct Player {
    float x, y, z;              /* feet position, Y points upward */
    float vx, vy, vz;
    float yaw, pitch;           /* radians, yaw 0 looks toward -Z */
    int on_ground;
    int flying;
    int creative;
    int selected_slot;
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
} BlockHit;

void player_spawn(Player *player, World *world, int creative);
void player_tick(Player *player, World *world, const PlayerInput *input, float dt);
BlockHit player_raycast(const Player *player, World *world, float reach);
int player_break_block(Player *player, World *world);
/* Local placement with an explicit Beta metadata nibble. */
int player_place_block_state(Player *player, World *world, BetaBlockState state);
int player_place_block(Player *player, World *world, uint8_t id);

#endif
