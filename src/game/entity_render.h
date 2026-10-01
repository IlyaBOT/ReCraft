#ifndef RECRAFT_ENTITY_RENDER_H
#define RECRAFT_ENTITY_RENDER_H

#include "../renderer/renderer.h"
#include "inventory.h"

typedef struct RenderEntity {
    int active, id, type;
    float x, y, z;
} RenderEntity;

/* One depth-tested fixed-function batch of simple geometry for remote entities.
   Returns the number actually drawn. The backend owns no textures or buffers. */
int entity_render_draw(const RenderEntity *entities, int count,
                       const RendererCamera *camera, int width, int height,
                       int render_distance_chunks);
int item_drop_draw(const ItemDrop *drops, int count, const RendererCamera *camera,
                   int width, int height, int render_distance_chunks);

#endif
