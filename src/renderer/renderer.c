#include "renderer.h"
#include "../world/redstone.h"
#include "../world/rail.h"
#include "../assets/assets.h"
#include "../world/fluid.h"
#include "../world/environment.h"
#include "../world/block_entity.h"
#include "../game/sign.h"
#include "../ui/pixel_font.h"
#include "texture_animation.h"

#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <GL/gl.h>
#elif defined(__APPLE__)
#include <OpenGL/gl.h>
#include <OpenGL/OpenGL.h>
#include <dlfcn.h>
#else
#include <GL/gl.h>
#include <GL/glx.h>
#endif

#include <math.h>
#include <stdio.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#ifndef APIENTRY
#define APIENTRY
#endif

#ifndef GL_ARRAY_BUFFER_ARB
#define GL_ARRAY_BUFFER_ARB 0x8892
#endif
#ifndef GL_ARRAY_BUFFER_BINDING_ARB
#define GL_ARRAY_BUFFER_BINDING_ARB 0x8894
#endif
#ifndef GL_STATIC_DRAW_ARB
#define GL_STATIC_DRAW_ARB 0x88E4
#endif
#ifndef GL_TEXTURE_MAX_LEVEL
#define GL_TEXTURE_MAX_LEVEL 0x813D
#endif

#define ATLAS_TILES 16
#define TILE_PIXELS 16
#define TILE_REPEATS 4
#define ATLAS_SLOT_PIXELS (TILE_PIXELS * TILE_REPEATS)
#define ATLAS_PIXELS (ATLAS_TILES * ATLAS_SLOT_PIXELS)
#define MAX_QUADS_PER_LAYER 65536u
/* A fixed ceiling protects old unified-memory drivers even when their
   advertised VRAM/GART figures are large, dynamic, or unavailable. */
#define DEFAULT_VBO_BUDGET_MB 16u
#define VERTEX_COORD_SCALE 128.0f
#ifndef GL_MAX_TEXTURE_UNITS_ARB
#define GL_MAX_TEXTURE_UNITS_ARB 0x84E2
#endif

typedef void (APIENTRY *GenericProc)(void);
typedef void (APIENTRY *GenBuffersProc)(GLsizei, GLuint *);
typedef void (APIENTRY *BindBufferProc)(GLenum, GLuint);
typedef void (APIENTRY *BufferDataProc)(GLenum, ptrdiff_t, const void *, GLenum);
typedef void (APIENTRY *DeleteBuffersProc)(GLsizei, const GLuint *);

/* GL 1.1 accepts signed-short position/texture coordinates. Positions are
   chunk-local in 1/128 blocks; modelview scales and translates them.
   This needs no per-vertex CPU conversion when drawing client arrays. */
typedef struct VoxelVertex {
    int16_t x, y, z, pad;
    int16_t u, v;
    uint8_t r, g, b, a;
} VoxelVertex;
typedef char VoxelVertex_must_be_16_bytes[(sizeof(VoxelVertex) == 16) ? 1 : -1];

typedef struct MeshLayer {
    VoxelVertex *vertices;
    uint32_t vertex_count;
    uint32_t capacity;
    GLuint vbo;
    uint32_t vbo_bytes;
    uint8_t overflow;
} MeshLayer;

typedef struct ChunkMesh {
    MeshLayer layers[3];
    DeleteBuffersProc delete_buffers;
    Renderer *owner;
    int32_t chunk_x, chunk_z;
    uint32_t revision;
    uint8_t animations;
    struct SignInstance *signs;
    unsigned sign_count;
} ChunkMesh;

typedef struct SignInstance { uint8_t x,y,z,metadata,id; } SignInstance;

typedef struct VisibleChunk {
    const ChunkMesh *mesh;
    float distance2;
} VisibleChunk;

struct Renderer {
    GLuint atlas;
    GenBuffersProc gen_buffers;
    BindBufferProc bind_buffer;
    BufferDataProc buffer_data;
    DeleteBuffersProc delete_buffers;
    RendererStats stats;
    GpuCapabilities capabilities;
    RendererOptions options;
    World *hooked_world;
    float last_far_plane;
    uint64_t vbo_live_bytes;
    int vbo_upload_disabled;
    int mip_level_limit;
    VisibleChunk *visible_scratch;
    size_t visible_capacity;
    TextureAnimation animations;
    uint64_t animation_tick;
    unsigned animated_visible;
};

static void upload_animated_slot(unsigned slot,const uint8_t source[1024],int scroll)
{
    uint8_t pixels[ATLAS_SLOT_PIXELS*ATLAS_SLOT_PIXELS*4];
    unsigned x,y,size=ATLAS_SLOT_PIXELS,level=0;
    for(y=0;y<size;++y) for(x=0;x<size;++x)
        memcpy(pixels+(y*size+x)*4,source+(((y+scroll)&15)*16+(x&15))*4,4);
    for(;;) {
        glTexSubImage2D(GL_TEXTURE_2D,(GLint)level,(slot%ATLAS_TILES)*size,(slot/ATLAS_TILES)*size,size,size,GL_RGBA,GL_UNSIGNED_BYTE,pixels);
        if(level==4) break;
        for(y=0;y<size/2;++y) for(x=0;x<size/2;++x) {
            unsigned c;
            for(c=0;c<4;++c) {
                unsigned a=((y*2)*size+x*2)*4+c;
                pixels[(y*(size/2)+x)*4+c]=(uint8_t)((pixels[a]+pixels[a+4]+pixels[a+size*4]+pixels[a+size*4+4])/4);
            }
        }
        size/=2; ++level;
    }
}
void renderer_animate(Renderer *r,uint64_t tick)
{
    GLint old,unpack;
    unsigned visible;
    if(!r || !r->atlas || r->animation_tick==tick) return;
    r->animation_tick=tick;
    visible=r->animated_visible&assets_animation_mask();
    if(!visible) return;
    texture_animation_step(&r->animations);
    glGetIntegerv(GL_TEXTURE_BINDING_2D,&old); glGetIntegerv(GL_UNPACK_ALIGNMENT,&unpack);
    glBindTexture(GL_TEXTURE_2D,r->atlas); glPixelStorei(GL_UNPACK_ALIGNMENT,1);
    if(visible&1) {
        upload_animated_slot(11,r->animations.water_pixels,0);
        upload_animated_slot(83,r->animations.water_pixels,0);
        upload_animated_slot(84,r->animations.water_pixels,(int)tick);
    }
    if(visible&2) {
        upload_animated_slot(64,r->animations.lava_pixels,0);
        upload_animated_slot(65,r->animations.lava_pixels,(int)(tick/3));
    }
    if(visible&4) upload_animated_slot(66,r->animations.portal[tick&31],0);
    if(visible&8) { upload_animated_slot(121,r->animations.fire_pixels,0);upload_animated_slot(122,r->animations.fire_pixels,0); }
    glPixelStorei(GL_UNPACK_ALIGNMENT,unpack); glBindTexture(GL_TEXTURE_2D,(GLuint)old);
}
static GenericProc gl_proc(const char *name)
{
#if defined(_WIN32)
    PROC proc = wglGetProcAddress(name);
    if (!proc || (uintptr_t)proc <= 3u || (intptr_t)proc == -1) {
        HMODULE library = GetModuleHandleA("opengl32.dll");
        proc = library ? (PROC)GetProcAddress(library, name) : NULL;
    }
    return (GenericProc)proc;
#elif defined(__APPLE__)
    void *address = dlsym(RTLD_DEFAULT, name);
    GenericProc proc = NULL;
    memcpy(&proc, &address, sizeof(proc));
    return proc;
#else
    return (GenericProc)glXGetProcAddressARB((const GLubyte *)name);
#endif
}

static int has_extension(const char *extensions, const char *wanted)
{
    const size_t wanted_length = strlen(wanted);
    const char *cursor = extensions;
    if (!cursor) return 0;
    while ((cursor = strstr(cursor, wanted)) != NULL) {
        if ((cursor == extensions || cursor[-1] == ' ') &&
            (cursor[wanted_length] == '\0' || cursor[wanted_length] == ' ')) return 1;
        cursor += wanted_length;
    }
    return 0;
}

static void copy_gl_string(char *destination, size_t capacity, GLenum which)
{
    const GLubyte *source = glGetString(which);
    if (!capacity) return;
    if (!source) {
        destination[0] = '\0';
        return;
    }
    strncpy(destination, (const char *)source, capacity - 1);
    destination[capacity - 1] = '\0';
}

static void probe_platform_memory(GpuCapabilities *capabilities)
{
    capabilities->gpu_vertex_processing = -1;
#if defined(__APPLE__)
    {
        CGLContextObj context = CGLGetCurrentContext();
        CGLPixelFormatObj pixel_format;
        GLint gpu_vertices = 0, current_renderer_id = 0, count = 0;
        GLint virtual_screen = -1, display_mask = 0;
        CGLRendererInfoObj info = NULL;
        int i;
        if (!context) return;
        if (CGLGetParameter(context, kCGLCPGPUVertexProcessing, &gpu_vertices) == kCGLNoError)
            capabilities->gpu_vertex_processing = gpu_vertices ? 1 : 0;
        pixel_format = CGLGetPixelFormat(context);
        if (!pixel_format ||
            CGLGetVirtualScreen(context, &virtual_screen) != kCGLNoError ||
            virtual_screen < 0 ||
            CGLDescribePixelFormat(pixel_format, virtual_screen, kCGLPFADisplayMask,
                                   &display_mask) != kCGLNoError ||
            display_mask == 0) return;
        if (CGLGetParameter(context, kCGLCPCurrentRendererID,
                            &current_renderer_id) != kCGLNoError) return;
        /* The 10.6 CGL byte-valued properties are driver reports. Match both
           the current virtual screen's display mask and current renderer ID. */
        if (CGLQueryRendererInfo((GLuint)display_mask, &info, &count) != kCGLNoError ||
            !info)
            return;
        for (i = 0; i < count; ++i) {
            GLint id = 0, bytes = 0;
            if (CGLDescribeRenderer(info, i, kCGLRPRendererID, &id) != kCGLNoError ||
                id != current_renderer_id) continue;
            if (CGLDescribeRenderer(info, i, kCGLRPVideoMemory, &bytes) == kCGLNoError &&
                bytes > 0) {
                capabilities->vram_report_valid = 1;
                capabilities->reported_vram_bytes = (uint64_t)(unsigned)bytes;
            }
            bytes = 0;
            if (CGLDescribeRenderer(info, i, kCGLRPTextureMemory, &bytes) == kCGLNoError &&
                bytes > 0) {
                capabilities->texture_memory_report_valid = 1;
                capabilities->reported_texture_memory_bytes = (uint64_t)(unsigned)bytes;
            }
            break;
        }
        CGLDestroyRendererInfo(info);
    }
#else
    (void)capabilities;
#endif
    /* There is no portable OpenGL GART capacity/usage query. The Apple
       texture-memory property is not a GART counter and must not be relabelled. */
}

static uint32_t pixel_hash(uint32_t tile, uint32_t x, uint32_t y)
{
    uint32_t value = tile * 0x9e3779b9u ^ x * 0x85ebca6bu ^ y * 0xc2b2ae35u;
    value ^= value >> 16;
    value *= 0x7feb352du;
    value ^= value >> 15;
    value *= 0x846ca68bu;
    return value ^ (value >> 16);
}

static uint8_t clamp_byte(int value)
{
    if (value < 0) return 0;
    if (value > 255) return 255;
    return (uint8_t)value;
}

/* World tile IDs are stable and shared with world_block_def(). */
static void atlas_pixel(unsigned tile, unsigned x, unsigned y, uint8_t rgba[4])
{
    const uint32_t noise = pixel_hash(tile, x, y);
    const int grain = (int)(noise % 25u) - 12;
    int r = 180, g = 55, b = 180, a = 255;
    switch (tile) {
        case 0: r = g = b = 132 + grain; break; /* slab top fallback */
        case 1: r = 122 + grain; g = 122 + grain; b = 126 + grain; break; /* stone */
        case 2: r = 112 + grain; g = 76 + grain; b = 50 + grain / 2; break; /* dirt */
        case 3: r = 81 + grain; g = 153 + grain; b = 55 + grain / 2; break; /* grass */
        case 4: /* grass side */
            if (y >= 12u + (noise % 3u)) {
                r = 81 + grain; g = 150 + grain; b = 52 + grain / 2;
            } else {
                r = 111 + grain; g = 77 + grain; b = 48 + grain / 2;
            }
            break;
        case 5: r = 212 + grain; g = 197 + grain; b = 142 + grain; break; /* sand */
        case 6: r = 139 + grain; g = 135 + grain; b = 129 + grain; break; /* gravel */
        case 7: /* cobblestone */
            r = 126 + grain; g = 126 + grain; b = 129 + grain;
            if ((x == 0u || y == 0u || (x == 8u && y < 9u)) && (noise & 3u))
                r = g = b = 77;
            break;
        case 8: { /* log end */
            int dx = (int)x - 7, dy = (int)y - 7;
            int ring = (dx * dx + dy * dy) / 13;
            r = 146 + grain - (ring & 1) * 23;
            g = 105 + grain - (ring & 1) * 17;
            b = 61 + grain / 2;
            break;
        }
        case 9: /* log bark */
            r = 102 + grain + ((x / 3u) & 1u) * 12;
            g = 72 + grain; b = 39 + grain / 2;
            break;
        case 10: /* leaf cutouts */
            r = 58 + grain; g = 132 + grain; b = 48 + grain / 2;
            a = (noise % 11u == 0u) ? 0 : 255;
            break;
        case 11: /* water */
            r = 47 + grain / 2; g = 100 + grain / 2; b = 182 + grain;
            if (((y + x / 3u) % 7u) == 0u) { g += 16; b += 13; }
            a = 178;
            break;
        case 12: /* glass */
            r = 185 + grain / 2; g = 218 + grain / 2; b = 224 + grain / 2;
            a = (x == 0u || y == 0u || x == 15u || y == 15u) ? 190 : 45;
            break;
        case 13: /* torch */
            if (y > 10u) { r = 245; g = 188 + grain; b = 69; }
            else { r = 117 + grain; g = 75 + grain; b = 37 + grain / 2; }
            break;
        default:
            if (((x / 4u) ^ (y / 4u)) & 1u) { r = 60; g = 45; b = 65; }
            break;
    }
    rgba[0] = clamp_byte(r);
    rgba[1] = clamp_byte(g);
    rgba[2] = clamp_byte(b);
    rgba[3] = clamp_byte(a);
}

static void apply_atlas_filter(Renderer *renderer)
{
    GLint old_binding = 0;
    int level = renderer->options.mipmap;
    if (!renderer->atlas) return;
    if (level < 0) level = 0;
    if (level > 4) level = 4;
    if (!renderer->mip_level_limit) level = 0;
    glGetIntegerv(GL_TEXTURE_BINDING_2D, &old_binding);
    glBindTexture(GL_TEXTURE_2D, renderer->atlas);
    if (renderer->mip_level_limit)
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, level);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER,
                    level ? GL_NEAREST_MIPMAP_LINEAR : GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glBindTexture(GL_TEXTURE_2D, (GLuint)old_binding);
}

/* Stable slot-to-terrain mapping is shared with block texture selection. */
static int beta_source_tile(unsigned slot)
{ return beta_render_source_tile(slot); }

static uint64_t atlas_upload_bytes(void)
{
    unsigned dimension = ATLAS_PIXELS;
    uint64_t bytes = 0;
    for (;;) {
        bytes += (uint64_t)dimension * dimension * 4u;
        if (dimension == 1u) break;
        dimension /= 2u;
    }
    return bytes;
}

