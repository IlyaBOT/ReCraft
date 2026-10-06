#ifndef RECRAFT_UI_H
#define RECRAFT_UI_H

#include <stddef.h>
#include <stdint.h>
#include "../game/inventory.h"
#include "../game/container.h"

#define UI_NAME_MAX 96
#define UI_ID_MAX 64
#define UI_ADDRESS_MAX 256
#define UI_MAX_WORLDS 128
#define UI_MAX_SERVERS 128

typedef enum UiScreen {
    UI_SCREEN_MAIN = 0,
    UI_SCREEN_WORLDS,
    UI_SCREEN_CREATE_WORLD,
    UI_SCREEN_WORLD_EDIT,
    UI_SCREEN_MULTIPLAYER,
    UI_SCREEN_SERVER_EDIT,
    UI_SCREEN_DIRECT_CONNECT,
    UI_SCREEN_OPTIONS,
    UI_SCREEN_VIDEO,
    UI_SCREEN_ENVIRONMENT,
    UI_SCREEN_LANGUAGES,
    UI_SCREEN_PACKS,
    UI_SCREEN_CONTROLS,
    UI_SCREEN_MULTIPLAYER_OPTIONS,
    UI_SCREEN_PROFILE,
    UI_SCREEN_PAUSE,
    UI_SCREEN_CONFIRM,
    UI_SCREEN_DEATH,
    UI_SCREEN_GAME
} UiScreen;

typedef enum UiActionType {
    UI_ACTION_NONE = 0,
    UI_ACTION_QUIT,
    UI_ACTION_OPEN_GITHUB,
    UI_ACTION_OPEN_PACK_FOLDER,
    UI_ACTION_CHOOSE_SKIN,
    UI_ACTION_MICROSOFT_LOGIN,
    UI_ACTION_MICROSOFT_CANCEL,
    UI_ACTION_MICROSOFT_LOGOUT,
    UI_ACTION_PLAY_WORLD,
    UI_ACTION_CREATE_WORLD,
    UI_ACTION_DELETE_WORLD,
    UI_ACTION_EDIT_WORLD,
    UI_ACTION_RECREATE_WORLD,
    UI_ACTION_JOIN_SERVER,
    UI_ACTION_SAVE_SERVER,
    UI_ACTION_DELETE_SERVER,
    UI_ACTION_REFRESH_SERVERS,
    UI_ACTION_RESUME,
    UI_ACTION_RESPAWN,
    UI_ACTION_RETURN_TO_MENU
} UiActionType;

typedef struct UiWorldEntry {
    char id[UI_ID_MAX];
    char name[UI_NAME_MAX];
    uint64_t seed;
    uint64_t last_played;
    int creative;
    int flat;
    int beta_format;          /* Beta 1.7.3 level.dat + McRegion .mcr */
    int spawn_x, spawn_y, spawn_z;
    unsigned region_files;
    int64_t world_time;
    int rain_time,thunder_time,raining,thundering;
    int has_bed,bed_x,bed_y,bed_z;
    int save_version,dimension;
    int has_player,player_sleeping;
    double player_x,player_y,player_z;
    float player_yaw,player_pitch;
} UiWorldEntry;

typedef struct UiServerEntry {
    char name[UI_NAME_MAX];
    char address[UI_ADDRESS_MAX];
    char motd[512];
    char version[64];
    int players;
    int max_players;
    int ping_ms;             /* -1 means unknown */
    int compatible;
    int hide_address;
    int protocol,query_state,connect_ms;
} UiServerEntry;

#define UI_CHAT_LINES 20
typedef struct UiChatLine { char text[512]; double arrived; } UiChatLine;
typedef struct UiPlayerEntry { char name[64]; int ping_ms; } UiPlayerEntry;

/* A field changing in this struct is a request to the platform/game layer.
   Unsupported visual features remain visible but disabled. */
