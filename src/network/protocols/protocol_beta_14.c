#include "protocol_beta_14.h"
#include <string.h>
#include <math.h>

uint16_t beta14_u16(const uint8_t *p) { return (uint16_t)((uint16_t)p[0]<<8 | p[1]); }
uint32_t beta14_u32(const uint8_t *p) { return (uint32_t)p[0]<<24 | (uint32_t)p[1]<<16 | (uint32_t)p[2]<<8 | p[3]; }
int32_t beta14_i32(const uint8_t *p) { uint32_t u=beta14_u32(p); int32_t s; memcpy(&s,&u,4); return s; }
float beta14_f32(const uint8_t *p) { uint32_t u=beta14_u32(p); float f; memcpy(&f,&u,4); return f; }
double beta14_f64(const uint8_t *p) { uint64_t u=(uint64_t)beta14_u32(p)<<32 | beta14_u32(p+4); double d; memcpy(&d,&u,8); return d; }
static void put16(uint8_t *p, unsigned v) { p[0]=(uint8_t)(v>>8); p[1]=(uint8_t)v; }
static void put32(uint8_t *p, uint32_t v) { p[0]=(uint8_t)(v>>24); p[1]=(uint8_t)(v>>16); p[2]=(uint8_t)(v>>8); p[3]=(uint8_t)v; }
static void putf32(uint8_t *p, float f) { uint32_t u; memcpy(&u,&f,4); put32(p,u); }
static void putf64(uint8_t *p, double d) { uint64_t u; memcpy(&u,&d,8); put32(p,(uint32_t)(u>>32)); put32(p+4,(uint32_t)u); }

/* Cursor scans never consume a partial field. Negative lengths are malformed. */
static int string_end(const uint8_t *p,size_t n,size_t *off,size_t limit)
{
    size_t len;
    if (*off>n || n-*off<2) return 0;
    len=beta14_u16(p+*off);
    if (len>limit) return -1;
    if (n-*off-2<len*2) return 0;
    *off+=2+len*2; return 1;
}
/* Java DataInput.readUTF uses a byte count, unlike the packet String16 type. */
static int string8_end(const uint8_t *p,size_t n,size_t *off,size_t limit)
{
    size_t len;
    if (*off>n || n-*off<2) return 0;
    len=beta14_u16(p+*off);
    if (len>limit) return -1;
    if (n-*off-2<len) return 0;
    *off+=2+len; return 1;
}
static int slot_end(const uint8_t *p,size_t n,size_t *off)
{
    size_t count;
    if (*off>n || n-*off<2) return 0;
    count=beta14_u16(p+*off)==65535u ? 2:5;
    if (n-*off<count) return 0;
    *off+=count; return 1;
}
static int metadata_end(const uint8_t *p,size_t n,size_t *off)
{
    unsigned fields=0;
    while (*off<n) {
        unsigned tag=p[(*off)++],type=tag>>5;
        size_t count;
        if (tag==127) return 1;
        if (++fields>32) return -1;
        switch(type) {
        case 0: count=1; break; case 1: count=2; break;
        case 2: case 3: count=4; break;
        case 4: /* DataWatcher uses Java writeUTF, not Packet String16. */
            if (n-*off<2) return 0;
            count=2+beta14_u16(p+*off); if (count>194) return -1; break;
        case 5: count=5; break;
        case 6: count=12; break;
        default: return -1;
        }
        if (n-*off<count) return 0;
        *off+=count;
    }
    return 0;
}

