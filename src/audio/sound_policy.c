#include "sound_policy.h"
static const BetaBlockSound blocks[97]={
#include "beta_block_sounds.def"
};
const BetaBlockSound *beta_block_sound(unsigned block)
{ return block<97 && blocks[block].step ? &blocks[block] : &blocks[1]; }
unsigned music_schedule_random(MusicSchedule *s,unsigned bound)
{
    unsigned bits,value;
    if(!bound) return 0;
    do {
        s->random=(s->random*UINT64_C(0x5deece66d)+11)&UINT64_C(0xffffffffffff);
        bits=(unsigned)(s->random>>17);
        if(!(bound&(bound-1))) return (unsigned)(((uint64_t)bound*bits)>>31);
        value=bits%bound;
    } while((uint64_t)bits-value+bound-1>INT32_MAX);
    return value;
}
void music_schedule_init(MusicSchedule *s,uint64_t seed)
{ s->random=(seed^UINT64_C(0x5deece66d))&UINT64_C(0xffffffffffff); s->delay=music_schedule_random(s,12000); }
int music_schedule_tick(MusicSchedule *s,int enabled,int playing,int record)
{
    if(!enabled || playing || record) return 0;
    if(s->delay) { --s->delay; return 0; }
    s->delay=12000+music_schedule_random(s,12000); return 1;
}