typedef struct UiOptions {
    char player_name[17];    /* Offline Beta username; shared with Protocol 14. */
    char language[16],texture_pack[300];
    char skin[16];            /* default, classic, custom, microsoft. */
    int invert_mouse,sensitivity,fov,chat_visible;
    int difficulty;           /* 0 peaceful, 1 easy, 2 normal, 3 hard. */
    int sound_volume,music_volume; /* 0..100, independent. */
    int fancy_graphics;
    int render_distance;      /* 2..12 chunks */
    int smooth_lighting;      /* 0 off, 1 simple */
    int menu_blur;            /* Optional fixed-function pause background. */
    int max_framerate;        /* 0 when VSync is selected */
    int anaglyph;
    int view_bobbing;
    int gui_scale;            /* 0 auto, 1 small, 2 normal, 3 large */
    int brightness;           /* 0..100 */
    int clouds;               /* 0 off, 1 fast */
    int particles;            /* 0 minimal, 1 decreased, 2 all */
    int fullscreen;
    int vsync;
    int mipmaps;              /* 0..4 */
    int alternate_blocks;
    int entity_shadows;
    int use_vbo;              /* 0 auto, 1 on, 2 off */
    int vbo_budget_mb;        /* 4, 8, 16, 32 MiB requested VBO storage */
    int greedy_mesh;
    int chunk_build_budget;   /* meshes per frame, 1..8 */
    int fog;                  /* 0 fast, 1 off */
    int fancy_leaves;
    int colored_redstone;     /* Custom visual option, default OFF. */
    int reduced_transparency; /* Preserve essential alpha only; default OFF. */
    int dynamic_updates;      /* 1..8 */
    int debug_statistics;
} UiOptions;

typedef struct UiAction {
    UiActionType type;
    int index;                /* selected server index, or -1 */
    char world_id[UI_ID_MAX];
    char world_name[UI_NAME_MAX];
    char seed[64];
    int creative;
    int flat;
    int structures;
    char server_name[UI_NAME_MAX];
    char server_address[UI_ADDRESS_MAX];
    int hide_address;
    int server_protocol;
} UiAction;

typedef struct Ui {
    UiScreen screen;
    UiScreen previous_screen;
    UiOptions options;
    char player_name_input[17];
    int selected_world;
    int selected_server;
    int world_scroll;
    int server_scroll;
    int language_scroll,language_drag,pack_scroll,pack_drag;
    int slider_drag;
    UiScreen language_parent;
    int focus;
    int select_all;
    UiScreen options_parent;
    int create_more_options;
    int create_creative;
    int create_flat;
    int create_structures;
    int server_hide_address;
    int server_protocol;
    int editing_server;
    int pending_confirm;
    int click_sound;
    int mipmap_available;       /* Set by platform capability probe. */
    int vbo_available;          /* Set by platform capability probe. */
    int monitor_hz;             /* Current display refresh, or 60 fallback. */
    int world_background;
    int network_mode;
    int creative_scroll, creative_drag;
    char search[UI_NAME_MAX];
    char world_name[UI_NAME_MAX];
    char world_seed[64];
    char server_name[UI_NAME_MAX];
    char server_address[UI_ADDRESS_MAX];
    char status[160];
    char profile_status[160],auth_code[32];
    int auth_busy,auth_signed_in;
} Ui;

void ui_init(Ui *ui);
void ui_shutdown(void);
void ui_set_screen(Ui *ui, UiScreen screen);
/* Call inside BeginDrawing/EndDrawing. Input is handled once per call. */
UiAction ui_frame(Ui *ui, const UiWorldEntry *worlds, int world_count,
                  const UiServerEntry *servers, int server_count);
/* Draw after the 3D renderer, before EndDrawing. */
void ui_draw_hud(const Ui *ui, int selected_slot, const InventorySlot *hotbar,
                 const char *const labels[9],
                 int debug_visible, const char *debug_text,int health,int air,int hurt);
void ui_draw_container(const Ui *ui,const ContainerSession *session,const InventorySlot *inventory,
                       const InventorySlot *contents,int burn,int fuel,int cook);
int ui_container_slot_at(const Ui *ui,const ContainerSession *session);
void ui_creative_input(Ui *ui, InventorySlot *slots, int *hotbar, int can_give);
void ui_draw_creative(const Ui *ui, const InventorySlot *slots, int hotbar);

int ui_draw_sleep(Ui *ui,int ticks);
int ui_draw_sign_editor(Ui *ui,const char lines[4][61],int *row);
void ui_draw_chat(const Ui *ui,const UiChatLine *lines,int count,const char *draft,int open,double now);
void ui_draw_player_list(const Ui *ui,const UiPlayerEntry *players,int count,int complete);
#endif
