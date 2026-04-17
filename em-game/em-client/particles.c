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
#include <string.h>
#include <math.h>


#include "types.h"
#include "stringbuf.h"
#include "buffer.h"
#include "minmax.h"
#include "rdtsc.h"
#include "gsub.h"
#include "timer.h"
#include "main.h"
#include "game.h"
#include "render.h"
#include "particles.h"



#define NUMPARTICLES 80000
#define MAX_PARTICLE_TIME 0.025


struct particle_t *upper_particles;
uint32_t last_upper_particle_time;
uint8_t upper_pinuse[NUMPARTICLES];
int nupperpart;

struct particle_t *lower_particles;
uint32_t last_lower_particle_time;
uint8_t lower_pinuse[NUMPARTICLES];
int nlowerpart;



void init_particles()
{
	upper_particles = (struct particle_t*)malloc(sizeof(struct particle_t) * NUMPARTICLES);
	lower_particles = (struct particle_t*)malloc(sizeof(struct particle_t) * NUMPARTICLES);
}


void kill_particles()
{
	free(upper_particles);
	free(lower_particles);
}


void clear_particles()
{
	memset(upper_pinuse, 0, NUMPARTICLES);
	memset(lower_pinuse, 0, NUMPARTICLES);
}


void create_upper_particle(struct particle_t *p)
{
	upper_particles[nupperpart] = *p;

	upper_pinuse[nupperpart] = 1;

	++nupperpart;
	nupperpart %= NUMPARTICLES;
}


void create_lower_particle(struct particle_t *p)
{
	lower_particles[nlowerpart] = *p;

	lower_pinuse[nlowerpart] = 1;

	++nlowerpart;
	nlowerpart %= NUMPARTICLES;
}


/*
	switch(params->dest->flags)
	{
	case SURFACE_24BITPADDING8BIT:
	case SURFACE_24BIT:
	case SURFACE_24BITALPHA8BIT:
		pixel_alpha_plot_888(params);
		break;
	
	case SURFACE_16BIT:
	case SURFACE_16BITALPHA8BIT:
		pixel_alpha_plot_565(params);
		break;
	}

void pixel_alpha_plot_565_c(struct blit_params_t *params)
{
	uint16_t *dst = get_pixel_addr(params->dest, params->dest_x, params->dest_y);
	uint16_t colour = convert_24bit_to_16bit(params->red, 
		params->green, params->blue);	// error probably induced here
	uint16_t oldcolour = *dst;
	uint8_t negalpha = ~params->alpha;

	*dst = (vid_redalphalookup[((colour & 0xf800) >> 3) | params->alpha] + 
		vid_redalphalookup[((oldcolour & 0xf800) >> 3) | negalpha]) |
		(vid_greenalphalookup[((colour & 0x7e0) << 3) | params->alpha] +
		vid_greenalphalookup[((oldcolour & 0x7e0) << 3) | negalpha]) |
		(vid_bluealphalookup[((colour & 0x1f) << 8) | params->alpha] + 
		vid_bluealphalookup[((oldcolour & 0x1f) << 8) | negalpha]);
}

*/


