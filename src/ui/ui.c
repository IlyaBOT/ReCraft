#include "ui.h"
#include "../game/crafting.h"
#include "../game/settings.h"
#include "../config.h"
#include "raylib.h"
#include "pixel_font.h"
#include "gui_button.h"
#include "language.h"
#include "../assets/resource_pack.h"
#include "../assets/assets.h"
#include "../world/beta_blocks.h"
#include "../game/creative.h"
#include "../game/entity_render.h"
#include "../util/display.h"
#include "../util/build_platform.h"
#include "../network/server_status.h"

#if defined(__APPLE__)
#include <OpenGL/gl.h>
#else
#include <GL/gl.h>
#endif

#include <ctype.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

/* All layout coordinates refer to the same 640 x 480 canvas. Raylib's 2D
   functions are used only as a thin platform layer; no raygui or render target. */
#define VW 640
#define VH 480

typedef struct UiLayout {
    float scale;
    int x;
    int y;
    float mouse_x;
    float mouse_y;
    int clicked;
    int world_background;
} UiLayout;

static UiLayout layout;
static ResourcePackEntry pack_entries[128];
static int pack_count;

static Color col(unsigned char r, unsigned char g, unsigned char b, unsigned char a)
{
    Color c = { r, g, b, a };
    return c;
}

static int px(float v) { return layout.x + (int)(v*layout.scale + 0.5f); }
static int py(float v) { return layout.y + (int)(v*layout.scale + 0.5f); }
static int ps(float v) { int n = (int)(v*layout.scale + 0.5f); return n > 0 ? n : 1; }

static void rect(int x, int y, int w, int h, Color c)
{
    DrawRectangle(px((float)x), py((float)y), ps((float)w), ps((float)h), c);
}

static void linebox(int x, int y, int w, int h, Color c)
{
    DrawRectangleLines(px((float)x), py((float)y), ps((float)w), ps((float)h), c);
}

static void label(const char *s, int x, int y, int size, Color c)
{
    DrawMinecraftText(s, px((float)x), py((float)y), ps((float)size), c, 1);
}

static void label_fit(const char *s, int x, int y, int size, int width, Color c)
{
    char text[256];
    const char *p, *end;
    size_t n = 0;
    int clipped;
    if (!s) return;
    /* Keep complete UTF-8 characters, including when the buffer fills. */
    p = s;
    while (*p) {
        end = p;
        MinecraftTextCodepoint(&end);
        if ((size_t)(end-s) > sizeof(text)-4) break;
        n = (size_t)(end-s);
        p = end;
    }
    memcpy(text, s, n); text[n] = '\0';
    clipped = *p || MeasureMinecraftText(text, ps((float)size)) > ps((float)width);
    if (clipped) {
        for (;;) {
            memcpy(text+n, "...", 4);
            if (MeasureMinecraftText(text, ps((float)size)) <= ps((float)width)) break;
            if (!n) { text[0] = '\0'; break; }
            do { --n; } while (n && ((unsigned char)text[n]&0xc0)==0x80);
            /* A trailing section sign must not consume an ellipsis dot. */
            if (n>=2 && (unsigned char)text[n-2]==0xc2 && (unsigned char)text[n-1]==0xa7) n-=2;
        }
    }
    label(text, x, y, size, c);
}

static void centered(const char *s, int cx, int y, int size, Color c)
{
    int font_size = ps((float)size);
    DrawMinecraftTextCentered(s, px((float)cx), py((float)y), font_size, c, 1);
}

static int inside(int x, int y, int w, int h)
{
    return layout.mouse_x >= x && layout.mouse_x < x + w &&
           layout.mouse_y >= y && layout.mouse_y < y + h;
}

static int button(Ui *ui, int x, int y, int w, int h, const char *caption, int enabled)
{
    int hot = enabled && inside(x, y, w, h);
    int pressed = hot && IsMouseButtonDown(MOUSE_LEFT_BUTTON);
    gui_button_draw(px((float)x),py((float)y),ps((float)w),ps((float)h),language_caption(caption),
        !enabled ? GUI_BUTTON_DISABLED : pressed ? GUI_BUTTON_PRESSED :
        hot ? GUI_BUTTON_HOVERED : GUI_BUTTON_NORMAL);
    if (hot && layout.clicked) {
        ui->click_sound = 1;
        return 1;
    }
    return 0;
}

static void text_field_enabled(Ui *ui, int id, int x, int y, int w, int h,
                       char *value, size_t capacity, const char *hint,int enabled)
{
    int active;
    const char *visible = value;
    if(!enabled && ui->focus==id) ui->focus=0;
    if (enabled && layout.clicked && inside(x, y, w, h)) { ui->focus = id; ui->select_all = 0; }
    active = enabled && ui->focus == id;
    rect(x, y, w, h, col(14, 14, 16, 255));
    linebox(x, y, w, h, active ? col(202, 205, 207, 255) : col(107, 108, 109, 255));
    while (*visible && MeasureMinecraftText(visible, ps(11)) > ps((float)(w-16))) ++visible;
    if (active && ui->select_all && value[0])
        rect(x+4, y+3, w-8, h-6, col(57, 76, 113, 255));
    if (value[0]) label(visible, x+6, y+5, 11, enabled ? col(245,245,241,255) : col(150,150,150,255));
    else label(hint, x+6, y+5, 11, col(111, 112, 113, 255));
    if (active) {
        int tw = MeasureMinecraftText(visible, ps(11));
        int cursor_x = px((float)(x+6)) + tw + ps(1);
        DrawRectangle(cursor_x, py((float)(y+5)), ps(1), ps(12), col(255, 255, 255, 255));
    }
    (void)capacity;
}
static void text_field(Ui *ui,int id,int x,int y,int w,int h,char *value,size_t capacity,const char *hint)
{ text_field_enabled(ui,id,x,y,w,h,value,capacity,hint,1); }

static void edit_text(Ui *ui, int id, char *value, size_t capacity)
{
    int k;
    size_t length;
    if (ui->focus != id || capacity < 2) return;
    length = strlen(value);
    k = GetKeyPressed();
    if ((IsKeyDown(KEY_LEFT_CONTROL) || IsKeyDown(KEY_RIGHT_CONTROL)) && IsKeyPressed(KEY_A)) {
        ui->select_all = 1;
        return;
    }
    if (IsKeyPressed(KEY_BACKSPACE) || k == 3) {
        if (ui->select_all) value[0] = '\0';
        else if (length > 0) value[length-1] = '\0';
        ui->select_all = 0;
        return;
    }
    if (IsKeyDown(KEY_LEFT_CONTROL) || IsKeyDown(KEY_RIGHT_CONTROL)) return;
    if (ui->select_all && k >= 32 && k <= 126) { length = 0; ui->select_all = 0; }
    if (k >= 32 && k <= 126 && length + 1 < capacity) {
        value[length] = (char)k;
        value[length+1] = '\0';
    }
}

static void copy_text(char *dst, size_t n, const char *src)
{
    if (!dst || n == 0) return;
    if (!src) src = "";
    strncpy(dst, src, n-1);
    dst[n-1] = '\0';
}

static void begin_layout(const Ui *ui)
{
    int width = recraft_screen_width();
    int height = recraft_screen_height();
    Vector2 mouse = GetMousePosition();
    float sx = (float)width / VW;
    float sy = (float)height / VH;
    layout.scale = sx < sy ? sx : sy;
    /* Half-step scaling keeps layout geometry on a stable pixel grid. The
       requested GUI size is clamped to fit even a 640 x 480 window. */
    if (layout.scale >= 1.0f) layout.scale = floorf(layout.scale*2.0f)*0.5f;
    if (ui && ui->options.gui_scale > 0) {
        float requested = 0.5f + 0.5f*ui->options.gui_scale;
        if (requested < layout.scale) layout.scale = requested;
    }
    if (layout.scale <= 0.0f) layout.scale = 1.0f;
    layout.x = (width - (int)(VW*layout.scale))/2;
    layout.y = (height - (int)(VH*layout.scale))/2;
    layout.mouse_x = (mouse.x - layout.x)/layout.scale;
    layout.mouse_y = (mouse.y - layout.y)/layout.scale;
    layout.clicked = IsMouseButtonPressed(MOUSE_LEFT_BUTTON);
    layout.world_background = ui && ui->world_background;
}
int ui_draw_sleep(Ui *ui,int ticks)
{
    begin_layout(ui);
    if(ticks>100) ticks=100;
    rect(0,0,VW,VH,col(8,10,35,(unsigned char)(ticks*180/100)));
    return button(ui,220,400,200,24,"Leave Bed",1);
}

/* One tiled Beta dirt texture or one static panorama; no FBO or shader pass. */
static void draw_background(int dirt)
{
    if (layout.world_background) {
        DrawRectangle(0,0,recraft_screen_width(),recraft_screen_height(),col(0,0,0,120));
        return;
    }
    Texture2D texture = assets_get_texture(dirt ? ASSET_GUI_BACKGROUND : ASSET_GUI_PANORAMA);
    Rectangle src = dirt ? (Rectangle){0,0,640,480} :
                  (Rectangle){0,64,(float)texture.width,384};
    Rectangle dst = {0,0,(float)recraft_screen_width(),(float)recraft_screen_height()};
    Vector2 origin = {0,0};
    Color veil = dirt ? col(39,30,23,125) : col(17,22,24,115);
    ClearBackground(col(24,27,28,255));
    DrawTexturePro(texture,src,dst,origin,0,col(255,255,255,255));
    DrawRectangle(0,0,recraft_screen_width(),recraft_screen_height(),veil);
}

static void panel(int x, int y, int w, int h)
{
    rect(x, y, w, h, col(17, 17, 18, 184));
    linebox(x, y, w, h, col(122, 119, 111, 160));
}

static void title(const char *s)
{
    s=language_caption(s);
    centered(s, 321, 25, 16, col(14, 14, 14, 255));
    centered(s, 320, 24, 16, col(252, 252, 246, 255));
}

static void footer(void)
{
    label(RECRAFT_TITLE " " RECRAFT_VERSION, 8, 460, 9,
          col(220, 220, 214, 220));
}

static int contains_case(const char *haystack, const char *needle)
{
    const unsigned char *a, *b;
    if (!needle || !needle[0]) return 1;
    if (!haystack) return 0;
    for (a = (const unsigned char *)haystack; *a; ++a) {
        const unsigned char *p = a;
        b = (const unsigned char *)needle;
        while (*p && *b && tolower(*p) == tolower(*b)) { ++p; ++b; }
        if (!*b) return 1;
    }
    return 0;
}

static UiAction empty_action(void)
{
    UiAction a;
    memset(&a, 0, sizeof(a));
    a.index = -1;
    return a;
}

static void fill_world_action(UiAction *action, UiActionType type,
                              const UiWorldEntry *worlds, int count, int index)
{
    if (index < 0 || index >= count || !worlds) return;
    action->type = type;
    action->index = index;
    copy_text(action->world_id, sizeof(action->world_id), worlds[index].id);
    copy_text(action->world_name, sizeof(action->world_name), worlds[index].name);
    action->creative = worlds[index].creative;
    action->flat = worlds[index].flat;
    snprintf(action->seed, sizeof(action->seed), "%llu", (unsigned long long)worlds[index].seed);
}

