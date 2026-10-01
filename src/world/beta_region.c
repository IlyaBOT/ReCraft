#include "beta_region.h"
#include "../nbt/nbt.h"

#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <zlib.h>

#define REGION_SECTOR 4096u
#define REGION_HEADER 8192u
#define BETA_NBT_LIMIT (16u*1024u*1024u)
#define BETA_COMPRESSED_LIMIT (255u*REGION_SECTOR)

typedef struct ChunkTags {
    const uint8_t *raw;
    size_t size, offsets[4];
    unsigned found;
    int32_t x,z;
    unsigned has_x,has_z;
} ChunkTags;

static uint32_t be32(const uint8_t *p)
{
    return ((uint32_t)p[0]<<24)|((uint32_t)p[1]<<16)|
           ((uint32_t)p[2]<<8)|(uint32_t)p[3];
}

static void put_be32(uint8_t *p,uint32_t value)
{
    p[0]=(uint8_t)(value>>24); p[1]=(uint8_t)(value>>16);
    p[2]=(uint8_t)(value>>8); p[3]=(uint8_t)value;
}

static int32_t floor_div32(int32_t value)
{
    return value>=0 ? value/32 : (int32_t)(-((-(int64_t)value+31)/32));
}

static int region_path(const World *world,const Chunk *chunk,char *path,size_t cap)
{
    int n=snprintf(path,cap,"%s/region/r.%ld.%ld.mcr",world->path,
                   (long)floor_div32(chunk->x),(long)floor_div32(chunk->z));
    return n>0 && (size_t)n<cap;
}

static unsigned location_index(const Chunk *chunk)
{
    return ((unsigned)chunk->x&31u)+(((unsigned)chunk->z&31u)<<5u);
}

static int tag_named(const NbtTag *tag,const char *name)
{
    size_t len=strlen(name);
    return tag->name.size==len && memcmp(tag->name.data,name,len)==0;
}

static int chunk_tag(void *context,NbtEvent event,const NbtTag *tag,unsigned depth)
{
    static const char *const names[4]={"Blocks","Data","BlockLight","SkyLight"};
    static const size_t lengths[4]={WORLD_CHUNK_VOLUME,WORLD_NIBBLE_BYTES,
                                    WORLD_NIBBLE_BYTES,WORLD_NIBBLE_BYTES};
    ChunkTags *tags=(ChunkTags *)context;
    int i;
    if (event!=NBT_VALUE || depth!=2) return 1;
    if (tag->type==NBT_INT && tag_named(tag,"xPos")) {
        tags->x=tag->value.int_value; tags->has_x=1;
    } else if (tag->type==NBT_INT && tag_named(tag,"zPos")) {
        tags->z=tag->value.int_value; tags->has_z=1;
    }
    if (tag->type!=NBT_BYTE_ARRAY) return 1;
    for (i=0;i<4;++i) if (tag_named(tag,names[i])) {
        if ((tags->found&(1u<<i)) || tag->value.bytes.size!=lengths[i] ||
            tags->size<lengths[i] ||
            tag->value.bytes.data<tags->raw ||
            (size_t)(tag->value.bytes.data-tags->raw)>tags->size-lengths[i])
            return 0;
        tags->offsets[i]=(size_t)(tag->value.bytes.data-tags->raw);
        tags->found|=1u<<i;
        return 1;
    }
    return 1;
}

static uint8_t nibble_at(const uint8_t *data,size_t index)
{
    uint8_t value=data[index>>1u];
    return (uint8_t)((index&1u) ? value>>4u : value&15u);
}

static void nibble_put(uint8_t *data,size_t index,uint8_t value)
{
    uint8_t *byte=&data[index>>1u];
    if (index&1u) *byte=(uint8_t)((*byte&15u)|((value&15u)<<4u));
    else *byte=(uint8_t)((*byte&240u)|(value&15u));
}

static WorldError inflate_chunk(const uint8_t *compressed,size_t compressed_size,
                                int compression,uint8_t **out,size_t *out_size)
{
    z_stream stream;
    uint8_t *raw;
    size_t capacity=128u*1024u;
    int code;
    if (compressed_size>UINT_MAX) return WORLD_ERROR_CORRUPT;
    raw=(uint8_t *)malloc(capacity);
    if (!raw) return WORLD_ERROR_OUT_OF_MEMORY;
    memset(&stream,0,sizeof(stream));
    stream.next_in=(Bytef *)compressed;
    stream.avail_in=(uInt)compressed_size;
    if (inflateInit2(&stream,compression==1 ? MAX_WBITS+16 : MAX_WBITS)!=Z_OK) {
        free(raw); return WORLD_ERROR_CORRUPT;
    }
    for (;;) {
        if (stream.total_out==capacity) {
            uint8_t *grown;
            if (capacity>=BETA_NBT_LIMIT) { code=Z_MEM_ERROR; break; }
            capacity*=2u;
            grown=(uint8_t *)realloc(raw,capacity);
            if (!grown) { code=Z_MEM_ERROR; break; }
            raw=grown;
        }
        stream.next_out=raw+stream.total_out;
        stream.avail_out=(uInt)(capacity-stream.total_out);
        code=inflate(&stream,Z_NO_FLUSH);
        if (code!=Z_OK) break;
    }
    if (code==Z_STREAM_END && stream.avail_in==0 && stream.total_out<=BETA_NBT_LIMIT) {
        *out=raw; *out_size=(size_t)stream.total_out;
        inflateEnd(&stream);
        return WORLD_OK;
    }
    inflateEnd(&stream);
    free(raw);
    return code==Z_MEM_ERROR ? WORLD_ERROR_OUT_OF_MEMORY : WORLD_ERROR_CORRUPT;
}

