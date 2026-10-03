#ifndef RECRAFT_RESOURCE_PACK_H
#define RECRAFT_RESOURCE_PACK_H
#include <stddef.h>
typedef struct ResourcePackEntry {
    char id[300],name[256],description[256];
} ResourcePackEntry;
void resource_pack_init(const char *game_root);
int resource_pack_list(ResourcePackEntry *out,int capacity);
int resource_pack_select(const char *id); /* Empty selects bundled assets. */
const char *resource_pack_current(void);
void *resource_pack_read(const char *relative,size_t *size); /* Caller frees. */
void *resource_pack_read_from(const char *id,const char *relative,size_t *size);
void resource_pack_shutdown(void);
#endif
