#ifndef RECRAFT_BETA_LEVEL_IO_H
#define RECRAFT_BETA_LEVEL_IO_H

#include <stddef.h>

/* Read-only, bounded gzip/NBT load. Vanilla tries level.dat then level.dat_old.
 * Returns owned bytes on success; the caller frees them. Outputs are unchanged
 * on failure. Requires the root's Data compound, without changing either file. */
int beta_level_read(const char *world_path, unsigned char **bytes, size_t *size,
                    int *used_old);

#endif
