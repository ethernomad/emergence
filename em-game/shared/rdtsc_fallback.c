#if !defined(__i386__) && !defined(__x86_64__)

#define _GNU_SOURCE
#define _REENTRANT

#include <stdint.h>
#include <time.h>

uint64_t rdtsc(void)
{
	struct timespec ts;
	clock_gettime(CLOCK_MONOTONIC, &ts);
	return (uint64_t)ts.tv_sec * 1000000000ULL + (uint64_t)ts.tv_nsec;
}

#endif
