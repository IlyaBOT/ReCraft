#ifndef RECRAFT_BETA_SESSION_H
#define RECRAFT_BETA_SESSION_H
#include <stdint.h>
/* Interactive play only. Discovery and read-only fixtures never acquire a
 * session. Beta uses an eight-byte big-endian timestamp, not an OS file lock. */
int beta_session_start(const char *world_path,int64_t *token);
int beta_session_check(const char *world_path,int64_t token);
#endif
