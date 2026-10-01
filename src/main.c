#include "config.h"
#include "raylib.h"
#include "world/world.h"
#include "world/beta_discovery.h"
#include "world/beta_region.h"
#include "world/beta_level.h"
#include "renderer/renderer.h"
#include "renderer/menu_background.h"
#include "game/player.h"
#include "game/entity_render.h"
#include "game/settings.h"
#include "game/inventory.h"
#include "game/creative.h"
#include "ui/ui.h"
#include "audio/audio.h"
#include "network/network.h"
#include "util/clock.h"
#include "util/server_list.h"
#include "util/game_paths.h"
#include "util/display.h"
#include "assets/assets.h"
#include "ui/pixel_font.h"
#include "GLFW/glfw3.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#ifdef __APPLE__
#include <OpenGL/gl.h>
#else
#include <GL/gl.h>
#endif

/* GLFW is the raylib platform backend. This avoids raylib 1.4's busy limiter. */
extern void glfwSwapInterval(int interval);
extern int stbi_write_png(const char *, int, int, int, const void *, int);

#define CACHE_CHUNKS 512
#define ENTITY_LIMIT 256
#define PI_F 3.14159265358979323846f
static const int beta_items[9] = { 1, 3, 2, 12, 4, 17, 18, 20, 50 };
static const char *const hotbar_labels[9] = {
    "Stone", "Dirt", "Grass", "Sand", "Cobble", "Wood", "Leaves", "Glass", "Torch" };

typedef RenderEntity Entity;

typedef struct App {
    Ui ui;
    UiOptions applied;
    RecraftDisplay display;
    Renderer *renderer;
    MenuBackground menu_background;
    AudioState audio;
    World world;
    Player player;
    Player previous_player;
    NetworkClient *network;
    int has_world, network_position, quit, chat_open, debug, skip_ui_frame;
    int health, entity_count, rendered_entities;
    Entity entities[ENTITY_LIMIT];
    ItemDrop drops[128];
    uint64_t prior_vbo_bytes, prior_client_bytes;
    uint64_t frame_vbo_bytes, frame_client_bytes;
    InventorySlot inventory[RECRAFT_INVENTORY_SLOTS];
    int inventory_open, inventory_pick;
    char inventory_labels[9][32];
    const char *inventory_label_ptrs[9];
    char chat[128], message[256];
    double message_until, step_time;
    Vector2 last_mouse;
    PlayerInput input;
    ServerList servers;
    UiServerEntry server_ui[RECRAFT_MAX_SERVERS];
    UiWorldEntry world_ui[UI_MAX_WORLDS];
    int world_count;
    char root[WORLD_PATH_MAX], saves[WORLD_PATH_MAX];
    char settings_path[WORLD_PATH_MAX], servers_path[WORLD_PATH_MAX];
    int transient;
} App;

typedef struct RunOptions {
    int frames, width, height, smoke, menu, no_audio, client_arrays, basic, fullscreen;
    int distance, mipmaps, smooth, blur, leaves, budget, vbo_budget, debug, profile_gpu;
    const char *benchmark, *csv, *capture, *screen, *connect, *data_dir, *world_id;
} RunOptions;

static void copy_text(char *dst, size_t size, const char *src)
{
    snprintf(dst, size, "%s", src ? src : "");
}

static int path_join(char *out, size_t capacity, const char *a, const char *b)
{
    int n = snprintf(out, capacity, "%s/%s", a, b);
    return n > 0 && (size_t)n < capacity;
}

static int setup_paths(App *app, const char *override)
{
    static const char *const directories[] = {
        "assets", "saves", "texturepacks", "resourcepacks", "shaderpacks",
        "screenshots", "logs", "config", "crash-reports"
    };
    char path[WORLD_PATH_MAX];
    size_t i;
    if (override) copy_text(app->root, sizeof(app->root), override);
    else if (!game_executable_root(app->root, sizeof(app->root))) return 0;
    if (!game_ensure_directory(app->root)) return 0;
    for (i = 0; i < sizeof(directories)/sizeof(directories[0]); ++i) {
        if (!game_path_join(path, sizeof(path), app->root, directories[i]) ||
            !game_ensure_directory(path)) return 0;
    }
    if (
        !path_join(app->saves, sizeof(app->saves), app->root, RECRAFT_SAVE_DIRECTORY) ||
        !path_join(path, sizeof(path), app->root, "config") ||
        !path_join(app->settings_path, sizeof(app->settings_path), path, RECRAFT_OPTIONS_FILE) ||
        !path_join(app->servers_path, sizeof(app->servers_path), path, RECRAFT_SERVER_FILE)) return 0;
    return 1;
}

static void refresh_lists(App *app)
{
    WorldInfo info[UI_MAX_WORLDS];
    size_t i, n = world_storage_list(app->saves, info, UI_MAX_WORLDS);
    app->world_count = (int)n;
    for (i = 0; i < n; ++i) {
        UiWorldEntry *entry = &app->world_ui[i];
        copy_text(entry->id, sizeof(entry->id), info[i].id);
        copy_text(entry->name, sizeof(entry->name), info[i].name);
        entry->seed = info[i].seed;
        entry->last_played = info[i].last_played;
        entry->creative = info[i].creative;
        entry->flat = info[i].flat;
        entry->beta_format = 0;
        entry->spawn_x = entry->spawn_y = entry->spawn_z = 0;
        entry->region_files = 0;
    }
    if (n < UI_MAX_WORLDS) {
        BetaWorldInfo beta[UI_MAX_WORLDS];
        size_t j, count = beta_world_discover(app->saves,beta,UI_MAX_WORLDS-n);
        for (j = 0; j < count; ++j) {
            UiWorldEntry *entry = &app->world_ui[n+j];
            memset(entry,0,sizeof(*entry));
            copy_text(entry->id,sizeof(entry->id),beta[j].directory);
            copy_text(entry->name,sizeof(entry->name),beta[j].name);
            entry->seed = (uint64_t)beta[j].seed;
            entry->last_played = beta[j].last_played;
            entry->beta_format = 1;
            entry->spawn_x = beta[j].spawn_x;
            entry->spawn_y = beta[j].spawn_y;
            entry->spawn_z = beta[j].spawn_z;
            entry->region_files = beta[j].region_files;
            entry->world_time = beta[j].world_time;
            entry->save_version = beta[j].save_version;
            entry->dimension = beta[j].dimension;
            entry->has_player=beta[j].has_player;
            entry->player_x=beta[j].player_x;
            entry->player_y=beta[j].player_y;
            entry->player_z=beta[j].player_z;
            entry->player_yaw=beta[j].player_yaw;
            entry->player_pitch=beta[j].player_pitch;
            fprintf(stderr,"Beta world: %s (spawn %d,%d,%d; %u .mcr regions)\n",
                entry->name,entry->spawn_x,entry->spawn_y,entry->spawn_z,entry->region_files);
        }
        app->world_count += (int)count;
    }
    for (i = 0; i < app->servers.count; ++i) {
        UiServerEntry *entry = &app->server_ui[i];
        memset(entry, 0, sizeof(*entry));
        copy_text(entry->name, sizeof(entry->name), app->servers.entries[i].name);
        copy_text(entry->address, sizeof(entry->address), app->servers.entries[i].address);
        copy_text(entry->version, sizeof(entry->version), "Beta 1.7.3 / 14");
        copy_text(entry->motd, sizeof(entry->motd), "Status not queried; offline-mode protocol 14");
        entry->ping_ms = -1;
        entry->compatible = 1;
        entry->hide_address = app->servers.entries[i].hide_address;
    }
}

static void notice(App *app, const char *text)
{
    copy_text(app->message, sizeof(app->message), text);
    copy_text(app->ui.status, sizeof(app->ui.status), text);
    app->message_until = recraft_now_seconds() + 8.0;
}

static void capture_cursor(App *app, int capture)
{
    if (capture) DisableCursor();
    else EnableCursor();
    app->last_mouse = GetMousePosition();
    memset(&app->input, 0, sizeof(app->input));
}