static GLuint make_atlas(void)
{
    /* Animated water keeps the small procedural fallback for now. */
    Image beta = assets_load_image(ASSET_TERRAIN);
    const uint8_t *beta_pixels = NULL;
    const size_t pixel_count = (size_t)ATLAS_PIXELS * ATLAS_PIXELS;
    uint8_t *pixels = (uint8_t *)malloc(pixel_count * 4u);
    uint8_t *current;
    GLuint texture = 0;
    GLint old_unpack = 4, old_binding = 0;
    unsigned tile, x, y, dimension, level;
    if (!pixels) { if (beta.data) UnloadImage(beta); return 0; }
    if (beta.data && beta.format != UNCOMPRESSED_R8G8B8A8)
        ImageFormat(&beta, UNCOMPRESSED_R8G8B8A8);
    if (beta.data && beta.width == 256 && beta.height == 256)
        beta_pixels = (const uint8_t *)beta.data;
    /* Four repetitions per material preserve 16-pixel block scale with
       greedy quads and GL 1.1; all block variants share this texture. */
    for (tile = 0; tile < ATLAS_TILES * ATLAS_TILES; ++tile) {
        unsigned tx = tile % ATLAS_TILES, ty = tile / ATLAS_TILES;
        int source_tile = beta_source_tile(tile);
        for (y = 0; y < ATLAS_SLOT_PIXELS; ++y) {
            for (x = 0; x < ATLAS_SLOT_PIXELS; ++x) {
                size_t offset = ((size_t)(ty * ATLAS_SLOT_PIXELS + y) * ATLAS_PIXELS +
                                 tx * ATLAS_SLOT_PIXELS + x) * 4u;
                if (beta_pixels && source_tile >= 0) {
                    unsigned sx = ((unsigned)source_tile % 16u)*16u + x % TILE_PIXELS;
                    unsigned sy = ((unsigned)source_tile / 16u)*16u + y % TILE_PIXELS;
                    memcpy(pixels + offset, beta_pixels + ((size_t)sy*256u+sx)*4u, 4u);
                    /* Reduced-transparency door variants keep the original
                     * panel pixels and fill its holes with matching material.
                     * Runtime-only atlas slots leave pack images untouched. */
                    if(tile>=106 && tile<=109) {
                        unsigned background=tile<=107 ? 4u : 22u,alpha=pixels[offset+3],channel;
                        unsigned bx=(background%16)*16+x%16,by=(background/16)*16+y%16;
                        const uint8_t *solid=beta_pixels+((size_t)by*256+bx)*4;
                        for(channel=0;channel<3;++channel)
                            pixels[offset+channel]=(uint8_t)((pixels[offset+channel]*alpha+solid[channel]*(255-alpha)+127)/255);
                        pixels[offset+3]=255;
                    }
                    /* Beta biome-neutral grass/leaves are grayscale. Apply a
                       fixed plains tint once, without per-frame work. */
                    if (tile == 3 || tile == 10 || tile == 16 || tile == 52 || tile == 54 || tile==99 || tile==100) {
                        pixels[offset+0] = clamp_byte(pixels[offset+0]*4/5);
                        pixels[offset+1] = clamp_byte(pixels[offset+1]*5/4);
                        pixels[offset+2] = clamp_byte(pixels[offset+2]*2/3);
                    }
                } else atlas_pixel(tile, x % TILE_PIXELS, y % TILE_PIXELS, pixels + offset);
            }
        }
    }
    if (beta.data) UnloadImage(beta);
    glGetIntegerv(GL_UNPACK_ALIGNMENT, &old_unpack);
    glGetIntegerv(GL_TEXTURE_BINDING_2D, &old_binding);
    glGenTextures(1, &texture);
    if (!texture) { free(pixels); return 0; }
    glBindTexture(GL_TEXTURE_2D, texture);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP);
    current = pixels;
    dimension = ATLAS_PIXELS;
    for (level = 0; ; ++level) {
        glTexImage2D(GL_TEXTURE_2D, (GLint)level, GL_RGBA,
                     (GLsizei)dimension, (GLsizei)dimension, 0,
                     GL_RGBA, GL_UNSIGNED_BYTE, current);
        if (dimension == 1u) break;
        {
            unsigned next_dimension = dimension / 2u;
            uint8_t *next = (uint8_t *)malloc((size_t)next_dimension * next_dimension * 4u);
            if (!next) {
                if (current != pixels) free(current);
                free(pixels);
                glDeleteTextures(1, &texture);
                glBindTexture(GL_TEXTURE_2D, (GLuint)old_binding);
                glPixelStorei(GL_UNPACK_ALIGNMENT, old_unpack);
                return 0;
            }
            for (y = 0; y < next_dimension; ++y) {
                for (x = 0; x < next_dimension; ++x) {
                    unsigned channel;
                    for (channel = 0; channel < 4u; ++channel) {
                        size_t source = ((size_t)(y * 2u) * dimension + x * 2u) * 4u + channel;
                        unsigned sum = (unsigned)current[source] +
                            current[source + 4u] +
                            current[source + (size_t)dimension * 4u] +
                            current[source + (size_t)dimension * 4u + 4u];
                        next[((size_t)y * next_dimension + x) * 4u + channel] =
                            (uint8_t)((sum + 2u) / 4u);
                    }
                }
            }
            if (current != pixels) free(current);
            current = next;
            dimension = next_dimension;
        }
    }
    if (current != pixels) free(current);
    free(pixels);
    glBindTexture(GL_TEXTURE_2D, (GLuint)old_binding);
    glPixelStorei(GL_UNPACK_ALIGNMENT, old_unpack);
    return texture;
}
static void chunk_mesh_destroy(void *pointer)
{
    ChunkMesh *mesh = (ChunkMesh *)pointer;
    int layer;
    if (!mesh) return;
    for (layer = 0; layer < 3; ++layer) {
        free(mesh->layers[layer].vertices);
        if (mesh->layers[layer].vbo && mesh->delete_buffers) {
            mesh->delete_buffers(1, &mesh->layers[layer].vbo);
            if (mesh->owner && mesh->owner->vbo_live_bytes >= mesh->layers[layer].vbo_bytes)
                mesh->owner->vbo_live_bytes -= mesh->layers[layer].vbo_bytes;
        }
    }
    free(mesh->signs);
    free(mesh);
}

Renderer *renderer_init(void)
{
    Renderer *renderer = (Renderer *)calloc(1, sizeof(*renderer));
    const char *extensions;
    if (!renderer) return NULL;
    renderer->options.vbo_mode = RENDERER_VBO_AUTO;
    renderer->options.vbo_budget_mb = DEFAULT_VBO_BUDGET_MB;
    renderer->options.greedy = 1;
    renderer->options.fog = 1;
    renderer->options.mipmap = 0;
    renderer->options.smooth_lighting = 0;
    renderer->options.transparent_leaves = 0;
    renderer->options.brightness = 50;
    renderer->stats.vbo_budget_bytes = DEFAULT_VBO_BUDGET_MB * 1048576u;
    renderer->last_far_plane = 160.0f;
    copy_gl_string(renderer->stats.gpu_vendor, sizeof(renderer->stats.gpu_vendor), GL_VENDOR);
    copy_gl_string(renderer->stats.gpu_renderer, sizeof(renderer->stats.gpu_renderer), GL_RENDERER);
    copy_gl_string(renderer->stats.gpu_version, sizeof(renderer->stats.gpu_version), GL_VERSION);
    extensions = (const char *)glGetString(GL_EXTENSIONS);
    if (has_extension(extensions, "GL_ARB_vertex_buffer_object")) {
        renderer->gen_buffers = (GenBuffersProc)gl_proc("glGenBuffersARB");
        renderer->bind_buffer = (BindBufferProc)gl_proc("glBindBufferARB");
        renderer->buffer_data = (BufferDataProc)gl_proc("glBufferDataARB");
        renderer->delete_buffers = (DeleteBuffersProc)gl_proc("glDeleteBuffersARB");
        renderer->stats.vbo_available = renderer->gen_buffers && renderer->bind_buffer &&
                                        renderer->buffer_data && renderer->delete_buffers;
    }
    renderer->stats.using_vbo = 0;
    memcpy(renderer->capabilities.vendor, renderer->stats.gpu_vendor,
           sizeof(renderer->capabilities.vendor));
    memcpy(renderer->capabilities.renderer, renderer->stats.gpu_renderer,
           sizeof(renderer->capabilities.renderer));
    memcpy(renderer->capabilities.version, renderer->stats.gpu_version,
           sizeof(renderer->capabilities.version));
    probe_platform_memory(&renderer->capabilities);
    renderer->stats.gpu_vertex_processing = renderer->capabilities.gpu_vertex_processing;
    renderer->stats.vram_report_valid = renderer->capabilities.vram_report_valid;
    renderer->stats.texture_memory_report_valid =
        renderer->capabilities.texture_memory_report_valid;
    renderer->stats.gart_report_valid = renderer->capabilities.gart_report_valid;
    renderer->stats.reported_vram_bytes = renderer->capabilities.reported_vram_bytes;
    renderer->stats.reported_texture_memory_bytes =
        renderer->capabilities.reported_texture_memory_bytes;
    renderer->stats.reported_gart_bytes = renderer->capabilities.reported_gart_bytes;
    (void)sscanf(renderer->capabilities.version, "%d.%d",
                 &renderer->capabilities.gl_major, &renderer->capabilities.gl_minor);
    renderer->mip_level_limit = renderer->capabilities.gl_major > 1 ||
        (renderer->capabilities.gl_major == 1 && renderer->capabilities.gl_minor >= 2) ||
        has_extension(extensions, "GL_SGIS_texture_lod");
    renderer->capabilities.mipmap_level_control = renderer->mip_level_limit;
    glGetIntegerv(GL_MAX_TEXTURE_SIZE, &renderer->capabilities.max_texture_size);
    renderer->capabilities.arb_vbo =
        has_extension(extensions, "GL_ARB_vertex_buffer_object");
    renderer->capabilities.vbo_functions = renderer->stats.vbo_available;
    renderer->capabilities.arb_multitexture =
        has_extension(extensions, "GL_ARB_multitexture");
    renderer->capabilities.arb_npot =
        has_extension(extensions, "GL_ARB_texture_non_power_of_two");
    renderer->capabilities.ext_anisotropic =
        has_extension(extensions, "GL_EXT_texture_filter_anisotropic");
    renderer->capabilities.texture_compression =
        has_extension(extensions, "GL_ARB_texture_compression") ||
        has_extension(extensions, "GL_EXT_texture_compression_s3tc");
    renderer->capabilities.occlusion_query =
        has_extension(extensions, "GL_ARB_occlusion_query");
    renderer->capabilities.generate_mipmap =
        has_extension(extensions, "GL_SGIS_generate_mipmap") ||
        has_extension(extensions, "GL_EXT_framebuffer_object") ||
        has_extension(extensions, "GL_ARB_framebuffer_object");
    renderer->capabilities.max_texture_units = 1;
    if (renderer->capabilities.arb_multitexture)
        glGetIntegerv(GL_MAX_TEXTURE_UNITS_ARB, &renderer->capabilities.max_texture_units);
    fprintf(stderr,
            "ReCraft GL: %s | %s | %s\n"
            "  texture max=%d, units=%d, VBO ext=%d proc=%d, multitexture=%d, NPOT=%d, anisotropic=%d\n"
            "  compression=%d, occlusion query=%d, generate mipmap=%d\n",
            renderer->capabilities.vendor, renderer->capabilities.renderer,
            renderer->capabilities.version,
            renderer->capabilities.max_texture_size,
            renderer->capabilities.max_texture_units,
            renderer->capabilities.arb_vbo,
            renderer->capabilities.vbo_functions,
            renderer->capabilities.arb_multitexture,
            renderer->capabilities.arb_npot,
            renderer->capabilities.ext_anisotropic,
            renderer->capabilities.texture_compression,
            renderer->capabilities.occlusion_query,
            renderer->capabilities.generate_mipmap);
    fprintf(stderr, "  mip level control=%s\n",
            renderer->mip_level_limit ? "available" : "unavailable (nearest fallback)");
    fprintf(stderr,
            "  vertex processing=%s, driver VRAM=%s, texture memory=%s, GART=unknown; "
            "VBO ceiling=%u MiB (reports do not set budget)\n",
            renderer->capabilities.gpu_vertex_processing < 0 ? "unknown" :
                (renderer->capabilities.gpu_vertex_processing ? "GPU" : "CPU"),
            renderer->capabilities.vram_report_valid ? "reported" : "unknown",
            renderer->capabilities.texture_memory_report_valid ? "reported" : "unknown",
            DEFAULT_VBO_BUDGET_MB);
    renderer->atlas = make_atlas();
    texture_animation_init(&renderer->animations);
    renderer->stats.texture_bytes = atlas_upload_bytes();
    apply_atlas_filter(renderer);
    if (!renderer->atlas) {
        free(renderer);
        return NULL;
    }
    return renderer;
}

GpuCapabilities renderer_capabilities(const Renderer *renderer)
{
    GpuCapabilities empty = {0};
    return renderer ? renderer->capabilities : empty;
}
void renderer_reload_assets(Renderer *renderer)
{
    GLuint atlas;
    if(!renderer) return;
    atlas=make_atlas(); if(!atlas) return;
    glDeleteTextures(1,&renderer->atlas); renderer->atlas=atlas;
    apply_atlas_filter(renderer);
}

RendererOptions renderer_options(const Renderer *renderer)
{
    RendererOptions empty = {0};
    return renderer ? renderer->options : empty;
}

void renderer_set_options(Renderer *renderer, World *world, RendererOptions options)
{
    RendererOptions previous;
    size_t index, count;
    int mesh_change;
    if (!renderer) return;
    previous = renderer->options;
    if (options.vbo_mode < RENDERER_VBO_AUTO || options.vbo_mode > RENDERER_VBO_OFF)
        options.vbo_mode = RENDERER_VBO_AUTO;
    if (options.vbo_budget_mb <= 0) options.vbo_budget_mb = DEFAULT_VBO_BUDGET_MB;
    else if (options.vbo_budget_mb < 8) options.vbo_budget_mb = 4;
    else if (options.vbo_budget_mb < 16) options.vbo_budget_mb = 8;
    else if (options.vbo_budget_mb < 32) options.vbo_budget_mb = 16;
    else options.vbo_budget_mb = 32;
    options.greedy = !!options.greedy;
    options.fog = !!options.fog;
    options.smooth_lighting = !!options.smooth_lighting;
    options.transparent_leaves = !!options.transparent_leaves;
    options.reduced_transparency = !!options.reduced_transparency;
    if (options.mipmap < 0) options.mipmap = 0;
    if (options.mipmap > 4) options.mipmap = 4;
    if (options.brightness < 0) options.brightness = 0;
    if (options.brightness > 100) options.brightness = 100;
    renderer->options = options;
    renderer->stats.vbo_budget_bytes = (uint64_t)options.vbo_budget_mb * 1048576u;
    renderer->stats.using_vbo = 0;
    if (options.mipmap != previous.mipmap) apply_atlas_filter(renderer);
    mesh_change = options.greedy != previous.greedy ||
                  options.smooth_lighting != previous.smooth_lighting ||
                  options.transparent_leaves != previous.transparent_leaves ||
                  options.reduced_transparency != previous.reduced_transparency ||
                  options.brightness != previous.brightness ||
                  options.vbo_mode != previous.vbo_mode ||
                  options.vbo_budget_mb != previous.vbo_budget_mb;
    if (!world || !mesh_change) return;
    count = world_cached_chunk_count(world);
    for (index = 0; index < count; ++index) {
        Chunk *chunk = world_cached_chunk_at(world, index);
        if (chunk) chunk->dirty_flags |= CHUNK_DIRTY_MESH;
    }
}

static uint8_t light_at(const World *world, const Chunk *chunk, int x, int y, int z)
{
    const Chunk *source = chunk;
    if (y >= WORLD_HEIGHT) return (uint8_t)(15-world->sky_subtracted);
    if (y < 0) return 0;
    if (x < 0 || z < 0 || x >= WORLD_CHUNK_SIZE || z >= WORLD_CHUNK_SIZE) {
        int32_t cx = chunk->x, cz = chunk->z;
        if (x < 0) { --cx; x += WORLD_CHUNK_SIZE; }
        else if (x >= WORLD_CHUNK_SIZE) { ++cx; x -= WORLD_CHUNK_SIZE; }
        if (z < 0) { --cz; z += WORLD_CHUNK_SIZE; }
        else if (z >= WORLD_CHUNK_SIZE) { ++cz; z -= WORLD_CHUNK_SIZE; }
        source = world_peek_chunk(world, cx, cz);
        if (!source) return (uint8_t)(15-world->sky_subtracted);
    }
    {
        int sky = chunk_get_sky_light(source, x, y, z)-world->sky_subtracted;
        uint8_t block = chunk_get_block_light(source, x, y, z);
        return sky > block ? (uint8_t)sky : block;
    }
}

