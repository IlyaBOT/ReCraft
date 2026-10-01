#include "world/beta_level.h"
#include "world/beta_discovery.h"

#include <assert.h>
#include <stdio.h>
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

static void copy_file(const char *source,const char *target)
{
    FILE *in=fopen(source,"rb"),*out=fopen(target,"wb");
    unsigned char bytes[4096];
    size_t n;
    assert(in && out);
    while ((n=fread(bytes,1,sizeof(bytes),in))>0)
        assert(fwrite(bytes,1,n,out)==n);
    assert(!ferror(in) && fclose(in)==0 && fclose(out)==0);
}

static int same_file(const char *left,const char *right)
{
    FILE *a=fopen(left,"rb"),*b=fopen(right,"rb");
    int x,y;
    assert(a && b);
    do { x=fgetc(a); y=fgetc(b); if (x!=y) break; } while (x!=EOF);
    fclose(a); fclose(b);
    return x==y;
}

int main(int argc,char **argv)
{
    char dir[128],source[512],copy[512],old[512],backup[512],temporary[512];
    BetaLevelState state;
    BetaWorldInfo worlds[64];
    InventorySlot loaded[RECRAFT_INVENTORY_SLOTS];
    size_t count,i;
    int found=0;
    FILE *file;
    if (argc!=2) return 2;
    snprintf(dir,sizeof(dir),"build/beta-level-test-%d",(int)test_pid());
    snprintf(source,sizeof(source),"%s/level.dat",argv[1]);
    snprintf(copy,sizeof(copy),"%s/level.dat",dir);
    snprintf(old,sizeof(old),"%s/level.dat_old",dir);
    snprintf(backup,sizeof(backup),"%s/level.dat.recraft.bak",dir);
    snprintf(temporary,sizeof(temporary),"%s/level.dat_new",dir);
    assert(test_mkdir(dir)==0);
    copy_file(source,copy);
    memset(&state,0,sizeof(state));
    assert(beta_world_read_inventory(dir,state.inventory));
    state.x=17.25; state.y=70.5; state.z=-9.75;
    state.yaw=92.5f; state.pitch=-10.5f;
    state.world_time=1234567;
    state.inventory[0].id=1;
    state.inventory[0].count=7;
    state.inventory[0].damage=0;
    state.inventory[1].id=-1;
    state.inventory[1].count=0;
    state.inventory[1].damage=0;
    assert(beta_level_save(dir,&state));
    assert(same_file(source,backup));
    assert(same_file(source,old));
    assert(beta_world_read_inventory(dir,loaded));
    assert(memcmp(loaded,state.inventory,sizeof(loaded))==0);
    count=beta_world_discover("build",worlds,64);
    for (i=0;i<count;++i)
        if (!strcmp(worlds[i].directory,dir+6)) {
            assert(worlds[i].world_time==state.world_time);
            assert(worlds[i].has_player);
            assert(worlds[i].player_x==state.x && worlds[i].player_y==state.y &&
                   worlds[i].player_z==state.z);
            assert(worlds[i].player_yaw==state.yaw &&
                   worlds[i].player_pitch==state.pitch);
            found=1;
        }
    assert(found);
    file=fopen(temporary,"rb");
    assert(!file);
    assert(remove(copy)==0 && remove(old)==0 && remove(backup)==0 && test_rmdir(dir)==0);
    puts("Beta level.dat player, inventory and time roundtrip passed on copy");
    return 0;
}
