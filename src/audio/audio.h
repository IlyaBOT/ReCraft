#ifndef RECRAFT_AUDIO_H
#define RECRAFT_AUDIO_H

#include "raylib.h"

typedef enum RecraftSound {
    RECRAFT_SOUND_CLICK,
    RECRAFT_SOUND_STEP,
    RECRAFT_SOUND_BREAK,
    RECRAFT_SOUND_PLACE,
    RECRAFT_SOUND_COUNT
} RecraftSound;

typedef struct AudioState {
    int ready;
    Sound effects[RECRAFT_SOUND_COUNT];
} AudioState;

void audio_init(AudioState *audio);
void audio_play(AudioState *audio, RecraftSound effect);
void audio_shutdown(AudioState *audio);

#endif
