#ifndef RECRAFT_PROTOCOL_BETA_14_H
#define RECRAFT_PROTOCOL_BETA_14_H

#include <stddef.h>
#include <stdint.h>

#define BETA14_VERSION 14
#define BETA14_MAX_COMPRESSED 131072u
#define BETA14_MAX_PACKET (BETA14_MAX_COMPRESSED + 32u)

typedef struct Beta14Packet {
    uint8_t id;
    size_t size;                  /* Includes one-byte packet ID. */
    const uint8_t *bytes;         /* Borrowed from receive buffer. */
} Beta14Packet;

/* 1=complete, 0=need more data, -1=malformed/over limit, -2=unknown packet. */
int beta14_next_packet(const uint8_t *bytes, size_t available, Beta14Packet *out);

uint16_t beta14_u16(const uint8_t *p);
uint32_t beta14_u32(const uint8_t *p);
int32_t beta14_i32(const uint8_t *p);
double beta14_f64(const uint8_t *p);
float beta14_f32(const uint8_t *p);

/* Decode a String16 at offset. Returns byte offset after string, or 0. */
size_t beta14_read_string(const Beta14Packet *packet, size_t offset,
                          char *out, size_t capacity);

/* All encoders return encoded length, or zero for invalid/buffer too small. */
size_t beta14_handshake(uint8_t *out, size_t capacity, const char *username);
size_t beta14_login(uint8_t *out, size_t capacity, const char *username);
size_t beta14_movement(uint8_t *out, size_t capacity, double x, double feet_y,
                       double z, float yaw, float pitch, int on_ground);
size_t beta14_chat(uint8_t *out, size_t capacity, const char *message);
size_t beta14_held_item(uint8_t *out, size_t capacity, int slot);
size_t beta14_mine(uint8_t *out, size_t capacity, int status, int x, int y,
                   int z, int face);
size_t beta14_place(uint8_t *out, size_t capacity, int x, int y, int z,
                    int face, int item_id, int count, int damage);
size_t beta14_window_click(uint8_t *out,size_t capacity,int window,int slot,int button,
                           int action,int shift,int item,int count,int damage);

#endif
