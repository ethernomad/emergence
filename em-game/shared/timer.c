/**
 * @file
 * @copyright 1998-2004 Jonathan Brown <jbrown@bluedroplet.com>
 * @license https://www.gnu.org/licenses/gpl-3.0.html
 * @homepage https://github.com/bluedroplet/emergence
 */

#define _GNU_SOURCE
#define _REENTRANT

#include <stdio.h>
#include <stdint.h>
#include <pthread.h>
#include <signal.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <math.h>

#include <sys/time.h>

#include "types.h"
#include "llist.h"
#include "rdtsc.h"

#include "console.h"



uint64_t start_count, counts_per_second;


uint64_t timestamp()
{
	return rdtsc() - start_count;
}


uint32_t get_tick_from_wall_time()
{
	return (uint32_t)lround(((double)(rdtsc() - start_count) * 200.0) / 
		(double)counts_per_second);
}


double get_wall_time()
{
	return (double)(rdtsc() - start_count) / (double)counts_per_second;
}


void init_timer()
{
	struct timespec ts;
	clock_gettime(CLOCK_MONOTONIC, &ts);
	counts_per_second = 1000000000ULL;

	console_print("Timer resolution: %u Hz\n", (uint32_t)counts_per_second);

	start_count = rdtsc();
}
