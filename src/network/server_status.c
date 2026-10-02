#ifndef _WIN32
#define _POSIX_C_SOURCE 200112L
#endif
#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#include <process.h>
typedef SOCKET StatusSocket;
typedef CRITICAL_SECTION StatusLock;
#define STATUS_INVALID INVALID_SOCKET
#define status_close closesocket
static void lock_init(StatusLock *l){InitializeCriticalSection(l);}
static void lock_take(StatusLock *l){EnterCriticalSection(l);}
static void lock_drop(StatusLock *l){LeaveCriticalSection(l);}
static void lock_free(StatusLock *l){DeleteCriticalSection(l);}
#else
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <sys/time.h>
#include <netdb.h>
#include <netinet/tcp.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <pthread.h>
#include <time.h>
#ifdef __APPLE__
#include <mach/mach_time.h>
#endif
typedef int StatusSocket;
typedef pthread_mutex_t StatusLock;
#define STATUS_INVALID (-1)
#define status_close close
static void lock_init(StatusLock *l){(void)pthread_mutex_init(l,NULL);}
static void lock_take(StatusLock *l){(void)pthread_mutex_lock(l);}
static void lock_drop(StatusLock *l){(void)pthread_mutex_unlock(l);}
static void lock_free(StatusLock *l){(void)pthread_mutex_destroy(l);}
#endif
#include "server_status.h"
#include "protocols/protocol_1_8_47.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct StatusEntry {char host[256];uint16_t port;ServerStatusResult result;} StatusEntry;
typedef struct StatusJob {
    StatusLock lock;
    char host[256];uint16_t port;int protocol,index,cancelled,done,abandoned;
    double deadline;
    ServerStatusResult result;
} StatusJob;
struct ServerStatusBrowser {StatusEntry entry[SERVER_STATUS_MAX_ENTRIES];StatusJob *jobs[4];unsigned revision;};
static double status_time(void)
{
#ifdef _WIN32
    LARGE_INTEGER now,freq;QueryPerformanceCounter(&now);QueryPerformanceFrequency(&freq);return (double)now.QuadPart/freq.QuadPart;
#elif defined(__APPLE__)
    mach_timebase_info_data_t base;mach_timebase_info(&base);
    return (double)mach_absolute_time()*(double)base.numer/(double)base.denom/1000000000.0;
#else
    struct timespec ts;
    if(clock_gettime(CLOCK_MONOTONIC,&ts)==0) return (double)ts.tv_sec+ts.tv_nsec/1000000000.0;
    {
        struct timeval tv;gettimeofday(&tv,NULL);
        return (double)tv.tv_sec+tv.tv_usec/1000000.0;
    }
#endif
}
static int blocked(void)
{
#ifdef _WIN32
    int e=WSAGetLastError();return e==WSAEWOULDBLOCK||e==WSAEINPROGRESS||e==WSAEINTR;
#else
    return errno==EAGAIN||errno==EWOULDBLOCK||errno==EINPROGRESS||errno==EINTR;
#endif
}
static int nonblocking(StatusSocket s)
{
    int tcp_nodelay=1;
    (void)setsockopt(s,IPPROTO_TCP,TCP_NODELAY,(const char *)&tcp_nodelay,sizeof(tcp_nodelay));
#ifdef _WIN32
    u_long yes=1;return ioctlsocket(s,FIONBIO,&yes)==0;
#else
    int flags=fcntl(s,F_GETFL,0);
#ifdef SO_NOSIGPIPE
    int yes=1;(void)setsockopt(s,SOL_SOCKET,SO_NOSIGPIPE,&yes,sizeof(yes));
#endif
    return flags>=0&&fcntl(s,F_SETFL,flags|O_NONBLOCK)==0;
#endif
}
static int cancelled(StatusJob *j)
{int result;lock_take(&j->lock);result=j->cancelled;lock_drop(&j->lock);return result||status_time()>=j->deadline;}
static int wait_socket(StatusJob *j,StatusSocket s,int write)
{
    while(!cancelled(j)) {
        fd_set fds;struct timeval tv={0,100000};int result;FD_ZERO(&fds);FD_SET(s,&fds);
        result=select((int)s+1,write?NULL:&fds,write?&fds:NULL,NULL,&tv);
        if(result>0)return 1;
        if(result<0&&!blocked())return 0;
    }return 0;
}
static int send_all(StatusJob *j,StatusSocket s,const uint8_t *p,size_t n)
{
    while(n&&!cancelled(j)) {int flags=0,sent;
#ifdef MSG_NOSIGNAL
        flags=MSG_NOSIGNAL;
#endif
        sent=(int)send(s,(const char *)p,(int)n,flags);if(sent>0){p+=sent;n-=(size_t)sent;}
        else if(sent<0&&blocked()){if(!wait_socket(j,s,1))return 0;}else return 0;
    }return n==0;
}
static StatusSocket connect_host(StatusJob *j)
{
    struct addrinfo hints,*addresses=NULL,*a;char port[6];StatusSocket s=STATUS_INVALID;int error;
    memset(&hints,0,sizeof(hints));hints.ai_family=AF_UNSPEC;hints.ai_socktype=SOCK_STREAM;hints.ai_protocol=IPPROTO_TCP;
    snprintf(port,sizeof(port),"%u",(unsigned)j->port);error=getaddrinfo(j->host,port,&hints,&addresses);
    if(error||!addresses){snprintf(j->result.error,sizeof(j->result.error),"Hostname lookup failed");return STATUS_INVALID;}
    for(a=addresses;a&&!cancelled(j);a=a->ai_next){double start=status_time();int connected,error_code=0;
#ifdef _WIN32
        int length=sizeof(error_code);
#else
        socklen_t length=sizeof(error_code);
#endif
        s=socket(a->ai_family,a->ai_socktype,a->ai_protocol);if(s==STATUS_INVALID)continue;
        if(!nonblocking(s)){status_close(s);s=STATUS_INVALID;continue;}
        connected=connect(s,a->ai_addr,(int)a->ai_addrlen)==0;
        if(!connected&&blocked()&&wait_socket(j,s,1))connected=getsockopt(s,SOL_SOCKET,SO_ERROR,(char *)&error_code,&length)==0&&!error_code;
        if(connected){j->result.connect_ms=(int)((status_time()-start)*1000.0);break;}
        status_close(s);s=STATUS_INVALID;
    }freeaddrinfo(addresses);return s;
}
static int receive_frame(StatusJob *j,StatusSocket s,uint8_t *rx,size_t *used,P47Frame *frame)
{
    for(;;){uint32_t length;size_t prefix;int r=p47_read_varint(rx,*used,&length,&prefix);
        if(r<0||(r==1&&(prefix>3||length>SERVER_STATUS_JSON_MAX+8)))return 0;
        r=p47_decode_frame(rx,*used,-1,NULL,0,frame);if(r<0)return 0;if(r==1)return 1;
        if(*used>=SERVER_STATUS_JSON_MAX+16||cancelled(j))return 0;
        r=(int)recv(s,(char *)rx+*used,(int)(SERVER_STATUS_JSON_MAX+16-*used),0);
        if(r>0)*used+=(size_t)r;else if(r<0&&blocked()){if(!wait_socket(j,s,0))return 0;}else return 0;
    }
}
static int modern_query(StatusJob *j,StatusSocket s)
{
    uint8_t payload[280],tx[320],ping[8],*rx;size_t n=0,k,used=0,prefix;uint32_t length;P47Frame frame;int ok=0;double sent;
    size_t host_len=strlen(j->host);rx=(uint8_t *)malloc(SERVER_STATUS_JSON_MAX+16);if(!rx)return 0;
    n+=p47_write_varint(payload+n,sizeof(payload)-n,(uint32_t)j->protocol);
    n+=p47_write_varint(payload+n,sizeof(payload)-n,(uint32_t)host_len);memcpy(payload+n,j->host,host_len);n+=host_len;
    payload[n++]=(uint8_t)(j->port>>8);payload[n++]=(uint8_t)j->port;payload[n++]=1;
    k=p47_encode_frame(tx,sizeof(tx),-1,0,payload,n);if(!k||!send_all(j,s,tx,k))goto done;
    k=p47_encode_frame(tx,sizeof(tx),-1,0,NULL,0);if(!k||!send_all(j,s,tx,k)||!receive_frame(j,s,rx,&used,&frame))goto done;
    if(frame.packet_id!=0||p47_read_varint(frame.payload,frame.payload_size,&length,&prefix)!=1||length>SERVER_STATUS_JSON_MAX||length!=frame.payload_size-prefix||
       !server_status_parse_json((const char *)frame.payload+prefix,length,&j->result))goto done;
    used-=frame.frame_size;memmove(rx,rx+frame.frame_size,used);
    /* An opaque millisecond token, compared byte-for-byte with the pong. */
    {uint64_t stamp=(uint64_t)(status_time()*1000.0);int i;for(i=0;i<8;++i)ping[i]=(uint8_t)(stamp>>(56-i*8));}
    k=p47_encode_frame(tx,sizeof(tx),-1,1,ping,8);sent=status_time();
    if(!k||!send_all(j,s,tx,k)||!receive_frame(j,s,rx,&used,&frame)||frame.packet_id!=1||frame.payload_size!=8||memcmp(frame.payload,ping,8))goto done;
    j->result.ping_ms=(int)((status_time()-sent)*1000.0);ok=1;
done:free(rx);return ok;
}
static void job_free(StatusJob *j){lock_free(&j->lock);free(j);}
#ifdef _WIN32
static unsigned __stdcall status_worker(void *arg)
#else
static void *status_worker(void *arg)
#endif
{
    StatusJob *j=(StatusJob *)arg;StatusSocket s=connect_host(j);int ok=0,abandoned;
    if(s!=STATUS_INVALID){if(j->protocol==14){ok=1;j->result.reachability_only=1;strcpy(j->result.version,"Beta 1.7.3");strcpy(j->result.motd,"TCP reachable; Beta has no server-list status");}
        else ok=modern_query(j,s);
        status_close(s);}
    j->result.state=ok?SERVER_STATUS_ONLINE:SERVER_STATUS_ERROR;
    if(!ok){j->result.ping_ms=j->result.connect_ms=j->result.online=j->result.maximum=-1;j->result.icon_size=0;j->result.motd[0]=0;}
    if(!ok&&!j->result.error[0])snprintf(j->result.error,sizeof(j->result.error),cancelled(j)?"Server query timed out":"Cannot query server");
    lock_take(&j->lock);j->done=1;abandoned=j->abandoned;lock_drop(&j->lock);
    if(abandoned)job_free(j);
#ifdef _WIN32
    WSACleanup();return 0;
#else
    return NULL;
#endif
}
ServerStatusBrowser *server_status_create(void)
{
    ServerStatusBrowser *b=(ServerStatusBrowser *)calloc(1,sizeof(*b));
    if(b)server_status_clear(b);
    return b;
}
static void mark_cancel(StatusJob *j){lock_take(&j->lock);j->cancelled=1;lock_drop(&j->lock);}
void server_status_clear(ServerStatusBrowser *b)
{
    int i;if(!b)return;for(i=0;i<4;++i)if(b->jobs[i])mark_cancel(b->jobs[i]);memset(b->entry,0,sizeof(b->entry));
    for(i=0;i<SERVER_STATUS_MAX_ENTRIES;++i)b->entry[i].result.ping_ms=b->entry[i].result.connect_ms=b->entry[i].result.online=b->entry[i].result.maximum=-1;
}
void server_status_destroy(ServerStatusBrowser *b)
{
    int i;if(!b)return;
    for(i=0;i<4;++i)if(b->jobs[i]){StatusJob *j=b->jobs[i];int done;lock_take(&j->lock);done=j->done;j->cancelled=j->abandoned=1;lock_drop(&j->lock);if(done)job_free(j);}
    free(b);
}
int server_status_request(ServerStatusBrowser *b,unsigned index,const char *host,uint16_t port,int protocol)
{
    StatusEntry *e;int i;if(!b||index>=SERVER_STATUS_MAX_ENTRIES||!host||!host[0]||strlen(host)>255||!port||(protocol!=14&&protocol!=47))return 0;
    for(i=0;host[i];++i)if((unsigned char)host[i]<=32||(unsigned char)host[i]==127)return 0;
    for(i=0;i<4;++i)if(b->jobs[i]&&b->jobs[i]->index==(int)index)mark_cancel(b->jobs[i]);
    e=b->entry+index;memset(e,0,sizeof(*e));strcpy(e->host,host);e->port=port;
    e->result.state=SERVER_STATUS_QUEUED;e->result.requested_protocol=protocol;e->result.protocol=protocol;
    e->result.ping_ms=e->result.connect_ms=e->result.online=e->result.maximum=-1;e->result.revision=++b->revision;return 1;
}
const ServerStatusResult *server_status_get(const ServerStatusBrowser *b,unsigned index)
{return b&&index<SERVER_STATUS_MAX_ENTRIES?&b->entry[index].result:NULL;}
void server_status_tick(ServerStatusBrowser *b)
{
    int i,k;if(!b)return;
    for(i=0;i<4;++i)if(b->jobs[i]){StatusJob *j=b->jobs[i];int done,cancel;lock_take(&j->lock);done=j->done;cancel=j->cancelled;
        if(done&&!cancel){b->entry[j->index].result=j->result;b->entry[j->index].result.revision=++b->revision;}
        if(!done&&!cancel&&status_time()>=j->deadline){j->cancelled=1;b->entry[j->index].result.state=SERVER_STATUS_ERROR;
            strcpy(b->entry[j->index].result.error,"Server query timed out");b->entry[j->index].result.revision=++b->revision;}
        lock_drop(&j->lock);if(done){job_free(j);b->jobs[i]=NULL;}
    }
    for(i=0;i<4;++i)if(!b->jobs[i]){StatusJob *j;StatusEntry *e=NULL;
        for(k=0;k<SERVER_STATUS_MAX_ENTRIES;++k)if(b->entry[k].result.state==SERVER_STATUS_QUEUED){e=b->entry+k;break;}
        if(!e)break;
        j=(StatusJob *)calloc(1,sizeof(*j));if(!j)break;lock_init(&j->lock);
        strcpy(j->host,e->host);j->port=e->port;j->protocol=e->result.requested_protocol;j->index=k;j->deadline=status_time()+4.0;j->result=e->result;
        e->result.state=SERVER_STATUS_QUERYING;e->result.revision=++b->revision;
#ifdef _WIN32
        {WSADATA data;uintptr_t thread;if(WSAStartup(MAKEWORD(2,2),&data)!=0){job_free(j);e->result.state=SERVER_STATUS_ERROR;continue;}
            thread=_beginthreadex(NULL,0,status_worker,j,0,NULL);if(!thread){WSACleanup();job_free(j);e->result.state=SERVER_STATUS_ERROR;continue;}CloseHandle((HANDLE)thread);}
#else
        {pthread_t thread;if(pthread_create(&thread,NULL,status_worker,j)!=0){job_free(j);e->result.state=SERVER_STATUS_ERROR;continue;}(void)pthread_detach(thread);}
#endif
        b->jobs[i]=j;
    }
}
