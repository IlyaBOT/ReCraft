#include "audio.h"
#include "../assets/assets.h"

#include <math.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "AL/alc.h"

static Sound make_effect(int kind)
{
    const unsigned int rate = 22050;
    const unsigned int count = kind == RECRAFT_SOUND_CLICK ? 1050u : 2600u;
    int16_t *pcm = (int16_t *)malloc(count * sizeof(int16_t));
    unsigned int i, noise = 0x92375a31u;
    Wave wave;
    Sound empty = { 0 };
    if (!pcm) return empty;
    for (i = 0; i < count; ++i) {
        float t = (float)i / (float)rate;
        float decay = 1.0f - (float)i / (float)count;
        float sample;
        noise = noise * 1664525u + 1013904223u;
        if (kind == RECRAFT_SOUND_CLICK)
            sample = sinf(6.2831853f * (850.0f - t * 6000.0f) * t) * decay;
        else if (kind == RECRAFT_SOUND_STEP)
            sample = ((float)((noise >> 16) & 65535u) / 32768.0f - 1.0f) * decay * 0.28f;
        else if (kind == RECRAFT_SOUND_BREAK)
            sample = ((float)((noise >> 16) & 65535u) / 32768.0f - 1.0f) * decay * 0.42f;
        else
            sample = sinf(6.2831853f * (125.0f - t * 400.0f) * t) * decay * 0.65f;
        pcm[i] = (int16_t)(sample * 17000.0f);
    }
    wave.data = pcm;
    wave.dataSize = count * (unsigned int)sizeof(int16_t);
    wave.sampleRate = rate;
    wave.bitsPerSample = 16;
    wave.channels = 1;
    /* raylib 1.4 copies the samples to OpenAL and frees wave.data. */
    return LoadSoundFromWave(wave);
}

void audio_init(AudioState *audio)
{
    ALCdevice *probe;
    int i;
    memset(audio, 0, sizeof(*audio));
    if (getenv("RECRAFT_NO_AUDIO")) return;
    probe = alcOpenDevice(NULL);
    if (!probe) return;
    alcCloseDevice(probe);
    InitAudioDevice();
    audio->ready = 1;
    for (i = 0; i < RECRAFT_SOUND_COUNT; ++i)
        audio->effects[i]=i==RECRAFT_SOUND_PORTAL ? assets_get_sound(ASSET_SOUND_PORTAL) : make_effect(i);
    if(audio->effects[RECRAFT_SOUND_PORTAL].source) SetSoundVolume(audio->effects[RECRAFT_SOUND_PORTAL],.5f);
}

void audio_play(AudioState *audio, RecraftSound effect)
{
    if (audio && audio->ready && effect >= 0 && effect < RECRAFT_SOUND_COUNT &&
        audio->effects[effect].source)
        PlaySound(audio->effects[effect]);
}

void audio_shutdown(AudioState *audio)
{
    int i;
    if (!audio || !audio->ready) return;
    for (i = 0; i < RECRAFT_SOUND_COUNT; ++i)
        if(i!=RECRAFT_SOUND_PORTAL && audio->effects[i].source) UnloadSound(audio->effects[i]);
    assets_release_sounds();
    CloseAudioDevice();
    memset(audio, 0, sizeof(*audio));
}
