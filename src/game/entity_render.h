#ifndef RECRAFT_ENTITY_RENDER_H
#define RECRAFT_ENTITY_RENDER_H

#include "../renderer/renderer.h"
#include "inventory.h"

typedef struct RenderEntity {
    int active, id, type;
    float x, y, z;
    float yaw,pitch,draw_x,draw_y,draw_z,walk;
    int positioned;
} RenderEntity;

/* One depth-tested fixed-function batch of simple geometry for remote entities.
   Returns the number actually drawn. The backend owns no textures or buffers. */
int entity_render_draw(RenderEntity *entities, int count,
                       const RendererCamera *camera, int width, int height,
                       int render_distance_chunks,float dt);
int item_drop_draw(const ItemDrop *drops, int count, const RendererCamera *camera,
                   int width, int height, int render_distance_chunks);
void first_person_draw(const InventorySlot *item,int width,int height,float swing,int hurt);
void mining_cracks_draw(const RendererCamera *camera,int width,int height,
                        int x,int y,int z,BetaBlockState block,float progress);

#endif