static void apply_options(App *app)
{
    UiOptions *o = &app->ui.options;
    RendererOptions r;
    if (!app->ui.mipmap_available) o->mipmaps = 0;
    if (memcmp(o, &app->applied, sizeof(*o)) == 0) return;
    menu_background_clear(&app->menu_background);
    r.vbo_mode = o->use_vbo;
    r.vbo_budget_mb = o->vbo_budget_mb;
    r.greedy = o->greedy_mesh;
    r.fog = o->fog == 0;
    r.mipmap = o->mipmaps;
    r.smooth_lighting = o->smooth_lighting;
    r.transparent_leaves = o->fancy_leaves;
    r.brightness = o->brightness;
    renderer_set_options(app->renderer, app->has_world ? &app->world : NULL, r);
    if (o->fullscreen != app->applied.fullscreen)
        recraft_display_fullscreen(&app->display,o->fullscreen);
    glfwSwapInterval(o->vsync ? 1 : 0);
    app->applied = *o;
}

static int save_player(App *app)
{
    char path[WORLD_PATH_MAX], temporary[WORLD_PATH_MAX];
    FILE *f;
    int ok;
    if (!app->world.persistent || app->world.beta_format) return 1;
    if (!path_join(path, sizeof(path), app->world.path, "player.txt") ||
        !path_join(temporary, sizeof(temporary), app->world.path, "player.tmp")) return 0;
    f = fopen(temporary, "wb");
    if (!f) return 0;
    ok = fprintf(f, "1 %.9g %.9g %.9g %.9g %.9g %d %d\n", app->player.x,
        app->player.y, app->player.z, app->player.yaw, app->player.pitch,
        app->player.selected_slot, app->player.flying) > 0;
    if (fclose(f) != 0) ok = 0;
    if (!ok) return 0;
#ifdef _WIN32
    return MoveFileExA(temporary, path, MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH) != 0;
#else
    return rename(temporary, path) == 0;
#endif
}

static void load_player(App *app)
{
    char path[WORLD_PATH_MAX];
    FILE *f;
    Player p = app->player;
    int version;
    if (!path_join(path, sizeof(path), app->world.path, "player.txt")) return;
    f = fopen(path, "rb");
    if (!f) return;
    if (fscanf(f, "%d %f %f %f %f %f %d %d", &version, &p.x, &p.y, &p.z,
        &p.yaw, &p.pitch, &p.selected_slot, &p.flying) == 8 && version == 1 &&
        isfinite(p.x) && isfinite(p.y) && isfinite(p.z) && isfinite(p.yaw) &&
        isfinite(p.pitch) && fabsf(p.x) < 10000000.0f && fabsf(p.z) < 10000000.0f &&
        p.y >= 0 && p.y < 256 && fabsf(p.pitch) <= 1.48f &&
        p.selected_slot >= 0 && p.selected_slot < 9) {
        p.flying = p.creative && p.flying;
        app->player = p;
    }
    fclose(f);
}

static int save_inventory(App *app)
{
    char path[WORLD_PATH_MAX],temp[WORLD_PATH_MAX];
    FILE *file;
    int i,ok=1;
    if (!app->world.persistent || app->world.beta_format) return 1;
    if (!path_join(path,sizeof(path),app->world.path,"inventory.txt") ||
        !path_join(temp,sizeof(temp),app->world.path,"inventory.tmp")) return 0;
    file=fopen(temp,"wb");
    if (!file) return 0;
    if (fprintf(file,"1\n")<0) ok=0;
    for (i=0;i<RECRAFT_INVENTORY_SLOTS;++i)
        if (fprintf(file,"%d %d %d\n",app->inventory[i].id,
                    app->inventory[i].count,app->inventory[i].damage)<0) ok=0;
    if (fclose(file)!=0) ok=0;
    if (!ok) return 0;
#ifdef _WIN32
    return MoveFileExA(temp,path,MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH)!=0;
#else
    return rename(temp,path)==0;
#endif
}

static void load_inventory(App *app)
{
    char path[WORLD_PATH_MAX];
    InventorySlot loaded[RECRAFT_INVENTORY_SLOTS];
    FILE *file;
    int i,version,valid=1;
    if (!path_join(path,sizeof(path),app->world.path,"inventory.txt")) return;
    file=fopen(path,"rb");
    if (!file) return;
    if (fscanf(file,"%d",&version)!=1 || version!=1) valid=0;
    for (i=0;i<RECRAFT_INVENTORY_SLOTS && valid;++i) {
        InventorySlot *item=&loaded[i];
        if (fscanf(file,"%d %d %d",&item->id,&item->count,&item->damage)!=3 ||
            item->id< -1 || item->id>32767 || item->count<0 ||
            item->count>64 || item->damage<0 || item->damage>65535 ||
            ((item->id<0)!=(item->count==0))) valid=0;
    }
    fclose(file);
    if (valid) memcpy(app->inventory,loaded,sizeof(loaded));
    else fprintf(stderr,"Invalid inventory in %s; using initial slots.\n",path);
}

static int leave_world(App *app)
{
    if (app->has_world) {
        BetaLevelState beta_state;
        if (app->world.beta_format) {
            memset(&beta_state,0,sizeof(beta_state));
            beta_state.x=app->player.x;
            beta_state.y=app->player.y;
            beta_state.z=app->player.z;
            beta_state.yaw=app->player.yaw*180.0f/PI_F+180.0f;
            beta_state.pitch=-app->player.pitch*180.0f/PI_F;
            beta_state.on_ground=app->player.on_ground;
            beta_state.world_time=app->world.beta_world_time;
            memcpy(beta_state.inventory,app->inventory,sizeof(beta_state.inventory));
        }
        if (!save_player(app) || !save_inventory(app) || world_save(&app->world) != WORLD_OK ||
            (app->world.beta_format &&
             !beta_level_save(app->world.path,&beta_state))) {
            notice(app, "Save failed. World remains open; check disk space and permissions.");
            ui_set_screen(&app->ui, UI_SCREEN_PAUSE);
            capture_cursor(app, 0);
            return 0;
        }
        if (app->network) { network_destroy(app->network); app->network = NULL; }
        if (world_close(&app->world) != WORLD_OK) return 0;
        app->has_world = 0;
    }
    capture_cursor(app, 0);
    menu_background_clear(&app->menu_background);
    app->chat_open = 0;
    app->entity_count = 0;
    memset(app->entities, 0, sizeof(app->entities));
    memset(app->drops,0,sizeof(app->drops));
    memset(app->inventory, 0, sizeof(app->inventory));
    app->inventory_open=0;
    app->inventory_pick=-1;
    return 1;
}

static void enter_world(App *app)
{
    app->has_world = 1;
    player_spawn(&app->player, &app->world, app->world.creative);
    if (app->world.beta_format && app->world.beta_has_player) {
        app->player.x=(float)app->world.beta_player_x;
        app->player.y=(float)app->world.beta_player_y;
        app->player.z=(float)app->world.beta_player_z;
        app->player.yaw=(app->world.beta_player_yaw-180.0f)*PI_F/180.0f;
        app->player.pitch=-app->world.beta_player_pitch*PI_F/180.0f;
    }
    app->previous_player=app->player;
    inventory_init(app->inventory,app->world.creative);
    if (app->world.beta_format &&
        !beta_world_read_inventory(app->world.path,app->inventory))
        fprintf(stderr,"Could not read Beta inventory from %s/level.dat.\n",
                app->world.path);
    app->inventory_open=0;
    app->inventory_pick=-1;
    if (app->world.persistent && !app->world.beta_format) { load_player(app); load_inventory(app); }
    app->previous_player=app->player;
    ui_set_screen(&app->ui, UI_SCREEN_GAME);
    capture_cursor(app, 1);
}

static uint64_t seed_value(const char *text)
{
    char *end;
    uint64_t value;
    const unsigned char *p;
    if (!text[0]) return (uint64_t)time(NULL);
    value = (uint64_t)strtoull(text, &end, 10);
    if (!*end) return value;
    value = UINT64_C(1469598103934665603);
    for (p = (const unsigned char *)text; *p; ++p) { value ^= *p; value *= UINT64_C(1099511628211); }
    return value;
}