static uint8_t block_at(const World *world, const Chunk *chunk, int x, int y, int z)
{
    if (y < 0 || y >= WORLD_HEIGHT) return BLOCK_AIR;
    if (x >= 0 && x < WORLD_CHUNK_SIZE && z >= 0 && z < WORLD_CHUNK_SIZE)
        return chunk_get_block(chunk, x, y, z);
    return world_peek_block(world, chunk->x * WORLD_CHUNK_SIZE + x,
                            y, chunk->z * WORLD_CHUNK_SIZE + z);
}

static int visible_face(const Renderer *renderer, uint8_t self, uint8_t neighbor)
{
    const BlockDef *self_def, *neighbor_def;
    if (self == BLOCK_AIR || self == BLOCK_TORCH || self == BETA_BLOCK_SLAB ||
        self==BETA_BLOCK_REDSTONE_TORCH || self==BETA_BLOCK_UNLIT_REDSTONE_TORCH ||
        self==BETA_BLOCK_CACTUS || self==BETA_BLOCK_NETHER_PORTAL || self==BETA_BLOCK_REDSTONE_WIRE ||
        self==26 || self==93 || self==94 || self==69 || self==77 || self==78 || self==51 || self==70 || self==72 || self==29 || self==33 || self==34 || self==36 ||
        self==BETA_BLOCK_WOOD_DOOR || self==BETA_BLOCK_IRON_DOOR ||
        self==BETA_BLOCK_STANDING_SIGN || self==BETA_BLOCK_WALL_SIGN || self==BETA_BLOCK_WEB ||
        beta_block_cross_plant(self) || rail_is(self))
        return 0;
    if (fluid_kind(self) && fluid_kind(self)==fluid_kind(neighbor))
        return 0;
    self_def = world_block_def(self);
    if (!self_def || self_def->render_layer == BLOCK_LAYER_NONE) return 0;
    if (neighbor == BLOCK_AIR) return 1;
    neighbor_def = world_block_def(neighbor);
    if (!neighbor_def || neighbor_def->render_layer == BLOCK_LAYER_NONE) return 1;
    if (neighbor_def->opaque) return 0;
    if (self==BLOCK_LEAVES && neighbor==BLOCK_LEAVES &&
        renderer->options.transparent_leaves && !renderer->options.reduced_transparency)
        return 1;
    if (self == neighbor) return 0;
    return 1;
}

static uint8_t block_face_tile(const BlockDef *definition, uint8_t block,
                               uint8_t metadata, int axis, int direction)
{
    unsigned face=axis==1 ? (direction>0 ? 1u : 0u) :
        axis==2 ? (direction>0 ? 3u : 2u) : (direction>0 ? 5u : 4u);
    int source=beta_block_terrain_tile((BetaBlockState){block,metadata},face);
    if (source>=0) return (uint8_t)beta_render_tile(source);
    if (axis==1) return direction>0 ? definition->texture_top : definition->texture_bottom;
    return definition->texture_side;
}

static uint8_t render_layer(const Renderer *renderer,uint8_t block)
{
    if (renderer->options.reduced_transparency &&
        (block==BLOCK_LEAVES || block==BETA_BLOCK_NETHER_PORTAL ||
         block==BETA_BLOCK_WOOD_DOOR || block==BETA_BLOCK_IRON_DOOR))
        return BLOCK_LAYER_OPAQUE;
    if (block==BLOCK_LEAVES || block==BETA_BLOCK_WOOD_DOOR || block==BETA_BLOCK_IRON_DOOR ||
        block==BETA_BLOCK_WEB || block==BETA_BLOCK_BED)
        return BLOCK_LAYER_CUTOUT;
    return world_block_def(block)->render_layer;
}

static int chest_face_flips_u(uint8_t block,int axis,int direction)
{
    /* Beta RenderBlocks reverses U on north (-Z) and east (+X) faces.
     * BlockChest's half selection relies on that orientation: the latch
     * belongs at the join, and the outer borders belong at the ends. */
    return block==BETA_BLOCK_CHEST &&
        ((axis==2 && direction<0) || (axis==0 && direction>0));
}

static uint32_t face_key(const Renderer *renderer, uint8_t block, uint8_t metadata,
                         int axis, int direction, uint8_t light)
{
    const BlockDef *definition = world_block_def(block);
    uint8_t tile, layer = render_layer(renderer,block);
    tile = block_face_tile(definition, block, metadata, axis, direction);
    if (block==BLOCK_LEAVES && renderer->options.reduced_transparency)
        tile=(uint8_t)beta_render_tile((metadata&3u)==1u ? 133 : 53);
    return (uint32_t)block | ((uint32_t)tile << 8) |
           ((uint32_t)(light & 15u) << 16) |
           ((uint32_t)layer << 20) |
           ((uint32_t)(fluid_kind(block) ?
                        metadata&15u : 0u) << 24);
}

static int16_t texcoord(unsigned tile, int upper, int vertical, int span)
{
    unsigned base;
    float pixels;
    /* Unknown registry tiles use the last diagnostic checkerboard slot. */
    if (tile >= ATLAS_TILES * ATLAS_TILES) tile = ATLAS_TILES * ATLAS_TILES - 1;
    base = ((vertical ? tile / ATLAS_TILES : tile % ATLAS_TILES) * ATLAS_SLOT_PIXELS);
    /* Every block advances exactly 16 texels. Half-texel insets applied to a
     * whole greedy rectangle changed the phase/scale of neighboring blocks
     * whenever an edit changed the rectangle's width. */
    pixels=(float)base+(upper ? (float)(span*TILE_PIXELS) : 0);
    return (int16_t)(pixels * (32767.0f / (float)ATLAS_PIXELS) + 0.5f);
}

static int layer_reserve(MeshLayer *layer, uint32_t count)
{
    VoxelVertex *grown;
    uint32_t capacity;
    if (layer->overflow || count > MAX_QUADS_PER_LAYER * 4u) return 0;
    if (count <= layer->capacity) return 1;
    capacity = layer->capacity ? layer->capacity : 1024u;
    while (capacity < count && capacity < MAX_QUADS_PER_LAYER * 4u)
        capacity *= 2u;
    if (capacity > MAX_QUADS_PER_LAYER * 4u) capacity = MAX_QUADS_PER_LAYER * 4u;
    grown = (VoxelVertex *)realloc(layer->vertices, (size_t)capacity * sizeof(*grown));
    if (!grown) return 0;
    layer->vertices = grown;
    layer->capacity = capacity;
    return 1;
}

static void emit_quad(const Renderer *renderer, const World *world,
                      ChunkMesh *mesh, const Chunk *chunk, uint32_t key,
                      int axis, int direction, int slice,
                      int u0, int v0, int u_size, int v_size)
{
    static const int u_axis[3] = {1, 2, 0};
    static const int v_axis[3] = {2, 0, 1};
    const uint8_t tile = (uint8_t)(key >> 8);
    const uint8_t light = (uint8_t)((key >> 16) & 15u);
    const uint8_t render_layer = (uint8_t)((key >> 20) & 3u);
    const uint8_t water_level=(uint8_t)((key >> 24)&15u);
    const int water=fluid_kind((uint8_t)key)!=0;
    const int target_layer = (int)render_layer - 1;
    const int u = u_axis[axis], v = v_axis[axis];
    int corners[4][3] = {{0}};
    float shade, illumination;
    uint8_t color;
    MeshLayer *target;
    uint32_t base;
    int16_t us[4], vs[4];
    int i;
    if (target_layer < 0 || target_layer >= 3) return;
    target = &mesh->layers[target_layer];
    if (!layer_reserve(target, target->vertex_count + 4u)) {
        target->overflow = 1;
        return;
    }
    corners[0][axis] = corners[1][axis] = corners[2][axis] = corners[3][axis] =
        slice + (direction > 0);
    corners[0][u] = corners[3][u] = u0;
    corners[1][u] = corners[2][u] = u0 + u_size;
    corners[0][v] = corners[1][v] = v0;
    corners[2][v] = corners[3][v] = v0 + v_size;
    if (axis == 0) {
        /* X-facing walls use Z for texture U and height for texture V. */
        us[0] = us[1] = texcoord(tile, 0, 0, v_size);
        us[2] = us[3] = texcoord(tile, 1, 0, v_size);
        vs[0] = vs[3] = texcoord(tile, 0, 1, u_size);
        vs[1] = vs[2] = texcoord(tile, 1, 1, u_size);
    } else {
        us[0] = us[3] = texcoord(tile, 0, 0, u_size);
        us[1] = us[2] = texcoord(tile, 1, 0, u_size);
        vs[0] = vs[1] = texcoord(tile, 0, 1, v_size);
        vs[2] = vs[3] = texcoord(tile, 1, 1, v_size);
    }
    /* PNG row zero is the top of the picture. Vertical faces must map their
     * top vertices there, independently of the greedy rectangle's size. */
    if (axis!=1) {
        int16_t a=texcoord(tile,0,1,axis==0 ? u_size : v_size);
        int16_t b=texcoord(tile,1,1,axis==0 ? u_size : v_size);
        for (i=0;i<4;++i) vs[i]=(int16_t)(a+b-vs[i]);
    }
    if (chest_face_flips_u((uint8_t)key,axis,direction)) {
        int span=axis==0 ? v_size : u_size;
        int a=texcoord(tile,0,0,span),b=texcoord(tile,1,0,span);
        for (i=0;i<4;++i) us[i]=(int16_t)(a+b-us[i]);
    }
    shade = axis == 1 ? (direction > 0 ? 1.0f : 0.54f) :
            (axis == 0 ? 0.79f : 0.69f);
    base = target->vertex_count;
    for (i = 0; i < 4; ++i) {
        const int order = direction > 0 ? i : (i == 0 ? 0 : 4 - i);
        float corner_light = (float)light;
        VoxelVertex *vertex = &target->vertices[base + (uint32_t)i];
        if (renderer->options.smooth_lighting) {
            int su, sv, sum = 0;
            for (sv = -1; sv <= 0; ++sv) {
                for (su = -1; su <= 0; ++su) {
                    int sample[3] = {
                        corners[order][0], corners[order][1], corners[order][2]
                    };
                    sample[axis] = slice + direction;
                    sample[u] += su;
                    sample[v] += sv;
                    sum += light_at(world, chunk, sample[0], sample[1], sample[2]);
                }
            }
            corner_light = (float)sum * 0.25f;
        }
        {
            float ambient = 0.06f + (float)renderer->options.brightness * 0.0034f;
            illumination = ambient + (1.0f - ambient) * (corner_light / 15.0f);
        }
        color = clamp_byte((int)(255.0f * shade * illumination));
        vertex->x = (int16_t)(corners[order][0] * (int)VERTEX_COORD_SCALE);
        vertex->y = (int16_t)(corners[order][1] * (int)VERTEX_COORD_SCALE);
        if (water && (axis!=1 || direction>0)) {
            int upper=corners[0][1],j;
            int height128=(int)floorf((1-(float)((water_level>=8 ? 0 : water_level)+1)/9)*VERTEX_COORD_SCALE+0.5f);
            for (j=1;j<4;++j) if (corners[j][1]>upper) upper=corners[j][1];
            if (corners[order][1]==upper)
                vertex->y=(int16_t)(vertex->y-(int)VERTEX_COORD_SCALE+height128);
        }
        vertex->z = (int16_t)(corners[order][2] * (int)VERTEX_COORD_SCALE);
        vertex->pad = 0;
        vertex->u = us[order];
        vertex->v = vs[order];
        vertex->r = color;
        vertex->g = color;
        vertex->b = color;
        vertex->a = 255;
    }
    target->vertex_count += 4u;
}

static void emit_torch_kind(ChunkMesh *mesh,int x,int y,int z,uint8_t metadata,uint8_t id)
{
    static const uint8_t faces[5][4] = {
        {1, 5, 6, 2}, {3, 7, 4, 0}, {2, 6, 7, 3},
        {0, 4, 5, 1}, {4, 7, 6, 5}
    };
    const BlockDef *definition = world_block_def(id);
    MeshLayer *layer;
    float positions[8][3];
    uint32_t base;
    int face, corner;
    int target = (int)definition->render_layer - 1;
    if (target < 0 || target >= 3) return;
    layer = &mesh->layers[target];
    if (!layer_reserve(layer, layer->vertex_count + 20u)) {
        layer->overflow = 1;
        return;
    }
    {
        float base_x=0.5f, top_x=0.5f, base_z=0.5f, top_z=0.5f;
        float y0=0.0f, y1=0.6f;
        /* vm's attachment metadata: four wall faces and floor/default.
         * Keep each small prism inside that state's selection bounds. */
        switch (metadata & 7u) {
        case 1: base_x=0.075f; top_x=0.225f; break;
        case 2: base_x=0.925f; top_x=0.775f; break;
        case 3: base_z=0.075f; top_z=0.225f; break;
        case 4: base_z=0.925f; top_z=0.775f; break;
        default: break;
        }
        if ((metadata & 7u)>=1u && (metadata & 7u)<=4u) {
            y0=0.2f; y1=0.8f;
        }
        const float box[8][3] = {
            {(float)x+base_x-0.0625f,(float)y+y0,(float)z+base_z-0.0625f},
            {(float)x+base_x+0.0625f,(float)y+y0,(float)z+base_z-0.0625f},
            {(float)x+base_x+0.0625f,(float)y+y0,(float)z+base_z+0.0625f},
            {(float)x+base_x-0.0625f,(float)y+y0,(float)z+base_z+0.0625f},
            {(float)x+top_x-0.0625f,(float)y+y1,(float)z+top_z-0.0625f},
            {(float)x+top_x+0.0625f,(float)y+y1,(float)z+top_z-0.0625f},
            {(float)x+top_x+0.0625f,(float)y+y1,(float)z+top_z+0.0625f},
            {(float)x+top_x-0.0625f,(float)y+y1,(float)z+top_z+0.0625f}
        };
        memcpy(positions, box, sizeof(positions));
    }
    base = layer->vertex_count;
    for (face = 0; face < 5; ++face) {
        for (corner = 0; corner < 4; ++corner) {
            const float *point = positions[faces[face][corner]];
            VoxelVertex *vertex = &layer->vertices[base++];
            vertex->x = (int16_t)floorf(point[0] * VERTEX_COORD_SCALE + 0.5f);
            vertex->y = (int16_t)floorf(point[1] * VERTEX_COORD_SCALE + 0.5f);
            vertex->z = (int16_t)floorf(point[2] * VERTEX_COORD_SCALE + 0.5f);
            vertex->pad = 0;
            {
                unsigned tile=id==BLOCK_TORCH ? 13u : id==BETA_BLOCK_REDSTONE_TORCH ? 80u : 81u;
                int upixel=corner>=2 ? 9 : 7;
                int vpixel=face==4 ? (corner==1 || corner==2 ? 8 : 6) : (corner==1 || corner==2 ? 6 : 16);
                unsigned ub=(tile%ATLAS_TILES)*ATLAS_SLOT_PIXELS;
                unsigned vb=(tile/ATLAS_TILES)*ATLAS_SLOT_PIXELS;
                vertex->u=(int16_t)((ub+upixel)*(32767.0f/ATLAS_PIXELS)+0.5f);
                vertex->v=(int16_t)((vb+vpixel)*(32767.0f/ATLAS_PIXELS)+0.5f);
            }
            vertex->r = 255;
            vertex->g = vertex->b = 255;
            vertex->a = 255;
        }
    }
    layer->vertex_count = base;
}

static void emit_torch(ChunkMesh *mesh,int x,int y,int z,uint8_t metadata)
{ emit_torch_kind(mesh,x,y,z,metadata,BLOCK_TORCH); }

