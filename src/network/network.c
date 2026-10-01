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
typedef SOCKET NetSocket;
#define NET_INVALID INVALID_SOCKET
#define net_close closesocket
typedef CRITICAL_SECTION NetLock;
static void lock_init(NetLock *l) { InitializeCriticalSection(l); }
static void lock_take(NetLock *l) { EnterCriticalSection(l); }
static void lock_drop(NetLock *l) { LeaveCriticalSection(l); }
static void lock_free(NetLock *l) { DeleteCriticalSection(l); }
#else
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <sys/time.h>
#include <netdb.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <pthread.h>
typedef int NetSocket;
#define NET_INVALID (-1)
#define net_close close
typedef pthread_mutex_t NetLock;
static void lock_init(NetLock *l) { (void)pthread_mutex_init(l,NULL); }
static void lock_take(NetLock *l) { (void)pthread_mutex_lock(l); }
static void lock_drop(NetLock *l) { (void)pthread_mutex_unlock(l); }
static void lock_free(NetLock *l) { (void)pthread_mutex_destroy(l); }
#endif
#include "network.h"
#include "protocols/protocol_beta_14.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <limits.h>
#include <zlib.h>

#define NET_RX_CAP (BETA14_MAX_PACKET*2u)
#define NET_TX_CAP 8192u
#define NET_MAX_ENTITIES 512
#define NET_MAX_REGION_BLOCKS (WORLD_CHUNK_VOLUME*4u)

typedef struct ResolveJob {
    NetLock lock;
    char host[256],port[6];
    struct addrinfo *addresses;
    int done,abandoned,error;
} ResolveJob;
typedef struct RemoteEntity {
    int used;
    int32_t id;
    double x,y,z;
    float yaw,pitch;
    uint8_t type;
} RemoteEntity;
struct NetworkClient {
    World *world;
    NetworkEventFn callback;
    void *user;
    NetworkState state;
    NetSocket socket;
    ResolveJob *resolver;
    struct addrinfo *addresses,*next_address;
    double deadline,last_receive;
    char username[17],error[512];
    uint8_t rx[NET_RX_CAP],tx[NET_TX_CAP];
    size_t rx_count,tx_count;
    int logged_in;
    RemoteEntity entities[NET_MAX_ENTITIES];
};

