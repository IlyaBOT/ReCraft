#ifndef RECRAFT_TEST_PNG_FIXTURE_H
#define RECRAFT_TEST_PNG_FIXTURE_H

#include <assert.h>
#include <stddef.h>
#include <string.h>
#include <zlib.h>

/* Synthetic solid RGBA64x64 favicon shared by protocol and GL upload tests.
 * This fixture is generated in memory and never becomes a runtime game asset. */
static void test_png_put32(unsigned char *p,unsigned n)
{
    p[0]=(unsigned char)(n>>24);p[1]=(unsigned char)(n>>16);
    p[2]=(unsigned char)(n>>8);p[3]=(unsigned char)n;
}
static size_t test_png_chunk(unsigned char *out,const char *type,
                             const unsigned char *bytes,size_t n)
{
    uLong crc;
    test_png_put32(out,(unsigned)n);memcpy(out+4,type,4);
    if(n)memcpy(out+8,bytes,n);
    crc=crc32(0L,Z_NULL,0);crc=crc32(crc,out+4,(uInt)n+4);
    test_png_put32(out+8+n,(unsigned)crc);return n+12;
}
static size_t test_png_fixture(unsigned char *out)
{
    unsigned char header[13]={0},raw[64*(64*4+1)],compressed[200];
    uLongf n=sizeof(compressed);size_t at=8;int y,x;
    memcpy(out,"\x89PNG\r\n\x1a\n",8);
    test_png_put32(header,64);test_png_put32(header+4,64);header[8]=8;header[9]=6;
    memset(raw,0,sizeof(raw));
    for(y=0;y<64;++y)for(x=0;x<64;++x) {
        size_t p=(size_t)y*257+1+x*4;
        raw[p]=48;raw[p+1]=96;raw[p+2]=192;raw[p+3]=255;
    }
    assert(compress2(compressed,&n,raw,sizeof(raw),Z_BEST_COMPRESSION)==Z_OK);
    at+=test_png_chunk(out+at,"IHDR",header,13);
    at+=test_png_chunk(out+at,"IDAT",compressed,n);
    at+=test_png_chunk(out+at,"IEND",NULL,0);
    return at;
}

#endif
