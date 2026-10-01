#include "world.h"
#include "block_entity.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#ifdef _WIN32
#include <windows.h>
#include <direct.h>
#else
#include <dirent.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#endif

#define RAW_CHUNK_BYTES (WORLD_CHUNK_VOLUME + 3 * WORLD_NIBBLE_BYTES)
#define MAX_RLE_BYTES (RAW_CHUNK_BYTES + (RAW_CHUNK_BYTES + 127) / 128)
#define CHUNK_HEADER_BYTES 26
#define META_HEADER_BYTES 24
#define META_VERSION 1
#define CHUNK_VERSION 2

/* Native RCC1 version 1 used private IDs 0..11. Version 2 stores Beta IDs.
 * Convert only after the original checksum has been verified. */
static uint8_t native_v1_to_beta(uint8_t id)
{
    static const uint8_t ids[12] = {
        BLOCK_AIR, BLOCK_STONE, BLOCK_DIRT, BLOCK_GRASS,
        BLOCK_SAND, BLOCK_GRAVEL, BLOCK_COBBLESTONE, BLOCK_WOOD,
        BLOCK_LEAVES, BLOCK_WATER, BLOCK_GLASS, BLOCK_TORCH
    };
    return ids[id];
}

static int join_path(char *out, size_t cap, const char *parent, const char *child)
{
    size_t len;
    int n;
    if (!out || !cap || !parent || !child || !parent[0]) return 0;
    len=strlen(parent);
    n=snprintf(out,cap,"%s%s%s",parent,
               (parent[len-1]=='/' || parent[len-1]=='\\') ? "" : "/",child);
    return n>=0 && (size_t)n<cap;
}

static int chunk_path(const World *world, int32_t cx, int32_t cz,
                      char *out, size_t cap)
{
    char name[64];
    int n=snprintf(name,sizeof(name),"chunk_%ld_%ld.rcg",(long)cx,(long)cz);
    return n>=0 && (size_t)n<sizeof(name) &&
           join_path(out,cap,world->path,name);
}

static int temporary_path(char *out, size_t cap, const char *path)
{
    int n=snprintf(out,cap,"%s.tmp",path);
    return n>=0 && (size_t)n<cap;
}

static uint16_t read_u16(const uint8_t *p)
{
    return (uint16_t)((uint16_t)p[0] | (uint16_t)((uint16_t)p[1]<<8));
}

static uint32_t read_u32(const uint8_t *p)
{
    return (uint32_t)p[0] | ((uint32_t)p[1]<<8) |
           ((uint32_t)p[2]<<16) | ((uint32_t)p[3]<<24);
}

static uint64_t read_u64(const uint8_t *p)
{
    return (uint64_t)read_u32(p) | ((uint64_t)read_u32(p+4)<<32);
}

static void write_u16(uint8_t *p, uint16_t v)
{
    p[0]=(uint8_t)v;
    p[1]=(uint8_t)(v>>8);
}

static void write_u32(uint8_t *p, uint32_t v)
{
    p[0]=(uint8_t)v;
    p[1]=(uint8_t)(v>>8);
    p[2]=(uint8_t)(v>>16);
    p[3]=(uint8_t)(v>>24);
}

static void write_u64(uint8_t *p, uint64_t v)
{
    write_u32(p,(uint32_t)v);
    write_u32(p+4,(uint32_t)(v>>32));
}

static uint32_t checksum(const uint8_t *data, size_t size)
{
    uint32_t h=UINT32_C(2166136261);
    size_t i;
    for (i=0; i<size; ++i) {
        h^=data[i];
        h*=UINT32_C(16777619);
    }
    return h;
}

static int is_directory(const char *path)
{
#ifdef _WIN32
    DWORD flags=GetFileAttributesA(path);
    return flags!=INVALID_FILE_ATTRIBUTES &&
           (flags&FILE_ATTRIBUTE_DIRECTORY)!=0 &&
           (flags&FILE_ATTRIBUTE_REPARSE_POINT)==0;
#else
    struct stat st;
    return lstat(path,&st)==0 && S_ISDIR(st.st_mode);
#endif
}

