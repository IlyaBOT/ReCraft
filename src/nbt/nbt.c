#include "nbt.h"

#include <float.h>
#include <limits.h>
#include <string.h>

/* The targeted x86 platforms use IEEE-754; fail at build time otherwise. */
typedef char nbt_float_must_be_binary32[(sizeof(float) == 4 && FLT_RADIX == 2 && FLT_MANT_DIG == 24) ? 1 : -1];
typedef char nbt_double_must_be_binary64[(sizeof(double) == 8 && DBL_MANT_DIG == 53) ? 1 : -1];

typedef struct Reader {
    const uint8_t *data;
    size_t size, offset, tags;
    NbtLimits limits;
    NbtVisitor visitor;
    void *context;
    NbtResult result;
} Reader;

NbtLimits nbt_default_limits(void)
{
    NbtLimits limits;
    limits.max_bytes = 16u*1024u*1024u;
    limits.max_tags = 1048576u;
    limits.max_array_elements = 1048576u;
    limits.max_string_bytes = 65535u;
    limits.max_depth = NBT_MAX_DEPTH;
    return limits;
}

static NbtLimits normalized_limits(const NbtLimits *requested)
{
    NbtLimits limits = nbt_default_limits();
    if (requested) {
        if (requested->max_bytes) limits.max_bytes = requested->max_bytes;
        if (requested->max_tags) limits.max_tags = requested->max_tags;
        if (requested->max_array_elements) limits.max_array_elements = requested->max_array_elements;
        if (requested->max_string_bytes) limits.max_string_bytes = requested->max_string_bytes;
        if (requested->max_depth) limits.max_depth = requested->max_depth;
    }
    if (limits.max_depth > NBT_MAX_DEPTH) limits.max_depth = NBT_MAX_DEPTH;
    if (limits.max_string_bytes > 65535u) limits.max_string_bytes = 65535u;
    return limits;
}

NbtSpan nbt_span(const char *text)
{
    NbtSpan span;
    span.data = (const uint8_t *)text;
    span.size = text ? strlen(text) : 0;
    return span;
}

const char *nbt_result_string(NbtResult result)
{
    static const char *const names[] = {
        "OK", "invalid argument", "truncated NBT", "invalid tag type",
        "invalid length", "NBT limit exceeded", "trailing NBT data",
        "visitor cancelled", "output buffer full", "invalid writer state"
    };
    if ((unsigned int)result >= sizeof(names)/sizeof(names[0])) return "unknown NBT error";
    return names[result];
}

static uint64_t get_be(const uint8_t *data, size_t width)
{
    uint64_t value = 0;
    size_t i;
    for (i = 0; i < width; ++i) value = (value << 8) | data[i];
    return value;
}

/* Avoid implementation-defined conversion of an out-of-range unsigned value. */
static int64_t signed_bits(uint64_t bits, unsigned int width)
{
    uint64_t sign = UINT64_C(1) << (width-1);
    uint64_t mask = sign-1;
    if (!(bits & sign)) return (int64_t)bits;
    return -1 - (int64_t)((~bits) & (mask | sign));
}

static const uint8_t *take(Reader *reader, size_t bytes)
{
    const uint8_t *value;
    if (reader->result != NBT_OK) return NULL;
    if (bytes > reader->size-reader->offset) { reader->result = NBT_TRUNCATED; return NULL; }
    value = reader->data+reader->offset;
    reader->offset += bytes;
    return value;
}

static int read_string(Reader *reader, NbtSpan *span)
{
    const uint8_t *length = take(reader, 2);
    if (!length) return 0;
    span->size = (size_t)get_be(length, 2);
    if (span->size > reader->limits.max_string_bytes) { reader->result = NBT_LIMIT; return 0; }
    span->data = take(reader, span->size);
    return span->data != NULL;
}

static int visit(Reader *reader, NbtEvent event, const NbtTag *tag, unsigned int depth)
{
    if (reader->visitor && !reader->visitor(reader->context, event, tag, depth)) {
        reader->result = NBT_CANCELLED;
        return 0;
    }
    return 1;
}

