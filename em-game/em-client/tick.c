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
#include <math.h>

#include "cvar.h"
#include "ping.h"
#include "timer.h"
#define TABLE_SIZE 100

int next_table_entry = 0;
int table_entries_used = 0;

float tracking = 0.5;
float time_shift = 0.0;

double tick_a, tick_b;

int different_ticks = 0;

struct
{
	uint64_t tsc;
	uint32_t game_tick;
	
	double tsc_squared;
	double game_tick_squared;
	
} tick_times[TABLE_SIZE];


void add_game_tick(uint32_t game_tick, uint64_t *tsc)
{
	tick_times[next_table_entry].tsc = *tsc;
	tick_times[next_table_entry].game_tick = game_tick;
	
	tick_times[next_table_entry].tsc_squared = (double)*tsc * (double)*tsc;
	tick_times[next_table_entry].game_tick_squared = (double)game_tick * (double)game_tick;
	
	if(next_table_entry == TABLE_SIZE - 1)
		next_table_entry = 0;
	else
		next_table_entry++;

	if(table_entries_used < TABLE_SIZE)
		table_entries_used++;
}


void update_tick_parameters()
{
	if(table_entries_used == 0)
		return;
	
	if(table_entries_used == 1)
		return;
	
	int e;
	
	if(!different_ticks)
	{
		for(e = 1; e < table_entries_used; e++)
		{
			if(tick_times[e].game_tick != tick_times[0].game_tick)
			{
				different_ticks = 1;
				break;
			}
		}
	}
		
	if(!different_ticks)
		return;

	double game_tick_squared_total = 0.0;
	double tsc_squared_total = 0.0;
	double game_tick_mean = 0.0;
	double tsc_mean = 0.0;
	double tsc_times_game_tick_total = 0.0;
	
	for(e = 0; e < table_entries_used; e++)
	{
		game_tick_squared_total += tick_times[e].game_tick_squared;
		tsc_squared_total += tick_times[e].tsc_squared;
		game_tick_mean += (double)tick_times[e].game_tick;
		tsc_mean += (double)tick_times[e].tsc;
		tsc_times_game_tick_total += (double)tick_times[e].tsc * (double)tick_times[e].game_tick;
	}
	
	game_tick_mean /= (double)table_entries_used;
	tsc_mean /= (double)table_entries_used;
	
	double bigb = ((game_tick_squared_total - 
		table_entries_used * game_tick_mean * game_tick_mean) - 
		(tsc_squared_total - table_entries_used * tsc_mean * tsc_mean)) /
		(2.0 * (table_entries_used * game_tick_mean * tsc_mean - tsc_times_game_tick_total));
	
	tick_b = -bigb + sqrt(bigb * bigb + 1);
	tick_a = game_tick_mean - tick_b * tsc_mean;
}


uint32_t get_game_tick()
{
	if(table_entries_used == 0)
		return 0;
	
	if(table_entries_used == 1)
		return tick_times[0].game_tick;
	
	if(!different_ticks)
		return tick_times[0].game_tick;
	
	return lround(tick_a + tick_b * ((double)timestamp() - (latency * (tracking - 1.0) - time_shift) * 
		(double)counts_per_second));
}


double get_time_from_game_tick(float tick)
{
	if(table_entries_used == 0)
		return 0.0;
	
	if(table_entries_used == 1)
		return (double)tick_times[0].tsc / (double)counts_per_second;
	
	if(!different_ticks)
		return (double)tick_times[0].tsc / (double)counts_per_second;
	
	return ((tick - tick_a) / tick_b) / (double)counts_per_second + 
		(latency * (tracking - 1.0) - time_shift);
}


void init_tick_cvars()
{
	create_cvar_float("tracking", &tracking, 0);
	create_cvar_float("time_shift", &time_shift, 0);
}


void clear_ticks()
{
	next_table_entry = 0;
	table_entries_used = 0;
	different_ticks = 0;
}
