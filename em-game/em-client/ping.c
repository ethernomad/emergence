/**
 * @file
 * @copyright 1998-2004 Jonathan Brown <jbrown@bluedroplet.com>
 * @license https://www.gnu.org/licenses/gpl-3.0.html
 * @homepage https://github.com/bluedroplet/emergence
 */

#ifdef LINUX
#define _GNU_SOURCE
#define _REENTRANT
#endif

#include <stdint.h>
#include <stdlib.h>
#include <memory.h>

#include "types.h"
#include "stringbuf.h"
#include "buffer.h"
#include "llist.h"
#include "network.h"
#include "cvar.h"
#include "ping.h"
#include "game.h"
#include "console.h"
#include "tick.h"
#include "timer.h"

struct ping_t
{
	uint32_t index;
	double time;
	
	struct ping_t *next;
};

struct ping_t *ping0 = NULL;


int weights[] = {-1, -3, -5, -5, -2, 6, 18, 33, 47, 57, 60};


double latencies[11];
int clatency = 0;
int nlatencies = 0;

float latency;

void add_latency(double new_latency)
{
	if(!nlatencies)
	{
		latencies[0] = new_latency;
		nlatencies = 1;
	}
	else
	{
		clatency++;
		if(clatency == 11)
			clatency = 0;
		
		if(nlatencies != 11)
			nlatencies++;
		
		latencies[clatency] = new_latency;
	}
	
	int cweight = 10;
	int pos = clatency;
	int total_weight = 0;
	latency = 0.0;
	
	int left = nlatencies;
	
	do
	{
		latency += weights[cweight] * latencies[pos];
		
		if(pos == 0)
			pos = 10;
		else
			pos--;
		
		total_weight += weights[cweight];
		
		cweight--;
		
	} while(--left);
	
	latency /= total_weight;
}



void init_ping()
{
	create_cvar_float("latency", &latency, 0);
}