static int payload(Reader *reader, NbtTag *tag, unsigned int depth)
{
    const uint8_t *bytes;
    unsigned int width = 0;
    uint32_t i;
    uint64_t bits;
    if (depth >= reader->limits.max_depth || reader->tags >= reader->limits.max_tags) {
        reader->result = NBT_LIMIT; return 0;
    }
    ++reader->tags;
    switch (tag->type) {
        case NBT_BYTE: width = 1; break;
        case NBT_SHORT: width = 2; break;
        case NBT_INT: case NBT_FLOAT: width = 4; break;
        case NBT_LONG: case NBT_DOUBLE: width = 8; break;
        case NBT_STRING:
            if (!read_string(reader, &tag->value.bytes)) return 0;
            return visit(reader, NBT_VALUE, tag, depth);
        case NBT_BYTE_ARRAY: case NBT_INT_ARRAY: case NBT_LONG_ARRAY:
            bytes = take(reader, 4);
            if (!bytes) return 0;
            tag->count = (uint32_t)get_be(bytes, 4);
            if (tag->count > INT32_MAX) { reader->result = NBT_INVALID_LENGTH; return 0; }
            if (tag->count > reader->limits.max_array_elements) { reader->result = NBT_LIMIT; return 0; }
            width = tag->type == NBT_BYTE_ARRAY ? 1 : tag->type == NBT_INT_ARRAY ? 4 : 8;
            if (tag->count > (reader->size-reader->offset)/width) { reader->result = NBT_TRUNCATED; return 0; }
            tag->value.bytes.size = (size_t)tag->count*width;
            tag->value.bytes.data = take(reader, tag->value.bytes.size);
            return tag->value.bytes.data && visit(reader, NBT_VALUE, tag, depth);
        case NBT_LIST:
            bytes = take(reader, 5);
            if (!bytes) return 0;
            tag->list_type = (NbtType)bytes[0];
            tag->count = (uint32_t)get_be(bytes+1, 4);
            /* The Java convention accepts non-positive lists as empty. */
            if (tag->count > INT32_MAX) tag->count = 0;
            if (tag->count && (tag->list_type <= NBT_END || tag->list_type > NBT_LONG_ARRAY)) {
                reader->result = NBT_INVALID_TYPE; return 0;
            }
            if (tag->count > reader->limits.max_array_elements ||
                tag->count > reader->limits.max_tags-reader->tags) {
                reader->result = NBT_LIMIT; return 0;
            }
            if (!visit(reader, NBT_BEGIN, tag, depth)) return 0;
            for (i = 0; i < tag->count; ++i) {
                NbtTag child;
                memset(&child, 0, sizeof(child));
                child.type = tag->list_type;
                if (!payload(reader, &child, depth+1)) return 0;
            }
            return visit(reader, NBT_FINISH, tag, depth);
        case NBT_COMPOUND:
            if (!visit(reader, NBT_BEGIN, tag, depth)) return 0;
            for (;;) {
                NbtTag child;
                bytes = take(reader, 1);
                if (!bytes) return 0;
                if (*bytes == NBT_END) break;
                if (*bytes > NBT_LONG_ARRAY) { reader->result = NBT_INVALID_TYPE; return 0; }
                memset(&child, 0, sizeof(child));
                child.type = (NbtType)*bytes;
                if (!read_string(reader, &child.name) || !payload(reader, &child, depth+1)) return 0;
            }
            return visit(reader, NBT_FINISH, tag, depth);
        default: reader->result = NBT_INVALID_TYPE; return 0;
    }
    bytes = take(reader, width);
    if (!bytes) return 0;
    bits = get_be(bytes, width);
    switch (tag->type) {
        case NBT_BYTE: tag->value.byte = (int8_t)signed_bits(bits, 8); break;
        case NBT_SHORT: tag->value.short_value = (int16_t)signed_bits(bits, 16); break;
        case NBT_INT: tag->value.int_value = (int32_t)signed_bits(bits, 32); break;
        case NBT_LONG: tag->value.long_value = signed_bits(bits, 64); break;
        case NBT_FLOAT: { uint32_t value = (uint32_t)bits; memcpy(&tag->value.float_value, &value, 4); } break;
        case NBT_DOUBLE: memcpy(&tag->value.double_value, &bits, 8); break;
        default: break;
    }
    return visit(reader, NBT_VALUE, tag, depth);
}

