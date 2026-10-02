#include "player.h"
#include "../world/block_entity.h"
#include "../world/fluid.h"
#include "bed.h"
#include "sign.h"

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

static void collision_box(BetaBlockState state, BetaBlockBox *box)
{
    unsigned id=state.id;
    if ((id==BETA_BLOCK_SLAB || id==BETA_BLOCK_CACTUS || id==26 || id==93 || id==94 ||
         id==BETA_BLOCK_WOOD_DOOR || id==BETA_BLOCK_IRON_DOOR) && beta_block_selection_box(state,box)) return;
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
                    collision_box((BetaBlockState){id,world_get_metadata(world,x,y,z)}, &box);
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
                           float start, float end, int place[3],float *hit_distance)
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
    *hit_distance=near_t>=0 ? near_t : far_t;
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
    player->health=20; player->air=300;
    for (y = WORLD_HEIGHT - 3; y >= 1; --y) {
        uint8_t id = world_get_block(world,spawn_x,y,spawn_z);
        if (world_block_def(id)->solid) {
            player->y = (float)y + (id == BETA_BLOCK_SLAB ? 0.51f : 1.01f);
            return;
        }
    }
    player->y = world->beta_format ? (float)world->spawn_y : 68.0f;
}

void player_damage(Player *p,int amount)
{
    if (p->creative || p->health<=0 || amount<=0) return;
    if (p->hurt_ticks>10) {
        if (amount<=p->last_damage) return;
        p->health-=amount-p->last_damage;
    } else { p->health-=amount; p->hurt_ticks=20; }
    p->last_damage=amount;
    if (p->health<0) p->health=0;
}
void player_mob_damage(Player *p,const World *w,int amount)
{
    if(w->network_mode || w->difficulty==0) return;
    if(w->difficulty==1) amount=amount/3+1;
    else if(w->difficulty==3) amount=amount*3/2;
    player_damage(p,amount);
}
static void hazards(Player *p,World *w,float old_y)
{
    int x0=(int)floorf(p->x-.3f),x1=(int)floorf(p->x+.3f),z0=(int)floorf(p->z-.3f),z1=(int)floorf(p->z+.3f);
    int x,y,z,lava=0,cactus=0,fire=0,water=0;
    uint8_t eye=world_peek_block(w,(int)floorf(p->x),(int)floorf(p->y+1.62f),(int)floorf(p->z));
    if (w->network_mode || p->creative) return;
    if (p->hurt_ticks>0) --p->hurt_ticks;
    for (y=(int)floorf(p->y);y<= (int)floorf(p->y+1.79f);++y) for (z=z0;z<=z1;++z) for (x=x0;x<=x1;++x) {
        uint8_t id=world_peek_block(w,x,y,z);
        if (id==10 || id==11) lava=1;
        if (id==8 || id==9) water=1;
        if (id==51) fire=1;
        if (id==81) cactus=1;
    }
    if (water) { p->fall_distance=0; p->fire=0; }
    else {
        /* Include the last movement down to the contact face. Omitting it
         * made falls just over the three-block threshold deal no damage. */
        if(p->y<old_y) p->fall_distance+=old_y-p->y;
        if(p->on_ground) { player_damage(p,(int)ceilf(p->fall_distance-3)); p->fall_distance=0; }
    }
    if (eye==8 || eye==9) {
        if (--p->air<=-20) { p->air=0; player_damage(p,2); }
    } else p->air=300;
    if (lava) { player_damage(p,4); p->fire=600; }
    else if (fire) { player_damage(p,1); if (p->fire<=0) p->fire=160; }
    else if (p->fire>0) { if (p->fire%20==0) player_damage(p,1); --p->fire; }
    if (cactus) player_damage(p,1);
    if(world_block_def(eye)->opaque) player_damage(p,1);
    if (p->y< -64) player_damage(p,4);
}
void player_tick(Player *player, World *world, const PlayerInput *input, float dt)
{
    ++player->age;
    if(!world->network_mode && world->difficulty==0 && player->health>0 && player->health<20 && player->age%20==0) ++player->health;
    if(player->sleeping) { player_sleep_tick(player,world); return; }
    float old_y=player->y;
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
        float flow[3]; fluid_flow_vector(world,foot_x,foot_y,foot_z,flow);
        /* World.handleMaterialAcceleration adds a normalized current of .014
         * blocks per Beta tick (.28 blocks/s in our velocity units). */
        player->vx+=flow[0]*.28f; player->vy+=flow[1]*.28f; player->vz+=flow[2]*.28f;
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
    hazards(player,world,old_y);
    if (player->creative && player->y < -32.0f) player_spawn(player, world,1);
}

