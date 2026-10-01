#include "world/beta_level.h"
#include "world/beta_level_io.h"
#include "world/beta_discovery.h"
#include "nbt/nbt.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <zlib.h>
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

static void tag(NbtWriter *writer,NbtType type,const char *name,int value)
{
    NbtTag t;
    memset(&t,0,sizeof(t)); t.type=type; t.name=nbt_span(name);
    switch(type) {
        case NBT_BYTE: t.value.byte=(int8_t)value; break;
        case NBT_SHORT: t.value.short_value=(int16_t)value; break;
        case NBT_INT: t.value.int_value=value; break;
        case NBT_LONG: t.value.long_value=value; break;
        case NBT_FLOAT: t.value.float_value=(float)value; break;
        case NBT_DOUBLE: t.value.double_value=value; break;
        default: break;
    }
    assert(nbt_writer_tag(writer,&t)==NBT_OK);
}

/* Small, generated vanilla-shaped fixture: no private worlds needed in CI. */
static void fixture(const char *path,int seed,int slot,int data)
{
    unsigned char bytes[2048];
    size_t size;
    NbtWriter writer;
    NbtTag t;
    gzFile file;
    int i;
    nbt_writer_init(&writer,bytes,sizeof(bytes),NULL);
    tag(&writer,NBT_COMPOUND,"",0);
    tag(&writer,NBT_COMPOUND,data?"Data":"Wrong",0);
    tag(&writer,NBT_LONG,"RandomSeed",seed);
    tag(&writer,NBT_LONG,"Time",100);
    tag(&writer,NBT_LONG,"LastPlayed",1000);
    tag(&writer,NBT_INT,"SpawnY",64);
    tag(&writer,NBT_INT,"version",19132);
    tag(&writer,NBT_BYTE,"raining",1);
    tag(&writer,NBT_COMPOUND,"Player",0);
    tag(&writer,NBT_SHORT,"Health",17);
    tag(&writer,NBT_INT,"Dimension",0);
    memset(&t,0,sizeof(t)); t.type=NBT_LIST;
    t.name=nbt_span("Pos"); t.list_type=NBT_DOUBLE; t.count=3;
    assert(nbt_writer_tag(&writer,&t)==NBT_OK);
    tag(&writer,NBT_DOUBLE,"",seed); tag(&writer,NBT_DOUBLE,"",65);
    tag(&writer,NBT_DOUBLE,"",4); assert(nbt_writer_end(&writer)==NBT_OK);
    t.name=nbt_span("Motion");
    assert(nbt_writer_tag(&writer,&t)==NBT_OK);
    for(i=0;i<3;++i) tag(&writer,NBT_DOUBLE,"",0);
    assert(nbt_writer_end(&writer)==NBT_OK);
    t.name=nbt_span("Rotation"); t.list_type=NBT_FLOAT; t.count=2;
    assert(nbt_writer_tag(&writer,&t)==NBT_OK);
    tag(&writer,NBT_FLOAT,"",90); tag(&writer,NBT_FLOAT,"",0);
    assert(nbt_writer_end(&writer)==NBT_OK);
    tag(&writer,NBT_BYTE,"OnGround",1); tag(&writer,NBT_FLOAT,"FallDistance",0);
    t.name=nbt_span("Inventory"); t.list_type=NBT_COMPOUND; t.count=2;
    assert(nbt_writer_tag(&writer,&t)==NBT_OK);
    for(i=0;i<2;++i) {
        tag(&writer,NBT_COMPOUND,"",0);
        tag(&writer,NBT_BYTE,"Slot",i?100:slot);
        tag(&writer,NBT_SHORT,"id",i?298:1);
        tag(&writer,NBT_BYTE,"Count",i?1:7);
        tag(&writer,NBT_SHORT,"Damage",i?5:0);
        assert(nbt_writer_end(&writer)==NBT_OK);
    }
    for(i=0;i<3;++i) assert(nbt_writer_end(&writer)==NBT_OK);
    /* Same field name outside Data must not become the world clock. */
    tag(&writer,NBT_COMPOUND,"Extra",0);
    tag(&writer,NBT_LONG,"Time",999);
    assert(nbt_writer_end(&writer)==NBT_OK);
    assert(nbt_writer_end(&writer)==NBT_OK);
    assert(nbt_writer_finish(&writer,&size)==NBT_OK);
    file=gzopen(path,"wb6"); assert(file);
    assert(gzwrite(file,bytes,(unsigned)size)==(int)size && gzclose(file)==Z_OK);
}

static void corrupt(const char *path)
{
    FILE *file=fopen(path,"wb"); assert(file);
    assert(fwrite("invalid",1,7,file)==7 && fclose(file)==0);
}

static unsigned hash_file(const char *path)
{
    FILE *file=fopen(path,"rb");
    unsigned hash=2166136261u;
    int c;
    assert(file);
    while((c=fgetc(file))!=EOF) hash=(hash^(unsigned)c)*16777619u;
    assert(!ferror(file) && fclose(file)==0);
    return hash;
}