static int make_directory(const char *path)
{
#ifdef _WIN32
    return _mkdir(path)==0;
#else
    return mkdir(path,0775)==0;
#endif
}

static int remove_directory(const char *path)
{
#ifdef _WIN32
    return _rmdir(path)==0;
#else
    return rmdir(path)==0;
#endif
}

static int replace_file(const char *temp, const char *final)
{
#ifdef _WIN32
    return MoveFileExA(temp,final,MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH)!=0;
#else
    return rename(temp,final)==0;
#endif
}

int world_valid_id(const char *id)
{
    size_t i;
    if (!id || !id[0]) return 0;
    for (i=0; id[i]; ++i) {
        unsigned char c=(unsigned char)id[i];
        if (i>=WORLD_ID_MAX) return 0;
        if (!((c>='a' && c<='z') || (c>='A' && c<='Z') ||
              (c>='0' && c<='9') || c=='_' || c=='-')) return 0;
    }
    return 1;
}

static WorldError read_info_path(const char *path, const char *id, WorldInfo *info)
{
    uint8_t bytes[META_HEADER_BYTES+WORLD_NAME_MAX+4];
    size_t len,expected;
    FILE *file=fopen(path,"rb");
    if (!file) return errno==ENOENT ? WORLD_ERROR_NOT_FOUND : WORLD_ERROR_IO;
    len=fread(bytes,1,sizeof(bytes),file);
    if (ferror(file)) {
        fclose(file);
        return WORLD_ERROR_IO;
    }
    if (fgetc(file)!=EOF) {
        fclose(file);
        return WORLD_ERROR_CORRUPT;
    }
    fclose(file);
    if (len<META_HEADER_BYTES+1+4 || memcmp(bytes,"RCW1",4)!=0 ||
        read_u16(bytes+4)!=META_VERSION || (bytes[6]&~7u)!=0) return WORLD_ERROR_CORRUPT;
    expected=META_HEADER_BYTES+(size_t)bytes[7]+4;
    if (!bytes[7] || bytes[7]>WORLD_NAME_MAX || len!=expected ||
        checksum(bytes,len-4)!=read_u32(bytes+len-4) ||
        memchr(bytes+META_HEADER_BYTES,0,bytes[7])!=NULL) return WORLD_ERROR_CORRUPT;
    memset(info,0,sizeof(*info));
    strcpy(info->id,id);
    memcpy(info->name,bytes+META_HEADER_BYTES,bytes[7]);
    info->name[bytes[7]]=0;
    info->flat=(uint8_t)(bytes[6]&1u);
    info->creative=(uint8_t)((bytes[6]>>1)&1u);
    info->structures=(uint8_t)((bytes[6]&4u)==0);
    info->seed=read_u64(bytes+8);
    info->last_played=read_u64(bytes+16);
    return WORLD_OK;
}

static WorldError write_info_path(const char *directory, const WorldInfo *info)
{
    char path[WORLD_PATH_MAX], temp[WORLD_PATH_MAX];
    uint8_t bytes[META_HEADER_BYTES+WORLD_NAME_MAX+4];
    size_t name_len, size;
    FILE *file;
    time_t now;
    if (!join_path(path,sizeof(path),directory,"world.dat") ||
        !temporary_path(temp,sizeof(temp),path)) return WORLD_ERROR_PATH_TOO_LONG;
    name_len=strlen(info->name);
    if (!name_len || name_len>WORLD_NAME_MAX) return WORLD_ERROR_INVALID_ARGUMENT;
    size=META_HEADER_BYTES+name_len+4;
    memset(bytes,0,size);
    memcpy(bytes,"RCW1",4);
    write_u16(bytes+4,META_VERSION);
    bytes[6]=(uint8_t)((info->flat!=0) | ((info->creative!=0)<<1) | ((info->structures==0)<<2));
    bytes[7]=(uint8_t)name_len;
    write_u64(bytes+8,info->seed);
    now=time(NULL);
    write_u64(bytes+16,now<0 ? 0 : (uint64_t)now);
    memcpy(bytes+META_HEADER_BYTES,info->name,name_len);
    write_u32(bytes+size-4,checksum(bytes,size-4));
    file=fopen(temp,"wb");
    if (!file) return WORLD_ERROR_IO;
    {
        int good=fwrite(bytes,1,size,file)==size && fflush(file)==0;
        if (fclose(file)!=0) good=0;
        if (!good) {
            remove(temp);
            return WORLD_ERROR_IO;
        }
    }
    if (!replace_file(temp,path)) {
        remove(temp);
        return WORLD_ERROR_IO;
    }
    return WORLD_OK;
}

