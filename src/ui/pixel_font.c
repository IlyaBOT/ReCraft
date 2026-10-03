#include "pixel_font.h"
#include "../assets/assets.h"

#include <stddef.h>
#include <stdlib.h>
#include <string.h>

static unsigned char widths[256];
static int widths_ready;
static unsigned char glyph_sizes[65536];
static int unicode_ready;
void MinecraftTextReset(void) { widths_ready=unicode_ready=0; memset(widths,0,sizeof(widths)); memset(glyph_sizes,0,sizeof(glyph_sizes)); }
static void prepare_unicode(void)
{
    void *bytes; size_t size;
    if(unicode_ready) return;
    unicode_ready=1; bytes=assets_read_file("fonts/unicode/glyph_sizes.bin",&size);
    if(bytes && size==sizeof(glyph_sizes)) {
        unsigned i;
        memcpy(glyph_sizes,bytes,size);
        for(i=0;i<sizeof(glyph_sizes);++i)
            if((glyph_sizes[i]&15)<(glyph_sizes[i]>>4)) glyph_sizes[i]=0;
    }
    free(bytes);
}

unsigned MinecraftTextCodepoint(const char **cursor)
{
    const unsigned char *p; unsigned value; int count,i;
    if(!cursor || !*cursor || !**cursor) return 0;
    p=(const unsigned char *)*cursor;
    if(*p<128) { ++*cursor; return *p; }
    if(*p>=0xc2 && *p<=0xdf) { value=*p&31; count=2; }
    else if(*p>=0xe0 && *p<=0xef) { value=*p&15; count=3; }
    else if(*p>=0xf0 && *p<=0xf4) { value=*p&7; count=4; }
    else { ++*cursor; return '?'; }
    for(i=1;i<count;++i) {
        if(!p[i] || (p[i]&0xc0)!=0x80) { ++*cursor; return '?'; }
        value=(value<<6)|(p[i]&63);
    }
    *cursor+=count;
    if((count==3 && value<0x800) || (count==4 && value<0x10000) ||
       value>0x10ffff || (value>=0xd800 && value<=0xdfff)) return '?';
    return value;
}

unsigned MinecraftGlyph(unsigned codepoint)
{
    if(codepoint>=32 && codepoint<127) return codepoint;
    prepare_unicode();
    return codepoint<65536 && glyph_sizes[codepoint] ? codepoint : '?';
}

static void prepare_widths(void)
{
    Image image;
    int ch, x, y;
    Color *pixels;
    if (widths_ready) return;
    widths_ready = 1;
    image = assets_load_image(ASSET_FONT_ASCII);
    if (!image.data) return;
    if (image.format != UNCOMPRESSED_R8G8B8A8) ImageFormat(&image, UNCOMPRESSED_R8G8B8A8);
    pixels = (Color *)image.data;
    if (pixels && image.width == 128 && image.height == 128) {
        for (ch = 0; ch < 256; ++ch) {
            int right = -1;
            for (y = 0; y < 8; ++y) for (x = 0; x < 8; ++x) {
                Color pixel = pixels[((ch/16)*8+y)*128+(ch%16)*8+x];
                if (pixel.a > 16 && x > right) right = x;
            }
            widths[ch] = (unsigned char)(right < 0 ? 4 : right+2);
            if (widths[ch] > 8) widths[ch] = 8;
        }
        widths[' '] = 4;
    }
    UnloadImage(image);
}

int MinecraftGlyphWidth(unsigned glyph)
{
    prepare_widths();
    glyph=MinecraftGlyph(glyph);
    if(glyph>=127) { unsigned size=glyph_sizes[glyph]; return ((size&15)+1-(size>>4))/2+1; }
    return widths[glyph] ? widths[glyph] : 6;
}

int MeasureMinecraftText(const char *text, int height)
{
    int width = 0,maximum=0;
    const char *p = text;
    prepare_widths();
    if (!p || height <= 0) return 0;
    while (*p) {
        unsigned ch=MinecraftTextCodepoint(&p);
        if(ch==0xa7 && *p) { MinecraftTextCodepoint(&p); continue; }
        if(ch=='\n') { if(width>maximum) maximum=width; width=0; continue; }
        width += MinecraftGlyphWidth(ch);
    }
    if(width>maximum) maximum=width;
    return (maximum*height+4)/8;
}

void DrawMinecraftText(const char *text, int x, int y, int height, Color color, int shadow)
{
    Texture2D atlas;
    int offset = 0;
    const char *p = text;
    Color original=color;
    if (!p || height <= 0) return;
    prepare_widths();
    atlas = assets_get_texture(ASSET_FONT_ASCII);
    while (*p) {
        unsigned ch = MinecraftTextCodepoint(&p);
        Rectangle src, dst;
        Vector2 origin = {0, 0};
        int px;
        if(ch==0xa7 && *p) {
            unsigned code=MinecraftTextCodepoint(&p);
            int index=code>='0' && code<='9' ? (int)(code-'0') :
                code>='a' && code<='f' ? (int)(code-'a'+10) :
                code>='A' && code<='F' ? (int)(code-'A'+10) : -1;
            if(index>=0) {
                int bright=(index>>3)*85;
                color.r=(unsigned char)(((index>>2)&1)*170+bright);
                color.g=(unsigned char)(((index>>1)&1)*170+bright);
                color.b=(unsigned char)((index&1)*170+bright);
                if(index==6) color.r=(unsigned char)(color.r+85);
                color.a=original.a;
            } else if(code=='r' || code=='R') color=original;
            continue;
        }
        if(ch=='\n') { offset=0; y+=height+height/4; continue; }
        px = x + (offset*height+4)/8;
        ch = MinecraftGlyph(ch);
        if (ch != ' ') {
            Texture2D texture=atlas;
            src.x = (float)((ch%16)*8); src.y = (float)((ch/16)*8);
            src.width = src.height = 8;
            dst.x = (float)px; dst.y = (float)y;
            dst.width = dst.height = (float)height;
            if(ch>=127) {
                unsigned size=glyph_sizes[ch],left=size>>4,right=(size&15)+1;
                texture=assets_unicode_page(ch>>8);
                src.x=(float)((ch&15)*16+left); src.y=(float)(((ch&255)>>4)*16);
                src.width=(float)(right-left); src.height=16;
                dst.width=(float)height*(right-left)/16;
            }
            if (shadow) {
                Color dark = {color.r/4,color.g/4,color.b/4,color.a};
                dst.x += height >= 16 ? 2 : 1;
                dst.y += height >= 16 ? 2 : 1;
                if(texture.id) DrawTexturePro(texture,src,dst,origin,0,dark);
                dst.x = (float)px; dst.y = (float)y;
            }
            if(texture.id) DrawTexturePro(texture,src,dst,origin,0,color);
        }
        offset += MinecraftGlyphWidth(ch);
    }
}

void DrawMinecraftTextCentered(const char *text, int center_x, int y, int height,
                               Color color, int shadow)
{
    DrawMinecraftText(text, center_x-MeasureMinecraftText(text,height)/2, y,
                      height,color,shadow);
}
