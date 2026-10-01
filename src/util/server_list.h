#ifndef RECRAFT_SERVER_LIST_H
#define RECRAFT_SERVER_LIST_H

#include <stddef.h>

#define RECRAFT_MAX_SERVERS 64

typedef struct SavedServer {
    char name[64];
    char address[256];
    int hide_address;
} SavedServer;

typedef struct ServerList {
    SavedServer entries[RECRAFT_MAX_SERVERS];
    size_t count;
} ServerList;

void server_list_load(ServerList *list, const char *path);
int server_list_save(const ServerList *list, const char *path);

#endif
