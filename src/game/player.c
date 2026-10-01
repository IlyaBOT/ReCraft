#include "player.h"

#include <math.h>
#include <string.h>

#define PLAYER_RADIUS 0.30f
#define PLAYER_HEIGHT 1.80f
#define PLAYER_EYE 1.62f

static int body_intersects_block(const Player *player, int bx, int by, int bz,
                                 const BetaBlockBox *box)
{
    return player->x + PLAYER_RADIUS > (float)bx + box->min_x &&
           player->x - PLAYER_RADIUS < (float)bx + box->max_x &&
           player->y + PLAYER_HEIGHT > (float)by + box->min_y &&
           player->y < (float)by + box->max_y &&
           player->z + PLAYER_RADIUS > (float)bz + box->min_z &&
           player->z - PLAYER_RADIUS < (float)bz + box->max_z;
}

static void collision_box(uint8_t id, BetaBlockBox *box)
{
    BetaBlockState state = { id, 0 };
    if (id == BETA_BLOCK_SLAB && beta_block_selection_box(state, box)) return;
    box->min_x = box->min_y = box->min_z = 0.0f;
    box->max_x = box->max_y = box->max_z = 1.0f;
}

static int body_collides(const Player *player, World *world)
{
    int x0 = (int)floorf(player->x - PLAYER_RADIUS + 0.0001f);
    int x1 = (int)floorf(player->x + PLAYER_RADIUS - 0.0001f);
    int y0 = (int)floorf(player->y + 0.0001f);
    int y1 = (int)floorf(player->y + PLAYER_HEIGHT - 0.0001f);
    int z0 = (int)floorf(player->z - PLAYER_RADIUS + 0.0001f);
    int z1 = (int)floorf(player->z + PLAYER_RADIUS - 0.0001f);
    int x, y, z;
    for (y = y0; y <= y1; ++y)
        for (z = z0; z <= z1; ++z)
            for (x = x0; x <= x1; ++x) {
                uint8_t id = world_get_block(world, x, y, z);
                if (world_block_def(id)->solid) {
                    BetaBlockBox box;
                    collision_box(id, &box);
                    if (body_intersects_block(player, x, y, z, &box)) return 1;
                }
            }
    return 0;
}

static int move_axis(Player *player, World *world, int axis, float delta)
{
    float *position = axis == 0 ? &player->x : axis == 1 ? &player->y : &player->z;
    int steps = (int)ceilf(fabsf(delta) * 4.0f);
    int i;
    if (steps < 1) return 0;
    for (i = 0; i < steps; ++i) {
        float start = *position;
        float portion = delta / (float)steps;
        int j;
        *position += portion;
        if (!body_collides(player, world)) continue;
        *position = start;
        /* Move close to the contact face without letting the player overlap it. */
        {
            float lo = 0.0f, hi = 1.0f;
            for (j = 0; j < 8; ++j) {
                float middle = (lo + hi) * 0.5f;
                *position = start + portion * middle;
                if (body_collides(player, world)) hi = middle;
                else lo = middle;
            }
            *position = start + portion * lo;
        }
        return 1;
    }
    return 0;
}

/* Intersect a ray with an audited non-cube selection box. The
 * entry face supplies the adjacent placement cell even when the box is
 * inset from its voxel; a ray starting inside uses the exit face. */
