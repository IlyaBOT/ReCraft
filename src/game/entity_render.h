#ifndef RECRAFT_ENTITY_RENDER_H
#define RECRAFT_ENTITY_RENDER_H

#include "../renderer/renderer.h"
#include "inventory.h"

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
} RenderEntity;
/* Pick the nearest supported living entity or vehicle; block_distance occludes targets.
 * Returns an entity id, or -1. Camera yaw/pitch follow local Player radians. */
int entity_pick(const RenderEntity *entities,int count,const RendererCamera *camera,
                float reach,float block_distance);

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
