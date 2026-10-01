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
    "assets/gui/inventory.png", "assets/gui/items.png"
};
static char root[512];
static Texture2D textures[ASSET_COUNT];
static unsigned char attempted[ASSET_COUNT];
static Texture2D fallback;

void assets_init(const char *game_root)
{
    snprintf(root, sizeof(root), "%s", game_root ? game_root : "");
    memset(textures, 0, sizeof(textures));
    memset(attempted, 0, sizeof(attempted));
    memset(&fallback, 0, sizeof(fallback));
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
        if (textures[id].id) nearest(textures[id], id == ASSET_GUI_BACKGROUND);
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

void assets_shutdown(void)
{
    int i;
    for (i = 0; i < ASSET_COUNT; ++i) if (textures[i].id) UnloadTexture(textures[i]);
    if (fallback.id) UnloadTexture(fallback);
    memset(textures, 0, sizeof(textures));
    memset(attempted, 0, sizeof(attempted));
    memset(&fallback, 0, sizeof(fallback));
}
