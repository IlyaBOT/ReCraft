#ifndef RECRAFT_BETA_REGION_H
#define RECRAFT_BETA_REGION_H

#include "world.h"

/* McRegion chunk IO preserves unknown NBT tags and entity lists verbatim.
 * Only the four terrain arrays are changed when a loaded chunk is saved. */
WorldError beta_region_read_chunk(const World *world, Chunk *chunk);
WorldError beta_region_write_chunk(const World *world, const Chunk *chunk);

#endif
