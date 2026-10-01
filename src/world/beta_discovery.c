#include "beta_discovery.h"
#include "beta_level_io.h"
#include "../nbt/nbt.h"
#include "../util/game_paths.h"

#include <stdlib.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#else
#include <dirent.h>
#include <sys/stat.h>
#endif

static int tag_is(const NbtTag *tag, const char *name)
{
    size_t length = strlen(name);
    return tag->name.size == length && memcmp(tag->name.data, name, length) == 0;
}

typedef struct LevelRead {
    BetaWorldInfo *world;
    InventorySlot *inventory;
    int data,player,pos,rotation,pos_count,rotation_count;
    int in_inventory,item_slot,item_id,item_count,item_damage;
    double xyz[3];
    float angles[2];
} LevelRead;

static int metadata_tag(void *context, NbtEvent event, const NbtTag *tag, unsigned depth)
{
    LevelRead *read=(LevelRead *)context;
    BetaWorldInfo *world=read->world;
    if (depth==1 && tag->type==NBT_COMPOUND && tag_is(tag,"Data"))
        read->data=event==NBT_BEGIN;
    if (!read->data) return 1;
    if (event==NBT_BEGIN && depth==2 && tag->type==NBT_COMPOUND &&
        tag_is(tag,"Player")) read->player=1;
    else if (event==NBT_FINISH && depth==2 && tag->type==NBT_COMPOUND &&
             tag_is(tag,"Player")) read->player=0;
    if (read->player && event==NBT_BEGIN && depth==3 && tag->type==NBT_LIST) {
        if (tag_is(tag,"Pos") && tag->list_type==NBT_DOUBLE && tag->count==3)
            read->pos=1;
        if (tag_is(tag,"Rotation") && tag->list_type==NBT_FLOAT && tag->count==2)
            read->rotation=1;
        if (tag_is(tag,"Inventory") && tag->list_type==NBT_COMPOUND)
            read->in_inventory=1;
    }
    if (read->in_inventory && event==NBT_BEGIN && depth==4 &&
        tag->type==NBT_COMPOUND) {
        read->item_slot=-1; read->item_id=-1;
        read->item_count=0; read->item_damage=0;
    }
    if (read->in_inventory && event==NBT_VALUE && depth==5) {
        if (tag_is(tag,"Slot") && tag->type==NBT_BYTE)
            read->item_slot=(uint8_t)tag->value.byte;
        else if (tag_is(tag,"id") && tag->type==NBT_SHORT)
            read->item_id=(uint16_t)tag->value.short_value;
        else if (tag_is(tag,"Count") && tag->type==NBT_BYTE)
            read->item_count=(uint8_t)tag->value.byte;
        else if (tag_is(tag,"Damage") && tag->type==NBT_SHORT)
            read->item_damage=(uint16_t)tag->value.short_value;
    }
    if (read->in_inventory && event==NBT_FINISH && depth==4 &&
        tag->type==NBT_COMPOUND && read->inventory &&
        read->item_slot>=0 && read->item_slot<RECRAFT_INVENTORY_SLOTS &&
        read->item_id>0 && read->item_count>0) {
        InventorySlot *slot=&read->inventory[read->item_slot];
        slot->id=read->item_id;
        slot->count=read->item_count;
        slot->damage=read->item_damage;
    }
    if (read->player && event==NBT_VALUE && depth==4) {
        if (read->pos && tag->type==NBT_DOUBLE && read->pos_count<3)
            read->xyz[read->pos_count++]=tag->value.double_value;
        if (read->rotation && tag->type==NBT_FLOAT && read->rotation_count<2)
            read->angles[read->rotation_count++]=tag->value.float_value;
    }
    if (read->player && event==NBT_VALUE && depth==3 &&
        tag->type==NBT_INT && tag_is(tag,"Dimension"))
        world->dimension=tag->value.int_value;
    if (event==NBT_FINISH && depth==3 && tag->type==NBT_LIST) {
        if (tag_is(tag,"Pos")) read->pos=0;
        if (tag_is(tag,"Rotation")) read->rotation=0;
        if (tag_is(tag,"Inventory")) read->in_inventory=0;
    }
    if (event != NBT_VALUE || depth != 2) return 1;
    if (tag->type == NBT_STRING && tag_is(tag,"LevelName")) {
        size_t n = tag->value.bytes.size;
        if (n >= sizeof(world->name)) n = sizeof(world->name)-1;
        memcpy(world->name,tag->value.bytes.data,n); world->name[n] = '\0';
    } else if (tag->type == NBT_LONG && tag_is(tag,"RandomSeed"))
        world->seed = tag->value.long_value;
    else if (tag->type == NBT_LONG && tag_is(tag,"LastPlayed"))
        world->last_played = (uint64_t)(tag->value.long_value/1000);
    else if (tag->type == NBT_LONG && tag_is(tag,"Time"))
        world->world_time = tag->value.long_value;
    else if (tag->type == NBT_INT && tag_is(tag,"SpawnX"))
        world->spawn_x = tag->value.int_value;
    else if (tag->type == NBT_INT && tag_is(tag,"SpawnY"))
        world->spawn_y = tag->value.int_value;
    else if (tag->type == NBT_INT && tag_is(tag,"SpawnZ"))
        world->spawn_z = tag->value.int_value;
    else if (tag->type == NBT_INT && tag_is(tag,"version"))
        world->save_version = tag->value.int_value;
    return 1;
}

