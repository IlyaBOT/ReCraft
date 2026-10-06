#ifndef RECRAFT_ENTITY_RENDER_H
#define RECRAFT_ENTITY_RENDER_H

#include "../renderer/renderer.h"
#include "inventory.h"

struct Player;
typedef struct RenderEntity {
    int active, id, type;
    float x, y, z;
    float yaw,pitch,draw_x,draw_y,draw_z,walk;
    int positioned;
    int color,sheared;
    int fuse,powered,fire;
    unsigned cape,skin;int skin_height,appearance;
    int local_interpolation;
    float previous_x,previous_y,previous_z,phase;
    float draw_yaw,draw_pitch,walk_amount,lerp_remaining,age;
    float vx,vy,vz;
    int on_ground,hurt,death,vehicle_id;
    InventorySlot item;
} RenderEntity;
/* Pick the nearest supported living entity or vehicle; block_distance occludes targets.
 * Returns an entity id, or -1. Camera yaw/pitch follow local Player radians. */
int entity_pick(const RenderEntity *entities,int count,const RendererCamera *camera,
                float reach,float block_distance);
int entity_pick_except(const RenderEntity *entities,int count,const RendererCamera *camera,
                       float reach,float block_distance,int ignored);
void entity_remote_pose(RenderEntity *e,float x,float y,float z,float yaw,float pitch);
void entity_render_update(RenderEntity *entities,int count,float dt,float fraction);
void entity_network_collide(RenderEntity *entities,int count,struct Player *player);
void entity_network_tick(RenderEntity *entities,int count,World *world);
int entity_rider_position(const RenderEntity *entities,int count,int vehicle,int rendered,float *x,float *y,float *z);
void entity_spider_leg_pose(float phase,float amount,int leg,float *yaw,float *roll);

/* One depth-tested fixed-function batch of simple geometry for remote entities.
   Returns the number actually drawn. The backend owns no textures or buffers. */
int entity_render_draw(RenderEntity *entities, int count,
                       const RendererCamera *camera, int width, int height,
                       int render_distance_chunks,float dt);
/* Local entity poses use the same 20 Hz remainder as the player camera. */
void entity_render_tick_fraction(RenderEntity *entities,int count,float fraction);
int item_drop_draw(const ItemDrop *drops, int count, const RendererCamera *camera,
                   int width, int height, int render_distance_chunks);
void first_person_draw(const InventorySlot *item,int width,int height,float swing,int hurt);
typedef struct FirstPersonState { InventorySlot item; int slot; float equip,previous_equip; } FirstPersonState;
void first_person_tick(FirstPersonState *state,const InventorySlot *item,int slot);
void first_person_draw_pose(const InventorySlot *item,int width,int height,float swing,int hurt,float equip,float bob);
void first_person_fire(int width,int height);
/* Beta inventory biped; screen coordinates and pixels per world unit. Uses
 * the existing skin/model and restores GL state, without a render target. */
void player_inventory_draw(int x,int feet_y,int scale,float mouse_x,float mouse_y,
                           int width,int height);
void mining_cracks_draw(const RendererCamera *camera,int width,int height,
                        int x,int y,int z,BetaBlockState block,float progress);

#endif
