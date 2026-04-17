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
#include <math.h>

#include <zlib.h>

#include "llist.h"
#include "gsub.h"
#include "game.h"
#include "render.h"

struct floating_image_t
{
	float x, y;
	struct surface_t *surface;

	struct floating_image_t *next;

} *floating_image0;


int generate_and_write_scaled_floating_images(gzFile infile, gzFile outfile)
{
	int c;
	
	if(gzread(infile, &c, 4) != 4)
		goto error;
	
	if(gzwrite(outfile, &c, 4) != 4)
		goto error;
	
	while(c--)
	{
		struct floating_image_t floating_image;
			
		if(gzread(infile, &floating_image.x, 4) != 4)
			goto error;
		
		if(gzwrite(outfile, &floating_image.x, 4) != 4)
			goto error;
		
		if(gzread(infile, &floating_image.y, 4) != 4)
			goto error;
		
		if(gzwrite(outfile, &floating_image.y, 4) != 4)
			goto error;
		
		struct surface_t *surface = gzread_raw_surface(infile);
		
		floating_image.surface = resize(surface, surface->width * vid_width / 1600, 
			surface->height * vid_height / 1200);

		free_surface(surface);
		
		gzwrite_raw_surface(outfile, floating_image.surface);


		LL_ADD(struct floating_image_t, &floating_image0, &floating_image);
	}
	
	return 1;
	
	error:
	
	return 0;
}


void gzread_floating_images(gzFile file)
{
	int c;
	
	gzread(file, &c, 4);
	
	while(c--)
	{
		struct floating_image_t floating_image;
			
		gzread(file, &floating_image.x, 4);
		gzread(file, &floating_image.y, 4);
		
		floating_image.surface = gzread_raw_surface(file);
		
		LL_ADD(struct floating_image_t, &floating_image0, &floating_image);
	}
}


void render_floating_images()
{
	struct floating_image_t *cfloating_image = floating_image0;
	int x, y;
	struct blit_params_t params;
	params.dest = s_backbuffer;
	
	while(cfloating_image)
	{
		world_to_screen(cfloating_image->x, cfloating_image->y, &x, &y);
	
		params.dest_x = x - cfloating_image->surface->width / 2;
		params.dest_y = y - cfloating_image->surface->width / 2;
	
		params.width = cfloating_image->surface->width;
		params.height = cfloating_image->surface->height;
		
		params.source = cfloating_image->surface;

		blit_surface(&params);
		
		cfloating_image = cfloating_image->next;
	}
}


void clear_floating_images()
{
	struct floating_image_t *cfloating_image = floating_image0;
	
	while(cfloating_image)
	{
		free_surface(cfloating_image->surface);
		cfloating_image = cfloating_image->next;
	}
	
	LL_REMOVE_ALL(struct floating_image_t, &floating_image0);
}
