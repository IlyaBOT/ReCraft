#ifndef RECRAFT_PIXEL_FONT_H
#define RECRAFT_PIXEL_FONT_H

#include "raylib.h"

int MeasureMinecraftText(const char *text, int height);
void DrawMinecraftText(const char *text, int x, int y, int height, Color color, int shadow);
void DrawMinecraftTextCentered(const char *text, int center_x, int y, int height,
                               Color color, int shadow);

#endif
