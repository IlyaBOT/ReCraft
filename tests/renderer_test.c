#include "../src/renderer/renderer.c"
#include "../src/renderer/menu_background.c"
#include "../src/game/entity_render.h"
#include "../src/game/skin_geometry.h"
#include "../src/ui/gui_button.h"
#include "../src/ui/ui.h"
#include "ui/language.h"
#include "assets/resource_pack.h"
#include "util/game_paths.h"
#ifdef _WIN32
#include <direct.h>
#include <process.h>
#define fixture_pid() _getpid()
#define fixture_rmdir(p) _rmdir(p)
#else
#include <unistd.h>
#define fixture_pid() getpid()
#define fixture_rmdir(p) rmdir(p)
#endif
#include <assert.h>
#include "png_fixture.h"
#if defined(__APPLE__)
#include "macos_context.h"
#elif !defined(_WIN32)
#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>
static void context_error(int code,const char *description)
{
    fprintf(stderr,"renderer_test: GLFW error 0x%x: %s\n",code,description);
}
#endif

/* The test provides its framebuffer dimensions instead of querying a window. */
int recraft_screen_width(void) { return 320; }
int recraft_screen_height(void) { return 240; }
extern int stbi_write_png(const char *,int,int,int,const void *,int);

static void menu_render_test(const char *capture)
{
    Ui ui;
    UiAction action;
    GLubyte pixels[320*240*3],row[320*3];
    int y,visible=0;
    assets_init(RECRAFT_TEST_ASSET_ROOT); ui_init(&ui);
    glViewport(0,0,320,240); glDisable(GL_SCISSOR_TEST); glDisable(GL_DEPTH_TEST);
    glDisable(GL_LIGHTING); glDisable(GL_ALPHA_TEST);
    glEnable(GL_BLEND); glBlendFunc(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA);
    glTexEnvi(GL_TEXTURE_ENV,GL_TEXTURE_ENV_MODE,GL_MODULATE);
    glDepthMask(GL_TRUE); glClearColor(0,0,0,1); glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);
    glMatrixMode(GL_PROJECTION); glLoadIdentity(); glOrtho(0,320,240,0,0,1);
    glMatrixMode(GL_MODELVIEW); glLoadIdentity();
    glMatrixMode(GL_TEXTURE); glLoadIdentity(); glMatrixMode(GL_MODELVIEW);
    action=ui_frame(&ui,NULL,0,NULL,0);
    assert(action.type==UI_ACTION_NONE && ui.screen==UI_SCREEN_MAIN);
    glReadPixels(0,0,320,240,GL_RGB,GL_UNSIGNED_BYTE,pixels);
    assert(glGetError()==GL_NO_ERROR);
    for(y=0;y<320*240;++y) if(pixels[y*3]>20 || pixels[y*3+1]>20 || pixels[y*3+2]>20) ++visible;
    assert(visible>3000);
    if(capture) {
        for(y=0;y<120;++y) {
            GLubyte *a=pixels+y*320*3,*b=pixels+(239-y)*320*3;
            memcpy(row,a,sizeof(row)); memcpy(a,b,sizeof(row)); memcpy(b,row,sizeof(row));
        }
        assert(stbi_write_png(capture,320,240,3,pixels,320*3));
    }
    ui_shutdown(); assets_shutdown();
    fprintf(stderr,"renderer_test: actual main-menu UI rendered (%d pixels)\n",visible);
}

static void equal_matrix(const GLfloat *a, const GLfloat *b)
{
    int i;
    for (i = 0; i < 16; ++i) assert(fabsf(a[i] - b[i]) < 0.00001f);
}

static void entity_pick_test(void)
{
    RenderEntity entities[4]={{0}};
    RendererCamera camera={0,1.6f,0,0,0,70};
    entities[0].active=entities[1].active=1;
    entities[0].id=11; entities[0].z=-4;
    entities[1].id=22; entities[1].z=-2;
    assert(entity_pick(entities,4,&camera,6,6)==22);
    assert(entity_pick(entities,4,&camera,6,1)==-1); /* Wall occludes both. */
    assert(entity_pick(entities,4,&camera,1,6)==-1); /* Reach is independent. */
    entities[1].active=0;
    assert(entity_pick(entities,4,&camera,6,6)==11);
    entities[1].active=1; entities[1].type=255; /* Dropped items aren't attack targets. */
    assert(entity_pick(entities,4,&camera,6,6)==11);
    camera.yaw=3.14159265359f;
    assert(entity_pick(entities,4,&camera,6,6)==-1);
    camera.yaw=0; entities[0].y=4;
    assert(entity_pick(entities,4,&camera,6,6)==-1);
    assert(entity_pick(entities,0,&camera,6,6)==-1);
}

static void skin_geometry_test(void)
{
    float right[3],left[3],a[2],b[2];int face,corner,axis;
    for(face=0;face<6;++face)for(corner=0;corner<4;++corner) {
        skin_vertex(-3,-2,-2,4,12,4,40,16,0,32,0,face,corner,right,a);
        skin_vertex(-3,-2,-2,4,12,4,40,16,1,32,0,face,3-corner,left,b);
        assert(fabsf(left[0]-(-2-right[0]))<1e-6f);
        for(axis=1;axis<3;++axis)assert(right[axis]==left[axis]);
        assert(a[0]==b[0]&&a[1]==b[1]); /* Geometry and quad order mirror together. */
    }
    skin_vertex(-3,-2,-2,4,12,4,40,16,0,32,0,4,0,right,a);
    assert(right[0]==1&&right[1]==-2&&right[2]==-2);
    assert(fabsf(a[0]-47.9f/64)<1e-6f&&fabsf(a[1]-20.1f/32)<1e-6f);
    skin_vertex(-2,0,-2,4,12,4,0,16,0,32,0,4,0,right,a);
    assert(fabsf(a[0]-7.9f/64)<1e-6f); /* Leg uses its own atlas region. */
    skin_vertex(-1,-2,-2,4,12,4,32,48,0,64,0,4,0,right,a);
    assert(fabsf(a[0]-39.9f/64)<1e-6f&&fabsf(a[1]-52.1f/64)<1e-6f);
}

