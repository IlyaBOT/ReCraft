#include "../src/world/beta_region.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef _WIN32
#include <direct.h>
#include <process.h>
#define test_mkdir(p) _mkdir(p)
#define test_rmdir(p) _rmdir(p)
#define test_pid() _getpid()
#else
#include <sys/stat.h>
#include <unistd.h>
#define test_mkdir(p) mkdir(p,0700)
#define test_rmdir(p) rmdir(p)
#define test_pid() getpid()
#endif

static void open_beta(World *world,const char *path)
{
    assert(world_init(world,123,0,8)==WORLD_OK);
    snprintf(world->path,sizeof(world->path),"%s",path);
    world->persistent=1;
    world->beta_format=1;
    world->read_beta_chunk=beta_region_read_chunk;
    world->write_beta_chunk=beta_region_write_chunk;
}

static void copy_region(const char *source,const char *target)
{
    unsigned char buffer[8192];
    FILE *in=fopen(source,"rb"),*out=fopen(target,"wb");
    size_t n;
    assert(in && out);
    while ((n=fread(buffer,1,sizeof(buffer),in))>0)
        assert(fwrite(buffer,1,n,out)==n);
    assert(!ferror(in));
    assert(fclose(in)==0);
    assert(fclose(out)==0);
}

static int terrain_byte(size_t offset,const Chunk *chunk)
{
    static const size_t lengths[4]={WORLD_CHUNK_VOLUME,WORLD_NIBBLE_BYTES,
                                    WORLD_NIBBLE_BYTES,WORLD_NIBBLE_BYTES};
    int i;
    for (i=0;i<4;++i)
        if (offset>=chunk->beta_offsets[i] &&
            offset<chunk->beta_offsets[i]+lengths[i]) return 1;
    return 0;
}

int main(int argc,char **argv)
{
    World world={0};
    Chunk *chunk;
    char source[512],clone[512],region[520],copy[544];
    uint8_t *raw;
    size_t raw_size,i;
    int before,after;
    if (argc!=3) return 2;
    open_beta(&world,argv[1]);
    chunk=(Chunk *)calloc(1,sizeof(*chunk)); assert(chunk);
    chunk->x=4; chunk->z=1; /* Direct read: never attach originals to a save-capable cache. */
    assert(beta_region_read_chunk(&world,chunk)==WORLD_OK);
    assert(chunk->beta_raw && chunk->beta_raw_size>WORLD_CHUNK_VOLUME);
    free(chunk->beta_raw); free(chunk);
    assert(world_close(&world)==WORLD_OK);
    open_beta(&world,argv[2]);
    chunk=(Chunk *)calloc(1,sizeof(*chunk)); assert(chunk);
    chunk->x=-12; chunk->z=7;
    assert(beta_region_read_chunk(&world,chunk)==WORLD_OK);
    assert(chunk->beta_raw && chunk->beta_raw_size>WORLD_CHUNK_VOLUME);
    free(chunk->beta_raw); free(chunk);
    assert(world_close(&world)==WORLD_OK);

    snprintf(clone,sizeof(clone),"build/beta-region-test-%d",(int)test_pid());
    snprintf(region,sizeof(region),"%s/region",clone);
    assert(test_mkdir(clone)==0);
    assert(test_mkdir(region)==0);
    snprintf(source,sizeof(source),"%s/region/r.0.0.mcr",argv[1]);
    snprintf(copy,sizeof(copy),"%s/r.0.0.mcr",region);
    copy_region(source,copy);
    open_beta(&world,clone);
    chunk=world_get_chunk(&world,4,1);
    assert(chunk && chunk->beta_raw);
    raw_size=chunk->beta_raw_size;
    raw=(uint8_t *)malloc(raw_size);
    assert(raw);
    memcpy(raw,chunk->beta_raw,raw_size);
    before=world_get_block(&world,72,80,19);
    after=before==BLOCK_STONE ? BLOCK_GLASS : BLOCK_STONE;
    assert(world_set_block(&world,72,80,19,(uint8_t)after));
    assert(world_close(&world)==WORLD_OK);
    open_beta(&world,clone);
    chunk=world_get_chunk(&world,4,1);
    assert(chunk && chunk->beta_raw && chunk->beta_raw_size==raw_size);
    assert(world_get_block(&world,72,80,19)==after);
    for (i=0;i<raw_size;++i)
        if (!terrain_byte(i,chunk)) assert(raw[i]==chunk->beta_raw[i]);
    free(raw);
    assert(world_close(&world)==WORLD_OK);
    assert(remove(copy)==0);
    assert(test_rmdir(region)==0);
    assert(test_rmdir(clone)==0);
    puts("Beta 1.7.3 McRegion read and cloned write passed");
    return 0;
}
