#ifdef LINUX
#define _GNU_SOURCE
#define _REENTRANT
#endif

#include <stdint.h>
#include <math.h>

#include "../common/types.h"
#include "../gsub/gsub.h"
#include "render.h"
#include "game.h"

void draw_world_clipped_line(double x1, double y1, double x2, double y2)
{
	double temp;
	
	if(y1 > y2)
	{
		temp = y1;
		y1 = y2;
		y2 = temp;
		
		temp = x1;
		x1 = x2;
		x2 = temp;
	}
	
	double zoom = ((double)(vid_width) / 1600.0);
	
	double left = (-vid_width / 2 + 0.5) / zoom + viewx;
	double bottom = (-vid_height / 2 + 0.5) / zoom + viewy;
	
	double right = (vid_width / 2 - 0.5) / zoom + viewx;
	double top = (vid_height / 2 - 0.5) / zoom + viewy;

	if((y2 < bottom) || (y1 > top))
		return;
	
	if(y1 < bottom)
	{
		x1 = x2 - (x2 - x1) * (y2 - bottom) / (y2 - y1);
		y1 = bottom;
	}
	
	if(y2 > top)
	{
		x2 = x1 + (x2 - x1) * (top - y1) / (y2 - y1);
		y2 = top;
	}
	
	if(x1 < x2)
	{
		if((x2 < left) || (x1 > right))
			return;
		
		if(x1 < left)
		{
			y1 = y2 - (y2 - y1) * (x2 - left) / (x2 - x1);
			x1 = left;
		}
		
		if(x2 > right)
		{
			y2 = y1 + (y2 - y1) * (right - x1) / (x2 - x1);
			x2 = right;
		}
	}
	else
	{
		if((x1 < left) || (x2 > right))
			return;
		
		if(x2 < left)
		{
			y2 = y1 - (y1 - y2) * (x1 - left) / (x1 - x2);
			x2 = left;
		}
		
		if(x1 > right)
		{
			y1 = y2 + (y1 - y2) * (right - x2) / (x1 - x2);
			x1 = right;
		}
	}

	int sx1, sy1, sx2, sy2;
	
	world_to_screen(x1, y1, &sx1, &sy1);
	world_to_screen(x2, y2, &sx2, &sy2);
	
	draw_line(sx1, sy1, sx2, sy2);
}