WorldError beta_region_read_chunk(const World *world,Chunk *chunk)
{
    char path[WORLD_PATH_MAX];
    FILE *file;
    uint8_t location[4],length_bytes[4],compression;
    uint8_t *compressed=NULL,*raw=NULL;
    uint32_t offset,sectors,length;
    size_t raw_size,base,local,external;
    unsigned index=location_index(chunk);
    long file_size;
    ChunkTags tags;
    WorldError result=WORLD_ERROR_CORRUPT;
    int x,y,z;
    if (!world || !chunk || !region_path(world,chunk,path,sizeof(path)))
        return WORLD_ERROR_PATH_TOO_LONG;
    file=fopen(path,"rb");
    if (!file) return WORLD_ERROR_NOT_FOUND;
    if (fseek(file,0,SEEK_END)!=0 || (file_size=ftell(file))<(long)REGION_HEADER ||
        fseek(file,(long)(index*4u),SEEK_SET)!=0 ||
        fread(location,1,4,file)!=4) goto done;
    offset=((uint32_t)location[0]<<16)|((uint32_t)location[1]<<8)|location[2];
    sectors=location[3];
    if (!offset && !sectors) { result=WORLD_ERROR_NOT_FOUND; goto done; }
    if (offset<2 || sectors==0 || (uint64_t)(offset+sectors)*REGION_SECTOR>(uint64_t)file_size ||
        fseek(file,(long)((uint64_t)offset*REGION_SECTOR),SEEK_SET)!=0 ||
        fread(length_bytes,1,4,file)!=4 || fread(&compression,1,1,file)!=1) goto done;
    length=be32(length_bytes);
    if (length<2 || length>sectors*REGION_SECTOR-4u ||
        length>BETA_COMPRESSED_LIMIT || (compression!=1 && compression!=2)) goto done;
    compressed=(uint8_t *)malloc(length-1u);
    if (!compressed) { result=WORLD_ERROR_OUT_OF_MEMORY; goto done; }
    if (fread(compressed,1,length-1u,file)!=length-1u) goto done;
    result=inflate_chunk(compressed,length-1u,compression,&raw,&raw_size);
    if (result!=WORLD_OK) goto done;
    memset(&tags,0,sizeof(tags)); tags.raw=raw; tags.size=raw_size;
    if (nbt_read(raw,raw_size,NULL,NULL,NULL,NULL)!=NBT_OK ||
        nbt_read(raw,raw_size,NULL,chunk_tag,&tags,NULL)!=NBT_OK ||
        tags.found!=15u || !tags.has_x || !tags.has_z ||
        tags.x!=chunk->x || tags.z!=chunk->z) {
        result=WORLD_ERROR_CORRUPT; goto done;
    }
    for (x=0;x<16;++x) for (z=0;z<16;++z) for (y=0;y<128;++y) {
        external=(size_t)y+(size_t)z*128u+(size_t)x*2048u;
        local=(size_t)x+(size_t)z*16u+(size_t)y*256u;
        chunk->blocks[local]=raw[tags.offsets[0]+external];
        nibble_put(chunk->metadata,local,nibble_at(raw+tags.offsets[1],external));
        nibble_put(chunk->block_light,local,nibble_at(raw+tags.offsets[2],external));
        nibble_put(chunk->sky_light,local,nibble_at(raw+tags.offsets[3],external));
    }
    for (base=0;base<4;++base) chunk->beta_offsets[base]=tags.offsets[base];
    chunk->beta_raw=raw; chunk->beta_raw_size=raw_size;
    chunk->beta_compression=compression;
    chunk->dirty_flags=CHUNK_DIRTY_MESH;
    chunk->revision=1;
    raw=NULL;
    result=WORLD_OK;
done:
    free(raw); free(compressed); fclose(file);
    return result;
}