static void fill_server_action(UiAction *action, UiActionType type,
                               const UiServerEntry *servers, int count, int index)
{
    if (index < 0 || index >= count || !servers) return;
    action->type = type;
    action->index = index;
    copy_text(action->server_name, sizeof(action->server_name), servers[index].name);
    copy_text(action->server_address, sizeof(action->server_address), servers[index].address);
    action->hide_address = servers[index].hide_address;
    action->server_protocol = servers[index].protocol;
}

void ui_init(Ui *ui)
{
    if (!ui) return;
    memset(ui, 0, sizeof(*ui));
    ui->options.difficulty=2;
    ui->options.sensitivity=100; ui->options.fov=70; ui->options.chat_visible=1;
    copy_text(ui->options.language,sizeof(ui->options.language),"en_US");
    copy_text(ui->options.player_name,sizeof(ui->options.player_name),"Player");
    ui->options.sound_volume=ui->options.music_volume=100;
    ui->screen = UI_SCREEN_MAIN;
    ui->previous_screen = UI_SCREEN_MAIN;
    ui->options_parent = UI_SCREEN_MAIN;
    ui->selected_world = -1;
    ui->selected_server = -1;
    ui->editing_server = -1;
    ui->server_protocol = 14;
    ui->create_structures = 1;
    ui->options.render_distance = 4;
    ui->options.max_framerate = 0;
    ui->options.vsync = 1;
    ui->monitor_hz = 60;
    ui->options.view_bobbing = 1;
    ui->options.brightness = 50;
    ui->options.gui_scale = 0;
    ui->options.greedy_mesh = 1;
    ui->options.vbo_budget_mb = 16;
    ui->options.chunk_build_budget = 2;
    ui->options.dynamic_updates = 2;
    ui->mipmap_available = 1;
    ui->vbo_available = 1;
}

void ui_shutdown(void)
{
    /* Shared textures are released by assets_shutdown(). */
}

void ui_set_screen(Ui *ui, UiScreen screen)
{
    if (!ui) return;
    ui->previous_screen = ui->screen;
    ui->screen = screen;
    if(screen==UI_SCREEN_OPTIONS || screen==UI_SCREEN_PROFILE) copy_text(ui->player_name_input,sizeof(ui->player_name_input),ui->options.player_name);
    if(screen==UI_SCREEN_PACKS) pack_count=resource_pack_list(pack_entries,128);
    ui->slider_drag=0;
    ui->focus = 0;
    ui->select_all = 0;
}

static UiAction main_menu(Ui *ui)
{
    UiAction action = empty_action();
    int i; Texture2D globe=assets_get_texture(ASSET_GUI_LANGUAGE);
    int width=recraft_screen_width(),height=recraft_screen_height(),small=ps(7),normal=ps(9),margin=ps(4);
    const char *github="GitHub: https://github.com/IlyaBOT/ReCraft";
    int link_width=MeasureMinecraftText(github,normal),link_y=height-margin-normal*3-small-ps(6);
    draw_background(0);
    /* Original ReCraft wordmark: pixel geometry with an extruded dark edge. */
    for(i=8;i>=0;--i) centered(RECRAFT_TITLE,320-i/2,54+i,58,col(20,20,20,255));
    centered(RECRAFT_TITLE,320,51,58,col(192,190,187,255));
    player_inventory_draw(px(68),py(329),ps(63),px(layout.mouse_x),py(layout.mouse_y),width,height);
    if(button(ui,8,342,120,24,ui->options.player_name,1)) ui_set_screen(ui,UI_SCREEN_PROFILE);
    if (button(ui, 170, 198, 300, 30, "Singleplayer", 1)) ui_set_screen(ui, UI_SCREEN_WORLDS);
    if (button(ui, 170, 234, 300, 30, "Multiplayer", 1)) ui_set_screen(ui, UI_SCREEN_MULTIPLAYER);
    if (button(ui, 170, 320, 145, 30, "Options...", 1)) {
        ui->options_parent = UI_SCREEN_MAIN; ui_set_screen(ui, UI_SCREEN_OPTIONS);
    }
    if (button(ui, 325, 320, 145, 30, "Quit Game", 1)) {
        ui->pending_confirm = 3; ui_set_screen(ui, UI_SCREEN_CONFIRM);
    }
    {
        Rectangle src={0,inside(133,320,30,30) ? 126 : 106,20,20};
        Rectangle dst={(float)px(133),(float)py(320),(float)ps(30),(float)ps(30)}; Vector2 origin={0,0};
        DrawTexturePro(globe,src,dst,origin,0,WHITE);
        if(inside(133,320,30,30) && layout.clicked) { ui->click_sound=1; ui->language_parent=UI_SCREEN_MAIN; ui_set_screen(ui,UI_SCREEN_LANGUAGES); }
    }
    DrawMinecraftText(RECRAFT_TITLE " " RECRAFT_VERSION,margin,height-margin-small-normal-ps(3),normal,WHITE,1);
    DrawMinecraftText("Build " RECRAFT_BUILD_REVISION " / " RECRAFT_BUILD_DATE " / " RECRAFT_BUILD_PLATFORM,
        margin,height-margin-small,small,col(220,220,220,255),1);
    DrawMinecraftText(github,width-margin-link_width,link_y,normal,WHITE,1);
    {
        const char *fan="Free fan parody. Original Minecraft assets belong to Mojang.";
        const char *aff="Not affiliated with Mojang or Microsoft.";
        DrawMinecraftText(fan,width-margin-MeasureMinecraftText(fan,small),height-margin-normal-small-ps(3),small,col(230,230,230,255),1);
        DrawMinecraftText(aff,width-margin-MeasureMinecraftText(aff,normal),height-margin-normal,normal,col(205,205,205,255),1);
    }
    if(IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
        Vector2 mouse=GetMousePosition();
        if(mouse.x>=width-margin-link_width && mouse.y>=link_y && mouse.y<link_y+normal) {
            ui->click_sound=1; action.type=UI_ACTION_OPEN_GITHUB;
        }
    }
    return action;
}

static void last_played_label(uint64_t timestamp, char *out, size_t size)
{
    time_t stamp;
    struct tm *local;
    if (!timestamp) { copy_text(out, size, "Never played"); return; }
    stamp = (time_t)timestamp;
    local = localtime(&stamp);
    if (!local || strftime(out, size, "Last played: %Y-%m-%d %H:%M", local) == 0)
        copy_text(out, size, "Last played: unknown");
}

static void world_icon(int x, int y, int flat)
{
    Texture2D terrain = assets_get_texture(ASSET_TERRAIN);
    Rectangle src = {flat ? 0.0f : 48.0f,0,16,16};
    Rectangle dst = {(float)px(x),(float)py(y),(float)ps(48),(float)ps(48)};
    Vector2 origin = {0,0};
    DrawTexturePro(terrain,src,dst,origin,0,WHITE);
    linebox(x, y, 48, 48, col(12, 12, 12, 255));
}

static UiAction world_list(Ui *ui, const UiWorldEntry *worlds, int count)
{
    UiAction action = empty_action();
    int filtered[UI_MAX_WORLDS];
    int n = 0, i, pos, row;
    int has_selection;
    char date[64];
    draw_background(1);
    title("Select World");
    panel(54, 55, 532, 299);
    label("Search:", 67, 66, 11, col(238, 238, 234, 255));
    text_field(ui, 1, 125, 60, 448, 24, ui->search, sizeof(ui->search), "Filter by world name");
    edit_text(ui, 1, ui->search, sizeof(ui->search));
    if (count < 0) count = 0;
    if (count > UI_MAX_WORLDS) count = UI_MAX_WORLDS;
    for (i = 0; i < count; ++i) {
        if (worlds && (contains_case(worlds[i].name, ui->search) ||
                       contains_case(worlds[i].id, ui->search))) filtered[n++] = i;
    }
    /* Keep a valid selection when filtering; either save format can be played. */
    has_selection = 0;
    for (pos = 0; pos < n; ++pos)
        if (filtered[pos] == ui->selected_world) has_selection = 1;
    if (!has_selection) ui->selected_world = n ? filtered[0] : -1;
    if (ui->world_scroll > n-4) ui->world_scroll = n > 4 ? n-4 : 0;
    if (ui->world_scroll < 0) ui->world_scroll = 0;
    if (inside(60, 90, 515, 257)) {
        ui->world_scroll -= GetMouseWheelMove();
        if (ui->world_scroll < 0) ui->world_scroll = 0;
        if (ui->world_scroll > n-4) ui->world_scroll = n > 4 ? n-4 : 0;
    }
    rect(62, 90, 513, 256, col(7, 8, 9, 173));
    if (n == 0) centered(ui->search[0] ? "No matching worlds" : "No worlds yet", 318,
                         203, 13, col(198, 198, 190, 255));
    for (row = 0; row < 4 && ui->world_scroll + row < n; ++row) {
        int index = filtered[ui->world_scroll + row];
        int y = 94 + row*61;
        char detail[95];
        if (layout.clicked && inside(65, y, 505, 58)) ui->selected_world = index;
        rect(65, y, 505, 58, ui->selected_world == index ?
             col(108, 110, 113, 229) : col(47, 47, 49, 199));
        if (ui->selected_world == index) linebox(65, y, 505, 58, col(211, 211, 205, 255));
        world_icon(70, y+5, worlds[index].flat);
        label_fit(worlds[index].name, 126, y+4, 13, 434, col(250, 250, 247, 255));
        last_played_label(worlds[index].last_played, date, sizeof(date));
        label(date, 126, y+23, 10, col(178, 178, 174, 255));
        if (worlds[index].beta_format)
            snprintf(detail,sizeof(detail),"Beta 1.7.3  |  spawn %d, %d, %d  |  %u regions",
                worlds[index].spawn_x,worlds[index].spawn_y,worlds[index].spawn_z,
                worlds[index].region_files);
        else snprintf(detail, sizeof(detail), "%s  |  %s  |  ReCraft world",
                 worlds[index].creative ? "Creative" : "Survival",
                 worlds[index].flat ? "Flat" : "Default");
        label(detail, 126, y+38, 9, col(198, 198, 193, 255));
    }
    if (n > 4) {
        rect(576, 90, 6, 256, col(18, 18, 18, 255));
        rect(576, 90 + ui->world_scroll*256/n, 6,
             256*4/n > 14 ? 256*4/n : 14, col(173, 173, 170, 255));
    }
    has_selection = 0;
    for (pos = 0; pos < n; ++pos)
        if (filtered[pos] == ui->selected_world) has_selection = 1;
    if (!has_selection) ui->selected_world = -1;
    if (button(ui, 67, 366, 247, 27,"Play Selected World",has_selection))
        fill_world_action(&action, UI_ACTION_PLAY_WORLD, worlds, count, ui->selected_world);
    if (button(ui, 327, 366, 247, 27, "Create New World", 1)) {
        ui->world_name[0] = ui->world_seed[0] = '\0';
        ui->create_creative = ui->create_flat = ui->create_more_options = 0;
        ui->create_structures = 1;
        ui_set_screen(ui, UI_SCREEN_CREATE_WORLD);
    }
    if (button(ui, 67, 400, 117, 25, "Edit",
               has_selection && !worlds[ui->selected_world].beta_format)) {
        copy_text(ui->world_name, sizeof(ui->world_name), worlds[ui->selected_world].name);
        ui_set_screen(ui, UI_SCREEN_WORLD_EDIT);
    }
    if (button(ui, 192, 400, 117, 25, "Delete",
               has_selection && !worlds[ui->selected_world].beta_format)) {
        ui->pending_confirm = 1; ui_set_screen(ui, UI_SCREEN_CONFIRM);
    }
    if (button(ui, 317, 400, 117, 25, "Re-Create",
               has_selection && !worlds[ui->selected_world].beta_format))
        fill_world_action(&action, UI_ACTION_RECREATE_WORLD, worlds, count, ui->selected_world);
    if (button(ui, 442, 400, 132, 25, "Cancel", 1)) ui_set_screen(ui, UI_SCREEN_MAIN);
    if (ui->status[0]) centered(ui->status, 320, 438, 10, col(255, 206, 162, 255));
    if (IsKeyPressed(KEY_UP) || IsKeyPressed(KEY_DOWN)) {
        int direction = IsKeyPressed(KEY_UP) ? -1 : 1;
        for (pos = 0; pos < n; ++pos) if (filtered[pos] == ui->selected_world) break;
        if (pos == n) pos = direction > 0 ? -1 : n;
        pos += direction;
        if (pos >= 0 && pos < n) {
            ui->selected_world = filtered[pos];
            if (pos < ui->world_scroll) ui->world_scroll = pos;
            if (pos >= ui->world_scroll+4) ui->world_scroll = pos-3;
        }
    }
    if (ui->screen == UI_SCREEN_WORLDS && IsKeyPressed(KEY_ENTER) && has_selection)
        fill_world_action(&action, UI_ACTION_PLAY_WORLD, worlds, count, ui->selected_world);
    return action;
}