static unsigned cross_slot(uint8_t id, uint8_t metadata)
{
    switch (id) {
    case BETA_BLOCK_SAPLING:
        return (metadata & 3u) == 1u ? 50u : (metadata & 3u) == 2u ? 51u : 49u;
    case BETA_BLOCK_TALL_GRASS:
        return metadata == 0 ? 53u : metadata == 2 ? 54u : 52u;
    case BETA_BLOCK_DEAD_BUSH: return 53u;
    case BETA_BLOCK_DANDELION: return 55u;
    case BETA_BLOCK_ROSE: return 56u;
    case BETA_BLOCK_BROWN_MUSHROOM: return 57u;
    case BETA_BLOCK_RED_MUSHROOM: return 58u;
    case BETA_BLOCK_REEDS: return 59u;
    case BETA_BLOCK_WEB: return (unsigned)beta_render_tile(11);
    case BETA_BLOCK_FIRE: return beta_render_tile(31);
    default: return 0u;
    }
}

static void emit_cross_plant(const Renderer *renderer, const World *world,
                             ChunkMesh *mesh, const Chunk *chunk,
                             int x, int y, int z, uint8_t id, uint8_t metadata)
{
    MeshLayer *layer = &mesh->layers[BLOCK_LAYER_CUTOUT - 1];
    unsigned slot = cross_slot(id, metadata);
    uint8_t light = light_at(world, chunk, x, y, z);
    float ambient = 0.06f + (float)renderer->options.brightness * 0.0034f;
    float illumination = ambient + (1.0f - ambient) * ((float)light / 15.0f);
    uint8_t color = clamp_byte((int)(255.0f * illumination));
    BetaBlockState state = { id, metadata };
    BetaBlockBox box;
    float endpoints[2][2][2];
    int plane, side, corner;
    if(id==51) { box=(BetaBlockBox){0,0,0,1,1,1}; color=255; }
    else if (!slot || !beta_block_selection_box(state, &box)) return;
    if (!layer_reserve(layer, layer->vertex_count + 16u)) {
        layer->overflow = 1;
        return;
    }
    endpoints[0][0][0] = endpoints[1][0][0] = (float)x + box.min_x;
    endpoints[0][1][0] = endpoints[1][1][0] = (float)x + box.max_x;
    endpoints[0][0][1] = endpoints[1][1][1] = (float)z + box.min_z;
    endpoints[0][1][1] = endpoints[1][0][1] = (float)z + box.max_z;
    for (plane = 0; plane < 2; ++plane) {
        for (side = 0; side < 2; ++side) {
            for (corner = 0; corner < 4; ++corner) {
                int end = (corner == 1 || corner == 2) != side;
                int top = corner >= 2;
                VoxelVertex *vertex = &layer->vertices[layer->vertex_count++];
                vertex->x = (int16_t)floorf(endpoints[plane][end][0] *
                                             VERTEX_COORD_SCALE + 0.5f);
                vertex->y = (int16_t)floorf(((float)y + (top ? box.max_y : box.min_y)) *
                                             VERTEX_COORD_SCALE + 0.5f);
                vertex->z = (int16_t)floorf(endpoints[plane][end][1] *
                                             VERTEX_COORD_SCALE + 0.5f);
                vertex->pad = 0;
                vertex->u = texcoord(slot, corner == 1 || corner == 2, 0, 1);
                vertex->v = texcoord(slot, !top, 1, 1);
                vertex->r = vertex->g = vertex->b = color;
                vertex->a = 255;
            }
        }
    }
}

static int16_t partial_texcoord(unsigned tile, float fraction, int upper, int vertical)
{
    unsigned base = (vertical ? tile / ATLAS_TILES : tile % ATLAS_TILES) *
                    ATLAS_SLOT_PIXELS;
    float pixel = (float)base + fraction * TILE_PIXELS;
    (void)upper;
    return (int16_t)(pixel * (32767.0f / (float)ATLAS_PIXELS) + 0.5f);
}

/* The half slab and the exposed upper half of an adjacent opaque cube bypass
 * the full-voxel greedy mask. Both remain in the same cached opaque layer. */
static void emit_partial_face(const Renderer *renderer, const World *world,
                              ChunkMesh *mesh, const Chunk *chunk,
                              int x, int y, int z, uint8_t id, uint8_t metadata,
                              int axis, int direction,
                              const float lower[3], const float upper[3])
{
    static const int u_axis[3] = {1,2,0};
    static const int v_axis[3] = {2,0,1};
    MeshLayer *layer = &mesh->layers[render_layer(renderer,id) - 1];
    float corners[4][3];
    int adjacent[3] = {x,y,z};
    int u = u_axis[axis], v = v_axis[axis], i;
    unsigned tile = block_face_tile(world_block_def(id),id,metadata,axis,direction);
    uint8_t light, color;
    float ambient, illumination, shade;
    int16_t us[4], vs[4];
    if(id==BETA_BLOCK_CHEST) {
        unsigned face=axis==1 ? (direction>0 ? 1u : 0u) :
            axis==2 ? (direction>0 ? 3u : 2u) : (direction>0 ? 5u : 4u);
        tile=beta_render_tile(block_chest_texture(world,chunk->x*16+x,y,chunk->z*16+z,face));
    }
    if(renderer->options.reduced_transparency &&
       (id==BETA_BLOCK_WOOD_DOOR || id==BETA_BLOCK_IRON_DOOR)) tile+=4;
    if (!layer_reserve(layer,layer->vertex_count+4u)) { layer->overflow=1; return; }
    adjacent[axis] += direction;
    light = light_at(world,chunk,adjacent[0],adjacent[1],adjacent[2]);
    ambient = 0.06f + (float)renderer->options.brightness * 0.0034f;
    illumination = ambient + (1.0f-ambient)*(float)light/15.0f;
    shade = axis == 1 ? (direction > 0 ? 1.0f : 0.54f) :
            (axis == 0 ? 0.79f : 0.69f);
    color = clamp_byte((int)(255.0f*shade*illumination));
    for (i=0;i<4;++i) {
        corners[i][axis] = (direction>0 ? upper[axis] : lower[axis]);
        corners[i][u] = (i==0 || i==3 ? lower[u] : upper[u]);
        corners[i][v] = (i<2 ? lower[v] : upper[v]);
    }
    if (axis==0) {
        us[0]=us[1]=partial_texcoord(tile,lower[v],0,0);
        us[2]=us[3]=partial_texcoord(tile,upper[v],1,0);
        vs[0]=vs[3]=partial_texcoord(tile,lower[u],0,1);
        vs[1]=vs[2]=partial_texcoord(tile,upper[u],1,1);
    } else {
        us[0]=us[3]=partial_texcoord(tile,lower[u],0,0);
        us[1]=us[2]=partial_texcoord(tile,upper[u],1,0);
        vs[0]=vs[1]=partial_texcoord(tile,lower[v],0,1);
        vs[2]=vs[3]=partial_texcoord(tile,upper[v],1,1);
    }
    if (axis!=1) {
        for (i=0;i<4;++i) {
            float h=axis==0 ? corners[i][u] : corners[i][v];
            vs[i]=partial_texcoord(tile,1.0f-h,h<0.5f,1);
        }
    }
    if(chest_face_flips_u(id,axis,direction)) for(i=0;i<4;++i) {
        float fraction=axis==0 ? corners[i][2] : corners[i][0];
        us[i]=partial_texcoord(tile,1.0f-fraction,0,0);
    }
    for (i=0;i<4;++i) {
        int order = direction>0 ? i : (i==0 ? 0 : 4-i);
        VoxelVertex *vertex = &layer->vertices[layer->vertex_count++];
        vertex->x=(int16_t)floorf((x+corners[order][0])*VERTEX_COORD_SCALE+0.5f);
        vertex->y=(int16_t)floorf((y+corners[order][1])*VERTEX_COORD_SCALE+0.5f);
        vertex->z=(int16_t)floorf((z+corners[order][2])*VERTEX_COORD_SCALE+0.5f);
        vertex->pad=0;
        vertex->u=us[order]; vertex->v=vs[order];
        vertex->r=vertex->g=vertex->b=color; vertex->a=255;
    }
}

static void emit_slab(const Renderer *renderer, const World *world,
                      ChunkMesh *mesh, const Chunk *chunk,
                      int x, int y, int z, uint8_t metadata)
{
    static const float slab_lower[3]={0.0f,0.0f,0.0f};
    static const float slab_upper[3]={1.0f,0.5f,1.0f};
    int axis, direction;
    for(axis=0;axis<3;++axis) for(direction=-1;direction<=1;direction+=2) {
        int adjacent[3]={x,y,z};
        uint8_t neighbor;
        adjacent[axis]+=direction;
        neighbor=block_at(world,chunk,adjacent[0],adjacent[1],adjacent[2]);
        if (axis==1) {
            if (direction<0 && world_block_def(neighbor)->opaque) continue;
        } else if (neighbor==BETA_BLOCK_SLAB || world_block_def(neighbor)->opaque) {
            continue;
        }
        emit_partial_face(renderer,world,mesh,chunk,x,y,z,BETA_BLOCK_SLAB,
                          metadata,axis,direction,slab_lower,slab_upper);
    }
}

static void emit_cactus(const Renderer *renderer,const World *world,ChunkMesh *mesh,
                        const Chunk *chunk,int x,int y,int z)
{
    static const float low[3]={0.0625f,0.0f,0.0625f},high[3]={0.9375f,1.0f,0.9375f};
    int axis,direction;
    for (axis=0;axis<3;++axis) for (direction=-1;direction<=1;direction+=2) {
        if (axis==1 && block_at(world,chunk,x,y+direction,z)==BETA_BLOCK_CACTUS) continue;
        emit_partial_face(renderer,world,mesh,chunk,x,y,z,BETA_BLOCK_CACTUS,0,axis,direction,low,high);
    }
}
/* Beta RenderBlocks type 12: cobblestone foot and a separately tilted handle.
 * All geometry stays in the cached chunk layer; no additional draw calls. */
static void lever_rotate(float p[3],float angle,int axis)
{
    float s=sinf(angle),c=cosf(angle),a=p[axis==0 ? 1 : 0],b=p[2];
    p[axis==0 ? 1 : 0]=a*c+b*s; p[2]=b*c-a*s;
}
static void emit_lever(const Renderer *r,const World *w,ChunkMesh *mesh,const Chunk *chunk,
                       int x,int y,int z,uint8_t meta)
{
    static const int faces[6][4]={{0,1,2,3},{7,6,5,4},{1,0,4,5},{2,1,5,6},{3,2,6,7},{0,3,7,4}};
    float lo[3]={.3125f,0,.25f},hi[3]={.6875f,.1875f,.75f},p[8][3];
    unsigned dir=meta&7,tile=beta_render_tile(96); int a,s,i,f;
    MeshLayer *layer=&mesh->layers[BLOCK_LAYER_OPAQUE-1];
    uint8_t color=clamp_byte((int)(255*(.06f+r->options.brightness*.0034f+
        (1-.06f-r->options.brightness*.0034f)*light_at(w,chunk,x,y,z)/15.0f)));
    if(dir==6) { lo[0]=.25f; hi[0]=.75f; lo[2]=.3125f; hi[2]=.6875f; }
    else if(dir<5) {
        lo[1]=.25f; hi[1]=.75f;
        if(dir<=2) { lo[2]=.3125f; hi[2]=.6875f; lo[0]=dir==1 ? 0 : .8125f; hi[0]=dir==1 ? .1875f : 1; }
        else { lo[0]=.3125f; hi[0]=.6875f; lo[2]=dir==3 ? 0 : .8125f; hi[2]=dir==3 ? .1875f : 1; }
    }
    for(a=0;a<3;++a) for(s=-1;s<=1;s+=2)
        emit_partial_face(r,w,mesh,chunk,x,y,z,BETA_BLOCK_COBBLESTONE,0,a,s,lo,hi);
    for(i=0;i<8;++i) {
        p[i][0]=(i==1 || i==2 || i==5 || i==6) ? .0625f : -.0625f;
        p[i][1]=i>=4 ? .625f : 0; p[i][2]=(i==2 || i==3 || i==6 || i==7) ? .0625f : -.0625f;
        p[i][2]+=(meta&8) ? -.0625f : .0625f;
        lever_rotate(p[i],(meta&8) ? .69813174f : -.69813174f,0);
        if(dir==6) lever_rotate(p[i],1.57079633f,1);
        if(dir<5) {
            p[i][1]-=.375f; lever_rotate(p[i],1.57079633f,0);
            lever_rotate(p[i],dir==1 ? -1.57079633f : dir==2 ? 1.57079633f : dir==3 ? 3.14159265f : 0,1);
        }
        p[i][0]+=.5f; p[i][1]+=dir<5 ? .5f : .125f; p[i][2]+=.5f;
    }
    if(!layer_reserve(layer,layer->vertex_count+24)) { layer->overflow=1; return; }
    for(f=0;f<6;++f) for(i=0;i<4;++i) {
        VoxelVertex *v=&layer->vertices[layer->vertex_count++]; float *q=p[faces[f][i]];
        v->x=(int16_t)floorf((x+q[0])*VERTEX_COORD_SCALE+.5f);
        v->y=(int16_t)floorf((y+q[1])*VERTEX_COORD_SCALE+.5f);
        v->z=(int16_t)floorf((z+q[2])*VERTEX_COORD_SCALE+.5f); v->pad=0;
        v->u=partial_texcoord(tile,(i==0 || i==3 ? 7 : 8.99f)/16,0,0);
        v->v=partial_texcoord(tile,(i<2 ? (f<2 ? 7.99f : 15.99f) : 6)/16,0,1);
        v->r=v->g=v->b=color; v->a=255;
    }
}
static void emit_rail(const Renderer *r,const World *w,ChunkMesh *mesh,const Chunk *chunk,
                      int x,int y,int z,uint8_t id,uint8_t meta)
{
    float p[4][3]={{0,.0625f,0},{0,.0625f,1},{1,.0625f,1},{1,.0625f,0}};
    int shape=id==66 ? meta : meta&7,i,side,rotation=shape==1 || shape==2 || shape==3 ? 1 : shape==7 ? 1 : shape==8 ? 2 : shape==9 ? 3 : 0;
    unsigned tile=beta_render_tile(beta_block_terrain_tile((BetaBlockState){id,meta},1));
    MeshLayer *layer=&mesh->layers[BLOCK_LAYER_CUTOUT-1];
    uint8_t color=clamp_byte((int)(255*(.06f+r->options.brightness*.0034f+(1-.06f-r->options.brightness*.0034f)*light_at(w,chunk,x,y,z)/15.0f)));
    if(!layer_reserve(layer,layer->vertex_count+8)) { layer->overflow=1; return; }
    if(shape>=2 && shape<=5) for(i=0;i<4;++i) p[i][1]+=shape==2 ? p[i][0] : shape==3 ? 1-p[i][0] : shape==4 ? 1-p[i][2] : p[i][2];
    for(side=0;side<2;++side) for(i=0;i<4;++i) {
        int index=side ? 3-i : i,uv=(index+rotation)&3;
        VoxelVertex *v=&layer->vertices[layer->vertex_count++];
        v->x=(int16_t)((x+p[index][0])*VERTEX_COORD_SCALE); v->y=(int16_t)((y+p[index][1])*VERTEX_COORD_SCALE);
        v->z=(int16_t)((z+p[index][2])*VERTEX_COORD_SCALE); v->pad=0;
        v->u=partial_texcoord(tile,uv<2 ? 0 : .999375f,0,0);
        v->v=partial_texcoord(tile,uv==0 || uv==3 ? 0 : .999375f,0,1);
        v->r=v->g=v->b=color; v->a=255;
    }
}
static void emit_low_block(const Renderer *r,const World *w,ChunkMesh *mesh,const Chunk *chunk,
                           int x,int y,int z,uint8_t id,uint8_t metadata)
{
    if(id==69) { emit_lever(r,w,mesh,chunk,x,y,z,metadata); return; }
    float lo[3]={0,0,0},hi[3]={1,id==26 ? .5625f : .125f,1};
    int axis,sign;
    if(id==69 || id==77 || id==70 || id==72) {
        BetaBlockBox box; beta_block_selection_box((BetaBlockState){id,metadata},&box);
        lo[0]=box.min_x; lo[1]=box.min_y; lo[2]=box.min_z;
        hi[0]=box.max_x; hi[1]=box.max_y; hi[2]=box.max_z;
    }
    for(axis=0;axis<3;++axis) for(sign=-1;sign<=1;sign+=2) {
        MeshLayer *layer=&mesh->layers[BLOCK_LAYER_OPAQUE-1]; unsigned before=layer->vertex_count,i;
        emit_partial_face(r,w,mesh,chunk,x,y,z,id,metadata,axis,sign,lo,hi);
        if(axis==1 && sign==1) {
            unsigned tile=block_face_tile(world_block_def(id),id,metadata,axis,sign);
            for(i=before;i<layer->vertex_count;++i) {
                float u=(float)layer->vertices[i].x/VERTEX_COORD_SCALE-x;
                float v=(float)layer->vertices[i].z/VERTEX_COORD_SCALE-z;
                int n;
                for(n=0;n<((id==93 || id==94) ? metadata&3 : 0);++n) { float t=u; u=v; v=1-t; }
                layer->vertices[i].u=partial_texcoord(tile,u,0,0);
                layer->vertices[i].v=partial_texcoord(tile,v,0,1);
            }
        }
    }
}

