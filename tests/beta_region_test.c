#include "../src/world/block_entity.h"
#include "../src/world/entities.h"
#include "../src/world/ticks.h"
#include "../src/world/beta_region.h"
#include "../src/world/beta_session.h"
#include "../src/game/player.h"
#include "../src/game/sign.h"

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

static uint32_t disk_hash(const char *path)
{
    uint32_t hash=UINT32_C(2166136261); int byte; FILE *file=fopen(path,"rb"); assert(file);
    while((byte=fgetc(file))!=EOF) { hash^=(unsigned)byte; hash*=UINT32_C(16777619); }
    assert(!ferror(file) && fclose(file)==0); return hash;
}
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

typedef struct Unrelated { NbtWriter writer; int skip; } Unrelated;
static int unrelated_tag(void *context,NbtEvent event,const NbtTag *tag,unsigned depth)
{
    Unrelated *u=(Unrelated *)context;
    if(depth==2 && tag->type==NBT_LIST && tag->name.size==9 && !memcmp(tag->name.data,"TileTicks",9)) {
        u->skip=event==NBT_BEGIN; return 1;
    }
    if(u->skip) return 1;
    if(depth==2 && tag->type==NBT_BYTE_ARRAY &&
        ((tag->name.size==6 && !memcmp(tag->name.data,"Blocks",6)) ||
         (tag->name.size==4 && !memcmp(tag->name.data,"Data",4)) ||
         (tag->name.size==10 && !memcmp(tag->name.data,"BlockLight",10)) ||
         (tag->name.size==8 && !memcmp(tag->name.data,"SkyLight",8)))) return 1;
    return (event==NBT_FINISH ? nbt_writer_end(&u->writer) : nbt_writer_tag(&u->writer,tag))==NBT_OK;
}
static unsigned unrelated_hash(const uint8_t *raw,size_t size)
{
    Unrelated u={0}; uint8_t *bytes=(uint8_t *)malloc(size+64); size_t count,i; unsigned hash=2166136261u;
    assert(bytes); nbt_writer_init(&u.writer,bytes,size+64,NULL);
    assert(nbt_read(raw,size,NULL,unrelated_tag,&u,NULL)==NBT_OK && nbt_writer_finish(&u.writer,&count)==NBT_OK);
    for(i=0;i<count;++i) hash=(hash^bytes[i])*16777619u;
    free(bytes); return hash;
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
    block_entities_free(chunk); world_entities_free(chunk); world_ticks_free(chunk); free(chunk->beta_raw); free(chunk);
    assert(world_close(&world)==WORLD_OK);
    open_beta(&world,argv[2]);
    chunk=(Chunk *)calloc(1,sizeof(*chunk)); assert(chunk);
    chunk->x=-12; chunk->z=7;
    assert(beta_region_read_chunk(&world,chunk)==WORLD_OK);
    assert(chunk->beta_raw && chunk->beta_raw_size>WORLD_CHUNK_VOLUME);
    block_entities_free(chunk); world_entities_free(chunk); world_ticks_free(chunk); free(chunk->beta_raw); free(chunk);
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
    {
        int64_t other; uint32_t hash=disk_hash(copy);
        assert(beta_session_start(clone,&world.beta_session));
        assert(beta_session_start(clone,&other));
        assert(world_save(&world)==WORLD_ERROR_SESSION_LOCK);
        assert(beta_region_write_chunk(&world,chunk)==WORLD_ERROR_SESSION_LOCK);
        assert(disk_hash(copy)==hash);
        assert(beta_session_start(clone,&world.beta_session));
    }
    /* Light can reach a cached placeholder beyond the original region. No
     * fabricated region chunk should be written, and exit must still succeed. */
    {
        Chunk *missing=world_get_chunk(&world,10000,10000);
        assert(missing && !missing->beta_raw);
        world_relight_chunk(&world,missing);
    }
    assert(world_close(&world)==WORLD_OK);
    open_beta(&world,clone);
    chunk=world_get_chunk(&world,4,1);
    assert(chunk && chunk->beta_raw);
    assert(world_get_block(&world,72,80,19)==after);
    assert(unrelated_hash(raw,raw_size)==unrelated_hash(chunk->beta_raw,chunk->beta_raw_size));
    free(raw);
    /* Real Minecraft lists, including a later terrain-only save of a cached chunk. */
    {
        BlockEntity *chest;
        assert(world_set_block(&world,72,81,19,54));
        chest=block_entity_get(&world,72,81,19,1); assert(chest);
        chest->slots[0]=(InventorySlot){35,19,6}; block_entity_changed(&world,chest);
        world_drop_stack(&world,74,82,19,(InventorySlot){278,1,123});
        {
            Player p={0}; InventorySlot inv[36]={{262,2,0}},disc={2257,1,0}; int sx,sy,sz;
            SavedEntity *entity;
            assert(world_set_block(&world,70,99,19,1));
            assert(world_set_state(&world,70,100,19,(BetaBlockState){69,14}));
            assert(sign_place(&world,70,99,19,3,0,&sx,&sy,&sz));
            assert(sign_text_set(&world,sx,sy,sz,(const char [4][61]){{"Done / ESC"},{"Saved Beta sign"}}));
            assert(world_set_block(&world,71,100,19,84) && jukebox_use(&world,71,100,19,&disc));
            assert(world_set_block(&world,72,99,19,1)); assert(world_set_state(&world,72,100,19,(BetaBlockState){66,1}));
            assert(world_minecart_spawn(&world,72.5f,100,19.5f,1));
            entity=chunk->saved_entities; entity->transport.cargo[26]=(InventorySlot){278,1,37}; world_transport_changed(&world,entity);
            p.x=73.5f; p.y=100; p.z=19.5f;
            assert(world_bow_use(&world,&p,inv));
            entity=chunk->saved_entities; entity->transport.in_ground=1; entity->transport.x_tile=72;
            entity->transport.y_tile=100; entity->transport.z_tile=19; entity->transport.in_tile=66; entity->transport.in_data=1;
            world_transport_changed(&world,entity);
        }
        assert(world_save(&world)==WORLD_OK);
        assert(world_set_block(&world,73,80,19,20)); assert(world_save(&world)==WORLD_OK);
    }
    assert(world_close(&world)==WORLD_OK);
    open_beta(&world,clone);
    {
        BlockEntity *chest=block_entity_get(&world,72,81,19,0); ItemDrop items[128]; int count;
        assert(chest && chest->slots[0].id==35 && chest->slots[0].count==19 && chest->slots[0].damage==6);
        {
            SavedEntity *entity; int carts=0,arrows=0;
            assert(world_peek_metadata(&world,70,100,19)==14 && world_peek_block(&world,70,100,19)==69);
            assert(world_peek_block(&world,70,99,19)==1 && !strcmp(sign_text_get(&world,70,99,20)[0],"Done / ESC"));
            assert(world_peek_metadata(&world,71,100,19)==1 && block_entity_get(&world,71,100,19,0)->record==2257);
            for(entity=world_peek_chunk(&world,4,1)->saved_entities;entity;entity=entity->next) {
                if(entity->transport.kind==2 && entity->transport.type==1 && entity->transport.cargo[26].id==278 && entity->transport.cargo[26].damage==37) ++carts;
                if(entity->transport.kind==1 && entity->transport.in_ground && entity->transport.player && entity->transport.in_tile==66) ++arrows;
            }
            assert(carts==1 && arrows==1);
        }
        count=world_items_visible(&world,items,128);
        for(i=0;i<(size_t)count;++i) if(items[i].id==278 && items[i].damage==123) break;
        assert(i<(size_t)count && items[i].count==1);
        assert(world_set_block(&world,72,81,19,0) && world_save(&world)==WORLD_OK);
    }
    assert(world_close(&world)==WORLD_OK);
    open_beta(&world,clone);
    assert(!block_entity_get(&world,72,81,19,0));
    {
        ItemDrop items[128]; int count=world_items_visible(&world,items,128);
        for(i=0;i<(size_t)count;++i) if(items[i].id==35 && items[i].damage==6 && items[i].count==19) break;
        assert(i<(size_t)count);
    }
    assert(world_close(&world)==WORLD_OK);
    assert(remove(copy)==0);
    snprintf(copy,sizeof(copy),"%s/session.lock",clone); assert(remove(copy)==0);
    assert(test_rmdir(region)==0);
    assert(test_rmdir(clone)==0);
    puts("Beta 1.7.3 McRegion read and cloned write passed");
    return 0;
}
