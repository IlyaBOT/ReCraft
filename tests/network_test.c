#ifndef _WIN32
#define _POSIX_C_SOURCE 200112L
#endif
#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#include <process.h>
typedef SOCKET TestSocket;
#define TEST_INVALID INVALID_SOCKET
#define test_close closesocket
#else
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <pthread.h>
typedef int TestSocket;
#define TEST_INVALID (-1)
#define test_close close
#endif

#include "../src/network/network.h"
#include "../src/network/protocols/protocol_beta_14.h"
#include <assert.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <zlib.h>

typedef struct MockServer {
    TestSocket listener;
    int passed;
} MockServer;
typedef struct Observed {
    int state_play,position,chunk,block,inventory;
    double feet_y;
    int16_t item_id,item_slot;
    uint8_t item_count;
} Observed;

static void pause_ms(unsigned ms)
{
#ifdef _WIN32
    Sleep(ms);
#else
    struct timeval tv; tv.tv_sec=(long)(ms/1000); tv.tv_usec=(long)(ms%1000)*1000;
    (void)select(0,NULL,NULL,NULL,&tv);
#endif
}
static int send_all(TestSocket s,const uint8_t *p,size_t n)
{
    while(n) { int sent=(int)send(s,(const char *)p,(int)n,0); if(sent<=0) return 0; p+=sent; n-=(size_t)sent; }
    return 1;
}
static int recv_all(TestSocket s,uint8_t *p,size_t n)
{
    while(n) { int got=(int)recv(s,(char *)p,(int)n,0); if(got<=0) return 0; p+=got; n-=(size_t)got; }
    return 1;
}
static void put32(uint8_t *p,uint32_t n)
{ p[0]=(uint8_t)(n>>24); p[1]=(uint8_t)(n>>16); p[2]=(uint8_t)(n>>8); p[3]=(uint8_t)n; }
static void put64(uint8_t *p,double d)
{ uint64_t n; memcpy(&n,&d,8); put32(p,(uint32_t)(n>>32)); put32(p+4,(uint32_t)n); }
static void put_float(uint8_t *p,float f)
{ uint32_t n; memcpy(&n,&f,4); put32(p,n); }
static int mock_session(TestSocket s)
{
    uint8_t p[64],out[64],*blocks,*chunk_packet; size_t i,index,wool_index,packet_size;
    uLongf compressed_size=131072;
    /* The first two packets must be the offline Beta 14 handshake and login. */
    if(!recv_all(s,p,15) || p[0]!=2 || p[1]!=0 || p[2]!=6 ||
       memcmp(p+3,"\0P\0l\0a\0y\0e\0r",12)!=0) return 0;
    { const uint8_t reply[]={2,0,1,0,'-'}; if(!send_all(s,reply,sizeof(reply))) return 0; }
    if(!recv_all(s,p,28) || p[0]!=1 || beta14_u32(p+1)!=14 ||
       p[5]!=0 || p[6]!=6 || memcmp(p+7,"\0P\0l\0a\0y\0e\0r",12)!=0) return 0;
    memset(out,0,16); out[0]=1; put32(out+1,123); out[14]=42; /* Seed=42, overworld. */
    if(!send_all(s,out,16)) return 0;
    memset(out,0,42); out[0]=0x0d;
    put64(out+1,8.5); put64(out+9,65.0); put64(out+17,66.62); put64(out+25,8.5);
    put_float(out+33,180.0f); put_float(out+37,0.0f); out[41]=1;
    if(!send_all(s,out,42)) return 0;

    blocks=(uint8_t *)calloc(81920,1);
    chunk_packet=(uint8_t *)malloc(18+131072);
    if(!blocks || !chunk_packet) { free(blocks); free(chunk_packet); return 0; }
    index=63u+8u*128u+8u*2048u;
    blocks[index]=1;
    wool_index=63u+8u*128u+9u*2048u;
    blocks[wool_index]=35; /* Keep an unsupported visual block's real Beta ID. */
    blocks[32768u+index/2u]=3u<<(4u*(index&1u));
    blocks[32768u+wool_index/2u]=14u<<(4u*(wool_index&1u));
    blocks[49152u+index/2u]=10u<<(4u*(index&1u));
    blocks[65536u+index/2u]=15u<<(4u*(index&1u));
    if(compress2(chunk_packet+18,&compressed_size,blocks,81920,Z_BEST_SPEED)!=Z_OK) {
        free(blocks); free(chunk_packet); return 0;
    }
    memset(chunk_packet,0,18); chunk_packet[0]=0x33;
    chunk_packet[11]=15; chunk_packet[12]=127; chunk_packet[13]=15;
    put32(chunk_packet+14,(uint32_t)compressed_size);
    packet_size=18u+(size_t)compressed_size;
    i=send_all(s,chunk_packet,packet_size);
    free(blocks); free(chunk_packet);
    if(!i) return 0;

    memset(out,0,12); out[0]=0x35; put32(out+1,8); out[5]=64;
    put32(out+6,8); out[10]=50; out[11]=3; /* Torch triggers local relighting. */
    if(!send_all(s,out,12)) return 0;
    memset(out,0,19); out[0]=0x34; out[9]=0; out[10]=2;
    out[11]=0x98; out[12]=0x40; /* X=9, Z=8, Y=64. */
    out[13]=0x88; out[14]=0x40; /* X=8, Z=8, Y=64. */
    out[15]=50; out[16]=0; /* Move the torch in one batch. */
    if(!send_all(s,out,19)) return 0;
    /* Window zero has 45 slots; Beta hotbar begins at slot 36. */
    { uint8_t slots[4+45*5]; size_t at=0;
      slots[at++]=0x68; slots[at++]=0; slots[at++]=0; slots[at++]=45;
      for(i=0;i<45;++i) {
          if(i==36) { slots[at++]=0; slots[at++]=1; slots[at++]=32; slots[at++]=0; slots[at++]=0; }
          else { slots[at++]=0xff; slots[at++]=0xff; }
      }
      if(!send_all(s,slots,at)) return 0;
    }
    out[0]=0; if(!send_all(s,out,1)) return 0;
    /* Position acknowledgement, keepalive and selected hotbar slot. */
    if(!recv_all(s,p,42) || p[0]!=0x0d || fabs(beta14_f64(p+9)-66.62)>0.0001 ||
       fabs(beta14_f64(p+17)-65.0)>0.0001) return 0;
    if(!recv_all(s,p,1) || p[0]!=0) return 0;
    if(!recv_all(s,p,3) || p[0]!=0x10 || beta14_u16(p+1)!=2) return 0;
    return 1;
}
#ifdef _WIN32
static unsigned __stdcall mock_worker(void *arg)
#else
static void *mock_worker(void *arg)
#endif
{
    MockServer *server=(MockServer *)arg; TestSocket s;
    s=accept(server->listener,NULL,NULL);
    if(s!=TEST_INVALID) {
#ifdef _WIN32
        { DWORD timeout=3000; setsockopt(s,SOL_SOCKET,SO_RCVTIMEO,(const char *)&timeout,sizeof(timeout)); }
#else
        { struct timeval timeout={3,0}; setsockopt(s,SOL_SOCKET,SO_RCVTIMEO,&timeout,sizeof(timeout)); }
#endif
        server->passed=mock_session(s); test_close(s);
    }
#ifdef _WIN32
    return 0;
#else
    return NULL;
#endif
}
static void observe(void *user,const NetworkEvent *e)
{
    Observed *o=(Observed *)user;
    if(e->type==NETWORK_EVENT_STATE && e->state==NETWORK_PLAY) o->state_play++;
    if(e->type==NETWORK_EVENT_POSITION) { o->position++; o->feet_y=e->y; }
    if(e->type==NETWORK_EVENT_CHUNK) o->chunk++;
    if(e->type==NETWORK_EVENT_BLOCK) o->block++;
    if(e->type==NETWORK_EVENT_INVENTORY && e->entity_type==0 && e->slot==36) {
        o->inventory++; o->item_id=e->item_id; o->item_count=e->item_count; o->item_slot=e->slot;
    }
}
static void test_packet_boundaries(void)
{
    uint8_t packet[128]; Beta14Packet parsed; size_t n,i;
    n=beta14_movement(packet,sizeof(packet),1.0,65.0,-2.0,180.0f,-15.0f,1);
    assert(n==42 && packet[0]==0x0d);
    assert(fabs(beta14_f64(packet+9)-66.62)<0.0001);
    assert(fabs(beta14_f64(packet+17)-65.0)<0.0001);
    for(i=0;i<n;++i) assert(beta14_next_packet(packet,i,&parsed)==0);
    assert(beta14_next_packet(packet,n,&parsed)==1 && parsed.size==n);
    assert(beta14_held_item(packet,sizeof(packet),8)==3);
    assert(beta14_held_item(packet,sizeof(packet),9)==0);
    { const uint8_t window[]={0x64,1,0,0,5,'C','h','e','s','t',27};
      for(i=0;i<sizeof(window);++i) assert(beta14_next_packet(window,i,&parsed)==0);
      assert(beta14_next_packet(window,sizeof(window),&parsed)==1 && parsed.size==sizeof(window));
    }
    { const uint8_t invalid[]={0x33,0,0,0,0,0,0,0,0,0,0,15,127,15,0xff,0xff,0xff,0xff};
      assert(beta14_next_packet(invalid,sizeof(invalid),&parsed)==-1);
    }
}
int main(void)
{
    TestSocket listener; struct sockaddr_in addr; World world; NetworkClient *client;
    MockServer server; Observed observed; int ready=0,i;
#ifdef _WIN32
    WSADATA data; uintptr_t worker;
    assert(WSAStartup(MAKEWORD(2,2),&data)==0);
#else
    pthread_t worker;
#endif
    test_packet_boundaries();
    listener=socket(AF_INET,SOCK_STREAM,IPPROTO_TCP); assert(listener!=TEST_INVALID);
    memset(&addr,0,sizeof(addr)); addr.sin_family=AF_INET;
    addr.sin_addr.s_addr=htonl(INADDR_LOOPBACK); addr.sin_port=0;
    assert(bind(listener,(struct sockaddr *)&addr,sizeof(addr))==0);
    assert(listen(listener,1)==0);
    {
#ifdef _WIN32
        int len=sizeof(addr);
#else
        socklen_t len=sizeof(addr);
#endif
        assert(getsockname(listener,(struct sockaddr *)&addr,&len)==0);
    }
    server.listener=listener; server.passed=0;
#ifdef _WIN32
    worker=_beginthreadex(NULL,0,mock_worker,&server,0,NULL); assert(worker!=0);
#else
    assert(pthread_create(&worker,NULL,mock_worker,&server)==0);
#endif
    assert(world_init(&world,0,0,64)==WORLD_OK); world.network_mode=1;
    memset(&observed,0,sizeof(observed));
    client=network_create(&world,observe,&observed); assert(client!=NULL);
    assert(network_connect(client,"127.0.0.1",ntohs(addr.sin_port),"Player"));
    for(i=0;i<5000;++i) {
        network_tick(client);
        if(network_state(client)==NETWORK_PLAY && !ready) {
            assert(network_send_held_item(client,2)); ready=1;
        }
        if(observed.chunk && observed.block && observed.inventory) break;
        if(network_state(client)==NETWORK_ERROR) break;
        pause_ms(1);
    }
    assert(observed.state_play==1 && observed.position==1 && fabs(observed.feet_y-65.0)<0.0001);
    assert(observed.chunk==1 && observed.block==1);
    assert(observed.inventory==1 && observed.item_id==1 && observed.item_count==32 && observed.item_slot==36);
    assert(world_peek_block(&world,8,63,8)==BLOCK_STONE);
    assert(world_peek_block(&world,9,63,8)==BETA_BLOCK_WOOL);
    assert(world_peek_block(&world,8,64,8)==BLOCK_AIR);
    assert(world_peek_block(&world,9,64,8)==BLOCK_TORCH);
    { Chunk *chunk=world_peek_chunk(&world,0,0); assert(chunk!=NULL);
      assert(chunk_get_metadata(chunk,8,63,8)==3);
      assert(chunk_get_metadata(chunk,9,63,8)==14);
      assert(chunk_get_block_light(chunk,9,64,8)==14);
      assert(chunk_get_block_light(chunk,8,64,8)==13);
      assert(chunk_get_sky_light(chunk,9,64,8)==15);
    }
    /* Let the nonblocking client flush the queued held-item packet. */
    for(i=0;i<50;++i) { network_tick(client); pause_ms(1); }
    network_destroy(client);
#ifdef _WIN32
    WaitForSingleObject((HANDLE)worker,5000); CloseHandle((HANDLE)worker);
#else
    pthread_join(worker,NULL);
#endif
    assert(server.passed);
    assert(world_close(&world)==WORLD_OK);
    test_close(listener);
#ifdef _WIN32
    WSACleanup();
#endif
    puts("network_test: protocol framing and loopback Beta 14 session passed");
    return 0;
}
