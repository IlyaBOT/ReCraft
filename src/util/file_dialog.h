#ifndef RECRAFT_FILE_DIALOG_H
#define RECRAFT_FILE_DIALOG_H
#include <stddef.h>
int game_choose_skin_file(char *path,size_t capacity);
void *game_read_small_file(const char *path,size_t limit,size_t *size);
#endif
