#include "../src/nbt/nbt.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

typedef struct Observation { int begins, finishes, integers, bytes; } Observation;

static int observe(void *context, NbtEvent event, const NbtTag *tag, unsigned depth)
{
    Observation *o = (Observation *)context;
    if (event == NBT_BEGIN) ++o->begins;
    if (event == NBT_FINISH) ++o->finishes;
    if (event == NBT_VALUE && tag->type == NBT_INT) {
        assert(depth == 1 && tag->name.size == 6 && !memcmp(tag->name.data, "answer", 6));
        assert(tag->value.int_value == 42); ++o->integers;
    }
    if (event == NBT_VALUE && tag->type == NBT_BYTE) {
        assert(depth == 2 && tag->name.size == 0);
        assert(tag->value.byte == -3 || tag->value.byte == 7); ++o->bytes;
    }
    return 1;
}

int main(void)
{
    static const unsigned char expected[] = {
        10,0,0, 3,0,6,'a','n','s','w','e','r',0,0,0,42,
        9,0,5,'v','a','l','u','e',1,0,0,0,2,253,7,0
    };
    unsigned char encoded[128];
    NbtWriter writer;
    NbtTag tag;
    NbtLimits limit = nbt_default_limits();
    NbtError error;
    Observation seen = {0};
    size_t size = 0, i;
    memset(&tag, 0, sizeof(tag));
    nbt_writer_init(&writer, encoded, sizeof(encoded), NULL);
    tag.type = NBT_COMPOUND;
    assert(nbt_writer_tag(&writer, &tag) == NBT_OK);
    tag.type = NBT_INT; tag.name = nbt_span("answer"); tag.value.int_value = 42;
    assert(nbt_writer_tag(&writer, &tag) == NBT_OK);
    memset(&tag, 0, sizeof(tag));
    tag.type = NBT_LIST; tag.name = nbt_span("value"); tag.list_type = NBT_BYTE; tag.count = 2;
    assert(nbt_writer_tag(&writer, &tag) == NBT_OK);
    memset(&tag, 0, sizeof(tag));
    tag.type = NBT_BYTE; tag.value.byte = -3;
    assert(nbt_writer_tag(&writer, &tag) == NBT_OK);
    tag.value.byte = 7;
    assert(nbt_writer_tag(&writer, &tag) == NBT_OK);
    assert(nbt_writer_end(&writer) == NBT_OK);
    assert(nbt_writer_end(&writer) == NBT_OK);
    assert(nbt_writer_finish(&writer, &size) == NBT_OK);
    assert(size == sizeof(expected) && !memcmp(encoded, expected, size));
    assert(nbt_read(encoded, size, NULL, observe, &seen, &error) == NBT_OK);
    assert(seen.begins == 2 && seen.finishes == 2 && seen.integers == 1 && seen.bytes == 2);
    for (i = 0; i < size; ++i)
        assert(nbt_read(encoded, i, NULL, NULL, NULL, &error) == NBT_TRUNCATED);
    encoded[size] = 0;
    assert(nbt_read(encoded, size+1, NULL, NULL, NULL, &error) == NBT_TRAILING_DATA);
    limit.max_tags = 3;
    assert(nbt_read(expected, sizeof(expected), &limit, NULL, NULL, &error) == NBT_LIMIT);
    limit = nbt_default_limits(); limit.max_depth = 1;
    assert(nbt_read(expected, sizeof(expected), &limit, NULL, NULL, &error) == NBT_LIMIT);
    puts("NBT wire fixture, writer, bounded/truncated input: pass");
    return 0;
}