int beta14_next_packet(const uint8_t *p,size_t n,Beta14Packet *out)
{
    size_t len=0,off=0,count,i; int r;
    if (!p || !out) return -1;
    if (!n) return 0;
    switch(p[0]) {
    case 0x00: len=1; break;
    case 0x01: off=5; r=string_end(p,n,&off,16); if (r!=1) return r; len=off+9; break;
    case 0x02: case 0x03: case 0xff:
        off=1; r=string_end(p,n,&off,p[0]==2?64:32767); if(r!=1) return r; len=off; break;
    case 0x04: len=9; break; case 0x05: len=11; break;
    case 0x06: len=13; break; case 0x07: len=10; break;
    case 0x08: len=3; break; case 0x09: len=2; break;
    case 0x0a: len=2; break; case 0x0b: len=34; break;
    case 0x0c: len=10; break; case 0x0d: len=42; break;
    case 0x0e: len=12; break;
    case 0x0f: off=11; r=slot_end(p,n,&off); if(r!=1) return r; len=off; break;
    case 0x10: len=3; break; case 0x11: len=15; break;
    case 0x12: case 0x13: len=6; break;
    case 0x14: off=5; r=string_end(p,n,&off,16); if(r!=1) return r; len=off+16; break;
    case 0x15: len=25; break; case 0x16: len=9; break;
    case 0x17: if(n<22) return 0; len=beta14_i32(p+18)>0?28:22; break;
    case 0x18: if(n<20) return 0; off=20; r=metadata_end(p,n,&off); if(r!=1) return r; len=off; break;
    case 0x19: off=5; r=string_end(p,n,&off,64); if(r!=1) return r; len=off+16; break;
    case 0x1b: len=19; break; case 0x1c: len=11; break;
    case 0x1d: case 0x1e: len=5; break; case 0x1f: len=8; break;
    case 0x20: len=7; break; case 0x21: len=10; break;
    case 0x22: len=19; break; case 0x26: len=6; break;
    case 0x27: len=9; break;
    case 0x28: if(n<5) return 0; off=5; r=metadata_end(p,n,&off); if(r!=1) return r; len=off; break;
    case 0x32: len=10; break;
    case 0x33:
        if(n<18) return 0;
        count=beta14_u32(p+14); if(!count || count>BETA14_MAX_COMPRESSED) return -1;
        len=18+count; break;
    case 0x34:
        if(n<11) return 0;
        count=beta14_u16(p+9); if(count>32767) return -1; len=11+count*4; break;
    case 0x35: len=12; break; case 0x36: len=13; break;
    case 0x3c:
        if(n<33) return 0;
        count=beta14_u32(p+29); if(count>32768) return -1; len=33+count*3; break;
    case 0x3d: len=18; break; case 0x46: len=2; break; case 0x47: len=18; break;
    case 0x64: off=3; r=string8_end(p,n,&off,64); if(r!=1) return r; len=off+1; break;
    case 0x65: len=2; break;
    case 0x66: off=8; r=slot_end(p,n,&off); if(r!=1) return r; len=off; break;
    case 0x67: off=4; r=slot_end(p,n,&off); if(r!=1) return r; len=off; break;
    case 0x68:
        if(n<4) return 0;
        count=beta14_u16(p+2); if(count>1024) return -1; off=4;
        for(i=0;i<count;++i) { r=slot_end(p,n,&off); if(r!=1) return r; }
        len=off; break;
    case 0x69: len=6; break; case 0x6a: len=5; break;
    case 0x82:
        off=11; for(i=0;i<4;++i) { r=string_end(p,n,&off,15); if(r!=1) return r; } len=off; break;
    case 0x83: if(n<6) return 0; len=6+p[5]; break;
    case 0xc8: len=6; break;
    default: return -2;
    }
    if(len>BETA14_MAX_PACKET) return -1;
    if(n<len) return 0;
    out->id=p[0]; out->size=len; out->bytes=p; return 1;
}

