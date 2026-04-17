/**
 * @file
 * @copyright 1998-2004 Jonathan Brown <jbrown@bluedroplet.com>
 * @license https://www.gnu.org/licenses/gpl-3.0.html
 * @homepage https://github.com/bluedroplet/emergence
 */

#ifdef LINUX
#define _GNU_SOURCE
#ifndef _REENTRANT
#define _REENTRANT
#endif
#endif

#include <stdint.h>
#include <math.h>

#include "gsub.h"
#include "nodes.h"
#include "interface.h"
#include "main.h"

int grid_granularity = 0;
double grid_spacing = 20.0;

void draw_grid()
{
	int startx, starty, endx, endy;

	double this_grid_spacing = exp2(floor(log(1 / zoom) / log(2))) * 
		grid_spacing * exp2(grid_granularity);

	float world_x, world_y;

	screen_to_world(0, vid_height - 1, &world_x, &world_y);

	startx = (int)(world_x / this_grid_spacing);
	starty = (int)(world_y / this_grid_spacing);

	screen_to_world(vid_width - 1, 0, &world_x, &world_y);

	endx = (int)(world_x / this_grid_spacing) + 1;
	endy = (int)(world_y / this_grid_spacing) + 1;

	
	struct blit_params_t params;
		
	params.red = params.green = params.blue = 0xff;
	params.dest = s_backbuffer;
	
	int x, y;

	for(x = startx; x != endx; x++)
		for(y = starty; y != endy; y++)
		{
			world_to_screen(x * this_grid_spacing, y * this_grid_spacing, 
				&params.dest_x, &params.dest_y);
			plot_pixel(&params);
		}
}


void snap_to_grid(float inx, float iny, float *outx, float *outy)
{
	double this_grid_spacing = exp2(floor(log(1 / zoom) / log(2))) * 
		grid_spacing * exp2(grid_granularity);
	
	if(view_state & VIEW_GRID)
	{
		*outx = (round(inx / this_grid_spacing)) * this_grid_spacing;
		*outy = (round(iny / this_grid_spacing)) * this_grid_spacing;
	}
	else
	{
		*outx = inx;
		*outy = iny;
	}
}

void decrease_grid_granularity()
{
	if(grid_granularity > -3)
		grid_granularity--;
}

void increase_grid_granularity()
{
	if(grid_granularity < 40000)
		grid_granularity++;
}