static double net_time(void)
{
#ifdef _WIN32
    LARGE_INTEGER now,freq;
    QueryPerformanceCounter(&now); QueryPerformanceFrequency(&freq);
    return (double)now.QuadPart/(double)freq.QuadPart;
#else
    struct timeval tv; gettimeofday(&tv,NULL);
    return (double)tv.tv_sec+(double)tv.tv_usec/1000000.0;
#endif
}
static int would_block(void)
{
#ifdef _WIN32
    int err=WSAGetLastError(); return err==WSAEWOULDBLOCK || err==WSAEINPROGRESS || err==WSAEINTR;
#else
    return errno==EAGAIN || errno==EWOULDBLOCK || errno==EINPROGRESS || errno==EINTR;
#endif
}
static void emit(NetworkClient *c,NetworkEvent *e)
{ e->state=c->state; if(c->callback) c->callback(c->user,e); }
static void set_state(NetworkClient *c,NetworkState state)
{
    NetworkEvent e; c->state=state;
    memset(&e,0,sizeof(e)); e.type=NETWORK_EVENT_STATE; emit(c,&e);
}
static void resolve_free(ResolveJob *job)
{
    if(job->addresses) freeaddrinfo(job->addresses);
    lock_free(&job->lock); free(job);
}
#ifdef _WIN32
static unsigned __stdcall resolve_worker(void *arg)
#else
static void *resolve_worker(void *arg)
#endif
{
    ResolveJob *job=(ResolveJob *)arg;
    struct addrinfo hints,*addresses=NULL; int error,abandoned;
    memset(&hints,0,sizeof(hints)); hints.ai_family=AF_UNSPEC;
    hints.ai_socktype=SOCK_STREAM; hints.ai_protocol=IPPROTO_TCP;
    error=getaddrinfo(job->host,job->port,&hints,&addresses);
    lock_take(&job->lock);
    job->addresses=addresses; job->error=error; job->done=1; abandoned=job->abandoned;
    lock_drop(&job->lock);
    if(abandoned) resolve_free(job);
#ifdef _WIN32
    WSACleanup(); return 0;
#else
    return NULL;
#endif
}
static void stop_transport(NetworkClient *c)
{
    if(c->socket!=NET_INVALID) { net_close(c->socket); c->socket=NET_INVALID; }
    if(c->resolver) {
        ResolveJob *job=c->resolver; int done;
        lock_take(&job->lock); done=job->done; job->abandoned=1; lock_drop(&job->lock);
        if(done) resolve_free(job);
        c->resolver=NULL;
    }
    if(c->addresses) freeaddrinfo(c->addresses);
    c->addresses=c->next_address=NULL;
    c->rx_count=c->tx_count=0; c->logged_in=0;
}
static void fail(NetworkClient *c,const char *reason)
{
    NetworkEvent e;
    snprintf(c->error,sizeof(c->error),"%s",reason);
    stop_transport(c); set_state(c,NETWORK_ERROR);
    memset(&e,0,sizeof(e)); e.type=NETWORK_EVENT_DISCONNECT; e.text=c->error; emit(c,&e);
}
static int queue_bytes(NetworkClient *c,const uint8_t *bytes,size_t count)
{
    if(!count) return 0;
    if(count>NET_TX_CAP-c->tx_count) { fail(c,"Network send queue overflow"); return 0; }
    memcpy(c->tx+c->tx_count,bytes,count); c->tx_count+=count; return 1;
}
static void flush_send(NetworkClient *c)
{
    unsigned budget=4;
    while(c->tx_count && budget--) {
        int flags=0,sent;
#ifdef MSG_NOSIGNAL
        flags=MSG_NOSIGNAL;
#endif
        sent=(int)send(c->socket,(const char *)c->tx,(int)c->tx_count,flags);
        if(sent<0) { if(would_block()) return; fail(c,"TCP send failed"); return; }
        if(!sent) { fail(c,"Server closed the connection"); return; }
        c->tx_count-=(size_t)sent; memmove(c->tx,c->tx+sent,c->tx_count);
    }
}
static int nonblocking(NetSocket s)
{
#ifdef _WIN32
    u_long yes=1; return ioctlsocket(s,FIONBIO,&yes)==0;
#else
    int flags=fcntl(s,F_GETFL,0);
#ifdef SO_NOSIGPIPE
    int yes=1; (void)setsockopt(s,SOL_SOCKET,SO_NOSIGPIPE,&yes,sizeof(yes));
#endif
    return flags>=0 && fcntl(s,F_SETFL,flags|O_NONBLOCK)==0;
#endif
}
static void connected(NetworkClient *c)
{
    uint8_t packet[64]; size_t count;
    if(c->addresses) freeaddrinfo(c->addresses);
    c->addresses=c->next_address=NULL;
    c->deadline=net_time()+20.0; c->last_receive=net_time();
    count=beta14_handshake(packet,sizeof(packet),c->username);
    if(!queue_bytes(c,packet,count)) return;
    set_state(c,NETWORK_HANDSHAKE);
}
static void try_address(NetworkClient *c)
{
    while(c->next_address) {
        struct addrinfo *a=c->next_address; c->next_address=a->ai_next;
        c->socket=socket(a->ai_family,a->ai_socktype,a->ai_protocol);
        if(c->socket==NET_INVALID) continue;
        if(!nonblocking(c->socket)) { net_close(c->socket); c->socket=NET_INVALID; continue; }
        if(connect(c->socket,a->ai_addr,(int)a->ai_addrlen)==0) { connected(c); return; }
        if(would_block()) return;
        net_close(c->socket); c->socket=NET_INVALID;
    }
    fail(c,"Cannot connect to server (all resolved addresses failed)");
}
static void poll_connect(NetworkClient *c)
{
    if(c->resolver) {
        ResolveJob *job=c->resolver; int done,error;
        lock_take(&job->lock); done=job->done; error=job->error;
        if(done) { c->addresses=job->addresses; job->addresses=NULL; }
        lock_drop(&job->lock);
        if(!done) return;
        c->resolver=NULL; resolve_free(job);
        if(error || !c->addresses) { fail(c,"Hostname lookup failed"); return; }
        c->next_address=c->addresses; try_address(c); return;
    }
    if(c->socket!=NET_INVALID) {
        fd_set write_set,error_set; struct timeval wait={0,0}; int result,error=0;
#ifdef _WIN32
        int error_size=sizeof(error);
#else
        socklen_t error_size=sizeof(error);
#endif
        FD_ZERO(&write_set); FD_ZERO(&error_set); FD_SET(c->socket,&write_set); FD_SET(c->socket,&error_set);
        result=select((int)c->socket+1,NULL,&write_set,&error_set,&wait);
        if(result==0) return;
        if(result<0 && would_block()) return;
        if(result>0 && getsockopt(c->socket,SOL_SOCKET,SO_ERROR,(char *)&error,&error_size)==0 && !error) { connected(c); return; }
        net_close(c->socket); c->socket=NET_INVALID; try_address(c);
    }
}

