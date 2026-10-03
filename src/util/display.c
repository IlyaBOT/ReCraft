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
#include <stdio.h>

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

int recraft_display_check_resize(void)
{
    GLFWwindow *window=glfwGetCurrentContext();
    GLuint probe; GLint binding; int original_width,original_height,i,ok=1;
    static const int sizes[2][2]={{800,600},{1280,800}};
    const unsigned char pixel[4]={64,128,192,255};
    if(!window || !glfwGetWindowAttrib(window,GLFW_RESIZABLE)) return 0;
    glfwGetWindowSize(window,&original_width,&original_height);
    glGetIntegerv(GL_TEXTURE_BINDING_2D,&binding); glGenTextures(1,&probe);
    glBindTexture(GL_TEXTURE_2D,probe); glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA,1,1,0,GL_RGBA,GL_UNSIGNED_BYTE,pixel);
    for(i=0;i<2;++i) {
        double deadline=glfwGetTime()+1; int width=0,height=0;
        glfwSetWindowSize(window,sizes[i][0],sizes[i][1]);
        do { glfwPollEvents(); glfwGetWindowSize(window,&width,&height); }
        while((width!=sizes[i][0] || height!=sizes[i][1]) && glfwGetTime()<deadline);
        if(width!=sizes[i][0] || height!=sizes[i][1] || recraft_screen_width()<1 || recraft_screen_height()<1 || !glIsTexture(probe)) ok=0;
    }
#ifdef _WIN32
    {
        HWND hwnd=glfwGetWin32Window(window);
        if(!(GetWindowLongPtr(hwnd,GWL_STYLE)&WS_MAXIMIZEBOX)) ok=0;
        ShowWindow(hwnd,SW_MAXIMIZE); glfwPollEvents();
        if(!IsZoomed(hwnd) || !glIsTexture(probe) || recraft_screen_width()<1) ok=0;
        ShowWindow(hwnd,SW_RESTORE);
    }
#endif
    glfwSetWindowSize(window,original_width,original_height); glfwPollEvents();
    glDeleteTextures(1,&probe); glBindTexture(GL_TEXTURE_2D,(GLuint)binding);
    fprintf(stderr,"Window resize/context check: %s\n",ok ? "passed" : "failed"); return ok;
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
