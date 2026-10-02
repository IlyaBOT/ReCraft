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
#include "../src/network/server_status.h"
#include "../src/network/protocols/protocol_1_8_47.h"
#include "../src/util/server_list.h"
#include "../src/assets/server_icon_png.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <zlib.h>
#include "png_fixture.h"

static void pause_ms(unsigned ms)
{
#ifdef _WIN32
    Sleep(ms);
#else
    struct timeval tv;tv.tv_sec=ms/1000;tv.tv_usec=(ms%1000)*1000;(void)select(0,NULL,NULL,NULL,&tv);
#endif
}
static int transfer(TestSocket s,unsigned char *p,size_t n,int sending)
{while(n){int got=sending?(int)send(s,(const char *)p,(int)n,0):(int)recv(s,(char *)p,(int)n,0);if(got<=0)return 0;p+=got;n-=(size_t)got;}return 1;}
static void base64(const unsigned char *p,size_t n,char *out)
{
    const char table[]="ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";size_t i,w=0;
    for(i=0;i<n;i+=3){unsigned bits=(unsigned)p[i]<<16;if(i+1<n)bits|=(unsigned)p[i+1]<<8;if(i+2<n)bits|=p[i+2];
        out[w++]=table[bits>>18];out[w++]=table[(bits>>12)&63];out[w++]=i+1<n?table[(bits>>6)&63]:'=';out[w++]=i+2<n?table[bits&63]:'=';}out[w]=0;
}
static char status_json[4096];
static void parser_tests(void)
{
    ServerStatusResult r;unsigned char png[300];char encoded[500];size_t n=test_png_fixture(png);const char *bad[]=
    {"{}","[]","{\"version\":{\"protocol\":47},\"players\":{\"online\":-1,\"max\":20}}",
     "{\"version\":{\"protocol\":2147483648},\"players\":{\"online\":1,\"max\":20}}",
     "{\"version\":{\"protocol\":47},\"players\":{\"online\":1,\"max\":20},\"description\":\"\\ud800\"}",
     "{\"version\":{\"protocol\":47},\"players\":{\"online\":1,\"max\":20},\"description\":\"\xc0\x80\"}"};size_t i;
    base64(png,n,encoded);snprintf(status_json,sizeof(status_json),
      "{\"version\":{\"name\":\"1.8.9\",\"protocol\":47},\"players\":{\"online\":2,\"max\":20,\"sample\":[{\"name\":\"Guest\"}]},"
      "\"description\":{\"text\":\"\\u00a7aHello \",\"extra\":[{\"text\":\"\\u041c\\u0438\\u0440\\n\"},\"Beta\"]},\"favicon\":\"data:image/png;base64,%s\"}",encoded);
    memset(&r,0,sizeof(r));assert(server_status_parse_json(status_json,strlen(status_json),&r));
    assert(r.protocol==47&&r.online==2&&r.maximum==20&&!strcmp(r.version,"1.8.9"));
    assert(!strcmp(r.motd,"\xc2\xa7" "aHello \xd0\x9c\xd0\xb8\xd1\x80\nBeta"));assert(r.icon_size==n&&!memcmp(r.icon,png,n));
    assert(server_icon_png_valid(png,n));png[29]^=1;assert(!server_icon_png_valid(png,n));png[29]^=1;
    assert(!server_icon_png_valid(png,32)&&!server_icon_png_valid(png,16385));
    {unsigned char oversized[500],header[13]={0},compressed[300],*raw=(unsigned char *)calloc(70000,1);uLongf count=sizeof(compressed);size_t at=8;assert(raw);
        test_png_put32(header,64);test_png_put32(header+4,64);header[8]=8;header[9]=6;memcpy(oversized,png,8);
        assert(compress2(compressed,&count,raw,70000,Z_BEST_COMPRESSION)==Z_OK);free(raw);
        at+=test_png_chunk(oversized+at,"IHDR",header,13);at+=test_png_chunk(oversized+at,"IDAT",compressed,count);at+=test_png_chunk(oversized+at,"IEND",NULL,0);
        assert(!server_icon_png_valid(oversized,at));}
    for(i=0;i<sizeof(bad)/sizeof(bad[0]);++i)assert(!server_status_parse_json(bad[i],strlen(bad[i]),&r));
    {const char *colored="{\"version\":{\"protocol\":47},\"players\":{\"online\":0,\"max\":20},\"description\":{\"text\":\"Welcome\",\"color\":\"green\",\"extra\":[{\"text\":\"!\",\"color\":\"red\"}]}}";
        assert(server_status_parse_json(colored,strlen(colored),&r));assert(!strcmp(r.motd,"\xc2\xa7" "aWelcome\xc2\xa7" "c!\xc2\xa7" "a\xc2\xa7" "7"));}
    {const char *hidden="{\"version\":{\"protocol\":-1,\"name\":\"Proxy\"},\"description\":\"Hidden players\"}";
        assert(server_status_parse_json(hidden,strlen(hidden),&r)&&r.protocol==-1&&r.online==-1&&r.maximum==-1);}
    {char deep[100];memset(deep,'[',30);memset(deep+30,']',30);assert(!server_status_parse_json(deep,60,&r));}
    assert(!server_status_parse_json(status_json,SERVER_STATUS_JSON_MAX+1,&r));
    assert(server_status_ping_bars(-1)==-1&&server_status_ping_bars(149)==5&&server_status_ping_bars(150)==4&&
           server_status_ping_bars(300)==3&&server_status_ping_bars(600)==2&&server_status_ping_bars(1000)==1);
}
typedef struct Mock {TestSocket listener;int mode,passed;uint16_t port;
#ifdef _WIN32
    uintptr_t thread;
#else
    pthread_t thread;
#endif
} Mock;
static int recv_frame(TestSocket s,unsigned char *out,size_t cap,P47Frame *frame)
{
    size_t n=0,prefix;uint32_t length;int result;
    do{if(n>=3||!transfer(s,out+n,1,0))return 0;++n;result=p47_read_varint(out,n,&length,&prefix);}while(result==0);
    if(result!=1||length>cap-n||!transfer(s,out+n,length,0))return 0;
    return p47_decode_frame(out,n+length,-1,NULL,0,frame)==1;
}
static int mock_session(Mock *m,TestSocket s)
{
    unsigned char p[8192],payload[4100],pong[8];size_t n,prefix;uint32_t protocol,hostlen;P47Frame f;
    if(m->mode==3)return 1;
    if(!recv_frame(s,p,sizeof(p),&f)||f.packet_id!=0||p47_read_varint(f.payload,f.payload_size,&protocol,&prefix)!=1||protocol!=47)return 0;
    n=prefix;if(p47_read_varint(f.payload+n,f.payload_size-n,&hostlen,&prefix)!=1)return 0;n+=prefix;
    if(hostlen!=9||memcmp(f.payload+n,"127.0.0.1",9)||f.payload_size!=n+hostlen+3||f.payload[n+hostlen+2]!=1)return 0;
    if(!recv_frame(s,p,sizeof(p),&f)||f.packet_id!=0||f.payload_size)return 0;
    if(m->mode==4){pause_ms(4300);return 1;}
    if(m->mode==2){unsigned char oversized[]={0x80,0x80,0x08};return transfer(s,oversized,sizeof(oversized),1);}
    n=p47_write_varint(payload,sizeof(payload),(uint32_t)strlen(status_json));memcpy(payload+n,status_json,strlen(status_json));n+=strlen(status_json);
    n=p47_encode_frame(p,sizeof(p),-1,0,payload,n);
    /* Exercise framing across length/payload boundaries. */
    if(!n||!transfer(s,p,1,1)||!transfer(s,p+1,n-1,1)||!recv_frame(s,p,sizeof(p),&f)||f.packet_id!=1||f.payload_size!=8)return 0;
    memcpy(pong,f.payload,8);if(m->mode==1)pong[0]^=1;
    n=p47_encode_frame(p,sizeof(p),-1,1,pong,8);return n&&transfer(s,p,n,1);
}
#ifdef _WIN32
static unsigned __stdcall worker(void *arg)
#else
static void *worker(void *arg)
#endif
{
    Mock *m=(Mock *)arg;TestSocket s=accept(m->listener,NULL,NULL);
    if(s!=TEST_INVALID){
#ifdef _WIN32
        DWORD timeout=2000;(void)setsockopt(s,SOL_SOCKET,SO_RCVTIMEO,(const char *)&timeout,sizeof(timeout));
#else
        struct timeval timeout={2,0};(void)setsockopt(s,SOL_SOCKET,SO_RCVTIMEO,&timeout,sizeof(timeout));
#endif
        m->passed=mock_session(m,s);test_close(s);
    }
#ifdef _WIN32
    return 0;
#else
    return NULL;
#endif
}
static void start_mock(Mock *m,int mode)
{
    struct sockaddr_in address;
#ifdef _WIN32
    int length=sizeof(address);
#else
    socklen_t length=sizeof(address);
#endif
    memset(m,0,sizeof(*m));m->mode=mode;m->listener=socket(AF_INET,SOCK_STREAM,IPPROTO_TCP);assert(m->listener!=TEST_INVALID);
    memset(&address,0,sizeof(address));address.sin_family=AF_INET;assert(inet_pton(AF_INET,"127.0.0.1",&address.sin_addr)==1);
    assert(bind(m->listener,(struct sockaddr *)&address,sizeof(address))==0&&listen(m->listener,1)==0);
    assert(getsockname(m->listener,(struct sockaddr *)&address,&length)==0);m->port=ntohs(address.sin_port);
#ifdef _WIN32
    m->thread=_beginthreadex(NULL,0,worker,m,0,NULL);assert(m->thread);
#else
    assert(pthread_create(&m->thread,NULL,worker,m)==0);
#endif
}
static void finish_mock(Mock *m)
{
#ifdef _WIN32
    assert(WaitForSingleObject((HANDLE)m->thread,6000)==WAIT_OBJECT_0);CloseHandle((HANDLE)m->thread);
#else
    pthread_join(m->thread,NULL);
#endif
    assert(m->passed);test_close(m->listener);
}
static void saved_protocol_tests(void)
{
    char host[256];uint16_t port;
    assert(server_address_parse("localhost",host,sizeof(host),&port)&&!strcmp(host,"localhost")&&port==25565);
    assert(server_address_parse("example.org:25566",host,sizeof(host),&port)&&!strcmp(host,"example.org")&&port==25566);
    assert(server_address_parse("[::1]:25567",host,sizeof(host),&port)&&!strcmp(host,"::1")&&port==25567);
    assert(server_address_parse("2001:db8::1",host,sizeof(host),&port)&&!strcmp(host,"2001:db8::1")&&port==25565);
    assert(!server_address_parse("[::1]:0",host,sizeof(host),&port)&&!server_address_parse("host:65536",host,sizeof(host),&port));
    assert(!server_address_parse("host:",host,sizeof(host),&port)&&!server_address_parse("[::1",host,sizeof(host),&port));
    assert(!server_address_parse("host:abc",host,sizeof(host),&port)&&!server_address_parse("a b",host,sizeof(host),&port));
    assert(!server_address_parse("localhost",host,3,&port));
    ServerList list,loaded;FILE *f=fopen("status-servers-test.tmp","wb");assert(f);
    fputs("Legacy\tlocalhost:25565\t0\nModern\tlocalhost:25566\t1\t47\nBad\tlocalhost\t0\t999\n",f);assert(!fclose(f));
    server_list_load(&list,"status-servers-test.tmp");assert(list.count==2&&list.entries[0].protocol==14&&list.entries[1].protocol==47);
    assert(server_list_save(&list,"status-servers-test.tmp"));server_list_load(&loaded,"status-servers-test.tmp");assert(loaded.count==2&&loaded.entries[1].protocol==47);
    assert(!remove("status-servers-test.tmp"));
}
int main(void)
{
    ServerStatusBrowser *b;Mock mocks[5];int i,done=0;
#ifdef _WIN32
    WSADATA data;assert(WSAStartup(MAKEWORD(2,2),&data)==0);
#endif
    parser_tests();saved_protocol_tests();b=server_status_create();assert(b);
    for(i=0;i<5;++i){start_mock(mocks+i,i);assert(server_status_request(b,(unsigned)i,"127.0.0.1",mocks[i].port,i==3?14:47));}
    assert(!server_status_request(b,64,"localhost",25565,14));assert(!server_status_request(b,1,"localhost",0,14));assert(!server_status_request(b,1,"localhost",25565,999));
    for(i=0;i<6000;++i){unsigned k;done=0;server_status_tick(b);for(k=0;k<5;++k){ServerStatusState s=server_status_get(b,k)->state;done+=s==SERVER_STATUS_ONLINE||s==SERVER_STATUS_ERROR;}if(done==5)break;pause_ms(1);}
    assert(done==5);assert(server_status_get(b,0)->state==SERVER_STATUS_ONLINE&&server_status_get(b,0)->ping_ms>=0&&server_status_get(b,0)->online==2&&server_status_get(b,0)->icon_size>0);
    assert(server_status_get(b,1)->state==SERVER_STATUS_ERROR&&server_status_get(b,2)->state==SERVER_STATUS_ERROR);
    assert(server_status_get(b,3)->state==SERVER_STATUS_ONLINE&&server_status_get(b,3)->reachability_only&&server_status_get(b,3)->ping_ms==-1&&server_status_get(b,3)->online==-1);
    assert(server_status_get(b,4)->state==SERVER_STATUS_ERROR&&strstr(server_status_get(b,4)->error,"timed out"));
    server_status_clear(b);assert(server_status_get(b,0)->state==SERVER_STATUS_IDLE);server_status_destroy(b);
    for(i=0;i<5;++i)finish_mock(mocks+i);
#ifdef _WIN32
    WSACleanup();
#endif
    puts("server_status_test: modern status/pong/favicon, Beta TCP, malformed replies, timeouts and protocol persistence passed");return 0;
}