static int preserved(void *context,NbtEvent event,const NbtTag *t,unsigned depth)
{
    int *seen=(int *)context;
    (void)depth;
    if(event!=NBT_VALUE) return 1;
    if(t->type==NBT_SHORT && t->name.size==6 && !memcmp(t->name.data,"Health",6)) {
        assert(t->value.short_value==17); *seen|=1;
    }
    if(t->type==NBT_BYTE && t->name.size==7 && !memcmp(t->name.data,"raining",7)) {
        assert(t->value.byte==1); *seen|=2;
    }
    if(t->type==NBT_BYTE && t->name.size==4 && !memcmp(t->name.data,"Slot",4) &&
       (uint8_t)t->value.byte==100) *seen|=4;
    if(t->type==NBT_LONG && t->name.size==4 && !memcmp(t->name.data,"Time",4) &&
       t->value.long_value==999) *seen|=8;
    return 1;
}

int main(void)
{
    char parent[128],dir[160],primary[192],old[192],backup[192];
    BetaWorldInfo worlds[2];
    BetaLevelState state;
    InventorySlot loaded[RECRAFT_INVENTORY_SLOTS],unchanged[RECRAFT_INVENTORY_SLOTS];
    unsigned char *bytes;
    size_t size;
    unsigned old_hash,primary_hash;
    int used_old,seen;
    FILE *file;
    snprintf(parent,sizeof(parent),"beta-recovery-%d",(int)test_pid());
    snprintf(dir,sizeof(dir),"%s/Recovery",parent);
    snprintf(primary,sizeof(primary),"%s/level.dat",dir);
    snprintf(old,sizeof(old),"%s/level.dat_old",dir);
    snprintf(backup,sizeof(backup),"%s/level.dat.recraft.bak",dir);
    assert(test_mkdir(parent)==0 && test_mkdir(dir)==0);
    fixture(primary,10,0,1); fixture(old,20,1,1);
    old_hash=hash_file(old);
    assert(beta_world_discover(parent,worlds,2)==1 && worlds[0].seed==10);
    assert(worlds[0].player_x==10);
    assert(beta_world_read_inventory(dir,loaded) && loaded[0].count==7);
    /* Corrupt/missing primary and wrong root all fall back without writes. */
    corrupt(primary);
    assert(beta_world_discover(parent,worlds,2)==1 && worlds[0].seed==20);
    assert(beta_world_read_inventory(dir,loaded) && loaded[1].count==7 && loaded[0].id==-1);
    assert(hash_file(old)==old_hash);
    assert(remove(primary)==0);
    assert(beta_world_discover(parent,worlds,2)==1 && worlds[0].seed==20);
    fixture(primary,30,0,0);
    assert(beta_world_discover(parent,worlds,2)==1 && worlds[0].seed==20);
    /* A gzip CRC failure is not a partially successful import. */
    fixture(primary,10,0,1);
    file=fopen(primary,"r+b"); assert(file && fseek(file,-8,SEEK_END)==0);
    seen=fgetc(file); assert(seen!=EOF && fseek(file,-1,SEEK_CUR)==0);
    assert(fputc(seen^1,file)!=EOF && fclose(file)==0);
    assert(beta_world_discover(parent,worlds,2)==1 && worlds[0].seed==20);
    memset(&state,0,sizeof(state));
    assert(beta_world_read_inventory(dir,state.inventory));
    state.x=17.25; state.y=70.5; state.z=-9.75; state.world_time=4567;
    assert(beta_level_save(dir,&state));
    assert(hash_file(old)==old_hash && hash_file(backup)==old_hash);
    assert(beta_world_discover(parent,worlds,2)==1 && worlds[0].player_x==state.x);
    assert(worlds[0].seed==20 && worlds[0].world_time==4567);
    assert(beta_level_read(dir,&bytes,&size,&used_old) && !used_old);
    seen=0; assert(nbt_read(bytes,size,NULL,preserved,&seen,NULL)==NBT_OK && seen==15);
    free(bytes);
    /* The second save rotates the valid primary while preserving first backup. */
    primary_hash=hash_file(primary); state.world_time=4568;
    assert(beta_level_save(dir,&state));
    assert(hash_file(old)==primary_hash && hash_file(backup)==old_hash);
    /* Neither reader nor writer changes an unusable world. */
    corrupt(primary); corrupt(old);
    memset(loaded,0x55,sizeof(loaded)); memcpy(unchanged,loaded,sizeof(loaded));
    primary_hash=hash_file(primary); old_hash=hash_file(old);
    assert(beta_world_discover(parent,worlds,2)==0);
    assert(!beta_world_read_inventory(dir,loaded) && !memcmp(loaded,unchanged,sizeof(loaded)));
    assert(!beta_level_save(dir,&state));
    assert(hash_file(primary)==primary_hash && hash_file(old)==old_hash);
    assert(remove(primary)==0 && remove(old)==0 && remove(backup)==0);
    assert(test_rmdir(dir)==0 && test_rmdir(parent)==0);
    puts("Beta recovery: fallback, gzip CRC, backup rotation and unrelated NBT passed");
    return 0;
}