static void network_event(void *user, const NetworkEvent *event)
{
    App *app = (App *)user;
    int i, free_slot = -1;
    if (event->type == NETWORK_EVENT_POSITION) {
        app->previous_player=app->player;
        app->player.x = (float)event->x;
        app->player.y = (float)event->y;
        app->player.z = (float)event->z;
        app->player.yaw = (event->yaw - 180.0f) * PI_F / 180.0f;
        app->player.pitch = -event->pitch * PI_F / 180.0f;
        app->player.vx = app->player.vy = app->player.vz = 0;
        app->network_position = 1;
    } else if (event->type == NETWORK_EVENT_CHAT) notice(app, event->text);
    else if (event->type == NETWORK_EVENT_DISCONNECT) {
        notice(app, event->text);
        ui_set_screen(&app->ui, UI_SCREEN_PAUSE);
        capture_cursor(app, 0);
    } else if (event->type == NETWORK_EVENT_HEALTH) app->health = event->health;
    else if (event->type == NETWORK_EVENT_INVENTORY && event->entity_type == 0 &&
             event->slot >= 9 && event->slot < 45) {
        int index=event->slot>=36 ? event->slot-36 : event->slot;
        InventorySlot *item = &app->inventory[index];
        item->id = event->item_id;
        item->count = event->item_count;
        item->damage = event->item_damage;
    }
    else if (event->type == NETWORK_EVENT_ENTITY_SPAWN || event->type == NETWORK_EVENT_ENTITY_MOVE ||
             event->type == NETWORK_EVENT_ENTITY_DESPAWN) {
        for (i = 0; i < ENTITY_LIMIT; ++i) {
            Entity *e = &app->entities[i];
            if (!e->active && free_slot < 0) free_slot = i;
            if (e->active && e->id == event->entity_id) {
                if (event->type == NETWORK_EVENT_ENTITY_DESPAWN) { e->active = 0; --app->entity_count; }
                else { e->x = (float)event->x; e->y = (float)event->y; e->z = (float)event->z; }
                return;
            }
        }
        if (event->type == NETWORK_EVENT_ENTITY_SPAWN && free_slot >= 0) {
            Entity *e = &app->entities[free_slot];
            e->active = 1; e->id = event->entity_id; e->type = event->entity_type;
            e->x = (float)event->x; e->y = (float)event->y; e->z = (float)event->z;
            ++app->entity_count;
        }
    }
}

static int parse_address(const char *address, char *host, size_t capacity, uint16_t *port)
{
    const char *colon = strrchr(address, ':');
    size_t n = colon ? (size_t)(colon - address) : strlen(address), i;
    if (n == 0 || n >= capacity) return 0;
    for (i = 0; i < n; ++i) if ((unsigned char)address[i] <= 32 || address[i] == ':') return 0;
    memcpy(host, address, n); host[n] = 0;
    *port = 25565;
    if (colon) {
        char *end;
        unsigned long p = strtoul(colon + 1, &end, 10);
        if (!colon[1] || *end || p == 0 || p > 65535) return 0;
        *port = (uint16_t)p;
    }
    return 1;
}

static void join_server(App *app, const char *address)
{
    char host[256];
    uint16_t port;
    if (!parse_address(address, host, sizeof(host), &port)) {
        notice(app, "Use hostname[:port] or IPv4[:port]."); return;
    }
    if (!leave_world(app)) return;
    if (world_init(&app->world, 0, 0, CACHE_CHUNKS) != WORLD_OK) {
        notice(app, "Unable to allocate network world."); return;
    }
    app->world.network_mode = 1;
    app->has_world = 1;
    memset(&app->player, 0, sizeof(app->player));
    app->network_position = 0;
    {
        int i;
        for (i = 0; i < RECRAFT_INVENTORY_SLOTS; ++i) app->inventory[i].id = -1;
    }
    app->network = network_create(&app->world, network_event, app);
    if (!app->network || !network_connect(app->network, host, port, RECRAFT_OFFLINE_NAME)) {
        notice(app, app->network ? network_last_error(app->network) : "Network allocation failed.");
        leave_world(app); return;
    }
    notice(app, "Connecting to offline-mode Beta 1.7.3 server...");
    ui_set_screen(&app->ui, UI_SCREEN_GAME);
    capture_cursor(app, 1);
}

static WorldError open_beta_world(App *app,const UiWorldEntry *entry)
{
    WorldError error;
    char path[WORLD_PATH_MAX];
    if (!entry || !entry->beta_format ||
        entry->dimension!=0 ||
        !path_join(path,sizeof(path),app->saves,entry->id))
        return WORLD_ERROR_INVALID_ARGUMENT;
    error=world_init(&app->world,entry->seed,0,CACHE_CHUNKS);
    if (error!=WORLD_OK) return error;
    copy_text(app->world.path,sizeof(app->world.path),path);
    copy_text(app->world.id,sizeof(app->world.id),entry->id);
    copy_text(app->world.name,sizeof(app->world.name),entry->name);
    app->world.persistent=1;
    app->world.beta_format=1;
    app->world.beta_world_time=entry->world_time;
    app->world.spawn_x=entry->spawn_x;
    app->world.spawn_y=entry->spawn_y;
    app->world.spawn_z=entry->spawn_z;
    app->world.beta_has_player=entry->has_player;
    app->world.beta_player_x=entry->player_x;
    app->world.beta_player_y=entry->player_y;
    app->world.beta_player_z=entry->player_z;
    app->world.beta_player_yaw=entry->player_yaw;
    app->world.beta_player_pitch=entry->player_pitch;
    app->world.read_beta_chunk=beta_region_read_chunk;
    app->world.write_beta_chunk=beta_region_write_chunk;
    return WORLD_OK;
}

static void handle_action(App *app, UiAction action)
{
    WorldError error = WORLD_OK;
    size_t index;
    switch (action.type) {
        case UI_ACTION_QUIT: if (leave_world(app)) app->quit = 1; break;
        case UI_ACTION_PLAY_WORLD:
            if (!leave_world(app)) break;
            if (action.index >= 0 && action.index < app->world_count &&
                app->world_ui[action.index].beta_format)
                error=open_beta_world(app,&app->world_ui[action.index]);
            else error = world_open(&app->world, app->saves, action.world_id, CACHE_CHUNKS);
            if (error == WORLD_OK) enter_world(app);
            break;
        case UI_ACTION_CREATE_WORLD: {
            char id[64];
            unsigned attempt;
            if (!leave_world(app)) break;
            for (attempt = 0; attempt < 1000; ++attempt) {
                snprintf(id, sizeof(id), "world-%lu-%u", (unsigned long)time(NULL), attempt);
                error = world_create(&app->world, app->saves, id, action.world_name,
                    seed_value(action.seed), action.flat, action.creative, action.structures, CACHE_CHUNKS);
                if (error != WORLD_ERROR_EXISTS) break;
            }
            if (error == WORLD_OK) enter_world(app);
            refresh_lists(app);
            break;
        }
        case UI_ACTION_DELETE_WORLD:
            if (action.index >= 0 && action.index < app->world_count &&
                app->world_ui[action.index].beta_format) break;
            error = world_storage_delete(app->saves, action.world_id);
            refresh_lists(app); break;
        case UI_ACTION_EDIT_WORLD:
            if (action.index >= 0 && action.index < app->world_count &&
                app->world_ui[action.index].beta_format) break;
            error = world_storage_rename(app->saves, action.world_id, action.world_name);
            refresh_lists(app); break;
        case UI_ACTION_RECREATE_WORLD:
            if (action.index >= 0 && action.index < app->world_count &&
                app->world_ui[action.index].beta_format) break;
            ui_set_screen(&app->ui, UI_SCREEN_CREATE_WORLD);
            copy_text(app->ui.world_name, sizeof(app->ui.world_name), action.world_name);
            copy_text(app->ui.world_seed, sizeof(app->ui.world_seed), action.seed);
            app->ui.create_creative = action.creative;
            app->ui.create_flat = action.flat;
            break;
        case UI_ACTION_JOIN_SERVER: join_server(app, action.server_address); break;
        case UI_ACTION_SAVE_SERVER:
            index = action.index < 0 ? app->servers.count : (size_t)action.index;
            if (index >= RECRAFT_MAX_SERVERS || index > app->servers.count) {
                notice(app, "Server list is full."); break;
            }
            copy_text(app->servers.entries[index].name, sizeof(app->servers.entries[index].name), action.server_name);
            copy_text(app->servers.entries[index].address, sizeof(app->servers.entries[index].address), action.server_address);
            app->servers.entries[index].hide_address = action.hide_address;
            if (index == app->servers.count) ++app->servers.count;
            if (!server_list_save(&app->servers, app->servers_path)) notice(app, "Unable to save server list.");
            refresh_lists(app); break;
        case UI_ACTION_DELETE_SERVER:
            if (action.index >= 0 && (size_t)action.index < app->servers.count) {
                index = (size_t)action.index;
                memmove(&app->servers.entries[index], &app->servers.entries[index+1],
                    (app->servers.count - index - 1)*sizeof(SavedServer));
                --app->servers.count;
                if (!server_list_save(&app->servers, app->servers_path)) notice(app, "Unable to save server list.");
                refresh_lists(app);
            }
            break;
        case UI_ACTION_REFRESH_SERVERS:
            refresh_lists(app); notice(app, "Saved servers reloaded. Legacy status ping is not implemented."); break;
        case UI_ACTION_RESUME:
            ui_set_screen(&app->ui, UI_SCREEN_GAME); capture_cursor(app, 1); break;
        case UI_ACTION_RETURN_TO_MENU:
            if (leave_world(app)) { ui_set_screen(&app->ui, UI_SCREEN_MAIN); refresh_lists(app); }
            break;
        default: break;
    }
    if (error != WORLD_OK) {
        char message[100];
        snprintf(message, sizeof(message), "World operation failed (error %d). Existing data was kept.", error);
        notice(app, message);
    }
}