static BlockHit raycast(const Player *player, World *world, float reach,int sources)
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
        if (id != BLOCK_AIR && (!fluid_kind(id) || (sources && world_get_metadata(world,x,y,z)==0))) {
            int place[3] = { px, py, pz };
            int intersects = 1;
            float face_distance=distance;
            BetaBlockBox box;
            if (beta_block_cross_plant(id) || id == BETA_BLOCK_SLAB ||
                id == BETA_BLOCK_TORCH ||
                id == BETA_BLOCK_UNLIT_REDSTONE_TORCH ||
                id == BETA_BLOCK_REDSTONE_TORCH || id==BETA_BLOCK_CACTUS || id==BETA_BLOCK_NETHER_PORTAL ||
                id==26 || id==93 || id==94 || id==69 || id==77 || id==78 || sign_is_block(id) ||
                id==BETA_BLOCK_WOOD_DOOR || id==BETA_BLOCK_IRON_DOOR) {
                BetaBlockState state = { id, world_get_metadata(world, x, y, z) };
                float end = fminf(reach, fminf(tx, fminf(ty, tz)));
                place[0] = x; place[1] = y; place[2] = z;
                intersects = beta_block_selection_box(state, &box) &&
                    bounded_ray_hit(&box, x, y, z, origin, direction,
                                    distance, end, place,&face_distance);
            }
            if (intersects) {
                result.hit = 1;
                result.x = x; result.y = y; result.z = z;
                result.place_x = place[0]; result.place_y = place[1];
                result.place_z = place[2];
                result.block = id;
                result.distance=face_distance;
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

BlockHit player_raycast(const Player *player,World *world,float reach)
{ return raycast(player,world,reach,0); }
BlockHit player_raycast_sources(const Player *player,World *world,float reach)
{ return raycast(player,world,reach,1); }

int player_use_item(Player *p,World *w,InventorySlot *item)
{
    int heal=0;
    if(!item || item->count<=0 || p->health<=0 || w->network_mode) return 0;
    switch(item->id) {
    case 260: heal=4; break;
    case 282: heal=10; break;
    case 297: heal=5; break;
    case 319: heal=3; break;
    case 320: heal=8; break;
    case 322: heal=42; break;
    case 349: heal=2; break;
    case 350: heal=5; break;
    case 357: heal=1; break;
    default: break;
    }
    /* Beta food heals immediately; it has no modern hunger/use timer. */
    if(heal) {
        p->health+=heal; if(p->health>20) p->health=20;
        if(!p->creative) {
            if(item->id==282) *item=(InventorySlot){281,1,0};
            else if(--item->count<=0) inventory_clear_slot(item);
        }
        return 1;
    }
    if(item->id==325 || item->id==326 || item->id==327) {
        BlockHit hit=raycast(p,w,5,item->id==325);
        if(!hit.hit) return 0;
        if(item->id==325) {
            int kind=fluid_kind(hit.block);
            if(!kind || world_get_metadata(w,hit.x,hit.y,hit.z)!=0 || !world_set_block(w,hit.x,hit.y,hit.z,0)) return 0;
            if(!p->creative) *item=(InventorySlot){kind==1 ? 326 : 327,1,0};
        } else {
            uint8_t old=world_get_block(w,hit.place_x,hit.place_y,hit.place_z);
            if(beta_material_solid(old) || !world_set_block(w,hit.place_x,hit.place_y,hit.place_z,item->id==326 ? 8 : 10)) return 0;
            if(!p->creative) *item=(InventorySlot){325,1,0};
        }
        return 1;
    }
    if(item->id==355) {
        BlockHit hit=player_raycast(p,w,5);
        unsigned dir=(unsigned)(int)floorf(p->yaw*.63661977236f+2.5f)&3;
        if(!hit.hit || hit.place_y!=hit.y+1 || !bed_place(w,hit.place_x,hit.place_y,hit.place_z,dir)) return 0;
        if(!p->creative && --item->count<=0) inventory_clear_slot(item);
        return 1;
    }
    if((item->id==331 || item->id==356) && player_place_block_state(p,w,(BetaBlockState){item->id==331 ? 55 : 93,0})) {
        if(!p->creative && --item->count<=0) inventory_clear_slot(item);
        return 1;
    }
    return 0;
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
    if (old != BLOCK_AIR && !fluid_kind(old)) return 0;
    if ((state.id==BETA_BLOCK_TORCH || state.id==BETA_BLOCK_REDSTONE_TORCH ||
         state.id==BETA_BLOCK_UNLIT_REDSTONE_TORCH) && state.metadata==0) {
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
    if (state.id==BETA_BLOCK_CHEST && !block_chest_can_place(world,hit.place_x,hit.place_y,hit.place_z)) return 0;
    if(state.id==69 || state.id==77) {
        if(!world_block_def(hit.block)->opaque) return 0;
        if(hit.place_y>hit.y) {
            if(state.id==77) return 0;
            state.metadata=(sinf(player->yaw)*sinf(player->yaw)>.5f) ? 6 : 5;
        } else if(hit.place_y<hit.y) return 0;
        else if(hit.place_x>hit.x) state.metadata=1;
        else if(hit.place_x<hit.x) state.metadata=2;
        else if(hit.place_z>hit.z) state.metadata=3;
        else state.metadata=4;
    }
    if(state.id==BETA_BLOCK_REDSTONE_WIRE && !world_block_def(world_get_block(world,hit.place_x,hit.place_y-1,hit.place_z))->opaque) return 0;
    if(state.id==93 || state.id==94) {
        if(!world_block_def(world_get_block(world,hit.place_x,hit.place_y-1,hit.place_z))->opaque) return 0;
        state.metadata=(uint8_t)((int)floorf(player->yaw*.63661977236f+2.5f)+2)&3;
    }
    if (state.id==BETA_BLOCK_CACTUS) {
        static const int dx[4]={-1,1,0,0},dz[4]={0,0,-1,1};
        uint8_t below=world_get_block(world,hit.place_x,hit.place_y-1,hit.place_z);
        int i;
        if (below!=BETA_BLOCK_SAND && below!=BETA_BLOCK_CACTUS) return 0;
        for (i=0;i<4;++i) if (beta_material_solid(world_get_block(world,hit.place_x+dx[i],
                                      hit.place_y,hit.place_z+dz[i]))) return 0;
    }
    if (state.id==BETA_BLOCK_FURNACE || state.id==BETA_BLOCK_BURNING_FURNACE) {
        static const uint8_t facing[4]={2,5,3,4};
        int index=(int)floorf(player->yaw*0.63661977236f+2.5f)&3;
        state.metadata=facing[index];
    }
    /* ys.c in the Beta client merges a newly placed slab with the matching
     * slab directly beneath it. The final block occupies the lower cell. */
    if (state.id == BETA_BLOCK_SLAB && hit.place_y > 0 &&
        world_get_block(world,hit.place_x,hit.place_y-1,hit.place_z) == BETA_BLOCK_SLAB &&
        world_get_metadata(world,hit.place_x,hit.place_y-1,hit.place_z) == state.metadata) {
        BetaBlockBox full;
        collision_box((BetaBlockState){BETA_BLOCK_DOUBLE_SLAB,0},&full);
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
        collision_box(state, &box);
        if (body_intersects_block(player, hit.place_x, hit.place_y,
                                  hit.place_z, &box)) return 0;
    }
    if (!world_set_block(world,hit.place_x,hit.place_y,hit.place_z,state.id)) return 0;
    if (state.id==54 || state.id==61 || state.id==62)
        block_entity_get(world,hit.place_x,hit.place_y,hit.place_z,1);
    return world_set_metadata(world,hit.place_x,hit.place_y,hit.place_z,state.metadata);
}

int player_place_block(Player *player, World *world, uint8_t id)
{
    BetaBlockState state = { id, 0 };
    return player_place_block_state(player,world,state);
}