NbtResult nbt_read(const void *data, size_t size, const NbtLimits *limits,
                   NbtVisitor visitor, void *context, NbtError *error)
{
    Reader reader;
    NbtTag root;
    const uint8_t *type;
    memset(&reader, 0, sizeof(reader));
    memset(&root, 0, sizeof(root));
    reader.data = (const uint8_t *)data;
    reader.size = size;
    reader.limits = normalized_limits(limits);
    reader.visitor = visitor;
    reader.context = context;
    if (!data) reader.result = NBT_INVALID_ARGUMENT;
    else if (size > reader.limits.max_bytes) reader.result = NBT_LIMIT;
    else {
        type = take(&reader, 1);
        if (type && *type != NBT_COMPOUND) reader.result = NBT_INVALID_TYPE;
        else if (type && read_string(&reader, &root.name)) {
            root.type = NBT_COMPOUND;
            if (payload(&reader, &root, 0) && reader.offset != reader.size)
                reader.result = NBT_TRAILING_DATA;
        }
    }
    if (error) { error->result = reader.result; error->offset = reader.offset; }
    return reader.result;
}

int nbt_array_int(const NbtTag *tag, uint32_t index, int32_t *value)
{
    if (!tag || !value || tag->type != NBT_INT_ARRAY || index >= tag->count ||
        !tag->value.bytes.data || tag->count > tag->value.bytes.size/4) return 0;
    *value = (int32_t)signed_bits(get_be(tag->value.bytes.data+(size_t)index*4, 4), 32);
    return 1;
}

int nbt_array_long(const NbtTag *tag, uint32_t index, int64_t *value)
{
    if (!tag || !value || tag->type != NBT_LONG_ARRAY || index >= tag->count ||
        !tag->value.bytes.data || tag->count > tag->value.bytes.size/8) return 0;
    *value = signed_bits(get_be(tag->value.bytes.data+(size_t)index*8, 8), 64);
    return 1;
}

static NbtResult fail(NbtWriter *writer, NbtResult result)
{
    if (writer->result == NBT_OK) writer->result = result;
    return writer->result;
}

static int put(NbtWriter *writer, const void *bytes, size_t size)
{
    if (writer->result != NBT_OK) return 0;
    if (size && !bytes) { fail(writer, NBT_INVALID_ARGUMENT); return 0; }
    if (size > writer->limits.max_bytes-writer->size) { fail(writer, NBT_LIMIT); return 0; }
    if (size > writer->capacity-writer->size) { fail(writer, NBT_BUFFER_FULL); return 0; }
    if (size) memcpy(writer->data+writer->size, bytes, size);
    writer->size += size;
    return 1;
}

static int put_be(NbtWriter *writer, uint64_t bits, unsigned int width)
{
    uint8_t bytes[8];
    unsigned int i;
    for (i = 0; i < width; ++i) bytes[width-1-i] = (uint8_t)(bits >> (i*8));
    return put(writer, bytes, width);
}

static int put_string(NbtWriter *writer, NbtSpan span)
{
    if (span.size > writer->limits.max_string_bytes) { fail(writer, NBT_LIMIT); return 0; }
    return put_be(writer, span.size, 2) && put(writer, span.data, span.size);
}

void nbt_writer_init(NbtWriter *writer, void *buffer, size_t capacity,
                      const NbtLimits *limits)
{
    if (!writer) return;
    memset(writer, 0, sizeof(*writer));
    writer->data = (uint8_t *)buffer;
    writer->capacity = capacity;
    writer->limits = normalized_limits(limits);
    if (!buffer && capacity) writer->result = NBT_INVALID_ARGUMENT;
}