/* Load nearest missing chunk, one per call. Touch the complete visible set so
   the LRU eviction policy retains it while the player moves. */
static int stream_chunk(App *app)
{
    int cx = (int)floorf(app->player.x / 16), cz = (int)floorf(app->player.z / 16);
    int x, z, best = 1000000, bx = 0, bz = 0, r = app->ui.options.render_distance;
    if (app->world.network_mode) return 0;
    for (z = -r; z <= r; ++z) for (x = -r; x <= r; ++x) {
        int distance = x*x + z*z;
        Chunk *chunk;
        if (distance > r*r) continue;
        chunk = world_peek_chunk(&app->world, cx+x, cz+z);
        if (chunk) world_touch_chunk(&app->world, chunk);
        else if (distance < best) { best = distance; bx = cx+x; bz = cz+z; }
    }
    return best < 1000000 && world_get_chunk(&app->world, bx, bz) != NULL;
}

static int hit_face(BlockHit hit)
{
    if (hit.place_y < hit.y) return 0;
    if (hit.place_y > hit.y) return 1;
    if (hit.place_z < hit.z) return 2;
    if (hit.place_z > hit.z) return 3;
    return hit.place_x < hit.x ? 4 : 5;
}

static void spawn_drop(App *app, int id, int x, int y, int z)
{
    int i;
    if (id<=0 || app->player.creative || app->network) return;
    for (i=0;i<128;++i) if (!app->drops[i].active) {
        app->drops[i].active=1;
        app->drops[i].id=id;
        app->drops[i].count=1;
        app->drops[i].x=(float)x+0.5f;
        app->drops[i].y=(float)y+0.35f;
        app->drops[i].z=(float)z+0.5f;
        app->drops[i].vy=0;
        app->drops[i].age=0;
        return;
    }
}

static void tick_drops(App *app, float dt)
{
    int i;
    for (i=0;i<128;++i) if (app->drops[i].active) {
        float dx,dz;
        int bx,by,bz;
        app->drops[i].age+=dt;
        app->drops[i].vy-=18.0f*dt;
        if (app->drops[i].vy< -20.0f) app->drops[i].vy=-20.0f;
        app->drops[i].y+=app->drops[i].vy*dt;
        bx=(int)floorf(app->drops[i].x);
        by=(int)floorf(app->drops[i].y-0.2f);
        bz=(int)floorf(app->drops[i].z);
        if (world_block_def(world_peek_block(&app->world,bx,by,bz))->solid) {
            app->drops[i].y=(float)by+1.2f;
            app->drops[i].vy=0;
        }
        dx=app->drops[i].x-app->player.x;
        dz=app->drops[i].z-app->player.z;
        if (app->drops[i].age>0.35f && dx*dx+dz*dz<1.4f &&
            fabsf(app->drops[i].y-(app->player.y+0.9f))<1.6f) {
            int added=inventory_add(app->inventory,app->drops[i].id,
                                    app->drops[i].count);
            app->drops[i].count-=added;
            if (app->drops[i].count<=0) app->drops[i].active=0;
        }
        if (app->drops[i].age>300.0f || app->drops[i].y< -32.0f)
            app->drops[i].active=0;
    }
}

static void game_input(App *app)
{
    Vector2 mouse = GetMousePosition();
    int i, wheel;
    if (IsKeyPressed(KEY_F3)) app->debug = !app->debug;
    if (app->chat_open) {
        size_t n = strlen(app->chat);
        int key = GetKeyPressed();
        if (key >= 32 && key < 127 && n + 1 < sizeof(app->chat)) { app->chat[n] = (char)key; app->chat[n+1] = 0; }
        if (IsKeyPressed(KEY_BACKSPACE) && n) app->chat[n-1] = 0;
        if (IsKeyPressed(KEY_ENTER)) {
            if (app->network && app->chat[0]) network_send_chat(app->network, app->chat);
            app->chat_open = 0; app->chat[0] = 0;
        }
        if (IsKeyPressed(KEY_ESCAPE)) app->chat_open = 0;
        memset(&app->input, 0, sizeof(app->input));
        app->last_mouse = mouse;
        return;
    }
    if (IsKeyPressed(KEY_ESCAPE)) {
        if (app->inventory_open) {
            app->inventory_open=0;
            app->inventory_pick=-1;
            capture_cursor(app,1);
            return;
        }
        ui_set_screen(&app->ui, UI_SCREEN_PAUSE);
        app->skip_ui_frame=1;
        capture_cursor(app, 0); return;
    }
    if (IsKeyPressed(KEY_E)) {
        app->inventory_open=!app->inventory_open;
        app->inventory_pick=-1;
        capture_cursor(app,!app->inventory_open);
        return;
    }
    if (app->inventory_open) {
        if (app->player.creative) {
            ui_creative_input(&app->ui,app->inventory,&app->player.selected_slot,!app->network);
        } else if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
            int slot=ui_inventory_slot_at(&app->ui);
            if (slot>=0 && !app->network) {
                if (app->inventory_pick<0) app->inventory_pick=slot;
                else {
                    inventory_swap(app->inventory,app->inventory_pick,slot);
                    app->inventory_pick=-1;
                }
            }
        }
        memset(&app->input,0,sizeof(app->input));
        app->last_mouse=mouse;
        return;
    }
    if (IsKeyPressed(KEY_T) && app->network) { app->chat_open = 1; app->chat[0] = 0; return; }
    app->input.look_dx += mouse.x - app->last_mouse.x;
    app->input.look_dy += mouse.y - app->last_mouse.y;
    app->last_mouse = mouse;
    /* raylib 1.4 typedefs bool as an enum whose arithmetic may be unsigned.
     * Normalize each key to signed int before subtracting opposite axes. */
    app->input.forward = (float)((IsKeyDown(KEY_W) ? 1 : 0) -
                                 (IsKeyDown(KEY_S) ? 1 : 0));
    app->input.strafe = (float)((IsKeyDown(KEY_D) ? 1 : 0) -
                                (IsKeyDown(KEY_A) ? 1 : 0));
    app->input.jump = IsKeyDown(KEY_SPACE);
    app->input.descend = IsKeyDown(KEY_LEFT_SHIFT);
    app->input.sprint = IsKeyDown(KEY_LEFT_CONTROL);
    if (IsKeyPressed(KEY_F) && app->player.creative && !app->network) {
        app->player.flying = !app->player.flying; app->player.vy = 0;
    }
    {
        int old_slot = app->player.selected_slot;
        for (i = 0; i < 9; ++i) if (IsKeyPressed(KEY_ONE+i)) app->player.selected_slot = i;
        wheel = GetMouseWheelMove();
        app->player.selected_slot = (app->player.selected_slot - wheel % 9 + 9) % 9;
        if (app->network && app->player.selected_slot != old_slot)
            network_send_held_item(app->network, app->player.selected_slot);
    }
    if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
        if (app->network) {
            BlockHit hit = player_raycast(&app->player, &app->world, 5);
            if (hit.hit) {
                network_mine_block(app->network, 0, hit.x, hit.y, hit.z, hit_face(hit));
                network_mine_block(app->network, 2, hit.x, hit.y, hit.z, hit_face(hit));
            }
        } else {
            BlockHit hit=player_raycast(&app->player,&app->world,5);
            if (hit.hit && player_break_block(&app->player,&app->world)) {
                spawn_drop(app,hit.block,hit.x,hit.y,hit.z);
                audio_play(&app->audio,RECRAFT_SOUND_BREAK);
            }
        }
    }
    if (IsMouseButtonPressed(MOUSE_RIGHT_BUTTON)) {
        if (app->network) {
            BlockHit hit = player_raycast(&app->player, &app->world, 5);
            const InventorySlot *item = &app->inventory[app->player.selected_slot];
            if (hit.hit) network_place_block(app->network, hit.x, hit.y, hit.z, hit_face(hit),
                item->id, item->count, item->damage);
        } else {
            InventorySlot *item=&app->inventory[app->player.selected_slot];
            if (item->id>0 && item->id<BETA_BLOCK_COUNT && item->count>0 &&
                player_place_block_state(&app->player,&app->world,
                    (BetaBlockState){(uint8_t)item->id,(uint8_t)item->damage})) {
                inventory_take(app->inventory,app->player.selected_slot,app->player.creative);
                audio_play(&app->audio,RECRAFT_SOUND_PLACE);
            }
        }
    }
}

