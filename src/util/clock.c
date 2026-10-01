#include "clock.h"
#include <limits.h>

#ifdef _WIN32
#include <windows.h>

double recraft_now_seconds(void)
{
    static LARGE_INTEGER frequency;
    LARGE_INTEGER counter;
    if (!frequency.QuadPart) QueryPerformanceFrequency(&frequency);
    QueryPerformanceCounter(&counter);
    return (double)counter.QuadPart / (double)frequency.QuadPart;
}
void recraft_sleep_seconds(double seconds)
{
    if (seconds > 0.0) Sleep((DWORD)(seconds * 1000.0 + 0.999));
}
#elif defined(__APPLE__)
#include <mach/mach_time.h>
#include <sys/select.h>

double recraft_now_seconds(void)
{
    static mach_timebase_info_data_t timebase;
    if (!timebase.denom) mach_timebase_info(&timebase);
    return (double)mach_absolute_time() * (double)timebase.numer /
           (double)timebase.denom * 0.000000001;
}
void recraft_sleep_seconds(double seconds)
{
    struct timeval delay;
    if (seconds <= 0.0) return;
    delay.tv_sec = (long)seconds;
    delay.tv_usec = (long)((seconds - delay.tv_sec) * 1000000.0);
    select(0, 0, 0, 0, &delay);
}
#else
#include <sys/time.h>
#include <sys/select.h>

double recraft_now_seconds(void)
{
    struct timeval now;
    gettimeofday(&now, 0);
    return (double)now.tv_sec + (double)now.tv_usec * 0.000001;
}
void recraft_sleep_seconds(double seconds)
{
    struct timeval delay;
    if (seconds <= 0.0) return;
    delay.tv_sec = (long)seconds;
    delay.tv_usec = (long)((seconds - delay.tv_sec) * 1000000.0);
    select(0, 0, 0, 0, &delay);
}
#endif
