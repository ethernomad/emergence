#ifndef RDTSC_H
#define RDTSC_H

#include <stdint.h>

#if defined(__i386__) || defined(__x86_64__)

static inline uint64_t rdtsc(void)
{
	unsigned int lo, hi;
	__asm__ __volatile__ ("rdtsc" : "=a" (lo), "=d" (hi));
	return ((uint64_t)hi << 32) | lo;
}

#else

uint64_t rdtsc(void);

#endif

#endif
