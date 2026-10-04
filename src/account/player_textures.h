#ifndef RECRAFT_PLAYER_TEXTURES_H
#define RECRAFT_PLAYER_TEXTURES_H
#include "../util/https.h"
#define PLAYER_TEXTURE_LIMIT 64
typedef struct PlayerTextures PlayerTextures;
typedef struct PlayerTextureResult {
    unsigned slot; char id[33];
    unsigned char *skin,*cape; size_t skin_size,cape_size;
} PlayerTextureResult;
typedef int (*PlayerTextureRequest)(void *,const char *,HttpsResponse *);
PlayerTextures *player_textures_create(void);
void player_textures_destroy(PlayerTextures *textures);
/* UUID is preferred for future protocols; Beta supplies a validated name.
 * Returns a session-local cache slot, or -1. No credentials are used. */
int player_textures_queue(PlayerTextures *,const char *name,const char *uuid);
int player_textures_poll(PlayerTextures *,PlayerTextureResult *out);
void player_texture_result_free(PlayerTextureResult *out);
/* Pure bounded parser and injectable request path for headless tests. */
int player_texture_urls(const void *,size_t,const char *uuid,char *skin,char *cape);
int player_texture_fetch(const char *name,const char *uuid,PlayerTextureRequest,void *,PlayerTextureResult *);
#endif
