#include "display.h"

#ifdef _WIN32
#define GLFW_EXPOSE_NATIVE_WIN32
#define GLFW_EXPOSE_NATIVE_WGL
#include "GLFW/glfw3native.h"
#include <windows.h>
#elif defined(__APPLE__)
#include <OpenGL/gl.h>
#else
#include <GL/gl.h>
#endif
#ifdef _WIN32
#include <GL/gl.h>
#endif

#include <string.h>

int recraft_screen_width(void)
{
    GLFWwindow *window=glfwGetCurrentContext();
    int width=0;
    if (window) glfwGetFramebufferSize(window,&width,NULL);
    return width>0 ? width : 1;
}

int recraft_screen_height(void)
{
    GLFWwindow *window=glfwGetCurrentContext();
    int height=0;
    if (window) glfwGetFramebufferSize(window,NULL,&height);
    return height>0 ? height : 1;
}

void recraft_begin_2d(void)
{
    int width=recraft_screen_width(),height=recraft_screen_height();
    glViewport(0,0,width,height);
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glOrtho(0,width,height,0,0,1);
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
}

void recraft_display_init(RecraftDisplay *display)
{
    memset(display,0,sizeof(*display));
    display->window=glfwGetCurrentContext();
}

void recraft_display_fullscreen(RecraftDisplay *display, int enable)
{
    GLFWwindow *window=display->window;
    if (!window || display->fullscreen==(enable!=0)) return;
    if (enable) {
        GLFWmonitor *monitor=glfwGetPrimaryMonitor();
        const GLFWvidmode *mode=monitor ? glfwGetVideoMode(monitor) : NULL;
        if (!mode) return;
        glfwGetWindowPos(window,&display->window_x,&display->window_y);
        glfwGetWindowSize(window,&display->window_width,&display->window_height);
#ifdef _WIN32
        {
            HWND hwnd=glfwGetWin32Window(window);
            MONITORINFO info;
            HMONITOR native=MonitorFromWindow(hwnd,MONITOR_DEFAULTTONEAREST);
            memset(&info,0,sizeof(info)); info.cbSize=sizeof(info);
            display->window_style=(long)GetWindowLongPtr(hwnd,GWL_STYLE);
            if (GetMonitorInfo(native,&info)) {
                SetWindowLongPtr(hwnd,GWL_STYLE,
                    (LONG_PTR)(display->window_style & ~WS_OVERLAPPEDWINDOW));
                SetWindowPos(hwnd,HWND_TOP,info.rcMonitor.left,info.rcMonitor.top,
                    info.rcMonitor.right-info.rcMonitor.left,
                    info.rcMonitor.bottom-info.rcMonitor.top,
                    SWP_FRAMECHANGED|SWP_SHOWWINDOW);
            }
        }
#else
        /* GLFW 3.1 has no in-place monitor switch. A monitor-sized window
         * preserves the GL context and avoids changing the display mode. */
        glfwSetWindowPos(window,0,0);
        glfwSetWindowSize(window,mode->width,mode->height);
#endif
        display->fullscreen=1;
    } else {
#ifdef _WIN32
        HWND hwnd=glfwGetWin32Window(window);
        SetWindowLongPtr(hwnd,GWL_STYLE,(LONG_PTR)display->window_style);
        SetWindowPos(hwnd,HWND_NOTOPMOST,display->window_x,display->window_y,
            display->window_width,display->window_height,
            SWP_FRAMECHANGED|SWP_SHOWWINDOW);
#else
        glfwSetWindowSize(window,display->window_width,display->window_height);
        glfwSetWindowPos(window,display->window_x,display->window_y);
#endif
        display->fullscreen=0;
    }
}
