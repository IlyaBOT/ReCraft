#include "assets.h"
#include "../util/game_paths.h"

#if defined(__APPLE__)
#include <OpenGL/gl.h>
#else
#include <GL/gl.h>
#endif
#include <stdio.h>
#include <string.h>

static const char *const files[ASSET_COUNT] = {
    "assets/gui/widgets.png", "assets/gui/background.png",
    "assets/gui/icons.png", "assets/gui/panorama.png",
    "assets/textures/terrain.png", "assets/fonts/ascii.png",
    "assets/gui/inventory.png", "assets/gui/items.png",
    "assets/gui/crafting.png", "assets/gui/furnace.png", "assets/gui/container.png",
    "assets/textures/mob/char.png",
    "assets/textures/environment/rain.png","assets/textures/environment/snow.png",
    "assets/textures/terrain/sun.png","assets/textures/terrain/moon.png",
    "assets/textures/mob/pig.png","assets/textures/mob/sheep.png","assets/textures/mob/sheep_fur.png",
    "assets/textures/mob/cow.png","assets/textures/mob/chicken.png","assets/textures/mob/zombie.png",
    "assets/textures/mob/skeleton.png","assets/textures/mob/spider.png","assets/textures/mob/creeper.png"
};
static char root[512];
static Texture2D textures[ASSET_COUNT];
static unsigned char attempted[ASSET_COUNT];
static Texture2D fallback;
static Sound sounds[ASSET_SOUND_COUNT];
static unsigned char sound_attempted[ASSET_SOUND_COUNT];
static const char *const sound_files[ASSET_SOUND_COUNT]={
#define SOUND(token,key,path) "assets/sounds/" path,
#include "../audio/sound_assets.def"
#undef SOUND
};
static const char *const sound_keys[ASSET_SOUND_COUNT]={
#define SOUND(token,key,path) key,
#include "../audio/sound_assets.def"
#undef SOUND
};
static const char *const music_files[]={
#define MUSIC(path) "assets/music/" path,
#include "../audio/music_assets.def"
#undef MUSIC
};
AssetSoundId assets_find_sound(const char *key,unsigned variant)
{
    unsigned i,count=0;
    for(i=0;i<ASSET_SOUND_COUNT;++i) if(!strcmp(key,sound_keys[i])) ++count;
    if(!count) return ASSET_SOUND_COUNT;
    variant%=count;
    for(i=0;i<ASSET_SOUND_COUNT;++i) if(!strcmp(key,sound_keys[i]) && variant--==0) return (AssetSoundId)i;
    return ASSET_SOUND_COUNT;
}
unsigned assets_music_count(void) { return sizeof(music_files)/sizeof(music_files[0]); }
const char *assets_music_path(unsigned index,char *buffer,size_t capacity)
{ return index<assets_music_count() && game_path_join(buffer,capacity,root,music_files[index]) ? buffer : NULL; }

void assets_init(const char *game_root)
{
    snprintf(root, sizeof(root), "%s", game_root ? game_root : "");
    memset(textures, 0, sizeof(textures));
    memset(attempted, 0, sizeof(attempted));
    memset(&fallback, 0, sizeof(fallback));
    memset(sounds,0,sizeof(sounds)); memset(sound_attempted,0,sizeof(sound_attempted));
}

const char *assets_path(AssetId id, char *buffer, size_t capacity)
{
    if (id < 0 || id >= ASSET_COUNT || !game_path_join(buffer, capacity, root, files[id]))
        return NULL;
    return buffer;
}

static void nearest(Texture2D texture, int repeat)
{
    GLint old;
    if (!texture.id) return;
    glGetIntegerv(GL_TEXTURE_BINDING_2D, &old);
    glBindTexture(GL_TEXTURE_2D, texture.id);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, repeat ? GL_REPEAT : GL_CLAMP);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, repeat ? GL_REPEAT : GL_CLAMP);
    glBindTexture(GL_TEXTURE_2D, (GLuint)old);
}

static Texture2D checker(void)
{
    if (!fallback.id) {
        Color pixels[4] = {{255,0,255,255},{0,0,0,255},{0,0,0,255},{255,0,255,255}};
        Image image;
        image.data = pixels; image.width = image.height = 2;
        image.mipmaps = 1; image.format = UNCOMPRESSED_R8G8B8A8;
        fallback = LoadTextureFromImage(image);
        nearest(fallback, 1);
    }
    return fallback;
}

Texture2D assets_get_texture(AssetId id)
{
    char path[768];
    if (id < 0 || id >= ASSET_COUNT) return checker();
    if (!attempted[id]) {
        attempted[id] = 1;
        if (assets_path(id, path, sizeof(path))) textures[id] = LoadTexture(path);
        if (textures[id].id) nearest(textures[id], id == ASSET_GUI_BACKGROUND || id==ASSET_RAIN || id==ASSET_SNOW);
        else fprintf(stderr, "Missing optional asset: %s\n", files[id]);
    }
    return textures[id].id ? textures[id] : checker();
}

Image assets_load_image(AssetId id)
{
    Image image;
    char path[768];
    memset(&image, 0, sizeof(image));
    if (assets_path(id, path, sizeof(path))) image = LoadImage(path);
    if (!image.data && id >= 0 && id < ASSET_COUNT)
        fprintf(stderr, "Missing image asset: %s\n", files[id]);
    return image;
}

Sound assets_get_sound(AssetSoundId id)
{
    Sound empty={0}; char path[768]; FILE *file;
    if(id<0 || id>=ASSET_SOUND_COUNT) return empty;
    if(!sound_attempted[id]) {
        sound_attempted[id]=1;
        if(game_path_join(path,sizeof(path),root,sound_files[id]) && (file=fopen(path,"rb"))!=NULL) {
            fclose(file); sounds[id]=LoadSound(path);
        }
        if(!sounds[id].source) fprintf(stderr,"Missing optional sound: %s\n",sound_files[id]);
    }
    return sounds[id];
}
void assets_release_sounds(void)
{
    int i; for(i=0;i<ASSET_SOUND_COUNT;++i) if(sounds[i].source) UnloadSound(sounds[i]);
    memset(sounds,0,sizeof(sounds)); memset(sound_attempted,0,sizeof(sound_attempted));
}
void assets_shutdown(void)
{
    int i;
    for (i = 0; i < ASSET_COUNT; ++i) if (textures[i].id) UnloadTexture(textures[i]);
    if (fallback.id) UnloadTexture(fallback);
    memset(textures, 0, sizeof(textures));
    memset(attempted, 0, sizeof(attempted));
    memset(&fallback, 0, sizeof(fallback));
}