static void make_world_id(char *out, size_t size, const char *name)
{
    size_t n = 0, i;
    if (!size) return;
    for (i = 0; name[i] && n+1 < size; ++i) {
        unsigned char c = (unsigned char)name[i];
        if (isalnum(c)) out[n++] = (char)tolower(c);
        else if ((c == ' ' || c == '-' || c == '_') && n > 0 && out[n-1] != '_')
            out[n++] = '_';
    }
    while (n > 0 && out[n-1] == '_') --n;
    if (n == 0) copy_text(out, size, "new_world");
    else out[n] = '\0';
}

static UiAction create_world(Ui *ui)
{
    UiAction action = empty_action();
    draw_background(1);
    title("Create New World");
    panel(115, 63, 410, 357);
    label("World Name", 150, 84, 11, col(239, 239, 236, 255));
    text_field(ui, 1, 150, 102, 340, 27, ui->world_name,
               sizeof(ui->world_name), "New World");
    edit_text(ui, 1, ui->world_name, sizeof(ui->world_name));
    label("Will be saved in: saves/<world id>", 150, 136, 10,
          col(181, 181, 178, 255));
    if (button(ui, 150, 166, 340, 28,
               ui->create_creative ? "Game Mode: Creative" : "Game Mode: Survival", 1))
        ui->create_creative = !ui->create_creative;
    centered(ui->create_creative ? "Unlimited blocks and free flight" :
             "Gather blocks and explore", 320, 202, 10, col(220, 220, 214, 255));
    if (button(ui, 150, 232, 340, 27,
               ui->create_more_options ? "Less World Options..." : "More World Options...", 1))
        ui->create_more_options = !ui->create_more_options;
    if (ui->create_more_options) {
        label("Seed (blank = random)", 150, 269, 10, col(239, 239, 236, 255));
        text_field(ui, 2, 150, 284, 340, 25, ui->world_seed,
                   sizeof(ui->world_seed), "Optional seed");
        edit_text(ui, 2, ui->world_seed, sizeof(ui->world_seed));
        if (button(ui, 150, 319, 165, 25,
                   ui->create_structures ? "Structures: ON" : "Structures: OFF", 1))
            ui->create_structures = !ui->create_structures;
        if (button(ui, 325, 319, 165, 25,
                   ui->create_flat ? "World Type: Flat" : "World Type: Default", 1))
            ui->create_flat = !ui->create_flat;
    }
    if (button(ui, 150, 374, 165, 29, "Create New World", 1) ||
        (IsKeyPressed(KEY_ENTER) && ui->focus == 1)) {
        action.type = UI_ACTION_CREATE_WORLD;
        copy_text(action.world_name, sizeof(action.world_name),
                  ui->world_name[0] ? ui->world_name : "New World");
        make_world_id(action.world_id, sizeof(action.world_id), action.world_name);
        copy_text(action.seed, sizeof(action.seed), ui->world_seed);
        action.creative = ui->create_creative;
        action.flat = ui->create_flat;
        action.structures = ui->create_structures;
    }
    if (button(ui, 325, 374, 165, 29, "Cancel", 1)) ui_set_screen(ui, UI_SCREEN_WORLDS);
    if (ui->status[0]) centered(ui->status, 320, 428, 10, col(255, 206, 162, 255));
    return action;
}

static UiAction edit_world(Ui *ui, const UiWorldEntry *worlds, int count)
{
    UiAction action = empty_action();
    int valid = worlds && ui->selected_world >= 0 && ui->selected_world < count;
    draw_background(1);
    title("Edit World");
    panel(115, 130, 410, 220);
    label("World Name", 150, 157, 11, col(239, 239, 236, 255));
    text_field(ui, 1, 150, 179, 340, 27, ui->world_name,
               sizeof(ui->world_name), "World Name");
    edit_text(ui, 1, ui->world_name, sizeof(ui->world_name));
    centered("Only the display name changes; world data stays in place.",
             320, 222, 10, col(204, 204, 200, 255));
    if (button(ui, 150, 291, 165, 28, "Save", valid && ui->world_name[0])) {
        fill_world_action(&action, UI_ACTION_EDIT_WORLD, worlds, count, ui->selected_world);
        copy_text(action.world_name, sizeof(action.world_name), ui->world_name);
        ui_set_screen(ui, UI_SCREEN_WORLDS);
    }
    if (button(ui, 325, 291, 165, 28, "Cancel", 1)) ui_set_screen(ui, UI_SCREEN_WORLDS);
    return action;
}

static void server_icon(int x, int y, int index)
{
    Texture2D icon=assets_get_server_icon(index);
    Rectangle source={0,0,(float)icon.width,(float)icon.height};
    Rectangle destination={(float)px(x),(float)py(y),(float)ps(45),(float)ps(45)};
    DrawTexturePro(icon,source,destination,(Vector2){0,0},0,WHITE);
}

static void ping_icon(int x,int y,int ping,int failed)
{
    int i,bars=server_status_ping_bars(ping);
    for(i=0;i<5;++i) rect(x+i*4,y+12-i*3,3,3+i*3,
        i<bars ? col(102,196,91,255) : col(79,81,78,255));
    if(ping<0) label(failed ? "x" : "?",x+7,y+15,9,
        failed ? col(244,134,124,255) : col(212,210,180,255));
}

static int chat_wrap(const char *text,char output[6][512],int width,int height);

static UiAction multiplayer(Ui *ui, const UiServerEntry *servers, int count)
{
    UiAction action = empty_action();
    int row, valid, can_join;
    draw_background(1);
    title("Play Multiplayer");
    if (!servers || count < 0) count = 0;
    if (count > UI_MAX_SERVERS) count = UI_MAX_SERVERS;
    if (ui->selected_server >= count) ui->selected_server = -1;
    if (inside(58, 66, 524, 280)) ui->server_scroll -= GetMouseWheelMove();
    if (ui->server_scroll > count-4) ui->server_scroll = count > 4 ? count-4 : 0;
    if (ui->server_scroll < 0) ui->server_scroll = 0;
    panel(55, 62, 530, 288);
    if (!count) centered("No servers saved. Add a Beta 1.7.3 server.", 320, 196,
                         12, col(204, 204, 196, 255));
    for (row = 0; row < 4 && ui->server_scroll+row < count; ++row) {
        int index = ui->server_scroll+row, y = 69+row*68;
        const UiServerEntry *server = &servers[index];
        char info[64];
        char motd[6][512];
        int line,lines;
        if (layout.clicked && inside(63, y, 506, 63)) {
            ui->selected_server = index;
            ui->focus = 0;
        }
        rect(63, y, 506, 63, ui->selected_server == index ?
             col(103, 106, 111, 230) : col(42, 43, 45, 215));
        if (ui->selected_server == index) linebox(63, y, 506, 63, col(213, 214, 207, 255));
        server_icon(70, y+8, index);
        label_fit(server->name, 124, y+5, 13, 304, col(247, 247, 240, 255));
        lines=chat_wrap(server->motd[0] ? server->motd : "Status unknown",motd,ps(343),ps(9));
        for(line=0;line<lines && line<2;++line)
            label(motd[line],124,y+22+line*11,9,col(197,198,190,255));
        label_fit(server->hide_address ? "Address hidden" : server->address,
                  124, y+46, 9, 272, col(166, 167, 161, 255));
        if (server->max_players >= 0) snprintf(info, sizeof(info), "%d/%d", server->players, server->max_players);
        else snprintf(info, sizeof(info), "?/?");
        label(info, 452, y+6, 10, col(211, 212, 206, 255));
        ping_icon(531,y+9,server->ping_ms,server->query_state==SERVER_STATUS_ERROR);
        if(server->ping_ms>=0) snprintf(info,sizeof(info),"%d ms",server->ping_ms);
        else if(server->connect_ms>=0) snprintf(info,sizeof(info),"TCP %d ms",server->connect_ms);
        else snprintf(info,sizeof(info),"%s",server->query_state==SERVER_STATUS_QUERYING || server->query_state==SERVER_STATUS_QUEUED ? "..." : "--");
        label_fit(info,484,y+28,8,74,col(197,198,190,255));
        label_fit(server->version[0] ? server->version : "Beta 1.7.3", 403, y+46, 9,
                  155, server->compatible || server->ping_ms < 0 ?
                  col(181, 183, 175, 255) : col(244, 134, 124, 255));
    }
    if (count > 4) {
        rect(574, 69, 5, 272, col(16, 17, 18, 255));
        rect(574, 69+ui->server_scroll*272/count, 5,
             272*4/count > 14 ? 272*4/count : 14, col(172, 173, 167, 255));
    }
    if (IsKeyPressed(KEY_UP) || IsKeyPressed(KEY_DOWN)) {
        int step = IsKeyPressed(KEY_UP) ? -1 : 1;
        int next = ui->selected_server < 0 ? (step > 0 ? 0 : count-1) : ui->selected_server+step;
        if (next >= 0 && next < count) {
            ui->selected_server = next;
            if (next < ui->server_scroll) ui->server_scroll = next;
            if (next >= ui->server_scroll+4) ui->server_scroll = next-3;
        }
    }
    valid = ui->selected_server >= 0 && ui->selected_server < count;
    can_join = valid && servers[ui->selected_server].protocol==14;
    if (button(ui, 67, 365, 164, 28, "Join Server", can_join) ||
        (IsKeyPressed(KEY_ENTER) && can_join))
        fill_server_action(&action, UI_ACTION_JOIN_SERVER, servers, count, ui->selected_server);
    if (button(ui, 238, 365, 164, 28, "Direct Connect", 1)) {
        ui->server_address[0] = '\0';
        ui_set_screen(ui, UI_SCREEN_DIRECT_CONNECT); ui->focus = 1;
    }
    if (button(ui, 409, 365, 164, 28, "Add Server", count < UI_MAX_SERVERS)) {
        ui->editing_server = -1;
        copy_text(ui->server_name, sizeof(ui->server_name), "My Server");
        ui->server_address[0] = '\0'; ui->server_hide_address = 0; ui->server_protocol=14;
        ui_set_screen(ui, UI_SCREEN_SERVER_EDIT); ui->focus = 1;
    }
    if (button(ui, 67, 400, 117, 26, "Edit", valid)) {
        ui->editing_server = ui->selected_server;
        copy_text(ui->server_name, sizeof(ui->server_name), servers[ui->selected_server].name);
        copy_text(ui->server_address, sizeof(ui->server_address), servers[ui->selected_server].address);
        ui->server_hide_address = servers[ui->selected_server].hide_address;
        ui->server_protocol = servers[ui->selected_server].protocol;
        ui_set_screen(ui, UI_SCREEN_SERVER_EDIT); ui->focus = 1;
    }
    if (button(ui, 192, 400, 117, 26, "Delete", valid)) {
        ui->pending_confirm = 2; ui_set_screen(ui, UI_SCREEN_CONFIRM);
    }
    if (button(ui, 317, 400, 117, 26, "Refresh", 1)) action.type = UI_ACTION_REFRESH_SERVERS;
    if (button(ui, 442, 400, 131, 26, "Cancel", 1)) ui_set_screen(ui, UI_SCREEN_MAIN);
    if (ui->status[0]) label_fit(ui->status, 60, 442, 10, 520, col(255, 206, 162, 255));
    return action;
}