static void skin_upload_test(void)
{
    unsigned char png[512];size_t size=test_png_fixture(png);
    Texture2D before,after;GLint filter;Image image;
    assets_init(RECRAFT_TEST_ASSET_ROOT);
    before=assets_get_texture(ASSET_PLAYER_SKIN);assert(before.id&&before.height==32);
    assert(assets_skin_png_valid(png,size)&&assets_set_player_skin(png,size));
    assert(!glIsTexture(before.id));
    after=assets_get_texture(ASSET_PLAYER_SKIN);assert(after.id&&after.width==64&&after.height==64);
    image=assets_load_image(ASSET_PLAYER_SKIN);assert(image.data&&image.height==64);
    ImageFormat(&image,UNCOMPRESSED_R8G8B8A8);
    assert(((Color *)image.data)[52*64+36].b==192);UnloadImage(image);
    glBindTexture(GL_TEXTURE_2D,after.id);glGetTexParameteriv(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,&filter);assert(filter==GL_NEAREST);
    assert(!assets_set_player_skin(png,7)&&glIsTexture(after.id));
    assert(assets_select_pack("")&&assets_get_texture(ASSET_PLAYER_SKIN).height==64);
    assert(assets_set_player_skin(NULL,0)&&assets_get_texture(ASSET_PLAYER_SKIN).height==32);
    assets_shutdown();puts("Skin 64x64 import, cache invalidation, pack switch and nearest filtering passed");
}

static void button_texture_test(void)
{
    Image atlas;GLubyte pixel[3];int state,x,scale;GLint filter;
    assets_init(RECRAFT_TEST_ASSET_ROOT);atlas=assets_load_image(ASSET_GUI_WIDGETS);
    assert(atlas.data&&atlas.width==256&&atlas.height==256);
    ImageFormat(&atlas,UNCOMPRESSED_R8G8B8A8);
    glViewport(0,0,320,240);glDisable(GL_SCISSOR_TEST);glDisable(GL_DEPTH_TEST);
    glDisable(GL_LIGHTING);glDisable(GL_ALPHA_TEST);glDisable(GL_BLEND);
    glColor4ub(255,255,255,255);glTexEnvi(GL_TEXTURE_ENV,GL_TEXTURE_ENV_MODE,GL_MODULATE);
    glMatrixMode(GL_PROJECTION);glLoadIdentity();glOrtho(0,320,240,0,-1,1);
    glMatrixMode(GL_MODELVIEW);glLoadIdentity();glMatrixMode(GL_TEXTURE);glLoadIdentity();glMatrixMode(GL_MODELVIEW);
    for(state=0;state<4;++state)for(scale=1;scale<=2;++scale) {
        int row=state==GUI_BUTTON_DISABLED?46:state==GUI_BUTTON_NORMAL?66:86;
        gui_button_draw(0,0,100*scale,20*scale,NULL,(GuiButtonState)state);
        for(x=3;x<=93;x+=10) {
            int sx=x<50?x:100+x;
            Color expected=((Color *)atlas.data)[(row+5)*256+sx];
            glReadPixels(x*scale,239-5*scale,1,1,GL_RGB,GL_UNSIGNED_BYTE,pixel);
            assert(pixel[0]==expected.r&&pixel[1]==expected.g&&pixel[2]==expected.b);
        }
    }
    glBindTexture(GL_TEXTURE_2D,assets_get_texture(ASSET_GUI_WIDGETS).id);
    glGetTexParameteriv(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,&filter);assert(filter==GL_NEAREST);
    UnloadImage(atlas);assets_shutdown();puts("Button atlas halves/states at 1x/2x and nearest filtering passed");
}

static void pixel_text_test(void)
{
    const char *text="A\xd0\xaf\xf0\x9f\x98\x80";
    assert(MinecraftTextCodepoint(&text)=='A');
    assert(MinecraftTextCodepoint(&text)==0x42f);
    assert(MinecraftTextCodepoint(&text)==0x1f600);
    assert(MinecraftTextCodepoint(&text)==0);
    text="\xe2\x82"; /* Incomplete sequence has bounded reads. */
    assert(MinecraftTextCodepoint(&text)=='?');
    assert(MinecraftTextCodepoint(&text)=='?');
    assert(MinecraftTextCodepoint(&text)==0);
    assets_init(RECRAFT_TEST_ASSET_ROOT);
    language_init(); assert(language_count()>=60 && language_select("ru_RU"));
    assert(strcmp(language_text("menu.options","Options..."),"Options...")!=0);
    assert(!language_select("../escape")); language_shutdown();
    assert(MeasureMinecraftText("\xc2\xa7" "aHello\xc2\xa7" "r",16)==MeasureMinecraftText("Hello",16));
    MinecraftTextReset(); assert(MinecraftGlyph(0x42f)==0x42f);
    assert(MeasureMinecraftText("\xd0\xaf",16)>0);
    assert(MeasureMinecraftText("Hello\nx",16)==MeasureMinecraftText("Hello",16));
    assets_shutdown();
}