static void emit_repeater(const Renderer *r,const World *w,ChunkMesh *mesh,const Chunk *chunk,
                          int x,int y,int z,uint8_t id,uint8_t meta)
{
    static const float offsets[4]={-.0625f,.0625f,.1875f,.3125f};
    static const int dx[4]={0,-1,0,1},dz[4]={1,0,-1,0};
    int n,dir=meta&3;
    emit_low_block(r,w,mesh,chunk,x,y,z,id,meta);
    for(n=0;n<2;++n) {
        MeshLayer *layer=&mesh->layers[BLOCK_LAYER_CUTOUT-1];
        unsigned first=layer->vertex_count,i;
        float offset=n ? -.3125f : offsets[(meta>>2)&3];
        emit_torch_kind(mesh,x,y,z,5,id==94 ? 76 : 75);
        /* RenderBlocks places both torch stems 3/16 below the plate origin. */
        for(i=first;i<layer->vertex_count;++i) {
            layer->vertices[i].x+=(int16_t)(dx[dir]*offset*VERTEX_COORD_SCALE);
            layer->vertices[i].y-=(int16_t)(.1875f*VERTEX_COORD_SCALE);
            /* The standard torch mesh is .6 high; Beta's repeater torch is
             * exactly 10 texture pixels (10/16), with its foot below the slab. */
            if(layer->vertices[i].y>y*VERTEX_COORD_SCALE)
                layer->vertices[i].y+=(int16_t)(.025f*VERTEX_COORD_SCALE);
            layer->vertices[i].z+=(int16_t)(dz[dir]*offset*VERTEX_COORD_SCALE);
        }
    }
}

/* RenderBlocks.renderBlockBed uses a separate top UV orientation, lower
 * atlas rows for the wooden frame, and no face between the two bed halves. */
static void emit_bed(const Renderer *r,const World *w,ChunkMesh *mesh,const Chunk *chunk,
                     int x,int y,int z,uint8_t metadata)
{
    static const unsigned hidden[2][4]={{3,4,2,5},{2,5,3,4}};
    static const unsigned flipped[4]={5,3,4,2};
    int axis,sign;
    unsigned direction=metadata&3;
    MeshLayer *layer=&mesh->layers[BLOCK_LAYER_CUTOUT-1];
    for(axis=0;axis<3;++axis) for(sign=-1;sign<=1;sign+=2) {
        float lo[3]={0,0,0},hi[3]={1,.5625f,1};
        unsigned face=axis==1 ? (sign>0 ? 1u : 0u) : axis==2 ? (sign>0 ? 3u : 2u) : (sign>0 ? 5u : 4u);
        unsigned before=layer->vertex_count,i,tile;
        if(face==hidden[(metadata>>3)&1][direction]) continue;
        if(axis!=1) {
            int p[3]={x,y,z}; p[axis]+=sign;
            if(world_block_def(block_at(w,chunk,p[0],p[1],p[2]))->opaque) continue;
        } else if(sign<0) lo[1]=hi[1]=.1875f;
        emit_partial_face(r,w,mesh,chunk,x,y,z,BETA_BLOCK_BED,metadata,axis,sign,lo,hi);
        tile=block_face_tile(world_block_def(BETA_BLOCK_BED),BETA_BLOCK_BED,metadata,axis,sign);
        for(i=before;i<layer->vertex_count;++i) {
            VoxelVertex *v=&layer->vertices[i];
            float a=(float)v->x/VERTEX_COORD_SCALE-x,b=(float)v->z/VERTEX_COORD_SCALE-z;
            float u=a,t=b;
            if(axis==1 && sign>0) {
                if(direction==0) { u=b; t=a; }
                else if(direction==1) { u=1-a; t=b; }
                else if(direction==2) { u=1-b; t=1-a; }
                else { u=a; t=1-b; }
            } else if(axis!=1) {
                u=axis==0 ? (sign>0 ? 1-b : b) : (sign>0 ? a : 1-a);
                if(face==flipped[direction]) u=1-u;
                t=1-((float)v->y/VERTEX_COORD_SCALE-y);
            }
            v->u=partial_texcoord(tile,u,0,0); v->v=partial_texcoord(tile,t,0,1);
        }
    }
}

static void emit_door(const Renderer *r,const World *w,ChunkMesh *mesh,const Chunk *chunk,
                      int x,int y,int z,uint8_t id,uint8_t metadata)
{
    BetaBlockBox box; float lo[3],hi[3]; int axis,sign;
    unsigned direction=((metadata&4) ? metadata : metadata-1)&3;
    MeshLayer *layer=&mesh->layers[render_layer(r,id)-1];
    if(!beta_block_selection_box((BetaBlockState){id,metadata},&box)) return;
    lo[0]=box.min_x; lo[1]=box.min_y; lo[2]=box.min_z;
    hi[0]=box.max_x; hi[1]=box.max_y; hi[2]=box.max_z;
    for(axis=0;axis<3;++axis) for(sign=-1;sign<=1;sign+=2) {
        unsigned before=layer->vertex_count,i,tile;
        unsigned face=axis==1 ? (sign>0 ? 1u : 0u) : axis==2 ? (sign>0 ? 3u : 2u) : (sign>0 ? 5u : 4u);
        int flip=0;
        if(face>1 && !((direction==0 || direction==2)!=(face<=3)))
            flip=((direction/2+((face&1)^direction)+(metadata&4)/4)&1)!=0;
        emit_partial_face(r,w,mesh,chunk,x,y,z,id,metadata,axis,sign,lo,hi);
        tile=block_face_tile(world_block_def(id),id,metadata,axis,sign);
        if(r->options.reduced_transparency) tile+=4;
        for(i=before;i<layer->vertex_count && axis!=1;++i) {
            VoxelVertex *v=&layer->vertices[i];
            float a=(float)v->x/VERTEX_COORD_SCALE-x,b=(float)v->z/VERTEX_COORD_SCALE-z;
            float u=axis==0 ? (sign>0 ? 1-b : b) : (sign>0 ? a : 1-a);
            if(flip) u=1-u;
            v->u=partial_texcoord(tile,u,0,0);
        }
    }
}

static void emit_piston(const Renderer *r,const World *w,ChunkMesh *mesh,const Chunk *chunk,int x,int y,int z,unsigned id,unsigned meta)
{
    unsigned first[3],i; int a,s,l; float offset[3]={0,0,0},lo[3]={0,0,0},hi[3]={1,1,1};
    BetaBlockBox box;
    if(id==36) {
        const BlockEntity *e;
        static const int dx[6]={0,0,0,0,-1,1},dy[6]={-1,1,0,0,0,0},dz[6]={0,0,-1,1,0,0};
        for(e=chunk->entities;e;e=e->next) if(e->x==chunk->x*16+x && e->y==y && e->z==chunk->z*16+z) break;
        if(!e || e->kind!=BLOCK_ENTITY_PISTON || e->piston_facing<0 || e->piston_facing>5 || e->piston_id<=0 || e->piston_id>=97) return;
        id=(unsigned)e->piston_id; meta=(unsigned)e->piston_data;
        { float t=e->piston_extending ? e->piston_progress-1 : 1-e->piston_progress;
          offset[0]=t*dx[e->piston_facing]; offset[1]=t*dy[e->piston_facing]; offset[2]=t*dz[e->piston_facing]; }
        if(e->piston_base && !e->piston_extending && (id==29 || id==33)) {
            /* On retraction the base stays in its cell; only the head moves. */
            emit_piston(r,w,mesh,chunk,x,y,z,id,(unsigned)e->piston_facing|8u);
            for(l=0;l<3;++l) first[l]=mesh->layers[l].vertex_count;
            emit_piston(r,w,mesh,chunk,x,y,z,34,(unsigned)e->piston_facing|(id==29 ? 8u : 0u));
            for(l=0;l<3;++l) for(i=first[l];i<mesh->layers[l].vertex_count;++i) {
                VoxelVertex *v=&mesh->layers[l].vertices[i];
                v->x+=(int16_t)(offset[0]*VERTEX_COORD_SCALE); v->y+=(int16_t)(offset[1]*VERTEX_COORD_SCALE); v->z+=(int16_t)(offset[2]*VERTEX_COORD_SCALE);
            }
            return;
        }
    }
    for(l=0;l<3;++l) first[l]=mesh->layers[l].vertex_count;
    if(beta_block_selection_box((BetaBlockState){(uint8_t)id,(uint8_t)meta},&box)) {
        lo[0]=box.min_x; lo[1]=box.min_y; lo[2]=box.min_z;
        hi[0]=box.max_x; hi[1]=box.max_y; hi[2]=box.max_z;
    }
    for(a=0;a<3;++a) for(s=-1;s<=1;s+=2) emit_partial_face(r,w,mesh,chunk,x,y,z,(uint8_t)id,(uint8_t)meta,a,s,lo,hi);
    if(id==34) {
        unsigned face=meta&7,axis=face<2 ? 1 : face<4 ? 2 : 0;
        lo[0]=lo[1]=lo[2]=.375f; hi[0]=hi[1]=hi[2]=.625f;
        lo[axis]=face&1 ? -.25f : .25f; hi[axis]=face&1 ? .75f : 1.25f;
        for(a=0;a<3;++a) for(s=-1;s<=1;s+=2) if((unsigned)a!=axis)
        {
            MeshLayer *layer=&mesh->layers[0]; unsigned before=layer->vertex_count,j;
            int width_axis=3-a-(int)axis;
            emit_partial_face(r,w,mesh,chunk,x,y,z,(uint8_t)id,(uint8_t)meta,a,s,lo,hi);
            for(j=before;j<layer->vertex_count;++j) {
                VoxelVertex *v=&layer->vertices[j]; float p[3]={v->x/VERTEX_COORD_SCALE-x,v->y/VERTEX_COORD_SCALE-y,v->z/VERTEX_COORD_SCALE-z};
                unsigned tile=beta_render_tile(108);
                v->u=partial_texcoord(tile,p[axis]-lo[axis],0,0);
                v->v=partial_texcoord(tile,p[width_axis]-.375f,0,1);
            }
        }
    }
    for(l=0;l<3;++l) for(i=first[l];i<mesh->layers[l].vertex_count;++i) {
        VoxelVertex *v=&mesh->layers[l].vertices[i];
        v->x+=(int16_t)(offset[0]*VERTEX_COORD_SCALE); v->y+=(int16_t)(offset[1]*VERTEX_COORD_SCALE); v->z+=(int16_t)(offset[2]*VERTEX_COORD_SCALE);
    }
}
static void emit_decorative_plane(const Renderer *r,ChunkMesh *mesh,int x,int y,int z,unsigned tile,int axis,int power)
{
    MeshLayer *layer=&mesh->layers[axis==1 ? BLOCK_LAYER_CUTOUT-1 : render_layer(r,BETA_BLOCK_NETHER_PORTAL)-1];
    int side,corner,u=axis==0 ? 2 : 0,v=axis==1 ? 2 : 1;
    if (!layer_reserve(layer,layer->vertex_count+(axis==1 ? 4u : 8u))) { layer->overflow=1; return; }
    for (side=0;side<(axis==1 ? 1 : 2);++side) for (corner=0;corner<4;++corner) {
        float p[3]={(float)x,(float)y,(float)z};
        int c=side ? (corner==0 ? 0 : 4-corner) : corner;
        VoxelVertex *vertex=&layer->vertices[layer->vertex_count++];
        p[axis]+=axis==1 ? 0.0625f : 0.5f;
        p[u]+=(c==1 || c==2); p[v]+=(c>=2);
        vertex->x=(int16_t)floorf(p[0]*VERTEX_COORD_SCALE+0.5f);
        vertex->y=(int16_t)floorf(p[1]*VERTEX_COORD_SCALE+0.5f);
        vertex->z=(int16_t)floorf(p[2]*VERTEX_COORD_SCALE+0.5f); vertex->pad=0;
        vertex->u=texcoord(tile,c==1 || c==2,0,1);
        vertex->v=texcoord(tile,axis==1 ? c>=2 : c<2,1,1);
        vertex->r=axis==1 ? (uint8_t)(power>0 ? 100+power*155/15 : 75) : 255;
        vertex->g=axis==1 ? 0 : 255; vertex->b=axis==1 ? 0 : 255;
        vertex->a=axis==1 || r->options.reduced_transparency ? 255 : 180;
    }
}
static void wire_quad(ChunkMesh *mesh,const float p[4][3],int tile,int rotate,
                      float u0,float v0,float u1,float v1,const uint8_t color[3])
{
    MeshLayer *layer=&mesh->layers[BLOCK_LAYER_CUTOUT-1]; int i;
    if(!layer_reserve(layer,layer->vertex_count+4)) { layer->overflow=1; return; }
    for(i=0;i<4;++i) {
        VoxelVertex *v=&layer->vertices[layer->vertex_count++];
        float u=i==0 || i==1 ? u1 : u0,t=i==0 || i==3 ? v1 : v0;
        if(rotate) { float tmp=u; u=t; t=tmp; }
        v->x=(int16_t)floorf(p[i][0]*VERTEX_COORD_SCALE+.5f);
        v->y=(int16_t)floorf(p[i][1]*VERTEX_COORD_SCALE+.5f);
        v->z=(int16_t)floorf(p[i][2]*VERTEX_COORD_SCALE+.5f); v->pad=0;
        v->u=partial_texcoord((unsigned)tile,u,0,0); v->v=partial_texcoord((unsigned)tile,t,0,1);
        v->r=color[0]; v->g=color[1]; v->b=color[2]; v->a=255;
    }
}
static void emit_wire(const Renderer *r,const World *w,ChunkMesh *mesh,const Chunk *chunk,int x,int y,int z)
{
    int wx=chunk->x*16+x,wz=chunk->z*16+z,n,links[4],line,pass;
    float lo[2]={0,0},hi[2]={1,1},f=chunk_get_metadata(chunk,x,y,z)/15.0f;
    float light=.06f+r->options.brightness*.0034f;
    uint8_t colors[2][3]; float p[4][3];
    light+=(1-light)*light_at(w,chunk,x,y,z)/15.0f;
    colors[0][0]=clamp_byte((int)(255*light*(f ? f*.6f+.4f : .3f)));
    colors[0][1]=clamp_byte((int)(255*light*fmaxf(0,f*f*.7f-.5f)));
    colors[0][2]=clamp_byte((int)(255*light*fmaxf(0,f*f*.6f-.7f)));
    colors[1][0]=colors[1][1]=colors[1][2]=clamp_byte((int)(255*light));
    for(n=0;n<4;++n) links[n]=world_wire_visual_connects(w,wx,y,wz,n);
    line=(links[1] || links[3]) && !links[0] && !links[2] ? 1 :
         (links[0] || links[2]) && !links[1] && !links[3] ? 2 : 0;
    /* Disconnected dust also crops the cross to its central 6x6 pixels, as
     * RenderBlocks 1.5.2 does; one link uses a full straight line. */
    if(!line) {
        if(!links[1]) lo[0]=.3125f;
        if(!links[3]) hi[0]=.6875f;
        if(!links[2]) lo[1]=.3125f;
        if(!links[0]) hi[1]=.6875f;
    }
    p[0][0]=p[1][0]=x+hi[0]; p[2][0]=p[3][0]=x+lo[0];
    p[0][2]=p[3][2]=z+hi[1]; p[1][2]=p[2][2]=z+lo[1];
    for(n=0;n<4;++n) p[n][1]=y+.015625f;
    for(pass=0;pass<2;++pass) wire_quad(mesh,p,(int)beta_render_tile(164+(line ? 1 : 0)+pass*16),line==2,
                                     lo[0],lo[1],hi[0],hi[1],colors[pass]);
    /* Dust climbing a solid neighbour uses the line and highlight tiles. */
    if(!world_block_def(world_peek_block(w,wx,y+1,wz))->opaque) for(n=0;n<4;++n) {
        static const int dx[4]={0,-1,0,1},dz[4]={1,0,-1,0};
        if(!world_block_def(world_peek_block(w,wx+dx[n],y,wz+dz[n]))->opaque ||
           world_peek_block(w,wx+dx[n],y+1,wz+dz[n])!=55) continue;
        for(pass=0;pass<2;++pass) {
            int i;
            for(i=0;i<4;++i) {
                float a=i==0 || i==1 ? 1 : 0,b=i==0 || i==3 ? 1 : 0;
                p[i][0]=x+(dx[n]<0 ? .015625f : dx[n]>0 ? .984375f : a);
                p[i][2]=z+(dz[n]<0 ? .015625f : dz[n]>0 ? .984375f : 1-a);
                p[i][1]=y+b;
            }
            if(n==0 || n==1) { float t[3]; memcpy(t,p[0],sizeof(t)); memcpy(p[0],p[3],sizeof(t)); memcpy(p[3],t,sizeof(t)); memcpy(t,p[1],sizeof(t)); memcpy(p[1],p[2],sizeof(t)); memcpy(p[2],t,sizeof(t)); }
            wire_quad(mesh,p,(int)beta_render_tile(165+pass*16),0,0,0,1,1,colors[pass]);
        }
    }
}
/* Keep the large, level interior of an ocean greedy. Only shorelines, falling
 * columns and changing levels need the four-corner liquid mesh. */
