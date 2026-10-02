#ifndef RECRAFT_AUDIO_H
#define RECRAFT_AUDIO_H

#include "raylib.h"
#include "sound_policy.h"
#include "music_stream.h"

typedef enum RecraftSound {
    RECRAFT_SOUND_CLICK,
    RECRAFT_SOUND_STEP,
    RECRAFT_SOUND_BREAK,
    RECRAFT_SOUND_PLACE,
    RECRAFT_SOUND_PORTAL,
    RECRAFT_SOUND_COUNT
} RecraftSound;

typedef struct AudioState {
    int ready;
    unsigned voices[16],voice_next;
    MusicStream music;
    MusicStream record;
    MusicSchedule schedule;
    MusicSchedule effects_random;
    float sound_volume,music_volume;
    double controller_time;
    float walked;
    int next_step;
    unsigned weather_ticks;
    int rain_sound_counter;
} AudioState;

void audio_init(AudioState *audio);
void audio_play(AudioState *audio, RecraftSound effect);
void audio_named(AudioState *audio,const char *key,float volume,float pitch,int spatial,float x,float y,float z);
/* action: 0 footstep, 1 mining hit, 2 break, 3 placement. */
void audio_block(AudioState *audio,unsigned block,int action,float x,float y,float z);
void audio_listener(AudioState *audio,float x,float y,float z,float yaw);
struct World;
void audio_weather_tick(AudioState *audio,struct World *world,float x,float y,float z,int fancy);
void audio_ambient_tick(AudioState *audio,struct World *world,float x,float y,float z);
void audio_update(AudioState *audio,double elapsed,int controller_active,int music_volume,int sound_volume);
void audio_shutdown(AudioState *audio);
void audio_stop_records(AudioState *audio);

#endif