static void tick_game(App *app)
{
    if (app->network && (!app->network_position || network_state(app->network) != NETWORK_PLAY ||
        !world_peek_chunk(&app->world, (int)floorf(app->player.x/16), (int)floorf(app->player.z/16)))) {
        app->input.look_dx = app->input.look_dy = 0;
        return;
    }
    app->previous_player=app->player;
    player_tick(&app->player, &app->world, &app->input, (float)RECRAFT_TICK_SECONDS);
    if (!app->network) {
        WorldDropEvent drop;
        if (app->world.beta_format) ++app->world.beta_world_time;
        world_step_physics(&app->world,64);
        while (world_take_drop(&app->world,&drop))
            spawn_drop(app,drop.id,drop.x,drop.y,drop.z);
        tick_drops(app,(float)RECRAFT_TICK_SECONDS);
    }
    app->input.look_dx = app->input.look_dy = 0;
    if (app->network) network_send_position(app->network, app->player.x, app->player.y,
        app->player.z, app->player.yaw*180.0f/PI_F+180.0f,
        -app->player.pitch*180.0f/PI_F, app->player.on_ground);
    if (app->player.on_ground && (app->input.forward || app->input.strafe)) {
        app->step_time += RECRAFT_TICK_SECONDS;
        if (app->step_time >= 0.42) { audio_play(&app->audio, RECRAFT_SOUND_STEP); app->step_time = 0; }
    } else app->step_time = 0;
}

static int capture_png(const char *path)
{
    int w = recraft_screen_width(), h = recraft_screen_height(), y, ok;
    unsigned char *pixels = (unsigned char *)malloc((size_t)w*h*3);
    unsigned char *row = (unsigned char *)malloc((size_t)w*3);
    GLint alignment;
    if (!pixels || !row) { free(pixels); free(row); return 0; }
    glGetIntegerv(GL_PACK_ALIGNMENT, &alignment);
    glPixelStorei(GL_PACK_ALIGNMENT, 1);
    glReadPixels(0, 0, w, h, GL_RGB, GL_UNSIGNED_BYTE, pixels);
    glPixelStorei(GL_PACK_ALIGNMENT, alignment);
    for (y = 0; y < h/2; ++y) {
        unsigned char *a = pixels + (size_t)y*w*3, *b = pixels + (size_t)(h-1-y)*w*3;
        memcpy(row, a, (size_t)w*3); memcpy(a, b, (size_t)w*3); memcpy(b, row, (size_t)w*3);
    }
    ok = stbi_write_png(path, w, h, 3, pixels, w*3);
    free(row); free(pixels); return ok;
}

static int capture_screenshot(App *app)
{
    char stamp[32], name[80], directory[WORLD_PATH_MAX], path[WORLD_PATH_MAX];
    time_t now = time(NULL);
    struct tm *local = localtime(&now);
    unsigned attempt;
    if (!local || !strftime(stamp,sizeof(stamp),"%Y-%m-%d_%H.%M.%S",local) ||
        !path_join(directory,sizeof(directory),app->root,"screenshots")) return 0;
    for (attempt = 0; attempt < 1000; ++attempt) {
        FILE *existing;
        snprintf(name,sizeof(name),"%s_%03u.png",stamp,attempt);
        if (!path_join(path,sizeof(path),directory,name)) return 0;
        existing = fopen(path,"rb");
        if (existing) { fclose(existing); continue; }
        return capture_png(path);
    }
    return 0;
}

static void benchmark_scene(App *app, const char *scene)
{
    int x, y, z;
    if (!strcmp(scene,"bench_stream")) return;
    while (stream_chunk(app)) { }
    if (!strcmp(scene,"bench_torch")) {
        world_set_block(&app->world,8,64,5,BLOCK_TORCH);
        world_set_metadata(&app->world,8,64,5,5);
        app->player.x=8.5f; app->player.y=64.0f; app->player.z=8.5f;
        app->player.yaw=0; app->player.pitch=-0.12f;
    } else if (!strcmp(scene, "bench_caves")) {
        for (z = -24; z <= 12; ++z) for (x = 3; x <= 13; ++x)
            for (y = 24; y <= 29; ++y) world_set_block(&app->world, x, y, z, BLOCK_AIR);
        for (z = -22; z < 12; z += 6) world_set_block(&app->world, 4, 25, z, BLOCK_TORCH);
        app->player.y = 24.05f; app->player.pitch = 0;
    } else if (!strcmp(scene, "bench_worstcase_transparency")) {
        for (z = -20; z <= 4; z += 2) for (x = -12; x < 29; x += 2)
            for (y = 64; y < 72; ++y) world_set_block(&app->world, x, y, z,
                (uint8_t)((x+z)%3 == 0 ? BLOCK_LEAVES : (y%2 ? BLOCK_GLASS : BLOCK_WATER)));
        app->player.y = 66; app->player.flying = 1;
    } else if (!strcmp(scene, "bench_forest")) {
        /* Fixed groves make the foliage workload independent of spawn terrain. */
        for (z = -24; z < 24; z += 8) for (x = -24; x < 24; x += 8) {
            int ground = 126, dx, dz;
            while (ground > 1 && !world_block_def(world_get_block(&app->world,x,ground,z))->solid) --ground;
            for (y = ground+1; y < ground+5 && y < 127; ++y) world_set_block(&app->world,x,y,z,BLOCK_WOOD);
            for (dx = -2; dx <= 2; ++dx) for (dz = -2; dz <= 2; ++dz)
                for (y = ground+4; y <= ground+6 && y < 128; ++y)
                    if (dx || dz || y > ground+4) world_set_block(&app->world,x+dx,y,z+dz,BLOCK_LEAVES);
        }
        app->player.y += 5; app->player.flying = 1;
    }
    if (strcmp(scene,"bench_torch"))
        app->player.pitch = !strcmp(scene, "bench_caves") ? 0 : -0.20f;
    while (renderer_rebuild_budget(app->renderer, &app->world, 0, 0, 8) > 0) { }
}

static int compare_double(const void *a, const void *b)
{
    double x = *(const double *)a, y = *(const double *)b;
    return x < y ? -1 : x > y ? 1 : 0;
}

