#ifndef RECRAFT_SERVER_LIST_H
#define RECRAFT_SERVER_LIST_H

#include <stddef.h>
#include <stdint.h>

#define RECRAFT_MAX_SERVERS 64

typedef struct SavedServer {
    char name[64];
    char address[256];
    int hide_address;
    int protocol;             /* 14 Beta gameplay, 47 modern status only. */
} SavedServer;

typedef struct ServerList {
    SavedServer entries[RECRAFT_MAX_SERVERS];
    size_t count;
} ServerList;

void server_list_load(ServerList *list, const char *path);
int server_list_save(const ServerList *list, const char *path);
/* host[:port], [IPv6][:port], or bare IPv6; default port25565. */
int server_address_parse(const char *address,char *host,size_t capacity,uint16_t *port);

#endif
