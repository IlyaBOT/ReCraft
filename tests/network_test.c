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
    int passed,online,joins;
} MockServer;
typedef struct Observed {
    World *world;
    int state_play,position,chunk,block,inventory;
    double feet_y;
    int16_t item_id,item_slot;
    uint8_t item_count;
    NetworkClient *client;
    int chest_open,chest_sync,chest_slots,cursor_updates;
    int furnace_open,furnace_sync,furnace_slots,properties[3];
    int rejected,accepted,closed,dead,alive,respawn;
    int player_spawn,player_despawn,chat,sign;
    int vehicle_spawn,fire_flags,notes,attach,item_spawn,item_collect,velocity,status,metadata;
    int16_t chest_id[63],chest_damage[63];
    uint8_t chest_count[63];
} Observed;
static const char unicode_chat[]="Hello \xc3\xa9 \xd0\x9c\xd0\xb8\xd1\x80";
/* Sent only to the loopback test server, never to a public game server. */
static const char *server_commands[]={"/tp 1 65 2","/gamemode survival","/timeset day","/plugin-command"};
static const char unicode_sign[4][61]={"\xd0\xa2\xd0\xb5\xd1\x81\xd1\x82","Beta 1.7.3","","123"};

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
static size_t put_slot(uint8_t *p,int id,int count,int damage)
{
    p[0]=(uint8_t)((uint16_t)id>>8); p[1]=(uint8_t)id;
    if(id<0) return 2;
    p[2]=(uint8_t)count; p[3]=(uint8_t)((unsigned)damage>>8); p[4]=(uint8_t)damage;
    return 5;
}
static int send_window(TestSocket s,int window,int type,const char *title,int slots)
{
    uint8_t p[70]; size_t length=strlen(title);
    assert(length<=64);
    p[0]=0x64; p[1]=(uint8_t)window; p[2]=(uint8_t)type;
    p[3]=0; p[4]=(uint8_t)length; memcpy(p+5,title,length);
    p[5+length]=(uint8_t)slots;
    /* Split inside the title to exercise partial String8 framing. */
    return send_all(s,p,6) && send_all(s,p+6,length);
}
static int send_chest_contents(TestSocket s,int count)
{
    uint8_t p[4+63*5]; size_t at=4; int i;
    p[0]=0x68; p[1]=1; p[2]=0; p[3]=63;
    for(i=0;i<63;++i) {
        if(i==0) at+=put_slot(p+at,17,count,2);
        else if(i==26) at+=put_slot(p+at,263,12,0);
        else if(i==27) at+=put_slot(p+at,1,64,0);
        else if(i==54) at+=put_slot(p+at,278,1,321);
        else at+=put_slot(p+at,-1,0,0);
    }
    return send_all(s,p,at);
}
static int mock_containers(TestSocket s)
{
    uint8_t p[64],out[4+39*5]; size_t at; int i;
    if(!send_window(s,1,0,"Chest",27) || !send_chest_contents(s,7)) return 0;
    if(!recv_all(s,p,13) || p[0]!=0x66 || p[1]!=1 || beta14_u16(p+2)!=0 ||
       p[4]!=0 || beta14_u16(p+5)!=65534 || p[7]!=0 ||
       beta14_u16(p+8)!=17 || p[10]!=7 || beta14_u16(p+11)!=2) return 0;
    { const uint8_t rejection[]={0x6a,1,0xff,0xfe,0};
      if(!send_all(s,rejection,sizeof(rejection))) return 0;
    }
    /* A rejected predicted click must be confirmed, then replaced by the
     * server's complete inventory and cursor snapshots. */
    if(!recv_all(s,p,5) || p[0]!=0x6a || p[1]!=1 ||
       beta14_u16(p+2)!=65534 || p[4]!=1) return 0;
    { const uint8_t cursor_empty[]={0x67,0xff,0xff,0xff,0xff,0xff};
      if(!send_all(s,cursor_empty,sizeof(cursor_empty)) || !send_chest_contents(s,8)) return 0;
    }
    if(!recv_all(s,p,13) || p[0]!=0x66 || p[1]!=1 || beta14_u16(p+2)!=0 ||
       p[4]!=1 || beta14_u16(p+5)!=65535 || p[7]!=0 ||
       beta14_u16(p+8)!=17 || p[10]!=8 || beta14_u16(p+11)!=2) return 0;
    { const uint8_t accepted[]={0x6a,1,0xff,0xff,1};
      if(!send_all(s,accepted,sizeof(accepted))) return 0;
    }
    if(!recv_all(s,p,2) || p[0]!=0x65 || p[1]!=1) return 0;

    if(!send_window(s,2,2,"Furnace",3)) return 0;
    at=4; out[0]=0x68; out[1]=2; out[2]=0; out[3]=39;
    for(i=0;i<39;++i) {
        if(i==0) at+=put_slot(out+at,15,2,0);
        else if(i==1) at+=put_slot(out+at,263,1,0);
        else if(i==2) at+=put_slot(out+at,265,3,0);
        else at+=put_slot(out+at,-1,0,0);
    }
    if(!send_all(s,out,at)) return 0;
    { const uint8_t status[]={0x69,2,0,0,0,100,0x69,2,0,1,3,32,
          0x69,2,0,2,6,64,0x08,0,0,0x65,2};
      if(!send_all(s,status,sizeof(status))) return 0;
    }
    if(!recv_all(s,p,2) || p[0]!=0x09 || p[1]!=0) return 0;
    { const uint8_t respawn[]={0x09,0,0x08,0,20};
      if(!send_all(s,respawn,sizeof(respawn))) return 0;
    }
    memset(out,0,42); out[0]=0x0d;
    put64(out+1,20.5); put64(out+9,66.62); put64(out+17,65.0); put64(out+25,20.5);
    put_float(out+33,90.0f); out[41]=1;
    if(!send_all(s,out,42)) return 0;
    if(!recv_all(s,p,42) || p[0]!=0x0d || fabs(beta14_f64(p+1)-20.5)>0.0001 ||
       fabs(beta14_f64(p+9)-65.0)>0.0001 || fabs(beta14_f64(p+17)-66.62)>0.0001) return 0;
    /* Plugin-style positional correction without a look field, then look-only. */
    out[0]=0x0b; put64(out+1,12.5); put64(out+9,66.62); put64(out+17,65);
    put64(out+25,-57.5); out[33]=0;
    if(!send_all(s,out,34) || !recv_all(s,p,34) || p[0]!=0x0b ||
       fabs(beta14_f64(p+1)-12.5)>.0001 || fabs(beta14_f64(p+9)-65)>.0001 ||
       fabs(beta14_f64(p+17)-66.62)>.0001 || fabs(beta14_f64(p+25)+57.5)>.0001) return 0;
    out[0]=0x0c; put_float(out+1,90); put_float(out+5,20); out[9]=0;
    if(!send_all(s,out,10) || !recv_all(s,p,10) || memcmp(out,p,10)) return 0;
    return 1;
}
static int mock_session(TestSocket s,int online)
{
    uint8_t p[64],out[64],*blocks,*chunk_packet; size_t i,index,wool_index,packet_size;
    uLongf compressed_size=131072;
    /* Online login must wait for the asynchronous session join callback. */
    if(!recv_all(s,p,15) || p[0]!=2 || p[1]!=0 || p[2]!=6 ||
       memcmp(p+3,"\0P\0l\0a\0y\0e\0r",12)!=0) return 0;
    { uint8_t reply[64];size_t n=beta14_handshake(reply,sizeof(reply),online ? "-abc123" : "-"); if(!send_all(s,reply,n)) return 0; }
    if(!recv_all(s,p,28) || p[0]!=1 || beta14_u32(p+1)!=14 ||
       p[5]!=0 || p[6]!=6 || memcmp(p+7,"\0P\0l\0a\0y\0e\0r",12)!=0) return 0;
    memset(out,0,16); out[0]=1; put32(out+1,123); out[14]=42; /* Seed=42, overworld. */
    if(!send_all(s,out,16)) return 0;
    memset(out,0,9); out[0]=0x04; put32(out+1,1); put32(out+5,18000);
    if(!send_all(s,out,9)) return 0;
    { const uint8_t weather[]={0x46,1}; if(!send_all(s,weather,sizeof(weather))) return 0; }
    memset(out,0,42); out[0]=0x0d;
    put64(out+1,8.5); put64(out+9,66.62); put64(out+17,65.0); put64(out+25,8.5);
    put_float(out+33,180.0f); put_float(out+37,0.0f); out[41]=1;
    if(!send_all(s,out,42)) return 0;

    /* Real Beta servers wait for feet-Y acknowledgement before streaming the
     * login room. Reversing feet and stance must fail before any map data. */
    if(!recv_all(s,p,42) || p[0]!=0x0d || fabs(beta14_f64(p+9)-65.0)>0.0001 ||
       fabs(beta14_f64(p+17)-66.62)>0.0001) return 0;

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

    { uint8_t partial[64]={0x33},data[4]={1,0,0,0}; uLongf size=46;
      put32(partial+1,48); partial[6]=64; put32(partial+7,48);
      if(compress2(partial+18,&size,data,4,Z_BEST_SPEED)!=Z_OK) return 0;
      put32(partial+14,(uint32_t)size);
      if(!send_all(s,partial,18+size)) return 0;
    }

    /* The chunk callback seeds another entry; server unload must remove it. */
    memset(out,0,10); out[0]=0x32; put32(out+1,2); put32(out+5,2);
    if(!send_all(s,out,10)) return 0;

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
    for(i=0;i<5;++i) {
        uint8_t note[13]={0x36};put32(note+1,(uint32_t)-3);note[5]=0;note[6]=65;
        put32(note+7,8);note[11]=(uint8_t)i;note[12]=(uint8_t)(i*6);
        if(!send_all(s,note,6) || !send_all(s,note+6,7))return 0;
    }
    out[0]=0; if(!send_all(s,out,1)) return 0;
    /* Keepalive and selected hotbar slot. Teleport ACK was checked above. */
    { int keepalive=0,held=0;
      for(i=0;i<2;++i) {
          if(!recv_all(s,p,1)) return 0;
          if(p[0]==0 && !keepalive) keepalive=1;
          else if(p[0]==0x10 && !held && recv_all(s,p+1,2) && beta14_u16(p+1)==2) held=1;
          else return 0;
      }
      if(!held || !keepalive) return 0;
    }
    /* Named entity visibility is the only remote roster data Beta sends. */
    { uint8_t spawn[22]={0x17}; int object; static const unsigned char types[]={1,10,11,12,60};
      for(object=0;object<5;++object) {
          put32(spawn+1,1001+object); spawn[5]=types[object]; put32(spawn+6,8*32); put32(spawn+10,65*32); put32(spawn+14,8*32);
          if(!send_all(s,spawn,sizeof(spawn))) return 0;
      }
      if(!recv_all(s,p,10)||p[0]!=0x07||beta14_i32(p+5)!=1001||p[9]!=0)return 0;
      { uint8_t attach[9]={0x27},item[25]={0x15},v[11]={0x1c},collect[9]={0x16},status[6]={0x26};
        put32(attach+1,123);put32(attach+5,1001);
        if(!send_all(s,attach,4)||!send_all(s,attach+4,5))return 0;
        if(!recv_all(s,p,42)||p[0]!=0x0d||beta14_f64(p+9)!=-999||beta14_f64(p+17)!=-999||fabs(beta14_f64(p+1)-.1)>.000001||fabs(beta14_f64(p+25)+.2)>.000001)return 0;
        put32(attach+5,0xffffffff);if(!send_all(s,attach,9))return 0;
        put32(item+1,1100);item[5]=1;item[6]=22;item[7]=3;item[9]=9;
        put32(item+10,8*32);put32(item+14,65*32);put32(item+18,8*32);item[22]=64;item[23]=128;item[24]=32;
        put32(v+1,1100);v[5]=0x1f;v[6]=0x40;v[7]=0xe0;v[8]=0xc0;
        put32(status+1,1001);status[5]=2;
        put32(collect+1,1100);put32(collect+5,123);
        if(!send_all(s,item,25)||!send_all(s,v,11)||!send_all(s,status,6)||!send_all(s,collect,9))return 0;
      }
    }
    { uint8_t spawn[64],string[32],chat[256],sign[139];size_t n,at;Beta14Packet parsed;char decoded[64];
      n=beta14_handshake(string,sizeof(string),"Bob");spawn[0]=0x14;put32(spawn+1,456);
      memcpy(spawn+5,string+1,n-1);at=5+n-1;memset(spawn+at,0,16);put32(spawn+at,9*32);put32(spawn+at+4,65*32);put32(spawn+at+8,8*32);
      if(!send_all(s,spawn,at+16))return 0;
      { const uint8_t flags[]={0x28,0,0,0,123,0,1,127,0x28,0,0,1,200,0,1,127};
        if(!send_all(s,flags,sizeof(flags)))return 0; }
      for(i=0;i<sizeof(server_commands)/sizeof(server_commands[0]);++i) {
          n=beta14_chat(chat,sizeof(chat),server_commands[i]);
          if(!recv_all(s,p,n)||memcmp(p,chat,n))return 0;
      }
      n=beta14_chat(chat,sizeof(chat),unicode_chat);if(!recv_all(s,p,n)||memcmp(p,chat,n))return 0;
      if(!recv_all(s,p,10)||p[0]!=0x07||beta14_i32(p+1)!=123||beta14_i32(p+5)!=456||p[9]!=1)return 0;
      if(!recv_all(s,p,6)||p[0]!=0x12||beta14_i32(p+1)!=123||p[5]!=1)return 0;
      if(!recv_all(s,p,6)||p[0]!=0x13||beta14_i32(p+1)!=123||p[5]!=1)return 0;
      if(!recv_all(s,p,6)||p[0]!=0x13||beta14_i32(p+1)!=123||p[5]!=2)return 0;
      if(!send_all(s,chat,n))return 0;
      n=beta14_sign_update(sign,sizeof(sign),8,64,8,unicode_sign);if(!send_all(s,sign,n)||!recv_all(s,p,n)||memcmp(p,sign,n))return 0;
      if(beta14_next_packet(p,n,&parsed)!=1||!beta14_read_string(&parsed,11,decoded,sizeof(decoded))||strcmp(decoded,unicode_sign[0]))return 0;
      spawn[0]=0x1d;put32(spawn+1,456);if(!send_all(s,spawn,5))return 0;
    }
    return mock_containers(s);
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
        server->passed=mock_session(s,server->online); test_close(s);
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
    if(e->type==NETWORK_EVENT_ATTACH) {
        assert(e->entity_id==123 && e->entity_type==-1);++o->attach;
        assert(network_vehicle_id(o->client)==e->vehicle_id);
        if(e->vehicle_id>=0)assert(network_send_riding(o->client,2,-4,90,0,1));
    }
    if(e->type==NETWORK_EVENT_ENTITY_SPAWN && e->entity_type==1008) {
        assert(e->entity_id==1100 && e->item_id==278 && e->item_count==3 && e->item_damage==9);
        assert(e->vx==10 && e->vy==-20 && e->vz==5);++o->item_spawn;
    }
    if(e->type==NETWORK_EVENT_ENTITY_VELOCITY) {assert(e->entity_id==1100 && e->vx==20 && e->vy==-20 && e->vz==0);++o->velocity;}
    if(e->type==NETWORK_EVENT_ENTITY_STATUS) {assert(e->entity_id==1001 && e->value==2);++o->status;}
    if(e->type==NETWORK_EVENT_ENTITY_COLLECT) {assert(e->entity_id==1100 && e->vehicle_id==123 && e->entity_type==-1);++o->item_collect;}
    if(e->type==NETWORK_EVENT_NOTE) {
        assert(e->block_x==-3 && e->block_y==65 && e->block_z==8);
        assert(e->property==o->notes && e->value==o->notes*6);++o->notes;
    }
    if(e->type==NETWORK_EVENT_ENTITY_FLAGS) {assert(e->value==1);assert(e->entity_id==123 ? e->entity_type==-1 : e->entity_id==456 && e->entity_type==0);++o->fire_flags;}
    if(e->type==NETWORK_EVENT_STATE && e->state==NETWORK_PLAY) o->state_play++;
    if(e->type==NETWORK_EVENT_POSITION) {
        o->position++;
        if(e->value&1) {
            o->feet_y=e->y; assert(fabs(e->y-65.0)<0.0001);
            assert(!network_terrain_ready(o->client,e->x,e->z));
            if(e->value==1) assert(e->x==12.5 && e->z==-57.5);
        } else { assert(e->value==2 && e->yaw==90 && e->pitch==20); }
    }
    if(e->type==NETWORK_EVENT_CHUNK) {
        if(!o->chunk) {
            Chunk *chunk=world_get_chunk(o->world,2,2); assert(chunk); chunk->network_received=1;
            assert(network_terrain_ready(o->client,8.5,8.5));
        } else assert(!network_terrain_ready(o->client,48.5,48.5));
        o->chunk++;
    }
    if(e->type==NETWORK_EVENT_BLOCK) { assert(!world_peek_chunk(o->world,2,2)); o->block++; }
    if(e->type==NETWORK_EVENT_INVENTORY && e->entity_type==0 && e->slot==36) {
        o->inventory++; o->item_id=e->item_id; o->item_count=e->item_count; o->item_slot=e->slot;
    }
    if(e->type==NETWORK_EVENT_WINDOW_OPEN) {
        if(e->window_id==1) { assert(e->window_type==0 && e->window_slots==27); ++o->chest_open; }
        if(e->window_id==2) { assert(e->window_type==2 && e->window_slots==3); ++o->furnace_open; }
    }
    if(e->type==NETWORK_EVENT_INVENTORY && e->entity_type==1) {
        assert(e->slot>=0 && e->slot<63); ++o->chest_slots;
        o->chest_id[e->slot]=e->item_id; o->chest_count[e->slot]=e->item_count; o->chest_damage[e->slot]=e->item_damage;
    }
    if(e->type==NETWORK_EVENT_INVENTORY && e->entity_type==255) { assert(e->slot==-1 && e->item_id==-1); ++o->cursor_updates; }
    if(e->type==NETWORK_EVENT_INVENTORY && e->entity_type==2) {
        ++o->furnace_slots;
        if(e->slot==0) assert(e->item_id==15 && e->item_count==2);
        if(e->slot==1) assert(e->item_id==263);
        if(e->slot==2) assert(e->item_id==265 && e->item_count==3);
    }
    if(e->type==NETWORK_EVENT_WINDOW_SYNC) {
        if(e->window_id==1) {
            int action=++o->chest_sync==1 ? 65534 : 65535;
            assert(o->chest_id[0]==17 && o->chest_damage[0]==2);
            assert(o->chest_count[0]==(action==65534 ? 7 : 8));
            assert(o->chest_id[26]==263 && o->chest_id[27]==1 && o->chest_id[54]==278 && o->chest_damage[54]==321);
            assert(network_click_window(o->client,1,0,action==65535,action,0,(InventorySlot){17,o->chest_count[0],2}));
        }
        if(e->window_id==2) ++o->furnace_sync;
    }
    if(e->type==NETWORK_EVENT_WINDOW_TRANSACTION) {
        assert(e->window_id==1);
        if(!e->accepted) { assert(e->action==65534); ++o->rejected; assert(network_confirm_window(o->client,1,e->action)); }
        else { assert(e->action==65535); ++o->accepted; assert(network_close_window(o->client,1)); }
    }
    if(e->type==NETWORK_EVENT_WINDOW_PROPERTY) { assert(e->window_id==2 && e->property>=0 && e->property<3); o->properties[e->property]=e->value; }
    if(e->type==NETWORK_EVENT_WINDOW_CLOSE) { assert(e->window_id==2); ++o->closed; }
    if(e->type==NETWORK_EVENT_HEALTH) {
        if(e->health==0) { ++o->dead; assert(network_respawn(o->client)); }
        if(e->health==20) ++o->alive;
    }
    if(e->type==NETWORK_EVENT_RESPAWN) { assert(e->dimension==0); ++o->respawn; }
    if(e->type==NETWORK_EVENT_ENTITY_SPAWN&&e->entity_type==0) {
        NetworkPlayerInfo players[4];assert(network_player_list(o->client,players,4)==2);
        assert(players[0].self&&!strcmp(players[0].name,"Player")&&players[0].entity_id==123&&players[0].ping_ms==-1);
        assert(!strcmp(players[1].name,"Bob")&&players[1].entity_id==456&&players[1].ping_ms==-1);
        {size_t i;for(i=0;i<sizeof(server_commands)/sizeof(server_commands[0]);++i)
            assert(network_send_chat(o->client,server_commands[i]));}
        ++o->player_spawn;assert(network_send_chat(o->client,unicode_chat));
        assert(network_use_entity(o->client,456,1)&&network_send_animation(o->client,1));
        assert(network_send_player_action(o->client,1)&&network_send_player_action(o->client,2));
        assert(!network_use_entity(o->client,123,1)&&!network_use_entity(o->client,456,2));
    }
    if(e->type==NETWORK_EVENT_ENTITY_SPAWN && e->entity_type>=1000 && e->entity_type!=1008) {
        int object=e->entity_id-1001; assert(object>=0 && object<5);
        assert(e->entity_type==(object==0 ? 1002 : object==4 ? 1000 : 1001));
        if(object>=1 && object<=3) assert(e->entity_variant==object-1);
        if(!object) assert(network_use_entity(o->client,e->entity_id,0));
        ++o->vehicle_spawn;
    }
    if(e->type==NETWORK_EVENT_ENTITY_DESPAWN) {
        /* Despawn callback is emitted before the entry is released. */
        if(e->entity_id==456)++o->player_despawn;else assert(e->entity_id==1100);
    }
    if(e->type==NETWORK_EVENT_CHAT) {assert(!strcmp(e->text,unicode_chat));++o->chat;}
    if(e->type==NETWORK_EVENT_SIGN) {
        assert(e->block_x==8&&e->block_y==64&&e->block_z==8&&!memcmp(e->sign_lines,unicode_sign,sizeof(unicode_sign)));
        ++o->sign;assert(network_send_sign_update(o->client,e->block_x,e->block_y,e->block_z,e->sign_lines));
    }

}
static void test_packet_boundaries(void)
{
    uint8_t packet[128]; Beta14Packet parsed; size_t n,i;
    n=beta14_movement(packet,sizeof(packet),1.0,65.0,-2.0,180.0f,-15.0f,1);
    assert(n==42 && packet[0]==0x0d);
    assert(fabs(beta14_f64(packet+9)-65.0)<0.0001);
    assert(fabs(beta14_f64(packet+17)-66.62)<0.0001);
    for(i=0;i<n;++i) assert(beta14_next_packet(packet,i,&parsed)==0);
    assert(beta14_next_packet(packet,n,&parsed)==1 && parsed.size==n);
    assert(beta14_held_item(packet,sizeof(packet),8)==3);
    assert(beta14_held_item(packet,sizeof(packet),9)==0);
    n=beta14_window_click(packet,sizeof(packet),2,54,1,32768,0,278,1,1234);
    assert(n==13 && packet[0]==0x66 && packet[1]==2 && beta14_u16(packet+2)==54);
    assert(packet[4]==1 && beta14_u16(packet+5)==32768 && !packet[7] && beta14_u16(packet+8)==278 && beta14_u16(packet+11)==1234);
    for(i=0;i<n;++i) assert(beta14_next_packet(packet,i,&parsed)==0);
    assert(beta14_next_packet(packet,n,&parsed)==1 && parsed.size==n);
    assert(beta14_window_click(packet,sizeof(packet),0,1,0,1,0,-1,0,0)==10);
    { const uint8_t window[]={0x64,1,0,0,5,'C','h','e','s','t',27};
      for(i=0;i<sizeof(window);++i) assert(beta14_next_packet(window,i,&parsed)==0);
      assert(beta14_next_packet(window,sizeof(window),&parsed)==1 && parsed.size==sizeof(window));
    }
    { const uint8_t invalid[]={0x33,0,0,0,0,0,0,0,0,0,0,15,127,15,0xff,0xff,0xff,0xff};
      assert(beta14_next_packet(invalid,sizeof(invalid),&parsed)==-1);
    }
    n=beta14_chat(packet,sizeof(packet),unicode_chat);assert(n&&beta14_next_packet(packet,n,&parsed)==1);
    {char text[64];assert(beta14_read_string(&parsed,1,text,sizeof(text))==n&&!strcmp(text,unicode_chat));}
    assert(!beta14_chat(packet,sizeof(packet),"\xc0\x80"));assert(!beta14_chat(packet,sizeof(packet),"\xed\xa0\x80"));
    n=beta14_sign_update(packet,sizeof(packet),-1,64,0,unicode_sign);assert(n&&packet[0]==0x82);
    for(i=0;i<n;++i)assert(beta14_next_packet(packet,i,&parsed)==0);
    assert(beta14_next_packet(packet,n,&parsed)==1);
    {char bad[4][61]={{0}};memset(bad[0],'x',16);assert(!beta14_sign_update(packet,sizeof(packet),0,64,0,bad));}
}
static int mock_join(void *context,const char *id,char *error,size_t capacity)
{ MockServer *s=(MockServer *)context;(void)error;(void)capacity;assert(!strcmp(id,"-abc123"));return ++s->joins>=3; }
int main(void)
{
    TestSocket listener; struct sockaddr_in addr; World world; NetworkClient *client;
    MockServer server; Observed observed; int ready=0,i,online;
#ifdef _WIN32
    WSADATA data; uintptr_t worker;
    assert(WSAStartup(MAKEWORD(2,2),&data)==0);
#else
    pthread_t worker;
#endif
    test_packet_boundaries();
    for(online=0;online<=1;++online) {
    ready=0;
    listener=socket(AF_INET,SOCK_STREAM,IPPROTO_TCP); assert(listener!=TEST_INVALID);
    memset(&addr,0,sizeof(addr)); addr.sin_family=AF_INET;
    /* Darwin hides the BSD INADDR_LOOPBACK constant under _POSIX_C_SOURCE.
     * inet_pton is available in the POSIX and Winsock APIs and writes network order. */
    assert(inet_pton(AF_INET,"127.0.0.1",&addr.sin_addr)==1);
    addr.sin_port=0;
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
    server.listener=listener; server.passed=0;server.online=online;server.joins=0;
#ifdef _WIN32
    worker=_beginthreadex(NULL,0,mock_worker,&server,0,NULL); assert(worker!=0);
#else
    assert(pthread_create(&worker,NULL,mock_worker,&server)==0);
#endif
    assert(world_init(&world,0,0,64)==WORLD_OK); world.network_mode=1;
    memset(&observed,0,sizeof(observed));
    observed.world=&world;
    client=network_create(&world,observe,&observed); assert(client!=NULL); observed.client=client;
    if(online)network_set_auth(client,mock_join,&server);
    assert(network_connect(client,"127.0.0.1",ntohs(addr.sin_port),"Player"));
    for(i=0;i<5000;++i) {
        network_tick(client);
        if(network_state(client)==NETWORK_PLAY && !ready) {
            assert(network_send_held_item(client,2)); ready=1;
        }
        if(observed.respawn && observed.position==4 && observed.alive) break;
        if(network_state(client)==NETWORK_ERROR) break;
        pause_ms(1);
    }
    assert(observed.state_play==1 && observed.position==4 && fabs(observed.feet_y-65.0)<0.0001);
    assert(observed.chunk==2 && observed.block==1);
    assert(world_peek_block(&world,48,64,48)==BLOCK_STONE);
    assert(!network_terrain_ready(client,48.5,48.5));
    assert(network_terrain_ready(client,8.5,8.5));
    assert(!network_terrain_ready(client,20.5,20.5));
    assert(!network_terrain_ready(client,15.9,8.5)); /* Footprint reaches unloaded chunk. */
    assert(world_get_chunk(&world,2,2)); /* Sparse/collision cache entry is not map data. */
    assert(!network_terrain_ready(client,32.5,32.5));
    assert(observed.inventory==1 && observed.item_id==1 && observed.item_count==32 && observed.item_slot==36);
    assert(world_peek_block(&world,8,63,8)==BLOCK_STONE);
    assert(world_peek_block(&world,9,63,8)==BETA_BLOCK_WOOL);
    assert(world_peek_block(&world,8,64,8)==BLOCK_AIR);
    assert(world_peek_block(&world,9,64,8)==BLOCK_TORCH);
    assert(world.beta_world_time==INT64_C(4294985296) && world.raining);
    { Chunk *chunk=world_peek_chunk(&world,0,0); assert(chunk!=NULL);
      assert(chunk_get_metadata(chunk,8,63,8)==3);
      assert(chunk_get_metadata(chunk,9,63,8)==14);
      assert(chunk_get_block_light(chunk,9,64,8)==14);
      assert(chunk_get_block_light(chunk,8,64,8)==13);
      assert(chunk_get_sky_light(chunk,9,64,8)==15);
    }
    assert(observed.chest_open==1 && observed.chest_sync==2 && observed.chest_slots==126 && observed.cursor_updates==1);
    assert(observed.rejected==1 && observed.accepted==1 && observed.furnace_open==1 && observed.furnace_sync==1);
    assert(observed.furnace_slots==39 && observed.properties[0]==100 && observed.properties[1]==800 && observed.properties[2]==1600);
    assert(observed.closed==1 && observed.dead==1 && observed.respawn==1 && observed.alive==1);
    assert(observed.player_spawn==1&&observed.player_despawn==1&&observed.chat==1&&observed.sign==1);
    assert(observed.vehicle_spawn==5);
    assert(observed.attach==2 && observed.item_spawn==1 && observed.item_collect==1 && observed.velocity==1 && observed.status==1);
    assert(observed.fire_flags==2 && server.joins==(online ? 3 : 0));
    assert(observed.notes==5);
    {NetworkPlayerInfo players[2];assert(network_player_list(client,players,2)==1&&players[0].self);}
    /* Let the nonblocking client flush the final respawn acknowledgement. */
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
    }
#ifdef _WIN32
    WSACleanup();
#endif
    puts("network_test: offline/online Beta 14 sessions, asynchronous join and burning metadata passed");
    return 0;
}
