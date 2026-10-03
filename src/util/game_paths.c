#include "game_paths.h"

#include <errno.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <direct.h>
#include <shellapi.h>
#else
#include <limits.h>
#include <sys/stat.h>
#include <unistd.h>
#include <sys/wait.h>
#endif

int game_open_external(const char *path_or_url)
{
    if(!path_or_url || !*path_or_url) return 0;
#ifdef _WIN32
    return (INT_PTR)ShellExecuteA(NULL,"open",path_or_url,NULL,NULL,SW_SHOWNORMAL)>32;
#else
    pid_t child=fork(); int result;
    if(child<0) return 0;
    if(!child) {
#ifdef __APPLE__
        execl("/usr/bin/open","open",path_or_url,(char *)NULL);
#else
        execlp("xdg-open","xdg-open",path_or_url,(char *)NULL);
#endif
        _exit(127);
    }
    if(waitpid(child,&result,0)<0) return 0;
    return WIFEXITED(result) && WEXITSTATUS(result)==0;
#endif
}
#ifdef __APPLE__
#include <mach-o/dyld.h>
#endif

int game_path_join(char *out, size_t capacity, const char *root, const char *relative)
{
    int n = snprintf(out, capacity, "%s/%s", root, relative);
    return n > 0 && (size_t)n < capacity;
}

int game_ensure_directory(const char *path)
{
#ifdef _WIN32
    if (_mkdir(path) == 0) return 1;
#else
    if (mkdir(path, 0700) == 0) return 1;
#endif
    return errno == EEXIST;
}

int game_executable_root(char *out, size_t capacity)
{
    char *slash;
#ifdef _WIN32
    DWORD length;
    if (capacity > 0xffffffffu) return 0;
    length = GetModuleFileNameA(NULL, out, (DWORD)capacity);
    if (!length || length >= capacity) return 0;
#elif defined(__APPLE__)
    uint32_t length;
    char resolved[PATH_MAX];
    if (capacity > 0xffffffffu) return 0;
    length = (uint32_t)capacity;
    if (_NSGetExecutablePath(out, &length) != 0) return 0;
    if (!realpath(out,resolved) || strlen(resolved) >= capacity) return 0;
    strcpy(out,resolved);
#else
    ssize_t length = readlink("/proc/self/exe", out, capacity ? capacity-1 : 0);
    if (length <= 0 || (size_t)length >= capacity) return 0;
    out[length] = '\0';
#endif
    slash = strrchr(out, '/');
#ifdef _WIN32
    {
        char *backslash = strrchr(out, '\\');
        if (!slash || (backslash && backslash > slash)) slash = backslash;
    }
#endif
    if (!slash) return 0;
    *slash = '\0';
#ifdef __APPLE__
    /* build/ReCraft.app/Contents/MacOS -> build. Data stays
       outside the signed application bundle and survives replacing it. */
    if (strlen(out) >= 15 && strcmp(out + strlen(out)-15, "/Contents/MacOS") == 0) {
        out[strlen(out)-15] = '\0';
        slash = strrchr(out, '/');
        if (!slash || strcmp(out + strlen(out)-4, ".app") != 0) return 0;
        *slash = '\0';
    }
#endif
    return out[0] != '\0';
}