NetworkClient *network_create(World *world,NetworkEventFn callback,void *user)
{
    NetworkClient *c;
    if(!world || !world->cache) return NULL;
#ifdef _WIN32
    { WSADATA data; if(WSAStartup(MAKEWORD(2,2),&data)!=0) return NULL; }
#endif
    c=(NetworkClient *)calloc(1,sizeof(*c));
    if(!c) {
#ifdef _WIN32
        WSACleanup();
#endif
        return NULL;
    }
    c->world=world; c->callback=callback; c->user=user; c->socket=NET_INVALID; return c;
}
void network_destroy(NetworkClient *c)
{
    if(!c) return;
    stop_transport(c); free(c);
#ifdef _WIN32
    WSACleanup();
#endif
}
void network_disconnect(NetworkClient *c)
{
    if(!c) return;
    if(c->socket!=NET_INVALID && c->state>=NETWORK_HANDSHAKE && c->state<=NETWORK_PLAY) {
        static const uint8_t leave[]={255,0,4,0,'Q',0,'u',0,'i',0,'t'};
        if(queue_bytes(c,leave,sizeof(leave))) flush_send(c);
    }
    stop_transport(c); set_state(c,NETWORK_DISCONNECTED);
}
int network_connect(NetworkClient *c,const char *host,uint16_t port,const char *username)
{
    ResolveJob *job; size_t i,len;
    if(!c || !host || !username) return 0;
    stop_transport(c); c->error[0]=0; memset(c->entities,0,sizeof(c->entities));
    if(!port || !host[0] || strlen(host)>255) { fail(c,"Invalid server address or port"); return 0; }
    len=strlen(username);
    if(!len || len>16) { fail(c,"Username must contain 1 to 16 ASCII letters, digits or underscores"); return 0; }
    for(i=0;i<len;++i) { unsigned char ch=(unsigned char)username[i]; if(!((ch>='a'&&ch<='z')||(ch>='A'&&ch<='Z')||(ch>='0'&&ch<='9')||ch=='_')) { fail(c,"Invalid username"); return 0; } }
    if(c->world->persistent || !c->world->network_mode) { fail(c,"Network connection requires an empty nonpersistent network world"); return 0; }
    memcpy(c->username,username,len+1);
    job=(ResolveJob *)calloc(1,sizeof(*job));
    if(!job) { fail(c,"Out of memory resolving hostname"); return 0; }
    lock_init(&job->lock); snprintf(job->host,sizeof(job->host),"%s",host); snprintf(job->port,sizeof(job->port),"%u",(unsigned)port);
#ifdef _WIN32
    { WSADATA data; uintptr_t worker;
      if(WSAStartup(MAKEWORD(2,2),&data)!=0) { resolve_free(job); fail(c,"Cannot initialize resolver"); return 0; }
      worker=_beginthreadex(NULL,0,resolve_worker,job,0,NULL);
      if(!worker) { WSACleanup(); resolve_free(job); fail(c,"Cannot start hostname resolver"); return 0; }
      CloseHandle((HANDLE)worker); }
#else
    { pthread_t worker;
      if(pthread_create(&worker,NULL,resolve_worker,job)!=0) { resolve_free(job); fail(c,"Cannot start hostname resolver"); return 0; }
      (void)pthread_detach(worker); }
#endif
    c->resolver=job; c->deadline=net_time()+10.0; set_state(c,NETWORK_CONNECTING); return 1;
}
NetworkState network_state(const NetworkClient *c) { return c?c->state:NETWORK_DISCONNECTED; }
const char *network_last_error(const NetworkClient *c) { return c?c->error:"No network client"; }

