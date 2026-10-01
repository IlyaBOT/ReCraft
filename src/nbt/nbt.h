#ifndef RECRAFT_NBT_H
#define RECRAFT_NBT_H

#include <stddef.h>
#include <stdint.h>

/* Uncompressed Java NBT, big endian. Compression and file IO are separate.
   Wire format reference: https://c4k3.github.io/wiki.vg/NBT.html
   No allocations; spans borrow the input for the duration of the read call.
   Names/strings preserve their encoded bytes (including modified UTF-8).
   The parser does not transcode or validate string encoding. */
#define NBT_MAX_DEPTH 64

typedef enum NbtType {
    NBT_END = 0, NBT_BYTE, NBT_SHORT, NBT_INT, NBT_LONG, NBT_FLOAT, NBT_DOUBLE,
    NBT_BYTE_ARRAY, NBT_STRING, NBT_LIST, NBT_COMPOUND, NBT_INT_ARRAY, NBT_LONG_ARRAY
} NbtType;

typedef enum NbtResult {
    NBT_OK = 0, NBT_INVALID_ARGUMENT, NBT_TRUNCATED, NBT_INVALID_TYPE,
    NBT_INVALID_LENGTH, NBT_LIMIT, NBT_TRAILING_DATA, NBT_CANCELLED,
    NBT_BUFFER_FULL, NBT_INVALID_STATE
} NbtResult;

typedef struct NbtSpan { const uint8_t *data; size_t size; } NbtSpan;

typedef struct NbtLimits {
    size_t max_bytes;             /* default: 16 MiB */
    size_t max_tags;              /* default: 1048576, excluding container END */
    uint32_t max_array_elements;  /* default: 1048576, also applies to lists */
    unsigned int max_string_bytes;/* default: 65535, also applies to names */
    unsigned int max_depth;       /* default: 64, root depth is zero */
} NbtLimits;

typedef struct NbtTag {
    NbtType type;
    NbtSpan name;                 /* Empty for list elements. */
    NbtType list_type;
    uint32_t count;               /* Elements in a list/array. */
    union {
        int8_t byte;
        int16_t short_value;
        int32_t int_value;
        int64_t long_value;
        float float_value;
        double double_value;
        NbtSpan bytes;            /* Strings or raw big endian array payload. */
    } value;
} NbtTag;

typedef enum NbtEvent { NBT_VALUE, NBT_BEGIN, NBT_FINISH } NbtEvent;
/* Returning zero cancels the parse. BEGIN and FINISH surround each container.
   Callbacks may already have run when a later malformed tag is discovered;
   validate with visitor=NULL first when transactional import is required. */
typedef int (*NbtVisitor)(void *context, NbtEvent event, const NbtTag *tag,
                          unsigned int depth);
typedef struct NbtError { NbtResult result; size_t offset; } NbtError;

NbtLimits nbt_default_limits(void);
NbtSpan nbt_span(const char *text);
const char *nbt_result_string(NbtResult result);
/* Requires exactly one named COMPOUND root, with no trailing bytes. A limits
   pointer may be NULL; individual zero limit fields select their defaults. */
NbtResult nbt_read(const void *data, size_t size, const NbtLimits *limits,
                   NbtVisitor visitor, void *context, NbtError *error);

/* Safe native-endian access to the borrowed array payload. */
int nbt_array_int(const NbtTag *tag, uint32_t index, int32_t *value);
int nbt_array_long(const NbtTag *tag, uint32_t index, int64_t *value);

typedef struct NbtWriteFrame {
    NbtType type, list_type;
    uint32_t remaining;
} NbtWriteFrame;

typedef struct NbtWriter {
    uint8_t *data;
    size_t size, capacity, tags;
    NbtLimits limits;
    NbtResult result;             /* Sticky: discard output after any error. */
    unsigned int depth;
    int root_written;
    NbtWriteFrame stack[NBT_MAX_DEPTH];
} NbtWriter;

void nbt_writer_init(NbtWriter *writer, void *buffer, size_t capacity,
                      const NbtLimits *limits);
/* Writes a scalar or opens a COMPOUND/LIST. Array bytes must already be big
   endian and have exactly count*element_width bytes. For list children use
   an empty name and exactly the declared element type. */
NbtResult nbt_writer_tag(NbtWriter *writer, const NbtTag *tag);
NbtResult nbt_writer_end(NbtWriter *writer);
/* Verifies that one root was written and all containers/list counts closed. */
NbtResult nbt_writer_finish(NbtWriter *writer, size_t *size);

#endif