void plot_particle(struct blit_params_t *params)
{
	uint8_t full_alpha = params->alpha;
	uint8_t neg_full_alpha = 255 - full_alpha;
	uint8_t half_alpha = full_alpha >> 1;
	uint8_t neg_half_alpha = 255 - half_alpha;
	
	uint8_t half_blended_blue = (params->blue * half_alpha) >> 8;
	uint8_t half_blended_green = (params->green * half_alpha) >> 8;
	uint8_t half_blended_red = (params->red * half_alpha) >> 8;
	
	
	uint8_t *dst = &params->dest->buf[(params->dest_y - 1) * params->dest->pitch + 
		params->dest_x * 4];

	if(dst[0] == 0)
		dst[0] = half_blended_blue;
	else
		dst[0] = half_blended_blue + ((dst[0] * neg_half_alpha) >> 8);
	
	if(dst[1] == 0)
		dst[1] = half_blended_green;
	else
		dst[1] = half_blended_green + ((dst[1] * neg_half_alpha) >> 8);
	
	if(dst[2] == 0)
		dst[2] = half_blended_red;
	else
		dst[2] = half_blended_red + ((dst[2] * neg_half_alpha) >> 8);


	dst += params->dest->pitch - 4;

	if(dst[0] == 0)
		dst[0] = half_blended_blue;
	else
		dst[0] = half_blended_blue + ((dst[0] * neg_half_alpha) >> 8);
	
	if(dst[1] == 0)
		dst[1] = half_blended_green;
	else
		dst[1] = half_blended_green + ((dst[1] * neg_half_alpha) >> 8);
	
	if(dst[2] == 0)
		dst[2] = half_blended_red;
	else
		dst[2] = half_blended_red + ((dst[2] * neg_half_alpha) >> 8);


	dst += 4;

	if(dst[0] == 0)
		dst[0] = (params->blue * full_alpha) >> 8;
	else
		dst[0] = (params->blue * full_alpha + dst[0] * neg_full_alpha) >> 8;
	
	if(dst[1] == 0)
		dst[1] = (params->green * full_alpha) >> 8;
	else
		dst[1] = (params->green * full_alpha + dst[1] * neg_full_alpha) >> 8;
	
	if(dst[2] == 0)
		dst[2] = (params->red * full_alpha) >> 8;
	else
		dst[2] = (params->red * full_alpha + dst[2] * neg_full_alpha) >> 8;


	dst += 4;

	if(dst[0] == 0)
		dst[0] = half_blended_blue;
	else
		dst[0] = half_blended_blue + ((dst[0] * neg_half_alpha) >> 8);
	
	if(dst[1] == 0)
		dst[1] = half_blended_green;
	else
		dst[1] = half_blended_green + ((dst[1] * neg_half_alpha) >> 8);
	
	if(dst[2] == 0)
		dst[2] = half_blended_red;
	else
		dst[2] = half_blended_red + ((dst[2] * neg_half_alpha) >> 8);


	dst += params->dest->pitch - 4;

	if(dst[0] == 0)
		dst[0] = half_blended_blue;
	else
		dst[0] = half_blended_blue + ((dst[0] * neg_half_alpha) >> 8);
	
	if(dst[1] == 0)
		dst[1] = half_blended_green;
	else
		dst[1] = half_blended_green + ((dst[1] * neg_half_alpha) >> 8);
	
	if(dst[2] == 0)
		dst[2] = half_blended_red;
	else
		dst[2] = half_blended_red + ((dst[2] * neg_half_alpha) >> 8);
}
		

void render_upper_particles()
{
	int p;
	
	struct blit_params_t params;
		
	params.dest = s_backbuffer;
	params.red = 0xff;
	
	for(p = 0; p != NUMPARTICLES; p++)
	{
		int i = (p + nupperpart) % NUMPARTICLES;

		if(upper_pinuse[i])
		{
			float life = cgame_time - upper_particles[i].creation;
			
			if(life > 1.0f)
			{
				upper_pinuse[i] = 0;
				continue;
			}
			
			float delta_time = cgame_time - upper_particles[i].last;
			
			upper_particles[i].last = cgame_time;
			
		//	double particle_time = delta_time;
		//	int particle_ticks = 1;
			
		//	while(particle_time > MAX_PARTICLE_TIME)
		//		particle_time /= 2, particle_ticks++;
			

		//	while(particle_ticks--)
			{
				upper_particles[i].xvel += (drand48() - 0.5) * 2400 * delta_time;
				upper_particles[i].yvel += (drand48() - 0.5) * 2400 * delta_time;
				
				float dampening = exp(-8.0f * delta_time);
				
				upper_particles[i].xvel *= dampening;
				upper_particles[i].yvel *= dampening;
	
				upper_particles[i].xpos += upper_particles[i].xvel * delta_time;
				upper_particles[i].ypos += upper_particles[i].yvel * delta_time;
			}

			int x, y;
			
			world_to_screen(upper_particles[i].xpos, upper_particles[i].ypos, &x, &y);
			
			
			// determine if we are visible
			
			if(x - 1 < 0 || x + 2 > vid_width - 1 || y - 1 < 0 || y + 1 > vid_height - 1)
				continue;

			
			
			float a = life * 4.0;
			
			if(life < 0.25)
			{
				params.red = (uint8_t)floor(upper_particles[i].start_red * (1.0 - a) + 
					upper_particles[i].end_red * a);
				
				params.green = (uint8_t)floor(upper_particles[i].start_green * (1.0 - a) + 
					upper_particles[i].end_green * a);
				
				params.blue = (uint8_t)floor(upper_particles[i].start_blue * (1.0 - a) + 
					upper_particles[i].end_blue * a);
			}
			else
			{
				params.red = upper_particles[i].end_red;
				params.green = upper_particles[i].end_green;
				params.blue = upper_particles[i].end_blue;
			}
			
			params.alpha  = (uint8_t)(255 - floor(life * 255.0f));
			
			params.dest_x = x;
			params.dest_y = y;
			
			switch(s_backbuffer->flags)
			{
			case SURFACE_24BITPADDING8BIT:
			case SURFACE_24BIT:
				plot_particle(&params);
				break;
			
			case SURFACE_16BIT:
				alpha_plot_pixel(&params);
			
				params.alpha >>= 1;
				params.dest_y--;
				alpha_plot_pixel(&params);

				params.dest_x--;
				params.dest_y++;
				alpha_plot_pixel(&params);

				params.dest_x += 2;
				alpha_plot_pixel(&params);

				params.dest_x--;
				params.dest_y++;
				alpha_plot_pixel(&params);
				break;
			}
		}
	}
}


