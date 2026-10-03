#include "assets/resource_pack.h"
#include "util/game_paths.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <zlib.h>
#ifdef _WIN32
#include <direct.h>
#include <process.h>
#define rmdir _rmdir
#define getpid _getpid
#else
#include <unistd.h>
#endif
static void u16(FILE *f,unsigned n) { assert(fputc(n&255,f)!=EOF && fputc((n>>8)&255,f)!=EOF); }
static void u32(FILE *f,uint32_t n) { u16(f,n&65535); u16(f,n>>16); }
static void zip(const char *path,const char *name,const char *text,int method,int bad_crc)
{
    FILE *f=fopen(path,"wb"); unsigned char compressed[512]; size_t size=strlen(text),names=strlen(name),packed=size;
    uint32_t crc=(uint32_t)crc32(0,(const unsigned char *)text,(uInt)size); long central,end;
    assert(f);
    if(method==8) {
        z_stream s={0}; s.next_in=(unsigned char *)text; s.avail_in=(uInt)size; s.next_out=compressed; s.avail_out=sizeof(compressed);
        assert(deflateInit2(&s,Z_DEFAULT_COMPRESSION,Z_DEFLATED,-MAX_WBITS,8,Z_DEFAULT_STRATEGY)==Z_OK);
        assert(deflate(&s,Z_FINISH)==Z_STREAM_END); packed=s.total_out; deflateEnd(&s);
    } else memcpy(compressed,text,size);
    if(bad_crc) ++crc;
    u32(f,0x04034b50); u16(f,20); u16(f,0); u16(f,method); u16(f,0); u16(f,0);
    u32(f,crc); u32(f,(uint32_t)packed); u32(f,(uint32_t)size); u16(f,(unsigned)names); u16(f,0);
    assert(fwrite(name,1,names,f)==names && fwrite(compressed,1,packed,f)==packed); central=ftell(f);
    u32(f,0x02014b50); u16(f,20); u16(f,20); u16(f,0); u16(f,method); u16(f,0); u16(f,0);
    u32(f,crc); u32(f,(uint32_t)packed); u32(f,(uint32_t)size); u16(f,(unsigned)names); u16(f,0); u16(f,0);
    u16(f,0); u16(f,0); u32(f,0); u32(f,0); assert(fwrite(name,1,names,f)==names); end=ftell(f);
    u32(f,0x06054b50); u16(f,0); u16(f,0); u16(f,1); u16(f,1); u32(f,(uint32_t)(end-central)); u32(f,(uint32_t)central); u16(f,0);
    assert(fclose(f)==0);
}
int main(void)
{
    char root[128],path[160]; const char *ids[]={"texturepacks/stored.ZIP","resourcepacks/deflated.zip","texturepacks/bad.zip","texturepacks/traversal.zip"};
    void *bytes; size_t size; ResourcePackEntry entries[8]; int i;
    snprintf(root,sizeof(root),"pack-test-%d",(int)getpid()); assert(game_ensure_directory(root));
    snprintf(path,sizeof(path),"%s/texturepacks",root); assert(game_ensure_directory(path));
    snprintf(path,sizeof(path),"%s/resourcepacks",root); assert(game_ensure_directory(path));
    for(i=0;i<4;++i) { snprintf(path,sizeof(path),"%s/%s",root,ids[i]); zip(path,i==3 ? "../escape" : "terrain.png","fixture payload",i==1 ? 8 : 0,i==2); }
    snprintf(path,sizeof(path),"%s/texturepacks/folder",root); assert(game_ensure_directory(path));
    strcat(path,"/terrain.png"); { FILE *f=fopen(path,"wb"); assert(f); assert(fputs("folder payload",f)>=0 && fclose(f)==0); }
    resource_pack_init(root); assert(resource_pack_list(entries,8)==5);
    for(i=0;i<2;++i) {
        assert(resource_pack_select(ids[i])); bytes=resource_pack_read("terrain.png",&size);
        assert(bytes && size==15 && !memcmp(bytes,"fixture payload",15)); free(bytes);
        assert(!resource_pack_read("missing.png",&size) && size==0);
        assert(!resource_pack_read("../escape",&size));
    }
    assert(!resource_pack_select("texturepacks/../../escape") && !strcmp(resource_pack_current(),ids[1]));
    assert(resource_pack_select(ids[2]) && !resource_pack_read("terrain.png",&size));
    assert(resource_pack_select(ids[3]) && !resource_pack_read("../escape",&size));
    assert(resource_pack_select("texturepacks/folder")); bytes=resource_pack_read("terrain.png",&size);
    assert(bytes && size==14 && !memcmp(bytes,"folder payload",14)); free(bytes);
    assert(resource_pack_select("") && !resource_pack_read("terrain.png",&size)); resource_pack_shutdown();
    assert(remove(path)==0); snprintf(path,sizeof(path),"%s/texturepacks/folder",root); assert(rmdir(path)==0);
    for(i=0;i<4;++i) { snprintf(path,sizeof(path),"%s/%s",root,ids[i]); assert(remove(path)==0); }
    snprintf(path,sizeof(path),"%s/texturepacks",root); assert(rmdir(path)==0);
    snprintf(path,sizeof(path),"%s/resourcepacks",root); assert(rmdir(path)==0); assert(rmdir(root)==0);
    puts("Directory, stored/deflated ZIP, CRC, path confinement, fallback and discovery passed"); return 0;
}