static int uniform_liquid(const World *world,const Chunk *chunk,int x,int y,int z,uint8_t id)
{
    int a,b,kind=fluid_kind(id);
    if (chunk_get_metadata(chunk,x,y,z)!=0) return 0;
    for (a=-1;a<=1;++a) for (b=-1;b<=1;++b) {
        int wx=chunk->x*16+x+a,wz=chunk->z*16+z+b;
        if (fluid_kind(block_at(world,chunk,x+a,y,z+b))!=kind ||
            world_peek_metadata(world,wx,y,wz)!=0 || fluid_kind(block_at(world,chunk,x+a,y+1,z+b))==kind) return 0;
    }
    return 1;
}
static void emit_liquid(const Renderer *renderer,const World *world,ChunkMesh *mesh,
                        const Chunk *chunk,int x,int y,int z,uint8_t id)
{
    static const int offsets[6][3]={{-1,0,0},{1,0,0},{0,0,-1},{0,0,1},{0,1,0},{0,-1,0}};
    float h[4],flow[3];
    int wx=chunk->x*16+x,wz=chunk->z*16+z,kind=fluid_kind(id),face,i;
    MeshLayer *layer=&mesh->layers[kind==1 ? BLOCK_LAYER_TRANSPARENT-1 : BLOCK_LAYER_OPAQUE-1];
    h[0]=fluid_corner_height(world,wx,y,wz,kind);
    h[1]=fluid_corner_height(world,wx+1,y,wz,kind);
    h[2]=fluid_corner_height(world,wx+1,y,wz+1,kind);
    h[3]=fluid_corner_height(world,wx,y,wz+1,kind);
    fluid_flow_vector(world,wx,y,wz,flow);
    for (face=0;face<6;++face) {
        uint8_t neighbor=block_at(world,chunk,x+offsets[face][0],y+offsets[face][1],z+offsets[face][2]);
        float p[4][3],height[4]={0},shade=face==4 ? 1 : face==5 ? .5f : face<2 ? .6f : .8f;
        unsigned tile=kind==1 ? (face==4 && fabsf(flow[0])+fabsf(flow[2])<.01f ? 83 : 84) :
            (face==4 && fabsf(flow[0])+fabsf(flow[2])<.01f ? 64 : 65);
        uint8_t light=kind==2 ? 15 : light_at(world,chunk,x,y,z);
        float ambient=.06f+renderer->options.brightness*.0034f;
        uint8_t color=clamp_byte((int)(255*shade*(ambient+(1-ambient)*light/15)));
        if (fluid_kind(neighbor)==kind || neighbor==79 || (face!=4 && world_block_def(neighbor)->opaque)) continue;
        if (!layer_reserve(layer,layer->vertex_count+4)) { layer->overflow=1; return; }
        if (face==0) {
            float a[4][3]={{0,0,0},{0,0,1},{0,h[3],1},{0,h[0],0}}; memcpy(p,a,sizeof(p)); height[2]=h[3]; height[3]=h[0];
        } else if (face==1) {
            float a[4][3]={{1,0,1},{1,0,0},{1,h[1],0},{1,h[2],1}}; memcpy(p,a,sizeof(p)); height[2]=h[1]; height[3]=h[2];
        } else if (face==2) {
            float a[4][3]={{1,0,0},{0,0,0},{0,h[0],0},{1,h[1],0}}; memcpy(p,a,sizeof(p)); height[2]=h[0]; height[3]=h[1];
        } else if (face==3) {
            float a[4][3]={{0,0,1},{1,0,1},{1,h[2],1},{0,h[3],1}}; memcpy(p,a,sizeof(p)); height[2]=h[2]; height[3]=h[3];
        } else if (face==4) {
            float a[4][3]={{0,h[0],0},{0,h[3],1},{1,h[2],1},{1,h[1],0}}; memcpy(p,a,sizeof(p));
        } else {
            float a[4][3]={{0,0,1},{0,0,0},{1,0,0},{1,0,1}}; memcpy(p,a,sizeof(p));
        }
        for (i=0;i<4;++i) {
            VoxelVertex *v=&layer->vertices[layer->vertex_count++];
            float tu=face<4 ? (i==1 || i==2) : p[i][0],tv=face<4 ? 1-height[i] : p[i][2];
            if (face==4 && fabsf(flow[0])+fabsf(flow[2])>.01f) {
                float u=tu-.5f,t=tv-.5f;
                tu=.5f+(u*flow[2]-t*flow[0])*.5f;
                tv=.5f+(u*flow[0]+t*flow[2])*.5f;
            }
            v->x=(int16_t)floorf((x+p[i][0])*VERTEX_COORD_SCALE+.5f);
            v->y=(int16_t)floorf((y+p[i][1])*VERTEX_COORD_SCALE+.5f);
            v->z=(int16_t)floorf((z+p[i][2])*VERTEX_COORD_SCALE+.5f); v->pad=0;
            v->u=partial_texcoord(tile,tu,tu>=1,0); v->v=partial_texcoord(tile,tv,tv>=1,1);
            v->r=v->g=v->b=color; v->a=255;
        }
    }
}
static ChunkMesh *build_chunk_mesh(const Renderer *renderer, const World *world, const Chunk *chunk)
{
    static const int dimensions[3] = {WORLD_CHUNK_SIZE, WORLD_HEIGHT, WORLD_CHUNK_SIZE};
    static const int u_axis[3] = {1, 2, 0};
    static const int v_axis[3] = {2, 0, 1};
    static const float exposed_lower[3] = {0.0f,0.5f,0.0f};
    static const float cube_upper[3] = {1.0f,1.0f,1.0f};
    ChunkMesh *mesh = (ChunkMesh *)calloc(1, sizeof(*mesh));
    uint32_t mask[WORLD_CHUNK_SIZE * WORLD_HEIGHT];
    int axis, direction, slice, min_y=WORLD_HEIGHT, max_y=-1, y;
    if (!mesh) return NULL;
    mesh->chunk_x = chunk->x;
    mesh->chunk_z = chunk->z;
    mesh->revision = chunk->revision;
    /* Most generated chunks end well below Y=128. Avoid six complete air
     * sweeps above their actual contents, including on old software T&L. */
    for (y=0;y<WORLD_HEIGHT;++y) {
        int x,z,found=0;
        for (z=0;z<WORLD_CHUNK_SIZE && !found;++z)
            for (x=0;x<WORLD_CHUNK_SIZE;++x)
                if (chunk_get_block(chunk,x,y,z)!=BLOCK_AIR) { found=1; break; }
        if (found) { if (min_y==WORLD_HEIGHT) min_y=y; max_y=y; }
    }
    if (max_y<0) return mesh;
    {
        unsigned i;
        for(i=0;i<WORLD_CHUNK_VOLUME;++i) {
            unsigned id=chunk->blocks[i];
            if(fluid_kind(id)==1) mesh->animations|=1;
            if(fluid_kind(id)==2) mesh->animations|=2;
            if(id==90) mesh->animations|=4;
            if(id==51) mesh->animations|=8;
        }
    }
    for (axis = 0; axis < 3; ++axis) {
        const int u = u_axis[axis], v = v_axis[axis];
        const int width = dimensions[u], height = dimensions[v];
        const int column_begin=u==1 ? min_y : 0;
        const int column_end=u==1 ? max_y+1 : width;
        const int row_begin=v==1 ? min_y : 0;
        const int row_end=v==1 ? max_y+1 : height;
        for (direction = -1; direction <= 1; direction += 2) {
            for (slice = axis==1 ? min_y : 0;
                 slice < (axis==1 ? max_y+1 : dimensions[axis]); ++slice) {
                int row, column;
                for (row = row_begin; row < row_end; ++row) {
                    for (column = column_begin; column < column_end; ++column) {
                        int point[3] = {0, 0, 0};
                        int adjacent[3];
                        uint8_t block, neighbor;
                        int face_visible;
                        point[axis] = slice;
                        point[u] = column;
                        point[v] = row;
                        adjacent[0] = point[0];
                        adjacent[1] = point[1];
                        adjacent[2] = point[2];
                        adjacent[axis] += direction;
                        block = chunk_get_block(chunk, point[0], point[1], point[2]);
                        neighbor = block_at(world, chunk, adjacent[0], adjacent[1], adjacent[2]);
                        face_visible = visible_face(renderer, block, neighbor);
                        if (face_visible && fluid_kind(block) && !uniform_liquid(world,chunk,point[0],point[1],point[2],block)) face_visible=0;
                        if (face_visible &&
                            neighbor==BETA_BLOCK_SLAB && world_block_def(block)->opaque &&
                            axis!=1) {
                            emit_partial_face(renderer,world,mesh,chunk,
                                              point[0],point[1],point[2],block,
                                              chunk_get_metadata(chunk,point[0],point[1],point[2]),
                                              axis,direction,exposed_lower,cube_upper);
                            mask[row * width + column]=0;
                        } else if (face_visible &&
                                   !(neighbor==BETA_BLOCK_SLAB &&
                                     world_block_def(block)->opaque &&
                                     axis==1 && direction>0)) {
                            uint8_t light = light_at(world, chunk,
                                                     adjacent[0], adjacent[1], adjacent[2]);
                            uint8_t metadata = chunk_get_metadata(chunk,
                                point[0], point[1], point[2]);
                            mask[row * width + column] = face_key(renderer, block,
                                metadata, axis, direction, light);
                            if(block==54) {
                                unsigned face=axis==1 ? (direction>0 ? 1 : 0) : axis==2 ? (direction>0 ? 3 : 2) : (direction>0 ? 5 : 4);
                                unsigned tile=beta_render_tile(block_chest_texture(world,chunk->x*16+point[0],point[1],chunk->z*16+point[2],face));
                                mask[row*width+column]=(mask[row*width+column]&~UINT32_C(0xff00))|(tile<<8);
                            }
                            if (renderer->options.smooth_lighting) {
                                int du,dv;
                                /* Only merge uniformly lit faces. A merged quad
                                 * cannot retain interior light-gradient vertices. */
                                for (dv=-1;dv<=1;++dv) for (du=-1;du<=1;++du) {
                                    int sample[3]={adjacent[0],adjacent[1],adjacent[2]};
                                    sample[u]+=du; sample[v]+=dv;
                                    if (light_at(world,chunk,sample[0],sample[1],sample[2])!=light)
                                        mask[row*width+column]|=UINT32_C(0x80000000);
                                }
                            }
                        } else {
                            mask[row * width + column] = 0;
                        }
                    }
                }
                for (row = row_begin; row < row_end; ++row) {
                    for (column = column_begin; column < column_end;) {
                        uint32_t key = mask[row * width + column];
                        int run_width = 1, run_height = 1, i, j;
                        if (!key) { ++column; continue; }
                        while (renderer->options.greedy && !(key & UINT32_C(0x80000000)) && run_width < TILE_REPEATS &&
                               column + run_width < column_end &&
                               mask[row * width + column + run_width] == key)
                            ++run_width;
                        while (renderer->options.greedy && !(key & UINT32_C(0x80000000)) && run_height < TILE_REPEATS &&
                               row + run_height < row_end) {
                            for (i = 0; i < run_width; ++i) {
                                if (mask[(row + run_height) * width + column + i] != key)
                                    break;
                            }
                            if (i != run_width) break;
                            ++run_height;
                        }
                        emit_quad(renderer, world, mesh, chunk, key, axis, direction, slice,
                                  column, row, run_width, run_height);
                        for (j = 0; j < run_height; ++j)
                            for (i = 0; i < run_width; ++i)
                                mask[(row + j) * width + column + i] = 0;
                        column += run_width;
                    }
                }
            }
        }
    }
    {
        int x, z;
        for (y = min_y; y <= max_y; ++y)
            for (z = 0; z < WORLD_CHUNK_SIZE; ++z)
                for (x = 0; x < WORLD_CHUNK_SIZE; ++x)
                    if (chunk_get_block(chunk, x, y, z) == BLOCK_TORCH)
                        emit_torch(mesh, x, y, z,chunk_get_metadata(chunk,x,y,z));
                    else if (fluid_kind(chunk_get_block(chunk,x,y,z))) {
                        uint8_t id=chunk_get_block(chunk,x,y,z);
                        if (!uniform_liquid(world,chunk,x,y,z,id)) emit_liquid(renderer,world,mesh,chunk,x,y,z,id);
                    }
                    else if (chunk_get_block(chunk,x,y,z)==BETA_BLOCK_REDSTONE_TORCH ||
                             chunk_get_block(chunk,x,y,z)==BETA_BLOCK_UNLIT_REDSTONE_TORCH)
                        emit_torch_kind(mesh,x,y,z,chunk_get_metadata(chunk,x,y,z),chunk_get_block(chunk,x,y,z));
                    else if (chunk_get_block(chunk,x,y,z)==BETA_BLOCK_CACTUS)
                        emit_cactus(renderer,world,mesh,chunk,x,y,z);
                    else if (chunk_get_block(chunk,x,y,z)==BETA_BLOCK_NETHER_PORTAL)
                        emit_decorative_plane(renderer,mesh,x,y,z,66,
                            block_at(world,chunk,x-1,y,z)==90 || block_at(world,chunk,x+1,y,z)==90 ? 2 : 0,0);
                    else if (chunk_get_block(chunk,x,y,z)==BETA_BLOCK_REDSTONE_WIRE)
                        emit_wire(renderer,world,mesh,chunk,x,y,z);
                    else if (chunk_get_block(chunk,x,y,z)==BETA_BLOCK_BED)
                        emit_bed(renderer,world,mesh,chunk,x,y,z,chunk_get_metadata(chunk,x,y,z));
                    else if (chunk_get_block(chunk,x,y,z)==BETA_BLOCK_WOOD_DOOR || chunk_get_block(chunk,x,y,z)==BETA_BLOCK_IRON_DOOR)
                        emit_door(renderer,world,mesh,chunk,x,y,z,chunk_get_block(chunk,x,y,z),chunk_get_metadata(chunk,x,y,z));
                    else if (chunk_get_block(chunk,x,y,z)==BETA_BLOCK_WEB)
                        emit_cross_plant(renderer,world,mesh,chunk,x,y,z,BETA_BLOCK_WEB,0);
                    else if (chunk_get_block(chunk,x,y,z)==BETA_BLOCK_FIRE)
                        emit_cross_plant(renderer,world,mesh,chunk,x,y,z,BETA_BLOCK_FIRE,0);
                    else if (sign_is_block(chunk_get_block(chunk,x,y,z))) {
                        SignInstance *grown=(SignInstance *)realloc(mesh->signs,(mesh->sign_count+1)*sizeof(*grown));
                        if(grown) {
                            SignInstance *sign;
                            mesh->signs=grown; sign=&grown[mesh->sign_count++];
                            sign->x=(uint8_t)x; sign->y=(uint8_t)y; sign->z=(uint8_t)z;
                            sign->id=chunk_get_block(chunk,x,y,z); sign->metadata=chunk_get_metadata(chunk,x,y,z);
                        } else mesh->layers[0].overflow=1;
                    }
                    else if (beta_block_cross_plant(chunk_get_block(chunk,x,y,z)))
                        emit_cross_plant(renderer, world, mesh, chunk, x, y, z,
                                         chunk_get_block(chunk,x,y,z),
                                         chunk_get_metadata(chunk,x,y,z));
                    else if (chunk_get_block(chunk,x,y,z)==BETA_BLOCK_SLAB)
                        emit_slab(renderer,world,mesh,chunk,x,y,z,
                                  chunk_get_metadata(chunk,x,y,z));
                    else if(rail_is(chunk_get_block(chunk,x,y,z)))
                        emit_rail(renderer,world,mesh,chunk,x,y,z,chunk_get_block(chunk,x,y,z),chunk_get_metadata(chunk,x,y,z));
                    else if(chunk_get_block(chunk,x,y,z)==93 || chunk_get_block(chunk,x,y,z)==94)
                        emit_repeater(renderer,world,mesh,chunk,x,y,z,chunk_get_block(chunk,x,y,z),chunk_get_metadata(chunk,x,y,z));
                    else if(chunk_get_block(chunk,x,y,z)==29 || chunk_get_block(chunk,x,y,z)==33 || chunk_get_block(chunk,x,y,z)==34 || chunk_get_block(chunk,x,y,z)==36)
                        emit_piston(renderer,world,mesh,chunk,x,y,z,chunk_get_block(chunk,x,y,z),chunk_get_metadata(chunk,x,y,z));
                    else if(
                            chunk_get_block(chunk,x,y,z)==69 || chunk_get_block(chunk,x,y,z)==77 || chunk_get_block(chunk,x,y,z)==78 || chunk_get_block(chunk,x,y,z)==70 || chunk_get_block(chunk,x,y,z)==72)
                        emit_low_block(renderer,world,mesh,chunk,x,y,z,chunk_get_block(chunk,x,y,z),chunk_get_metadata(chunk,x,y,z));
    }
    return mesh;
}

