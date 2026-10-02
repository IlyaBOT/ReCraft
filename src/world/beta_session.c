#include "beta_session.h"
#include <stdio.h>
#include <stdint.h>
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#else
#include <sys/time.h>
#endif
static int lock_path(char *path,size_t size,const char *root)
{
    int n; if(!root || !root[0]) return 0;
    n=snprintf(path,size,"%s/session.lock",root); return n>0 && (size_t)n<size;
}
static int read_token(const char *path,int64_t *token)
{
    unsigned char bytes[8]; unsigned i; uint64_t value=0;
    FILE *file=fopen(path,"rb"); int ok;
    if(!file) return 0;
    ok=fread(bytes,1,8,file)==8 && fgetc(file)==EOF && !ferror(file);
    if(fclose(file)!=0) ok=0;
    if(!ok) return 0;
    for(i=0;i<8;++i) value=(value<<8)|bytes[i];
    *token=(int64_t)value; return 1;
}
int beta_session_check(const char *root,int64_t token)
{
    char path[512]; int64_t actual;
    return token!=0 && lock_path(path,sizeof(path),root) && read_token(path,&actual) && actual==token;
}
int beta_session_start(const char *root,int64_t *token)
{
    char path[512]; unsigned char bytes[8]; int64_t now,previous; unsigned i;
    FILE *file; int ok;
    if(!token || !lock_path(path,sizeof(path),root)) return 0;
#ifdef _WIN32
    {
        FILETIME time; ULARGE_INTEGER ticks;
        GetSystemTimeAsFileTime(&time); ticks.LowPart=time.dwLowDateTime; ticks.HighPart=time.dwHighDateTime;
        now=(int64_t)(ticks.QuadPart/10000-UINT64_C(11644473600000));
    }
#else
    {
        struct timeval time;
        if(gettimeofday(&time,NULL)!=0) return 0;
        now=(int64_t)time.tv_sec*1000+time.tv_usec/1000;
    }
#endif
    if(now<=0) return 0;
    /* Distinguish two clients opened within one millisecond or clock rollback. */
    if(read_token(path,&previous) && previous>=now) {
        if(previous==INT64_MAX) return 0;
        now=previous+1;
    }
    for(i=0;i<8;++i) bytes[7-i]=(unsigned char)((uint64_t)now>>(i*8));
    file=fopen(path,"wb"); if(!file) return 0;
    ok=fwrite(bytes,1,8,file)==8 && fflush(file)==0;
    if(fclose(file)!=0) ok=0;
    if(!ok || !beta_session_check(root,now)) return 0;
    *token=now; return 1;
}