WorldError world_storage_write_info(const World *world)
{
    WorldInfo info;
    if (!world || !world->persistent || !world->path[0]) return WORLD_ERROR_INVALID_ARGUMENT;
    memset(&info,0,sizeof(info));
    strcpy(info.id,world->id);
    strcpy(info.name,world->name);
    info.seed=world->seed;
    info.flat=world->flat;
    info.creative=world->creative;
    info.structures=world->structures;
    return write_info_path(world->path,&info);
}

WorldError world_storage_make(const char *saves_dir, const WorldInfo *info,
                              char *path, size_t path_capacity)
{
    WorldError err;
    if (!saves_dir || !info || !path || !world_valid_id(info->id) ||
        !info->name[0] || strlen(info->name)>WORLD_NAME_MAX)
        return WORLD_ERROR_INVALID_ARGUMENT;
    if (!is_directory(saves_dir) && !make_directory(saves_dir))
        return WORLD_ERROR_IO;
    if (!join_path(path,path_capacity,saves_dir,info->id))
        return WORLD_ERROR_PATH_TOO_LONG;
    if (!make_directory(path))
        return is_directory(path) ? WORLD_ERROR_EXISTS : WORLD_ERROR_IO;
    err=write_info_path(path,info);
    if (err!=WORLD_OK) remove_directory(path);
    return err;
}

WorldError world_storage_open(const char *saves_dir, const char *id,
                              WorldInfo *info, char *path, size_t path_capacity)
{
    char meta[WORLD_PATH_MAX];
    if (!saves_dir || !info || !path || !world_valid_id(id))
        return WORLD_ERROR_INVALID_ARGUMENT;
    if (!join_path(path,path_capacity,saves_dir,id) ||
        !join_path(meta,sizeof(meta),path,"world.dat"))
        return WORLD_ERROR_PATH_TOO_LONG;
    if (!is_directory(path)) return WORLD_ERROR_NOT_FOUND;
    return read_info_path(meta,id,info);
}

WorldError world_storage_rename(const char *saves_dir, const char *id, const char *name)
{
    WorldInfo info;
    char path[WORLD_PATH_MAX];
    WorldError err;
    if (!name || !name[0] || strlen(name)>WORLD_NAME_MAX) return WORLD_ERROR_INVALID_ARGUMENT;
    err=world_storage_open(saves_dir,id,&info,path,sizeof(path));
    if (err!=WORLD_OK) return err;
    strcpy(info.name,name);
    return write_info_path(path,&info);
}

static size_t rle_encode(const uint8_t *raw, uint8_t *encoded)
{
    size_t i=0, out=0;
    while (i<RAW_CHUNK_BYTES) {
        size_t run=1;
        while (i+run<RAW_CHUNK_BYTES && run<128 && raw[i+run]==raw[i]) ++run;
        if (run>=3) {
            encoded[out++]=(uint8_t)(128u | (uint8_t)(run-1));
            encoded[out++]=raw[i];
            i+=run;
        } else {
            size_t start=i, length;
            i+=run;
            while (i<RAW_CHUNK_BYTES && i-start<128) {
                run=1;
                while (i+run<RAW_CHUNK_BYTES && run<128 && raw[i+run]==raw[i]) ++run;
                if (run>=3 || i+run-start>128) break;
                i+=run;
            }
            length=i-start;
            encoded[out++]=(uint8_t)(length-1);
            memcpy(encoded+out,raw+start,length);
            out+=length;
        }
    }
    return out;
}

