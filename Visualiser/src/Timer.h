#ifndef TIMER_H_GUARD
#define TIMER_H_GUARD

#ifdef __cplusplus
extern "C" {
#endif

#if 1

#include <time.h>

typedef struct Timer
{
	struct timespec beg_;
} Timer;

Timer timer_create();
void timer_reset(Timer* timer);
double timer_elapsed(Timer* timer);

#else

#define Timer unsigned int
#define timer_create(...) 0
#define timer_reset(...)
#define timer_elapsed(...) 0.0

#endif

#ifdef __cplusplus
}
#endif

#endif /* TIMER_H_GUARD */