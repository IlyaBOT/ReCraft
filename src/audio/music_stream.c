#include "music_stream.h"
#include "stb_vorbis.h"
#include "AL/al.h"
#include <string.h>
static int refill(MusicStream *s,unsigned buffer)
{
    short pcm[8192]; int frames=stb_vorbis_get_samples_short_interleaved((stb_vorbis *)s->decoder,s->channels,pcm,8192);
    if(!frames) { s->eof=1; return 0; }
    alBufferData(buffer,s->channels==2 ? AL_FORMAT_STEREO16 : AL_FORMAT_MONO16,pcm,
        frames*s->channels*(int)sizeof(short),s->rate);
    return alGetError()==AL_NO_ERROR;
}
void music_stream_close(MusicStream *s)
{
    int i;
    if(s->source) { alSourceStop(s->source); alDeleteSources(1,&s->source); }
    for(i=0;i<4;++i) if(s->buffers[i]) alDeleteBuffers(1,&s->buffers[i]);
    if(s->decoder) stb_vorbis_close((stb_vorbis *)s->decoder);
    memset(s,0,sizeof(*s));
}
int music_stream_open(MusicStream *s,const char *path)
{
    stb_vorbis_info info; int error=0,i,queued;
    music_stream_close(s); if(!path) return 0;
    while(alGetError()!=AL_NO_ERROR) { }
    s->decoder=stb_vorbis_open_filename(path,&error,NULL);
    if(!s->decoder) return 0;
    info=stb_vorbis_get_info((stb_vorbis *)s->decoder);
    if(info.channels<1 || info.channels>2) { music_stream_close(s); return 0; }
    s->channels=info.channels; s->rate=info.sample_rate;
    alGenSources(1,&s->source);
    if(alGetError()!=AL_NO_ERROR) { music_stream_close(s); return 0; }
    alGenBuffers(4,s->buffers);
    if(alGetError()!=AL_NO_ERROR) { music_stream_close(s); return 0; }
    alSourcei(s->source,AL_SOURCE_RELATIVE,AL_TRUE); alSourcei(s->source,AL_LOOPING,AL_FALSE);
    for(i=0;i<4 && !s->eof;++i) if(refill(s,s->buffers[i])) alSourceQueueBuffers(s->source,1,&s->buffers[i]);
    alGetSourcei(s->source,AL_BUFFERS_QUEUED,&queued);
    if(!queued || alGetError()!=AL_NO_ERROR) { music_stream_close(s); return 0; }
    alSourcePlay(s->source); return 1;
}
void music_stream_update(MusicStream *s,float volume)
{
    int processed,queued,state;
    if(!s->source) return;
    alSourcef(s->source,AL_GAIN,volume);
    alGetSourcei(s->source,AL_BUFFERS_PROCESSED,&processed);
    while(processed-- >0) {
        unsigned buffer; alSourceUnqueueBuffers(s->source,1,&buffer);
        if(!s->eof && refill(s,buffer)) alSourceQueueBuffers(s->source,1,&buffer);
    }
    alGetSourcei(s->source,AL_BUFFERS_QUEUED,&queued);
    if(!queued) { music_stream_close(s); return; }
    alGetSourcei(s->source,AL_SOURCE_STATE,&state);
    if(state!=AL_PLAYING) alSourcePlay(s->source);
}
