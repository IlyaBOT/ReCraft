#ifndef RECRAFT_GAME_PATHS_H
#define RECRAFT_GAME_PATHS_H

#include <stddef.h>

/* Runtime files live beside the executable (beside the .app on macOS). */
int game_executable_root(char *out, size_t capacity);
int game_path_join(char *out, size_t capacity, const char *root, const char *relative);
int game_ensure_directory(const char *path);
int game_open_external(const char *path_or_url);

#endif
