#include "beta_level_io.h"
#include "../nbt/nbt.h"
#include "../util/game_paths.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <zlib.h>

#define LEVEL_LIMIT (2u * 1024u * 1024u)

static int find_data(void *context, NbtEvent event, const NbtTag *tag, unsigned depth)
{
    if (depth == 1 && event == NBT_BEGIN && tag->type == NBT_COMPOUND &&
        tag->name.size == 4 && memcmp(tag->name.data, "Data", 4) == 0)
        *(int *)context = 1;
    return 1;
}

static int read_file(const char *path, unsigned char **out, size_t *out_size)
{
    FILE *probe;
    gzFile file;
    unsigned char *bytes;
    size_t size = 0;
    int n = -1, valid, has_data = 0, first, second;
    /* gzread also accepts uncompressed files; Minecraft's GZIP stream doesn't. */
    probe = fopen(path, "rb");
    if (!probe) return 0;
    first = fgetc(probe); second = fgetc(probe);
    fclose(probe);
    if (first != 0x1f || second != 0x8b) return 0;
    file = gzopen(path, "rb");
    if (!file) return 0;
    bytes = (unsigned char *)malloc(LEVEL_LIMIT + 1u);
    if (!bytes) { gzclose(file); return 0; }
    while (size <= LEVEL_LIMIT) {
        n = gzread(file, bytes + size, (unsigned)(LEVEL_LIMIT + 1u - size));
        if (n <= 0) break;
        size += (size_t)n;
    }
    valid = gzclose(file) == Z_OK && n == 0 && size <= LEVEL_LIMIT &&
        nbt_read(bytes, size, NULL, find_data, &has_data, NULL) == NBT_OK && has_data;
    if (!valid) { free(bytes); return 0; }
    *out = bytes; *out_size = size;
    return 1;
}

int beta_level_read(const char *world_path, unsigned char **bytes, size_t *size,
                    int *used_old)
{
    static const char *names[2] = { "level.dat", "level.dat_old" };
    char path[512];
    int i;
    if (!world_path || !bytes || !size) return 0;
    for (i = 0; i < 2; ++i) {
        if (!game_path_join(path, sizeof(path), world_path, names[i])) return 0;
        if (read_file(path, bytes, size)) {
            if (used_old) *used_old = i;
            return 1;
        }
    }
    return 0;
}
