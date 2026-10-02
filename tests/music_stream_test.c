#include "audio/music_stream.h"
#include <AL/al.h>
#include <AL/alc.h>
#include <assert.h>
#include <stdio.h>
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
    /* Drive the OpenAL queue deterministically. Apple's framework ignores
     * ALSOFT_DRIVERS=null and a runner may have no advancing output device.
     * A stopped streaming source marks its queued buffers processed by the
     * OpenAL contract; update must drain/refill and release the final buffer. */
    for(i=0;i<300 && s.source;++i) {
        alSourceStop(s.source);
        music_stream_update(&s,.25f);
    }
    assert(!s.source && !s.decoder); /* Short track drains its last partial buffer and never loops. */
    assert(music_stream_open(&s,argv[2]));
    music_stream_update(&s,0); music_stream_close(&s);
    assert(!s.source && !s.decoder && alGetError()==AL_NO_ERROR);
    alcMakeContextCurrent(NULL); alcDestroyContext(context); alcCloseDevice(device);
    puts("Original Vorbis EOF, no-loop playback and bounded streaming passed."); return 0;
}