static int bounded_ray_hit(const BetaBlockBox *box, int x, int y, int z,
                           const float origin[3], const float direction[3],
                           float start, float end, int place[3])
{
    float lower[3] = { x + box->min_x, y + box->min_y, z + box->min_z };
    float upper[3] = { x + box->max_x, y + box->max_y, z + box->max_z };
    float near_t = -1.0e30f, far_t = 1.0e30f;
    int near_axis = -1, far_axis = -1, near_side = 0, far_side = 0;
    int axis;
    for (axis = 0; axis < 3; ++axis) {
        float a, b;
        int side;
        if (fabsf(direction[axis]) < 0.000001f) {
            if (origin[axis] < lower[axis] || origin[axis] > upper[axis]) return 0;
            continue;
        }
        a = (lower[axis] - origin[axis]) / direction[axis];
        b = (upper[axis] - origin[axis]) / direction[axis];
        side = -1;
        if (a > b) { float swap = a; a = b; b = swap; side = 1; }
        if (a > near_t) { near_t = a; near_axis = axis; near_side = side; }
        if (b < far_t) { far_t = b; far_axis = axis; far_side = -side; }
        if (near_t > far_t) return 0;
    }
    if (far_t < 0.0f) return 0;
    if (near_t >= 0.0f) {
        if (near_t < start - 0.00001f || near_t > end + 0.00001f) return 0;
        axis = near_axis;
    } else {
        if (far_t < start - 0.00001f || far_t > end + 0.00001f) return 0;
        axis = far_axis;
    }
    if (axis < 0) return 0;
    place[axis] += near_t >= 0.0f ? near_side : far_side;
    return 1;
}

void player_spawn(Player *player, World *world, int creative)
{
    int y;
    int spawn_x=world->beta_format ? world->spawn_x : 8;
    int spawn_z=world->beta_format ? world->spawn_z : 8;
    memset(player, 0, sizeof(*player));
    player->x = (float)spawn_x+0.5f;
    player->z = (float)spawn_z+0.5f;
    player->yaw = 0.45f;
    player->creative = creative != 0;
    for (y = WORLD_HEIGHT - 3; y >= 1; --y) {
        uint8_t id = world_get_block(world,spawn_x,y,spawn_z);
        if (world_block_def(id)->solid) {
            player->y = (float)y + (id == BETA_BLOCK_SLAB ? 0.51f : 1.01f);
            return;
        }
    }
    player->y = world->beta_format ? (float)world->spawn_y : 68.0f;
}

void player_tick(Player *player, World *world, const PlayerInput *input, float dt)
{
    float forward = input->forward;
    float strafe = input->strafe;
    float length = sqrtf(forward * forward + strafe * strafe);
    float speed = player->flying ? 8.0f : input->sprint ? 5.6f : 4.3f;
    float sy, cy;
    int foot_x=(int)floorf(player->x), foot_y=(int)floorf(player->y+0.2f);
    int foot_z=(int)floorf(player->z);
    uint8_t fluid=world_get_block(world,foot_x,foot_y,foot_z);
    int in_water=fluid==BLOCK_WATER || fluid==BETA_BLOCK_FLOWING_WATER;
    if (!in_water) {
        fluid=world_get_block(world,foot_x,(int)floorf(player->y+1.0f),foot_z);
        in_water=fluid==BLOCK_WATER || fluid==BETA_BLOCK_FLOWING_WATER;
    }
    if (in_water && !player->flying) speed*=0.52f;
    if (length > 1.0f) { forward /= length; strafe /= length; }
    player->yaw += input->look_dx * 0.0025f;
    player->pitch -= input->look_dy * 0.0025f;
    if (player->pitch > 1.48f) player->pitch = 1.48f;
    if (player->pitch < -1.48f) player->pitch = -1.48f;
    sy = sinf(player->yaw);
    cy = cosf(player->yaw);
    player->vx = (sy * forward + cy * strafe) * speed;
    player->vz = (-cy * forward + sy * strafe) * speed;
    if (in_water && !player->flying) {
        static const int dx[4]={1,-1,0,0},dz[4]={0,0,1,-1};
        int i,level=world_get_metadata(world,foot_x,foot_y,foot_z)&7u;
        for (i=0;i<4;++i) {
            uint8_t neighbor=world_peek_block(world,foot_x+dx[i],foot_y,foot_z+dz[i]);
            if (neighbor==BLOCK_WATER || neighbor==BETA_BLOCK_FLOWING_WATER) {
                int gradient=(world_get_metadata(world,foot_x+dx[i],foot_y,
                                                   foot_z+dz[i])&7u)-level;
                if (gradient>0) {
                    player->vx+=dx[i]*0.11f*gradient;
                    player->vz+=dz[i]*0.11f*gradient;
                }
            }
        }
    }
    if (player->flying) {
        player->vy = ((input->jump != 0) - (input->descend != 0)) * speed;
        player->on_ground = 0;
        player->x += player->vx * dt;
        player->y += player->vy * dt;
        player->z += player->vz * dt;
    } else {
        if (input->jump && (player->on_ground || in_water)) {
            player->vy = in_water ? 3.2f : 8.2f;
            player->on_ground = 0;
        }
        player->vy -= (in_water ? 5.0f : 22.0f) * dt;
        if (player->vy < (in_water ? -4.0f : -35.0f))
            player->vy = in_water ? -4.0f : -35.0f;
        if (move_axis(player, world, 0, player->vx * dt)) player->vx = 0.0f;
        if (move_axis(player, world, 2, player->vz * dt)) player->vz = 0.0f;
        if (move_axis(player, world, 1, player->vy * dt)) {
            if (player->vy < 0.0f) player->on_ground = 1;
            player->vy = 0.0f;
        } else player->on_ground = 0;
    }
    if (player->y < -32.0f) player_spawn(player, world, player->creative);
}

