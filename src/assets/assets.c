#include "assets.h"
#include "resource_pack.h"
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
#include <stdint.h>

/* These symbols are supplied by pinned raylib's existing stb_image decoder. */
extern unsigned char *stbi_load_from_memory(const unsigned char *,int,int *,int *,int *,int);
extern void stbi_image_free(void *);
extern int stbi_info_from_memory(const unsigned char *,int,int *,int *,int *);

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
    "assets/textures/entity/arrows.png","assets/textures/entity/cart.png","assets/textures/entity/boat.png","assets/gui/language.png"
};
static char root[512];
static Texture2D textures[ASSET_COUNT];
static unsigned char attempted[ASSET_COUNT];
static Texture2D fallback;
static Image player_skin;
static struct { Texture2D texture; unsigned page,age; } unicode_pages[16];
static unsigned unicode_age;
static struct { char id[300]; Texture2D texture; unsigned age; } pack_icons[16];
static unsigned pack_age;
static unsigned animation_mask=15;
unsigned assets_animation_mask(void) { return animation_mask; }
static const char *const legacy[ASSET_COUNT]={
    "gui/gui.png","dirt.png","gui/icons.png",NULL,"terrain.png","font/default.png",
    "gui/inventory.png","gui/items.png","gui/crafting.png","gui/furnace.png","gui/container.png",
    "mob/char.png","environment/rain.png","environment/snow.png","terrain/sun.png","terrain/moon.png",
    "mob/pig.png","mob/sheep.png","mob/sheep_fur.png","mob/cow.png","mob/chicken.png",
    "mob/zombie.png","mob/skeleton.png","mob/spider.png","mob/creeper.png",
    "item/sign.png",NULL,"item/arrows.png","item/cart.png","item/boat.png",NULL
};
static const char *const modern[ASSET_COUNT]={
    "gui/widgets.png","gui/options_background.png","gui/icons.png",NULL,NULL,"font/ascii.png",
    "gui/container/inventory.png",NULL,"gui/container/crafting_table.png","gui/container/furnace.png","gui/container/generic_54.png",
    "entity/steve.png","environment/rain.png","environment/snow.png","environment/sun.png","environment/moon_phases.png"
};
void *assets_read_file(const char *relative,size_t *size)
{
    void *bytes; char path[1024],alias[600]; FILE *f; long n;
    if(size) *size=0;
    if(!relative || !*relative || *relative=='/' || strchr(relative,'\\') || strchr(relative,':') || strstr(relative,"..")) return NULL;
    bytes=resource_pack_read(relative,size); if(bytes) return bytes;
    snprintf(alias,sizeof(alias),"assets/%s",relative); bytes=resource_pack_read(alias,size); if(bytes) return bytes;
    snprintf(alias,sizeof(alias),"assets/minecraft/%s",relative); bytes=resource_pack_read(alias,size); if(bytes) return bytes;
    if(!strncmp(relative,"fonts/unicode/",14)) {
        unsigned page;
        snprintf(alias,sizeof(alias),"font/%s",relative+14); bytes=resource_pack_read(alias,size); if(bytes) return bytes;
        if(sscanf(relative+14,"glyph_%2x.png",&page)==1) {
            snprintf(alias,sizeof(alias),"font/glyph_%02X.png",page); bytes=resource_pack_read(alias,size); if(bytes) return bytes;
            snprintf(alias,sizeof(alias),"assets/minecraft/textures/font/unicode_page_%02x.png",page);
        } else snprintf(alias,sizeof(alias),"assets/minecraft/font/glyph_sizes.bin");
        bytes=resource_pack_read(alias,size); if(bytes) return bytes;
    }
    if(!game_path_join(path,sizeof(path),root,"assets") || strlen(path)+strlen(relative)+2>=sizeof(path)) return NULL;
    strcat(path,"/"); strcat(path,relative); f=fopen(path,"rb"); if(!f) return NULL;
    if(fseek(f,0,SEEK_END) || (n=ftell(f))<0 || n>32L*1024*1024 || fseek(f,0,SEEK_SET)) { fclose(f); return NULL; }
    bytes=malloc((size_t)n+1);
    if(!bytes || fread(bytes,1,(size_t)n,f)!=(size_t)n) { free(bytes); fclose(f); return NULL; }
    ((char *)bytes)[n]=0; fclose(f); if(size) *size=(size_t)n; return bytes;
}
static Image image_memory(const void *bytes,size_t size)
{
    Image image={0}; int channels;
    if(!bytes || size>INT32_MAX) return image;
    if(!stbi_info_from_memory(bytes,(int)size,&image.width,&image.height,&channels) ||
       image.width<=0 || image.height<=0 || image.width>2048 || image.height>2048) return image;
    image.data=stbi_load_from_memory(bytes,(int)size,&image.width,&image.height,&channels,4);
    if(image.data && (image.width>2048 || image.height>2048)) { stbi_image_free(image.data); memset(&image,0,sizeof(image)); }
    image.mipmaps=1; image.format=UNCOMPRESSED_R8G8B8A8; return image;
}
static Image pack_image(AssetId id)
{
    Image image={0}; void *bytes; size_t size; char alias[600];
    bytes=resource_pack_read(files[id]+7,&size);
    if(!bytes) bytes=resource_pack_read(files[id],&size);
    if(!bytes && legacy[id]) bytes=resource_pack_read(legacy[id],&size);
    if(!bytes && modern[id]) {
        snprintf(alias,sizeof(alias),"assets/minecraft/textures/%s",modern[id]); bytes=resource_pack_read(alias,&size);
        if(!bytes) { snprintf(alias,sizeof(alias),"textures/%s",modern[id]); bytes=resource_pack_read(alias,&size); }
    }
    if(bytes) { image=image_memory(bytes,size); free(bytes); }
    return image;
}
static void point_resize(Image *image,int width,int height)
{
    unsigned char *pixels; int x,y;
    if(!image->data || (image->width==width && image->height==height)) return;
    if(image->format!=UNCOMPRESSED_R8G8B8A8) ImageFormat(image,UNCOMPRESSED_R8G8B8A8);
    pixels=(unsigned char *)malloc((size_t)width*height*4); if(!pixels) return;
    for(y=0;y<height;++y) for(x=0;x<width;++x)
        memcpy(pixels+((size_t)y*width+x)*4,(unsigned char *)image->data+((size_t)(y*image->height/height)*image->width+x*image->width/width)*4,4);
    UnloadImage(*image); image->data=pixels; image->width=width; image->height=height; image->mipmaps=1; image->format=UNCOMPRESSED_R8G8B8A8;
}
static Image block_image(const char *name)
{
    static const char *prefix[]={"assets/minecraft/textures/blocks/","textures/blocks/","assets/minecraft/textures/block/","textures/block/"};
    unsigned i; char path[200]; size_t size; void *bytes; Image image={0};
    for(i=0;i<4;++i) { snprintf(path,sizeof(path),"%s%s.png",prefix[i],name); bytes=resource_pack_read(path,&size);
        if(bytes) { image=image_memory(bytes,size); free(bytes); if(image.data) break; } }
    return image;
}
static void terrain_overrides(Image *image)
{
    static const struct { int tile; const char *name; } tiles[]={
        {0,"grass_top"},{1,"stone"},{2,"dirt"},{3,"grass_side"},{4,"planks_oak"},{16,"cobblestone"},
        {18,"sand"},{19,"gravel"},{20,"log_oak"},{21,"log_oak_top"},{7,"brick"},{17,"bedrock"},
        {32,"gold_ore"},{33,"iron_ore"},{34,"coal_ore"},{50,"diamond_ore"},{51,"redstone_ore"},
        {52,"leaves_oak"},{49,"glass"},{37,"obsidian"},{36,"cobblestone_mossy"},
        {43,"crafting_table_top"},{59,"crafting_table_side"},{60,"crafting_table_front"},
        {62,"furnace_front_off"},{45,"furnace_side"},{61,"furnace_front_on"},{44,"furnace_top"},
        {64,"door_wood_upper"},{81,"door_wood_lower"},{82,"door_iron_lower"},{65,"door_iron_upper"},
        {80,"torch_on"},{99,"redstone_torch_on"},{115,"redstone_torch_off"},
        {96,"lever"},{131,"repeater_off"},{147,"repeater_on"},{128,"rail_normal"},{112,"rail_normal_turned"},
        {163,"rail_golden"},{179,"rail_golden_powered"},{195,"rail_detector"},
        {69,"cactus_top"},{70,"cactus_side"},{71,"cactus_bottom"},{11,"web"},
        {66,"snow"},{67,"ice"},{72,"clay"},{74,"jukebox_side"},{75,"jukebox_top"},
        {103,"netherrack"},{104,"soul_sand"},{105,"glowstone"},{134,"bed_feet_top"},{135,"bed_head_top"},
        {149,"bed_feet_end"},{150,"bed_feet_side"},{151,"bed_head_side"},{152,"bed_head_end"},
        {205,"water_still"},{206,"water_flow"},{237,"lava_still"},{238,"lava_flow"},{14,"portal"},
        {8,"tnt_side"},{9,"tnt_top"},{10,"tnt_bottom"},{31,"fire_layer_0"},{47,"fire_layer_1"}
    };
    unsigned i; int x,y;
    if(!image->data || !resource_pack_current()[0]) return;
    point_resize(image,256,256); if(image->format!=UNCOMPRESSED_R8G8B8A8) ImageFormat(image,UNCOMPRESSED_R8G8B8A8);
    for(i=0;i<sizeof(tiles)/sizeof(tiles[0]);++i) {
        Image tile=block_image(tiles[i].name);
        if(!tile.data) continue;
        if(tile.height<tile.width) { UnloadImage(tile); continue; }
        if(tiles[i].tile==205 || tiles[i].tile==206) animation_mask&=~1u;
        if(tiles[i].tile==237 || tiles[i].tile==238) animation_mask&=~2u;
        if(tiles[i].tile==14) animation_mask&=~4u;
        if(tiles[i].tile==31 || tiles[i].tile==47) animation_mask&=~8u;
        /* Animated strips use their first square frame. Fixed-function packs
         * need no shader or runtime dependency on blockstates/models. */
        for(y=0;y<16;++y) for(x=0;x<16;++x)
            memcpy((unsigned char *)image->data+((size_t)(tiles[i].tile/16*16+y)*256+tiles[i].tile%16*16+x)*4,
                   (unsigned char *)tile.data+((size_t)(y*tile.width/16)*tile.width+x*tile.width/16)*4,4);
        UnloadImage(tile);
    }
}
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
    resource_pack_init(root);
    animation_mask=15;
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
        {
            Image image=assets_load_image(id);
            if(image.data) { textures[id]=LoadTextureFromImage(image); UnloadImage(image); }
        }
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
int assets_skin_png_valid(const unsigned char *png,size_t size)
{
    int width,height,channels;
    static const unsigned char signature[8]={137,80,78,71,13,10,26,10};
    return png && size>=33 && size<=1024*1024 && !memcmp(png,signature,8) &&
        stbi_info_from_memory(png,(int)size,&width,&height,&channels) && width==64 && (height==32||height==64);
}
static void skin_alpha(Image *image)
{
    int x,y,transparent=0;unsigned char *pixels;
    if(image->format!=UNCOMPRESSED_R8G8B8A8) ImageFormat(image,UNCOMPRESSED_R8G8B8A8);
    pixels=(unsigned char *)image->data;
    /* ImageBufferDownload forces the base skin opaque; old skin exporters
     * left the unused right upper quarter opaque until a hat was painted. */
    for(y=0;y<16;++y) for(x=32;x<64;++x) if(pixels[(y*64+x)*4+3]<128) transparent=1;
    for(y=0;y<32;++y) for(x=0;x<64;++x) {
        if((y<16&&x<32)||y>=16) pixels[(y*64+x)*4+3]=255;
        else if(image->height==32 && !transparent) pixels[(y*64+x)*4+3]=0;
    }
    if(image->height==64) for(y=48;y<64;++y) for(x=16;x<48;++x) pixels[(y*64+x)*4+3]=255;
}
int assets_set_player_skin(const unsigned char *png,size_t size)
{
    Image image={0};
    if(size) {
        if(!assets_skin_png_valid(png,size)) return 0;
        image=image_memory(png,size);if(!image.data) return 0;skin_alpha(&image);
    }
    if(player_skin.data) UnloadImage(player_skin);
    player_skin=image;
    if(textures[ASSET_PLAYER_SKIN].id) UnloadTexture(textures[ASSET_PLAYER_SKIN]);
    textures[ASSET_PLAYER_SKIN]=(Texture2D){0};attempted[ASSET_PLAYER_SKIN]=0;return 1;
}

