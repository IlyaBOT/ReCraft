#include "resource_pack.h"
#include "../util/game_paths.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <sys/stat.h>
#include <zlib.h>
#ifdef _WIN32
#include <windows.h>
#else
#include <dirent.h>
#endif
#define MAX_FILE (32u*1024u*1024u)
typedef struct ZipEntry { char name[512]; uint32_t offset,size,packed,crc; unsigned method,flags; } ZipEntry;
typedef struct Pack { char id[300],path[1024]; ZipEntry *entries; unsigned count; int zip; } Pack;
static char root[768];
static Pack active;
static unsigned u16(const unsigned char *p) { return p[0]|p[1]<<8; }
static uint32_t u32(const unsigned char *p) { return u16(p)|(uint32_t)u16(p+2)<<16; }
static int safe(const char *p)
{
    const char *s=p;
    if(!p || !*p || *p=='/' || strchr(p,'\\') || strchr(p,':')) return 0;
    while(*s) { if((unsigned char)*s<32) return 0; ++s; }
    s=p;
    for(;;) {
        const char *end=strchr(s,'/'); size_t n=end ? (size_t)(end-s) : strlen(s);
        if(!n || (n==1 && s[0]=='.') || (n==2 && s[0]=='.' && s[1]=='.')) return 0;
        if(!end) break;
        s=end+1;
    }
    return 1;
}
static int pack_id(const char *id)
{
    return safe(id) && (!strncmp(id,"texturepacks/",13) || !strncmp(id,"resourcepacks/",14)) && strlen(id)<300;
}
static int zip_name(const char *name)
{
    size_t n=strlen(name);
    return n>=4 && name[n-4]=='.' && (name[n-3]=='z' || name[n-3]=='Z') &&
        (name[n-2]=='i' || name[n-2]=='I') && (name[n-1]=='p' || name[n-1]=='P');
}
static int zip_index(Pack *p)
{
    FILE *f=fopen(p->path,"rb"); unsigned char *tail=NULL,header[46]; long length,start,pos;
    unsigned i,count; int ok=0;
    if(!f || fseek(f,0,SEEK_END) || (length=ftell(f))<22 || length>512L*1024*1024) goto done;
    start=length>65557 ? length-65557 : 0;
    tail=(unsigned char *)malloc((size_t)(length-start));
    if(!tail || fseek(f,start,SEEK_SET) || fread(tail,1,(size_t)(length-start),f)!=(size_t)(length-start)) goto done;
    for(pos=length-start-22;pos>=0;--pos)
        if(u32(tail+pos)==UINT32_C(0x06054b50) && pos+22+(long)u16(tail+pos+20)==length-start) break;
    if(pos<0 || u16(tail+pos+4) || u16(tail+pos+6) || u16(tail+pos+8)!=u16(tail+pos+10)) goto done;
    count=u16(tail+pos+10);
    if(!count || count>8192 || u32(tail+pos+16)>=(uint32_t)length ||
       (uint64_t)u32(tail+pos+16)+u32(tail+pos+12)>(uint64_t)(start+pos)) goto done;
    p->entries=(ZipEntry *)calloc(count,sizeof(*p->entries));
    if(!p->entries || fseek(f,(long)u32(tail+pos+16),SEEK_SET)) goto done;
    for(i=0;i<count;++i) {
        unsigned names,extra,comment; ZipEntry *e=p->entries+p->count;
        if(fread(header,1,46,f)!=46 || u32(header)!=UINT32_C(0x02014b50)) goto done;
        names=u16(header+28); extra=u16(header+30); comment=u16(header+32);
        if(!names || names>=sizeof(e->name)) { if(fseek(f,names+extra+comment,SEEK_CUR)) goto done; continue; }
        if(fread(e->name,1,names,f)!=names || fseek(f,extra+comment,SEEK_CUR)) goto done;
        e->name[names]=0;
        if(strlen(e->name)!=names || !safe(e->name)) continue;
        e->flags=u16(header+8); e->method=u16(header+10); e->crc=u32(header+16);
        e->packed=u32(header+20); e->size=u32(header+24); e->offset=u32(header+42);
        if((e->flags&1) || (e->method!=0 && e->method!=8) || e->size>MAX_FILE || e->packed>MAX_FILE ||
           (uint64_t)e->offset+30+e->packed>(uint64_t)length) continue;
        ++p->count;
    }
    ok=1;
done:
    free(tail); if(f) fclose(f);
    if(!ok) { free(p->entries); p->entries=NULL; p->count=0; }
    return ok;
}
static int open_pack(Pack *p,const char *id)
{
    struct stat st;
    memset(p,0,sizeof(*p));
    if(!pack_id(id) || !game_path_join(p->path,sizeof(p->path),root,id) || stat(p->path,&st)) return 0;
    strcpy(p->id,id);
    if(S_ISDIR(st.st_mode)) return 1;
    if(!zip_name(id)) return 0;
    p->zip=1; return zip_index(p);
}
static void *read_pack(const Pack *p,const char *relative,size_t *size)
{
    FILE *f; unsigned char *out=NULL,*compressed=NULL,h[30]; char path[1600]; size_t length;
    unsigned i; const ZipEntry *e=NULL; int ok=0;
    if(size) *size=0;
    if(!p->id[0] || !safe(relative)) return NULL;
    if(!p->zip) {
        long n;
        if(!game_path_join(path,sizeof(path),p->path,relative) || !(f=fopen(path,"rb"))) return NULL;
        if(!fseek(f,0,SEEK_END) && (n=ftell(f))>=0 && n<=(long)MAX_FILE && !fseek(f,0,SEEK_SET)) {
            out=(unsigned char *)malloc((size_t)n+1);
            if(out && fread(out,1,(size_t)n,f)==(size_t)n) { out[n]=0; if(size) *size=(size_t)n; ok=1; }
        }
        fclose(f); if(!ok) { free(out); out=NULL; } return out;
    }
    for(i=0;i<p->count;++i) if(!strcmp(p->entries[i].name,relative)) { e=p->entries+i; break; }
    if(!e || !(f=fopen(p->path,"rb"))) return NULL;
    length=e->size;
    if(fseek(f,(long)e->offset,SEEK_SET) || fread(h,1,30,f)!=30 || u32(h)!=UINT32_C(0x04034b50) ||
       u16(h+8)!=e->method || (u16(h+6)&1) || fseek(f,u16(h+26)+u16(h+28),SEEK_CUR)) goto done;
    out=(unsigned char *)malloc(length+1); compressed=(unsigned char *)malloc(e->packed ? e->packed : 1);
    if(!out || !compressed || fread(compressed,1,e->packed,f)!=e->packed) goto done;
    if(e->method==0) { if(e->packed!=length) goto done; memcpy(out,compressed,length); }
    else {
        z_stream s; int code; memset(&s,0,sizeof(s));
        s.next_in=compressed; s.avail_in=e->packed; s.next_out=out; s.avail_out=(uInt)length+1;
        if(inflateInit2(&s,-MAX_WBITS)!=Z_OK) goto done;
        code=inflate(&s,Z_FINISH); inflateEnd(&s);
        if(code!=Z_STREAM_END || s.total_out!=length || s.total_in!=e->packed) goto done;
    }
    if(crc32(0,out,(uInt)length)!=e->crc) goto done;
    out[length]=0; if(size) *size=length; ok=1;
done:
    free(compressed); fclose(f); if(!ok) { free(out); out=NULL; } return out;
}
void resource_pack_init(const char *game_root)
{ resource_pack_shutdown(); snprintf(root,sizeof(root),"%s",game_root ? game_root : ""); }
void resource_pack_shutdown(void) { free(active.entries); memset(&active,0,sizeof(active)); }
const char *resource_pack_current(void) { return active.id; }
int resource_pack_select(const char *id)
{
    Pack next;
    if(!id || !*id) { resource_pack_shutdown(); return 1; }
    if(!open_pack(&next,id)) return 0;
    resource_pack_shutdown(); active=next; return 1;
}
void *resource_pack_read(const char *relative,size_t *size) { return read_pack(&active,relative,size); }
void *resource_pack_read_from(const char *id,const char *relative,size_t *size)
{
    Pack p; void *bytes;
    if(!open_pack(&p,id)) { if(size) *size=0; return NULL; }
    bytes=read_pack(&p,relative,size); free(p.entries); return bytes;
}
static void add(ResourcePackEntry *out,int *count,int capacity,const char *dir,const char *name,int directory)
{
    ResourcePackEntry *e;
    if(*count>=capacity || name[0]=='.' || (!directory && !zip_name(name))) return;
    e=out+(*count); memset(e,0,sizeof(*e));
    if(snprintf(e->id,sizeof(e->id),"%s/%s",dir,name)>=(int)sizeof(e->id) || !safe(e->id)) return;
    snprintf(e->name,sizeof(e->name),"%s",name); ++*count;
}
static int compare(const void *a,const void *b) { return strcmp(((const ResourcePackEntry *)a)->name,((const ResourcePackEntry *)b)->name); }
int resource_pack_list(ResourcePackEntry *out,int capacity)
{
    static const char *dirs[]={"texturepacks","resourcepacks"}; int count=0,d;
    if(!out || capacity<1) return 0;
    for(d=0;d<2;++d) {
        char path[1100];
        if(!game_path_join(path,sizeof(path),root,dirs[d])) continue;
#ifdef _WIN32
        {
            WIN32_FIND_DATAA info; HANDLE search; char pattern[1104];
            snprintf(pattern,sizeof(pattern),"%s/*",path); search=FindFirstFileA(pattern,&info);
            if(search==INVALID_HANDLE_VALUE) continue;
            do { add(out,&count,capacity,dirs[d],info.cFileName,!!(info.dwFileAttributes&FILE_ATTRIBUTE_DIRECTORY)); } while(FindNextFileA(search,&info));
            FindClose(search);
        }
#else
        {
            DIR *directory=opendir(path); struct dirent *entry;
            if(!directory) continue;
            while((entry=readdir(directory))!=NULL) {
                struct stat st; char full[1600];
                if(game_path_join(full,sizeof(full),path,entry->d_name) && !stat(full,&st))
                    add(out,&count,capacity,dirs[d],entry->d_name,S_ISDIR(st.st_mode));
            }
            closedir(directory);
        }
#endif
    }
    qsort(out,(size_t)count,sizeof(*out),compare);
    for(d=0;d<count;++d) {
        size_t size; char *bytes=(char *)resource_pack_read_from(out[d].id,"pack.txt",&size);
        if(bytes) { bytes[strcspn(bytes,"\r\n")]=0; snprintf(out[d].description,sizeof(out[d].description),"%s",bytes); free(bytes); }
        else snprintf(out[d].description,sizeof(out[d].description),"%s",!strncmp(out[d].id,"texturepacks/",13) ? "Legacy texture pack" : "Resource pack: compatible textures");
    }
    return count;
}