static int address_present(const char *address)
{
    const unsigned char *p = (const unsigned char *)address;
    if (!*p) return 0;
    while (*p) { if (isspace(*p)) return 0; ++p; }
    return 1; /* Detailed hostname/port validation belongs to the protocol layer. */
}

static UiAction server_form(Ui *ui, int direct)
{
    UiAction action = empty_action();
    int valid;
    draw_background(1);
    title(direct ? "Direct Connection" : ui->editing_server < 0 ? "Add Server" : "Edit Server");
    panel(113, 94, 414, 285);
    if (!direct) {
        label("Server Name", 145, 118, 12, col(240, 240, 235, 255));
        text_field(ui, 1, 145, 139, 350, 28, ui->server_name, sizeof(ui->server_name), "My Server");
        edit_text(ui, 1, ui->server_name, sizeof(ui->server_name));
    }
    label("Server Address", 145, direct ? 144 : 188, 12, col(240, 240, 235, 255));
    text_field(ui, direct ? 1 : 2, 145, direct ? 169 : 209, 350, 28,
               ui->server_address, sizeof(ui->server_address), "hostname:25565");
    edit_text(ui, direct ? 1 : 2, ui->server_address, sizeof(ui->server_address));
    if (direct) centered("Minecraft Java Beta 1.7.3 (protocol 14)", 320, 222,
                         11, col(201, 202, 194, 255));
    else if (button(ui, 145, 260, 350, 27,
                    ui->server_hide_address ? "Hide Address: Yes" : "Hide Address: No", 1))
        ui->server_hide_address = !ui->server_hide_address;
    if(!direct && button(ui,145,291,350,27,ui->server_protocol==47 ?
        "Protocol: Modern status (1.8+)" : "Protocol: Beta 1.7.3",1))
        ui->server_protocol=ui->server_protocol==47 ? 14 : 47;
    valid = address_present(ui->server_address) && (direct || ui->server_name[0]);
    if (button(ui, 145, 329, 170, 29, direct ? "Join Server" : "Done", valid) ||
        (IsKeyPressed(KEY_ENTER) && valid)) {
        action.type = direct ? UI_ACTION_JOIN_SERVER : UI_ACTION_SAVE_SERVER;
        action.index = direct ? -1 : ui->editing_server;
        copy_text(action.server_name, sizeof(action.server_name), direct ? "Direct Connection" : ui->server_name);
        copy_text(action.server_address, sizeof(action.server_address), ui->server_address);
        action.hide_address = ui->server_hide_address;
        action.server_protocol = direct ? 14 : ui->server_protocol;
        if (!direct) ui_set_screen(ui, UI_SCREEN_MULTIPLAYER);
    }
    if (button(ui, 325, 329, 170, 29, "Cancel", 1)) ui_set_screen(ui, UI_SCREEN_MULTIPLAYER);
    if (ui->status[0]) label_fit(ui->status, 90, 399, 10, 460, col(255, 206, 162, 255));
    return action;
}

static void option_slider(Ui *ui,int id,int x,int y,int *value,int minimum,int maximum,const char *name,int percent)
{
    char caption[100]; int hot=inside(x,y,258,30),position;
    if(hot && layout.clicked) { ui->slider_drag=id; ui->click_sound=1; }
    if(!IsMouseButtonDown(MOUSE_LEFT_BUTTON)) ui->slider_drag=0;
    if(ui->slider_drag==id) {
        int v=minimum+(int)((layout.mouse_x-x-7)*(maximum-minimum)/244+.5f);
        *value=v<minimum ? minimum : v>maximum ? maximum : v;
    }
    gui_button_draw(px(x),py(y),ps(258),ps(30),"",GUI_BUTTON_DISABLED);
    position=(*value-minimum)*244/(maximum-minimum);
    gui_button_draw(px(x+position+1),py(y+1),ps(12),ps(28),"",hot ? GUI_BUTTON_HOVERED : GUI_BUTTON_NORMAL);
    snprintf(caption,sizeof(caption),"%s: %d%s",language_caption(name),*value,percent ? "%" : "");
    centered(caption,x+129,y+9,12,WHITE);
}
static UiAction options_menu(Ui *ui)
{
    UiAction action = empty_action();
    static const char *const difficulties[]={"Peaceful","Easy","Normal","Hard"};
    char text[100];
    draw_background(1);
    title("Options");
    option_slider(ui,1,55,74,&ui->options.music_volume,0,100,"Music",1);
    option_slider(ui,2,327,74,&ui->options.sound_volume,0,100,"Sound",1);
    snprintf(text,sizeof(text),"%s: %s",language_caption("Invert Mouse"),language_caption(ui->options.invert_mouse ? "ON" : "OFF"));
    if(button(ui,55,116,258,30,text,1)) ui->options.invert_mouse=!ui->options.invert_mouse;
    option_slider(ui,3,327,116,&ui->options.sensitivity,0,200,"Sensitivity",1);
    option_slider(ui,4,55,158,&ui->options.fov,70,110,"FOV",0);
    snprintf(text,sizeof(text),"%s: %s",language_caption("Difficulty"),language_caption(difficulties[ui->options.difficulty]));
    if(button(ui,327,158,258,30,text,!ui->network_mode)) ui->options.difficulty=(ui->options.difficulty+1)%4;
    if(button(ui,55,274,258,30,"Video Settings...",1)) ui_set_screen(ui,UI_SCREEN_VIDEO);
    if(button(ui,327,274,258,30,"Controls...",1)) ui_set_screen(ui,UI_SCREEN_CONTROLS);
    if(button(ui,55,316,258,30,"Language...",1)) { ui->language_parent=UI_SCREEN_OPTIONS; ui_set_screen(ui,UI_SCREEN_LANGUAGES); }
    if(button(ui,327,316,258,30,"Multiplayer Settings...",1)) ui_set_screen(ui,UI_SCREEN_MULTIPLAYER_OPTIONS);
    if(button(ui,55,358,258,30,"Texture Packs",1)) ui_set_screen(ui,UI_SCREEN_PACKS);
    if (button(ui, 170, 420, 300, 30, "Done", 1)) ui_set_screen(ui, ui->options_parent);
    return action;
}
static UiAction profile_menu(Ui *ui)
{
    UiAction action=empty_action();draw_background(1);title("Player Profile");
    player_inventory_draw(px(155),py(330),ps(95),px(layout.mouse_x),py(layout.mouse_y),recraft_screen_width(),recraft_screen_height());
    label("Player Name",270,95,12,WHITE);
    text_field_enabled(ui,1,270,120,290,30,ui->player_name_input,sizeof(ui->player_name_input),"Player",!ui->auth_signed_in);
    if(!ui->auth_signed_in) {
        edit_text(ui,1,ui->player_name_input,sizeof(ui->player_name_input));
        if(settings_player_name_valid(ui->player_name_input)) copy_text(ui->options.player_name,sizeof(ui->options.player_name),ui->player_name_input);
    }
    if(button(ui,270,172,290,30,ui->auth_busy?"Cancel Microsoft sign-in":"Sign in with Microsoft",1))
        action.type=ui->auth_busy?UI_ACTION_MICROSOFT_CANCEL:UI_ACTION_MICROSOFT_LOGIN;
    if(button(ui,270,214,140,30,"Pack skin",1)) copy_text(ui->options.skin,sizeof(ui->options.skin),"default");
    if(button(ui,420,214,140,30,"Classic Steve",1)) copy_text(ui->options.skin,sizeof(ui->options.skin),"classic");
    if(button(ui,270,256,290,30,"Choose skin file...",1)) action.type=UI_ACTION_CHOOSE_SKIN;
    if(ui->auth_signed_in && button(ui,270,298,290,30,"Sign out",1)) action.type=UI_ACTION_MICROSOFT_LOGOUT;
    if(ui->auth_code[0]) {centered(ui->auth_code,415,345,16,YELLOW);centered("Enter this code at microsoft.com/link",415,370,9,WHITE);}
    if(ui->profile_status[0]) label_fit(ui->profile_status,50,393,9,540,col(255,206,162,255));
    if(button(ui,220,420,200,30,"Done",settings_player_name_valid(ui->player_name_input))) ui_set_screen(ui,UI_SCREEN_MAIN);
    return action;
}

