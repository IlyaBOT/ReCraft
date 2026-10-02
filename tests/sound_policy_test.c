#include "audio/sound_policy.h"
#include <assert.h>
#include <string.h>
int main(void)
{
    MusicSchedule schedule; unsigned initial,i;
    assert(!strcmp(beta_block_sound(3)->step,"step.gravel"));
    assert(!strcmp(beta_block_sound(5)->step,"step.wood"));
    assert(!strcmp(beta_block_sound(12)->breaking,"step.gravel"));
    assert(!strcmp(beta_block_sound(20)->breaking,"random.glass"));
    assert(!strcmp(beta_block_sound(20)->step,"step.stone"));
    assert(beta_block_sound(41)->pitch==1.5f);
    music_schedule_init(&schedule,123); initial=schedule.delay; assert(initial<12000);
    assert(!music_schedule_tick(&schedule,0,0,0) && schedule.delay==initial);
    assert(!music_schedule_tick(&schedule,1,1,0) && schedule.delay==initial);
    assert(!music_schedule_tick(&schedule,1,0,1) && schedule.delay==initial);
    for(i=0;i<initial;++i) assert(!music_schedule_tick(&schedule,1,0,0));
    assert(music_schedule_tick(&schedule,1,0,0) && schedule.delay>=12000 && schedule.delay<24000);
    initial=schedule.delay;
    for(i=0;i<500;++i) assert(!music_schedule_tick(&schedule,1,1,0));
    assert(schedule.delay==initial); return 0;
}