Image assets_load_image(AssetId id)
{
    Image image;
    char path[768];
    memset(&image, 0, sizeof(image));
    if(id<0 || id>=ASSET_COUNT) return image;
    if(id==ASSET_TERRAIN) animation_mask=15;
    if(id==ASSET_PLAYER_SKIN && player_skin.data) {
        image=player_skin;image.data=malloc((size_t)image.width*image.height*4);
        if(image.data) memcpy(image.data,player_skin.data,(size_t)image.width*image.height*4);
    } else image=pack_image(id);
    if (!image.data && assets_path(id, path, sizeof(path))) image = LoadImage(path);
    if(image.data) {
        if(id==ASSET_FONT_ASCII) point_resize(&image,128,128);
        else if(id==ASSET_PLAYER_SKIN) {
            point_resize(&image,64,image.width==image.height?64:32);
            skin_alpha(&image);
        } else if(id==ASSET_BOAT || id==ASSET_MINECART || id==ASSET_SIGN) point_resize(&image,64,32);
        else if(id==ASSET_TERRAIN || id==ASSET_GUI_WIDGETS || id==ASSET_GUI_ICONS || id==ASSET_GUI_ITEMS ||
            id==ASSET_GUI_INVENTORY || id==ASSET_GUI_CRAFTING || id==ASSET_GUI_FURNACE || id==ASSET_GUI_CONTAINER || id==ASSET_GUI_LANGUAGE)
            point_resize(&image,256,256);
        if(id==ASSET_TERRAIN) terrain_overrides(&image);
    }
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
    if(player_skin.data) UnloadImage(player_skin);
    memset(&player_skin,0,sizeof(player_skin));
    assets_clear_server_icons();
    for (i = 0; i < ASSET_COUNT; ++i) if (textures[i].id) UnloadTexture(textures[i]);
    if (fallback.id) UnloadTexture(fallback);
    for(i=0;i<16;++i) if(unicode_pages[i].texture.id) UnloadTexture(unicode_pages[i].texture);
    for(i=0;i<16;++i) if(pack_icons[i].texture.id) UnloadTexture(pack_icons[i].texture);
    memset(pack_icons,0,sizeof(pack_icons));
    memset(unicode_pages,0,sizeof(unicode_pages));
    resource_pack_shutdown();
    memset(textures, 0, sizeof(textures));
    memset(attempted, 0, sizeof(attempted));
    memset(&fallback, 0, sizeof(fallback));
}
int assets_select_pack(const char *id)
{
    int i;
    if(!resource_pack_select(id)) return 0;
    animation_mask=15;
    for(i=0;i<ASSET_COUNT;++i) if(textures[i].id) UnloadTexture(textures[i]);
    memset(textures,0,sizeof(textures)); memset(attempted,0,sizeof(attempted));
    for(i=0;i<16;++i) if(unicode_pages[i].texture.id) UnloadTexture(unicode_pages[i].texture);
    memset(unicode_pages,0,sizeof(unicode_pages)); return 1;
}
Texture2D assets_unicode_page(unsigned page)
{
    unsigned i,oldest=0; char name[80]; void *bytes; size_t size; Image image;
    if(page>255) return (Texture2D){0};
    for(i=0;i<16;++i) {
        if(unicode_pages[i].texture.id && unicode_pages[i].page==page) { unicode_pages[i].age=++unicode_age; return unicode_pages[i].texture; }
        if(!unicode_pages[i].texture.id || unicode_pages[i].age<unicode_pages[oldest].age) oldest=i;
    }
    snprintf(name,sizeof(name),"fonts/unicode/glyph_%02x.png",page); bytes=assets_read_file(name,&size);
    image=image_memory(bytes,size); free(bytes);
    if(!image.data) return (Texture2D){0};
    point_resize(&image,256,256);
    if(unicode_pages[oldest].texture.id) UnloadTexture(unicode_pages[oldest].texture);
    unicode_pages[oldest].texture=LoadTextureFromImage(image); UnloadImage(image);
    unicode_pages[oldest].page=page; unicode_pages[oldest].age=++unicode_age;
    nearest(unicode_pages[oldest].texture,0); return unicode_pages[oldest].texture;
}
Texture2D assets_pack_icon(const char *id)
{
    unsigned i,oldest=0; Image image; void *bytes; size_t size;
    if(!id || !*id) return assets_get_texture(ASSET_GUI_PANORAMA);
    for(i=0;i<16;++i) {
        if(!strcmp(pack_icons[i].id,id)) { pack_icons[i].age=++pack_age; return pack_icons[i].texture.id ? pack_icons[i].texture : assets_get_texture(ASSET_SERVER_DEFAULT_ICON); }
        if(!pack_icons[i].id[0] || pack_icons[i].age<pack_icons[oldest].age) oldest=i;
    }
    bytes=resource_pack_read_from(id,"pack.png",&size); image=image_memory(bytes,size); free(bytes);
    if(pack_icons[oldest].texture.id) UnloadTexture(pack_icons[oldest].texture);
    memset(pack_icons+oldest,0,sizeof(pack_icons[oldest]));
    snprintf(pack_icons[oldest].id,sizeof(pack_icons[oldest].id),"%s",id); pack_icons[oldest].age=++pack_age;
    if(image.data) { point_resize(&image,64,64); pack_icons[oldest].texture=LoadTextureFromImage(image); UnloadImage(image); nearest(pack_icons[oldest].texture,0); }
    return pack_icons[oldest].texture.id ? pack_icons[oldest].texture : assets_get_texture(ASSET_SERVER_DEFAULT_ICON);
}
