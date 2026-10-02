#include "audio/audio.h"
#include "assets/assets.h"
#include <AL/al.h>
#include <AL/alc.h>
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
int main(int argc,char **argv)
{
    ALCdevice *device=alcOpenDevice(NULL); ALCcontext *context; AudioState a={0};
    float gain,position[3]; int relative,loop; char path[512];
    if(!device) return 77;
    context=alcCreateContext(device,NULL); assert(context && alcMakeContextCurrent(context) && argc==2);
    assets_init(argv[1]); a.ready=1; a.sound_volume=a.music_volume=1;
    alGenSources(16,a.voices); alDistanceModel(AL_LINEAR_DISTANCE_CLAMPED);
    assert(assets_music_path(0,path,sizeof(path)) && music_stream_open(&a.music,path));
    audio_named(&a,"records.13",1,1,1,3,64,8);
    assert(a.record.source && a.record.decoder && !a.music.source);
    alGetSourcef(a.record.source,AL_GAIN,&gain); alGetSourcefv(a.record.source,AL_POSITION,position);
    alGetSourcei(a.record.source,AL_SOURCE_RELATIVE,&relative); alGetSourcei(a.record.source,AL_LOOPING,&loop);
    assert(fabsf(gain-.5f)<.001f && relative==AL_FALSE && loop==AL_FALSE && position[0]==3 && position[1]==64);
    a.schedule.delay=0; audio_update(&a,.05,1,100,100); assert(a.record.source && !a.music.source && a.schedule.delay==0);
    audio_named(&a,"records.cat",1,1,1,10,64,8); assert(a.record.source && !a.music.source);
    alGetSourcefv(a.record.source,AL_POSITION,position); assert(position[0]==10);
    audio_named(&a,"records.stop",1,1,1,3,64,8); assert(!a.record.source && !a.record.decoder);
    audio_named(&a,"records.13",1,1,1,3,64,8); audio_update(&a,0,1,100,0); assert(!a.record.source);
    alDeleteSources(16,a.voices); assert(alGetError()==AL_NO_ERROR); assets_shutdown();
    alcMakeContextCurrent(NULL); alcDestroyContext(context); alcCloseDevice(device);
    puts("Beta discs: positional non-looping stream, replacement/ejection, half gain and music suspension passed");
    return 0;
}
