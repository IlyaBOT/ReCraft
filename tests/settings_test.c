#include "game/settings.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
int main(void)
{
    UiOptions saved={0},loaded={0}; FILE *file;
    assert(settings_player_name_valid("IlyaBOT") && settings_player_name_valid("A") && settings_player_name_valid("0123456789abcdef"));
    assert(!settings_player_name_valid("") && !settings_player_name_valid("0123456789abcdefg") && !settings_player_name_valid("Bad Name") && !settings_player_name_valid("\xd0\xaf"));
    strcpy(saved.player_name,"IlyaBOT"); saved.difficulty=3;
    assert(settings_save(&saved,"settings-test.cfg")); settings_load(&loaded,"settings-test.cfg");
    assert(!strcmp(loaded.player_name,"IlyaBOT") && loaded.difficulty==3);
    file=fopen("settings-test.cfg","wb"); assert(file); fputs("player_name=Bad Name\nplayer_name=0123456789abcdefg\n",file); assert(!fclose(file));
    settings_load(&loaded,"settings-test.cfg"); assert(!strcmp(loaded.player_name,"IlyaBOT"));
    assert(!remove("settings-test.cfg")); puts("Offline name validation and config restart passed"); return 0;
}
