#include "audio/music_stream.h"
#include <AL/al.h>
#include <AL/alc.h>
#include <assert.h>
#include <stdio.h>
#ifdef _WIN32
#include <windows.h>
#else
#include <unistd.h>
#endif
int main(int argc,char **argv)
{
    MusicStream s={0}; int i,loop; ALCdevice *device=alcOpenDevice(NULL);
    ALCcontext *context;
    if(!device) { puts("No OpenAL device available; streaming test skipped."); return 77; }
    context=alcCreateContext(device,NULL); assert(context && alcMakeContextCurrent(context));
    assert(argc==3);
    assert(!music_stream_open(&s,"missing-file.ogg") && !s.source && !s.decoder);
    assert(music_stream_open(&s,argv[1]));
    alGetSourcei(s.source,AL_LOOPING,&loop); assert(loop==AL_FALSE);
    alSourcef(s.source,AL_PITCH,4);
    for(i=0;i<300 && s.source;++i) {
        music_stream_update(&s,.25f);
#ifdef _WIN32
        Sleep(10);
#else
        usleep(10000);
#endif
    }
    assert(!s.source && !s.decoder); /* Short track drains its last partial buffer and never loops. */
    assert(music_stream_open(&s,argv[2]));
    music_stream_update(&s,0); music_stream_close(&s);
    assert(!s.source && !s.decoder && alGetError()==AL_NO_ERROR);
    alcMakeContextCurrent(NULL); alcDestroyContext(context); alcCloseDevice(device);
    puts("Original Vorbis EOF, no-loop playback and bounded streaming passed."); return 0;
}
