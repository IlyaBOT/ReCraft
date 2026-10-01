#ifndef RECRAFT_ASSETS_H
#define RECRAFT_ASSETS_H

#include <stddef.h>
#include "raylib.h"

typedef enum AssetId {
    ASSET_GUI_WIDGETS,
    ASSET_GUI_BACKGROUND,
    ASSET_GUI_ICONS,
    ASSET_GUI_PANORAMA,
    ASSET_TERRAIN,
    ASSET_FONT_ASCII,
    ASSET_GUI_INVENTORY,
    ASSET_GUI_ITEMS,
    ASSET_COUNT
} AssetId;

void assets_init(const char *game_root);
const char *assets_path(AssetId id, char *buffer, size_t capacity);
Texture2D assets_get_texture(AssetId id);
Image assets_load_image(AssetId id);
void assets_shutdown(void);

#endif