BlockHit player_raycast(const Player *player, World *world, float reach)
{
    BlockHit result;
    float dx = sinf(player->yaw) * cosf(player->pitch);
    float dy = sinf(player->pitch);
    float dz = -cosf(player->yaw) * cosf(player->pitch);
    float ox = player->x, oy = player->y + PLAYER_EYE, oz = player->z;
    float origin[3] = { ox, oy, oz };
    float direction[3] = { dx, dy, dz };
    int x = (int)floorf(ox), y = (int)floorf(oy), z = (int)floorf(oz);
    int px = x, py = y, pz = z;
    int sx = dx >= 0.0f ? 1 : -1;
    int sy = dy >= 0.0f ? 1 : -1;
    int sz = dz >= 0.0f ? 1 : -1;
    float tx = fabsf(dx) > 0.000001f ? (dx > 0.0f ? ((float)x + 1.0f - ox) / dx : (ox - (float)x) / -dx) : 1.0e30f;
    float ty = fabsf(dy) > 0.000001f ? (dy > 0.0f ? ((float)y + 1.0f - oy) / dy : (oy - (float)y) / -dy) : 1.0e30f;
    float tz = fabsf(dz) > 0.000001f ? (dz > 0.0f ? ((float)z + 1.0f - oz) / dz : (oz - (float)z) / -dz) : 1.0e30f;
    float ix = fabsf(dx) > 0.000001f ? 1.0f / fabsf(dx) : 1.0e30f;
    float iy = fabsf(dy) > 0.000001f ? 1.0f / fabsf(dy) : 1.0e30f;
    float iz = fabsf(dz) > 0.000001f ? 1.0f / fabsf(dz) : 1.0e30f;
    float distance = 0.0f;
    memset(&result, 0, sizeof(result));
    while (distance <= reach) {
        uint8_t id = world_get_block(world, x, y, z);
        if (id != BLOCK_AIR && id != BLOCK_WATER && id != BETA_BLOCK_FLOWING_WATER) {
            int place[3] = { px, py, pz };
            int intersects = 1;
            BetaBlockBox box;
            if (beta_block_cross_plant(id) || id == BETA_BLOCK_SLAB ||
                id == BETA_BLOCK_TORCH ||
                id == BETA_BLOCK_UNLIT_REDSTONE_TORCH ||
                id == BETA_BLOCK_REDSTONE_TORCH) {
                BetaBlockState state = { id, world_get_metadata(world, x, y, z) };
                float end = fminf(reach, fminf(tx, fminf(ty, tz)));
                place[0] = x; place[1] = y; place[2] = z;
                intersects = beta_block_selection_box(state, &box) &&
                    bounded_ray_hit(&box, x, y, z, origin, direction,
                                    distance, end, place);
            }
            if (intersects) {
                result.hit = 1;
                result.x = x; result.y = y; result.z = z;
                result.place_x = place[0]; result.place_y = place[1];
                result.place_z = place[2];
                result.block = id;
                return result;
            }
        }
        px = x; py = y; pz = z;
        if (tx <= ty && tx <= tz) { x += sx; distance = tx; tx += ix; }
        else if (ty <= tz) { y += sy; distance = ty; ty += iy; }
        else { z += sz; distance = tz; tz += iz; }
    }
    return result;
}

