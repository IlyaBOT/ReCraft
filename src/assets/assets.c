#include "assets.h"
#include "server_icon_png.h"
#include "../util/game_paths.h"

#if defined(__APPLE__)
#include <OpenGL/gl.h>
#else
#include <GL/gl.h>
#endif
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

/* These symbols are supplied by pinned raylib's existing stb_image decoder. */
extern unsigned char *stbi_load_from_memory(const unsigned char *,int,int *,int *,int *,int);
extern void stbi_image_free(void *);

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
    "assets/textures/mob/skeleton.png","assets/textures/mob/spider.png","assets/textures/mob/creeper.png",
    "assets/textures/item/sign.png","assets/gui/unknown_server.png",
    "assets/textures/entity/arrows.png","assets/textures/entity/cart.png"
};
static char root[512];
static Texture2D textures[ASSET_COUNT];
static unsigned char attempted[ASSET_COUNT];
static Texture2D fallback;
static Texture2D server_icons[64];
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
const char *assets_record_path(int item,char *buffer,size_t capacity)
{
    const char *path=item==2256 ? "assets/records/13.ogg" : item==2257 ? "assets/records/cat.ogg" : NULL;
    return path && game_path_join(buffer,capacity,root,path) ? buffer : NULL;
}
const char *assets_music_path(unsigned index,char *buffer,size_t capacity)
{ return index<assets_music_count() && game_path_join(buffer,capacity,root,music_files[index]) ? buffer : NULL; }

void assets_init(const char *game_root)
{
    snprintf(root, sizeof(root), "%s", game_root ? game_root : "");
    memset(textures, 0, sizeof(textures));
    memset(attempted, 0, sizeof(attempted));
    memset(&fallback, 0, sizeof(fallback));
    memset(server_icons,0,sizeof(server_icons));
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
        if(id==ASSET_SIGN && !textures[id].id &&
            game_path_join(path,sizeof(path),root,"assets/textures/entity/sign.png")) {
            fprintf(stderr,"Missing preferred sign texture; using original Beta sign.\n");
            textures[id]=LoadTexture(path);
        }
        if (textures[id].id) nearest(textures[id], id == ASSET_GUI_BACKGROUND || id==ASSET_RAIN || id==ASSET_SNOW);
        else fprintf(stderr, "Missing optional asset: %s\n", files[id]);
    }
    return textures[id].id ? textures[id] : checker();
}

int assets_set_server_icon(unsigned index,const unsigned char *png,size_t size)
{
    Image image;unsigned char *pixels;int width,height,channels;
    if(index>=64)return 0;
    if(server_icons[index].id)UnloadTexture(server_icons[index]);
    memset(server_icons+index,0,sizeof(*server_icons));
    if(!size)return 1;
    if(!server_icon_png_valid(png,size))return 0;
    pixels=stbi_load_from_memory(png,(int)size,&width,&height,&channels,4);if(!pixels)return 0;
    if(width!=64||height!=64){stbi_image_free(pixels);return 0;}
    image.data=pixels;image.width=width;image.height=height;image.mipmaps=1;image.format=UNCOMPRESSED_R8G8B8A8;
    server_icons[index]=LoadTextureFromImage(image);stbi_image_free(pixels);nearest(server_icons[index],0);return server_icons[index].id!=0;
}
Texture2D assets_get_server_icon(unsigned index)
{return index<64&&server_icons[index].id?server_icons[index]:assets_get_texture(ASSET_SERVER_DEFAULT_ICON);}
void assets_clear_server_icons(void)
{unsigned i;for(i=0;i<64;++i)if(server_icons[i].id)UnloadTexture(server_icons[i]);memset(server_icons,0,sizeof(server_icons));}

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
    assets_clear_server_icons();
    for (i = 0; i < ASSET_COUNT; ++i) if (textures[i].id) UnloadTexture(textures[i]);
    if (fallback.id) UnloadTexture(fallback);
    memset(textures, 0, sizeof(textures));
    memset(attempted, 0, sizeof(attempted));
    memset(&fallback, 0, sizeof(fallback));
}
