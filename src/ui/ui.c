#include "ui.h"
#include "../game/crafting.h"
#include "../config.h"
#include "raylib.h"
#include "pixel_font.h"
#include "gui_button.h"
#include "../assets/assets.h"
#include "../world/beta_blocks.h"
#include "../game/creative.h"
#include "../game/entity_render.h"
#include "../util/display.h"

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
    size_t n;
    if (!s) return;
    snprintf(text, sizeof(text), "%s", s);
    n = strlen(text);
    if (MeasureMinecraftText(text, ps((float)size)) > ps((float)width)) {
        while (n > 3 && MeasureMinecraftText(text, ps((float)size)) > ps((float)width)) {
            --n;
            text[n] = '\0';
            text[n-1] = text[n-2] = text[n-3] = '.';
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
    gui_button_draw(px((float)x),py((float)y),ps((float)w),ps((float)h),caption,
        !enabled ? GUI_BUTTON_DISABLED : pressed ? GUI_BUTTON_PRESSED :
        hot ? GUI_BUTTON_HOVERED : GUI_BUTTON_NORMAL);
    if (hot && layout.clicked) {
        ui->click_sound = 1;
        return 1;
    }
    return 0;
}

static void text_field(Ui *ui, int id, int x, int y, int w, int h,
                       char *value, size_t capacity, const char *hint)
{
    int active;
    const char *visible = value;
    if (layout.clicked && inside(x, y, w, h)) { ui->focus = id; ui->select_all = 0; }
    active = ui->focus == id;
    rect(x, y, w, h, col(14, 14, 16, 255));
    linebox(x, y, w, h, active ? col(202, 205, 207, 255) : col(107, 108, 109, 255));
    while (*visible && MeasureMinecraftText(visible, ps(11)) > ps((float)(w-16))) ++visible;
    if (active && ui->select_all && value[0])
        rect(x+4, y+3, w-8, h-6, col(57, 76, 113, 255));
    if (value[0]) label(visible, x+6, y+5, 11, col(245, 245, 241, 255));
    else label(hint, x+6, y+5, 11, col(111, 112, 113, 255));
    if (active) {
        int tw = MeasureMinecraftText(visible, ps(11));
        int cursor_x = px((float)(x+6)) + tw + ps(1);
        DrawRectangle(cursor_x, py((float)(y+5)), ps(1), ps(12), col(255, 255, 255, 255));
    }
    (void)capacity;
}

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
    Rectangle dst = {(float)layout.x,(float)layout.y,(float)ps(VW),(float)ps(VH)};
    Vector2 origin = {0,0};
    Color veil = dirt ? col(39,30,23,125) : col(17,22,24,115);
    ClearBackground(col(24,27,28,255));
    DrawTexturePro(texture,src,dst,origin,0,col(255,255,255,255));
    rect(0,0,VW,VH,veil);
}

static void panel(int x, int y, int w, int h)
{
    rect(x, y, w, h, col(17, 17, 18, 184));
    linebox(x, y, w, h, col(122, 119, 111, 160));
}

static void title(const char *s)
{
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
}

void ui_init(Ui *ui)
{
    if (!ui) return;
    memset(ui, 0, sizeof(*ui));
    ui->options.difficulty=2;
    ui->options.sound_volume=ui->options.music_volume=100;
    ui->screen = UI_SCREEN_MAIN;
    ui->previous_screen = UI_SCREEN_MAIN;
    ui->options_parent = UI_SCREEN_MAIN;
    ui->selected_world = -1;
    ui->selected_server = -1;
    ui->editing_server = -1;
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
    ui->focus = 0;
    ui->select_all = 0;
}