static int rle_decode(const uint8_t *encoded, size_t encoded_len, uint8_t *raw)
{
    size_t i=0,out=0;
    while (i<encoded_len) {
        uint8_t tag=encoded[i++];
        size_t length=(size_t)(tag&127u)+1;
        if (out+length>RAW_CHUNK_BYTES) return 0;
        if (tag&128u) {
            if (i>=encoded_len) return 0;
            memset(raw+out,encoded[i++],length);
        } else {
            if (i+length>encoded_len) return 0;
            memcpy(raw+out,encoded+i,length);
            i+=length;
        }
        out+=length;
    }
    return out==RAW_CHUNK_BYTES;
}

WorldError world_storage_write_chunk(const World *world, const Chunk *chunk)
{
    char path[WORLD_PATH_MAX],temp[WORLD_PATH_MAX];
    uint8_t header[CHUNK_HEADER_BYTES];
    uint8_t *raw,*encoded;
    size_t encoded_len;
    FILE *file;
    int good;
    if (!world || !chunk || !world->persistent) return WORLD_ERROR_INVALID_ARGUMENT;
    if (!chunk_path(world,chunk->x,chunk->z,path,sizeof(path)) ||
        !temporary_path(temp,sizeof(temp),path)) return WORLD_ERROR_PATH_TOO_LONG;
    raw=(uint8_t *)malloc(RAW_CHUNK_BYTES);
    encoded=(uint8_t *)malloc(MAX_RLE_BYTES);
    if (!raw || !encoded) {
        free(raw);
        free(encoded);
        return WORLD_ERROR_OUT_OF_MEMORY;
    }
    memcpy(raw,chunk->blocks,WORLD_CHUNK_VOLUME);
    memcpy(raw+WORLD_CHUNK_VOLUME,chunk->metadata,WORLD_NIBBLE_BYTES);
    memcpy(raw+WORLD_CHUNK_VOLUME+WORLD_NIBBLE_BYTES,chunk->block_light,WORLD_NIBBLE_BYTES);
    memcpy(raw+WORLD_CHUNK_VOLUME+2*WORLD_NIBBLE_BYTES,chunk->sky_light,WORLD_NIBBLE_BYTES);
    encoded_len=rle_encode(raw,encoded);
    if (encoded_len>MAX_RLE_BYTES) {
        free(raw);
        free(encoded);
        return WORLD_ERROR_CORRUPT;
    }
    memcpy(header,"RCC1",4);
    write_u16(header+4,CHUNK_VERSION);
    write_u32(header+6,(uint32_t)chunk->x);
    write_u32(header+10,(uint32_t)chunk->z);
    write_u32(header+14,RAW_CHUNK_BYTES);
    write_u32(header+18,(uint32_t)encoded_len);
    write_u32(header+22,checksum(raw,RAW_CHUNK_BYTES));
    file=fopen(temp,"wb");
    if (!file) {
        free(raw);
        free(encoded);
        return WORLD_ERROR_IO;
    }
    good=fwrite(header,1,sizeof(header),file)==sizeof(header) &&
         fwrite(encoded,1,encoded_len,file)==encoded_len &&
         fflush(file)==0;
    if (fclose(file)!=0) good=0;
    free(raw);
    free(encoded);
    if (!good || !replace_file(temp,path)) {
        remove(temp);
        return WORLD_ERROR_IO;
    }
    return block_entities_native_write(world,chunk) ? WORLD_OK : WORLD_ERROR_IO;
}

