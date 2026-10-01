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
    NETWORK_EVENT_POSITION,
    NETWORK_EVENT_DISCONNECT,
    NETWORK_EVENT_CHUNK,
    NETWORK_EVENT_BLOCK,
    NETWORK_EVENT_ENTITY_SPAWN,
    NETWORK_EVENT_ENTITY_MOVE,
    NETWORK_EVENT_ENTITY_DESPAWN,
    NETWORK_EVENT_INVENTORY,
    NETWORK_EVENT_HEALTH
} NetworkEventType;

typedef struct NetworkEvent {
    NetworkEventType type;
    NetworkState state;
    const char *text;              /* Valid only during callback. */
    double x, y, z;                /* Feet position for POSITION. */
    float yaw, pitch;              /* Beta protocol degrees, not local radians. */
    int32_t entity_id;
    int32_t block_x, block_y, block_z;
    int16_t item_id, item_damage;
    int16_t slot, health;
    uint8_t block_id, metadata, entity_type, item_count;
} NetworkEvent;

typedef void (*NetworkEventFn)(void *user, const NetworkEvent *event);

/* World must have been initialized with world_init(). Caller retains ownership. */
NetworkClient *network_create(World *world, NetworkEventFn callback, void *user);
void network_destroy(NetworkClient *client);

/* Starts a bounded TCP connection; protocol 14 and offline-mode servers only. */
int network_connect(NetworkClient *client, const char *host, uint16_t port,
                    const char *username);
void network_disconnect(NetworkClient *client);
void network_tick(NetworkClient *client); /* Nonblocking, bounded packet budget. */

NetworkState network_state(const NetworkClient *client);
const char *network_last_error(const NetworkClient *client);
int network_send_position(NetworkClient *client, double x, double feet_y,
                          double z, float yaw, float pitch, int on_ground);
int network_send_chat(NetworkClient *client, const char *message);
/* Current hotbar index (0..8), sent as Beta 14 packet 0x10. */
int network_send_held_item(NetworkClient *client, int slot);
/* status: 0=start, 2=finish, 4=drop; face: 0..5. */
int network_mine_block(NetworkClient *client, int status, int x, int y,
                       int z, int face);
/* item_id=-1 means empty hand; face: 0..5, 255 for air click. */
int network_place_block(NetworkClient *client, int x, int y, int z, int face,
                        int item_id, int count, int damage);

#ifdef __cplusplus
}
#endif
#endif
