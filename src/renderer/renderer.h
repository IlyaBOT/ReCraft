#ifndef RECRAFT_RENDERER_H
#define RECRAFT_RENDERER_H

#include <stdint.h>
#include "../world/world.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Call after InitWindow(), while the OpenGL context is current. */
typedef struct Renderer Renderer;
void renderer_animate(Renderer *renderer,uint64_t tick);

typedef enum RendererVboMode {
    RENDERER_VBO_AUTO = 0,
    RENDERER_VBO_ON = 1,
    RENDERER_VBO_OFF = 2
} RendererVboMode;

typedef struct RendererOptions {
    int vbo_mode;            /* RendererVboMode */
    int vbo_budget_mb;       /* 4, 8, 16, 32; 0 selects default 16. */
    int greedy;              /* 1: merge matching faces; 0: one quad per face. */
    int fog;
    int mipmap;              /* 0: nearest; 1..4: maximum mip level (GL 1.2/SGIS). */
    int smooth_lighting;
    int transparent_leaves;
    int brightness;          /* 0..100 ambient light; changes rebuild meshes. */
} RendererOptions;

typedef struct GpuCapabilities {
    int gl_major, gl_minor;
    int max_texture_size;
    int max_texture_units;
    int arb_vbo;
    int vbo_functions;
    int arb_multitexture;
    int arb_npot;
    int ext_anisotropic;
    int texture_compression;
    int occlusion_query;
    int generate_mipmap;
    int mipmap_level_control; /* GL 1.2 or SGIS_texture_lod. */
    int gpu_vertex_processing; /* -1 unknown, 0 CPU, 1 GPU (CGL on Mac). */
    int vram_report_valid;
    int texture_memory_report_valid;
    int gart_report_valid;
    uint64_t reported_vram_bytes;           /* Driver report, not free VRAM. */
    uint64_t reported_texture_memory_bytes; /* Not GART or app usage. */
    uint64_t reported_gart_bytes;           /* Zero/unknown without a driver API. */
    char vendor[96];
    char renderer[128];
    char version[96];
} GpuCapabilities;

typedef struct RendererCamera {
    float x, y, z;
    float yaw, pitch;       /* Radians; yaw 0 faces -Z, positive pitch looks up. */
    float fov_y;            /* Vertical field of view in degrees. */
} RendererCamera;

typedef struct RendererStats {
    int vbo_available;
    int using_vbo;
    uint32_t cached_chunks;
    uint32_t visible_chunks;
    uint32_t pending_rebuilds;
    uint32_t rebuilt_this_frame;
    uint32_t draw_calls;
    uint32_t vertices_drawn;
    uint32_t triangles_drawn;
    uint32_t quads_opaque;
    uint32_t quads_cutout;
    uint32_t quads_transparent;
    uint32_t mesh_overflows;
    uint64_t cpu_mesh_bytes;
    uint64_t gpu_mesh_bytes; /* Requested VBO storage, not confirmed residency. */
    uint64_t texture_bytes;  /* Uploaded atlas levels, not confirmed residency. */
    uint64_t vbo_budget_bytes; /* ReCraft's conservative allocation ceiling. */
    uint64_t vbo_upload_bytes_total;
    uint64_t client_vertex_bytes_total; /* Logical bytes submitted to draw calls. */
    uint32_t vbo_uploads_total;
    uint32_t vbo_budget_fallbacks_total;
    uint32_t vbo_upload_failures_total;
    int gpu_vertex_processing; /* -1 unknown, 0 CPU, 1 GPU. */
    int vram_report_valid;
    int texture_memory_report_valid;
    int gart_report_valid;
    uint64_t reported_vram_bytes;
    uint64_t reported_texture_memory_bytes;
    uint64_t reported_gart_bytes;
    char gpu_vendor[96];
    char gpu_renderer[128];
    char gpu_version[96];
} RendererStats;

Renderer *renderer_init(void);
GpuCapabilities renderer_capabilities(const Renderer *renderer);
RendererOptions renderer_options(const Renderer *renderer);
void renderer_set_options(Renderer *renderer, World *world, RendererOptions options);

/* Rebuilds at most max_chunks dirty cached meshes nearest to the camera. */
int renderer_rebuild_budget(Renderer *renderer, World *world,
                            int32_t camera_chunk_x, int32_t camera_chunk_z,
                            int max_chunks);

/* Draws terrain into the current framebuffer. Call before 2D HUD rendering. */
void renderer_draw(Renderer *renderer, const World *world,
                   const RendererCamera *camera,
                   int viewport_width, int viewport_height,
                   int render_distance_chunks);

/* Call after renderer_draw() and before the 2D HUD for the raycast target. */
void renderer_draw_selection(Renderer *renderer, const RendererCamera *camera,
                             int viewport_width, int viewport_height,
                             int bx, int by, int bz, BetaBlockState state);

RendererStats renderer_stats(const Renderer *renderer);

/* World chunks retain their mesh until world_close() invokes its destroy hook. */
void renderer_shutdown(Renderer *renderer);

#ifdef __cplusplus
}
#endif

#endif
