#ifndef RECRAFT_DISPLAY_H
#define RECRAFT_DISPLAY_H

#include "GLFW/glfw3.h"

/* Query the live framebuffer: raylib 1.4 does not update GetScreenWidth/Height
 * in its GLFW resize callback. Keep the GL context and display mode intact. */
int recraft_screen_width(void);
int recraft_screen_height(void);
void recraft_begin_2d(void);

typedef struct RecraftDisplay {
    GLFWwindow *window;
    int fullscreen;
    int window_x, window_y, window_width, window_height;
#ifdef _WIN32
    long window_style;
#endif
} RecraftDisplay;

void recraft_display_init(RecraftDisplay *display);
void recraft_display_fullscreen(RecraftDisplay *display, int enable);

#endif
