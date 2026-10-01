#include "../src/renderer/renderer.c"
#include "../src/renderer/menu_background.c"
#include <assert.h>
#ifndef _WIN32
#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>
#endif

static void equal_matrix(const GLfloat *a, const GLfloat *b)
{
    int i;
    for (i = 0; i < 16; ++i) assert(fabsf(a[i] - b[i]) < 0.00001f);
}

int main(int argc, char **argv)
{
    World world;
    Chunk *chunk;
    Renderer basic = {0};
    ChunkMesh *greedy, *plain;
    uint32_t g, p;
    int x, y, z, i;
#ifdef _WIN32
    WNDCLASSA klass = {0};
    HWND window;
    HDC dc;
    HGLRC context;
    PIXELFORMATDESCRIPTOR pfd = {0};
#else
    GLFWwindow *window;
    (void)argc; (void)argv;
#endif
    Renderer *renderer;
    RendererCamera camera = {8.0f, 8.0f, 22.0f, 0.7f, 0.4f, 70.0f};
    GLfloat before_model[16], before_projection[16], after[16], width;
    GLint viewport[4], binding, mode;
    GLuint probe = 0;
    GLubyte center_pixel[4] = {0};
    const GLfloat test_vertices[9] = {0};
    const GLvoid *pointer;
    ViewFrustum frustum;
    Chunk probe_chunk;
    {
        World light_world;
        Chunk *a,*b;
        ChunkMesh *ma=(ChunkMesh *)calloc(1,sizeof(*ma));
        ChunkMesh *mb=(ChunkMesh *)calloc(1,sizeof(*mb));
        Renderer smooth={0};
        assert(ma && mb && world_init(&light_world,1,1,4)==WORLD_OK);
        a=world_get_chunk(&light_world,0,0); b=world_get_chunk(&light_world,1,0);
        memset(a->sky_light,0,sizeof(a->sky_light));
        memset(b->sky_light,0,sizeof(b->sky_light));
        for (x=14;x<=17;++x) for (z=7;z<=10;++z)
            chunk_set_block_light(x<16?a:b,x&15,110,z,(uint8_t)(14-(x-14)-(z-7)));
        smooth.options.smooth_lighting=1;
        smooth.options.brightness=50;
        emit_quad(&smooth,&light_world,ma,a,face_key(&smooth,BLOCK_STONE,0,1,1,14),1,1,109,8,15,1,1);
        emit_quad(&smooth,&light_world,mb,b,face_key(&smooth,BLOCK_STONE,0,1,1,13),1,1,109,8,0,1,1);
        assert(ma->layers[0].vertices[2].r==mb->layers[0].vertices[1].r);
        assert(ma->layers[0].vertices[3].r==mb->layers[0].vertices[0].r);
        assert(ma->layers[0].vertices[0].r!=ma->layers[0].vertices[2].r);
        chunk_mesh_destroy(ma); chunk_mesh_destroy(mb);
        assert(world_close(&light_world)==WORLD_OK);
    }
    assert(world_init(&world, 1, 1, 8) == WORLD_OK);
    chunk = world_get_chunk(&world, 0, 0);
    assert(chunk);
    assert(!visible_face(BETA_BLOCK_FLOWING_WATER, BETA_BLOCK_STILL_WATER));
    assert(beta_source_tile(14)==116 && beta_source_tile(15)==117);
    assert(beta_source_tile(16)==132 && beta_source_tile(17)==4);
    assert(beta_source_tile(18)==64 && beta_source_tile(19)==210 &&
           beta_source_tile(33)==113);
    assert(beta_source_tile(34)==17 && beta_source_tile(38)==160 &&
           beta_source_tile(44)==51 && beta_source_tile(48)==105);
    assert(beta_source_tile(49)==15 && beta_source_tile(50)==63 &&
           beta_source_tile(51)==79 && beta_source_tile(52)==39 &&
           beta_source_tile(53)==55 && beta_source_tile(54)==56 &&
           beta_source_tile(59)==73);
    assert(beta_source_tile(0)==6 && beta_source_tile(60)==5 &&
           beta_source_tile(61)==208 && beta_source_tile(62)==176 &&
           beta_source_tile(63)==192);
    assert(atlas_upload_bytes()==1398100u);
    memset(chunk->blocks, 0, sizeof(chunk->blocks));
    memset(chunk->sky_light, 255, sizeof(chunk->sky_light));
    for (y = 0; y < 16; ++y)
        for (z = 0; z < 16; ++z)
            for (x = 0; x < 16; ++x)
                chunk_set_block(chunk, x, y, z, BLOCK_STONE);
    basic.options.greedy = 1;
    basic.options.brightness = 50;
    assert(sizeof(VoxelVertex) == 16);
    greedy = build_chunk_mesh(&basic, &world, chunk);
    basic.options.greedy = 0;
    plain = build_chunk_mesh(&basic, &world, chunk);
    assert(greedy && plain);
    g = greedy->layers[0].vertex_count / 4;
    p = plain->layers[0].vertex_count / 4;
    printf("solid16 cube quads greedy=%u plain=%u\n", g, p);
    assert(g == 96 && p == 1536);
    for (i = 0; i < (int)greedy->layers[0].vertex_count; i += 4) {
        const VoxelVertex *v = greedy->layers[0].vertices + i;
        float du = fabsf((float)v[0].u - (float)v[2].u) * ATLAS_PIXELS / 32767.0f;
        float dv = fabsf((float)v[0].v - (float)v[2].v) * ATLAS_PIXELS / 32767.0f;
        assert(fabsf(du - 63.0f) < 0.03f && fabsf(dv - 63.0f) < 0.03f);
        for (x = 0; x < 4; ++x) {
            assert(v[x].x >= 0 && v[x].x <= 16 * 16);
            assert(v[x].y >= 0 && v[x].y <= 16 * 16);
            assert(v[x].z >= 0 && v[x].z <= 16 * 16);
        }
    }
    chunk_mesh_destroy(greedy);
    chunk_mesh_destroy(plain);
    /* Wool metadata must prevent greedy meshing from merging different colors. */
    basic.options.greedy = 1;
    chunk_set_block(chunk, 0, 16, 0, BETA_BLOCK_WOOL);
    chunk_set_block(chunk, 1, 16, 0, BETA_BLOCK_WOOL);
    greedy = build_chunk_mesh(&basic, &world, chunk);
    assert(greedy && greedy->layers[0].vertex_count > 0);
    chunk_set_metadata(chunk, 1, 16, 0, 1);
    plain = build_chunk_mesh(&basic, &world, chunk);
    assert(plain && plain->layers[0].vertex_count > greedy->layers[0].vertex_count);
    assert(((face_key(&basic, BETA_BLOCK_WOOL, 0, 1, 1, 15) >> 8) & 255u)==18u);
    assert(((face_key(&basic, BETA_BLOCK_WOOL, 1, 1, 1, 15) >> 8) & 255u)==19u);
    assert(block_face_tile(world_block_def(BETA_BLOCK_LOG), BETA_BLOCK_LOG, 1, 0, 1)==14);
    assert(block_face_tile(world_block_def(BETA_BLOCK_LOG), BETA_BLOCK_LOG, 2, 0, 1)==15);
    assert(block_face_tile(world_block_def(BETA_BLOCK_LOG), BETA_BLOCK_LOG, 2, 1, 1)==8);
    assert(block_face_tile(world_block_def(BETA_BLOCK_LEAVES), BETA_BLOCK_LEAVES, 1, 0, 1)==16);
    assert(block_face_tile(world_block_def(BETA_BLOCK_SLAB), BETA_BLOCK_SLAB, 1, 1, 1)==62);
    assert(block_face_tile(world_block_def(BETA_BLOCK_SLAB), BETA_BLOCK_SLAB, 1, 1, -1)==61);
    assert(block_face_tile(world_block_def(BETA_BLOCK_SLAB), BETA_BLOCK_SLAB, 1, 0, 1)==63);
    chunk_mesh_destroy(greedy);
    chunk_mesh_destroy(plain);
    chunk_set_block(chunk, 0, 16, 0, BLOCK_AIR);
    chunk_set_block(chunk, 1, 16, 0, BLOCK_AIR);
    chunk_set_block(chunk, 0, 16, 0, BETA_BLOCK_SAPLING);
    greedy = build_chunk_mesh(&basic, &world, chunk);
    assert(greedy && greedy->layers[1].vertex_count==16u);
    assert(greedy->layers[0].vertex_count==96u*4u);
    for (i=0; i<(int)greedy->layers[1].vertex_count; ++i)
        assert(greedy->layers[1].vertices[i].y>=16*16 &&
               greedy->layers[1].vertices[i].y<=16*16+13);
    chunk_mesh_destroy(greedy);
    chunk_set_metadata(chunk, 0, 16, 0, 2);
    assert(cross_slot(BETA_BLOCK_SAPLING,2)==51u);
    assert(cross_slot(BETA_BLOCK_TALL_GRASS,2)==54u);
    chunk_set_block(chunk, 0, 16, 0, BLOCK_AIR);
    /* Half slab owns five visible faces next to a full cube. The cube face
     * facing it is clipped to its upper half, with no coplanar lower face. */
    memset(chunk->blocks,0,sizeof(chunk->blocks));
    chunk_set_block(chunk,4,16,4,BETA_BLOCK_SLAB);
    chunk_set_block(chunk,5,16,4,BLOCK_STONE);
    greedy=build_chunk_mesh(&basic,&world,chunk);
    assert(greedy && greedy->layers[0].vertex_count==11u*4u);
    {
        int upper_neighbor_faces=0, slab_top_faces=0;
        for (i=0;i<(int)greedy->layers[0].vertex_count;i+=4) {
            const VoxelVertex *vertices=greedy->layers[0].vertices+i;
            int j, min_y=32767, max_y=-32768, on_x5=1;
            for(j=0;j<4;++j) {
                if(vertices[j].y<min_y) min_y=vertices[j].y;
                if(vertices[j].y>max_y) max_y=vertices[j].y;
                if(vertices[j].x!=5*16) on_x5=0;
            }
            if(on_x5 && min_y==16*16+8 && max_y==17*16) ++upper_neighbor_faces;
            if(min_y==16*16+8 && max_y==min_y) ++slab_top_faces;
        }
        assert(upper_neighbor_faces==1 && slab_top_faces==1);
    }
    chunk_mesh_destroy(greedy);
    {
        static const uint8_t orientations[3]={1,2,5};
        static const int min_x_bounds[3]={64,75,71};
        static const int max_x_bounds[3]={69,80,73};
        int orientation;
        for(orientation=0;orientation<3;++orientation) {
            ChunkMesh torch_mesh={0};
            int min_x=32767,max_x=-32768,min_y=32767,max_y=-32768;
            emit_torch(&torch_mesh,4,16,4,orientations[orientation]);
            assert(torch_mesh.layers[1].vertex_count==20u);
            for(i=0;i<20;++i) {
                const VoxelVertex *vertex=torch_mesh.layers[1].vertices+i;
                if(vertex->x<min_x) min_x=vertex->x;
                if(vertex->x>max_x) max_x=vertex->x;
                if(vertex->y<min_y) min_y=vertex->y;
                if(vertex->y>max_y) max_y=vertex->y;
            }
            assert(min_x==min_x_bounds[orientation]);
            assert(max_x==max_x_bounds[orientation]);
            assert(min_y==(orientations[orientation]==5 ? 256 : 259));
            assert(max_y==(orientations[orientation]==5 ? 266 : 269));
            free(torch_mesh.layers[1].vertices);
        }
    }
    memset(chunk->blocks,0,sizeof(chunk->blocks));
    for (y=0;y<16;++y) for(z=0;z<16;++z) for(x=0;x<16;++x)
        chunk_set_block(chunk,x,y,z,BLOCK_STONE);
    memset(&probe_chunk, 0, sizeof(probe_chunk));
    camera.x = 8.0f; camera.y = 64.0f; camera.z = 8.0f;
    camera.yaw = (float)(3.14159265358979323846 * 0.5); camera.pitch = 0;
    make_frustum(&frustum, &camera, 1.0f);
    probe_chunk.x = 4;
    assert(chunk_in_view(&probe_chunk, &camera, 8, &frustum));
    probe_chunk.x = -4;
    assert(!chunk_in_view(&probe_chunk, &camera, 8, &frustum));
#ifdef _WIN32
    klass.lpfnWndProc = DefWindowProcA;
    klass.hInstance = GetModuleHandleA(NULL);
    klass.lpszClassName = "ReCraftRendererCheck";
    klass.style = CS_OWNDC;
    assert(RegisterClassA(&klass));
    window = CreateWindowA(klass.lpszClassName, "Hidden renderer check", 0,
                            0, 0, 320, 240, NULL, NULL, klass.hInstance, NULL);
    assert(window);
    dc = GetDC(window);
    pfd.nSize = sizeof(pfd); pfd.nVersion = 1;
    pfd.dwFlags = PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER;
    pfd.iPixelType = PFD_TYPE_RGBA; pfd.cColorBits = 24; pfd.cDepthBits = 24;
    if (argc > 1 && strcmp(argv[1], "--software-gl11") == 0) {
        int formats = DescribePixelFormat(dc, 1, sizeof(pfd), &pfd);
        for (i = 1; i <= formats; ++i) {
            DescribePixelFormat(dc, i, sizeof(pfd), &pfd);
            if ((pfd.dwFlags & (PFD_GENERIC_FORMAT | PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL)) ==
                (PFD_GENERIC_FORMAT | PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL) &&
                (pfd.dwFlags & PFD_DOUBLEBUFFER) &&
                pfd.iPixelType == PFD_TYPE_RGBA && pfd.cColorBits >= 24 && pfd.cDepthBits >= 16)
                break;
        }
        assert(i <= formats);
    } else i = ChoosePixelFormat(dc, &pfd);
    assert(i && SetPixelFormat(dc, i, &pfd));
    context = wglCreateContext(dc);
    assert(context && wglMakeCurrent(dc, context));
#else
    assert(glfwInit());
    glfwWindowHint(GLFW_VISIBLE, GLFW_FALSE);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 1);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 1);
    window = glfwCreateWindow(320, 240, "Hidden renderer check", NULL, NULL);
    assert(window);
    glfwMakeContextCurrent(window);