WorldError world_storage_read_chunk(const World *world, Chunk *chunk)
{
    char path[WORLD_PATH_MAX];
    uint8_t header[CHUNK_HEADER_BYTES];
    uint8_t *raw,*encoded;
    uint32_t encoded_len;
    uint16_t version;
    size_t i;
    FILE *file;
    WorldError result=WORLD_OK;
    if (!world || !chunk || !world->persistent) return WORLD_ERROR_INVALID_ARGUMENT;
    if (!chunk_path(world,chunk->x,chunk->z,path,sizeof(path)))
        return WORLD_ERROR_PATH_TOO_LONG;
    file=fopen(path,"rb");
    if (!file) return errno==ENOENT ? WORLD_ERROR_NOT_FOUND : WORLD_ERROR_IO;
    if (fread(header,1,sizeof(header),file)!=sizeof(header)) {
        result=ferror(file) ? WORLD_ERROR_IO : WORLD_ERROR_CORRUPT;
        goto done;
    }
    encoded_len=read_u32(header+18);
    version=read_u16(header+4);
    if (memcmp(header,"RCC1",4)!=0 ||
        (version!=1 && version!=CHUNK_VERSION) ||
        read_u32(header+6)!=(uint32_t)chunk->x ||
        read_u32(header+10)!=(uint32_t)chunk->z ||
        read_u32(header+14)!=RAW_CHUNK_BYTES ||
        encoded_len==0 || encoded_len>MAX_RLE_BYTES) {
        result=WORLD_ERROR_CORRUPT;
        goto done;
    }
    raw=(uint8_t *)malloc(RAW_CHUNK_BYTES);
    encoded=(uint8_t *)malloc(encoded_len);
    if (!raw || !encoded) {
        free(raw);
        free(encoded);
        result=WORLD_ERROR_OUT_OF_MEMORY;
        goto done;
    }
    if (fread(encoded,1,encoded_len,file)!=encoded_len || fgetc(file)!=EOF ||
        !rle_decode(encoded,encoded_len,raw) ||
        checksum(raw,RAW_CHUNK_BYTES)!=read_u32(header+22)) {
        result=ferror(file) ? WORLD_ERROR_IO : WORLD_ERROR_CORRUPT;
    } else {
        for (i=0; i<WORLD_CHUNK_VOLUME; ++i) {
            if (raw[i] >= (version==1 ? 12u : (unsigned)BLOCK_COUNT)) {
                result=WORLD_ERROR_CORRUPT;
                break;
            }
        }
        if (result==WORLD_OK) {
            if (version==1) {
                for (i=0; i<WORLD_CHUNK_VOLUME; ++i)
                    raw[i]=native_v1_to_beta(raw[i]);
            }
            memcpy(chunk->blocks,raw,WORLD_CHUNK_VOLUME);
            memcpy(chunk->metadata,raw+WORLD_CHUNK_VOLUME,WORLD_NIBBLE_BYTES);
            memcpy(chunk->block_light,raw+WORLD_CHUNK_VOLUME+WORLD_NIBBLE_BYTES,WORLD_NIBBLE_BYTES);
            memcpy(chunk->sky_light,raw+WORLD_CHUNK_VOLUME+2*WORLD_NIBBLE_BYTES,WORLD_NIBBLE_BYTES);
            chunk->dirty_flags=CHUNK_DIRTY_MESH |
                               (version==1 ? CHUNK_DIRTY_SAVE : 0u);
            chunk->revision=1;
        }
    }
    free(raw);
    free(encoded);
done:
    fclose(file);
    if (result==WORLD_OK && !block_entities_native_read(world,chunk)) result=WORLD_ERROR_CORRUPT;
    return result;
}

size_t world_storage_list(const char *saves_dir, WorldInfo *out, size_t capacity)
{
    size_t count=0;
    if (!saves_dir || !out || !capacity) return 0;
#ifdef _WIN32
    {
        char pattern[WORLD_PATH_MAX];
        WIN32_FIND_DATAA data;
        HANDLE handle;
        if (!join_path(pattern,sizeof(pattern),saves_dir,"*")) return 0;
        handle=FindFirstFileA(pattern,&data);
        if (handle==INVALID_HANDLE_VALUE) return 0;
        do {
            WorldInfo info;
            char path[WORLD_PATH_MAX];
            if (!(data.dwFileAttributes&FILE_ATTRIBUTE_DIRECTORY) ||
                (data.dwFileAttributes&FILE_ATTRIBUTE_REPARSE_POINT) ||
                !world_valid_id(data.cFileName)) continue;
            if (world_storage_open(saves_dir,data.cFileName,&info,path,sizeof(path))!=WORLD_OK)
                continue;
            out[count++]=info;
        } while (count<capacity && FindNextFileA(handle,&data));
        FindClose(handle);
    }
#else
    {
        DIR *directory=opendir(saves_dir);
        struct dirent *entry;
        if (!directory) return 0;
        while (count<capacity && (entry=readdir(directory))!=NULL) {
            WorldInfo info;
            char path[WORLD_PATH_MAX];
            if (!world_valid_id(entry->d_name)) continue;
            if (world_storage_open(saves_dir,entry->d_name,&info,path,sizeof(path))!=WORLD_OK)
                continue;
            out[count++]=info;
        }
        closedir(directory);
    }
#endif
    return count;
}