static WorldError deflate_chunk(const uint8_t *raw,size_t raw_size,int compression,
                                uint8_t **out,size_t *out_size)
{
    z_stream stream;
    uint8_t *compressed;
    size_t capacity;
    int code;
    if (raw_size>UINT_MAX) return WORLD_ERROR_CORRUPT;
    capacity=(size_t)compressBound((uLong)raw_size)+32u;
    compressed=(uint8_t *)malloc(capacity);
    if (!compressed) return WORLD_ERROR_OUT_OF_MEMORY;
    memset(&stream,0,sizeof(stream));
    if (deflateInit2(&stream,Z_DEFAULT_COMPRESSION,Z_DEFLATED,
                     compression==1 ? MAX_WBITS+16 : MAX_WBITS,8,Z_DEFAULT_STRATEGY)!=Z_OK) {
        free(compressed); return WORLD_ERROR_CORRUPT;
    }
    stream.next_in=(Bytef *)raw; stream.avail_in=(uInt)raw_size;
    stream.next_out=compressed; stream.avail_out=(uInt)capacity;
    code=deflate(&stream,Z_FINISH);
    if (code!=Z_STREAM_END) { deflateEnd(&stream); free(compressed); return WORLD_ERROR_CORRUPT; }
    *out=compressed; *out_size=(size_t)stream.total_out;
    deflateEnd(&stream);
    return WORLD_OK;
}

WorldError beta_region_write_chunk(const World *world,const Chunk *chunk)
{
    char path[WORLD_PATH_MAX];
    FILE *file=NULL;
    uint8_t *raw=NULL,*compressed=NULL,header[5],location[4],timestamp[4];
    size_t compressed_size,external,local;
    unsigned index=location_index(chunk),sectors;
    long end,aligned;
    int x,y,z;
    WorldError result=WORLD_ERROR_IO;
    if (!world || !chunk || !chunk->beta_raw ||
        !region_path(world,chunk,path,sizeof(path))) return WORLD_ERROR_INVALID_ARGUMENT;
    raw=(uint8_t *)malloc(chunk->beta_raw_size);
    if (!raw) return WORLD_ERROR_OUT_OF_MEMORY;
    memcpy(raw,chunk->beta_raw,chunk->beta_raw_size);
    for (x=0;x<16;++x) for (z=0;z<16;++z) for (y=0;y<128;++y) {
        external=(size_t)y+(size_t)z*128u+(size_t)x*2048u;
        local=(size_t)x+(size_t)z*16u+(size_t)y*256u;
        raw[chunk->beta_offsets[0]+external]=chunk->blocks[local];
        nibble_put(raw+chunk->beta_offsets[1],external,nibble_at(chunk->metadata,local));
        nibble_put(raw+chunk->beta_offsets[2],external,nibble_at(chunk->block_light,local));
        nibble_put(raw+chunk->beta_offsets[3],external,nibble_at(chunk->sky_light,local));
    }
    result=deflate_chunk(raw,chunk->beta_raw_size,chunk->beta_compression,
                          &compressed,&compressed_size);
    if (result!=WORLD_OK) goto done;
    result=WORLD_ERROR_IO;
    sectors=(unsigned)((compressed_size+5u+REGION_SECTOR-1u)/REGION_SECTOR);
    if (sectors==0 || sectors>255u) { result=WORLD_ERROR_CORRUPT; goto done; }
    file=fopen(path,"r+b");
    if (!file) { result=WORLD_ERROR_IO; goto done; }
    if (fseek(file,0,SEEK_END)!=0 || (end=ftell(file))<(long)REGION_HEADER) goto done;
    aligned=(end+(long)REGION_SECTOR-1L)&~((long)REGION_SECTOR-1L);
    if ((uint64_t)aligned/REGION_SECTOR>0xffffffu ||
        aligned>LONG_MAX-(long)(sectors*REGION_SECTOR)) goto done;
    if (fseek(file,end,SEEK_SET)!=0) goto done;
    while (end++<aligned) if (fputc(0,file)==EOF) goto done;
    put_be32(header,(uint32_t)(compressed_size+1u));
    header[4]=chunk->beta_compression;
    if (fwrite(header,1,5,file)!=5 ||
        fwrite(compressed,1,compressed_size,file)!=compressed_size) goto done;
    end=aligned+(long)(5u+compressed_size);
    while (end++<aligned+(long)(sectors*REGION_SECTOR))
        if (fputc(0,file)==EOF) goto done;
    if (fflush(file)!=0) goto done;
    location[0]=(uint8_t)(((unsigned long)aligned/REGION_SECTOR)>>16);
    location[1]=(uint8_t)(((unsigned long)aligned/REGION_SECTOR)>>8);
    location[2]=(uint8_t)((unsigned long)aligned/REGION_SECTOR);
    location[3]=(uint8_t)sectors;
    put_be32(timestamp,(uint32_t)time(NULL));
    if (fseek(file,(long)(index*4u),SEEK_SET)!=0 ||
        fwrite(location,1,4,file)!=4 ||
        fseek(file,(long)(REGION_SECTOR+index*4u),SEEK_SET)!=0 ||
        fwrite(timestamp,1,4,file)!=4 || fflush(file)!=0) goto done;
    result=WORLD_OK;
done:
    if (file && fclose(file)!=0) result=WORLD_ERROR_IO;
    free(compressed); free(raw);
    return result;
}