#endif
    renderer = renderer_init();
    assert(renderer);
    assert(renderer_stats(renderer).vbo_budget_bytes == 16u * 1024u * 1024u);
    assert(renderer_stats(renderer).gart_report_valid == 0);
    assert(renderer_stats(renderer).gpu_vertex_processing == -1);
    glMatrixMode(GL_MODELVIEW);
    camera_view(&camera);
    glGetFloatv(GL_MODELVIEW_MATRIX, after);
    assert(fabsf(after[0]) < 0.00001f && fabsf(after[2] + 1.0f) < 0.00001f);
    camera.yaw = 0.0f; camera.pitch = 0.0f; camera.z = 22; camera.y = 8;
    assert(renderer_rebuild_budget(renderer, &world, 0, 0, 1) == 1);
    glMatrixMode(GL_MODELVIEW); glLoadIdentity(); glTranslatef(3, 4, 5);
    glGetFloatv(GL_MODELVIEW_MATRIX, before_model);
    glMatrixMode(GL_PROJECTION); glLoadIdentity(); glScalef(2, 3, 4);
    glGetFloatv(GL_PROJECTION_MATRIX, before_projection);
    glViewport(2, 3, 64, 48);
    glLineWidth(3.0f);
    if (renderer->stats.vbo_available) {
        renderer->gen_buffers(1, &probe);
        renderer->bind_buffer(GL_ARRAY_BUFFER_ARB, probe);
        renderer->buffer_data(GL_ARRAY_BUFFER_ARB, sizeof(test_vertices), test_vertices, GL_STATIC_DRAW_ARB);
    }
    glEnableClientState(GL_VERTEX_ARRAY);
    glVertexPointer(3, GL_FLOAT, 0, probe ? NULL : test_vertices);
    renderer_draw(renderer, &world, &camera, 320, 240, 4);
    assert(glGetError() == GL_NO_ERROR);
    assert(renderer_stats(renderer).vertices_drawn == 96u * 4u);
    glReadPixels(160, 120, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, center_pixel);
    assert(glGetError() == GL_NO_ERROR);
    assert(center_pixel[0] < 140 && center_pixel[1] < 160);
    {
        BetaBlockState state = { BETA_BLOCK_STONE, 0 };
        renderer_draw_selection(renderer, &camera, 320, 240, 8, 8, 15, state);
        state.id = BETA_BLOCK_DANDELION;
        renderer_draw_selection(renderer, &camera, 320, 240, 8, 8, 15, state);
    }
    assert(glGetError() == GL_NO_ERROR);
    glGetFloatv(GL_MODELVIEW_MATRIX, after); equal_matrix(before_model, after);
    glGetFloatv(GL_PROJECTION_MATRIX, after); equal_matrix(before_projection, after);
    glGetIntegerv(GL_MATRIX_MODE, &mode); assert(mode == GL_PROJECTION);
    glGetIntegerv(GL_VIEWPORT, viewport);
    assert(viewport[0] == 2 && viewport[1] == 3 && viewport[2] == 64 && viewport[3] == 48);
    glGetFloatv(GL_LINE_WIDTH, &width); assert(width == 3.0f);
    glGetPointerv(GL_VERTEX_ARRAY_POINTER, (GLvoid **)&pointer);
    assert(pointer == (probe ? NULL : test_vertices));
    if (probe) {
        glGetIntegerv(GL_ARRAY_BUFFER_BINDING_ARB, &binding);
        assert(binding == (GLint)probe);
        glGetIntegerv(0x8896 /* GL_VERTEX_ARRAY_BUFFER_BINDING_ARB */, &binding);
        assert(binding == (GLint)probe);
        renderer->delete_buffers(1, &probe);
    }
    for (i = 0; i <= 4; ++i) {
        RendererOptions options = renderer_options(renderer);
        options.mipmap = i;
        renderer_set_options(renderer, &world, options);
        assert(glGetError() == GL_NO_ERROR);
        glBindTexture(GL_TEXTURE_2D, renderer->atlas);
        glGetTexParameteriv(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, &binding);
        assert(binding == ((i && renderer->mip_level_limit) ? GL_NEAREST_MIPMAP_LINEAR : GL_NEAREST));
    }
    chunk_set_block(chunk, 8, 16, 8, BETA_BLOCK_SAPLING);
    assert(renderer_rebuild_budget(renderer, &world, 0, 0, 1) == 1);
    renderer_draw(renderer, &world, &camera, 320, 240, 4);
    assert(glGetError() == GL_NO_ERROR);
    assert(renderer_stats(renderer).quads_cutout == 4u);
    chunk_set_block(chunk, 8, 16, 8, BLOCK_AIR);
    assert(renderer_rebuild_budget(renderer, &world, 0, 0, 1) == 1);
    if (renderer->stats.vbo_available) {
        ChunkMesh *budget_mesh = build_chunk_mesh(renderer, &world, chunk);
        uint64_t old_bytes = renderer->vbo_live_bytes;
        uint32_t old_fallbacks = renderer->stats.vbo_budget_fallbacks_total;
        assert(budget_mesh);
        renderer->vbo_live_bytes = renderer->stats.vbo_budget_bytes;
        upload_mesh(renderer, budget_mesh);
        assert(budget_mesh->layers[0].vbo == 0);
        assert(budget_mesh->layers[0].vertices != NULL);
        assert(renderer->stats.vbo_budget_fallbacks_total > old_fallbacks);
        renderer->vbo_live_bytes = old_bytes;
        chunk_mesh_destroy(budget_mesh);
    }
    {
        RendererOptions options = renderer_options(renderer);
        options.vbo_budget_mb = 6;
        renderer_set_options(renderer, &world, options);
        assert(renderer_options(renderer).vbo_budget_mb == 4);
        assert(renderer_stats(renderer).vbo_budget_bytes == 4u * 1048576u);
        options.vbo_budget_mb = 0;
        renderer_set_options(renderer, &world, options);
        assert(renderer_options(renderer).vbo_budget_mb == 16);
    }
    {
        RendererOptions options = renderer_options(renderer);
        uint64_t submitted = renderer_stats(renderer).client_vertex_bytes_total;
        options.vbo_mode = RENDERER_VBO_OFF;
        renderer_set_options(renderer, &world, options);
        assert(renderer_rebuild_budget(renderer, &world, 0, 0, 1) == 1);
        renderer_draw(renderer, &world, &camera, 320, 240, 4);
        assert(glGetError() == GL_NO_ERROR);
        assert(renderer_stats(renderer).using_vbo == 0);
        assert(renderer_stats(renderer).client_vertex_bytes_total > submitted);
    }
    printf("hidden WGL draw+selection: GL errors=0, matrices/viewport/line/client-array/VBO preserved\n");
    assert(world_close(&world) == WORLD_OK);
    assert(renderer->vbo_live_bytes == 0);
    assert(world_init(&world, 1, 1, 8) == WORLD_OK);
    chunk = world_get_chunk(&world, 2, -3);
    assert(chunk);
    memset(chunk->blocks, 0, sizeof(chunk->blocks));
    memset(chunk->sky_light, 255, sizeof(chunk->sky_light));
    for (y = 0; y < 16; ++y)
        for (z = 0; z < 16; ++z)
            for (x = 0; x < 16; ++x)
                chunk_set_block(chunk, x, y, z, BLOCK_STONE);
    assert(renderer_rebuild_budget(renderer, &world, 2, -3, 1) == 1);
    assert(((ChunkMesh *)chunk->render_data)->chunk_x == 2);
    assert(((ChunkMesh *)chunk->render_data)->chunk_z == -3);
    camera.x = 40; camera.y = 8; camera.z = -26;
    renderer_draw(renderer, &world, &camera, 320, 240, 4);
    glReadPixels(160, 120, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, center_pixel);
    assert(glGetError() == GL_NO_ERROR);
    assert(center_pixel[0] < 140 && center_pixel[1] < 160);
    assert(world_close(&world) == WORLD_OK);
    assert(renderer->vbo_live_bytes == 0);
    renderer_shutdown(renderer);
    {
        MenuBackground bg={0};
        int sw,sh,blur;
        glDisableClientState(GL_VERTEX_ARRAY);
        glViewport(0,0,320,240);
        glMatrixMode(GL_PROJECTION); glLoadIdentity(); glOrtho(0,320,240,0,0,1);
        glMatrixMode(GL_MODELVIEW); glLoadIdentity();
        for (blur=0;blur<=1;++blur) {
            assert(menu_background_prepare(&bg,320,240,blur,0,&sw,&sh));
            assert(sw==320 && sh==240 && bg.texture);
            glClearColor(1,0,0,1); glClear(GL_COLOR_BUFFER_BIT);
            menu_background_capture(&bg); assert(bg.valid);
            assert(!menu_background_prepare(&bg,320,240,blur,0,&sw,&sh));
            glClearColor(0,0,1,1); glClear(GL_COLOR_BUFFER_BIT);
            menu_background_draw(&bg);
            glReadPixels(160,120,1,1,GL_RGBA,GL_UNSIGNED_BYTE,center_pixel);
            fprintf(stderr,"pause cache blur=%d RGB=%u,%u,%u\n",blur,center_pixel[0],center_pixel[1],center_pixel[2]);
            assert(center_pixel[0]>230 && center_pixel[2]<10);
            /* Live multiplayer requests a new scene every frame. Without
             * blur it draws directly, without a redundant framebuffer copy. */
            assert(menu_background_prepare(&bg,320,240,blur,1,&sw,&sh));
            if (!blur) { assert(!bg.texture); continue; }
            glClearColor(0,1,0,1); glClear(GL_COLOR_BUFFER_BIT);
            menu_background_capture(&bg);
            glClearColor(0,0,1,1); glClear(GL_COLOR_BUFFER_BIT);
            menu_background_draw(&bg);
            glReadPixels(160,120,1,1,GL_RGBA,GL_UNSIGNED_BYTE,center_pixel);
            assert(center_pixel[1]>230 && center_pixel[2]<10);
            assert(glGetError()==GL_NO_ERROR);
        }
        assert(menu_background_prepare(&bg,1920,1080,1,0,&sw,&sh));
        assert(sw==512 && sh==288);
        assert(bg.texture_width==512 && bg.texture_height==512);
        menu_background_clear(&bg);
    }
#ifdef _WIN32
    wglMakeCurrent(NULL, NULL); wglDeleteContext(context);
    ReleaseDC(window, dc); DestroyWindow(window);
    UnregisterClassA(klass.lpszClassName, klass.hInstance);
#else
    glfwDestroyWindow(window);
    glfwTerminate();
#endif
    return 0;
}
