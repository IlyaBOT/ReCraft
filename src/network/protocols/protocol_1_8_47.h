#ifndef RECRAFT_PROTOCOL_1_8_47_H
#define RECRAFT_PROTOCOL_1_8_47_H

#include <stddef.h>
#include <stdint.h>

#define P47_VERSION 47
#define P47_MAX_FRAME_BYTES 2097151u
#define P47_MAX_UNCOMPRESSED_BYTES 8388608u

typedef struct P47Frame {
    uint32_t packet_id;
    const uint8_t *payload; /* Borrowed from input or scratch until next call. */
    size_t payload_size;
    size_t frame_size;       /* Complete wire frame, including length VarInt. */
    int compressed;
} P47Frame;

/* 1=complete, 0=need more bytes, -1=malformed. VarInt values use 32-bit wire bits. */
int p47_read_varint(const uint8_t *bytes, size_t available,
                    uint32_t *value, size_t *consumed);
size_t p47_write_varint(uint8_t *out, size_t capacity, uint32_t value);

/* compression_threshold < 0 means pre-compression framing. >= 0 means the
   post-Set Compression envelope. Compressed frames inflate into caller-owned
   scratch; no heap allocation occurs while decoding. */
int p47_decode_frame(const uint8_t *bytes, size_t available,
                     int compression_threshold, uint8_t *scratch,
                     size_t scratch_capacity, P47Frame *out);

/* Write one complete frame. Returns byte count, or zero on invalid input,
   insufficient capacity, or a packet exceeding the protocol limits. */
size_t p47_encode_frame(uint8_t *out, size_t capacity,
                        int compression_threshold, uint32_t packet_id,
                        const uint8_t *payload, size_t payload_size);

#endif
