#include "audio.h"
#include "../assets/assets.h"
#include "../world/environment.h"
#include "../world/fluid.h"
#include "AL/alc.h"
#include "AL/al.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
void audio_init(AudioState *a)
{
    ALCdevice *probe;
    memset(a,0,sizeof(*a));
    if(getenv("RECRAFT_NO_AUDIO")) return;
    probe=alcOpenDevice(NULL); if(!probe) return; alcCloseDevice(probe);
    InitAudioDevice();
    if(!alcGetCurrentContext()) return;
    a->ready=1; a->music_volume=a->sound_volume=1; a->next_step=1;
    music_schedule_init(&a->schedule,(uint64_t)time(NULL));
    music_schedule_init(&a->effects_random,(uint64_t)time(NULL)^UINT64_C(0x27182818));
    alGenSources(16,a->voices); alDistanceModel(AL_LINEAR_DISTANCE_CLAMPED);
}
void audio_named(AudioState *a,const char *key,float volume,float pitch,int spatial,float x,float y,float z)
{
    Sound sample; AssetSoundId id; unsigned i,voice; int state;
    if(!a || !a->ready || volume<=0 || a->sound_volume<=0) return;
    id=assets_find_sound(key,music_schedule_random(&a->effects_random,UINT32_C(0x10000)));
    sample=assets_get_sound(id); if(!sample.buffer) return;
    voice=a->voice_next++%16;
    for(i=0;i<16;++i) {
        unsigned index=(voice+i)%16;
        alGetSourcei(a->voices[index],AL_SOURCE_STATE,&state);
        if(state!=AL_PLAYING) { voice=index; break; }
    }
    voice=a->voices[voice]; alSourceStop(voice);
    alSourcei(voice,AL_BUFFER,sample.buffer); alSourcei(voice,AL_LOOPING,AL_FALSE);
    alSourcei(voice,AL_SOURCE_RELATIVE,spatial ? AL_FALSE : AL_TRUE);
    alSource3f(voice,AL_POSITION,spatial ? x : 0,spatial ? y : 0,spatial ? z : 0);
    alSourcef(voice,AL_REFERENCE_DISTANCE,0); alSourcef(voice,AL_MAX_DISTANCE,16*(volume>1 ? volume : 1));
    alSourcef(voice,AL_ROLLOFF_FACTOR,spatial ? 1 : 0);
    alSourcef(voice,AL_GAIN,(volume>1 ? 1 : volume)*a->sound_volume);
    alSourcef(voice,AL_PITCH,pitch); alSourcePlay(voice);
}
void audio_block(AudioState *a,unsigned block,int action,float x,float y,float z)
{
    const BetaBlockSound *s=beta_block_sound(block);
    float volume=action==0 ? s->volume*.15f : action==1 ? (s->volume+1)/8 : (s->volume+1)/2;
    float pitch=s->pitch*(action==0 ? 1 : action==1 ? .5f : .8f);
    audio_named(a,action==0 || action==3 ? s->step : s->breaking,volume,pitch,1,x,y,z);
}
void audio_play(AudioState *a,RecraftSound effect)
{
    if(effect==RECRAFT_SOUND_CLICK) audio_named(a,"random.click",.25f,1,0,0,0,0);
    else if(effect==RECRAFT_SOUND_PORTAL) audio_named(a,"portal.portal",.5f,1,0,0,0,0);
}
void audio_listener(AudioState *a,float x,float y,float z,float yaw)
{
    float orientation[6]={sinf(yaw),0,-cosf(yaw),0,1,0};
    if(!a || !a->ready) return;
    alListener3f(AL_POSITION,x,y,z); alListenerfv(AL_ORIENTATION,orientation);
}
void audio_weather_tick(AudioState *a,World *w,float x,float y,float z,int fancy)
{
    MusicSchedule random={0}; float rain=w->rain_strength*(fancy ? 1 : .5f),sx=0,sy=0,sz=0;
    int i,samples=0,px=(int)floorf(x),py=(int)floorf(y),pz=(int)floorf(z);
    ++a->weather_ticks;
    if(!a->ready || rain<=0 || a->sound_volume<=0) return;
    /* EntityRenderer.addRainParticles uses its own per-tick Java Random. The
     * reservoir and roof test select audible rain without altering world RNG. */
    random.random=((uint64_t)a->weather_ticks*312987231^UINT64_C(0x5deece66d))&UINT64_C(0xffffffffffff);
    for(i=0;i<(int)(100*rain*rain);++i) {
        unsigned kind; int xx=px+(int)music_schedule_random(&random,10)-(int)music_schedule_random(&random,10);
        int zz=pz+(int)music_schedule_random(&random,10)-(int)music_schedule_random(&random,10);
        int roof=world_precipitation_height(w,xx,zz,&kind);
        unsigned block=world_peek_block(w,xx,roof-1,zz);
        if(kind==1 && roof<=py+10 && roof>=py-10 && block) {
            float ox=(float)music_schedule_random(&random,16777216)/16777216;
            float oz=(float)music_schedule_random(&random,16777216)/16777216;
            if(fluid_kind(block)!=2 && music_schedule_random(&random,(unsigned)++samples)==0) {
                sx=xx+ox; sy=roof+.1f; sz=zz+oz;
            }
        }
    }
    if(samples && (int)music_schedule_random(&random,3)<a->rain_sound_counter++) {
        int sheltered=sy>y+1 && world_precipitation_height(w,px,pz,NULL)>py;
        a->rain_sound_counter=0;
        audio_named(a,"ambient.weather.rain",sheltered ? .1f : .2f,sheltered ? .5f : 1,1,sx,sy,sz);
    }
}
void audio_ambient_tick(AudioState *a,World *w,float x,float y,float z)
{
    MusicSchedule *r=&a->effects_random; int i,px=(int)floorf(x),py=(int)floorf(y),pz=(int)floorf(z);
    if(!a->ready || a->sound_volume<=0) return;
    /* World.randomDisplayUpdates samples a triangular 16-block neighbourhood.
     * Read loaded cells only; client ambience never creates or edits chunks. */
    for(i=0;i<1000;++i) {
        int xx=px+(int)music_schedule_random(r,16)-(int)music_schedule_random(r,16);
        int yy=py+(int)music_schedule_random(r,16)-(int)music_schedule_random(r,16);
        int zz=pz+(int)music_schedule_random(r,16)-(int)music_schedule_random(r,16);
        unsigned id=world_peek_block(w,xx,yy,zz),meta;
        const char *key=NULL; float volume=1,pitch=1;
        if(id==90 && !music_schedule_random(r,100)) {
            key="portal.portal"; pitch=.8f+(float)music_schedule_random(r,16777216)/16777216*.4f;
        } else if((id==8 || id==9) && !music_schedule_random(r,64)) {
            meta=world_peek_metadata(w,xx,yy,zz);
            if(meta>0 && meta<8) {
                key="liquid.water";
                volume=.75f+(float)music_schedule_random(r,16777216)/16777216*.25f;
                pitch=.5f+(float)music_schedule_random(r,16777216)/16777216;
            }
        } else if(id==51 && !music_schedule_random(r,24)) {
            key="fire.fire"; volume=1+(float)music_schedule_random(r,16777216)/16777216;
            pitch=.3f+(float)music_schedule_random(r,16777216)/16777216*.7f;
        }
        if(key) audio_named(a,key,volume,pitch,1,xx+.5f,yy+.5f,zz+.5f);
    }
}
void audio_update(AudioState *a,double elapsed,int active,int music_volume,int sound_volume)
{
    char path[768];
    if(!a || !a->ready) return;
    a->music_volume=music_volume*.01f; a->sound_volume=sound_volume*.01f;
    if(sound_volume==0) { unsigned i; for(i=0;i<16;++i) alSourceStop(a->voices[i]); }
    if(a->music.source && music_volume==0) music_stream_close(&a->music);
    music_stream_update(&a->music,a->music_volume);
    if(!active) { a->controller_time=0; return; }
    a->controller_time+=elapsed;
    while(a->controller_time>=.05) {
        a->controller_time-=.05;
        if(music_schedule_tick(&a->schedule,music_volume>0,a->music.source!=0,0)) {
            unsigned track=music_schedule_random(&a->schedule,assets_music_count());
            if(assets_music_path(track,path,sizeof(path)) && !music_stream_open(&a->music,path))
                fprintf(stderr,"Missing or invalid optional music: %s\n",path);
            music_stream_update(&a->music,a->music_volume);
        }
    }
}
void audio_shutdown(AudioState *a)
{
    if(!a || !a->ready) return;
    music_stream_close(&a->music); alDeleteSources(16,a->voices);
    assets_release_sounds(); CloseAudioDevice(); memset(a,0,sizeof(*a));
}