static int floor_chunk(int value) { int q=value/16; return value%16<0?q-1:q; }
static void dirty_neighbors(World *w,Chunk *chunk)
{
    static const int dx[4]={-1,1,0,0},dz[4]={0,0,-1,1}; int i;
    chunk->dirty_flags|=CHUNK_DIRTY_MESH;
    for(i=0;i<4;++i) { Chunk *neighbor=world_peek_chunk(w,chunk->x+dx[i],chunk->z+dz[i]); if(neighbor) { neighbor->dirty_flags|=CHUNK_DIRTY_MESH; ++neighbor->revision; } }
}
static void set_region_nibble(uint8_t *bytes,size_t index,uint8_t value)
{
    size_t at=index>>1;
    value &= 15u;
    bytes[at]=(uint8_t)((index&1u) ? ((bytes[at]&15u)|(value<<4)) :
                                    ((bytes[at]&240u)|value));
}
static void set_remote_block(NetworkClient *c,int x,int y,int z,unsigned id,unsigned meta,int notify)
{
    Chunk *chunk; int cx,cz; NetworkEvent e;
    if(y<0 || y>=WORLD_HEIGHT) return;
    if(id>=BLOCK_COUNT || meta>15) { fail(c,"Server sent an unknown Beta block state"); return; }
    cx=floor_chunk(x); cz=floor_chunk(z); chunk=world_get_chunk(c->world,cx,cz);
    if(!chunk) { fail(c,"Out of memory loading server block"); return; }
    if(!world_set_block(c->world,x,y,z,(uint8_t)id) ||
       !world_set_metadata(c->world,x,y,z,(uint8_t)meta)) {
        fail(c,"Cannot apply server block update"); return;
    }
    if(notify) { memset(&e,0,sizeof(e)); e.type=NETWORK_EVENT_BLOCK; e.block_x=x; e.block_y=y; e.block_z=z; e.block_id=(uint8_t)id; e.metadata=(uint8_t)meta; emit(c,&e); }
}
static int receive_region(NetworkClient *c,const uint8_t *p)
{
    int x=beta14_i32(p+1),y=(int16_t)beta14_u16(p+5),z=beta14_i32(p+7);
    int width=p[11]+1,height=p[12]+1,depth=p[13]+1,ix,iz,iy;
    size_t count=(size_t)width*height*depth,nibbles=(count+1)/2,expected=count+3*nibbles,idx;
    uint8_t *data; z_stream stream; int result;
    Chunk *last=NULL; int last_cx=INT_MAX,last_cz=INT_MAX;
    if(y<0 || y+height>WORLD_HEIGHT || count>NET_MAX_REGION_BLOCKS || x>INT_MAX-width || z>INT_MAX-depth) { fail(c,"Server chunk region exceeds supported bounds"); return 0; }
    data=(uint8_t *)malloc(expected);
    if(!data) { fail(c,"Out of memory inflating server chunk"); return 0; }
    memset(&stream,0,sizeof(stream)); stream.next_in=(Bytef *)(p+18); stream.avail_in=beta14_u32(p+14); stream.next_out=data; stream.avail_out=(uInt)expected;
    result=inflateInit(&stream);
    if(result==Z_OK) { result=inflate(&stream,Z_FINISH); (void)inflateEnd(&stream); }
    if(result!=Z_STREAM_END || stream.total_out!=expected || stream.avail_in!=0) { free(data); fail(c,"Malformed zlib chunk data"); return 0; }
    for(idx=0;idx<count;++idx) {
        if(data[idx]>=BLOCK_COUNT) {
            free(data); fail(c,"Server sent an unknown Beta block ID"); return 0;
        }
    }
    for(ix=0;ix<width;++ix) for(iz=0;iz<depth;++iz) {
        int wx=x+ix,wz=z+iz,cx=floor_chunk(wx),cz=floor_chunk(wz),lx=wx-cx*16,lz=wz-cz*16;
        if(cx!=last_cx || cz!=last_cz) {
            if(last) { ++last->revision; dirty_neighbors(c->world,last); }
            last=world_get_chunk(c->world,cx,cz); last_cx=cx; last_cz=cz;
            if(!last) { free(data); fail(c,"Out of memory applying server chunk"); return 0; }
        }
        for(iy=0;iy<height;++iy) {
            unsigned shift; size_t local;
            idx=((size_t)ix*depth+iz)*height+iy; shift=(unsigned)(idx&1)*4;
            local=(size_t)lx+(size_t)lz*16u+(size_t)(y+iy)*256u;
            last->blocks[local]=data[idx];
            set_region_nibble(last->metadata,local,(uint8_t)(data[count+idx/2]>>shift));
            set_region_nibble(last->block_light,local,(uint8_t)(data[count+nibbles+idx/2]>>shift));
            set_region_nibble(last->sky_light,local,(uint8_t)(data[count+nibbles*2+idx/2]>>shift));
        }
    }
    if(last) { ++last->revision; dirty_neighbors(c->world,last); }
    free(data); return 1;
}
static RemoteEntity *entity_find(NetworkClient *c,int32_t id,int create)
{
    int i; RemoteEntity *free_slot=NULL;
    for(i=0;i<NET_MAX_ENTITIES;++i) { RemoteEntity *e=c->entities+i; if(e->used && e->id==id) return e; if(!e->used && !free_slot) free_slot=e; }
    if(create && free_slot) { memset(free_slot,0,sizeof(*free_slot)); free_slot->used=1; free_slot->id=id; return free_slot; }
    return NULL;
}
static void entity_event(NetworkClient *c,RemoteEntity *r,NetworkEventType type,const char *name)
{
    NetworkEvent e; memset(&e,0,sizeof(e)); e.type=type; e.entity_id=r->id; e.entity_type=r->type;
    e.x=r->x; e.y=r->y; e.z=r->z; e.yaw=r->yaw; e.pitch=r->pitch; e.text=name; emit(c,&e);
}
static void inventory_slot(NetworkClient *c,const uint8_t *p,int window,int slot)
{
    NetworkEvent e; memset(&e,0,sizeof(e)); e.type=NETWORK_EVENT_INVENTORY; e.entity_type=(uint8_t)window; e.slot=(int16_t)slot;
    e.item_id=(int16_t)beta14_u16(p); if(e.item_id!=-1) { e.item_count=p[2]; e.item_damage=(int16_t)beta14_u16(p+3); } emit(c,&e);
}
static void handle_packet(NetworkClient *c,const Beta14Packet *packet)
{
    const uint8_t *p=packet->bytes; NetworkEvent e; char text[4096]; size_t off,i; uint8_t response[128];
    memset(&e,0,sizeof(e));
    if(p[0]==0xff) { beta14_read_string(packet,1,text,sizeof(text)); fail(c,text); return; }
    if(p[0]==0x00) { queue_bytes(c,p,1); return; }
    if(c->state==NETWORK_HANDSHAKE) {
        if(p[0]!=2) { fail(c,"Expected protocol 14 handshake response"); return; }
        if(!beta14_read_string(packet,1,text,sizeof(text)) || strcmp(text,"-")!=0) { fail(c,"Server requires online authentication; only offline-mode protocol 14 is supported"); return; }
        off=beta14_login(response,sizeof(response),c->username); if(queue_bytes(c,response,off)) set_state(c,NETWORK_LOGIN); return;
    }
    if(p[0]==0x01) {
        if(c->state!=NETWORK_LOGIN || c->logged_in) { fail(c,"Unexpected login packet"); return; }
        off=beta14_read_string(packet,5,text,sizeof(text));
        if(!off || (int8_t)p[off+8]!=0) { fail(c,"Only protocol 14 overworld sessions are supported"); return; }
        c->world->seed=(uint64_t)beta14_u32(p+off)<<32 | beta14_u32(p+off+4); c->logged_in=1; return;
    }
    if(!c->logged_in) { fail(c,"Received gameplay packet before login response"); return; }
    switch(p[0]) {
    case 0x03: beta14_read_string(packet,1,text,sizeof(text)); e.type=NETWORK_EVENT_CHAT; e.text=text; emit(c,&e); break;
    case 0x08: e.type=NETWORK_EVENT_HEALTH; e.health=(int16_t)beta14_u16(p+1); emit(c,&e); break;
    case 0x09: fail(c,"Server dimension/respawn transition is not implemented"); break;
    case 0x0d:
        /* Beta 14 clientbound: feet Y, camera Y. Serverbound reverses them. */
        e.type=NETWORK_EVENT_POSITION; e.x=beta14_f64(p+1); e.y=beta14_f64(p+9); e.z=beta14_f64(p+25);
        e.yaw=beta14_f32(p+33); e.pitch=beta14_f32(p+37);
        if(!isfinite(e.x)||!isfinite(e.y)||!isfinite(e.z)||!isfinite(e.yaw)||!isfinite(e.pitch)||fabs(e.x)>32000000||fabs(e.z)>32000000||fabs(e.y)>32000000) { fail(c,"Invalid server player position"); break; }
        if(c->state==NETWORK_LOGIN) set_state(c,NETWORK_PLAY);
        off=beta14_movement(response,sizeof(response),e.x,e.y,e.z,e.yaw,e.pitch,p[41]);
        if(queue_bytes(c,response,off)) emit(c,&e);
        break;
    case 0x32: {
        int cx=beta14_i32(p+1),cz=beta14_i32(p+5); Chunk *chunk=world_peek_chunk(c->world,cx,cz);
        if(chunk && !p[9]) { memset(chunk->blocks,0,sizeof(chunk->blocks)); memset(chunk->metadata,0,sizeof(chunk->metadata)); memset(chunk->block_light,0,sizeof(chunk->block_light)); memset(chunk->sky_light,0,sizeof(chunk->sky_light)); ++chunk->revision; dirty_neighbors(c->world,chunk); }
        break; }
    case 0x33:
        if(receive_region(c,p)) { e.type=NETWORK_EVENT_CHUNK; e.block_x=beta14_i32(p+1); e.block_y=(int16_t)beta14_u16(p+5); e.block_z=beta14_i32(p+7); emit(c,&e); } break;
    case 0x34: {
        int32_t cx=beta14_i32(p+1),cz=beta14_i32(p+5); size_t count=beta14_u16(p+9); Chunk *chunk;
        if(cx<INT_MIN/16 || cx>INT_MAX/16-1 || cz<INT_MIN/16 || cz>INT_MAX/16-1) { fail(c,"Invalid multi-block chunk coordinates"); break; }
        for(i=0;i<count;++i) {
            if(p[11+count*2+i]>=BLOCK_COUNT || p[11+count*3+i]>15) {
                fail(c,"Server sent an unknown Beta block state"); break;
            }
        }
        if(c->state==NETWORK_ERROR) break;
        chunk=world_get_chunk(c->world,cx,cz);
        if(!chunk) { fail(c,"Out of memory applying multi-block update"); break; }
        for(i=0;i<count;++i) {
            unsigned coord=beta14_u16(p+11+i*2),x=coord>>12,z=(coord>>8)&15u,y=coord&255u;
            if(y>=WORLD_HEIGHT) continue;
            chunk_set_block(chunk,(int)x,(int)y,(int)z,p[11+count*2+i]);
            chunk_set_metadata(chunk,(int)x,(int)y,(int)z,p[11+count*3+i]);
        }
        if(count) world_relight_chunk(c->world,chunk);
        break; }
    case 0x35: set_remote_block(c,beta14_i32(p+1),p[5],beta14_i32(p+6),p[10],p[11],1); break;
    case 0x3c: {
        double x=beta14_f64(p+1),y=beta14_f64(p+9),z=beta14_f64(p+17); size_t count=beta14_u32(p+29);
        if(!isfinite(x)||!isfinite(y)||!isfinite(z)||fabs(x)>32000000||fabs(y)>32000000||fabs(z)>32000000) { fail(c,"Invalid explosion coordinates"); break; }
        for(i=0;i<count;++i) { set_remote_block(c,(int)floor(x)+(int8_t)p[33+i*3],(int)floor(y)+(int8_t)p[34+i*3],(int)floor(z)+(int8_t)p[35+i*3],0,0,0); if(c->state==NETWORK_ERROR) break; } break; }
    case 0x14: case 0x15: case 0x17: case 0x18: {
        RemoteEntity *r=entity_find(c,beta14_i32(p+1),1); if(!r) break; text[0]=0;
        if(p[0]==0x14) { off=beta14_read_string(packet,5,text,sizeof(text)); r->type=0; }
        else { off=p[0]==0x15?10:6; r->type=p[0]==0x15?255:p[5]; }
        r->x=beta14_i32(p+off)/32.0; r->y=beta14_i32(p+off+4)/32.0; r->z=beta14_i32(p+off+8)/32.0;
        if(p[0]==0x14 || p[0]==0x18) { r->yaw=p[off+12]*(360.0f/256.0f); r->pitch=p[off+13]*(360.0f/256.0f); }
        entity_event(c,r,NETWORK_EVENT_ENTITY_SPAWN,text); break; }
    case 0x1f: case 0x20: case 0x21: case 0x22: {
        RemoteEntity *r=entity_find(c,beta14_i32(p+1),0); if(!r) break;
        if(p[0]==0x22) { r->x=beta14_i32(p+5)/32.0; r->y=beta14_i32(p+9)/32.0; r->z=beta14_i32(p+13)/32.0; off=17; }
        else if(p[0]!=0x20) { r->x+=(int8_t)p[5]/32.0; r->y+=(int8_t)p[6]/32.0; r->z+=(int8_t)p[7]/32.0; off=8; }
        else off=5;
        if(p[0]!=0x1f) { r->yaw=p[off]*(360.0f/256.0f); r->pitch=p[off+1]*(360.0f/256.0f); }
        entity_event(c,r,NETWORK_EVENT_ENTITY_MOVE,NULL); break; }
    case 0x1d: { RemoteEntity *r=entity_find(c,beta14_i32(p+1),0); if(r) { entity_event(c,r,NETWORK_EVENT_ENTITY_DESPAWN,NULL); r->used=0; } break; }
    case 0x67: inventory_slot(c,p+4,p[1],(int16_t)beta14_u16(p+2)); break;
    case 0x68: off=4; for(i=0;i<beta14_u16(p+2);++i) { inventory_slot(c,p+off,p[1],(int)i); off+=beta14_u16(p+off)==65535u?2:5; } break;
    default: break; /* Framed standard packets with no implemented visual effect. */
    }
}

