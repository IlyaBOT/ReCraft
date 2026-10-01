#ifndef RECRAFT_WIN_GL_COMPAT_H
#define RECRAFT_WIN_GL_COMPAT_H

/* Optional modern MinGW shim for raylib 1.4 name collisions. Not used on Snow Leopard. */
#if defined(_WIN32) && !defined(RECRAFT_NO_WIN_SHIM)
#define Rectangle Win32Rectangle
#define CloseWindow Win32CloseWindow
#define ShowCursor Win32ShowCursor
#define SwapBuffers Win32SwapBuffers
#include <windows.h>
#undef Rectangle
#undef CloseWindow
#undef ShowCursor
#undef SwapBuffers
#undef LoadImage
#undef DrawText
#undef DrawTextEx
#undef PlaySound
#ifdef ERROR
#undef ERROR
#endif
#ifdef near
#undef near
#endif
#ifdef far
#undef far
#endif
#endif

#endif
