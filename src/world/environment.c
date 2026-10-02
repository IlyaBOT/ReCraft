#include "environment.h"
#include "climate.h"
#include "fluid.h"
#include <math.h>
#include <stdlib.h>
#define PI_F 3.14159265358979323846f
static float clamp(float v) { return v<0 ? 0 : v>1 ? 1 : v; }
void world_precipitation_prepare(World *w,Chunk *c)
{
    int x,z,y;
    if(!c->climate_ready) {
        if(!w->climate) {
            w->climate=(BetaClimate *)malloc(sizeof(BetaClimate));
            if(!w->climate) return;
            beta_climate_init(w->climate,w->seed);
        }
        for(z=0;z<16;++z) for(x=0;x<16;++x) {
            double temperature,humidity;
            beta_climate_sample(w->climate,c->x*16+x,c->z*16+z,&temperature,&humidity);
            c->precipitation[x+z*16]=(uint8_t)beta_climate_precipitation(temperature,humidity);
        }
        c->climate_ready=1;
    }
    if(c->precipitation_revision==c->revision) return;
    for(z=0;z<16;++z) for(x=0;x<16;++x) {
        for(y=127;y>0;--y) {
            unsigned id=chunk_get_block(c,x,y,z);
            if(beta_material_solid(id) || fluid_kind(id)) break;
        }
        c->precipitation_height[x+z*16]=(uint8_t)(y+1);
    }
    c->precipitation_revision=c->revision;
}
int world_precipitation_height(World *w,int x,int z,unsigned *kind)
{
    int cx=x>=0 ? x/16 : (int)(-((-(int64_t)x+15)/16)),cz=z>=0 ? z/16 : (int)(-((-(int64_t)z+15)/16));
    Chunk *c=world_peek_chunk(w,cx,cz);
    if(kind) *kind=0;
    if(!c || (w->beta_format && !c->beta_raw)) return 128;
    world_precipitation_prepare(w,c);
    if(kind) *kind=c->precipitation[(x-cx*16)+(z-cz*16)*16];
    return c->precipitation_height[(x-cx*16)+(z-cz*16)*16];
}
unsigned world_random(World *w,unsigned bound)
{
    unsigned bits,value;
    if(!bound) return 0;
    do {
        w->random_seed=(w->random_seed*UINT64_C(0x5deece66d)+11)&UINT64_C(0xffffffffffff);
        bits=(unsigned)(w->random_seed>>17);
        if(!(bound&(bound-1))) return (unsigned)(((uint64_t)bound*bits)>>31);
        value=bits%bound;
    } while((uint64_t)bits-value+bound-1>INT32_MAX);
    return value;
}
float world_celestial_angle(const World *w,float partial)
{
    int64_t time=w->beta_world_time%24000;
    float a,original;
    if(time<0) time+=24000;
    a=((float)time+partial)/24000-.25f;
    if(a<0) a+=1;
    if(a>1) a-=1;
    original=a; a=1-(cosf(a*PI_F)+1)*.5f;
    return original+(a-original)/3;
}
float world_daylight(const World *w,float partial)
{ return clamp(cosf(world_celestial_angle(w,partial)*PI_F*2)*2+.5f); }
void world_environment_refresh(World *w)
{
    float light=world_daylight(w,1);
    int value; size_t i;
    light*=1-w->rain_strength*5/16;
    light*=1-w->thunder_strength*w->rain_strength*5/16;
    value=(int)((1-light)*11);
    if(value==w->sky_subtracted) return;
    w->sky_subtracted=value;
    /* Skylight stored in chunks remains the daytime value. Only the baked
     * mesh changes at a light level transition, never on every frame. */
    for(i=0;i<w->cache_count;++i) w->cache[i]->dirty_flags|=CHUNK_DIRTY_MESH;
}
int world_is_daytime(const World *w) { return w->sky_subtracted<4; }
void world_environment_tick(World *w)
{
    if(w->beta_world_time==INT64_MAX) w->beta_world_time%=24000;
    ++w->beta_world_time;
    if(!w->network_mode) {
        if(w->thunder_time<=0) w->thunder_time=(int)world_random(w,w->thundering ? 12000 : 168000)+(w->thundering ? 3600 : 12000);
        else if(--w->thunder_time==0) w->thundering=!w->thundering;
        if(w->rain_time<=0) w->rain_time=(int)world_random(w,w->raining ? 12000 : 168000)+12000;
        else if(--w->rain_time==0) w->raining=!w->raining;
    }
    w->rain_strength=clamp(w->rain_strength+(w->raining ? .01f : -.01f));
    w->thunder_strength=clamp(w->thunder_strength+(w->thundering ? .01f : -.01f));
    world_environment_refresh(w);
    if(!w->network_mode) {
        size_t i;
        for(i=0;i<w->cache_count;++i) {
            Chunk *c=w->cache[i];
            if(world_random(w,16)==0 && (!w->beta_format || c->beta_raw)) {
                unsigned kind; int x,z,y; unsigned below;
                w->random_tick=w->random_tick*3u+UINT32_C(1013904223);
                x=c->x*16+((w->random_tick>>2)&15); z=c->z*16+((w->random_tick>>10)&15);
                y=world_precipitation_height(w,x,z,&kind);
                if(kind!=2 || y>=128 || chunk_get_block_light(c,x-c->x*16,y,z-c->z*16)>=10) continue;
                below=world_peek_block(w,x,y-1,z);
                if(below==9 && world_peek_metadata(w,x,y-1,z)==0) world_set_block(w,x,y-1,z,79);
                if(w->raining && !world_peek_block(w,x,y,z) && below!=79 && beta_material_solid(below)) world_set_block(w,x,y,z,78);
            }
        }
    }
}
void world_environment_dawn(World *w)
{
    int64_t skipped;
    if(w->network_mode) return;
    skipped=24000-w->beta_world_time%24000;
    if(w->beta_world_time>INT64_MAX-skipped) w->beta_world_time%=24000;
    w->beta_world_time+=skipped;
    w->tick+=(uint64_t)skipped; /* Scheduled ticks become due at the skipped dawn. */
    w->raining=w->thundering=0; w->rain_time=w->thunder_time=0;
    world_environment_refresh(w);
}