void network_tick(NetworkClient *c)
{
    unsigned packets=0,reads=0,regions=0; double now;
    if(!c || c->state==NETWORK_DISCONNECTED || c->state==NETWORK_ERROR) return;
    now=net_time();
    if(c->state!=NETWORK_PLAY && now>c->deadline) { fail(c,"Connection/login timed out"); return; }
    if(c->state==NETWORK_CONNECTING) { poll_connect(c); if(c->state==NETWORK_CONNECTING || c->state==NETWORK_ERROR) return; }
    if(now-c->last_receive>60.0) { fail(c,"Server receive timeout"); return; }
    flush_send(c); if(c->state==NETWORK_ERROR) return;
    while(packets<128) {
        Beta14Packet packet; int result=beta14_next_packet(c->rx,c->rx_count,&packet);
        if(result<0) { char message[96]; snprintf(message,sizeof(message),result==-2?"Unsupported packet 0x%02X (requires Beta 1.7.3 protocol 14)":"Malformed packet 0x%02X",c->rx_count?c->rx[0]:0); fail(c,message); return; }
        if(result==1) {
            if(packet.id==0x33 && regions++>=2) break;
            handle_packet(c,&packet);
            if(c->state==NETWORK_ERROR || c->state==NETWORK_DISCONNECTED) return;
            c->rx_count-=packet.size; memmove(c->rx,c->rx+packet.size,c->rx_count); ++packets; continue;
        }
        if(reads++>=4) break;
        if(c->rx_count==NET_RX_CAP) { fail(c,"Network receive buffer exhausted"); return; }
        result=(int)recv(c->socket,(char *)c->rx+c->rx_count,(int)(NET_RX_CAP-c->rx_count),0);
        if(result<0) { if(would_block()) break; fail(c,"TCP receive failed"); return; }
        if(!result) { fail(c,"Server closed the connection"); return; }
        c->rx_count+=(size_t)result; c->last_receive=now;
    }
    flush_send(c);
}
int network_send_position(NetworkClient *c,double x,double y,double z,float yaw,float pitch,int ground)
{ uint8_t p[42]; size_t n; if(!c || c->state!=NETWORK_PLAY) return 0; n=beta14_movement(p,sizeof(p),x,y,z,yaw,pitch,ground); return queue_bytes(c,p,n); }
int network_send_chat(NetworkClient *c,const char *message)
{ uint8_t p[256]; size_t n; if(!c || c->state!=NETWORK_PLAY) return 0; n=beta14_chat(p,sizeof(p),message); return queue_bytes(c,p,n); }
int network_send_held_item(NetworkClient *c,int slot)
{ uint8_t p[3]; size_t n; if(!c || c->state!=NETWORK_PLAY) return 0; n=beta14_held_item(p,sizeof(p),slot); return queue_bytes(c,p,n); }
int network_mine_block(NetworkClient *c,int status,int x,int y,int z,int face)
{ uint8_t p[12]; size_t n; if(!c || c->state!=NETWORK_PLAY) return 0; n=beta14_mine(p,sizeof(p),status,x,y,z,face); return queue_bytes(c,p,n); }
int network_place_block(NetworkClient *c,int x,int y,int z,int face,int item,int count,int damage)
{ uint8_t p[16]; size_t n; if(!c || c->state!=NETWORK_PLAY) return 0; n=beta14_place(p,sizeof(p),x,y,z,face,item,count,damage); return queue_bytes(c,p,n); }