int player_break_block(Player *player, World *world)
{
    BlockHit hit = player_raycast(player, world, 5.0f);
    return hit.hit && world_set_block(world, hit.x, hit.y, hit.z, BLOCK_AIR);
}

int player_place_block_state(Player *player, World *world, BetaBlockState state)
{
    BlockHit hit = player_raycast(player, world, 5.0f);
    uint8_t old;
    if (!hit.hit || state.id == BLOCK_AIR || !beta_block_state_valid(state) ||
        hit.place_y < 0 || hit.place_y >= WORLD_HEIGHT) return 0;
    old = world_get_block(world, hit.place_x, hit.place_y, hit.place_z);
    if (old != BLOCK_AIR && old != BLOCK_WATER &&
        old != BETA_BLOCK_FLOWING_WATER) return 0;
    if (state.id == BETA_BLOCK_TORCH && state.metadata == 0) {
        /* vm.e maps the clicked face to the attachment metadata. Metadata
         * zero is the local hotbar's "choose face" request. */
        if (!world_block_def(hit.block)->opaque) return 0;
        if (hit.place_y > hit.y) state.metadata=5;
        else if (hit.place_y < hit.y) return 0; /* no ceiling torch */
        else if (hit.place_z < hit.z) state.metadata=4;
        else if (hit.place_z > hit.z) state.metadata=3;
        else if (hit.place_x < hit.x) state.metadata=2;
        else if (hit.place_x > hit.x) state.metadata=1;
        else return 0;
    }
    /* ys.c in the Beta client merges a newly placed slab with the matching
     * slab directly beneath it. The final block occupies the lower cell. */
    if (state.id == BETA_BLOCK_SLAB && hit.place_y > 0 &&
        world_get_block(world,hit.place_x,hit.place_y-1,hit.place_z) == BETA_BLOCK_SLAB &&
        world_get_metadata(world,hit.place_x,hit.place_y-1,hit.place_z) == state.metadata) {
        BetaBlockBox full;
        collision_box(BETA_BLOCK_DOUBLE_SLAB,&full);
        if (body_intersects_block(player,hit.place_x,hit.place_y-1,
                                  hit.place_z,&full)) return 0;
        if (!world_set_block(world,hit.place_x,hit.place_y-1,hit.place_z,
                             BETA_BLOCK_DOUBLE_SLAB)) return 0;
        if (!world_set_metadata(world,hit.place_x,hit.place_y-1,
                                hit.place_z,state.metadata)) return 0;
        /* The transient upper slab is removed by ys.c, including any water
         * that its placement replaced. Clear a stale nibble in that cell. */
        if (old != BLOCK_AIR && !world_set_block(world,hit.place_x,
                                                  hit.place_y,hit.place_z,BLOCK_AIR)) return 0;
        return world_set_metadata(world,hit.place_x,hit.place_y,hit.place_z,0);
    }
    if (world_block_def(state.id)->solid) {
        BetaBlockBox box;
        collision_box(state.id, &box);
        if (body_intersects_block(player, hit.place_x, hit.place_y,
                                  hit.place_z, &box)) return 0;
    }
    if (!world_set_block(world,hit.place_x,hit.place_y,hit.place_z,state.id)) return 0;
    return world_set_metadata(world,hit.place_x,hit.place_y,hit.place_z,state.metadata);
}

int player_place_block(Player *player, World *world, uint8_t id)
{
    BetaBlockState state = { id, 0 };
    return player_place_block_state(player,world,state);
}