static int allowed_chunk_filename(const char *name)
{
    const char *p=name;
    int numbers=0;
    if (strncmp(p,"chunk_",6)!=0) return 0;
    p+=6;
    while (numbers<2) {
        if (*p=='-') ++p;
        if (*p<'0' || *p>'9') return 0;
        do { ++p; } while (*p>='0' && *p<='9');
        ++numbers;
        if (numbers==1) {
            if (*p!='_') return 0;
            ++p;
        }
    }
    return strcmp(p,".rcg")==0 || strcmp(p,".rcg.tmp")==0 ||
           strcmp(p,".rct")==0 || strcmp(p,".rct.tmp")==0;
}

static int allowed_world_file(const char *name)
{
    return strcmp(name,"world.dat")==0 || strcmp(name,"world.dat.tmp")==0 ||
           strcmp(name,"player.txt")==0 || strcmp(name,"player.tmp")==0 ||
           strcmp(name,"inventory.txt")==0 || strcmp(name,"inventory.tmp")==0 ||
           allowed_chunk_filename(name);
}

#ifdef _WIN32
static WorldError delete_world_files(const char *path, int remove_files)
{
    char pattern[WORLD_PATH_MAX];
    WIN32_FIND_DATAA data;
    HANDLE handle;
    if (!join_path(pattern,sizeof(pattern),path,"*")) return WORLD_ERROR_PATH_TOO_LONG;
    handle=FindFirstFileA(pattern,&data);
    if (handle==INVALID_HANDLE_VALUE) return WORLD_ERROR_IO;
    do {
        char file_path[WORLD_PATH_MAX];
        const char *name=data.cFileName;
        if (strcmp(name,".")==0 || strcmp(name,"..")==0) continue;
        if ((data.dwFileAttributes&(FILE_ATTRIBUTE_DIRECTORY|FILE_ATTRIBUTE_REPARSE_POINT)) ||
            !allowed_world_file(name) ||
            !join_path(file_path,sizeof(file_path),path,name)) {
            FindClose(handle);
            return WORLD_ERROR_IO;
        }
        if (remove_files && !DeleteFileA(file_path)) {
            FindClose(handle);
            return WORLD_ERROR_IO;
        }
    } while (FindNextFileA(handle,&data));
    FindClose(handle);
    return WORLD_OK;
}
#else
static WorldError delete_world_files(const char *path, int remove_files)
{
    DIR *directory=opendir(path);
    struct dirent *entry;
    if (!directory) return WORLD_ERROR_IO;
    while ((entry=readdir(directory))!=NULL) {
        char file_path[WORLD_PATH_MAX];
        struct stat st;
        const char *name=entry->d_name;
        if (strcmp(name,".")==0 || strcmp(name,"..")==0) continue;
        if (!allowed_world_file(name) ||
            !join_path(file_path,sizeof(file_path),path,name) ||
            lstat(file_path,&st)!=0 || !S_ISREG(st.st_mode)) {
            closedir(directory);
            return WORLD_ERROR_IO;
        }
        if (remove_files && unlink(file_path)!=0) {
            closedir(directory);
            return WORLD_ERROR_IO;
        }
    }
    closedir(directory);
    return WORLD_OK;
}
#endif

WorldError world_storage_delete(const char *saves_dir, const char *id)
{
    char path[WORLD_PATH_MAX];
    WorldError err;
    if (!saves_dir || !world_valid_id(id)) return WORLD_ERROR_INVALID_ARGUMENT;
    if (!join_path(path,sizeof(path),saves_dir,id)) return WORLD_ERROR_PATH_TOO_LONG;
    if (!is_directory(path)) return WORLD_ERROR_NOT_FOUND;
    err=delete_world_files(path,0);
    if (err!=WORLD_OK) return err;
    err=delete_world_files(path,1);
    if (err!=WORLD_OK) return err;
    return remove_directory(path) ? WORLD_OK : WORLD_ERROR_IO;
}
