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

#include <stdio.h>
#include <stdint.h>

#include "prefix.h"

#include "resource.h"
#include "gsub.h"
#include "nodes.h"
#include "map.h"
#include "worker.h"
#include "main.h"

struct surface_t *s_cameron = NULL, *s_scaled_cameron = NULL;
int cameron_width = 0, cameron_height = 0;
int cameron_pending = 0;

struct surface_t *out_cameron;


void process_cameron()	// called when window size changes
{
	if(!s_cameron)
		return;

	double cameron_ratio = (double)s_cameron->width / (double)s_cameron->height;
	double vid_ratio = (double)vid_width / (double)vid_height;

	int new_cameron_width, new_cameron_height;

	if(cameron_ratio > vid_ratio)
	{
		new_cameron_width = vid_width;
		new_cameron_height = new_cameron_width / cameron_ratio;
	}
	else
	{
		new_cameron_height = vid_height;
		new_cameron_width = cameron_ratio * new_cameron_height;
	}

	if(cameron_width != new_cameron_width || cameron_height != new_cameron_height)
	{
		stop_working();

		cameron_width = new_cameron_width;
		cameron_height = new_cameron_height;
		cameron_pending = 1;
		
		start_working();
		return;
	}
}


void cameron_finished()
{
	free_surface(s_scaled_cameron);
	s_scaled_cameron = out_cameron;
	cameron_pending = 0;
}


void draw_cameron()
{
	if(s_scaled_cameron)
	{
		struct blit_params_t params;
			
		params.source = s_scaled_cameron;
		params.dest = s_backbuffer;
		params.dest_x = (vid_width - s_scaled_cameron->width) / 2;
		params.dest_y = (vid_height - s_scaled_cameron->height) / 2;
		
		blit_surface(&params);
	}
}


void init_cameron()
{
	s_cameron = read_png_surface(find_resource("splash.png"));
	cameron_width = s_cameron->width;
	cameron_height = s_cameron->height;
}


void kill_cameron()
{
	free_surface(s_cameron);
	s_cameron = NULL;
	free_surface(s_scaled_cameron);
	s_scaled_cameron = NULL;
}


//
// WORKER THREAD FUNCTIONS
//


void scale_cameron()
{
	if(cameron_width == s_cameron->width && cameron_height == s_cameron->height)
		out_cameron = duplicate_surface(s_cameron);
	else
		out_cameron = resize(s_cameron, cameron_width, cameron_height);
}
