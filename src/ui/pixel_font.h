#ifndef RECRAFT_PIXEL_FONT_H
#define RECRAFT_PIXEL_FONT_H

#include "raylib.h"

/* Decode one UTF-8 character; malformed input advances safely to a fallback. */
unsigned MinecraftTextCodepoint(const char **cursor);
unsigned MinecraftGlyph(unsigned codepoint);
int MinecraftGlyphWidth(unsigned glyph); /* Source pixels in the 128x128 atlas. */

int MeasureMinecraftText(const char *text, int height);
void DrawMinecraftText(const char *text, int x, int y, int height, Color color, int shadow);
void DrawMinecraftTextCentered(const char *text, int center_x, int y, int height,
                               Color color, int shadow);

#endif