static void pack_upload_test(void)
{
    char root[96],path[200]; unsigned char png[300]; size_t size=test_png_fixture(png);
    Image image; Texture2D before,after; Renderer r={0}; GLubyte *pixels,*unchanged;
    const size_t atlas_bytes=(size_t)ATLAS_PIXELS*ATLAS_PIXELS*4;
    static const char *dirs[]={"resourcepacks","resourcepacks/fixture","resourcepacks/fixture/textures","resourcepacks/fixture/textures/blocks"};
    int i; FILE *file; GLint filter;
    snprintf(root,sizeof(root),"pack-gl-test-%d",(int)fixture_pid());
    assert(game_ensure_directory(root));
    for(i=0;i<4;++i) { snprintf(path,sizeof(path),"%s/%s",root,dirs[i]); assert(game_ensure_directory(path)); }
    for(i=0;i<2;++i) {
        snprintf(path,sizeof(path),"%s/resourcepacks/fixture/textures/blocks/%s.png",root,i ? "water_still" : "stone");
        file=fopen(path,"wb"); assert(file && fwrite(png,1,size,file)==size && fclose(file)==0);
    }
    assets_init(RECRAFT_TEST_ASSET_ROOT); before=assets_get_texture(ASSET_TERRAIN); assert(before.id);
    resource_pack_init(root); /* Isolate pack fixtures; bundled fallback stays in the source root. */
    assert(assets_select_pack("resourcepacks/fixture") && !glIsTexture(before.id));
    image=assets_load_image(ASSET_TERRAIN); assert(image.data && image.width==256 && image.height==256);
    assert(((Color *)image.data)[16].r==48 && ((Color *)image.data)[16].b==192);
    UnloadImage(image); assert(assets_animation_mask()==14);
    after=assets_get_texture(ASSET_TERRAIN); assert(after.id && after.id==assets_get_texture(ASSET_TERRAIN).id);
    glBindTexture(GL_TEXTURE_2D,after.id); glGetTexParameteriv(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,&filter); assert(filter==GL_NEAREST);
    assert(assets_get_texture(ASSET_FONT_ASCII).width==128); /* Partial pack fallback. */
    assert(!assets_select_pack("resourcepacks/missing") && glIsTexture(after.id));
    r.atlas=make_atlas(); r.animated_visible=1; texture_animation_init(&r.animations); assert(r.atlas);
    pixels=(GLubyte *)malloc(atlas_bytes); unchanged=(GLubyte *)malloc(atlas_bytes); assert(pixels && unchanged);
    glBindTexture(GL_TEXTURE_2D,r.atlas); glGetTexImage(GL_TEXTURE_2D,0,GL_RGBA,GL_UNSIGNED_BYTE,pixels);
    renderer_animate(&r,1); glBindTexture(GL_TEXTURE_2D,r.atlas); glGetTexImage(GL_TEXTURE_2D,0,GL_RGBA,GL_UNSIGNED_BYTE,unchanged);
    assert(!memcmp(pixels,unchanged,atlas_bytes)); /* Procedural animation must preserve custom water. */
    free(pixels); free(unchanged); glDeleteTextures(1,&r.atlas);
    assert(assets_select_pack("") && !glIsTexture(after.id) && assets_animation_mask()==15);
    assets_shutdown(); assert(glGetError()==GL_NO_ERROR);
    for(i=0;i<2;++i) { snprintf(path,sizeof(path),"%s/resourcepacks/fixture/textures/blocks/%s.png",root,i ? "water_still" : "stone"); assert(remove(path)==0); }
    for(i=3;i>=0;--i) { snprintf(path,sizeof(path),"%s/%s",root,dirs[i]); assert(fixture_rmdir(path)==0); }
    assert(fixture_rmdir(root)==0);
    puts("Resource pack: sparse tiles, GL cache disposal, nearest filtering, fallback and custom water passed");
}

static void inventory_preview_test(void)
{
    static GLubyte neutral[128*192*3],turned[128*192*3];
    GLfloat model[16],projection[16],texture[16],after[16];
    GLint viewport[4],scissor[4],binding,mode;
    GLdouble range[2],clear_depth; int i,colored=0;
    assets_init(RECRAFT_TEST_ASSET_ROOT);
    glDisable(GL_SCISSOR_TEST); glClearColor(0,0,0,1); glDepthMask(GL_TRUE);
    glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);
    glViewport(7,9,200,160); glEnable(GL_SCISSOR_TEST); glScissor(1,2,20,21);
    glDisable(GL_DEPTH_TEST); glDepthMask(GL_FALSE); glDepthRange(.2,.8); glClearDepth(.25);
    glDisable(GL_LIGHTING); glEnable(GL_BLEND); glBindTexture(GL_TEXTURE_2D,0);
    glMatrixMode(GL_MODELVIEW); glLoadIdentity(); glTranslatef(3,4,5);
    glGetFloatv(GL_MODELVIEW_MATRIX,model);
    glMatrixMode(GL_PROJECTION); glLoadIdentity(); glScalef(2,3,4);
    glGetFloatv(GL_PROJECTION_MATRIX,projection);
    glMatrixMode(GL_TEXTURE); glLoadIdentity(); glTranslatef(.125f,.25f,0);
    glGetFloatv(GL_TEXTURE_MATRIX,texture);
    player_inventory_draw(160,200,65,160,200-50*(65.0f/30),320,240);
    assert(glGetError()==GL_NO_ERROR);
    glGetFloatv(GL_MODELVIEW_MATRIX,after); equal_matrix(model,after);
    glGetFloatv(GL_PROJECTION_MATRIX,after); equal_matrix(projection,after);
    glGetFloatv(GL_TEXTURE_MATRIX,after); equal_matrix(texture,after);
    glGetIntegerv(GL_MATRIX_MODE,&mode); assert(mode==GL_TEXTURE);
    glGetIntegerv(GL_VIEWPORT,viewport);
    assert(viewport[0]==7 && viewport[1]==9 && viewport[2]==200 && viewport[3]==160);
    glGetIntegerv(GL_SCISSOR_BOX,scissor);
    assert(scissor[0]==1 && scissor[1]==2 && scissor[2]==20 && scissor[3]==21);
    assert(glIsEnabled(GL_SCISSOR_TEST) && glIsEnabled(GL_BLEND));
    assert(!glIsEnabled(GL_DEPTH_TEST) && !glIsEnabled(GL_LIGHTING));
    glGetIntegerv(GL_DEPTH_WRITEMASK,&mode); assert(!mode);
    glGetIntegerv(GL_TEXTURE_BINDING_2D,&binding); assert(binding==0);
    glGetDoublev(GL_DEPTH_RANGE,range); glGetDoublev(GL_DEPTH_CLEAR_VALUE,&clear_depth);
    assert(fabs(range[0]-.2)<1e-6 && fabs(range[1]-.8)<1e-6 && clear_depth==.25);
    glReadPixels(96,32,128,192,GL_RGB,GL_UNSIGNED_BYTE,neutral);
    for(i=0;i<(int)sizeof(neutral);i+=3) if(neutral[i] || neutral[i+1] || neutral[i+2]) ++colored;
    assert(colored>100); /* Real textured geometry, not an empty model window. */
    glDisable(GL_SCISSOR_TEST); glClear(GL_COLOR_BUFFER_BIT);
    player_inventory_draw(160,200,65,40,60,320,240);
    glReadPixels(96,32,128,192,GL_RGB,GL_UNSIGNED_BYTE,turned);
    assert(memcmp(neutral,turned,sizeof(neutral))!=0); /* Pose follows the cursor. */
    assert(glGetError()==GL_NO_ERROR);
    assets_shutdown();
    puts("Inventory biped: textured pixels, cursor pose and GL state preservation passed");
}