static int parse_options(int argc, char **argv, RunOptions *o)
{
    int i;
    memset(o, 0, sizeof(*o)); o->width = 960; o->height = 720;
    o->blur = o->mipmaps = o->smooth = o->leaves = o->budget = o->vbo_budget = -1;
    for (i = 1; i < argc; ++i) {
        const char *s = argv[i];
        if (!strcmp(s, "--smoke-test")) o->smoke = 1;
        else if (!strcmp(s, "--menu-smoke")) o->menu = 1;
        else if (!strcmp(s, "--no-audio")) o->no_audio = 1;
        else if (!strcmp(s, "--client-arrays")) o->client_arrays = 1;
        else if (!strcmp(s, "--basic-mesh")) o->basic = 1;
        else if (!strcmp(s, "--debug")) o->debug = 1;
        else if (!strcmp(s, "--fullscreen")) o->fullscreen = 1;
        else if (!strcmp(s, "--profile-gpu")) o->profile_gpu = 1;
        else if (!strcmp(s, "--frames") && i+1 < argc) o->frames = atoi(argv[++i]);
        else if (!strcmp(s, "--distance") && i+1 < argc) o->distance = atoi(argv[++i]);
        else if (!strcmp(s, "--mipmaps") && i+1 < argc) o->mipmaps = atoi(argv[++i]);
        else if (!strcmp(s, "--menu-blur") && i+1 < argc) o->blur = atoi(argv[++i]);
        else if (!strcmp(s, "--smooth-lighting") && i+1 < argc) o->smooth = atoi(argv[++i]);
        else if (!strcmp(s, "--fancy-leaves") && i+1 < argc) o->leaves = atoi(argv[++i]);
        else if (!strcmp(s, "--chunk-budget") && i+1 < argc) o->budget = atoi(argv[++i]);
        else if (!strcmp(s, "--vbo-budget") && i+1 < argc) o->vbo_budget = atoi(argv[++i]);
        else if (!strcmp(s, "--window") && i+1 < argc) {
            if (sscanf(argv[++i], "%dx%d", &o->width, &o->height) != 2) return 0;
        } else if (!strcmp(s, "--benchmark") && i+1 < argc) o->benchmark = argv[++i];
        else if (!strcmp(s, "--csv") && i+1 < argc) o->csv = argv[++i];
        else if (!strcmp(s, "--capture") && i+1 < argc) o->capture = argv[++i];
        else if (!strcmp(s, "--screen") && i+1 < argc) { o->screen = argv[++i]; o->menu = 1; }
        else if (!strcmp(s, "--connect") && i+1 < argc) o->connect = argv[++i];
        else if (!strcmp(s, "--world") && i+1 < argc) o->world_id = argv[++i];
        else if (!strcmp(s, "--data-dir") && i+1 < argc) o->data_dir = argv[++i];
        else return 0;
    }
    if (o->benchmark && strcmp(o->benchmark,"bench_torch") &&
        strcmp(o->benchmark,"bench_stream") &&
        strcmp(o->benchmark,"bench_flat") && strcmp(o->benchmark,"bench_forest") &&
        strcmp(o->benchmark,"bench_caves") && strcmp(o->benchmark,"bench_chunk_updates") &&
        strcmp(o->benchmark,"bench_worstcase_transparency")) return 0;
    if ((o->smoke || o->menu) && !o->frames) o->frames = 120;
    if (o->benchmark && !o->frames) o->frames = 600;
    return o->width >= 640 && o->width <= 3840 && o->height >= 480 && o->height <= 2160 &&
        o->frames >= 0 && o->frames <= 1000000 && (o->distance == 0 ||
        (o->distance >= 2 && o->distance <= 12)) &&
        o->mipmaps >= -1 && o->mipmaps <= 4 && o->smooth >= -1 && o->smooth <= 1 && o->blur>=-1 && o->blur<=1 &&
        o->leaves >= -1 && o->leaves <= 1 && (o->budget == -1 ||
        (o->budget >= 1 && o->budget <= 8)) &&
        (o->vbo_budget == -1 || o->vbo_budget == 4 || o->vbo_budget == 8 ||
         o->vbo_budget == 16 || o->vbo_budget == 32);
}

