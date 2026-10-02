#ifndef RECRAFT_SERVER_STATUS_H
#define RECRAFT_SERVER_STATUS_H

#include <stddef.h>
#include <stdint.h>

#define SERVER_STATUS_MAX_ENTRIES 64
#define SERVER_STATUS_ICON_MAX 16384
#define SERVER_STATUS_JSON_MAX 65536

typedef enum ServerStatusState {
    SERVER_STATUS_IDLE, SERVER_STATUS_QUEUED, SERVER_STATUS_QUERYING,
    SERVER_STATUS_ONLINE, SERVER_STATUS_ERROR
} ServerStatusState;
typedef struct ServerStatusResult {
    ServerStatusState state;
    int requested_protocol, protocol;
    int ping_ms, connect_ms, online, maximum, reachability_only;
    char motd[512], version[64], error[128];
    /* Validated 64x64 PNG bytes; upload on the render thread, never the worker. */
    unsigned char icon[SERVER_STATUS_ICON_MAX];
    size_t icon_size;
    unsigned revision;
} ServerStatusResult;
typedef struct ServerStatusBrowser ServerStatusBrowser;

/* Four bounded background queries. DNS never blocks the UI; canceled DNS jobs
 * retain their slot until the OS lookup returns, preventing refresh thread storms.
 * Protocol14 probes TCP only (vanilla Beta has no status or player-list packet).
 * Protocol47 uses the modern status handshake, JSON response and echoed pong. */
ServerStatusBrowser *server_status_create(void);
void server_status_destroy(ServerStatusBrowser *browser);
void server_status_clear(ServerStatusBrowser *browser);
int server_status_request(ServerStatusBrowser *browser, unsigned index,
                          const char *host, uint16_t port, int protocol);
void server_status_tick(ServerStatusBrowser *browser);
const ServerStatusResult *server_status_get(const ServerStatusBrowser *browser, unsigned index);
/* Dependency-free bounded parser, also used by the loopback regression test. */
int server_status_parse_json(const char *json, size_t length, ServerStatusResult *out);
/* Vanilla modern ping bars: unknown=-1, then 5/4/3/2/1 bars. */
int server_status_ping_bars(int milliseconds);

#endif
