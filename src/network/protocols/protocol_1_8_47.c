#include "protocol_1_8_47.h"
#include <limits.h>
#include <stdlib.h>
#include <string.h>
#include <zlib.h>

int p47_read_varint(const uint8_t *p,size_t n,uint32_t *value,size_t *consumed)
{
    uint32_t result=0; size_t i;
    if(!p || !value || !consumed) return -1;
    for(i=0;i<5;++i) {
        uint8_t byte;
        if(i>=n) return 0;
        byte=p[i];
        if(i==4 && (byte&0xf0u)) return -1;
        result|=(uint32_t)(byte&0x7fu)<<(i*7u);
        if(!(byte&0x80u)) { *value=result; *consumed=i+1; return 1; }
    }
    return -1;
}
size_t p47_write_varint(uint8_t *out,size_t cap,uint32_t value)
{
    size_t i=0;
    if(!out) return 0;
    do {
        uint8_t byte=(uint8_t)(value&0x7fu);
        value>>=7;
        if(value) byte|=0x80u;
        if(i>=cap) return 0;
        out[i++]=byte;
    } while(value);
    return i;
}
static int p47_body(const uint8_t *body,size_t size,P47Frame *out)
{
    uint32_t packet_id; size_t id_size;
    if(p47_read_varint(body,size,&packet_id,&id_size)!=1 || packet_id>INT32_MAX) return -1;
    out->packet_id=packet_id;
    out->payload=body+id_size;
    out->payload_size=size-id_size;
    return 1;
}
int p47_decode_frame(const uint8_t *p,size_t n,int threshold,
                     uint8_t *scratch,size_t scratch_cap,P47Frame *out)
{
    uint32_t wire_length,data_length; size_t prefix,inner_prefix;
    const uint8_t *body; size_t body_size;
    int result;
    if(!p || !out || threshold < -1) return -1;
    if(n>=3 && (p[0]&0x80u) && (p[1]&0x80u) && (p[2]&0x80u)) return -1;
    result=p47_read_varint(p,n,&wire_length,&prefix);
    if(result!=1) return result;
    if(prefix>3 || !wire_length || wire_length>P47_MAX_FRAME_BYTES) return -1;
    if(n-prefix<wire_length) return 0;
    body=p+prefix; body_size=wire_length;
    out->frame_size=prefix+wire_length; out->compressed=0;
    if(threshold>=0) {
        result=p47_read_varint(body,body_size,&data_length,&inner_prefix);
        if(result!=1 || inner_prefix>=body_size) return -1;
        body+=inner_prefix; body_size-=inner_prefix;
        if(data_length) {
            z_stream stream;
            if(data_length>P47_MAX_UNCOMPRESSED_BYTES || !scratch ||
               data_length>scratch_cap || body_size>UINT_MAX) return -1;
            memset(&stream,0,sizeof(stream));
            stream.next_in=(Bytef *)body; stream.avail_in=(uInt)body_size;
            stream.next_out=scratch; stream.avail_out=(uInt)data_length;
            result=inflateInit(&stream);
            if(result==Z_OK) { result=inflate(&stream,Z_FINISH); (void)inflateEnd(&stream); }
            if(result!=Z_STREAM_END || stream.total_out!=data_length || stream.avail_in!=0) return -1;
            body=scratch; body_size=data_length; out->compressed=1;
        }
    }
    return p47_body(body,body_size,out);
}
size_t p47_encode_frame(uint8_t *out,size_t cap,int threshold,uint32_t id,
                        const uint8_t *payload,size_t payload_size)
{
    uint8_t id_bytes[5],length_bytes[5],data_length_bytes[5];
    size_t id_size,body_size,wire_size,prefix,data_prefix;
    if(!out || threshold < -1 || id>INT32_MAX ||
       (payload_size && !payload)) return 0;
    id_size=p47_write_varint(id_bytes,sizeof(id_bytes),id);
    if(!id_size || payload_size>P47_MAX_UNCOMPRESSED_BYTES-id_size) return 0;
    body_size=id_size+payload_size;
    if(threshold<0 || body_size<(size_t)threshold) {
        wire_size=body_size+(threshold>=0?1u:0u);
        if(wire_size>P47_MAX_FRAME_BYTES) return 0;
        prefix=p47_write_varint(length_bytes,sizeof(length_bytes),(uint32_t)wire_size);
        if(!prefix || prefix+wire_size>cap) return 0;
        memcpy(out,length_bytes,prefix);
        if(threshold>=0) out[prefix++]=0; /* Data Length = zero. */
        memcpy(out+prefix,id_bytes,id_size);
        if(payload_size) memmove(out+prefix+id_size,payload,payload_size);
        return prefix+body_size;
    }
    {
        uint8_t *raw; uLongf compressed_size; int result;
        if(cap<=8 || cap-8>ULONG_MAX) return 0;
        raw=(uint8_t *)malloc(body_size);
        if(!raw) return 0;
        memcpy(raw,id_bytes,id_size);
        if(payload_size) memcpy(raw+id_size,payload,payload_size);
        compressed_size=(uLongf)(cap-8);
        result=compress2(out+8,&compressed_size,raw,(uLong)body_size,Z_DEFAULT_COMPRESSION);
        free(raw);
        if(result!=Z_OK) return 0;
        data_prefix=p47_write_varint(data_length_bytes,sizeof(data_length_bytes),(uint32_t)body_size);
        wire_size=data_prefix+(size_t)compressed_size;
        if(!data_prefix || wire_size>P47_MAX_FRAME_BYTES) return 0;
        prefix=p47_write_varint(length_bytes,sizeof(length_bytes),(uint32_t)wire_size);
        if(!prefix || prefix+wire_size>cap) return 0;
        memmove(out+prefix+data_prefix,out+8,(size_t)compressed_size);
        memcpy(out,length_bytes,prefix);
        memcpy(out+prefix,data_length_bytes,data_prefix);
        return prefix+wire_size;
    }
}