static UiAction main_menu(Ui *ui)
{
    UiAction action = empty_action();
    int x = 195, w = 250;
    draw_background(0);
    /* Block-like original wordmark, kept as vector geometry. */
    rect(150, 62, 340, 64, col(13, 16, 17, 165));
    rect(158, 70, 324, 48, col(44, 57, 48, 225));
    rect(158, 70, 324, 4, col(132, 157, 126, 255));
    rect(158, 114, 324, 4, col(24, 32, 29, 255));
    centered(RECRAFT_TITLE, 323, 76, 35, col(11, 14, 13, 255));
    centered(RECRAFT_TITLE, 320, 73, 35, col(234, 233, 210, 255));
    centered("A WORLD BUILT BLOCK BY BLOCK", 320, 134, 10,
             col(231, 230, 215, 255));
    if (button(ui, x, 192, w, 30, "Singleplayer", 1)) ui_set_screen(ui, UI_SCREEN_WORLDS);
    if (button(ui, x, 228, w, 30, "Multiplayer", 1)) ui_set_screen(ui, UI_SCREEN_MULTIPLAYER);
    if (button(ui, x, 292, w, 30, "Options...", 1)) {
        ui->options_parent = UI_SCREEN_MAIN; ui_set_screen(ui, UI_SCREEN_OPTIONS);
    }
    if (button(ui, x, 328, w, 30, "Quit Game", 1)) {
        ui->pending_confirm = 3; ui_set_screen(ui, UI_SCREEN_CONFIRM);
    }
    footer();
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

static void server_icon(int x, int y, int compatible)
{
    rect(x, y, 45, 45, col(44, 50, 62, 255));
    rect(x+7, y+9, 31, 10, col(122, 126, 132, 255));
    rect(x+7, y+24, 31, 10, col(94, 98, 103, 255));
    rect(x+11, y+12, 4, 4, compatible ? col(92, 198, 98, 255) : col(180, 148, 71, 255));
    rect(x+11, y+27, 4, 4, col(92, 198, 98, 255));
    linebox(x, y, 45, 45, col(17, 17, 19, 255));
}

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
        int index = ui->server_scroll+row, y = 69+row*68, bar;
        const UiServerEntry *server = &servers[index];
        char info[64];
        int strength = server->ping_ms < 0 ? 0 : server->ping_ms < 80 ? 5 :
                       server->ping_ms < 150 ? 4 : server->ping_ms < 300 ? 3 : 2;
        if (layout.clicked && inside(63, y, 506, 63)) {
            ui->selected_server = index;
            ui->focus = 0;
        }
        rect(63, y, 506, 63, ui->selected_server == index ?
             col(103, 106, 111, 230) : col(42, 43, 45, 215));
        if (ui->selected_server == index) linebox(63, y, 506, 63, col(213, 214, 207, 255));
        server_icon(70, y+8, server->compatible);
        label_fit(server->name, 124, y+5, 13, 304, col(247, 247, 240, 255));
        label_fit(server->motd[0] ? server->motd : "Status unknown", 124, y+23, 10,
                  375, col(197, 198, 190, 255));
        label_fit(server->hide_address ? "Address hidden" : server->address,
                  124, y+43, 9, 272, col(166, 167, 161, 255));
        if (server->max_players > 0) snprintf(info, sizeof(info), "%d/%d", server->players, server->max_players);
        else snprintf(info, sizeof(info), "?/?");
        label(info, 452, y+6, 10, col(211, 212, 206, 255));
        for (bar = 0; bar < 5; ++bar)
            rect(526+bar*6, y+21-bar*3, 4, 4+bar*3,
                 bar < strength ? col(102, 196, 91, 255) : col(79, 81, 78, 255));
        if (server->ping_ms < 0) label("?", 539, y+26, 9, col(212, 210, 180, 255));
        label_fit(server->version[0] ? server->version : "Beta 1.7.3", 403, y+43, 9,
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
    can_join = valid && (servers[ui->selected_server].compatible || servers[ui->selected_server].ping_ms < 0);
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
        ui->server_address[0] = '\0'; ui->server_hide_address = 0;
        ui_set_screen(ui, UI_SCREEN_SERVER_EDIT); ui->focus = 1;
    }
    if (button(ui, 67, 400, 117, 26, "Edit", valid)) {
        ui->editing_server = ui->selected_server;
        copy_text(ui->server_name, sizeof(ui->server_name), servers[ui->selected_server].name);
        copy_text(ui->server_address, sizeof(ui->server_address), servers[ui->selected_server].address);
        ui->server_hide_address = servers[ui->selected_server].hide_address;
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
    valid = address_present(ui->server_address) && (direct || ui->server_name[0]);
    if (button(ui, 145, 329, 170, 29, direct ? "Join Server" : "Done", valid) ||
        (IsKeyPressed(KEY_ENTER) && valid)) {
        action.type = direct ? UI_ACTION_JOIN_SERVER : UI_ACTION_SAVE_SERVER;
        action.index = direct ? -1 : ui->editing_server;
        copy_text(action.server_name, sizeof(action.server_name), direct ? "Direct Connection" : ui->server_name);
        copy_text(action.server_address, sizeof(action.server_address), ui->server_address);
        action.hide_address = ui->server_hide_address;
        if (!direct) ui_set_screen(ui, UI_SCREEN_MULTIPLAYER);
    }
    if (button(ui, 325, 329, 170, 29, "Cancel", 1)) ui_set_screen(ui, UI_SCREEN_MULTIPLAYER);
    if (ui->status[0]) label_fit(ui->status, 90, 399, 10, 460, col(255, 206, 162, 255));
    return action;
}

static UiAction options_menu(Ui *ui)
{
    UiAction action = empty_action();
    static const char *const difficulties[]={"Peaceful","Easy","Normal","Hard"};
    char text[64];
    draw_background(1);
    title("Options");
    if (button(ui, 175, 104, 290, 30, "Video Settings...", 1)) ui_set_screen(ui, UI_SCREEN_VIDEO);
    snprintf(text,sizeof(text),"Music: %d%%",ui->options.music_volume);
    if(button(ui,104,146,210,28,text,1)) ui->options.music_volume=ui->options.music_volume>=25 ? ui->options.music_volume-25 : 100;
    snprintf(text,sizeof(text),"Sound: %d%%",ui->options.sound_volume);
    if(button(ui,326,146,210,28,text,1)) ui->options.sound_volume=ui->options.sound_volume>=25 ? ui->options.sound_volume-25 : 100;
    snprintf(text,sizeof(text),"Difficulty: %s",difficulties[ui->options.difficulty]);
    if(button(ui,175,180,290,28,text,!ui->network_mode)) ui->options.difficulty=(ui->options.difficulty+1)%4;
    label("WASD: move    Mouse: look    Space: jump", 129, 212, 11, col(219, 220, 211, 255));
    label("Left mouse: break    Right mouse: place", 129, 235, 11, col(219, 220, 211, 255));
    label("1-9 / mouse wheel: hotbar    F3: statistics", 129, 258, 11, col(219, 220, 211, 255));
    label("F: free flight    Shift / Space: down / up", 129, 281, 11, col(219, 220, 211, 255));
    label("Escape: pause / back    Tab: next text field", 129, 304, 11, col(219, 220, 211, 255));
    if (button(ui, 220, 389, 200, 30, "Done", 1)) ui_set_screen(ui, ui->options_parent);
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
    if (button(ui, 220, 419, 200, 29, "Done", 1)) ui_set_screen(ui, UI_SCREEN_OPTIONS);
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
            case UI_SCREEN_VIDEO: ui_set_screen(ui, UI_SCREEN_OPTIONS); break;
            case UI_SCREEN_OPTIONS: ui_set_screen(ui, ui->options_parent); break;
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
        case UI_SCREEN_VIDEO: return video_menu(ui);
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
    tile_quad(top,tx,ty,255);
    tile_quad(side,lx,ly,175);
    tile_quad(side,rx,ry,135);
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
        draw_stack(hotbar ? &hotbar[i] : NULL,x+5,439,29);
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
    DrawMinecraftText(text,px((float)x),py((float)y),ps(16),col(64,64,64,255),0);
}

void ui_draw_container(const Ui *ui,const ContainerSession *s,const InventorySlot *inventory,
                       const InventorySlot *contents,int burn,int fuel,int cook)
{
    int rows=s->size/9,height=s->kind==CONTAINER_CHEST ? 114+rows*18 : 166;
    int left=144,top=(480-height*2)/2,i,x,y;
    AssetId id=s->kind==CONTAINER_WORKBENCH ? ASSET_GUI_CRAFTING :
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
        name=beta_item_name(item.id);
        if (!name && beta_block_find((unsigned)item.id)) name=beta_block_find((unsigned)item.id)->name;
        snprintf(caption,sizeof(caption),"%s (%d:%d)",name?name:"Block",item.id,item.damage);
        centered(caption,320,383,12,WHITE);
    }
    centered("Choose a hotbar slot, then an item. Scroll for more.",320,406,10,WHITE);
    centered("1-9 selects a slot. E closes.",320,424,10,WHITE);
}