static void first_person_test(void)
{
    static GLubyte pixels[320*240*3],swung[320*240*3];
    static const int ids[]={0,1,278,276,261,260,69};
    FirstPersonState state={0}; InventorySlot item={1,1,0}; GLfloat texture[16],after[16];
    GLint binding,mode; GLdouble range[2]; int k,i,colored;
    assets_init(RECRAFT_TEST_ASSET_ROOT);
    for(i=0;i<4;++i) first_person_tick(&state,&item,0);
    assert(state.item.id==1 && state.equip>.9f);
    item.id=261; first_person_tick(&state,&item,1); assert(state.item.id==1 && state.equip<.7f);
    first_person_tick(&state,&item,1); first_person_tick(&state,&item,1); assert(state.item.id==261 && state.equip<.1f);
    glDisable(GL_SCISSOR_TEST); glDepthMask(GL_TRUE); glClearDepth(1);
    glMatrixMode(GL_TEXTURE); glLoadIdentity(); glTranslatef(.125f,.25f,0); glGetFloatv(GL_TEXTURE_MATRIX,texture);
    glBindTexture(GL_TEXTURE_2D,0); glDepthRange(.2,.8);
    for(k=0;k<7;++k) {
        item=(InventorySlot){ids[k],ids[k] ? 1 : 0,0}; glClearColor(0,0,0,1); glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);
        first_person_draw_pose(&item,320,240,0,0,1,0);
        glReadPixels(0,0,320,240,GL_RGB,GL_UNSIGNED_BYTE,pixels); colored=0;
        for(i=0;i<(int)sizeof(pixels);i+=3) if(pixels[i] || pixels[i+1] || pixels[i+2]) ++colored;
        assert(colored>100);
        glGetFloatv(GL_TEXTURE_MATRIX,after); equal_matrix(texture,after);
        glGetIntegerv(GL_MATRIX_MODE,&mode); assert(mode==GL_TEXTURE);
        glGetIntegerv(GL_TEXTURE_BINDING_2D,&binding); assert(binding==0);
        glGetDoublev(GL_DEPTH_RANGE,range); assert(fabs(range[0]-.2)<1e-6 && fabs(range[1]-.8)<1e-6);
        glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT); first_person_draw_pose(&item,320,240,.4f,0,1,0);
        glReadPixels(0,0,320,240,GL_RGB,GL_UNSIGNED_BYTE,swung);
        assert(memcmp(pixels,swung,sizeof(pixels))!=0 && glGetError()==GL_NO_ERROR);
    }
    assets_shutdown(); puts("First person: skin arm, block, extruded sprites, equip/swing and GL state passed");
}
static void sign_render_test(void)
{
    static GLubyte board[320*240*3],text[320*240*3];
    World w; Chunk *chunk; Renderer r={0}; ChunkMesh mesh={0};
    SignInstance sign={0,0,0,0,BETA_BLOCK_STANDING_SIGN};
    VisibleChunk visible={&mesh,0}; RendererCamera camera={.5f,.5f,2,0,0,70};
    GLfloat texture_matrix[16],after[16]; int i,colored=0,difference=0; GLuint probe; GLint binding;
    const char lines[SIGN_LINES][SIGN_LINE_BYTES]={"ReCraft","Beta 1.7.3","Hello","World"};
    assert(world_init(&w,1,1,4)==WORLD_OK); chunk=world_get_chunk(&w,0,0);
    memset(chunk->blocks,0,sizeof(chunk->blocks)); memset(chunk->sky_light,255,sizeof(chunk->sky_light));
    chunk_set_block(chunk,0,0,0,BETA_BLOCK_STANDING_SIGN);
    mesh.signs=&sign; mesh.sign_count=1; r.options.brightness=100;
    assets_init(RECRAFT_TEST_ASSET_ROOT);
    glDisable(GL_SCISSOR_TEST); glViewport(0,0,320,240); glClearColor(0,0,0,1);
    glEnable(GL_DEPTH_TEST); glDepthFunc(GL_LEQUAL); glDepthMask(GL_TRUE); glClearDepth(1);
    glEnable(GL_TEXTURE_2D); glDisable(GL_LIGHTING); glDisable(GL_FOG);
    glTexEnvi(GL_TEXTURE_ENV,GL_TEXTURE_ENV_MODE,GL_MODULATE);
    glMatrixMode(GL_PROJECTION); glLoadIdentity(); glOrtho(-.2,1.2,-.1,1.3,-2,2);
    glMatrixMode(GL_MODELVIEW); glLoadIdentity();
    glMatrixMode(GL_TEXTURE); glLoadIdentity(); glTranslatef(.125f,.25f,0);
    glGetFloatv(GL_TEXTURE_MATRIX,texture_matrix); glMatrixMode(GL_MODELVIEW);
    glGenTextures(1,&probe); glBindTexture(GL_TEXTURE_2D,probe);
    glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT); draw_signs(&r,&w,&camera,&visible,1);
    glGetIntegerv(GL_TEXTURE_BINDING_2D,&binding); assert((GLuint)binding==probe);
    glReadPixels(0,0,320,240,GL_RGB,GL_UNSIGNED_BYTE,board);
    for(i=0;i<(int)sizeof(board);i+=3) if(board[i] || board[i+1] || board[i+2]) ++colored;
    assert(colored>1000 && r.stats.quads_opaque==12); /* Real board and post pixels. */
    assert(sign_text_set(&w,0,0,0,lines));
    glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT); draw_signs(&r,&w,&camera,&visible,1);
    glReadPixels(0,0,320,240,GL_RGB,GL_UNSIGNED_BYTE,text);
    for(i=0;i<(int)sizeof(text);++i) if(text[i]!=board[i]) ++difference;
    assert(difference>100 && r.stats.quads_cutout>0); /* Four visible, depth-tested lines. */
    glGetFloatv(GL_TEXTURE_MATRIX,after); equal_matrix(texture_matrix,after);
    assert(glGetError()==GL_NO_ERROR);
    glBindTexture(GL_TEXTURE_2D,0); glDeleteTextures(1,&probe);
    assets_shutdown(); assert(world_close(&w)==WORLD_OK);
    puts("Sign rendering: original textured board/post, four text lines and texture state passed");
}

