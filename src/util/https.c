#include "https.h"
#include "json.h"
#include "clock.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef _WIN32
#include <windows.h>
#else
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/select.h>
#include <fcntl.h>
#include <signal.h>
#include <pthread.h>
#include <errno.h>
#endif
static int allowed(const char *url)
{
    static const char *hosts[]={"https://login.microsoftonline.com/","https://user.auth.xboxlive.com/",
        "https://xsts.auth.xboxlive.com/","https://api.minecraftservices.com/","https://textures.minecraft.net/","https://sessionserver.mojang.com/","https://api.mojang.com/"};
    unsigned i;for(i=0;i<sizeof(hosts)/sizeof(hosts[0]);++i)if(!strncmp(url,hosts[i],strlen(hosts[i])))return 1;return 0;
}
void https_response_free(HttpsResponse *response)
{if(response->data){memset(response->data,0,response->size);free(response->data);}memset(response,0,sizeof(*response));}
int https_request(const char *url,const char *content_type,const char *body,const char *bearer,HttpsResponse *r,HttpsCancelled cancelled,void *context)
{
    enum{LIMIT=1024*1024};char *config,*quoted;size_t used=0,received=0,cap=262144;int ok=0;double deadline;
#ifdef _WIN32
    HANDLE input_read=NULL,input_write=NULL,output_read=NULL,output_write=NULL,errors=NULL;
    SECURITY_ATTRIBUTES security={sizeof(security),NULL,TRUE};STARTUPINFOA startup;PROCESS_INFORMATION process;
    char curl[1024],command[1200];DWORD n;int started=0;
#else
    int input[2]={-1,-1},output[2]={-1,-1},status=0,started=0;pid_t pid=-1;
    const char *curl=getenv("RECRAFT_CURL");
#endif
    memset(r,0,sizeof(*r));
    if(!allowed(url) || (body&&strlen(body)>65536) || (bearer&&strlen(bearer)>8192))return 0;
    config=(char *)calloc(cap,1);quoted=(char *)malloc(cap);r->data=(unsigned char *)malloc(LIMIT+16);
    if(!config||!quoted||!r->data)goto done;
    used=(size_t)snprintf(config,cap,"silent\nshow-error\nproto = \"=https\"\nconnect-timeout = 10\nmax-time = 30\nwrite-out = \"\\n%%{http_code}\"\n");
    if(!json_quote(quoted,cap,url))goto done;
    used+=(size_t)snprintf(config+used,cap-used,"url = %s\n",quoted);
    if(content_type) {char header[128];snprintf(header,sizeof(header),"Content-Type: %s",content_type);
        if(!json_quote(quoted,cap,header))goto done;
        used+=(size_t)snprintf(config+used,cap-used,"header = %s\n",quoted);}
    if(bearer && *bearer) {char header[8256];snprintf(header,sizeof(header),"Authorization: Bearer %s",bearer);
        if(!json_quote(quoted,cap,header))goto done;
        used+=(size_t)snprintf(config+used,cap-used,"header = %s\n",quoted);memset(header,0,sizeof(header));}
    if(strstr(url,".auth.xboxlive.com/")) used+=(size_t)snprintf(config+used,cap-used,"header = \"x-xbl-contract-version: 1\"\n");
    if(body) {if(!json_quote(quoted,cap,body))goto done;used+=(size_t)snprintf(config+used,cap-used,"data = %s\n",quoted);}
    if(used>=cap)goto done;
    deadline=recraft_now_seconds()+35;
#ifdef _WIN32
    memset(&startup,0,sizeof(startup));memset(&process,0,sizeof(process));
    if(!CreatePipe(&input_read,&input_write,&security,0)||!CreatePipe(&output_read,&output_write,&security,0))goto done;
    SetHandleInformation(input_write,HANDLE_FLAG_INHERIT,0);SetHandleInformation(output_read,HANDLE_FLAG_INHERIT,0);
    errors=CreateFileA("NUL",GENERIC_WRITE,FILE_SHARE_READ|FILE_SHARE_WRITE,&security,OPEN_EXISTING,0,NULL);
    if(errors==INVALID_HANDLE_VALUE){errors=NULL;goto done;}
    startup.cb=sizeof(startup);startup.dwFlags=STARTF_USESTDHANDLES;
    startup.hStdInput=input_read;startup.hStdOutput=output_write;startup.hStdError=errors;
    {const char *override=getenv("RECRAFT_CURL");
     if(override&&strlen(override)<sizeof(curl)&&strlen(override)>3&&override[1]==':'&&override[2]=='\\'&&!strchr(override,'"')) strcpy(curl,override);
     else {n=GetSystemDirectoryA(curl,sizeof(curl)-12);if(!n||n>=sizeof(curl)-12)goto done;strcat(curl,"\\curl.exe");}}
    snprintf(command,sizeof(command),"\"%s\" -q --config -",curl);
    if(!CreateProcessA(curl,command,NULL,NULL,TRUE,CREATE_NO_WINDOW,NULL,NULL,&startup,&process))goto done;
    started=1;CloseHandle(input_read);input_read=NULL;CloseHandle(output_write);output_write=NULL;
    if(!WriteFile(input_write,config,(DWORD)used,&n,NULL)||n!=used)goto done;
    CloseHandle(input_write);input_write=NULL;
    for(;;) {
        DWORD available=0;
        if((cancelled&&cancelled(context))||recraft_now_seconds()>deadline)goto done;
        if(PeekNamedPipe(output_read,NULL,0,NULL,&available,NULL)&&available) {
            if(received>=LIMIT+4)goto done;
            if(available>LIMIT+4-received)available=(DWORD)(LIMIT+4-received);
            if(!ReadFile(output_read,r->data+received,available,&n,NULL))goto done;
            received+=n;
        } else if(WaitForSingleObject(process.hProcess,0)==WAIT_OBJECT_0)break;
        else Sleep(20);
    }
    {DWORD exit_code;if(!GetExitCodeProcess(process.hProcess,&exit_code)||exit_code)goto done;}
#else
    if(!curl || curl[0]!='/')curl="/usr/bin/curl";
    if(pipe(input)||pipe(output))goto done;
    fcntl(input[1],F_SETFD,FD_CLOEXEC);fcntl(output[0],F_SETFD,FD_CLOEXEC);
    pid=fork();if(pid<0)goto done;
    if(!pid) {
        int nullfd=open("/dev/null",O_WRONLY);
        dup2(input[0],STDIN_FILENO);dup2(output[1],STDOUT_FILENO);if(nullfd>=0)dup2(nullfd,STDERR_FILENO);
        close(input[0]);close(input[1]);close(output[0]);close(output[1]);if(nullfd>2)close(nullfd);
        execl(curl,curl,"-q","--config","-",(char *)NULL);_exit(127);
    }
    started=1;close(input[0]);input[0]=-1;close(output[1]);output[1]=-1;
    /* Ignore SIGPIPE per-thread while writing to a helper that may reject its
     * config. The rest of the client retains its signal policy. */
    {sigset_t set,previous;size_t sent=0;int had_pending;sigset_t pending;
     sigemptyset(&set);sigaddset(&set,SIGPIPE);pthread_sigmask(SIG_BLOCK,&set,&previous);sigpending(&pending);had_pending=sigismember(&pending,SIGPIPE);
     while(sent<used){ssize_t n=write(input[1],config+sent,used-sent);if(n<0){if(errno==EINTR)continue;break;}sent+=(size_t)n;}
     if(!had_pending){int caught;sigpending(&pending);if(sigismember(&pending,SIGPIPE))sigwait(&set,&caught);}
     pthread_sigmask(SIG_SETMASK,&previous,NULL);if(sent!=used)goto done;}
    close(input[1]);input[1]=-1;
    for(;;) {
        fd_set set;struct timeval wait={0,100000};int ready;ssize_t n;
        if((cancelled&&cancelled(context))||recraft_now_seconds()>deadline)goto done;
        FD_ZERO(&set);FD_SET(output[0],&set);ready=select(output[0]+1,&set,NULL,NULL,&wait);
        if(ready<0){if(errno==EINTR)continue;goto done;}if(!ready)continue;
        if(received>=LIMIT+4)goto done;
        n=read(output[0],r->data+received,LIMIT+4-received);if(n<0){if(errno==EINTR)continue;goto done;}if(!n)break;received+=(size_t)n;
    }
    for(;;) {
        pid_t result=waitpid(pid,&status,WNOHANG);
        if(result==pid){started=0;break;}
        if(result<0){if(errno==EINTR)continue;goto done;}
        if((cancelled&&cancelled(context))||recraft_now_seconds()>deadline)goto done;
        recraft_sleep_seconds(.02);
    }
    if(!WIFEXITED(status)||WEXITSTATUS(status))goto done;
#endif
    if(received<4 || r->data[received-4]!='\n' || r->data[received-3]<'0'||r->data[received-3]>'9'||r->data[received-2]<'0'||r->data[received-2]>'9'||r->data[received-1]<'0'||r->data[received-1]>'9')goto done;
    r->status=(r->data[received-3]-'0')*100+(r->data[received-2]-'0')*10+r->data[received-1]-'0';
    r->size=received-4;r->data[r->size]=0;ok=1;
done:
#ifdef _WIN32
    if(started){if(WaitForSingleObject(process.hProcess,0)!=WAIT_OBJECT_0)TerminateProcess(process.hProcess,1);WaitForSingleObject(process.hProcess,5000);CloseHandle(process.hThread);CloseHandle(process.hProcess);}
    if(input_read)CloseHandle(input_read);
    if(input_write)CloseHandle(input_write);
    if(output_read)CloseHandle(output_read);
    if(output_write)CloseHandle(output_write);
    if(errors)CloseHandle(errors);
#else
    if(started){kill(pid,SIGKILL);while(waitpid(pid,&status,0)<0&&errno==EINTR){}}
    if(input[0]>=0)close(input[0]);
    if(input[1]>=0)close(input[1]);
    if(output[0]>=0)close(output[0]);
    if(output[1]>=0)close(output[1]);
#endif
    if(config){memset(config,0,cap);free(config);}if(quoted){memset(quoted,0,cap);free(quoted);}
    if(!ok)https_response_free(r);
    return ok;
}
