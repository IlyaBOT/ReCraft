#include "world/beta_session.h"
#include <assert.h>
#include <stdio.h>
#include <time.h>
#ifdef _WIN32
#include <direct.h>
#include <process.h>
#define make_dir(p) _mkdir(p)
#define remove_dir(p) _rmdir(p)
#define test_pid() _getpid()
#else
#include <sys/stat.h>
#include <unistd.h>
#define make_dir(p) mkdir(p,0700)
#define remove_dir(p) rmdir(p)
#define test_pid() getpid()
#endif
int main(void)
{
    char root[128],path[160]; int64_t first,second; FILE *f;
    uint64_t parsed=0; unsigned i; unsigned char bytes[8];
    snprintf(root,sizeof(root),"build/session-test-%ld-%d",(long)time(NULL),(int)test_pid());
    snprintf(path,sizeof(path),"%s/session.lock",root); assert(make_dir(root)==0);
    assert(!beta_session_check(root,1) && beta_session_start(root,&first));
    f=fopen(path,"rb"); assert(f && fread(bytes,1,8,f)==8 && fgetc(f)==EOF && fclose(f)==0);
    for(i=0;i<8;++i) parsed=(parsed<<8)|bytes[i];
    assert(parsed==(uint64_t)first && beta_session_check(root,first));
    assert(beta_session_start(root,&second) && second!=first);
    assert(!beta_session_check(root,first) && beta_session_check(root,second));
    f=fopen(path,"wb"); assert(f && fwrite(bytes,1,3,f)==3 && fclose(f)==0);
    assert(!beta_session_check(root,second));
    assert(remove(path)==0 && !beta_session_check(root,second));
    assert(remove_dir(root)==0);
    puts("Beta session token format, competing sessions and missing/corrupt locks passed"); return 0;
}