static void server_icon_test(void)
{
    unsigned char png[300];static GLubyte pixels[64*64*4];size_t n=test_png_fixture(png),i;
    Texture2D fallback,icon,other;GLint value;
    assets_init(RECRAFT_TEST_ASSET_ROOT);
    fallback=assets_get_texture(ASSET_SERVER_DEFAULT_ICON);
    assert(fallback.id && fallback.width==128 && fallback.height==128);
    assert(assets_get_server_icon(0).id==fallback.id);
    assert(assets_set_server_icon(0,png,n));icon=assets_get_server_icon(0);
    assert(icon.id && icon.id!=fallback.id && icon.width==64 && icon.height==64);
    assert(assets_get_server_icon(0).id==icon.id); /* Fetch uses the uploaded cache. */
    glBindTexture(GL_TEXTURE_2D,icon.id);
    glGetTexLevelParameteriv(GL_TEXTURE_2D,0,GL_TEXTURE_WIDTH,&value);assert(value==64);
    glGetTexLevelParameteriv(GL_TEXTURE_2D,0,GL_TEXTURE_HEIGHT,&value);assert(value==64);
    glGetTexParameteriv(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,&value);assert(value==GL_NEAREST);
    glGetTexParameteriv(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,&value);assert(value==GL_NEAREST);
    glGetTexImage(GL_TEXTURE_2D,0,GL_RGBA,GL_UNSIGNED_BYTE,pixels);
    for(i=0;i<sizeof(pixels);i+=4)
        assert(pixels[i]==48 && pixels[i+1]==96 && pixels[i+2]==192 && pixels[i+3]==255);
    glBindTexture(GL_TEXTURE_2D,0);
    png[29]^=1; /* Bad IHDR CRC must discard the old icon and use the fallback. */
    assert(!assets_set_server_icon(0,png,n));assert(!glIsTexture(icon.id));
    assert(assets_get_server_icon(0).id==fallback.id);png[29]^=1;
    assert(!assets_set_server_icon(0,png,32));assert(assets_get_server_icon(0).id==fallback.id);
    assert(!assets_set_server_icon(64,png,n));assert(assets_get_server_icon(64).id==fallback.id);
    assert(assets_set_server_icon(0,png,n) && assets_set_server_icon(63,png,n));
    icon=assets_get_server_icon(0);other=assets_get_server_icon(63);
    assert(icon.id!=other.id && glIsTexture(icon.id) && glIsTexture(other.id));
    assets_clear_server_icons();
    assert(!glIsTexture(icon.id) && !glIsTexture(other.id) && glIsTexture(fallback.id));
    assert(assets_get_server_icon(0).id==fallback.id && assets_get_server_icon(63).id==fallback.id);
    assert(glGetError()==GL_NO_ERROR);
    assets_shutdown();assert(!glIsTexture(fallback.id));
    puts("Server favicon: decoded RGBA pixels, nearest filters, cache, fallback and GL disposal passed");
}

static void assert_chest_face(const ChunkMesh *mesh,const Chunk *chunk,
                              int x,int y,int z,unsigned face,int source_tile,int clipped)
{
    const MeshLayer *layer=&mesh->layers[0];
    int axis=face<4 ? 2 : 0,tangent=axis==2 ? 0 : 2;
    int position[3]={x-chunk->x*16,y,z-chunk->z*16};
    int plane=(position[axis]+(face==3 || face==5))*128;
    unsigned tile=beta_render_tile(source_tile),i;
    int found=0,j;
    for(i=0;i<layer->vertex_count;i+=4) {
        const VoxelVertex *quad=layer->vertices+i;
        int matches=1;
        for(j=0;j<4;++j) {
            int coords[3]={quad[j].x,quad[j].y,quad[j].z};
            if(coords[axis]!=plane || coords[tangent]<position[tangent]*128 ||
               coords[tangent]>(position[tangent]+1)*128 ||
               coords[1]<(y*128+(clipped ? 64 : 0)) || coords[1]>(y+1)*128)
                matches=0;
        }
        if(!matches) continue;
        ++found;
        for(j=0;j<4;++j) {
            int coordinate=tangent==0 ? quad[j].x : quad[j].z;
            int upper=coordinate==(position[tangent]+1)*128;
            /* Independent Beta face convention: -Z/+X reverse horizontal U. */
            if(face==2 || face==5) upper=!upper;
            assert(quad[j].u==texcoord(tile,upper,0,1));
            assert(quad[j].v==partial_texcoord(tile,1.0f-(quad[j].y-y*128)/128.0f,0,1));
        }
    }
    assert(found==1);
}