NbtResult nbt_writer_tag(NbtWriter *writer, const NbtTag *tag)
{
    unsigned int width = 0;
    uint64_t bits = 0;
    NbtWriteFrame *parent = NULL;
    if (!writer || !tag) return NBT_INVALID_ARGUMENT;
    if (writer->result != NBT_OK) return writer->result;
    if (tag->type <= NBT_END || tag->type > NBT_LONG_ARRAY) return fail(writer, NBT_INVALID_TYPE);
    if (writer->depth >= writer->limits.max_depth || writer->tags >= writer->limits.max_tags)
        return fail(writer, NBT_LIMIT);
    if (writer->depth) parent = &writer->stack[writer->depth-1];
    else {
        if (writer->root_written) return fail(writer, NBT_INVALID_STATE);
        if (tag->type != NBT_COMPOUND) return fail(writer, NBT_INVALID_TYPE);
        writer->root_written = 1;
    }
    if (parent && parent->type == NBT_LIST) {
        if (!parent->remaining || tag->type != parent->list_type || tag->name.size)
            return fail(writer, NBT_INVALID_STATE);
        --parent->remaining;
    } else if (!put_be(writer, tag->type, 1) || !put_string(writer, tag->name)) return writer->result;
    ++writer->tags;
    switch (tag->type) {
        case NBT_BYTE: bits = (uint8_t)tag->value.byte; width = 1; break;
        case NBT_SHORT: bits = (uint16_t)tag->value.short_value; width = 2; break;
        case NBT_INT: bits = (uint32_t)tag->value.int_value; width = 4; break;
        case NBT_LONG: bits = (uint64_t)tag->value.long_value; width = 8; break;
        case NBT_FLOAT: { uint32_t value; memcpy(&value, &tag->value.float_value, 4); bits = value; width = 4; } break;
        case NBT_DOUBLE: memcpy(&bits, &tag->value.double_value, 8); width = 8; break;
        case NBT_STRING: put_string(writer, tag->value.bytes); return writer->result;
        case NBT_BYTE_ARRAY: case NBT_INT_ARRAY: case NBT_LONG_ARRAY:
            width = tag->type == NBT_BYTE_ARRAY ? 1 : tag->type == NBT_INT_ARRAY ? 4 : 8;
            if (tag->count > INT32_MAX) return fail(writer, NBT_INVALID_LENGTH);
            if (tag->count > writer->limits.max_array_elements) return fail(writer, NBT_LIMIT);
            if (tag->value.bytes.size/width != tag->count || tag->value.bytes.size%width)
                return fail(writer, NBT_INVALID_LENGTH);
            if (put_be(writer, tag->count, 4)) put(writer, tag->value.bytes.data, tag->value.bytes.size);
            return writer->result;
        case NBT_LIST:
            if (tag->count > INT32_MAX) return fail(writer, NBT_INVALID_LENGTH);
            if ((tag->count && (tag->list_type <= NBT_END || tag->list_type > NBT_LONG_ARRAY)) ||
                (unsigned int)tag->list_type > 255u) return fail(writer, NBT_INVALID_TYPE);
            if (tag->count > writer->limits.max_array_elements ||
                tag->count > writer->limits.max_tags-writer->tags) return fail(writer, NBT_LIMIT);
            if (!put_be(writer, tag->list_type, 1) || !put_be(writer, tag->count, 4)) return writer->result;
            /* fall through */
        case NBT_COMPOUND:
            writer->stack[writer->depth].type = tag->type;
            writer->stack[writer->depth].list_type = tag->list_type;
            writer->stack[writer->depth].remaining = tag->count;
            ++writer->depth;
            return writer->result;
        default: return fail(writer, NBT_INVALID_TYPE);
    }
    put_be(writer, bits, width);
    return writer->result;
}

NbtResult nbt_writer_end(NbtWriter *writer)
{
    NbtWriteFrame *frame;
    if (!writer) return NBT_INVALID_ARGUMENT;
    if (writer->result != NBT_OK) return writer->result;
    if (!writer->depth) return fail(writer, NBT_INVALID_STATE);
    frame = &writer->stack[writer->depth-1];
    if (frame->type == NBT_LIST && frame->remaining) return fail(writer, NBT_INVALID_STATE);
    if (frame->type == NBT_COMPOUND && !put_be(writer, NBT_END, 1)) return writer->result;
    --writer->depth;
    return writer->result;
}

NbtResult nbt_writer_finish(NbtWriter *writer, size_t *size)
{
    if (size) *size = 0;
    if (!writer) return NBT_INVALID_ARGUMENT;
    if (writer->result != NBT_OK) return writer->result;
    if (!writer->root_written || writer->depth) return fail(writer, NBT_INVALID_STATE);
    if (size) *size = writer->size;
    return NBT_OK;
}