static void upload_mesh(Renderer *renderer, ChunkMesh *mesh)
{
    int i;
    GLint old_array_buffer = 0;
    mesh->owner = renderer;
    mesh->delete_buffers = renderer->delete_buffers;
    if (!renderer->stats.vbo_available || renderer->options.vbo_mode == RENDERER_VBO_OFF ||
        renderer->vbo_upload_disabled) return;
    glGetIntegerv(GL_ARRAY_BUFFER_BINDING_ARB, &old_array_buffer);
    for (i = 0; i < 3; ++i) {
        MeshLayer *layer = &mesh->layers[i];
        uint32_t bytes;
        if (!layer->vertex_count) continue;
        bytes = layer->vertex_count * (uint32_t)sizeof(VoxelVertex);
        if (renderer->vbo_live_bytes >= renderer->stats.vbo_budget_bytes ||
            (uint64_t)bytes > renderer->stats.vbo_budget_bytes - renderer->vbo_live_bytes) {
            ++renderer->stats.vbo_budget_fallbacks_total;
            continue; /* Keep the CPU mesh for GL 1.1 client arrays. */
        }
        renderer->gen_buffers(1, &layer->vbo);
        if (!layer->vbo) {
            ++renderer->stats.vbo_upload_failures_total;
            renderer->vbo_upload_disabled = 1;
            break;
        }
        renderer->bind_buffer(GL_ARRAY_BUFFER_ARB, layer->vbo);
        renderer->buffer_data(GL_ARRAY_BUFFER_ARB,
                              (ptrdiff_t)bytes,
                              layer->vertices, GL_STATIC_DRAW_ARB);
        renderer->bind_buffer(GL_ARRAY_BUFFER_ARB, (GLuint)old_array_buffer);
        if (glGetError() != GL_NO_ERROR) {
            renderer->delete_buffers(1, &layer->vbo);
            layer->vbo = 0;
            ++renderer->stats.vbo_upload_failures_total;
            renderer->vbo_upload_disabled = 1;
            break;
        } else {
            renderer->vbo_live_bytes += bytes;
            layer->vbo_bytes = bytes;
            ++renderer->stats.vbo_uploads_total;
            renderer->stats.vbo_upload_bytes_total += bytes;
            free(layer->vertices);
            layer->vertices = NULL;
            layer->capacity = 0;
        }
    }
}

int renderer_rebuild_budget(Renderer *renderer, World *world,
                            int32_t camera_chunk_x, int32_t camera_chunk_z,
                            int max_chunks)
{
    int rebuilt = 0;
    if (!renderer || !world || max_chunks <= 0) return 0;
    world_set_render_data_destroy(world, chunk_mesh_destroy);
    renderer->hooked_world = world;
    while (rebuilt < max_chunks) {
        Chunk *chosen = NULL;
        uint64_t best_distance = UINT64_MAX;
        size_t index, count = world_cached_chunk_count(world);
        for (index = 0; index < count; ++index) {
            Chunk *candidate = world_cached_chunk_at(world, index);
            int64_t dx, dz;
            uint64_t distance;
            if (!candidate || (!(candidate->dirty_flags & (CHUNK_DIRTY_MESH |
                               (renderer->options.smooth_lighting ? CHUNK_DIRTY_SMOOTH_MESH : 0))) &&
                               candidate->render_data)) continue;
            dx = (int64_t)candidate->x - camera_chunk_x;
            dz = (int64_t)candidate->z - camera_chunk_z;
            distance = (uint64_t)(dx * dx + dz * dz);
            if (distance < best_distance) {
                best_distance = distance;
                chosen = candidate;
            }
        }
        if (!chosen) break;
        {
            ChunkMesh *replacement = build_chunk_mesh(renderer, world, chosen);
            if (!replacement) break;
            upload_mesh(renderer, replacement);
            chunk_mesh_destroy(chosen->render_data);
            chosen->render_data = replacement;
            chosen->dirty_flags &= ~(CHUNK_DIRTY_MESH|CHUNK_DIRTY_SMOOTH_MESH);
            ++rebuilt;
        }
    }
    renderer->stats.rebuilt_this_frame = (uint32_t)rebuilt;
    return rebuilt;
}

static int visible_compare_descending(const void *a, const void *b)
{
    const VisibleChunk *left = (const VisibleChunk *)a;
    const VisibleChunk *right = (const VisibleChunk *)b;
    if (left->distance2 < right->distance2) return 1;
    if (left->distance2 > right->distance2) return -1;
    return 0;
}

typedef struct ViewFrustum {
    float planes[5][3];
    float radii[5];
} ViewFrustum;

static float camera_fov(const RendererCamera *camera)
{
    float fov = camera->fov_y;
    if (fov < 30.0f) fov = 30.0f;
    if (fov > 110.0f) fov = 110.0f;
    return fov;
}

static void camera_projection(const RendererCamera *camera, float aspect,
                               float far_plane)
{
    const float near_plane = 0.05f;
    float top = near_plane * tanf(camera_fov(camera) *
                                 (float)(3.14159265358979323846 / 360.0));
    glLoadIdentity();
    glFrustum(-top * aspect, top * aspect, -top, top, near_plane, far_plane);
}

static void camera_view(const RendererCamera *camera)
{
    const float degrees = (float)(180.0 / 3.14159265358979323846);
    glLoadIdentity();
    glRotatef(-camera->pitch * degrees, 1, 0, 0);
    /* Player/raycast forward is (sin(yaw), 0, -cos(yaw)). */
    glRotatef(camera->yaw * degrees, 0, 1, 0);
    glTranslatef(-camera->x, -camera->y, -camera->z);
}

static void make_frustum(ViewFrustum *frustum, const RendererCamera *camera,
                          float aspect)
{
    float sy = sinf(camera->yaw), cy = cosf(camera->yaw);
    float sp = sinf(camera->pitch), cp = cosf(camera->pitch);
    float forward[3] = {sy * cp, sp, -cy * cp};
    float right[3] = {cy, 0.0f, sy};
    float up[3] = {-sy * sp, cp, cy * sp};
    float vertical = tanf(camera_fov(camera) *
                          (float)(3.14159265358979323846 / 360.0));
    float horizontal = vertical * aspect;
    int i;
    for (i = 0; i < 3; ++i) {
        frustum->planes[0][i] = forward[i];
        frustum->planes[1][i] = forward[i] * horizontal + right[i];
        frustum->planes[2][i] = forward[i] * horizontal - right[i];
        frustum->planes[3][i] = forward[i] * vertical + up[i];
        frustum->planes[4][i] = forward[i] * vertical - up[i];
    }
    for (i = 0; i < 5; ++i) {
        const float *plane = frustum->planes[i];
        frustum->radii[i] = (fabsf(plane[0]) + fabsf(plane[2])) *
                              (WORLD_CHUNK_SIZE * 0.5f) +
                            fabsf(plane[1]) * (WORLD_HEIGHT * 0.5f);
    }
}

static int chunk_in_view(const Chunk *chunk, const RendererCamera *camera,
                         int distance_chunks, const ViewFrustum *frustum)
{
    float cx = (float)(chunk->x * WORLD_CHUNK_SIZE + WORLD_CHUNK_SIZE / 2) - camera->x;
    float cy = WORLD_HEIGHT * 0.5f - camera->y;
    float cz = (float)(chunk->z * WORLD_CHUNK_SIZE + WORLD_CHUNK_SIZE / 2) - camera->z;
    float maximum = (float)(distance_chunks * WORLD_CHUNK_SIZE + 24);
    int i;
    if (cx * cx + cz * cz > maximum * maximum) return 0;
    for (i = 0; i < 5; ++i) {
        const float *plane = frustum->planes[i];
        if (cx * plane[0] + cy * plane[1] + cz * plane[2] < -frustum->radii[i])
            return 0;
    }
    return 1;
}

static void draw_layer(Renderer *renderer, const MeshLayer *layer)
{
    uintptr_t base;
    if (!layer->vertex_count) return;
    if (layer->vbo) renderer->stats.using_vbo = 1;
    if (renderer->stats.vbo_available) {
        renderer->bind_buffer(GL_ARRAY_BUFFER_ARB, layer->vbo);
    }
    base = layer->vbo ? (uintptr_t)0 : (uintptr_t)layer->vertices;
    glVertexPointer(3, GL_SHORT, sizeof(VoxelVertex),
                    (const GLvoid *)(base + offsetof(VoxelVertex, x)));
    glTexCoordPointer(2, GL_SHORT, sizeof(VoxelVertex),
                      (const GLvoid *)(base + offsetof(VoxelVertex, u)));
    glColorPointer(4, GL_UNSIGNED_BYTE, sizeof(VoxelVertex),
                   (const GLvoid *)(base + offsetof(VoxelVertex, r)));
    glDrawArrays(GL_QUADS, 0, (GLsizei)layer->vertex_count);
    renderer->stats.draw_calls++;
    if (!layer->vbo)
        renderer->stats.client_vertex_bytes_total +=
            (uint64_t)layer->vertex_count * sizeof(VoxelVertex);
}

static void draw_celestials(const World *world,const RendererCamera *camera,float aspect)
{
    int i;
    glPushAttrib(GL_ALL_ATTRIB_BITS);
    glMatrixMode(GL_PROJECTION); glPushMatrix(); camera_projection(camera,aspect,256);
    glMatrixMode(GL_TEXTURE); glPushMatrix(); glLoadIdentity();
    glMatrixMode(GL_MODELVIEW); glPushMatrix();
    glTranslatef(camera->x,camera->y,camera->z);
    glRotatef(-90,0,1,0); glRotatef(world_celestial_angle(world,0)*360,1,0,0);
    glDisable(GL_DEPTH_TEST); glDepthMask(GL_FALSE); glDisable(GL_CULL_FACE);
    glDisable(GL_FOG); glDisable(GL_LIGHTING); glDisable(GL_ALPHA_TEST);
    glEnable(GL_TEXTURE_2D); glEnable(GL_BLEND); glBlendFunc(GL_SRC_ALPHA,GL_ONE);
    glTexEnvi(GL_TEXTURE_ENV,GL_TEXTURE_ENV_MODE,GL_MODULATE);
    glColor4f(1,1,1,1-world->rain_strength);
    for(i=0;i<2;++i) {
        Texture2D texture=assets_get_texture(i ? ASSET_MOON : ASSET_SUN);
        float y=i ? -100 : 100,size=i ? 20 : 30;
        glBindTexture(GL_TEXTURE_2D,texture.id); glBegin(GL_QUADS);
        glTexCoord2f(0,0); glVertex3f(-size,y,-size);
        glTexCoord2f(1,0); glVertex3f(size,y,-size);
        glTexCoord2f(1,1); glVertex3f(size,y,size);
        glTexCoord2f(0,1); glVertex3f(-size,y,size);
        glEnd();
    }
    glPopMatrix(); glMatrixMode(GL_TEXTURE); glPopMatrix();
    glMatrixMode(GL_PROJECTION); glPopMatrix(); glMatrixMode(GL_MODELVIEW); glPopAttrib();
}

/* Beta's SignModel is only a board and an optional post. Keep its 64x32
 * texture layout instead of adding sign images to the terrain atlas. */
static void sign_box(float x,float y,float z,int w,int h,int d,int u,int v)
{
    const float p[8][3]={{x,y,z},{x+w,y,z},{x+w,y+h,z},{x,y+h,z},
        {x,y,z+d},{x+w,y,z+d},{x+w,y+h,z+d},{x,y+h,z+d}};
    static const int face[6][4]={{5,1,2,6},{0,4,7,3},{5,4,0,1},{2,3,7,6},{1,0,3,2},{4,5,6,7}};
    const int uv[6][4]={{u+d+w,v+d,u+d+w+d,v+d+h},{u,v+d,u+d,v+d+h},
        {u+d,v,u+d+w,v+d},{u+d+w,v,u+d+w+w,v+d},{u+d,v+d,u+d+w,v+d+h},
        {u+d+w+d,v+d,u+d+w+d+w,v+d+h}};
    int f,i;
    for(f=0;f<6;++f) for(i=0;i<4;++i) {
        glTexCoord2f((float)uv[f][i==0 || i==3 ? 2 : 0]/64,(float)uv[f][i<2 ? 1 : 3]/32);
        glVertex3fv(p[face[f][i]]);
    }
}

static unsigned sign_line_draw(const char *line,int row)
{
    const char *cursor=line; int x=0,width=MeasureMinecraftText(line,8),y=row*10-20;
    unsigned quads=0;
    while(*cursor) {
        unsigned codepoint=MinecraftTextCodepoint(&cursor),glyph;
        if(codepoint==0xa7 && *cursor) { (void)MinecraftTextCodepoint(&cursor); continue; }
        glyph=MinecraftGlyph(codepoint);
        float u=(float)(glyph%16)/16,v=(float)(glyph/16)/16;
        if(glyph!=' ') {
            int left=x-width/2;
            glTexCoord2f(u,v); glVertex3f((float)left,(float)y,0);
            glTexCoord2f(u+.0625f,v); glVertex3f((float)left+8,(float)y,0);
            glTexCoord2f(u+.0625f,v+.0625f); glVertex3f((float)left+8,(float)y+8,0);
            glTexCoord2f(u,v+.0625f); glVertex3f((float)left,(float)y+8,0);
            ++quads;
        }
        x+=(int)MinecraftGlyphWidth(glyph);
    }
    return quads;
}

