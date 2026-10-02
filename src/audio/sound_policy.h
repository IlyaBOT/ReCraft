#ifndef RECRAFT_SOUND_POLICY_H
#define RECRAFT_SOUND_POLICY_H
#include <stdint.h>
typedef struct BetaBlockSound { const char *breaking,*step; float volume,pitch; } BetaBlockSound;
typedef struct MusicSchedule { uint64_t random; unsigned delay; } MusicSchedule;
const BetaBlockSound *beta_block_sound(unsigned block);
void music_schedule_init(MusicSchedule *schedule,uint64_t seed);
/* A controller tick: no countdown while music/records play or music is muted. */
int music_schedule_tick(MusicSchedule *schedule,int enabled,int playing,int record_playing);
unsigned music_schedule_random(MusicSchedule *schedule,unsigned bound);
#endif