static void chest_mesh_test(void)
{
    World w; Renderer policy={0}; Chunk *chunks[3];
    int axis,reversed,boundary,greedy,clipped,i,half;
    /* Both joining axes, four front directions, chunk seams and slabs. */
    assert(world_init(&w,1,1,8)==WORLD_OK);
    chunks[0]=world_get_chunk(&w,0,0); chunks[1]=world_get_chunk(&w,1,0);
    chunks[2]=world_get_chunk(&w,0,1);
    for(i=0;i<3;++i) assert(chunks[i]);
    for(axis=0;axis<=2;axis+=2) for(reversed=0;reversed<2;++reversed)
        for(boundary=0;boundary<2;++boundary) for(greedy=0;greedy<2;++greedy)
            for(clipped=0;clipped<2;++clipped) {
                int x=axis==0 && boundary ? 15 : 8,z=axis==2 && boundary ? 15 : 8;
                unsigned front=axis==0 ? (reversed ? 2u : 3u) : (reversed ? 4u : 5u);
                unsigned back=front^1u;
                for(i=0;i<3;++i) {
                    memset(chunks[i]->blocks,0,sizeof(chunks[i]->blocks));
                    memset(chunks[i]->sky_light,255,sizeof(chunks[i]->sky_light));
                }
                for(half=0;half<2;++half) {
                    int cx=x+(axis==0 ? half : 0),cz=z+(axis==2 ? half : 0);
                    Chunk *c=world_get_chunk(&w,cx/16,cz/16);
                    chunk_set_block(c,cx&15,110,cz&15,BETA_BLOCK_CHEST);
                }
                if(reversed) {
                    /* Block behind only the second half; both fronts turn. */
                    int bx=x+(axis==0 ? 1 : 0)+(axis==2 ? 1 : 0);
                    int bz=z+(axis==2 ? 1 : 0)+(axis==0 ? 1 : 0);
                    Chunk *c=world_get_chunk(&w,bx/16,bz/16);
                    chunk_set_block(c,bx&15,110,bz&15,BLOCK_STONE);
                }
                if(clipped) {
                    int sx=x+(axis==2 ? (front==5 ? 1 : -1) : 0);
                    int sz=z+(axis==0 ? (front==3 ? 1 : -1) : 0);
                    Chunk *c=world_get_chunk(&w,sx/16,sz/16);
                    chunk_set_block(c,sx&15,110,sz&15,BETA_BLOCK_SLAB);
                }
                policy.options.greedy=greedy;
                for(half=0;half<2;++half) {
                    int cx=x+(axis==0 ? half : 0),cz=z+(axis==2 ? half : 0);
                    /* Texture 41 is the left front, 42 the right front;
                     * back halves use 57/58 with the same face convention. */
                    int first_right=front==2 || front==5;
                    int front_tile=41+(first_right ? !half : half);
                    int back_tile=57+(first_right ? half : !half);
                    Chunk *c=world_get_chunk(&w,cx/16,cz/16);
                    ChunkMesh *mesh=build_chunk_mesh(&policy,&w,c);
                    assert(mesh);
                    assert(block_chest_texture(&w,cx,110,cz,front)==front_tile);
                    assert(block_chest_texture(&w,cx,110,cz,back)==back_tile);
                    assert_chest_face(mesh,c,cx,110,cz,front,front_tile,clipped && half==0);
                    if(!reversed || half==0)
                        assert_chest_face(mesh,c,cx,110,cz,back,back_tile,0);
                    chunk_mesh_destroy(mesh);
                }
            }
    assert(world_close(&w)==WORLD_OK);
    puts("Double chest: four facings, front/back UV, chunk seams, greedy/plain and slab clipping passed");
}