static void draw_signs(Renderer *r,const World *world,const RendererCamera *camera,
                       const VisibleChunk *visible,size_t count)
{
    Texture2D sign_texture={0},font={0}; size_t n; int ready=0;
    for(n=0;n<count;++n) {
        const ChunkMesh *mesh=visible[n].mesh;
        const Chunk *chunk=world_peek_chunk(world,mesh->chunk_x,mesh->chunk_z);
        unsigned i;
        if(!chunk || !mesh->sign_count) continue;
        if(!ready) {
            /* Texture uploads may change GL binding; save the terrain state
             * before requesting a lazily loaded sign/font texture. */
            glPushAttrib(GL_ALL_ATTRIB_BITS);
            sign_texture=assets_get_texture(ASSET_SIGN); font=assets_get_texture(ASSET_FONT_ASCII);
            /* Prepare cached glyph metrics before entering glBegin. */
            (void)MinecraftGlyphWidth('A');
            glMatrixMode(GL_TEXTURE); glPushMatrix(); glLoadIdentity(); glMatrixMode(GL_MODELVIEW);
            glDisable(GL_BLEND); glDisable(GL_CULL_FACE);
            glEnable(GL_ALPHA_TEST); glAlphaFunc(GL_GREATER,.1f); glDepthMask(GL_TRUE);
            ready=1;
        }
        for(i=0;i<mesh->sign_count;++i) {
            const SignInstance *sign=&mesh->signs[i];
            int x=mesh->chunk_x*16+sign->x,z=mesh->chunk_z*16+sign->z;
            const char (*lines)[SIGN_LINE_BYTES]=sign_text_get(world,x,sign->y,z);
            float dx=x+.5f-camera->x,dy=sign->y+.5f-camera->y,dz=z+.5f-camera->z,ambient;
            uint8_t light,color; unsigned row,text_quads=0;
            /* The original special renderer skips tile entities past 64 blocks. */
            if(dx*dx+dy*dy+dz*dz>64*64) continue;
            light=light_at(world,chunk,sign->x,sign->y,sign->z);
            ambient=.06f+r->options.brightness*.0034f;
            color=clamp_byte((int)(255*(ambient+(1-ambient)*light/15)));
            glPushMatrix(); glTranslatef(x+.5f,sign->y+.5f,z+.5f);
            if(sign->id==BETA_BLOCK_STANDING_SIGN) glRotatef(-sign->metadata*22.5f,0,1,0);
            else {
                float angle=sign->metadata==2 ? 180 : sign->metadata==4 ? 90 : sign->metadata==5 ? -90 : 0;
                glRotatef(-angle,0,1,0); glTranslatef(0,-.3125f,-.4375f);
            }
            glBindTexture(GL_TEXTURE_2D,sign_texture.id); glColor3ub(color,color,color);
            glPushMatrix(); glScalef(1.0f/24,-1.0f/24,-1.0f/24);
            glBegin(GL_QUADS); sign_box(-12,-14,-1,24,12,2,0,0);
            if(sign->id==BETA_BLOCK_STANDING_SIGN) sign_box(-1,-2,-1,2,14,2,0,14);
            glEnd(); glPopMatrix(); ++r->stats.draw_calls;
            r->stats.quads_opaque+=sign->id==BETA_BLOCK_STANDING_SIGN ? 12u : 6u;
            if(lines) {
                glTranslatef(0,1.0f/3,.07f*(2.0f/3)); glScalef(1.0f/90,-1.0f/90,1.0f/90);
                glBindTexture(GL_TEXTURE_2D,font.id); glColor3ub(0,0,0); glDepthMask(GL_FALSE);
                glBegin(GL_QUADS);
                for(row=0;row<SIGN_LINES;++row) text_quads+=sign_line_draw(lines[row],(int)row);
                glEnd(); glDepthMask(GL_TRUE);
                if(text_quads) { ++r->stats.draw_calls; r->stats.quads_cutout+=text_quads; }
            }
            glPopMatrix();
        }
    }
    if(ready) {
        glMatrixMode(GL_TEXTURE); glPopMatrix(); glMatrixMode(GL_MODELVIEW); glPopAttrib();
    }
}

void renderer_draw(Renderer *renderer, const World *world,
                   const RendererCamera *camera,
                   int viewport_width, int viewport_height,
                   int render_distance_chunks)
{
    VisibleChunk *visible;
    size_t count, index, visible_count = 0;
    int layer;
    float aspect, far_plane;
    ViewFrustum frustum;
    GLint old_matrix_mode, old_array_buffer = 0;
    GLfloat fog_color[4] = {0.58f, 0.73f, 0.94f, 1.0f};
    if (!renderer || !world || !camera || viewport_width <= 0 || viewport_height <= 0) return;
    {
        float daylight=world_daylight(world,0);
        float weather=1-world->rain_strength*.5f;
        fog_color[0]*=(daylight*.94f+.06f)*weather;
        fog_color[1]*=(daylight*.94f+.06f)*weather;
        fog_color[2]*=(daylight*.91f+.09f)*weather;
    }
    if (render_distance_chunks < 1) render_distance_chunks = 1;
    if (render_distance_chunks > 32) render_distance_chunks = 32;
    aspect = (float)viewport_width / (float)viewport_height;
    far_plane = (float)(render_distance_chunks * WORLD_CHUNK_SIZE + 32);
    renderer->last_far_plane = far_plane;
    make_frustum(&frustum, camera, aspect);
    count = world_cached_chunk_count(world);
    if (count > renderer->visible_capacity || !renderer->visible_scratch) {
        size_t capacity = renderer->visible_capacity ? renderer->visible_capacity : 64u;
        while (capacity < count) capacity *= 2u;
        visible = (VisibleChunk *)realloc(renderer->visible_scratch,
                                           capacity * sizeof(*visible));
        if (!visible) return;
        renderer->visible_scratch = visible;
        renderer->visible_capacity = capacity;
    }
    visible = renderer->visible_scratch;
    renderer->stats.cached_chunks = (uint32_t)count;
    renderer->stats.visible_chunks = 0;
    renderer->stats.pending_rebuilds = 0;
    renderer->stats.draw_calls = 0;
    renderer->stats.using_vbo = 0;
    renderer->stats.vertices_drawn = 0;
    renderer->stats.triangles_drawn = 0;
    renderer->stats.quads_opaque = 0;
    renderer->stats.quads_cutout = 0;
    renderer->stats.quads_transparent = 0;
    renderer->stats.mesh_overflows = 0;
    renderer->stats.cpu_mesh_bytes = 0;
    renderer->stats.gpu_mesh_bytes = 0;
    for (index = 0; index < count; ++index) {
        const Chunk *chunk = world_cached_chunk_at(world, index);
        const ChunkMesh *mesh;
        float dx, dz;
        if (!chunk) continue;
        if ((chunk->dirty_flags & CHUNK_DIRTY_MESH) || !chunk->render_data)
            renderer->stats.pending_rebuilds++;
        mesh = (const ChunkMesh *)chunk->render_data;
        if (!mesh) continue;
        for (layer = 0; layer < 3; ++layer) {
            const MeshLayer *part = &mesh->layers[layer];
            uint64_t bytes = (uint64_t)part->vertex_count * sizeof(VoxelVertex);
            if (part->vertices)
                renderer->stats.cpu_mesh_bytes += (uint64_t)part->capacity * sizeof(VoxelVertex);
            if (part->vbo) renderer->stats.gpu_mesh_bytes += bytes;
            renderer->stats.mesh_overflows += part->overflow;
        }
        if (!chunk_in_view(chunk, camera, render_distance_chunks, &frustum)) continue;
        dx = (float)(chunk->x * WORLD_CHUNK_SIZE + WORLD_CHUNK_SIZE / 2) - camera->x;
        dz = (float)(chunk->z * WORLD_CHUNK_SIZE + WORLD_CHUNK_SIZE / 2) - camera->z;
        visible[visible_count].mesh = mesh;
        visible[visible_count].distance2 = dx * dx + dz * dz;
        ++visible_count;
    }
    renderer->stats.visible_chunks = (uint32_t)visible_count;
    renderer->animated_visible=0;
    for(index=0;index<visible_count;++index) renderer->animated_visible|=visible[index].mesh->animations;
    glGetIntegerv(GL_MATRIX_MODE, &old_matrix_mode);
    if (renderer->stats.vbo_available)
        glGetIntegerv(GL_ARRAY_BUFFER_BINDING_ARB, &old_array_buffer);
    glPushAttrib(GL_ALL_ATTRIB_BITS);
    glPushClientAttrib(GL_CLIENT_ALL_ATTRIB_BITS);
    glMatrixMode(GL_PROJECTION);
    glPushMatrix();
    camera_projection(camera, aspect, far_plane);
    glMatrixMode(GL_MODELVIEW);
    glPushMatrix();
    camera_view(camera);
    glMatrixMode(GL_TEXTURE);
    glPushMatrix();
    glLoadIdentity();
    glScalef(1.0f / 32767.0f, 1.0f / 32767.0f, 1.0f);
    glMatrixMode(GL_MODELVIEW);
    glViewport(0, 0, viewport_width, viewport_height);
    glClearColor(fog_color[0], fog_color[1], fog_color[2], 1.0f);
    glDisable(GL_SCISSOR_TEST);
    glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
    glDepthMask(GL_TRUE);
    glClearDepth(1.0);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    draw_celestials(world,camera,aspect);
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LEQUAL);
    glDepthMask(GL_TRUE);
    glEnable(GL_CULL_FACE);
    glCullFace(GL_BACK);
    glFrontFace(GL_CCW);
    glDisable(GL_LIGHTING);
    glShadeModel(renderer->options.smooth_lighting ? GL_SMOOTH : GL_FLAT);
    glDisable(GL_BLEND);
    glDisable(GL_ALPHA_TEST);
    glEnable(GL_TEXTURE_2D);
    glBindTexture(GL_TEXTURE_2D, renderer->atlas);
    glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
    if (renderer->options.fog) glEnable(GL_FOG);
    else glDisable(GL_FOG);
    glFogi(GL_FOG_MODE, GL_LINEAR);
    glFogfv(GL_FOG_COLOR, fog_color);
    glFogf(GL_FOG_START, far_plane * 0.62f);
    glFogf(GL_FOG_END, far_plane - 8.0f);
    glEnableClientState(GL_VERTEX_ARRAY);
    glEnableClientState(GL_TEXTURE_COORD_ARRAY);
    glEnableClientState(GL_COLOR_ARRAY);
    for (layer = 0; layer < 3; ++layer) {
        if (layer == 1) {
            glEnable(GL_ALPHA_TEST);
            glAlphaFunc(GL_GREATER, 0.5f);
        } else if (layer == 2) {
            draw_signs(renderer,world,camera,visible,visible_count);
            glDisable(GL_ALPHA_TEST);
            glEnable(GL_BLEND);
            glDisable(GL_CULL_FACE);
            glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
            glDepthMask(GL_FALSE);
            qsort(visible, visible_count, sizeof(*visible), visible_compare_descending);
        }
        for (index = 0; index < visible_count; ++index) {
            const ChunkMesh *mesh = visible[index].mesh;
            const MeshLayer *part = &visible[index].mesh->layers[layer];
            if (part->vertex_count) {
                glPushMatrix();
                glTranslatef((float)(mesh->chunk_x * WORLD_CHUNK_SIZE), 0.0f,
                             (float)(mesh->chunk_z * WORLD_CHUNK_SIZE));
                glScalef(1.0f / VERTEX_COORD_SCALE,
                         1.0f / VERTEX_COORD_SCALE,
                         1.0f / VERTEX_COORD_SCALE);
                draw_layer(renderer, part);
                glPopMatrix();
            }
            if (layer == 0) renderer->stats.quads_opaque += part->vertex_count / 4u;
            else if (layer == 1) renderer->stats.quads_cutout += part->vertex_count / 4u;
            else renderer->stats.quads_transparent += part->vertex_count / 4u;
        }
    }
    renderer->stats.vertices_drawn =
        (renderer->stats.quads_opaque + renderer->stats.quads_cutout +
         renderer->stats.quads_transparent) * 4u;
    renderer->stats.triangles_drawn =
        (renderer->stats.quads_opaque + renderer->stats.quads_cutout +
         renderer->stats.quads_transparent) * 2u;
    if (renderer->stats.vbo_available)
        renderer->bind_buffer(GL_ARRAY_BUFFER_ARB, (GLuint)old_array_buffer);
    glMatrixMode(GL_TEXTURE);
    glPopMatrix();
    glMatrixMode(GL_MODELVIEW);
    glPopMatrix();
    glMatrixMode(GL_PROJECTION);
    glPopMatrix();
    glMatrixMode(old_matrix_mode);
    glPopClientAttrib();
    glPopAttrib();
}

void renderer_draw_selection(Renderer *renderer, const RendererCamera *camera,
                             int viewport_width, int viewport_height,
                             int bx, int by, int bz, BetaBlockState state)
{
    static const uint8_t edges[12][2] = {
        {0,1}, {1,3}, {3,2}, {2,0}, {4,5}, {5,7}, {7,6}, {6,4},
        {0,4}, {1,5}, {2,6}, {3,7}
    };
    GLfloat corners[8][3];
    GLint old_matrix_mode;
    BetaBlockBox box;
    int i;
    if (!renderer || !camera || viewport_width <= 0 || viewport_height <= 0) return;
    if (!beta_block_selection_box(state, &box)) {
        box.min_x = box.min_y = box.min_z = 0.0f;
        box.max_x = box.max_y = box.max_z = 1.0f;
    }
    /* Expand slightly so edges do not fight the selected block's depth. */
    for (i = 0; i < 8; ++i) {
        corners[i][0] = (float)bx + ((i & 1) ? box.max_x + 0.002f : box.min_x - 0.002f);
        corners[i][1] = (float)by + ((i & 2) ? box.max_y + 0.002f : box.min_y - 0.002f);
        corners[i][2] = (float)bz + ((i & 4) ? box.max_z + 0.002f : box.min_z - 0.002f);
    }
    glGetIntegerv(GL_MATRIX_MODE, &old_matrix_mode);
    glPushAttrib(GL_ALL_ATTRIB_BITS);
    glMatrixMode(GL_PROJECTION);
    glPushMatrix();
    camera_projection(camera, (float)viewport_width / (float)viewport_height,
                       renderer->last_far_plane);
    glMatrixMode(GL_MODELVIEW);
    glPushMatrix();
    camera_view(camera);
    glViewport(0, 0, viewport_width, viewport_height);
    glDisable(GL_SCISSOR_TEST);
    glDisable(GL_LIGHTING);
    glDisable(GL_TEXTURE_2D);
    glDisable(GL_ALPHA_TEST);
    glDisable(GL_BLEND);
    glDisable(GL_FOG);
    glDisable(GL_LINE_STIPPLE);
    glDisable(GL_LINE_SMOOTH);
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LEQUAL);
    glDepthMask(GL_FALSE);
    glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
    glLineWidth(1.0f);
    glColor4ub(20, 20, 20, 255);
    /* One bounded immediate draw: 24 vertices, no buffer or client state. */
    glBegin(GL_LINES);
    for (i = 0; i < 12; ++i) {
        glVertex3fv(corners[edges[i][0]]);
        glVertex3fv(corners[edges[i][1]]);
    }
    glEnd();
    glPopMatrix();
    glMatrixMode(GL_PROJECTION);
    glPopMatrix();
    glMatrixMode(old_matrix_mode);
    glPopAttrib();
}

RendererStats renderer_stats(const Renderer *renderer)
{
    RendererStats empty = {0};
    return renderer ? renderer->stats : empty;
}

void renderer_shutdown(Renderer *renderer)
{
    if (!renderer) return;
    if (renderer->atlas) glDeleteTextures(1, &renderer->atlas);
    free(renderer->visible_scratch);
    free(renderer);
}
