#ifndef RECRAFT_COMMANDS_H
#define RECRAFT_COMMANDS_H
#include "player.h"
/* Local singleplayer commands only. Multiplayer sends the original chat line
 * to the server, including plugin commands, without calling this function. */
int game_command(World *world,Player *player,const char *line,char *reply,size_t capacity);
#endif