static void list_scrollbar(Ui *ui,int count,int visible,int row_height,int top,int *scroll,int *drag)
{
    int max=count-visible,track=visible*row_height,height,position;
    (void)ui;
    if(max<0) max=0;
    if(inside(55,top,530,track)) *scroll-=GetMouseWheelMove()*3;
    if(*scroll<0) *scroll=0;
    if(*scroll>max) *scroll=max;
    if(!max) { *drag=0; return; }
    height=track*visible/count; if(height<24) height=24;
    position=*scroll*(track-height)/max;
    if(layout.clicked && inside(575,top,10,track)) *drag=1;
    if(!IsMouseButtonDown(MOUSE_LEFT_BUTTON)) *drag=0;
    if(*drag) {
        *scroll=(int)((layout.mouse_y-top-height*.5f)*max/(track-height)+.5f);
        if(*scroll<0) *scroll=0;
        if(*scroll>max) *scroll=max;
        position=*scroll*(track-height)/max;
    }
    rect(575,top,10,track,col(0,0,0,255)); rect(575,top+position,8,height,col(157,157,157,255)); rect(575,top+position,2,height,col(206,206,206,255));
}
static UiAction languages_menu(Ui *ui)
{
    UiAction action=empty_action(); int i,count=language_count();
    draw_background(1); title("Language..."); rect(55,65,530,330,col(0,0,0,135));
    list_scrollbar(ui,count,11,30,65,&ui->language_scroll,&ui->language_drag);
    for(i=ui->language_scroll;i<count && i<ui->language_scroll+11;++i) {
        const LanguageEntry *entry=language_at(i); int y=65+(i-ui->language_scroll)*30;
        if(!strcmp(entry->code,ui->options.language)) { rect(145,y,350,29,col(0,0,0,180)); linebox(145,y,350,29,col(155,155,155,255)); }
        centered(entry->name,320,y+9,12,inside(145,y,350,29) ? YELLOW : WHITE);
        if(layout.clicked && inside(145,y,350,29) && language_select(entry->code)) {
            copy_text(ui->options.language,sizeof(ui->options.language),entry->code); ui->click_sound=1;
        }
    }
    centered(language_text("options.languageWarning","Language translations may not be 100% accurate"),320,404,10,col(160,160,160,255));
    if(button(ui,220,432,200,30,"Done",1)) ui_set_screen(ui,ui->language_parent);
    return action;
}
static UiAction packs_menu(Ui *ui)
{
    UiAction action=empty_action(); int i;
    draw_background(1); title("Select Texture Pack"); rect(55,65,530,330,col(0,0,0,135));
    list_scrollbar(ui,pack_count+1,6,55,65,&ui->pack_scroll,&ui->pack_drag);
    for(i=ui->pack_scroll;i<=pack_count && i<ui->pack_scroll+6;++i) {
        int y=65+(i-ui->pack_scroll)*55; const char *id=i ? pack_entries[i-1].id : "";
        Texture2D icon=assets_pack_icon(id); Rectangle src={0,0,(float)icon.width,(float)icon.height};
        Rectangle dst={(float)px(65),(float)py(y+3),(float)ps(48),(float)ps(48)}; Vector2 origin={0,0};
        if(!strcmp(id,ui->options.texture_pack)) { rect(63,y,505,54,col(0,0,0,180)); linebox(63,y,505,54,col(150,150,150,255)); }
        label_fit(i ? pack_entries[i-1].name : "Default",120,y+9,12,432,WHITE);
        DrawTexturePro(icon,src,dst,origin,0,WHITE);
        label_fit(i ? pack_entries[i-1].description : "The bundled look of ReCraft",120,y+27,10,432,col(150,150,150,255));
        if(layout.clicked && inside(63,y,505,54)) { copy_text(ui->options.texture_pack,sizeof(ui->options.texture_pack),id); ui->click_sound=1; }
    }
    if(button(ui,55,417,230,30,"Open texture pack folder",1)) action.type=UI_ACTION_OPEN_PACK_FOLDER;
    if(button(ui,295,417,90,30,"Refresh",1)) pack_count=resource_pack_list(pack_entries,128);
    if(button(ui,395,417,190,30,"Done",1)) ui_set_screen(ui,UI_SCREEN_OPTIONS);
    centered("texturepacks/ and resourcepacks/ - ZIP files or folders",320,459,9,col(170,170,170,255));
    if(ui->status[0]) centered(ui->status,320,400,9,col(255,160,140,255));
    return action;
}
static UiAction controls_menu(Ui *ui,int multiplayer)
{
    UiAction action=empty_action(); draw_background(1); title(multiplayer ? "Multiplayer Settings..." : "Controls");
    if(multiplayer) {
        if(button(ui,170,105,300,30,ui->options.chat_visible ? "Chat: Shown" : "Chat: Hidden",1)) ui->options.chat_visible=!ui->options.chat_visible;
        centered("Player name is set in Options.",320,160,12,WHITE);
    } else {
        static const char *lines[]={"WASD: move    Mouse: look    Space: jump","Left mouse: break    Right mouse: use / place",
            "1-9 / mouse wheel: hotbar    E: inventory","F: creative flight    Shift: down / dismount",
            "Ctrl: sprint    T: chat    Tab: players","Escape: pause / back    F2: screenshot    F3: statistics"};
        int i; for(i=0;i<6;++i) centered(lines[i],320,100+i*38,12,WHITE);
    }
    if(button(ui,220,420,200,30,"Done",1)) ui_set_screen(ui,UI_SCREEN_OPTIONS);
    return action;
}

static int option_button(Ui *ui, int index, const char *caption, int enabled)
{
    return button(ui, (index%2) ? 327 : 55, 59+(index/2)*29, 258, 25, caption, enabled);
}

static void brightness_slider(Ui *ui, int index)
{
    int x = (index%2) ? 327 : 55, y = 59+(index/2)*29;
    char text[64];
    int old = ui->options.brightness;
    rect(x, y, 258, 25, col(21, 22, 23, 255));
    rect(x+2, y+2, 254, 21, col(80, 83, 88, 255));
    if (inside(x, y, 258, 25) && IsMouseButtonDown(MOUSE_LEFT_BUTTON)) {
        ui->options.brightness = (int)((layout.mouse_x-x-7)*100.0f/244.0f+0.5f);
        if (ui->options.brightness < 0) ui->options.brightness = 0;
        if (ui->options.brightness > 100) ui->options.brightness = 100;
        if (layout.clicked && old != ui->options.brightness) ui->click_sound = 1;
    }
    rect(x+2+ui->options.brightness*244/100, y+2, 10, 21, col(159, 163, 168, 255));
    snprintf(text, sizeof(text), "Brightness: %d%%", ui->options.brightness);
    centered(text, x+129, y+7, 11, col(248, 248, 241, 255));
}

static UiAction video_menu(Ui *ui)
{
    UiAction action = empty_action();
    UiOptions *o = &ui->options;
    char text[80];
    const char *gui_names[] = { "Auto", "Small", "Normal", "Large" };
    const char *vbo_names[] = { "Auto", "On", "Off" };
    draw_background(1);
    title("Video Settings");
    if (option_button(ui, 0, o->fancy_graphics ? "Graphics: Fancy" : "Graphics: Fast", 1)) {
        o->fancy_graphics = !o->fancy_graphics;
        o->fancy_leaves = o->fancy_graphics;
    }
    snprintf(text, sizeof(text), "Render Distance: %d chunks", o->render_distance);
    if (option_button(ui, 1, text, 1)) o->render_distance = o->render_distance >= 12 ? 2 : o->render_distance+1;
    if (option_button(ui, 2, o->smooth_lighting ? "Smooth Lighting: ON" : "Smooth Lighting: OFF", 1))
        o->smooth_lighting = !o->smooth_lighting;
    if (o->vsync) snprintf(text, sizeof(text), "Max Framerate: VSync (%d Hz)", ui->monitor_hz);
    else snprintf(text, sizeof(text), "Max Framerate: %d", o->max_framerate);
    if (option_button(ui, 3, text, 1)) {
        static const int caps[] = {25,30,50,60,75,90,120,144};
        int i, next = 0;
        for (i = 0; i < 8; ++i) {
            if (caps[i] > o->max_framerate && caps[i] <= ui->monitor_hz) {
                next = caps[i]; break;
            }
        }
        o->max_framerate = next;
        o->vsync = next == 0;
    }
    if (option_button(ui, 4, o->menu_blur ? "Menu Blur: ON" : "Menu Blur: OFF", 1))
        o->menu_blur = !o->menu_blur;
    if (option_button(ui, 5, o->view_bobbing ? "View Bobbing: ON" : "View Bobbing: OFF", 1))
        o->view_bobbing = !o->view_bobbing;
    snprintf(text, sizeof(text), "GUI Scale: %s", gui_names[o->gui_scale >= 0 && o->gui_scale < 4 ? o->gui_scale : 0]);
    if (option_button(ui, 6, text, 1)) o->gui_scale = (o->gui_scale+1)%4;
    brightness_slider(ui, 7);
    (void)option_button(ui, 8, "Clouds: OFF (unavailable)", 0);
    (void)option_button(ui, 9, "Particles: Minimal (unavailable)", 0);
    if (option_button(ui, 10, o->fullscreen ? "Fullscreen: ON" : "Fullscreen: OFF", 1)) o->fullscreen = !o->fullscreen;
    snprintf(text,sizeof(text),"Display Refresh: %d Hz",ui->monitor_hz);
    (void)option_button(ui, 11, text, 0);
    if (!ui->mipmap_available) (void)option_button(ui, 12, "Mipmaps: unavailable (GL 1.1)", 0);
    else {
        if (o->mipmaps) snprintf(text, sizeof(text), "Mipmaps: %d", o->mipmaps);
        else snprintf(text, sizeof(text), "Mipmaps: OFF");
        if (option_button(ui, 12, text, 1)) o->mipmaps = (o->mipmaps+1)%5;
    }
    (void)option_button(ui, 13, "Alternate Blocks: unavailable", 0);
    (void)option_button(ui, 14, "Entity Shadows: unavailable", 0);
    if (!ui->vbo_available) (void)option_button(ui, 15, "VBO: unavailable (client arrays)", 0);
    else {
        snprintf(text, sizeof(text), "Use VBO: %s", vbo_names[o->use_vbo >= 0 && o->use_vbo < 3 ? o->use_vbo : 0]);
        if (option_button(ui, 15, text, 1)) o->use_vbo = (o->use_vbo+1)%3;
    }
    if (option_button(ui, 16, o->greedy_mesh ? "Chunk Mesh Mode: Greedy" : "Chunk Mesh Mode: Basic", 1))
        o->greedy_mesh = !o->greedy_mesh;
    snprintf(text, sizeof(text), "Chunk Build Budget: %d/frame", o->chunk_build_budget);
    if (option_button(ui, 17, text, 1)) o->chunk_build_budget = o->chunk_build_budget >= 8 ? 1 : o->chunk_build_budget+1;
    if (option_button(ui, 18, o->fog ? "Fog: OFF" : "Fog: Fast", 1)) o->fog = !o->fog;
    if (option_button(ui, 19, o->fancy_leaves ? "Transparent Leaves: Fancy" : "Transparent Leaves: Fast", 1))
        o->fancy_leaves = !o->fancy_leaves;
    snprintf(text, sizeof(text), "Dynamic Chunk Updates: %d/tick", o->dynamic_updates);
    if (option_button(ui, 20, text, 1)) o->dynamic_updates = o->dynamic_updates >= 8 ? 1 : o->dynamic_updates+1;
    if (option_button(ui, 21, o->debug_statistics ? "Debug Statistics: ON" : "Debug Statistics: OFF", 1))
        o->debug_statistics = !o->debug_statistics;
    if (option_button(ui, 22, "Apply Legacy Hardware Preset", 1)) {
        Ui defaults;
        int fullscreen = o->fullscreen;
        ui_init(&defaults);
        *o = defaults.options;
        o->fullscreen = fullscreen;
    }
    if (!ui->vbo_available) (void)option_button(ui, 23, "VBO Budget: unavailable", 0);
    else {
        snprintf(text, sizeof(text), "VBO Budget: %d MiB", o->vbo_budget_mb);
        if (option_button(ui, 23, text, 1)) o->vbo_budget_mb = o->vbo_budget_mb >= 32 ? 4 : o->vbo_budget_mb*2;
    }
    if(option_button(ui,24,"Environment...",1)) ui_set_screen(ui,UI_SCREEN_ENVIRONMENT);
    if (button(ui, 220, 442, 200, 29, "Done", 1)) ui_set_screen(ui, UI_SCREEN_OPTIONS);
    return action;
}

static UiAction environment_menu(Ui *ui)
{
    UiOptions *o=&ui->options;UiAction action=empty_action();
    draw_background(1);title("Environment");
    if(option_button(ui,0,o->reduced_transparency ? "Reduced Transparency: ON" : "Reduced Transparency: OFF",1))o->reduced_transparency=!o->reduced_transparency;
    if(option_button(ui,1,o->colored_redstone ? "Red Torch Light: Red" : "Red Torch Light: Vanilla",1))o->colored_redstone=!o->colored_redstone;
    if(option_button(ui,2,o->fancy_leaves ? "Transparent Leaves: Fancy" : "Transparent Leaves: Fast",1))o->fancy_leaves=!o->fancy_leaves;
    if(option_button(ui,3,o->smooth_lighting ? "Smooth Lighting: ON" : "Smooth Lighting: OFF",1))o->smooth_lighting=!o->smooth_lighting;
    if(option_button(ui,4,o->menu_blur ? "Menu Blur: ON" : "Menu Blur: OFF",1))o->menu_blur=!o->menu_blur;
    centered("Red torch colour is an optional visual effect.",320,240,11,col(180,180,180,255));
    if(button(ui,220,442,200,29,"Done",1))ui_set_screen(ui,UI_SCREEN_VIDEO);
    return action;
}

