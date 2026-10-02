#include "server_icon_png.h"
#include <string.h>
#include <zlib.h>
static unsigned png_u32(const unsigned char *p)
{return ((unsigned)p[0]<<24)|((unsigned)p[1]<<16)|((unsigned)p[2]<<8)|p[3];}
int server_icon_png_valid(const unsigned char *png,size_t size)
{
    unsigned char compressed[16384],scratch[4096];size_t at=8,n=0;int idat=0,end=0,result=0,header=0;z_stream stream;
    if(!png||size<45||size>16384||memcmp(png,"\x89PNG\r\n\x1a\n",8)||png_u32(png+8)!=13||
       memcmp(png+12,"IHDR",4)||png_u32(png+16)!=64||png_u32(png+20)!=64)return 0;
    while(at<size){unsigned length;uLong crc;if(size-at<12)return 0;length=png_u32(png+at);
        if(length>size-at-12)return 0;
        crc=crc32(0L,Z_NULL,0);crc=crc32(crc,png+at+4,length+4);
        if((unsigned)crc!=png_u32(png+at+8+length))return 0;
        if(!memcmp(png+at+4,"IHDR",4)){if(header++)return 0;}
        if(!memcmp(png+at+4,"IDAT",4)){if(length>sizeof(compressed)-n)return 0;memcpy(compressed+n,png+at+8,length);n+=length;idat=1;}
        if(!memcmp(png+at+4,"IEND",4)){if(length||at+12!=size)return 0;end=1;break;}at+=12+length;
    }if(!end||!idat||!n)return 0;
    /* Even interlaced 16-bit RGBA64x64 fits below64KiB. The bound is checked
     * before stb_image may expand its temporary buffer for a malicious stream. */
    memset(&stream,0,sizeof(stream));stream.next_in=compressed;stream.avail_in=(uInt)n;
    if(inflateInit(&stream)!=Z_OK)return 0;
    do{stream.next_out=scratch;stream.avail_out=sizeof(scratch);result=inflate(&stream,Z_NO_FLUSH);
        if(stream.total_out>65536){result=Z_DATA_ERROR;break;}
    }while(result==Z_OK);
    end=result==Z_STREAM_END&&!stream.avail_in;(void)inflateEnd(&stream);return end;
}
