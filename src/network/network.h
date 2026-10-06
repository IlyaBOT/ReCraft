#ifndef RECRAFT_NETWORK_H
#define RECRAFT_NETWORK_H

#include <stdint.h>
#include "../world/world.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct NetworkClient NetworkClient;

typedef enum NetworkState {
    NETWORK_DISCONNECTED = 0,
    NETWORK_CONNECTING,
    NETWORK_HANDSHAKE,
    NETWORK_LOGIN,
    NETWORK_PLAY,
    NETWORK_ERROR
} NetworkState;

typedef enum NetworkEventType {
    NETWORK_EVENT_STATE = 0,
    NETWORK_EVENT_CHAT,
    NETWORK_EVENT_POSITION, /* value bits: 1=position, 2=rotation; 0=both. */
    NETWORK_EVENT_DISCONNECT,
    NETWORK_EVENT_CHUNK,
    NETWORK_EVENT_BLOCK,
    NETWORK_EVENT_ENTITY_SPAWN,
    NETWORK_EVENT_ENTITY_MOVE,
    NETWORK_EVENT_ENTITY_DESPAWN,
    NETWORK_EVENT_INVENTORY,
    NETWORK_EVENT_HEALTH,
    NETWORK_EVENT_WINDOW_OPEN,NETWORK_EVENT_WINDOW_CLOSE,NETWORK_EVENT_WINDOW_PROPERTY,
    NETWORK_EVENT_WINDOW_TRANSACTION,NETWORK_EVENT_WINDOW_SYNC,
    NETWORK_EVENT_RESPAWN,NETWORK_EVENT_SIGN,NETWORK_EVENT_ENTITY_FLAGS,
    NETWORK_EVENT_NOTE,NETWORK_EVENT_ATTACH,NETWORK_EVENT_ENTITY_VELOCITY,
    NETWORK_EVENT_ENTITY_STATUS,NETWORK_EVENT_ENTITY_COLLECT,NETWORK_EVENT_ENTITY_METADATA
} NetworkEventType;

typedef struct NetworkEvent {
    NetworkEventType type;
    NetworkState state;
    const char *text;              /* Valid only during callback. */
    double x, y, z;                /* Feet position for POSITION. */
    float yaw, pitch;              /* Beta protocol degrees, not local radians. */
    int32_t entity_id;
    int32_t vehicle_id;
    double vx,vy,vz; /* Blocks per second, not protocol units. */
    int32_t block_x, block_y, block_z;
    int16_t item_id, item_damage;
    int16_t slot, health;
    uint8_t block_id, metadata, item_count, entity_variant;
    int entity_type; /* Living Beta IDs; 1000 arrow, 1001 cart, 1002 boat, 1008 Item. */
    int window_id,window_type,window_slots,property,value,action,accepted;
    int dimension;
    char sign_lines[4][61];
} NetworkEvent;

typedef void (*NetworkEventFn)(void *user, const NetworkEvent *event);

/* World must have been initialized with world_init(). Caller retains ownership. */
NetworkClient *network_create(World *world, NetworkEventFn callback, void *user);
void network_destroy(NetworkClient *client);

/* Starts a bounded protocol 14 TCP connection. Optional authentication is
 * polled without blocking the render thread; 0 pending, 1 success, -1 error. */
typedef int (*NetworkJoinFn)(void *,const char *server_id,char *error,size_t capacity);
void network_set_auth(NetworkClient *client,NetworkJoinFn join,void *context);
int network_connect(NetworkClient *client, const char *host, uint16_t port,
                    const char *username);
void network_disconnect(NetworkClient *client);
typedef struct NetworkStats { double tick_ms,chunk_ms; unsigned packets,chunks; size_t pending_bytes; } NetworkStats;
NetworkStats network_stats(const NetworkClient *client);
void network_tick(NetworkClient *client); /* Nonblocking, bounded packet budget. */

NetworkState network_state(const NetworkClient *client);
/* Physics waits for map data under the player's footprint after a teleport.
 * PreChunk and isolated block updates do not make an empty cache entry ready. */
int network_terrain_ready(const NetworkClient *client,double x,double z);
const char *network_last_error(const NetworkClient *client);
typedef struct NetworkPlayerInfo {
    char name[65];
    int ping_ms;                  /* -1: Beta14 has no latency/player-list packet. */
    int self;
    int32_t entity_id;
} NetworkPlayerInfo;
/* Beta14 exposes only self and currently tracked named entities, not the entire
 * server roster. Returns total known count, copies at most capacity entries. */
size_t network_player_list(const NetworkClient *client,NetworkPlayerInfo *out,size_t capacity);
int network_send_position(NetworkClient *client, double x, double feet_y,
                          double z, float yaw, float pitch, int on_ground);
int32_t network_vehicle_id(const NetworkClient *client); /* -1 when unmounted. */
int network_send_riding(NetworkClient *client,double vx,double vz,float yaw,float pitch,int ground);
int network_send_chat(NetworkClient *client, const char *message);
/* Current hotbar index (0..8), sent as Beta 14 packet 0x10. */
int network_send_held_item(NetworkClient *client, int slot);
/* status: 0=start, 1=cancel, 2=finish, 4=drop; face: 0..5. */
int network_mine_block(NetworkClient *client, int status, int x, int y,
                       int z, int face);
/* item_id=-1 means empty hand; face: 0..5, 255 for air click. */
int network_place_block(NetworkClient *client, int x, int y, int z, int face,
                        int item_id, int count, int damage);
int network_click_window(NetworkClient *client,int window,int slot,int button,int action,
                         int shift,InventorySlot expected);
int network_close_window(NetworkClient *client,int window);
int network_confirm_window(NetworkClient *client,int window,int action);
/* Ask the server to respawn in the current dimension. Completion is a RESPAWN
 * event followed by server health/position/inventory updates. */
int network_respawn(NetworkClient *client);
int network_send_sign_update(NetworkClient *client,int x,int y,int z,const char lines[4][61]);
/* Beta0x07 server-authoritative interaction/combat; attack=0 interact,1attack. */
int network_use_entity(NetworkClient *client,int32_t target_id,int attack);
int network_send_animation(NetworkClient *client,int animation);
/* 1 start sneaking,2 stop sneaking,3 leave bed (Beta has no sprint action). */
int network_send_player_action(NetworkClient *client,int action);

#ifdef __cplusplus
}
#endif
#endif
