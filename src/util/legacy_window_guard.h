#ifndef RECRAFT_LEGACY_WINDOW_GUARD_H
#define RECRAFT_LEGACY_WINDOW_GUARD_H
#ifndef GLFW_INCLUDE_NONE
#define GLFW_INCLUDE_NONE
#endif
#include <GLFW/glfw3.h>
#include <stdio.h>
#include <stdlib.h>

/* raylib 1.4 centers the result before checking it. Keep its platform backend
 * but stop cleanly if no context can be created, instead of dereferencing NULL. */
static GLFWwindow *recraft_glfw_create_window(int width,int height,const char *title,
                                             GLFWmonitor *monitor,GLFWwindow *share)
{
    GLFWwindow *window;
    glfwWindowHint(GLFW_RESIZABLE,1);
    window=glfwCreateWindow(width,height,title,monitor,share);
    if (!window) {
        fprintf(stderr,"ReCraft: unable to create an OpenGL window.\n");
        glfwTerminate();
        exit(EXIT_FAILURE);
    }
    return window;
}
#endif
