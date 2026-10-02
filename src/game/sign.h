#ifndef RECRAFT_SIGN_H
#define RECRAFT_SIGN_H
#include "../world/world.h"

#define SIGN_LINES 4
#define SIGN_LINE_UNITS 15
#define SIGN_LINE_BYTES 61

/* Text is UTF-8 in memory. Beta stores at most 15 Java UTF-16 units per line. */
int sign_line_copy(char output[SIGN_LINE_BYTES],const char *input);
void sign_line_read_nbt(char output[SIGN_LINE_BYTES],const uint8_t *input,size_t size);
size_t sign_line_write_nbt(uint8_t *output,size_t capacity,const char *input);
int sign_is_block(unsigned id);
/* Clicked support coordinates and original Beta yaw in degrees, face 0..5.
 * No inventory mutation: the caller consumes the sign only after success. */
int sign_place(World *world,int x,int y,int z,unsigned face,float beta_yaw,
               int *placed_x,int *placed_y,int *placed_z);
void sign_neighbor_tick(World *world,int x,int y,int z);
/* Renderer query never loads chunks. NULL means no sign tile entity. */
const char (*sign_text_get(const World *world,int x,int y,int z))[SIGN_LINE_BYTES];
int sign_text_set(World *world,int x,int y,int z,
                   const char lines[SIGN_LINES][SIGN_LINE_BYTES]);
/* Only server updates may create/edit sign tile entities in network worlds. */
int sign_text_receive(World *world,int x,int y,int z,
                       const char lines[SIGN_LINES][SIGN_LINE_BYTES]);
#endif
