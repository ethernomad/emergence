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

#include <stdlib.h>
#include <stdint.h>
#include <math.h>

#include "types.h"
#include "cvar.h"
#include "gsub.h"
#include "game.h"
#include "render.h"

float star_zs = 1.0;
float star_max_depth = 1.0;

struct star_t
{
	float x, y, z;
	uint8_t alpha;

} *stars = NULL;

int numstars = 0;


/*
int qc_star_density(float density)
{
	numstars = (int)floor(((float)(mapwidth * mapheight * 32 * 32) * density));

	m_free(stars);

	if(!numstars)
	{
		return 1;
	}

	stars = (star_t*)m_alloc(numstars * sizeof(star_t));

	for(int i = 0; i != numstars; i++)
	{
		stars[i].z = rand_float() * star_zs * 2;
		stars[i].x = rand_float() * mapwidth * 32;
		stars[i].y = rand_float() * mapheight * 32;

		stars[i].colour = vid_graylookup[lround((rand_float() * 255.0f))];
	}

	return 1;
}


void init_stars()
{
	qc_StarDensity(get_cvar_float("r_StarDensity"));

	SetCvarQCFunction("r_StarDensity", qc_StarDensity);
}
*/

void init_stars()
{
	numstars = 120;
	
	stars = malloc(numstars * sizeof(struct star_t));

	double star_width = (1600.0 * (star_zs + star_max_depth)) / star_zs;
	double star_height = (1600.0 * (star_zs + star_max_depth)) / star_zs;
	
	int s;
	for(s = 0; s < numstars; s++)
	{
		stars[s].z = drand48() * star_max_depth + star_zs;
		stars[s].x = drand48() * star_width;
		stars[s].y = drand48() * star_height;

		stars[s].alpha = lround(drand48() * 255.0f);
	}
}


void render_stars()
{
	if(!numstars)
		return;

	int s;
	
	struct blit_params_t params;
		
	params.dest = s_backbuffer;
	
	double minx = (-800.0 * (star_zs + star_max_depth)) / star_zs + viewx;
	double maxx = (800.0 * (star_zs + star_max_depth)) / star_zs + viewx;
	double miny = (-600.0 * (star_zs + star_max_depth)) / star_zs + viewy;
	double maxy = (600.0 * (star_zs + star_max_depth)) / star_zs + viewy;

	for(s = 0; s < numstars; s++)
	{
		double sx = stars[s].x;
		double sy = stars[s].y;
		
		while(sx < minx)
			sx += maxx - minx;
		
		while(sx > maxx)
			sx -= maxx - minx;
		
		while(sy < miny)
			sy += maxy - miny;
		
		while(sy > maxy)
			sy -= maxy - miny;
		
		int x = (int)floor((sx - viewx) * star_zs / stars[s].z * ((double)(vid_width) / 1600.0)) + vid_width / 2;
		int y = vid_height / 2 - 1 - (int)floor((sy - viewy) * star_zs / stars[s].z * ((double)(vid_width) / 1600.0));

		if(y < 0 || y >= vid_height)
		{
			continue;
		}

		if(x < 0 || x >= vid_width)
		{
			continue;
		}

		
		params.red = stars[s].alpha;
		params.green = stars[s].alpha;
		params.blue = stars[s].alpha;
		
		params.dest_x = x;
		params.dest_y = y;
		
		plot_pixel(&params);
		
		params.red >>= 1;
		params.green = params.blue = params.red;
		
		params.dest_x++;
		plot_pixel(&params);
		
		params.dest_x--;
		params.dest_y++;
		plot_pixel(&params);
		
		params.dest_y -= 2;
		plot_pixel(&params);
		
		params.dest_x--;
		params.dest_y++;
		plot_pixel(&params);
	}
}