static UiAction pause_menu(Ui *ui)
{
    UiAction action = empty_action();
    /* The game layer can skip terrain rendering while paused. */
    draw_background(1);
    title("Game Paused");
    if (button(ui, 190, 170, 260, 31, "Back to Game", 1)) {
        action.type = UI_ACTION_RESUME; ui_set_screen(ui, UI_SCREEN_GAME);
    }
    if (button(ui, 190, 216, 260, 31, "Options...", 1)) {
        ui->options_parent = UI_SCREEN_PAUSE; ui_set_screen(ui, UI_SCREEN_OPTIONS);
    }
    if (button(ui, 190, 280, 260, 31, "Save and Return to Menu", 1)) {
        action.type=UI_ACTION_RETURN_TO_MENU;
    }
    if (ui->status[0]) centered(ui->status,320,342,10,col(255,180,160,255));
    return action;
}

static UiAction death_menu(Ui *ui)
{
    UiAction action=empty_action();
    rect(0,0,640,480,col(110,0,0,95));
    centered("You died!",320,120,28,WHITE);
    if (button(ui,120,215,400,36,"Respawn",1)) action.type=UI_ACTION_RESPAWN;
    if (button(ui,120,260,400,36,"Title screen",1)) action.type=UI_ACTION_RETURN_TO_MENU;
    return action;
}
static UiAction confirmation(Ui *ui, const UiWorldEntry *worlds, int world_count,
                             const UiServerEntry *servers, int server_count)
{
    UiAction action = empty_action();
    UiScreen parent = ui->previous_screen;
    const char *message = "Quit the game?";
    int valid = 1;
    draw_background(1);
    title("Confirm");
    panel(84, 140, 472, 191);
    if (ui->pending_confirm == 1) {
        valid = worlds && ui->selected_world >= 0 && ui->selected_world < world_count;
        message = "Permanently delete this world?";
        if (valid) label_fit(worlds[ui->selected_world].name, 113, 192, 13, 414, col(238, 224, 176, 255));
        centered("This removes all saved blocks in the selected world.", 320, 222, 10, col(218, 200, 193, 255));
    } else if (ui->pending_confirm == 2) {
        valid = servers && ui->selected_server >= 0 && ui->selected_server < server_count;
        message = "Remove this server from the list?";
        if (valid) label_fit(servers[ui->selected_server].name, 113, 203, 13, 414, col(238, 224, 176, 255));
    } else if (ui->pending_confirm == 4) message = "Save progress and return to the main menu?";
    centered(message, 320, 162, 13, col(247, 244, 236, 255));
    if (button(ui, 117, 278, 196, 29, "Yes", valid)) {
        if (ui->pending_confirm == 1)
            fill_world_action(&action, UI_ACTION_DELETE_WORLD, worlds, world_count, ui->selected_world);
        else if (ui->pending_confirm == 2)
            fill_server_action(&action, UI_ACTION_DELETE_SERVER, servers, server_count, ui->selected_server);
        else if (ui->pending_confirm == 4) action.type = UI_ACTION_RETURN_TO_MENU;
        else action.type = UI_ACTION_QUIT;
        ui_set_screen(ui, parent);
    }
    if (button(ui, 327, 278, 196, 29, "Cancel", 1)) ui_set_screen(ui, parent);
    return action;
}

UiAction ui_frame(Ui *ui, const UiWorldEntry *worlds, int world_count,
                  const UiServerEntry *servers, int server_count)
{
    UiAction action = empty_action();
    if (!ui) return action;
    begin_layout(ui);
    ui->click_sound = 0;
    if (IsKeyPressed(258)) { /* GLFW/raylib 1.4 TAB key. */
        int fields = ui->screen == UI_SCREEN_SERVER_EDIT ||
                     (ui->screen == UI_SCREEN_CREATE_WORLD && ui->create_more_options) ? 2 : 1;
        ui->focus = ui->focus%fields+1;
        ui->select_all = 0;
    }
    if (IsKeyPressed(KEY_ESCAPE)) {
        switch (ui->screen) {
            case UI_SCREEN_PAUSE:
                action.type = UI_ACTION_RESUME; ui_set_screen(ui, UI_SCREEN_GAME); return action;
            case UI_SCREEN_CONFIRM: ui_set_screen(ui, ui->previous_screen); break;
            case UI_SCREEN_ENVIRONMENT: ui_set_screen(ui,UI_SCREEN_VIDEO); break;
            case UI_SCREEN_VIDEO: ui_set_screen(ui, UI_SCREEN_OPTIONS); break;
            case UI_SCREEN_LANGUAGES: ui_set_screen(ui,ui->language_parent); break;
            case UI_SCREEN_PACKS:
            case UI_SCREEN_CONTROLS:
            case UI_SCREEN_MULTIPLAYER_OPTIONS: ui_set_screen(ui,UI_SCREEN_OPTIONS); break;
            case UI_SCREEN_OPTIONS: ui_set_screen(ui, ui->options_parent); break;
            case UI_SCREEN_PROFILE: ui_set_screen(ui,UI_SCREEN_MAIN);break;
            case UI_SCREEN_CREATE_WORLD:
            case UI_SCREEN_WORLD_EDIT: ui_set_screen(ui, UI_SCREEN_WORLDS); break;
            case UI_SCREEN_SERVER_EDIT:
            case UI_SCREEN_DIRECT_CONNECT: ui_set_screen(ui, UI_SCREEN_MULTIPLAYER); break;
            case UI_SCREEN_WORLDS:
            case UI_SCREEN_MULTIPLAYER: ui_set_screen(ui, UI_SCREEN_MAIN); break;
            default: break;
        }
    }
    switch (ui->screen) {
        case UI_SCREEN_MAIN: return main_menu(ui);
        case UI_SCREEN_WORLDS: return world_list(ui, worlds, world_count);
        case UI_SCREEN_CREATE_WORLD: return create_world(ui);
        case UI_SCREEN_WORLD_EDIT: return edit_world(ui, worlds, world_count);
        case UI_SCREEN_MULTIPLAYER: return multiplayer(ui, servers, server_count);
        case UI_SCREEN_SERVER_EDIT: return server_form(ui, 0);
        case UI_SCREEN_DIRECT_CONNECT: return server_form(ui, 1);
        case UI_SCREEN_OPTIONS: return options_menu(ui);
        case UI_SCREEN_PROFILE: return profile_menu(ui);
        case UI_SCREEN_VIDEO: return video_menu(ui);
        case UI_SCREEN_ENVIRONMENT: return environment_menu(ui);
        case UI_SCREEN_LANGUAGES: return languages_menu(ui);
        case UI_SCREEN_PACKS: return packs_menu(ui);
        case UI_SCREEN_CONTROLS: return controls_menu(ui,0);
        case UI_SCREEN_MULTIPLAYER_OPTIONS: return controls_menu(ui,1);
        case UI_SCREEN_PAUSE: return pause_menu(ui);
        case UI_SCREEN_DEATH: return death_menu(ui);
        case UI_SCREEN_CONFIRM: return confirmation(ui, worlds, world_count, servers, server_count);
        case UI_SCREEN_GAME: return action;
        default: ui_set_screen(ui, UI_SCREEN_MAIN); return main_menu(ui);
    }
}

static void tile_quad(int tile, const float x[4], const float y[4], unsigned shade)
{
    float u0,u1,v0,v1;
    if (tile<0) tile=1;
    u0=((tile%16)*16+0.5f)/256.0f;
    u1=((tile%16)*16+15.5f)/256.0f;
    v0=((tile/16)*16+0.5f)/256.0f;
    v1=((tile/16)*16+15.5f)/256.0f;
    if (tile==0 || tile==52 || tile==132)
        glColor4ub((unsigned char)(shade*4/5),(unsigned char)shade,(unsigned char)(shade*2/3),255);
    else glColor4ub((unsigned char)shade,(unsigned char)shade,(unsigned char)shade,255);
    glTexCoord2f(u0,v0); glVertex2f(x[0],y[0]);
    glTexCoord2f(u1,v0); glVertex2f(x[1],y[1]);
    glTexCoord2f(u1,v1); glVertex2f(x[2],y[2]);
    glTexCoord2f(u0,v1); glVertex2f(x[3],y[3]);
}

static void item_tiles(int id, int damage, int *top, int *side)
{
    /* Inventory previews for the remaining registry entries. World mesh
     * support is tracked separately in BETA_COMPATIBILITY_MATRIX.md. */
    static const unsigned char preview[97]={
        0,1,3,2,16,4,15,17,205,205,237,237,18,19,32,33,
        34,20,52,48,49,160,144,46,192,74,134,179,195,106,11,39,
        55,107,107,64,107,13,12,29,28,23,22,6,6,7,8,35,
        36,37,80,31,65,4,27,164,50,24,60,95,87,44,61,4,
        81,83,128,16,4,96,1,82,4,51,51,115,99,1,66,67,
        66,70,72,73,74,4,119,103,104,105,14,120,121,131,147,27,84
    };
    switch (id) {
    case 1: *top=*side=1; break;
    case 2: *top=0; *side=3; break;
    case 3: *top=*side=2; break;
    case 4: *top=*side=16; break;
    case 12: *top=*side=18; break;
    case 20: *top=*side=49; break;
    default: {
        BetaBlockState state={(uint8_t)id,(uint8_t)damage};
        *top=beta_block_terrain_tile(state,1);
        *side=beta_block_terrain_tile(state,2);
        if (*top<0) *top=id>0 && id<97 ? preview[id] : 1;
        if (*side<0) *side=*top;
        if (id==23 || id==61 || id==62) *top=62;
        if (id==24) *top=176;
        if (id==29 || id==33 || id==34) *side=108;
        if (id==46) *top=9;
        if (id==47) *top=4;
        if (id==54 || id==95) *top=25;
        if (id==58) *top=43;
        if (id==60) { *top=87; *side=2; }
        if (id==81) *top=69;
        if (id==84) *top=75;
        if (id==86 || id==91) *top=102;
        break;
    }
    }
}

static void item_box_icon(const BetaBlockBox *b,int top,int side,float x,float y,float size)
{
    float points[8][3],sx[4],sy[4]; int i,f;
    static const int faces[3][4]={{3,2,6,7},{4,5,6,7},{1,5,6,2}};
    for(i=0;i<8;++i) {
        points[i][0]=(i==1 || i==2 || i==5 || i==6) ? b->max_x : b->min_x;
        points[i][1]=i==2 || i==3 || i==6 || i==7 ? b->max_y : b->min_y;
        points[i][2]=i>=4 ? b->max_z : b->min_z;
    }
    for(f=0;f<3;++f) {
        for(i=0;i<4;++i) {
            const float *p=points[faces[f][i]];
            sx[i]=x+size*(.5f+(p[0]-p[2])*.41f);
            sy[i]=y+size*(.06f+(p[0]+p[2])*.22f+(1-p[1])*.42f);
        }
        tile_quad(f ? side : top,sx,sy,f==0 ? 255 : f==1 ? 175 : 135);
    }
}