int main(int argc, char **argv)
{
    World world;
    Chunk *chunk;
    Renderer basic = {0};
    ChunkMesh *greedy, *plain;
    uint32_t g, p;
    int x, y, z, i;
    skin_geometry_test();
#ifdef _WIN32
    WNDCLASSA klass = {0};
    HWND window;
    HDC dc;
    HGLRC context;
    PIXELFORMATDESCRIPTOR pfd = {0};
#elif defined(__APPLE__)
    (void)argc; (void)argv;
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
    entity_pick_test();
    chest_mesh_test();
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
    assert(!visible_face(&basic,BETA_BLOCK_FLOWING_WATER, BETA_BLOCK_STILL_WATER));
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
    assert(atlas_upload_bytes()==5592404u);
    /* Fancy leaves retain crisp opaque pixels and expose interior surfaces.
     * Reduced transparency uses the original opaque foliage tiles while
     * leaving glass, water and cobweb transparency essential. */
    {
        Renderer policy={0}; ChunkMesh *fast,*fancy,*reduced;
        memset(chunk->blocks,0,sizeof(chunk->blocks));
        chunk_set_block(chunk,4,100,4,BLOCK_LEAVES);
        chunk_set_block(chunk,5,100,4,BLOCK_LEAVES);
        fast=build_chunk_mesh(&policy,&world,chunk);
        policy.options.transparent_leaves=1;
        fancy=build_chunk_mesh(&policy,&world,chunk);
        policy.options.reduced_transparency=1;
        reduced=build_chunk_mesh(&policy,&world,chunk);
        assert(fast && fancy && reduced);
        assert(fast->layers[1].vertex_count==40 && fast->layers[2].vertex_count==0);
        assert(fancy->layers[1].vertex_count==48 && fancy->layers[2].vertex_count==0);
        for(i=0;i<48;++i) assert(fancy->layers[1].vertices[i].a==255);
        assert(reduced->layers[0].vertex_count==40 && reduced->layers[1].vertex_count==0);
        assert(((face_key(&policy,BLOCK_LEAVES,0,1,1,15)>>8)&255)==99);
        assert(((face_key(&policy,BLOCK_LEAVES,1,1,1,15)>>8)&255)==100);
        assert(render_layer(&policy,BLOCK_GLASS)==BLOCK_LAYER_CUTOUT);
        assert(render_layer(&policy,BLOCK_WATER)==BLOCK_LAYER_TRANSPARENT);
        assert(render_layer(&policy,BETA_BLOCK_WEB)==BLOCK_LAYER_CUTOUT);
        assert(render_layer(&policy,BETA_BLOCK_BED)==BLOCK_LAYER_CUTOUT);
        chunk_mesh_destroy(fast); chunk_mesh_destroy(fancy); chunk_mesh_destroy(reduced);
        memset(chunk->blocks,0,sizeof(chunk->blocks));
        chunk_set_block(chunk,4,100,4,BETA_BLOCK_WEB);
        reduced=build_chunk_mesh(&policy,&world,chunk);
        assert(reduced && reduced->layers[1].vertex_count==16 && !reduced->layers[0].vertex_count);
        chunk_mesh_destroy(reduced);
        {
            ChunkMesh normal={0},opaque={0};
            policy.options.reduced_transparency=0;
            emit_decorative_plane(&policy,&normal,4,100,4,66,2,0);
            policy.options.reduced_transparency=1;
            emit_decorative_plane(&policy,&opaque,4,100,4,66,2,0);
            assert(normal.layers[2].vertex_count==8 && opaque.layers[0].vertex_count==8);
            for(i=0;i<8;++i) assert(opaque.layers[0].vertices[i].a==255);
            free(normal.layers[2].vertices); free(opaque.layers[0].vertices);
        }
    }
    /* Bed frame V uses atlas rows 7..16, not the mattress's top strip.
     * Both halves share Beta's top rotation and omit their joining face. */
    memset(chunk->blocks,0,sizeof(chunk->blocks));
    for(x=1;x<=6;++x) {
        ChunkMesh off={0},on={0}; unsigned tile=(unsigned)beta_render_tile(96);
        emit_lever(&basic,&world,&off,chunk,4,100,4,(uint8_t)x);
        emit_lever(&basic,&world,&on,chunk,4,100,4,(uint8_t)(x|8));
        assert(off.layers[0].vertex_count==48 && on.layers[0].vertex_count==48);
        assert(memcmp(off.layers[0].vertices+24,on.layers[0].vertices+24,24*sizeof(VoxelVertex))!=0);
        for(i=24;i<48;++i) {
            const VoxelVertex *v=&off.layers[0].vertices[i];
            assert(v->u>=partial_texcoord(tile,7.0f/16,0,0) && v->u<=partial_texcoord(tile,8.99f/16,0,0));
            assert(v->v>=partial_texcoord(tile,6.0f/16,0,1) && v->v<=partial_texcoord(tile,15.99f/16,0,1));
        }
        free(off.layers[0].vertices); free(on.layers[0].vertices);
    }
    for(x=0;x<4;++x) for(y=0;y<=8;y+=8) {
        ChunkMesh bed_mesh={0}; unsigned top_tile=beta_render_tile(y ? 135 : 134);
        int top_vertices=0,bottom_vertices=0,side_vertices=0;
        emit_bed(&basic,&world,&bed_mesh,chunk,4,100,4,(uint8_t)(x|y));
        assert(bed_mesh.layers[1].vertex_count==20 && !bed_mesh.layers[0].vertex_count);
        for(i=0;i<20;++i) {
            const VoxelVertex *v=&bed_mesh.layers[1].vertices[i];
            int quad=i&~3,j,is_top=1,is_bottom=1;
            for(j=0;j<4;++j) {
                int height=bed_mesh.layers[1].vertices[quad+j].y-100*128;
                if(height!=72) is_top=0;
                if(height!=24) is_bottom=0;
            }
            if(is_top) {
                float a=(float)v->x/128-4,b=(float)v->z/128-4,u,t;
                if(x==0) { u=b; t=a; }
                else if(x==1) { u=1-a; t=b; }
                else if(x==2) { u=1-b; t=1-a; }
                else { u=a; t=1-b; }
                assert(v->u==partial_texcoord(top_tile,u,0,0));
                assert(v->v==partial_texcoord(top_tile,t,0,1)); ++top_vertices;
            } else if(is_bottom) ++bottom_vertices;
            else {
                float atlas_v=v->v*(float)ATLAS_PIXELS/32767;
                float local_v=fmodf(atlas_v+.02f,ATLAS_SLOT_PIXELS);
                assert(local_v>6.95f && local_v<16.06f); ++side_vertices;
            }
        }
        assert(top_vertices==4 && bottom_vertices==4 && side_vertices==12);
        free(bed_mesh.layers[1].vertices);
    }
    for(x=0;x<16;++x) {
        ChunkMesh door_mesh={0}; BetaBlockBox box; Renderer policy={0};
        assert(beta_block_selection_box((BetaBlockState){BETA_BLOCK_WOOD_DOOR,(uint8_t)x},&box));
        assert(fabsf((box.max_x-box.min_x)*(box.max_z-box.min_z)-.1875f)<1e-6f);
        emit_door(&policy,&world,&door_mesh,chunk,4,100,4,BETA_BLOCK_WOOD_DOOR,(uint8_t)x);
        assert(door_mesh.layers[1].vertex_count==24 && !door_mesh.layers[0].vertex_count);
        for(i=0;i<24;++i) assert(door_mesh.layers[1].vertices[i].a==255);
        free(door_mesh.layers[1].vertices);
        memset(&door_mesh,0,sizeof(door_mesh)); policy.options.reduced_transparency=1;
        emit_door(&policy,&world,&door_mesh,chunk,4,100,4,BETA_BLOCK_IRON_DOOR,(uint8_t)x);
        assert(door_mesh.layers[0].vertex_count==24 && !door_mesh.layers[1].vertex_count);
        free(door_mesh.layers[0].vertices);
    }
    chunk_set_block(chunk,4,100,4,BETA_BLOCK_STANDING_SIGN); chunk_set_metadata(chunk,4,100,4,11);
    chunk_set_block(chunk,7,100,4,BETA_BLOCK_WALL_SIGN); chunk_set_metadata(chunk,7,100,4,3);
    greedy=build_chunk_mesh(&basic,&world,chunk);
    assert(greedy && greedy->sign_count==2 && !greedy->layers[0].vertex_count && !greedy->layers[1].vertex_count);
    assert(greedy->signs[0].metadata==11 && greedy->signs[1].metadata==3);
    chunk_mesh_destroy(greedy);
    /* UV phase and texel density cannot change when an edit splits a greedy
     * face. One block must span exactly 16 texels at every rectangle width. */
    for(x=1;x<=4;++x) {
        int16_t start=texcoord(4,0,0,x),end=texcoord(4,1,0,x);
        float per_block=(end-start)*(float)ATLAS_PIXELS/32767/x;
        assert(fabsf(per_block-16)<.04f);
        assert(start==texcoord(4,0,0,1));
    }
    {
        World water; Chunk *a,*b; ChunkMesh *ma=(ChunkMesh *)calloc(1,sizeof(*ma)),*mb=(ChunkMesh *)calloc(1,sizeof(*mb));
        assert(ma && mb && world_init(&water,1,1,4)==WORLD_OK);
        a=world_get_chunk(&water,0,0); b=world_get_chunk(&water,1,0);
        chunk_set_block(a,15,64,8,8); chunk_set_metadata(a,15,64,8,0);
        chunk_set_block(b,0,64,8,8); chunk_set_metadata(b,0,64,8,1);
        emit_liquid(&basic,&water,ma,a,15,64,8,8); emit_liquid(&basic,&water,mb,b,0,64,8,8);
        assert(ma->layers[2].vertex_count==16 && mb->layers[2].vertex_count==16); /* three sides + top */
        {
            const VoxelVertex *left=ma->layers[2].vertices+12,*right=mb->layers[2].vertices+12;
            assert(left[3].y==right[0].y && left[2].y==right[1].y);
            assert(left[0].y!=left[3].y); /* actual slope rather than one height per voxel */
        }
        chunk_mesh_destroy(ma); chunk_mesh_destroy(mb); assert(world_close(&water)==WORLD_OK);
    }
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
        assert(fabsf(du - 64.0f) < 0.04f && fabsf(dv - 64.0f) < 0.04f);
        for (x = 0; x < 4; ++x) {
            assert(v[x].x >= 0 && v[x].x <= 16 * VERTEX_COORD_SCALE);
            assert(v[x].y >= 0 && v[x].y <= 16 * VERTEX_COORD_SCALE);
            assert(v[x].z >= 0 && v[x].z <= 16 * VERTEX_COORD_SCALE);
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
        assert(greedy->layers[1].vertices[i].y>=16*128 &&
               greedy->layers[1].vertices[i].y<=16*128+102);
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
                if(vertices[j].x!=5*128) on_x5=0;
            }
            if(on_x5 && min_y==16*128+64 && max_y==17*128) ++upper_neighbor_faces;
            if(min_y==16*128+64 && max_y==min_y) ++slab_top_faces;
        }
        assert(upper_neighbor_faces==1 && slab_top_faces==1);
    }
    chunk_mesh_destroy(greedy);
    {
        static const uint8_t orientations[3]={1,2,5};
        static const int min_x_bounds[3]={514,603,568};
        static const int max_x_bounds[3]={549,638,584};
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
            assert(min_y==(orientations[orientation]==5 ? 2048 : 2074));
            assert(max_y==(orientations[orientation]==5 ? 2125 : 2150));
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
    window = CreateWindowA(klass.lpszClassName, "Hidden renderer check", WS_POPUP,
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
#elif defined(__APPLE__)
    if(!test_cgl_context(320,240)) return EXIT_FAILURE;
    fprintf(stderr,"renderer_test: vendor=%s renderer=%s version=%s\n",
        glGetString(GL_VENDOR),glGetString(GL_RENDERER),glGetString(GL_VERSION));
#else
    fprintf(stderr,"renderer_test: creating hidden OpenGL context\n");
    glfwSetErrorCallback(context_error);
    if(!glfwInit()) return EXIT_FAILURE;
    fprintf(stderr,"renderer_test: GLFW initialized\n");
    glfwWindowHint(GLFW_VISIBLE, 0); /* GLFW 3.1 predates GLFW_FALSE. */
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 1);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 1);
    window = glfwCreateWindow(320, 240, "Hidden renderer check", NULL, NULL);
    if(!window) {
        glfwTerminate();
        return EXIT_FAILURE;
    }
    glfwMakeContextCurrent(window);
    fprintf(stderr,"renderer_test: OpenGL context ready\n");
    fprintf(stderr,"renderer_test: vendor=%s renderer=%s version=%s\n",
        glGetString(GL_VENDOR),glGetString(GL_RENDERER),glGetString(GL_VERSION));