int main(int argc, char **argv)
{
    App app;
    RunOptions run;
    double last, accumulator = 0, frame_ms = 0, update_ms = 0, render_ms = 0, mesh_ms = 0;
    double stream_ms=0,terrain_ms=0,hud_ms=0,swap_ms=0,gpu_wait_ms=0;
    double *samples = NULL;
    FILE *csv = NULL;
    unsigned frame = 0;
    int status = 0;
    if (argc == 2 && strcmp(argv[1], "--version") == 0) {
        puts(RECRAFT_TITLE " " RECRAFT_VERSION);
        return 0;
    }
    memset(&app, 0, sizeof(app));
    if (!parse_options(argc, argv, &run)) {
        fprintf(stderr, "Usage: ReCraft [--smoke-test|--menu-smoke|--benchmark bench_torch|bench_stream|bench_flat|bench_forest|bench_caves|bench_chunk_updates|bench_worstcase_transparency]\n"
            "  [--frames N] [--window 960x720] [--capture file.png] [--csv file.csv]\n"
            "  [--client-arrays] [--basic-mesh] [--distance 2..12] [--mipmaps 0..4]\n"
            "  [--smooth-lighting 0|1] [--menu-blur 0|1] [--fancy-leaves 0|1] [--chunk-budget 1..8] [--vbo-budget 4|8|16|32]\n"
            "  [--screen main|worlds|create|multiplayer|add|direct|video|inventory|pause] [--no-audio] [--debug] [--fullscreen]\n"
            "  [--connect host:port] [--world save-directory] [--data-dir directory] [--profile-gpu]\n");
        return 2;
    }
    app.transient = run.smoke || run.menu || run.benchmark;
    ui_init(&app.ui);
    if (!setup_paths(&app, run.data_dir)) { fprintf(stderr, "Unable to create data directory.\n"); return 1; }
    assets_init(app.root);
    if (!app.transient) settings_load(&app.ui.options, app.settings_path);
    if (run.fullscreen) app.ui.options.fullscreen=1;
    if (app.ui.options.vsync || app.ui.options.max_framerate == 0) {
        app.ui.options.vsync = 1;
        app.ui.options.max_framerate = 0;
    }
    server_list_load(&app.servers, app.servers_path);
    refresh_lists(&app);
    if (run.client_arrays) app.ui.options.use_vbo = 2;
    if (run.basic) app.ui.options.greedy_mesh = 0;
    if (run.distance >= 2) app.ui.options.render_distance = run.distance;
    if (run.mipmaps >= 0) app.ui.options.mipmaps = run.mipmaps;
    if (run.blur >= 0) app.ui.options.menu_blur = run.blur;
    if (run.smooth >= 0) app.ui.options.smooth_lighting = run.smooth;
    if (run.leaves >= 0) app.ui.options.fancy_leaves = run.leaves;
    if (run.budget >= 1) app.ui.options.chunk_build_budget = app.ui.options.dynamic_updates = run.budget;
    if (run.vbo_budget >= 0) app.ui.options.vbo_budget_mb = run.vbo_budget;
    if (run.benchmark) { app.ui.options.max_framerate = 0; app.ui.options.vsync = 0; }
    app.debug = run.debug;
    InitWindow(run.width, run.height, RECRAFT_TITLE);
    recraft_display_init(&app.display);
    {
        GLFWmonitor *monitor = glfwGetPrimaryMonitor();
        const GLFWvidmode *mode = monitor ? glfwGetVideoMode(monitor) : NULL;
        app.ui.monitor_hz = mode && mode->refreshRate > 0 ? mode->refreshRate : 60;
        if (app.ui.options.max_framerate > app.ui.monitor_hz) {
            app.ui.options.max_framerate = 0;
            app.ui.options.vsync = 1;
        }
    }
    SetExitKey(0);
    SetTargetFPS(-1);
    app.renderer = renderer_init();
    if (!app.renderer) { CloseWindow(); return 1; }
    app.ui.mipmap_available = renderer_capabilities(app.renderer).mipmap_level_control;
    app.ui.vbo_available = renderer_capabilities(app.renderer).vbo_functions;
    if (run.mipmaps > 0 && !app.ui.mipmap_available)
        fprintf(stderr, "Requested mipmap level is unavailable on this OpenGL context; using nearest filtering.\n");
    app.applied = app.ui.options;
    app.applied.fullscreen = 0;
    app.applied.brightness = -1;
    apply_options(&app);
    if (!run.no_audio) audio_init(&app.audio);
    if (run.smoke || run.benchmark) {
        if (world_init(&app.world, 42, !run.benchmark || strcmp(run.benchmark,"bench_forest") != 0,
            CACHE_CHUNKS) != WORLD_OK) { renderer_shutdown(app.renderer); CloseWindow(); return 1; }
        app.world.creative = 1;
        enter_world(&app);
        if (run.benchmark) {
            RendererStats baseline;
            benchmark_scene(&app, run.benchmark);
            baseline = renderer_stats(app.renderer);
            app.prior_vbo_bytes = baseline.vbo_upload_bytes_total;
            app.prior_client_bytes = baseline.client_vertex_bytes_total;
        }
    }
    if (run.connect) join_server(&app, run.connect);
    if (run.world_id) {
        int i,found=0;
        for (i=0;i<app.world_count;++i) if (!strcmp(app.world_ui[i].id,run.world_id)) {
            UiAction play;
            memset(&play,0,sizeof(play));
            play.type=UI_ACTION_PLAY_WORLD;
            play.index=i;
            copy_text(play.world_id,sizeof(play.world_id),app.world_ui[i].id);
            handle_action(&app,play);
            found=1;
            break;
        }
        if (!found || !app.has_world) {
            fprintf(stderr,"Unable to open world '%s'.\n",run.world_id);
            status=1; app.quit=1;
        }
    }
    if (run.screen) {
        if (!strcmp(run.screen,"worlds")) ui_set_screen(&app.ui,UI_SCREEN_WORLDS);
        else if (!strcmp(run.screen,"inventory") && app.has_world) {
            ui_set_screen(&app.ui,UI_SCREEN_GAME);
            app.inventory_open=1;
            capture_cursor(&app,0);
        }
        else if (!strcmp(run.screen,"pause") && app.has_world) {
            ui_set_screen(&app.ui,UI_SCREEN_PAUSE);
            capture_cursor(&app,0);
        }
        else if (!strcmp(run.screen,"create")) ui_set_screen(&app.ui,UI_SCREEN_CREATE_WORLD);
        else if (!strcmp(run.screen,"multiplayer")) ui_set_screen(&app.ui,UI_SCREEN_MULTIPLAYER);
        else if (!strcmp(run.screen,"add")) ui_set_screen(&app.ui,UI_SCREEN_SERVER_EDIT);
        else if (!strcmp(run.screen,"direct")) ui_set_screen(&app.ui,UI_SCREEN_DIRECT_CONNECT);
        else if (!strcmp(run.screen,"video")) ui_set_screen(&app.ui,UI_SCREEN_VIDEO);
    }
    if (run.csv) {
        csv = fopen(run.csv,"wb");
        if (!csv) { fprintf(stderr,"Cannot open CSV.\n"); status = 1; app.quit = 1; }
        else fprintf(csv,"frame,frame_ms,update_ms,render_ms,mesh_ms,loaded,visible,dirty,draw_calls,vertices,triangles,entities_drawn,world_bytes,mesh_cpu_bytes,vbo_storage_requested_bytes,texture_storage_requested_bytes,vbo,uploaded_bytes_frame,client_vertex_bytes_frame,vbo_budget_bytes,driver_reported_vram_bytes,stream_ms,terrain_ms,hud_ms,swap_ms,gpu_wait_ms\n");
    }
    if (run.benchmark) samples = (double *)calloc((size_t)run.frames,sizeof(double));
    last = recraft_now_seconds();
    while (!app.quit && (!run.frames || frame < (unsigned)run.frames)) {
        double start = recraft_now_seconds(), elapsed = start-last, phase;
        UiAction action;
        RendererStats stats;
        int scene_width=recraft_screen_width(),scene_height=recraft_screen_height();
        int draw_scene=1;
        char debug_text[2048];
        app.skip_ui_frame=0;
        stream_ms=terrain_ms=hud_ms=swap_ms=gpu_wait_ms=0;
        if (WindowShouldClose()) {
            if (leave_world(&app)) break;
        }
        last = start;
        if (elapsed > 0.25) elapsed = 0.25;
        if (app.network) network_tick(app.network);
        if (app.has_world && app.ui.screen == UI_SCREEN_GAME && !run.benchmark) game_input(&app);
        app.ui.world_background=app.has_world && app.ui.screen!=UI_SCREEN_GAME;
        if (app.ui.world_background) {
            memset(&app.input,0,sizeof(app.input));
            draw_scene=menu_background_prepare(&app.menu_background,scene_width,scene_height,
                app.ui.options.menu_blur,app.network!=NULL,&scene_width,&scene_height);
        } else menu_background_clear(&app.menu_background);
        phase = recraft_now_seconds();
        if (app.has_world && (app.ui.screen == UI_SCREEN_GAME || app.network)) {
            if (run.benchmark) {
                /* One simulation tick per sample: deterministic trajectory/workload. */
                if (strcmp(run.benchmark,"bench_torch"))
                    app.player.yaw = 0.45f + (float)frame*0.0015f;
                if (!strcmp(run.benchmark,"bench_chunk_updates"))
                    world_set_block(&app.world,(int)(frame%16),64,(int)((frame/16)%16),
                        ((frame/256)%2) ? BLOCK_AIR : BLOCK_STONE);
                if (!strcmp(run.benchmark,"bench_stream")) {
                    double began=recraft_now_seconds();
                    stream_chunk(&app);
                    stream_ms=(recraft_now_seconds()-began)*1000;
                }
            } else {
                accumulator += elapsed;
                while (accumulator >= RECRAFT_TICK_SECONDS) { tick_game(&app); accumulator -= RECRAFT_TICK_SECONDS; }
                {
                    double began=recraft_now_seconds();
                    stream_chunk(&app);
                    stream_ms=(recraft_now_seconds()-began)*1000;
                }
            }
        } else accumulator = 0;
        update_ms = (recraft_now_seconds()-phase)*1000;
        phase = recraft_now_seconds();
        if (app.has_world) {
            int built = 0, budget = app.ui.options.chunk_build_budget;
            if (budget > app.ui.options.dynamic_updates) budget = app.ui.options.dynamic_updates;
            while (built < budget) {
                if (!renderer_rebuild_budget(app.renderer,&app.world,(int)floorf(app.player.x/16),
                    (int)floorf(app.player.z/16),1)) break;
                ++built;
                draw_scene=1;
                if (recraft_now_seconds()-phase >= 0.003) break;
            }
        }
        mesh_ms = (recraft_now_seconds()-phase)*1000;
        phase = recraft_now_seconds();
        BeginDrawing();
        recraft_begin_2d();
        memset(&action,0,sizeof(action));
        if (app.has_world) {
            RendererCamera camera;
            BlockHit hit;
            double terrain_start=recraft_now_seconds(),hud_start;
            char vram[48], texture_report[48], gart[48];
            const char *vertex_stage;
            if (draw_scene) {
                camera.x=app.player.x; camera.y=app.player.y+1.62f; camera.z=app.player.z;
                camera.yaw=app.player.yaw; camera.pitch=app.player.pitch; camera.fov_y=70;
                if (!run.benchmark && !app.network && !app.ui.world_background) {
                    float alpha=(float)(accumulator/RECRAFT_TICK_SECONDS);
                    if (alpha<0) alpha=0;
                    if (alpha>1) alpha=1;
                    if (fabsf(app.player.x-app.previous_player.x)<16.0f &&
                        fabsf(app.player.y-app.previous_player.y)<16.0f &&
                        fabsf(app.player.z-app.previous_player.z)<16.0f) {
                        camera.x=app.previous_player.x+(app.player.x-app.previous_player.x)*alpha;
                        camera.y=app.previous_player.y+(app.player.y-app.previous_player.y)*alpha+1.62f;
                        camera.z=app.previous_player.z+(app.player.z-app.previous_player.z)*alpha;
                    }
                    camera.yaw=app.player.yaw+app.input.look_dx*0.0025f;
                    camera.pitch=app.player.pitch-app.input.look_dy*0.0025f;
                }
                if (app.ui.options.view_bobbing && app.player.on_ground && (app.input.forward || app.input.strafe))
                    camera.y += 0.035f*sinf((float)start*14);
                renderer_draw(app.renderer,&app.world,&camera,scene_width,scene_height,app.ui.options.render_distance);
                terrain_ms=(recraft_now_seconds()-terrain_start)*1000;
                app.rendered_entities = entity_render_draw(app.entities,ENTITY_LIMIT,&camera,
                    scene_width,scene_height,app.ui.options.render_distance);
                app.rendered_entities += item_drop_draw(app.drops,128,&camera,
                    scene_width,scene_height,app.ui.options.render_distance);
                hit = player_raycast(&app.player,&app.world,5);
                if (hit.hit && !app.ui.world_background) {
                    BetaBlockState state = { hit.block,
                        world_get_metadata(&app.world,hit.x,hit.y,hit.z) };
                    renderer_draw_selection(app.renderer,&camera,recraft_screen_width(),recraft_screen_height(),
                                            hit.x,hit.y,hit.z,state);
                }
                if (run.profile_gpu) {
                    double began=recraft_now_seconds();
                    glFinish();
                    gpu_wait_ms=(recraft_now_seconds()-began)*1000;
                }
                if (app.ui.world_background) menu_background_capture(&app.menu_background);
            }
            recraft_begin_2d();
            if (app.ui.world_background) menu_background_draw(&app.menu_background);
            if (app.ui.screen == UI_SCREEN_GAME) {
                stats = renderer_stats(app.renderer);
                app.frame_vbo_bytes = stats.vbo_upload_bytes_total - app.prior_vbo_bytes;
                app.frame_client_bytes = stats.client_vertex_bytes_total - app.prior_client_bytes;
                app.prior_vbo_bytes = stats.vbo_upload_bytes_total;
                app.prior_client_bytes = stats.client_vertex_bytes_total;
                if (stats.vram_report_valid)
                    snprintf(vram,sizeof(vram),"%.1f MiB (driver report)",stats.reported_vram_bytes/1048576.0);
                else copy_text(vram,sizeof(vram),"N/A");
                if (stats.texture_memory_report_valid)
                    snprintf(texture_report,sizeof(texture_report),"%.1f MiB (driver report)",
                        stats.reported_texture_memory_bytes/1048576.0);
                else copy_text(texture_report,sizeof(texture_report),"N/A");
                if (stats.gart_report_valid)
                    snprintf(gart,sizeof(gart),"%.1f MiB (driver report)",stats.reported_gart_bytes/1048576.0);
                else copy_text(gart,sizeof(gart),"N/A");
                vertex_stage = stats.gpu_vertex_processing == 0 ? "CPU/software" :
                    stats.gpu_vertex_processing > 0 ? "GPU" : "unknown";
                snprintf(debug_text,sizeof(debug_text),
                    RECRAFT_TITLE " | %.1f FPS  %.2f ms | fixed 20 Hz + interpolation\nCPU update %.2f ms  render %.2f ms  mesh %.2f ms\n"
                    "chunks %u loaded / %u visible / %lu dirty | terrain draws %u\nvertices %u  triangles %u | VBO %s\n"
                    "world %.2f MiB  mesh CPU %.2f MiB VBO request %.2f MiB  atlas request %.2f MiB\n"
                    "vertex processing %s | VBO budget %.1f MiB, fallback %u, failures %u\n"
                    "GL submitted this frame: client %.1f KiB, VBO upload %.1f KiB\n"
                    "driver VRAM %s | texture memory %s | GART %s\nGPU utilization N/A | GPU wait N/A\n"
                    "XYZ %.2f %.2f %.2f | chunk %d %d | %s\n%s\n%s\n%s\n"
                    "WASD move  Space jump  Shift down  Ctrl sprint  F fly (creative)\n1-9/wheel select  LMB break  RMB place  E inventory  Esc menu  F3 debug\n"
                    "network entities %d tracked / %d drawn  health %d",
                    frame_ms > 0 ? 1000/frame_ms : 0,frame_ms,update_ms,render_ms,mesh_ms,
                    stats.cached_chunks,stats.visible_chunks,(unsigned long)world_dirty_chunk_count(&app.world),stats.draw_calls,
                    stats.vertices_drawn,stats.triangles_drawn,stats.using_vbo?"enabled":"client arrays",
                    world_memory_bytes(&app.world)/1048576.0,stats.cpu_mesh_bytes/1048576.0,
                    stats.gpu_mesh_bytes/1048576.0,stats.texture_bytes/1048576.0,
                    vertex_stage,stats.vbo_budget_bytes/1048576.0,stats.vbo_budget_fallbacks_total,
                    stats.vbo_upload_failures_total,app.frame_client_bytes/1024.0,app.frame_vbo_bytes/1024.0,
                    vram,texture_report,gart,
                    app.player.x,app.player.y,app.player.z,(int)floorf(app.player.x/16),(int)floorf(app.player.z/16),
                    app.player.flying?"flying":"walking",stats.gpu_vendor,stats.gpu_renderer,stats.gpu_version,
                    app.entity_count,app.rendered_entities,app.health);
                {
                    int slot, known;
                    for (slot = 0; slot < 9; ++slot) {
                        const InventorySlot *item = &app.inventory[slot];
                        if (item->id < 0 || item->count <= 0) copy_text(app.inventory_labels[slot],
                            sizeof(app.inventory_labels[slot]), "Empty");
                        else {
                            const char *name = beta_item_name(item->id);
                            for (known = 0; known < 9; ++known)
                                if (item->id == beta_items[known]) name = hotbar_labels[known];
                            if (!name && item->id<BETA_BLOCK_COUNT) {
                                const BetaBlockDef *def=beta_block_find((unsigned)item->id);
                                if (def) name=def->name;
                            }
                            if (name) snprintf(app.inventory_labels[slot],sizeof(app.inventory_labels[slot]),
                                "%s x%d",name,item->count);
                            else snprintf(app.inventory_labels[slot],sizeof(app.inventory_labels[slot]),
                                "#%d x%d",item->id,item->count);
                        }
                        app.inventory_label_ptrs[slot] = app.inventory_labels[slot];
                    }
                }
                hud_start=recraft_now_seconds();
                if (!app.inventory_open)
                    ui_draw_hud(&app.ui,app.player.selected_slot,app.inventory,app.inventory_label_ptrs,
                        app.debug||app.ui.options.debug_statistics,debug_text);
                if (app.inventory_open) {
                    if (app.player.creative) ui_draw_creative(&app.ui,app.inventory,app.player.selected_slot);
                    else ui_draw_inventory(&app.ui,app.inventory,app.inventory_pick);
                }
                if (app.message_until > start) DrawMinecraftText(app.message,12,recraft_screen_height()-120,12,WHITE,1);
                if (app.chat_open) {
                    DrawRectangle(8,recraft_screen_height()-145,recraft_screen_width()-16,24,(Color){0,0,0,210});
                    DrawMinecraftText(app.chat,12,recraft_screen_height()-141,16,WHITE,1);
                }
                hud_ms=(recraft_now_seconds()-hud_start)*1000;
            }
        }
        if (app.ui.screen!=UI_SCREEN_GAME && !app.skip_ui_frame)
            action = ui_frame(&app.ui,app.world_ui,app.world_count,app.server_ui,(int)app.servers.count);
        if (IsKeyPressed(KEY_F2) && !capture_screenshot(&app))
            notice(&app,"Unable to save screenshot.");
        if (run.capture && run.frames && frame+1 == (unsigned)run.frames && !capture_png(run.capture)) status = 1;
        {
            double began=recraft_now_seconds();
            EndDrawing();
            swap_ms=(recraft_now_seconds()-began)*1000;
        }
        render_ms = (recraft_now_seconds()-phase)*1000;
        if (app.ui.click_sound) { audio_play(&app.audio,RECRAFT_SOUND_CLICK); app.ui.click_sound=0; }
        handle_action(&app,action);
        apply_options(&app);
        if (!run.benchmark && !app.ui.options.vsync && app.ui.options.max_framerate > 0)
            recraft_sleep_seconds(1.0/app.ui.options.max_framerate - (recraft_now_seconds()-start));
        frame_ms = (recraft_now_seconds()-start)*1000;
        stats = renderer_stats(app.renderer);
        if (samples) samples[frame]=frame_ms;
        if (csv) fprintf(csv,"%u,%.6f,%.6f,%.6f,%.6f,%u,%u,%lu,%u,%u,%u,%d,%lu,%llu,%llu,%llu,%d,%llu,%llu,%llu,%llu,%.6f,%.6f,%.6f,%.6f,%.6f\n",
            frame,frame_ms,update_ms,render_ms,mesh_ms,stats.cached_chunks,stats.visible_chunks,
            (unsigned long)(app.has_world?world_dirty_chunk_count(&app.world):0),stats.draw_calls,
            stats.vertices_drawn,stats.triangles_drawn,app.rendered_entities,
            (unsigned long)(app.has_world?world_memory_bytes(&app.world):0),
            (unsigned long long)stats.cpu_mesh_bytes,(unsigned long long)stats.gpu_mesh_bytes,
            (unsigned long long)stats.texture_bytes,stats.using_vbo,
            (unsigned long long)app.frame_vbo_bytes,(unsigned long long)app.frame_client_bytes,
            (unsigned long long)stats.vbo_budget_bytes,
            (unsigned long long)(stats.vram_report_valid?stats.reported_vram_bytes:0),
            stream_ms,terrain_ms,hud_ms,swap_ms,gpu_wait_ms);
        ++frame;
    }
    if (samples && frame) {
        unsigned i, slow_count=(frame+99)/100;
        double total=0,slow=0;
        for (i=0;i<frame;++i) total+=samples[i];
        qsort(samples,frame,sizeof(double),compare_double);
        for (i=frame-slow_count;i<frame;++i) slow+=samples[i];
        printf("BENCHMARK %s frames=%u avg_fps=%.3f one_percent_low=%.3f\n",run.benchmark,frame,
            total>0?1000*frame/total:0,slow>0?1000*slow_count/slow:0);
    }
    free(samples);
    if (csv && fclose(csv)!=0) status=1;
    if (!leave_world(&app)) { fprintf(stderr,"Unsaved world remains in memory; shutdown aborted.\n"); return 1; }
    if (!app.transient && !settings_save(&app.ui.options,app.settings_path)) status=1;
    audio_shutdown(&app.audio);
    ui_shutdown();
    renderer_shutdown(app.renderer);
    assets_shutdown();
    CloseWindow();
    printf("ReCraft finished: %u frames, status %d\n",frame,status);
    return status;
}