size_t beta14_read_string(const Beta14Packet *packet,size_t off,char *out,size_t cap)
{
    size_t end=off,w=0,i;
    if(!packet || !out || !cap || string_end(packet->bytes,packet->size,&end,32767)!=1) return 0;
    for(i=off+2;i<end;i+=2) {
        uint32_t u=beta14_u16(packet->bytes+i); unsigned need;
        if(u>=0xd800 && u<=0xdbff && i+2<end) {
            uint32_t lo=beta14_u16(packet->bytes+i+2);
            if(lo>=0xdc00 && lo<=0xdfff) { u=0x10000+((u-0xd800)<<10)+lo-0xdc00; i+=2; }
            else u=0xfffd;
        } else if(u>=0xd800 && u<=0xdfff) u=0xfffd;
        if(u==0) u=0xfffd;
        need=u<128?1:u<2048?2:u<65536?3:4;
        if(w+need>=cap) break;
        if(need==1) out[w++]=(char)u;
        else {
            if(need==4) out[w++]=(char)(0xf0|(u>>18));
            if(need>=3) out[w++]=(char)((need==3?0xe0:0x80)|((u>>12)&(need==3?15:63)));
            out[w++]=(char)((need==2?0xc0:0x80)|((u>>6)&(need==2?31:63)));
            out[w++]=(char)(0x80|(u&63));
        }
    }
    out[w]=0; return end;
}
static size_t write_string(uint8_t *out,size_t cap,const char *s,size_t limit)
{
    size_t n=0,w=2,i=0;
    if(!out || !s || cap<2) return 0;
    while(s[i]) {
        uint32_t u; unsigned j,k; uint8_t b=(uint8_t)s[i++];
        if(b<128) { u=b; k=0; }
        else if(b>=0xc2 && b<=0xdf) { u=b&31; k=1; }
        else if(b>=0xe0 && b<=0xef) { u=b&15; k=2; }
        else if(b>=0xf0 && b<=0xf4) { u=b&7; k=3; }
        else return 0;
        for(j=0;j<k;++j) { if(!s[i]) return 0; b=(uint8_t)s[i++]; if((b&0xc0)!=0x80) return 0; u=(u<<6)|(b&63); }
        if((k==1 && u<128)||(k==2 && u<2048)||(k==3 && u<65536)||u>0x10ffff||(u>=0xd800 && u<=0xdfff)) return 0;
        j=u>=65536?2:1;
        if(n+j>limit || w+j*2>cap) return 0;
        if(j==2) { u-=65536; put16(out+w,0xd800|(u>>10)); w+=2; put16(out+w,0xdc00|(u&1023)); }
        else put16(out+w,u);
        w+=2; n+=j;
    }
    put16(out,(unsigned)n); return w;
}
size_t beta14_handshake(uint8_t *out,size_t cap,const char *username)
{ size_t n; if(!out || cap<1) return 0; n=write_string(out+1,cap-1,username,16); if(!n) return 0; out[0]=2; return n+1; }
size_t beta14_login(uint8_t *out,size_t cap,const char *username)
{ size_t n; if(!out || cap<14) return 0; n=write_string(out+5,cap-14,username,16); if(!n) return 0; out[0]=1; put32(out+1,14); memset(out+5+n,0,9); return n+14; }
size_t beta14_chat(uint8_t *out,size_t cap,const char *message)
{ size_t n; if(!out || cap<1) return 0; n=write_string(out+1,cap-1,message,119); if(!n) return 0; out[0]=3; return n+1; }
size_t beta14_sign_update(uint8_t *out,size_t cap,int x,int y,int z,const char lines[4][61])
{
    size_t at=11,n;int i;
    if(!out||!lines||cap<19||y<0||y>=128)return 0;
    out[0]=0x82;put32(out+1,(uint32_t)x);put16(out+5,(unsigned)y);put32(out+7,(uint32_t)z);
    for(i=0;i<4;++i){if(!memchr(lines[i],0,61))return 0;n=write_string(out+at,cap-at,lines[i],15);if(!n)return 0;at+=n;}
    return at;
}
size_t beta14_held_item(uint8_t *out,size_t cap,int slot)
{ if(!out || cap<3 || slot<0 || slot>8) return 0; out[0]=0x10; put16(out+1,(unsigned)slot); return 3; }
size_t beta14_movement(uint8_t *out,size_t cap,double x,double y,double z,float yaw,float pitch,int ground)
{
    if(!out || cap<42 || !isfinite(x)||!isfinite(y)||!isfinite(y+1.62)||
       !isfinite(z)||!isfinite(yaw)||!isfinite(pitch)) return 0;
    /* Beta 14 serverbound field order is camera Y, then feet Y. */
    out[0]=13; putf64(out+1,x); putf64(out+9,y+1.62); putf64(out+17,y); putf64(out+25,z);
    putf32(out+33,yaw); putf32(out+37,pitch); out[41]=(uint8_t)(ground!=0); return 42;
}
size_t beta14_mine(uint8_t *out,size_t cap,int status,int x,int y,int z,int face)
{
    if(!out || cap<12 || y<0 || y>127 || face<0 || face>5 || (status!=0 && status!=1 && status!=2 && status!=4)) return 0;
    out[0]=14; out[1]=(uint8_t)status; put32(out+2,(uint32_t)x); out[6]=(uint8_t)y; put32(out+7,(uint32_t)z); out[11]=(uint8_t)face; return 12;
}
size_t beta14_place(uint8_t *out,size_t cap,int x,int y,int z,int face,int item,int count,int damage)
{
    size_t n=item<0?13:16;
    if(!out || cap<n || y<0 || y>255 || (face!=255 && (face<0 || face>5)) || item < -1 || item>32767 || count<0 || count>127 || damage<0 || damage>32767) return 0;
    out[0]=15; put32(out+1,(uint32_t)x); out[5]=(uint8_t)y; put32(out+6,(uint32_t)z); out[10]=(uint8_t)face; put16(out+11,(unsigned)item);
    if(item>=0) { out[13]=(uint8_t)count; put16(out+14,(unsigned)damage); } return n;
}
size_t beta14_window_click(uint8_t *out,size_t cap,int window,int slot,int button,
                           int action,int shift,int item,int count,int damage)
{
    size_t n=item<0 ? 10 : 13;
    if(!out || cap<n || window<0 || window>255 || slot< -999 || slot>1023 ||
        button<0 || button>1 || item< -1 || item>32767 || count<0 || count>127 || damage<0 || damage>65535) return 0;
    out[0]=0x66; out[1]=(uint8_t)window; put16(out+2,(uint16_t)slot);
    out[4]=(uint8_t)button; put16(out+5,(uint16_t)action); out[7]=(uint8_t)(shift!=0);
    put16(out+8,(uint16_t)item);
    if(item>=0) { out[10]=(uint8_t)count; put16(out+11,(uint16_t)damage); }
    return n;
}