#endif
    renderer = renderer_init();
    assert(renderer);
    assert(renderer_stats(renderer).vbo_budget_bytes == 16u * 1024u * 1024u);
    assert(renderer_stats(renderer).gart_report_valid == 0);
#ifdef __APPLE__
    {
        GLint gpu_vertices=0;
        int expected=-1;
        if(CGLGetParameter(CGLGetCurrentContext(),kCGLCPGPUVertexProcessing,
                           &gpu_vertices)==kCGLNoError)
            expected=gpu_vertices?1:0;
        assert(renderer_stats(renderer).gpu_vertex_processing==expected);
    }
#else
    assert(renderer_stats(renderer).gpu_vertex_processing == -1);
#endif
    fprintf(stderr,"renderer_test: renderer ready\n");
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
    pixel_text_test();
    pack_upload_test();
    button_texture_test();
    skin_upload_test();
    inventory_preview_test();
    first_person_test();
    sign_render_test();
    server_icon_test();
    menu_render_test(argc>2 && strcmp(argv[1],"--menu-capture")==0 ? argv[2] : NULL);
#ifdef _WIN32
    wglMakeCurrent(NULL, NULL); wglDeleteContext(context);
    ReleaseDC(window, dc); DestroyWindow(window);
    UnregisterClassA(klass.lpszClassName, klass.hInstance);
#elif defined(__APPLE__)
    test_cgl_close();
#else
    glfwDestroyWindow(window);
    glfwTerminate();
#endif
    fprintf(stderr,"renderer_test: passed\n");
    return 0;
}
