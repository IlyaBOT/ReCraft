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
    ASSET_GUI_CRAFTING,
    ASSET_GUI_FURNACE,
    ASSET_GUI_CONTAINER,
    ASSET_PLAYER_SKIN,
    ASSET_RAIN,ASSET_SNOW,ASSET_SUN,ASSET_MOON,
    ASSET_MOB_PIG,ASSET_MOB_SHEEP,ASSET_MOB_SHEEP_FUR,ASSET_MOB_COW,ASSET_MOB_CHICKEN,
    ASSET_MOB_ZOMBIE,ASSET_MOB_SKELETON,ASSET_MOB_SPIDER,ASSET_MOB_CREEPER,
    ASSET_SIGN,ASSET_SERVER_DEFAULT_ICON,
    ASSET_ARROW,ASSET_MINECART,
    ASSET_COUNT
} AssetId;
typedef enum AssetSoundId {
#define SOUND(token,key,path) ASSET_SOUND_##token,
#include "../audio/sound_assets.def"
#undef SOUND
    ASSET_SOUND_COUNT
} AssetSoundId;

void assets_init(const char *game_root);
const char *assets_path(AssetId id, char *buffer, size_t capacity);
Texture2D assets_get_texture(AssetId id);
/* Server icon uploads are render-thread only; 64 entries, validated 64x64 PNG.
 * Missing/invalid icons use the original Minecraft unknown-server asset. */
int assets_set_server_icon(unsigned index,const unsigned char *png,size_t size);
Texture2D assets_get_server_icon(unsigned index);
void assets_clear_server_icons(void);
Image assets_load_image(AssetId id);
Sound assets_get_sound(AssetSoundId id);
AssetSoundId assets_find_sound(const char *key,unsigned variant);
unsigned assets_music_count(void);
const char *assets_music_path(unsigned index,char *buffer,size_t capacity);
const char *assets_record_path(int item,char *buffer,size_t capacity);
void assets_release_sounds(void);
void assets_shutdown(void);

#endif
