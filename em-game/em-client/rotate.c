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

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <math.h>
#include <assert.h>

#include <zlib.h>


#include "types.h"
#include "vertex.h"
#include "polygon.h"
#include "llist.h"
#include "minmax.h"
#include "gsub.h"
#include "rotate.h"


/*
#define zs 48.0

#define sourcex ((double)src_xpos - (double)in_width / 2.0)
#define sourcey ((double)src_ypos - (double)in_height / 2.0)

#define rollx (sourcex * costhi)
#define rolly (sourcey)
#define rollz (sourcex * sinthi) + zs

#define rotatex (rollx * costheta - rolly * sintheta)
#define rotatey (rollx * sintheta + rolly * costheta)
#define rotatez (rollz)

#define destx ((rotatex * zs) / rotatez)
#define desty ((rotatey * zs) / rotatez)
*/


#define sourcex ((double)src_xpos - (double)in_width / 2.0)
#define sourcey ((double)src_ypos - (double)in_height / 2.0)

#define scalex (sourcex * factor)
#define scaley (-sourcey * factor)

#define destx ((scalex * cos_theta - scaley * sin_theta) + (double)out_width / 2.0)
#define desty (-(scalex * sin_theta + scaley * cos_theta) + (double)out_height / 2.0)



void do_rotate(struct rotate_t *rotate0, int width, int height, int frames)
{
	double factor;
	int in_width, in_height;
	
	assert(rotate0);
	assert(rotate0->in_surface);
	in_width = rotate0->in_surface->width;
	in_height = rotate0->in_surface->height;
	
	factor = (double)width / (double)in_width;
	
	int out_width = (int)ceil(hypot((double)width, (double)height));
	int out_height = out_width;
	
	struct rotate_t *rotate = rotate0;
		
	while(rotate)
	{
		assert(rotate->in_surface);
		assert(rotate->in_surface->flags == SURFACE_24BITALPHA8BIT);
		assert(rotate->in_surface->width == in_width);
		assert(rotate->in_surface->height == in_height);
		
		*rotate->out_surface = new_surface(SURFACE_FLOATSALPHAFLOATS, 
			out_width, out_height * frames);
		clear_surface(*rotate->out_surface);
		rotate = rotate->next;
	}
	
	struct vertex_t *upper_row = malloc((in_width + 1) * sizeof(struct vertex_t));
	struct vertex_t *lower_row = malloc((in_width + 1) * sizeof(struct vertex_t));

	int f, src_ypos, src_xpos, i, dst_ypos, dst_xpos;

	for(f = 0; f < frames / 4; f++)
	{
		double sin_theta, cos_theta;
		
		sincos(2 * M_PI * ((double)f / (double)frames - 0.5), &sin_theta, &cos_theta);

		src_ypos = 1;

		for(src_xpos = 0; src_xpos < (in_width + 1); src_xpos++)
		{
			lower_row[src_xpos].x = (float)destx;
			lower_row[src_xpos].y = (float)desty;
		}

		for(src_ypos = 0; src_ypos < in_height; src_ypos++)
		{
			for(src_xpos = 0; src_xpos < (in_width + 1); src_xpos++)
			{
				upper_row[src_xpos].x = (float)destx;
				upper_row[src_xpos].y = (float)desty;
			}

			for(src_xpos = 0; src_xpos < in_width; src_xpos++)
			{
				// obtain verticies in c/w order for source pixel

				struct polygon_t src_poly;
				src_poly.vertex[0] = upper_row[src_xpos];
				src_poly.vertex[1] = upper_row[src_xpos + 1];
				src_poly.vertex[2] = lower_row[src_xpos + 1];
				src_poly.vertex[3] = lower_row[src_xpos];

				src_poly.numverts = 4;


				// determine dest block to be tested

				int x = (int)floorf(src_poly.vertex[0].x);
				int y = (int)floorf(src_poly.vertex[0].y);

				int min_xpos = x;
				int max_xpos = x;
				int min_ypos = y;
				int max_ypos = y;

				for(i = 1; i != 4; i++)
				{
					x = (int)floorf(src_poly.vertex[i].x);
					y = (int)floorf(src_poly.vertex[i].y);

					if(x < min_xpos)
						min_xpos = x;

					if(x > max_xpos)
						max_xpos = x;

					if(y < min_ypos)
						min_ypos = y;

					if(y > max_ypos)
						max_ypos = y;
				}


				// early out for zero influence source pixels

				if(max_xpos < 0 || min_xpos >= out_width || 
					max_ypos < 0 || min_ypos >= out_height)
					continue;


				// trim dest block

				min_xpos = max(min_xpos, 0);
				min_ypos = max(min_ypos, 0);
				max_xpos = min(max_xpos, out_width - 1);
				max_ypos = min(max_ypos, out_height - 1);

				struct rotate_t *rotate = rotate0;
					
				while(rotate)
				{
					uint8_t *src = &((uint8_t*)rotate->in_surface->alpha_buf)
						[src_ypos * in_width + src_xpos];
					rotate->alpha = (double)(*src) / 255.0;
					
					src = &((uint8_t*)rotate->in_surface->buf)
						[(src_ypos * in_width + src_xpos) * 3];
					rotate->red = ((double)(*src++) / 255.0) * rotate->alpha;
					rotate->green = ((double)(*src++) / 255.0) * rotate->alpha;
					rotate->blue = ((double)(*src) / 255.0) * rotate->alpha;
					
					rotate = rotate->next;
				}

				for(dst_ypos = min_ypos; dst_ypos <= max_ypos; dst_ypos++)
				{
					for(dst_xpos = min_xpos; dst_xpos <= max_xpos; dst_xpos++)
					{
						// generate vertexes in c/w order for destination pixel polygon

						float x = (float)dst_xpos;
						float y = (float)dst_ypos;

						struct polygon_t dst_poly;

						dst_poly.vertex[0].x = x;
						dst_poly.vertex[0].y = y;
						dst_poly.vertex[1].x = x;
						dst_poly.vertex[1].y = y + 1.0f;
						dst_poly.vertex[2].x = x + 1.0f;
						dst_poly.vertex[2].y = y + 1.0f;
						dst_poly.vertex[3].x = x + 1.0f;
						dst_poly.vertex[3].y = y;

						dst_poly.numverts = 4;


						// calculate area of clipped dest pixel

						double area = poly_clip_area(&dst_poly, &src_poly);


						// early out for zero-influence dest pixels

						if(area == 0.0)
							continue;


						// actuate pixel intensity

						rotate = rotate0;
						
						while(rotate)
						{
							float *dst = &((float*)(*rotate->out_surface)->buf)
								[((dst_ypos + f * out_height) * out_width + dst_xpos) * 3];
	
							*dst++ += (float)(rotate->red * area);
							*dst++ += (float)(rotate->green * area);
							*dst += (float)(rotate->blue * area);
	
							((float*)(*rotate->out_surface)->alpha_buf)
								[(dst_ypos + f * out_height) * out_width + dst_xpos] += 
								(float)(rotate->alpha * area);
							
							rotate = rotate->next;
						}
					}
				}
			}

			struct vertex_t *temp = upper_row;
			upper_row = lower_row;
			lower_row = temp;
		}
		
		printf(".");
		fflush(stdout);
	}

	printf("\n");

	free(upper_row);
	free(lower_row);

	
	rotate = rotate0;
	
	while(rotate)
	{
		free_surface(rotate->in_surface);
	
		int x = out_width * out_height * frames / 4;
		float *dst = (float*)(*rotate->out_surface)->buf;
		float *src = (float*)(*rotate->out_surface)->alpha_buf;
		
		while(x--)
		{
			*dst++ /= *src;
			*dst++ /= *src;
			*dst++ /= *src++;
		}
	
		convert_surface_to_24bitalpha8bit(*rotate->out_surface);
		
		rotate = rotate->next;
	}
	
	
	rotate = rotate0;
	
	while(rotate)
	{
		uint8_t *dst = &(*rotate->out_surface)->buf
			[((frames / 4 * out_height) * out_width) * 3];
		
		uint8_t *adst = &(*rotate->out_surface)->alpha_buf
			[(frames / 4 * out_height) * out_width];
		
		int x, y;
		
		for(f = 0; f < frames / 4; f++)
		{
			for(y = 0; y < out_height; y++)
			{
				for(x = 0; x < out_width; x++)
				{
					uint8_t *src = &(*rotate->out_surface)->buf
						[(x * out_width + out_height - 1 - y + 
						out_height * out_width * f) * 3];
					
					*dst++ = *src++;
					*dst++ = *src++;
					*dst++ = *src;
					
					src = &(*rotate->out_surface)->alpha_buf
						[x * out_width + out_height - 1 - y + 
						out_height * out_width * f];
					
					*adst++ = *src;
				}
			}
		}
		
		for(f = 0; f < frames / 4; f++)
		{
			for(y = 0; y < out_height; y++)
			{
				for(x = 0; x < out_width; x++)
				{
					uint8_t *src = &(*rotate->out_surface)->buf
						[(x * out_width + out_height - 1 - y + 
						out_height * out_width * (f + frames / 4)) * 3];
					
					*dst++ = *src++;
					*dst++ = *src++;
					*dst++ = *src;
					
					src = &(*rotate->out_surface)->alpha_buf
						[x * out_width + out_height - 1 - y + 
						out_height * out_width * (f + frames / 4)];
					
					*adst++ = *src;
				}
			}
		}
		
		for(f = 0; f < frames / 4; f++)
		{
			for(y = 0; y < out_height; y++)
			{
				for(x = 0; x < out_width; x++)
				{
					uint8_t *src = &(*rotate->out_surface)->buf
						[(x * out_width + out_height - 1 - y + 
						out_height * out_width * (f + frames / 2)) * 3];
					
					*dst++ = *src++;
					*dst++ = *src++;
					*dst++ = *src;
					
					src = &(*rotate->out_surface)->alpha_buf
						[x * out_width + out_height - 1 - y + 
						out_height * out_width * (f + frames / 2)];
					
					*adst++ = *src;
				}
			}
		}
		
		rotate = rotate->next;
	}
}