void render_lower_particles()
{
	int p;
	
	struct blit_params_t params;
		
	params.dest = s_backbuffer;
	params.red = 0xff;

	for(p = 0; p != NUMPARTICLES; p++)
	{
		int i = (p + nlowerpart) % NUMPARTICLES;

		if(lower_pinuse[i])
		{
			float life = cgame_time - lower_particles[i].creation;
			
			if(life > 1.0f)
			{
				lower_pinuse[i] = 0;
				continue;
			}
			
			float delta_time = cgame_time - lower_particles[i].last;
			
			lower_particles[i].last = cgame_time;

		//	double particle_time = delta_time;
		//	int particle_ticks = 1;
			
		//	while(particle_time > MAX_PARTICLE_TIME)
		//		particle_time /= 2, particle_ticks++;
			

		//	while(particle_ticks--)
			{
				lower_particles[i].xvel += (drand48() - 0.5) * 2400 * delta_time;
				lower_particles[i].yvel += (drand48() - 0.5) * 2400 * delta_time;
				
				float dampening = exp(-8.0f * delta_time);
				
				lower_particles[i].xvel *= dampening;
				lower_particles[i].yvel *= dampening;
				
				lower_particles[i].xpos += lower_particles[i].xvel * delta_time;
				lower_particles[i].ypos += lower_particles[i].yvel * delta_time;
			}

			int x, y;
			
			world_to_screen(lower_particles[i].xpos, lower_particles[i].ypos, &x, &y);
			
			// determine if we are visible
			
			if(x - 1 < 0 || x + 2 > vid_width - 1 || y - 1 < 0 || y + 1 > vid_height - 1)
				continue;

			
			
			float a = life * 4.0;
			
			if(life < 0.25)
			{
				params.red = (uint8_t)floor(lower_particles[i].start_red * (1.0 - a) + 
					lower_particles[i].end_red * a);
				
				params.green = (uint8_t)floor(lower_particles[i].start_green * (1.0 - a) + 
					lower_particles[i].end_green * a);
				
				params.blue = (uint8_t)floor(lower_particles[i].start_blue * (1.0 - a) + 
					lower_particles[i].end_blue * a);
			}
			else
			{
				params.red = lower_particles[i].end_red;
				params.green = lower_particles[i].end_green;
				params.blue = lower_particles[i].end_blue;
			}
			
			params.alpha  = (uint8_t)(255 - floor(life * 255.0f));
			
			params.dest_x = x;
			params.dest_y = y;
			
			switch(s_backbuffer->flags)
			{
			case SURFACE_24BITPADDING8BIT:
			case SURFACE_24BIT:
				plot_particle(&params);
				break;
			
			case SURFACE_16BIT:
				alpha_plot_pixel(&params);
			
				params.alpha >>= 1;
				params.dest_y--;
				alpha_plot_pixel(&params);

				params.dest_x--;
				params.dest_y++;
				alpha_plot_pixel(&params);

				params.dest_x += 2;
				alpha_plot_pixel(&params);

				params.dest_x--;
				params.dest_y++;
				alpha_plot_pixel(&params);
				break;
			}
		}
	}
}
