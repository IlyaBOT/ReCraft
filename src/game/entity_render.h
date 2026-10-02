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
} RenderEntity;
/* Pick the nearest supported living entity; block_distance occludes targets.
 * Returns an entity id, or -1. Camera yaw/pitch follow local Player radians. */
int entity_pick(const RenderEntity *entities,int count,const RendererCamera *camera,
                float reach,float block_distance);

/* One depth-tested fixed-function batch of simple geometry for remote entities.
   Returns the number actually drawn. The backend owns no textures or buffers. */
int entity_render_draw(RenderEntity *entities, int count,
                       const RendererCamera *camera, int width, int height,
                       int render_distance_chunks,float dt);
int item_drop_draw(const ItemDrop *drops, int count, const RendererCamera *camera,
                   int width, int height, int render_distance_chunks);
void first_person_draw(const InventorySlot *item,int width,int height,float swing,int hurt);
/* Beta inventory biped; screen coordinates and pixels per world unit. Uses
 * the existing skin/model and restores GL state, without a render target. */
void player_inventory_draw(int x,int feet_y,int scale,float mouse_x,float mouse_y,
                           int width,int height);
void mining_cracks_draw(const RendererCamera *camera,int width,int height,
                        int x,int y,int z,BetaBlockState block,float progress);

#endif
