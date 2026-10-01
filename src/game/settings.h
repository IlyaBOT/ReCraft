#ifndef RECRAFT_SETTINGS_H
#define RECRAFT_SETTINGS_H
#include "../ui/ui.h"
void settings_load(UiOptions *options, const char *path);
int settings_save(const UiOptions *options, const char *path);
#endif