static void draw_item_icon(int id, int damage, int x, int y, int size)
{
    Texture2D terrain=assets_get_texture(ASSET_TERRAIN);
    float fx=(float)x,fy=(float)y,s=(float)size;
    float tx[4]={fx+s*0.50f,fx+s*0.91f,fx+s*0.50f,fx+s*0.09f};
    float ty[4]={fy+s*0.06f,fy+s*0.28f,fy+s*0.51f,fy+s*0.28f};
    float lx[4]={fx+s*0.09f,fx+s*0.50f,fx+s*0.50f,fx+s*0.09f};
    float ly[4]={fy+s*0.28f,fy+s*0.51f,fy+s*0.93f,fy+s*0.70f};
    float rx[4]={fx+s*0.50f,fx+s*0.91f,fx+s*0.91f,fx+s*0.50f};
    float ry[4]={fy+s*0.51f,fy+s*0.28f,fy+s*0.70f,fy+s*0.93f};
    int top,side;
    if (id<=0) return;
    if (id>=256) {
        Texture2D items=assets_get_texture(ASSET_GUI_ITEMS);
        int tile=beta_item_tile(id,damage);
        Rectangle src,dst;
        Vector2 origin={0,0};
        if (tile<0 || tile>=256) return;
        src.x=(float)((tile%16)*16); src.y=(float)((tile/16)*16);
        src.width=src.height=16;
        dst.x=(float)x; dst.y=(float)y;
        dst.width=dst.height=(float)size;
        DrawTexturePro(items,src,dst,origin,0,WHITE);
        return;
    }
    if (id==BETA_BLOCK_TORCH || beta_block_cross_plant((unsigned)id) ||
        id==27 || id==28 || id==30 || id==55 || id==59 || id==64 ||
        id==65 || id==66 || id==69 || id==71 || id==75 || id==76) {
        int tile,unused;
        item_tiles(id,damage,&unused,&tile);
        Rectangle src={(float)((tile%16)*16),(float)((tile/16)*16),16,16};
        Rectangle dst={(float)x,(float)y,(float)size,(float)size};
        Vector2 origin={0,0};
        DrawTexturePro(terrain,src,dst,origin,0,WHITE);
        return;
    }
    item_tiles(id,damage,&top,&side);
    glPushAttrib(GL_ENABLE_BIT|GL_TEXTURE_BIT|GL_CURRENT_BIT|GL_COLOR_BUFFER_BIT);
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_CULL_FACE);
    glEnable(GL_TEXTURE_2D);
    glEnable(GL_ALPHA_TEST);
    glAlphaFunc(GL_GREATER,0.5f);
    glBindTexture(GL_TEXTURE_2D,terrain.id);
    glBegin(GL_QUADS);
    if(id==53 || id==67 || id==85 || id==96 || id==77 || id==70 || id==72 || id==44 || id==78 || id==92 || id==60) {
        BetaBlockBox boxes[5]; int i,n=beta_block_item_boxes((BetaBlockState){(uint8_t)id,(uint8_t)damage},boxes);
        for(i=0;i<n;++i) item_box_icon(&boxes[i],top,side,fx,fy,s);
    } else {
        tile_quad(top,tx,ty,255);
        tile_quad(side,lx,ly,175);
        tile_quad(side,rx,ry,135);
    }
    glEnd();
    glPopAttrib();
}

static void draw_stack(const InventorySlot *item, int x, int y, int size)
{
    char count[16];
    int width;
    if (!item || item->id<=0 || item->count<=0) return;
    draw_item_icon(item->id,item->damage,px((float)x),py((float)y),ps((float)size));
    snprintf(count,sizeof(count),"%d",item->count);
    width=MeasureMinecraftText(count,ps(9));
    DrawMinecraftText(count,px((float)(x+size))-width,py((float)(y+size-10)),
                      ps(9),WHITE,1);
    if(item->damage>0 && inventory_max_damage(item->id)>0) {
        int maximum=inventory_max_damage(item->id);
        int remaining=(int)floorf(13.0f-item->damage*13.0f/maximum+.5f);
        int green=(int)floorf(255.0f-item->damage*255.0f/maximum+.5f);
        float unit=size/16.0f;
        int bx=px(x+2*unit),by=py(y+13*unit);
        if(remaining<0) remaining=0;
        if(green<0) green=0;
        DrawRectangle(bx,by,ps(13*unit),ps(2*unit),BLACK);
        DrawRectangle(bx,by,ps(12*unit),ps(unit),col((unsigned char)((255-green)/4),63,0,255));
        if(remaining>0) DrawRectangle(bx,by,ps(remaining*unit),ps(unit),col((unsigned char)(255-green),(unsigned char)green,0,255));
    }
}

void ui_draw_hud(const Ui *ui, int selected_slot, const InventorySlot *hotbar,
                 const char *const labels[9],
                 int debug_visible, const char *debug_text,int health,int air,int hurt)
{
    int i, cx = recraft_screen_width()/2, cy = recraft_screen_height()/2;
    Texture2D widgets = assets_get_texture(ASSET_GUI_WIDGETS);
    Texture2D icons = assets_get_texture(ASSET_GUI_ICONS);
    Vector2 origin = {0,0};
    Rectangle src, dst;
    begin_layout(ui);
    if (health>=0) {
        for (i=0;i<10;++i) {
            Rectangle heart={(float)(hurt>10 && (hurt/3)%2 ? 25 : 16),0,9,9};
            Rectangle dest={(float)px(139+i*16),(float)py(414),(float)ps(18),(float)ps(18)};
            DrawTexturePro(icons,heart,dest,origin,0,WHITE);
            if (health>i*2) {
                heart.x=health==i*2+1 ? 61 : 52;
                DrawTexturePro(icons,heart,dest,origin,0,WHITE);
            }
        }
        if (air<300) for (i=0;i<(air+29)/30 && i<10;++i) {
            Rectangle bubble={16,18,9,9};
            Rectangle dest={(float)px(139+i*16),(float)py(394),(float)ps(18),(float)ps(18)};
            DrawTexturePro(icons,bubble,dest,origin,0,WHITE);
        }
    }
    src = (Rectangle){0,0,16,16}; dst = (Rectangle){(float)(cx-8),(float)(cy-8),16,16};
    DrawTexturePro(icons,src,dst,origin,0,WHITE);
    src = (Rectangle){0,0,182,22};
    dst = (Rectangle){(float)px(130),(float)py(433),(float)ps(380),(float)ps(44)};
    DrawTexturePro(widgets,src,dst,origin,0,WHITE);
    src = (Rectangle){0,22,24,24};
    dst = (Rectangle){(float)px(130+selected_slot*42),(float)py(432),
                       (float)ps(44),(float)ps(46)};
    DrawTexturePro(widgets,src,dst,origin,0,WHITE);
    for (i = 0; i < 9; ++i) {
        int x=136+i*42;
        draw_stack(hotbar ? &hotbar[i] : NULL,x+1,439,29);
    }
    if (labels && selected_slot >= 0 && selected_slot < 9 && labels[selected_slot])
        centered(labels[selected_slot],320,health<0 ? 414 : air<300 ? 371 : 389,12,col(244,241,221,255));
    if (debug_visible && debug_text) {
        const char *p = debug_text;
        int row = 0;
        while (*p && row < 28) {
            const char *end = strchr(p, '\n');
            char text[256];
            size_t len = end ? (size_t)(end-p) : strlen(p);
            if (len >= sizeof(text)) len = sizeof(text)-1;
            memcpy(text, p, len); text[len] = '\0';
            rect(6, 5+row*13, 628, 13, col(11, 11, 13, 178));
            label_fit(text, 9, 7+row*13, 10, 619, col(247, 248, 239, 255));
            ++row;
            if (!end) break;
            p = end+1;
        }
    }
}

static void inventory_patch(int sx,int sy,int sw,int sh,int x,int y,int w,int h)
{
    Texture2D texture=assets_get_texture(ASSET_GUI_INVENTORY);
    Rectangle src={(float)sx,(float)sy,(float)sw,(float)sh};
    Rectangle dst={(float)px((float)x),(float)py((float)y),(float)ps((float)w),(float)ps((float)h)};
    Vector2 origin={0,0};
    DrawTexturePro(texture,src,dst,origin,0,WHITE);
}

/* Indices 0..35 inventory, 36..44 crafting, 45 result, 46..99 container. */
static int container_xy(const ContainerSession *s,int index,int *x,int *y)
{
    int rows=s->size/9,height=s->kind==CONTAINER_CHEST ? 114+rows*18 : 166;
    int left=144,top=(480-height*2)/2,ix,iy;
    if (index<36) {
        ix=8+(index%9)*18;
        iy=index<9 ? 142 : 84+(index-9)/9*18;
        if (s->kind==CONTAINER_CHEST) iy+=rows*18-53;
    } else if (index<45) {
        int n=index-36,width=s->kind==CONTAINER_WORKBENCH ? 3 : 2;
        if ((s->kind!=CONTAINER_PLAYER && s->kind!=CONTAINER_WORKBENCH) || n>=width*width) return 0;
        ix=(width==3 ? 30 : 88)+(n%width)*18;
        iy=(width==3 ? 17 : 26)+(n/width)*18;
    } else if (index==45) {
        if (s->kind!=CONTAINER_PLAYER && s->kind!=CONTAINER_WORKBENCH) return 0;
        ix=s->kind==CONTAINER_WORKBENCH ? 124 : 144;
        iy=s->kind==CONTAINER_WORKBENCH ? 35 : 36;
    } else if (s->kind==CONTAINER_CHEST) {
        int n=index-46;
        if (n>=s->size) return 0;
        ix=8+(n%9)*18; iy=18+(n/9)*18;
    } else if (s->kind==CONTAINER_DISPENSER) {
        int n=index-46;
        if(n>=9) return 0;
        ix=62+(n%3)*18; iy=17+(n/3)*18;
    } else if (s->kind==CONTAINER_FURNACE) {
        int n=index-46;
        if (n>2) return 0;
        ix=n==2 ? 116 : 56; iy=n==0 ? 17 : n==1 ? 53 : 35;
    } else return 0;
    *x=left+ix*2; *y=top+iy*2;
    return 1;
}

int ui_container_slot_at(const Ui *ui,const ContainerSession *session)
{
    int i,x,y;
    begin_layout(ui);
    for (i=0;i<100;++i) if (container_xy(session,i,&x,&y) && inside(x,y,32,32)) return i;
    return -1;
}

static void container_texture(Texture2D texture,int sx,int sy,int width,int height,int x,int y)
{
    Rectangle src={(float)sx,(float)sy,(float)width,(float)height};
    Rectangle dst={(float)px((float)x),(float)py((float)y),(float)ps((float)(width*2)),(float)ps((float)(height*2))};
    Vector2 origin={0,0};
    DrawTexturePro(texture,src,dst,origin,0,WHITE);
}

static void container_label(const char *text,int x,int y)
{
    /* Vanilla foreground captions are eight source pixels tall, without a
     * shadow. Match the atlas's 2x scale instead of shrinking the glyphs. */
    DrawMinecraftText(language_caption(text),px((float)x),py((float)y),ps(16),col(64,64,64,255),0);
}

