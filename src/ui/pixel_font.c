#include "pixel_font.h"
#include "../assets/assets.h"

#include <stddef.h>

static unsigned char widths[256];
static int widths_ready;

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

int MeasureMinecraftText(const char *text, int height)
{
    int width = 0;
    const unsigned char *p = (const unsigned char *)text;
    prepare_widths();
    if (!p || height <= 0) return 0;
    while (*p) {
        unsigned char ch = *p++;
        if (ch >= 128) ch = '?';
        width += widths[ch] ? widths[ch] : 6;
    }
    return (width*height+4)/8;
}

void DrawMinecraftText(const char *text, int x, int y, int height, Color color, int shadow)
{
    Texture2D atlas;
    int offset = 0;
    const unsigned char *p = (const unsigned char *)text;
    if (!p || height <= 0) return;
    prepare_widths();
    atlas = assets_get_texture(ASSET_FONT_ASCII);
    while (*p) {
        unsigned char ch = *p++;
        Rectangle src, dst;
        Vector2 origin = {0, 0};
        int px = x + (offset*height+4)/8;
        if (ch >= 128) ch = '?';
        if (ch != ' ') {
            src.x = (float)((ch%16)*8); src.y = (float)((ch/16)*8);
            src.width = src.height = 8;
            dst.x = (float)px; dst.y = (float)y;
            dst.width = dst.height = (float)height;
            if (shadow) {
                Color dark = {0,0,0,color.a};
                dst.x += height >= 16 ? 2 : 1;
                dst.y += height >= 16 ? 2 : 1;
                DrawTexturePro(atlas,src,dst,origin,0,dark);
                dst.x = (float)px; dst.y = (float)y;
            }
            DrawTexturePro(atlas,src,dst,origin,0,color);
        }
        offset += widths[ch] ? widths[ch] : 6;
    }
}

void DrawMinecraftTextCentered(const char *text, int center_x, int y, int height,
                               Color color, int shadow)
{
    DrawMinecraftText(text, center_x-MeasureMinecraftText(text,height)/2, y,
                      height,color,shadow);
}