static int read_level(const char *world_path, BetaWorldInfo *world,InventorySlot *inventory)
{
    unsigned char *bytes;
    size_t size = 0;
    int valid;
    LevelRead parsed;
    if (!beta_level_read(world_path,&bytes,&size,NULL)) return 0;
    memset(&parsed,0,sizeof(parsed)); parsed.world=world;
    parsed.inventory=inventory;
    valid = nbt_read(bytes,size,NULL,metadata_tag,&parsed,NULL) == NBT_OK;
    if (valid && parsed.pos_count==3 &&
        isfinite(parsed.xyz[0]) && isfinite(parsed.xyz[1]) && isfinite(parsed.xyz[2]) &&
        fabs(parsed.xyz[0])<10000000.0 && parsed.xyz[1]>=0 && parsed.xyz[1]<256 &&
        fabs(parsed.xyz[2])<10000000.0) {
        world->has_player=1;
        world->player_x=parsed.xyz[0];
        world->player_y=parsed.xyz[1];
        world->player_z=parsed.xyz[2];
        if (parsed.rotation_count==2 && isfinite(parsed.angles[0]) &&
            isfinite(parsed.angles[1])) {
            world->player_yaw=parsed.angles[0];
            world->player_pitch=parsed.angles[1];
        }
    }
    free(bytes);
    return valid;
}

static unsigned count_regions(const char *world_path)
{
    char path[512];
    unsigned count = 0;
    if (!game_path_join(path,sizeof(path),world_path,"region")) return 0;
#ifdef _WIN32
    {
        WIN32_FIND_DATAA data;
        HANDLE handle;
        char pattern[512];
        if (!game_path_join(pattern,sizeof(pattern),path,"*.mcr")) return 0;
        handle = FindFirstFileA(pattern,&data);
        if (handle == INVALID_HANDLE_VALUE) return 0;
        do { if (!(data.dwFileAttributes&(FILE_ATTRIBUTE_DIRECTORY|FILE_ATTRIBUTE_REPARSE_POINT))) ++count; }
        while (FindNextFileA(handle,&data));
        FindClose(handle);
    }
#else
    {
        DIR *dir = opendir(path);
        struct dirent *entry;
        if (!dir) return 0;
        while ((entry=readdir(dir)) != NULL) {
            size_t n = strlen(entry->d_name);
            if (n >= 4 && strcmp(entry->d_name+n-4,".mcr") == 0) ++count;
        }
        closedir(dir);
    }
#endif
    return count;
}

static int read_world(const char *saves, const char *directory, BetaWorldInfo *world)
{
    char world_path[512];
    if (!directory[0] || strcmp(directory,".") == 0 || strcmp(directory,"..") == 0 ||
        strlen(directory) >= sizeof(world->directory) ||
        !game_path_join(world_path,sizeof(world_path),saves,directory)) return 0;
    memset(world,0,sizeof(*world));
    snprintf(world->directory,sizeof(world->directory),"%s",directory);
    if (!read_level(world_path,world,NULL)) return 0;
    if (!world->name[0]) snprintf(world->name,sizeof(world->name),"%s",directory);
    world->region_files = count_regions(world_path);
    return 1;
}

int beta_world_read_inventory(const char *world_path,
                              InventorySlot slots[RECRAFT_INVENTORY_SLOTS])
{
    BetaWorldInfo ignored;
    InventorySlot loaded[RECRAFT_INVENTORY_SLOTS];
    if (!world_path || !slots) return 0;
    memset(&ignored,0,sizeof(ignored));
    inventory_init(loaded,0);
    if (!read_level(world_path,&ignored,loaded)) return 0;
    memcpy(slots,loaded,sizeof(loaded));
    return 1;
}

size_t beta_world_discover(const char *saves_dir, BetaWorldInfo *out, size_t capacity)
{
    size_t count = 0;
    if (!saves_dir || !out) return 0;
#ifdef _WIN32
    {
        WIN32_FIND_DATAA data;
        HANDLE handle;
        char pattern[512];
        if (!game_path_join(pattern,sizeof(pattern),saves_dir,"*")) return 0;
        handle = FindFirstFileA(pattern,&data);
        if (handle == INVALID_HANDLE_VALUE) return 0;
        do {
            if (count < capacity && (data.dwFileAttributes&FILE_ATTRIBUTE_DIRECTORY) &&
                !(data.dwFileAttributes&FILE_ATTRIBUTE_REPARSE_POINT) &&
                read_world(saves_dir,data.cFileName,&out[count])) ++count;
        } while (FindNextFileA(handle,&data));
        FindClose(handle);
    }
#else
    {
        DIR *dir = opendir(saves_dir);
        struct dirent *entry;
        if (!dir) return 0;
        while (count < capacity && (entry=readdir(dir)) != NULL) {
            char path[512];
            struct stat state;
            if (!game_path_join(path,sizeof(path),saves_dir,entry->d_name) ||
                lstat(path,&state) != 0 || !S_ISDIR(state.st_mode)) continue;
            if (read_world(saves_dir,entry->d_name,&out[count])) ++count;
        }
        closedir(dir);
    }
#endif
    return count;
}