void ui_draw_container(const Ui *ui,const ContainerSession *s,const InventorySlot *inventory,
                       const InventorySlot *contents,int burn,int fuel,int cook)
{
    int rows=s->size/9,height=s->kind==CONTAINER_CHEST ? 114+rows*18 : 166;
    int left=144,top=(480-height*2)/2,i,x,y;
    AssetId id=s->kind==CONTAINER_WORKBENCH ? ASSET_GUI_CRAFTING :
        s->kind==CONTAINER_DISPENSER ? ASSET_GUI_DISPENSER :
        s->kind==CONTAINER_FURNACE ? ASSET_GUI_FURNACE :
        s->kind==CONTAINER_CHEST ? ASSET_GUI_CONTAINER : ASSET_GUI_INVENTORY;
    Texture2D texture=assets_get_texture(id);
    InventorySlot result;
    begin_layout(ui);
    DrawRectangle(0,0,recraft_screen_width(),recraft_screen_height(),col(0,0,0,150));
    if(s->kind==CONTAINER_PLAYER) rect(left+52,top+16,100,140,BLACK);
    if (s->kind==CONTAINER_CHEST) {
        container_texture(texture,0,0,176,rows*18+17,left,top);
        container_texture(texture,0,126,176,96,left,top+(rows*18+17)*2);
        container_label(s->size==54 ? "Large Chest" : "Chest",left+16,top+12);
    } else container_texture(texture,0,0,176,166,left,top);
    if(s->kind==CONTAINER_DISPENSER) container_label("Dispenser",left+120,top+12);
    if (s->kind==CONTAINER_FURNACE) {
        container_label("Furnace",left+120,top+12);
        if (burn>0) {
            int n=fuel>0 ? burn*12/fuel : 0;
            if (n>12) n=12;
            container_texture(texture,176,12-n,14,n+2,left+112,top+(36+12-n)*2);
        }
        if (cook>0) container_texture(texture,176,14,cook*24/200+1,16,left+158,top+68);
    }
    if (s->kind==CONTAINER_PLAYER || s->kind==CONTAINER_WORKBENCH)
        container_label("Crafting",left+(s->kind==CONTAINER_PLAYER ? 172 : 56),
                        top+(s->kind==CONTAINER_PLAYER ? 32 : 12));
    if(s->kind==CONTAINER_PLAYER) player_inventory_draw(px((float)(left+102)),py((float)(top+150)),ps(60),
        (float)px(layout.mouse_x),(float)py(layout.mouse_y),recraft_screen_width(),recraft_screen_height());
    else container_label("Inventory",left+16,top+(s->kind==CONTAINER_CHEST ? rows*18+21 : 72)*2);
    inventory_clear_slot(&result);
    if (s->kind==CONTAINER_PLAYER || s->kind==CONTAINER_WORKBENCH)
        crafting_match(s->grid,s->kind==CONTAINER_WORKBENCH ? 3 : 2,&result);
    if(s->server) result=s->result;
    for (i=0;i<100;++i) if (container_xy(s,i,&x,&y)) {
        const InventorySlot *item=i<36 ? &inventory[i] : i<45 ? &s->grid[i-36] :
            i==45 ? &result : contents ? &contents[i-46] : NULL;
        draw_stack(item,x,y,31);
        if (inside(x,y,32,32)) rect(x,y,32,32,col(255,255,255,70));
    }
    draw_stack(&s->cursor,(int)layout.mouse_x-16,(int)layout.mouse_y-16,31);
}

void ui_creative_input(Ui *ui,InventorySlot *slots,int *hotbar,int can_give)
{
    int i,rows=(creative_count()+8)/9-5;
    begin_layout(ui);
    ui->creative_scroll-=GetMouseWheelMove();
    if (layout.clicked && inside(482,119,16,176)) ui->creative_drag=1;
    if (!IsMouseButtonDown(MOUSE_LEFT_BUTTON)) ui->creative_drag=0;
    if (ui->creative_drag)
        ui->creative_scroll=(int)((layout.mouse_y-119-12)*rows/152+0.5f);
    if (ui->creative_scroll<0) ui->creative_scroll=0;
    if (ui->creative_scroll>rows) ui->creative_scroll=rows;
    for (i=0;i<9;++i) {
        if (IsKeyPressed(KEY_ONE+i) || (layout.clicked && inside(150+i*36,325,32,32)))
            *hotbar=i;
    }
    if (layout.clicked) for (i=0;i<45;++i)
        if (inside(150+(i%9)*36,119+(i/9)*36,32,32))
            creative_give(slots,*hotbar,ui->creative_scroll*9+i,can_give);
}

void ui_draw_creative(const Ui *ui,const InventorySlot *slots,int hotbar)
{
    int i,hover=-1,rows=(creative_count()+8)/9-5;
    char caption[100];
    begin_layout(ui);
    DrawRectangle(0,0,recraft_screen_width(),recraft_screen_height(),col(0,0,0,150));
    /* Reuse the original panel border and slot recess, without duplicating
     * another atlas or stretching its icons/slots. */
    inventory_patch(4,4,1,1,134,81,372,290);
    inventory_patch(0,0,7,7,134,81,14,14);
    inventory_patch(169,0,7,7,492,81,14,14);
    inventory_patch(0,159,7,7,134,357,14,14);
    inventory_patch(169,159,7,7,492,357,14,14);
    inventory_patch(7,0,162,4,148,81,344,8);
    inventory_patch(7,162,162,4,148,363,344,8);
    inventory_patch(0,7,4,152,134,95,8,262);
    inventory_patch(172,7,4,152,498,95,8,262);
    label("Creative Inventory",150,96,12,col(64,64,64,255));
    for (i=0;i<45;++i) {
        InventorySlot item;
        int x=150+(i%9)*36,y=119+(i/9)*36;
        inventory_patch(7,83,18,18,x-2,y-2,36,36);
        if (creative_get(ui->creative_scroll*9+i,&item)) {
            draw_item_icon(item.id,item.damage,px((float)x),py((float)y),ps(32));
            if (inside(x,y,32,32)) {
                rect(x,y,32,32,col(255,255,255,70));
                hover=ui->creative_scroll*9+i;
            }
        }
    }
    rect(482,119,16,176,col(70,70,70,255));
    inventory_patch(7,0,20,20,482,119+(rows?ui->creative_scroll*152/rows:0),16,24);
    label("Hotbar",150,307,11,col(64,64,64,255));
    for (i=0;i<9;++i) {
        int x=150+i*36;
        inventory_patch(7,83,18,18,x-2,323,36,36);
        draw_stack(&slots[i],x,325,31);
        if (i==hotbar) linebox(x-2,323,36,36,col(255,235,120,255));
    }
    if (hover>=0) {
        InventorySlot item;
        const char *name;
        creative_get(hover,&item);
        name=language_item(item.id,item.damage);
        snprintf(caption,sizeof(caption),"%s",name);
        centered(caption,320,383,12,WHITE);
    }
    centered("Choose a hotbar slot, then an item. Scroll for more.",320,406,10,WHITE);
    centered("1-9 selects a slot. E closes.",320,424,10,WHITE);
}

int ui_draw_sign_editor(Ui *ui,const char lines[4][61],int *row)
{
    Texture2D texture=assets_get_texture(ASSET_SIGN);
    Rectangle source={2,2,24,12},destination;
    int i;
    begin_layout(ui);
    rect(0,0,640,480,col(0,0,0,150));
    centered("Edit sign message",320,74,20,WHITE);
    destination=(Rectangle){(float)px(176),(float)py(142),(float)ps(288),(float)ps(144)};
    DrawTexturePro(texture,source,destination,(Vector2){0,0},0,WHITE);
    for(i=0;i<4;++i) {
        char text[80];
        if(layout.clicked && inside(182,151+i*31,276,28)) *row=i;
        snprintf(text,sizeof(text),i==*row ? "> %s <" : "%s",lines[i]);
        centered(text,320,156+i*31,16,col(0,0,0,255));
    }
    centered("Up/Down selects a line. Enter selects the next line.",320,311,11,WHITE);
    return button(ui,220,366,200,29,"Done",1);
}

static int chat_wrap(const char *text,char output[6][512],int width,int height)
{
    const char *p=text; int row=0; size_t length=0;
    char format[4]={0};
    output[0][0]=0;
    while(*p && row<6) {
        const char *start=p; unsigned ch=MinecraftTextCodepoint(&p); size_t bytes=(size_t)(p-start);
        if(ch==0xa7 && *p) {
            unsigned code=MinecraftTextCodepoint(&p);
            bytes=(size_t)(p-start);
            if((code>='0' && code<='9') || (code>='a' && code<='f') ||
               (code>='A' && code<='F') || code=='r' || code=='R') {
                format[0]=(char)0xc2; format[1]=(char)0xa7; format[2]=(char)code;
            }
        }
        if(ch=='\n') {
            output[row][length]=0; if(++row>=6) break;
            strcpy(output[row],format); length=strlen(format); continue;
        }
        if(length+bytes>=512) break;
        memcpy(output[row]+length,start,bytes); output[row][length+bytes]=0;
        if(length && MeasureMinecraftText(output[row],height)>width) {
            output[row][length]=0;
            if(++row>=6) break;
            strcpy(output[row],format); length=strlen(format);
            memcpy(output[row]+length,start,bytes); length+=bytes; output[row][length]=0;
        } else length+=bytes;
    }
    return row<6 ? row+1 : 6;
}

void ui_draw_chat(const Ui *ui,const UiChatLine *lines,int count,const char *draft,int open,double now)
{
    int i,visible=0;
    begin_layout(ui);
    for(i=count-1;i>=0 && visible<10;--i) {
        char wrapped[6][512]; int j,n;
        double age=now-lines[i].arrived;
        unsigned char alpha=255;
        if(!open && age>=10) continue;
        if(!open && age>8) alpha=(unsigned char)((10-age)*127.5);
        n=chat_wrap(lines[i].text,wrapped,ps(400),ps(12));
        for(j=n-1;j>=0 && visible<10;--j,++visible) {
            int y=395-visible*15;
            rect(8,y-2,410,15,col(0,0,0,(unsigned char)(alpha*150/255)));
            label(wrapped[j],12,y,12,col(255,255,255,alpha));
        }
    }
    if(open) {
        char text[416];
        rect(8,412,624,24,col(0,0,0,210));
        snprintf(text,sizeof(text),"> %s_",draft ? draft : "");
        label_fit(text,12,416,14,614,WHITE);
    }
}

void ui_draw_player_list(const Ui *ui,const UiPlayerEntry *players,int count,int complete)
{
    int columns,rows,left,width,i;
    begin_layout(ui);
    if(count>80) count=80;
    if(count<=0) return;
    columns=(count+19)/20; rows=(count+columns-1)/columns;
    width=columns*142; left=(640-width)/2;
    rect(left-4,44,width+8,rows*19+30,col(0,0,0,180));
    centered(complete ? "Players" : "Nearby players (Beta has no global roster)",320,48,11,WHITE);
    for(i=0;i<count;++i) {
        int x=left+(i/rows)*142,y=66+(i%rows)*19;
        char ping[24];
        rect(x,y,140,18,col(70,70,70,90));
        label_fit(players[i].name,x+3,y+3,11,100,WHITE);
        if(players[i].ping_ms>=0) snprintf(ping,sizeof(ping),"%d ms",players[i].ping_ms);
        else snprintf(ping,sizeof(ping),"--");
        label_fit(ping,x+106,y+4,9,30,col(210,210,210,255));
    }
}
