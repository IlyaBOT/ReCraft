#include "../src/network/protocols/protocol_1_8_47.h"
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

static void varints(void)
{
    static const uint32_t values[]={0,1,127,128,255,2097151,2147483647u,UINT32_MAX};
    uint8_t bytes[8]; size_t i,j,size,used; uint32_t decoded;
    for(i=0;i<sizeof(values)/sizeof(values[0]);++i) {
        size=p47_write_varint(bytes,sizeof(bytes),values[i]);
        assert(size>=1 && size<=5);
        for(j=0;j<size;++j) assert(p47_read_varint(bytes,j,&decoded,&used)==0);
        assert(p47_read_varint(bytes,size,&decoded,&used)==1);
        assert(decoded==values[i] && used==size);
    }
    { const uint8_t overflow[]={0xff,0xff,0xff,0xff,0x10};
      assert(p47_read_varint(overflow,sizeof(overflow),&decoded,&used)==-1); }
    { const uint8_t too_long[]={0x80,0x80,0x80,0x80,0x80};
      assert(p47_read_varint(too_long,sizeof(too_long),&decoded,&used)==-1); }
    assert(p47_write_varint(bytes,0,128)==0);
}
static void frames(void)
{
    uint8_t wire[4096],scratch[2048],payload[1024];
    P47Frame frame; size_t n,i;
    const uint8_t small[]={0x10,0x20,0x30};
    n=p47_encode_frame(wire,sizeof(wire),-1,0x2a,small,sizeof(small));
    assert(n==5 && memcmp(wire,"\x04\x2a\x10\x20\x30",5)==0);
    for(i=0;i<n;++i) assert(p47_decode_frame(wire,i,-1,scratch,sizeof(scratch),&frame)==0);
    assert(p47_decode_frame(wire,n,-1,scratch,sizeof(scratch),&frame)==1);
    assert(frame.frame_size==n && frame.packet_id==0x2a && !frame.compressed);
    assert(frame.payload_size==sizeof(small) && memcmp(frame.payload,small,sizeof(small))==0);

    n=p47_encode_frame(wire,sizeof(wire),256,0x2a,small,sizeof(small));
    assert(n==6 && wire[0]==5 && wire[1]==0); /* Post-compression envelope, raw body. */
    assert(p47_decode_frame(wire,n,256,scratch,sizeof(scratch),&frame)==1);
    assert(!frame.compressed && frame.packet_id==0x2a && frame.payload_size==3);

    for(i=0;i<sizeof(payload);++i) payload[i]=(uint8_t)(i&15u);
    n=p47_encode_frame(wire,sizeof(wire),256,0x21,payload,sizeof(payload));
    assert(n>0 && n<sizeof(payload));
    for(i=0;i<n;++i) assert(p47_decode_frame(wire,i,256,scratch,sizeof(scratch),&frame)==0);
    assert(p47_decode_frame(wire,n,256,scratch,sizeof(scratch),&frame)==1);
    assert(frame.compressed && frame.frame_size==n && frame.packet_id==0x21);
    assert(frame.payload_size==sizeof(payload) && memcmp(frame.payload,payload,sizeof(payload))==0);
    assert(p47_decode_frame(wire,n,256,scratch,32,&frame)==-1);
    wire[n-1]^=1; /* zlib checksum mismatch cannot produce a frame. */
    assert(p47_decode_frame(wire,n,256,scratch,sizeof(scratch),&frame)==-1);
    wire[n-1]^=1;

    { const uint8_t invalid_length[]={0x80,0x80,0x80};
      assert(p47_decode_frame(invalid_length,sizeof(invalid_length),-1,scratch,sizeof(scratch),&frame)==-1); }
    { const uint8_t empty_frame[]={0};
      assert(p47_decode_frame(empty_frame,sizeof(empty_frame),-1,scratch,sizeof(scratch),&frame)==-1); }
    assert(p47_encode_frame(wire,4,-1,0x21,payload,sizeof(payload))==0);
}
int main(void)
{
    varints(); frames();
    puts("protocol_47_test: bounded VarInt and frame codec passed");
    return 0;
}
