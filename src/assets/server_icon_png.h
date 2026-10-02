#ifndef RECRAFT_SERVER_ICON_PNG_H
#define RECRAFT_SERVER_ICON_PNG_H
#include <stddef.h>
/* Header/chunk CRC/inflate bounds before the pinned PNG decoder receives data. */
int server_icon_png_valid(const unsigned char *png,size_t size);
#endif
