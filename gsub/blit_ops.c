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
#include <stdlib.h>
#include <string.h>

#include "gsub.h"

#if defined(__i386__) || defined(__x86_64__)
void alpha_surface_blit_mmx() __attribute__ ((cdecl));
void alpha_pixel_plot_x86()	__attribute__ ((cdecl));
void alpha_rect_draw_mmx() __attribute__ ((cdecl));
void surface_blit_mmx() __attribute__ ((cdecl));
#endif



void rect_draw_888P8_c(struct blit_params_t *params)
{
	uint8_t *dst = get_pixel_addr(params->dest, params->dest_x, params->dest_y);
	
	int x, y = params->height;

	while(y)
	{
		for(x = 0; x < params->width; x++)
		{
			dst[x * 4] = params->blue;
			dst[x * 4 + 1] = params->green;
			dst[x * 4 + 2] = params->red;
		}

		dst += params->dest->pitch;
		y--;
	}
}


void rect_alpha_draw_888P8_c(struct blit_params_t *params)
{
	uint8_t *dst = get_pixel_addr(params->dest, params->dest_x, params->dest_y);

	uint8_t redalpha = vid_alphalookup[params->red << 8 | params->alpha];
	uint8_t greenalpha = vid_alphalookup[params->green << 8 | params->alpha];
	uint8_t bluealpha = vid_alphalookup[params->blue << 8 | params->alpha];

	uint8_t negalpha = ~params->alpha;

	int x, y = params->height;
	
	while(y)
	{
		for(x = 0; x < params->width; x++)
		{
			dst[x * 4] = bluealpha + vid_alphalookup[dst[x * 4] << 8 | negalpha];
			dst[x * 4 + 1] = greenalpha + vid_alphalookup[dst[x * 4 + 1] << 8 | negalpha];
			dst[x * 4 + 2] = redalpha + vid_alphalookup[dst[x * 4 + 2] << 8 | negalpha];
		}

		dst += params->dest->pitch;
		y--;
	}
}


void surface_blit_A8_888P8_c(struct blit_params_t *params)
{
	uint8_t *alpha_src = get_alpha_pixel_addr(params->source, params->source_x, params->source_y);
	uint8_t *dst = get_pixel_addr(params->dest, params->dest_x, params->dest_y);

	int x, y = params->height;

	while(y)
	{
		for(x = 0; x < params->width; x++)
		{
			uint8_t alpha = alpha_src[x];
			if(alpha == 0)
				continue;
			
			uint8_t negalpha = ~alpha;

			dst[x * 4] = vid_alphalookup[params->blue << 8 | alpha] + 
				vid_alphalookup[dst[x * 4] << 8 | negalpha];
			dst[x * 4 + 1] = vid_alphalookup[params->green << 8 | alpha] + 
				vid_alphalookup[dst[x * 4 + 1] << 8 | negalpha];
			dst[x * 4 + 2] = vid_alphalookup[params->red << 8 | alpha] + 
				vid_alphalookup[dst[x * 4 + 2] << 8 | negalpha];
		}

		alpha_src += params->source->alpha_pitch;
		dst += params->dest->pitch;
		y--;
	}
}


void surface_alpha_blit_A8_888P8_c(struct blit_params_t *params)
{
	uint8_t *alpha_src = get_alpha_pixel_addr(params->source, params->source_x, params->source_y);
	uint8_t *dst = get_pixel_addr(params->dest, params->dest_x, params->dest_y);

	int x, y = params->height;

	while(y)
	{
		for(x = 0; x < params->width; x++)
		{
			uint8_t alpha = vid_alphalookup[alpha_src[x] << 8 | params->alpha];
			if(alpha == 0)
				continue;
			
			uint8_t negalpha = ~alpha;

			dst[x * 4] = vid_alphalookup[params->blue << 8 | alpha] + 
				vid_alphalookup[dst[x * 4] << 8 | negalpha];
			dst[x * 4 + 1] = vid_alphalookup[params->green << 8 | alpha] + 
				vid_alphalookup[dst[x * 4 + 1] << 8 | negalpha];
			dst[x * 4 + 2] = vid_alphalookup[params->red << 8 | alpha] + 
				vid_alphalookup[dst[x * 4 + 2] << 8 | negalpha];
		}

		alpha_src += params->source->alpha_pitch;
		dst += params->dest->pitch;
		y--;
	}
}


void surface_blit_888_888P8_c(struct blit_params_t *params)
{
	uint8_t *src = get_pixel_addr(params->source, params->source_x, params->source_y);
	uint8_t *dst = get_pixel_addr(params->dest, params->dest_x, params->dest_y);

	int x, y = params->height;

	while(y)
	{
		for(x = 0; x < params->width; x++)
		{
			dst[x * 4] = src[x * 3 + 2];
			dst[x * 4 + 1] = src[x * 3 + 1];
			dst[x * 4 + 2] = src[x * 3];
		}

		src += params->source->pitch;
		dst += params->dest->pitch;
		y--;
	}
}


void surface_alpha_blit_888_888P8_c(struct blit_params_t *params)
{
	uint8_t *src = get_pixel_addr(params->source, params->source_x, params->source_y);
	uint8_t *dst = get_pixel_addr(params->dest, params->dest_x, params->dest_y);

	uint8_t negalpha = ~params->alpha;

	int x, y = params->height;

	while(y)
	{
		for(x = 0; x < params->width; x++)
		{
			dst[x * 4] = vid_alphalookup[src[x * 3 + 2] << 8 | params->alpha] + 
				vid_alphalookup[dst[x * 4] << 8 | negalpha];
			dst[x * 4 + 1] = vid_alphalookup[src[x * 3 + 1] << 8 | params->alpha] + 
				vid_alphalookup[dst[x * 4 + 1] << 8 | negalpha];
			dst[x * 4 + 2] = vid_alphalookup[src[x * 3] << 8 | params->alpha] + 
				vid_alphalookup[dst[x * 4 + 2] << 8 | negalpha];
		}

		src += params->source->pitch;
		dst += params->dest->pitch;
		y--;
	}
}


void surface_blit_888A8_888P8_c(struct blit_params_t *params)
{
	uint8_t *src = get_pixel_addr(params->source, params->source_x, params->source_y);
	uint8_t *alpha_src = get_alpha_pixel_addr(params->source, params->source_x, params->source_y);
	uint8_t *dst = get_pixel_addr(params->dest, params->dest_x, params->dest_y);

	int x, y = params->height;

	while(y)
	{
		for(x = 0; x < params->width; x++)
		{
			uint8_t alpha = alpha_src[x];
			if(alpha == 0)
				continue;
			
			uint8_t negalpha = ~alpha;
			
			dst[x * 4] = vid_alphalookup[src[x * 3 + 2] << 8 | alpha] + 
				vid_alphalookup[dst[x * 4] << 8 | negalpha];
			dst[x * 4 + 1] = vid_alphalookup[src[x * 3 + 1] << 8 | alpha] + 
				vid_alphalookup[dst[x * 4 + 1] << 8 | negalpha];
			dst[x * 4 + 2] = vid_alphalookup[src[x * 3] << 8 | alpha] + 
				vid_alphalookup[dst[x * 4 + 2] << 8 | negalpha];
		}

		src += params->source->pitch;
		alpha_src += params->source->alpha_pitch;
		dst += params->dest->pitch;
		y--;
	}
}


void surface_alpha_blit_888A8_888P8_c(struct blit_params_t *params)
{
	uint8_t *src = get_pixel_addr(params->source, params->source_x, params->source_y);
	uint8_t *alpha_src = get_alpha_pixel_addr(params->source, params->source_x, params->source_y);
	uint8_t *dst = get_pixel_addr(params->dest, params->dest_x, params->dest_y);

	int x, y = params->height;

	while(y)
	{
		for(x = 0; x < params->width; x++)
		{
			uint8_t alpha = vid_alphalookup[alpha_src[x] << 8 | params->alpha];
			if(alpha == 0)
				continue;
			
			uint8_t negalpha = ~alpha;
			
			dst[x * 4] = vid_alphalookup[src[x * 3 + 2] << 8 | alpha] + 
				vid_alphalookup[dst[x * 4] << 8 | negalpha];
			dst[x * 4 + 1] = vid_alphalookup[src[x * 3 + 1] << 8 | alpha] + 
				vid_alphalookup[dst[x * 4 + 1] << 8 | negalpha];
			dst[x * 4 + 2] = vid_alphalookup[src[x * 3] << 8 | alpha] + 
				vid_alphalookup[dst[x * 4 + 2] << 8 | negalpha];
		}

		src += params->source->pitch;
		alpha_src += params->source->alpha_pitch;
		dst += params->dest->pitch;
		y--;
	}
}


void pixel_alpha_plot_888_c(struct blit_params_t *params)
{
	uint8_t *dst = get_pixel_addr(params->dest, params->dest_x, params->dest_y);
	uint8_t negalpha = ~params->alpha;
	
	dst[0] = vid_alphalookup[params->blue << 8 | params->alpha] + 
		vid_alphalookup[dst[0] << 8 | negalpha];
	dst[1] = vid_alphalookup[params->green << 8 | params->alpha] + 
		vid_alphalookup[dst[1] << 8 | negalpha];
	dst[2] = vid_alphalookup[params->red << 8 | params->alpha] + 
		vid_alphalookup[dst[2] << 8 | negalpha];
}


void rect_draw_888_c(struct blit_params_t *params)
{
	uint8_t *dst = get_pixel_addr(params->dest, params->dest_x, params->dest_y);
	
	int x, y = params->height;

	while(y)
	{
		for(x = 0; x < params->width; x++)
		{
			dst[x * 3] = params->red;
			dst[x * 3 + 1] = params->green;
			dst[x * 3 + 2] = params->blue;
		}

		dst += params->dest->pitch;
		y--;
	}
}


void rect_alpha_draw_888_c(struct blit_params_t *params)
{
	uint8_t *dst = get_pixel_addr(params->dest, params->dest_x, params->dest_y);

	uint8_t redalpha = vid_alphalookup[params->red << 8 | params->alpha];
	uint8_t greenalpha = vid_alphalookup[params->green << 8 | params->alpha];
	uint8_t bluealpha = vid_alphalookup[params->blue << 8 | params->alpha];

	uint8_t negalpha = ~params->alpha;

	int x, y = params->height;
	
	while(y)
	{
		for(x = 0; x < params->width; x++)
		{
			dst[x * 3] = redalpha + vid_alphalookup[dst[x * 4] << 8 | negalpha];
			dst[x * 3 + 1] = greenalpha + vid_alphalookup[dst[x * 4 + 1] << 8 | negalpha];
			dst[x * 3 + 2] = bluealpha + vid_alphalookup[dst[x * 4 + 2] << 8 | negalpha];
		}

		dst += params->dest->pitch;
		y--;
	}
}


void surface_blit_A8_888_c(struct blit_params_t *params)
{
	uint8_t *alpha_src = get_alpha_pixel_addr(params->source, params->source_x, params->source_y);
	uint8_t *dst = get_pixel_addr(params->dest, params->dest_x, params->dest_y);

	int x, y = params->height;

	while(y)
	{
		for(x = 0; x < params->width; x++)
		{
			uint8_t alpha = alpha_src[x];
			if(alpha == 0)
				continue;
			
			uint8_t negalpha = ~alpha;

			dst[x * 3] = vid_alphalookup[params->red << 8 | alpha] + 
				vid_alphalookup[dst[x * 3] << 8 | negalpha];
			dst[x * 3 + 1] = vid_alphalookup[params->green << 8 | alpha] + 
				vid_alphalookup[dst[x * 3 + 1] << 8 | negalpha];
			dst[x * 3 + 2] = vid_alphalookup[params->blue << 8 | alpha] + 
				vid_alphalookup[dst[x * 3 + 2] << 8 | negalpha];
		}

		alpha_src += params->source->alpha_pitch;
		dst += params->dest->pitch;
		y--;
	}
}


void surface_alpha_blit_A8_888_c(struct blit_params_t *params)
{
	uint8_t *alpha_src = get_alpha_pixel_addr(params->source, params->source_x, params->source_y);
	uint8_t *dst = get_pixel_addr(params->dest, params->dest_x, params->dest_y);

	int x, y = params->height;

	while(y)
	{
		for(x = 0; x < params->width; x++)
		{
			uint8_t alpha = vid_alphalookup[alpha_src[x] << 8 | params->alpha];
			if(alpha == 0)
				continue;
			
			uint8_t negalpha = ~alpha;

			dst[x * 3] = vid_alphalookup[params->red << 8 | alpha] + 
				vid_alphalookup[dst[x * 3] << 8 | negalpha];
			dst[x * 3 + 1] = vid_alphalookup[params->green << 8 | alpha] + 
				vid_alphalookup[dst[x * 3 + 1] << 8 | negalpha];
			dst[x * 3 + 2] = vid_alphalookup[params->blue << 8 | alpha] + 
				vid_alphalookup[dst[x * 3 + 2] << 8 | negalpha];
		}

		alpha_src += params->source->alpha_pitch;
		dst += params->dest->pitch;
		y--;
	}
}


void surface_blit_888_888_c(struct blit_params_t *params)
{
	uint8_t *src = get_pixel_addr(params->source, params->source_x, params->source_y);
	uint8_t *dst = get_pixel_addr(params->dest, params->dest_x, params->dest_y);

	int y = params->height;

	while(y)
	{
		memcpy(dst, src, params->width * 3);

		src += params->source->pitch;
		dst += params->dest->pitch;
		y--;
	}
}


void surface_alpha_blit_888_888_c(struct blit_params_t *params)
{
	uint8_t *src = get_pixel_addr(params->source, params->source_x, params->source_y);
	uint8_t *dst = get_pixel_addr(params->dest, params->dest_x, params->dest_y);

	uint8_t negalpha = ~params->alpha;

	int x, y = params->height;

	while(y)
	{
		for(x = 0; x < params->width; x++)
		{
			dst[x * 3] = vid_alphalookup[src[x * 3] << 8 | params->alpha] + 
				vid_alphalookup[dst[x * 3] << 8 | negalpha];
			dst[x * 3 + 1] = vid_alphalookup[src[x * 3 + 1] << 8 | params->alpha] + 
				vid_alphalookup[dst[x * 3 + 1] << 8 | negalpha];
			dst[x * 3 + 2] = vid_alphalookup[src[x * 3 + 2] << 8 | params->alpha] + 
				vid_alphalookup[dst[x * 3 + 2] << 8 | negalpha];
		}

		src += params->source->pitch;
		dst += params->dest->pitch;
		y--;
	}
}


void surface_blit_888A8_888_c(struct blit_params_t *params)
{
	uint8_t *src = get_pixel_addr(params->source, params->source_x, params->source_y);
	uint8_t *alpha_src = get_alpha_pixel_addr(params->source, params->source_x, params->source_y);
	uint8_t *dst = get_pixel_addr(params->dest, params->dest_x, params->dest_y);

	int x, y = params->height;

	while(y)
	{
		for(x = 0; x < params->width; x++)
		{
			uint8_t alpha = alpha_src[x];
			if(alpha == 0)
				continue;
			
			uint8_t negalpha = ~alpha;
			
			dst[x * 3] = vid_alphalookup[src[x * 3] << 8 | alpha] + 
				vid_alphalookup[dst[x * 3] << 8 | negalpha];
			dst[x * 3 + 1] = vid_alphalookup[src[x * 3 + 1] << 8 | alpha] + 
				vid_alphalookup[dst[x * 3 + 1] << 8 | negalpha];
			dst[x * 3 + 2] = vid_alphalookup[src[x * 3 + 2] << 8 | alpha] + 
				vid_alphalookup[dst[x * 3 + 2] << 8 | negalpha];
		}

		src += params->source->pitch;
		alpha_src += params->source->alpha_pitch;
		dst += params->dest->pitch;
		y--;
	}
}


void surface_alpha_blit_888A8_888_c(struct blit_params_t *params)
{
	uint8_t *src = get_pixel_addr(params->source, params->source_x, params->source_y);
	uint8_t *alpha_src = get_alpha_pixel_addr(params->source, params->source_x, params->source_y);
	uint8_t *dst = get_pixel_addr(params->dest, params->dest_x, params->dest_y);

	int x, y = params->height;

	while(y)
	{
		for(x = 0; x < params->width; x++)
		{
			uint8_t alpha = vid_alphalookup[alpha_src[x] << 8 | params->alpha];
			if(alpha == 0)
				continue;
			
			uint8_t negalpha = ~alpha;
			
			dst[x * 3] = vid_alphalookup[src[x * 3] << 8 | alpha] + 
				vid_alphalookup[dst[x * 3] << 8 | negalpha];
			dst[x * 3 + 1] = vid_alphalookup[src[x * 3 + 1] << 8 | alpha] + 
				vid_alphalookup[dst[x * 3 + 1] << 8 | negalpha];
			dst[x * 3 + 2] = vid_alphalookup[src[x * 3 + 2] << 8 | alpha] + 
				vid_alphalookup[dst[x * 3 + 2] << 8 | negalpha];
		}

		src += params->source->pitch;
		alpha_src += params->source->alpha_pitch;
		dst += params->dest->pitch;
		y--;
	}
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


void rect_draw_565_c(struct blit_params_t *params)
{
	uint8_t *dst = get_pixel_addr(params->dest, params->dest_x, params->dest_y);
	uint16_t colour = convert_24bit_to_16bit(params->red, params->green, params->blue);
	
	int x, y = params->height;

	while(y)
	{
		for(x = 0; x < params->width; x++)
			((uint16_t*)dst)[x] = colour;

		dst += params->dest->pitch;
		y--;
	}
}


void rect_alpha_draw_565_c(struct blit_params_t *params)
{
	uint8_t *dst = get_pixel_addr(params->dest, params->dest_x, params->dest_y);
	uint16_t colour = convert_24bit_to_16bit(params->red, 
		params->green, params->blue);	// error probably induced here

	uint16_t redalpha = vid_redalphalookup[((colour & 0xf800) >> 3) | params->alpha];
	uint16_t greenalpha = vid_greenalphalookup[((colour & 0x7e0) << 3) | params->alpha];
	uint16_t bluealpha = vid_bluealphalookup[((colour & 0x1f) << 8) | params->alpha];

	uint8_t negalpha = ~params->alpha;

	int x, y = params->height;
	
	while(y)
	{
		for(x = 0; x < params->width; x++)
		{
			uint16_t oldcolour = ((uint16_t*)dst)[x];

			((uint16_t*)dst)[x] = 
				(redalpha + vid_redalphalookup[((oldcolour & 0xf800) >> 3) | negalpha]) |
				(greenalpha + vid_greenalphalookup[((oldcolour & 0x7e0) << 3) | negalpha]) |
				(bluealpha + vid_bluealphalookup[((oldcolour & 0x1f) << 8) | negalpha]);
		}

		dst += params->dest->pitch;
		y--;
	}
}


void surface_blit_A8_565_c(struct blit_params_t *params)
{
	uint8_t *alpha_src = get_alpha_pixel_addr(params->source, params->source_x, params->source_y);
	uint8_t *dst = get_pixel_addr(params->dest, params->dest_x, params->dest_y);
	uint16_t colour = convert_24bit_to_16bit(params->red, 
		params->green, params->blue);	// error probably induced here

	uint16_t red = (colour & 0xf800) >> 3;
	uint16_t green = (colour & 0x7e0) << 3;
	uint16_t blue = (colour & 0x1f) << 8;

	int x, y = params->height;

	while(y)
	{
		for(x = 0; x < params->width; x++)
		{
			uint16_t oldcolour = ((uint16_t*)dst)[x];
			uint8_t alpha = alpha_src[x];
			uint8_t negalpha = ~alpha;

			((uint16_t*)dst)[x] = (vid_redalphalookup[red | alpha] + 
				vid_redalphalookup[((oldcolour & 0xf800) >> 3) | negalpha]) |
				(vid_greenalphalookup[green | alpha] +
				vid_greenalphalookup[((oldcolour & 0x7e0) << 3) | negalpha]) |
				(vid_bluealphalookup[blue | alpha] + 
				vid_bluealphalookup[((oldcolour & 0x1f) << 8) | negalpha]);
		}

		alpha_src += params->source->alpha_pitch;
		dst += params->dest->pitch;
		y--;
	}
}


void surface_alpha_blit_A8_565_c(struct blit_params_t *params)
{
	uint8_t *alpha_src = get_alpha_pixel_addr(params->source, params->source_x, params->source_y);
	uint8_t *dst = get_pixel_addr(params->dest, params->dest_x, params->dest_y);
	uint16_t colour = convert_24bit_to_16bit(params->red, 
		params->green, params->blue);	// error probably induced here

	uint16_t red = (colour & 0xf800) >> 3;
	uint16_t green = (colour & 0x7e0) << 3;
	uint16_t blue = (colour & 0x1f) << 8;

	int x, y = params->height;

	while(y)
	{
		for(x = 0; x < params->width; x++)
		{
			uint16_t oldcolour = ((uint16_t*)dst)[x];
			uint8_t alpha = vid_alphalookup[alpha_src[x] << 8 | params->alpha];
			uint8_t negalpha = ~alpha;

			((uint16_t*)dst)[x] = (vid_redalphalookup[red | alpha] + 
				vid_redalphalookup[((oldcolour & 0xf800) >> 3) | negalpha]) |
				(vid_greenalphalookup[green | alpha] +
				vid_greenalphalookup[((oldcolour & 0x7e0) << 3) | negalpha]) |
				(vid_bluealphalookup[blue | alpha] + 
				vid_bluealphalookup[((oldcolour & 0x1f) << 8) | negalpha]);
		}

		alpha_src += params->source->alpha_pitch;
		dst += params->dest->pitch;
		y--;
	}
}


void surface_blit_888_565_c(struct blit_params_t *params)
{
	uint8_t *src = get_pixel_addr(params->source, params->source_x, params->source_y);
	uint8_t *dst = get_pixel_addr(params->dest, params->dest_x, params->dest_y);

	int x, y = params->height;

	while(y)
	{
		for(x = 0; x < params->width; x++)
			((uint16_t*)dst)[x] = convert_24bit_to_16bit(src[x * 3], 
				src[x * 3 + 1], src[x * 3 + 2]);

		src += params->source->pitch;
		dst += params->dest->pitch;
		y--;
	}
}


void surface_alpha_blit_888_565_c(struct blit_params_t *params)
{
	uint8_t *src = get_pixel_addr(params->source, params->source_x, params->source_y);
	uint8_t *dst = get_pixel_addr(params->dest, params->dest_x, params->dest_y);

	uint8_t negalpha = ~params->alpha;

	int x, y = params->height;

	while(y)
	{
		for(x = 0; x < params->width; x++)
		{
			uint16_t oldcolour = ((uint16_t*)dst)[x];
			uint16_t blendcolour = convert_24bit_to_16bit(src[x * 3], 
				src[x * 3 + 1], src[x * 3 + 2]);	// error probably induced here

			((uint16_t*)dst)[x] = 
				(vid_redalphalookup[((blendcolour & 0xf800) >> 3) | params->alpha] + 
				vid_redalphalookup[((oldcolour & 0xf800) >> 3) | negalpha]) |
				(vid_greenalphalookup[((blendcolour & 0x7e0) << 3) | params->alpha] +
				vid_greenalphalookup[((oldcolour & 0x7e0) << 3) | negalpha]) |
				(vid_bluealphalookup[((blendcolour & 0x1f) << 8) | params->alpha] + 
				vid_bluealphalookup[((oldcolour & 0x1f) << 8) | negalpha]);
		}

		src += params->source->pitch;
		dst += params->dest->pitch;
		y--;
	}
}


void surface_blit_888A8_565_c(struct blit_params_t *params)
{
	uint8_t *src = get_pixel_addr(params->source, params->source_x, params->source_y);
	uint8_t *alpha_src = get_alpha_pixel_addr(params->source, params->source_x, params->source_y);
	uint8_t *dst = get_pixel_addr(params->dest, params->dest_x, params->dest_y);

	int x, y = params->height;

	while(y)
	{
		for(x = 0; x < params->width; x++)
		{
			uint8_t alpha = alpha_src[x];
			if(alpha == 0)
				continue;
			
			uint16_t oldcolour = ((uint16_t*)dst)[x];
			uint8_t negalpha = ~alpha;
			uint16_t blendcolour = convert_24bit_to_16bit(src[x * 3], 
				src[x * 3 + 1], src[x * 3 + 2]);

			((uint16_t*)dst)[x] = (vid_redalphalookup[((blendcolour & 0xf800) >> 3) | alpha] + 
				vid_redalphalookup[((oldcolour & 0xf800) >> 3) | negalpha]) |
				(vid_greenalphalookup[((blendcolour & 0x7e0) << 3) | alpha] +
				vid_greenalphalookup[((oldcolour & 0x7e0) << 3) | negalpha]) |
				(vid_bluealphalookup[((blendcolour & 0x1f) << 8) | alpha] + 
				vid_bluealphalookup[((oldcolour & 0x1f) << 8) | negalpha]);
		}

		alpha_src += params->source->alpha_pitch;
		src += params->source->pitch;
		dst += params->dest->pitch;
		y--;
	}
}


void surface_alpha_blit_888A8_565_c(struct blit_params_t *params)
{
	uint8_t *src = get_pixel_addr(params->source, params->source_x, params->source_y);
	uint8_t *alpha_src = get_alpha_pixel_addr(params->source, params->source_x, params->source_y);
	uint8_t *dst = get_pixel_addr(params->dest, params->dest_x, params->dest_y);

	int x, y = params->height;

	while(y)
	{
		for(x = 0; x < params->width; x++)
		{
			uint8_t alpha = vid_alphalookup[alpha_src[x] << 8 | params->alpha];
			if(alpha == 0)
				continue;
			
			uint16_t oldcolour = ((uint16_t*)dst)[x];
			uint8_t negalpha = ~alpha;
			uint16_t blendcolour = convert_24bit_to_16bit(src[x * 3], 
				src[x * 3 + 1], src[x * 3 + 2]);

			((uint16_t*)dst)[x] = (vid_redalphalookup[((blendcolour & 0xf800) >> 3) | alpha] + 
				vid_redalphalookup[((oldcolour & 0xf800) >> 3) | negalpha]) |
				(vid_greenalphalookup[((blendcolour & 0x7e0) << 3) | alpha] +
				vid_greenalphalookup[((oldcolour & 0x7e0) << 3) | negalpha]) |
				(vid_bluealphalookup[((blendcolour & 0x1f) << 8) | alpha] + 
				vid_bluealphalookup[((oldcolour & 0x1f) << 8) | negalpha]);
		}

		alpha_src += params->source->alpha_pitch;
		src += params->source->pitch;
		dst += params->dest->pitch;
		y--;
	}
}


void surface_blit_565_565_c(struct blit_params_t *params)
{
	uint8_t *src = get_pixel_addr(params->source, params->source_x, params->source_y);
	uint8_t *dst = get_pixel_addr(params->dest, params->dest_x, params->dest_y);

	int y = params->height;

	while(y)
	{
		memcpy(dst, src, params->width * 2);

		src += params->source->pitch;
		dst += params->dest->pitch;
		y--;
	}
}


void surface_alpha_blit_565_565_c(struct blit_params_t *params)
{
	uint8_t *src = get_pixel_addr(params->source, params->source_x, params->source_y);
	uint8_t *dst = get_pixel_addr(params->dest, params->dest_x, params->dest_y);

	uint8_t negalpha = ~params->alpha;

	int x, y = params->height;

	while(y)
	{
		for(x = 0; x < params->width; x++)
		{
			uint16_t oldcolour = ((uint16_t*)dst)[x];
			uint16_t blendcolour = ((uint16_t*)src)[x];

			((uint16_t*)dst)[x] = 
				(vid_redalphalookup[((blendcolour & 0xf800) >> 3) | params->alpha] + 
				vid_redalphalookup[((oldcolour & 0xf800) >> 3) | negalpha]) |
				(vid_greenalphalookup[((blendcolour & 0x7e0) << 3) | params->alpha] +
				vid_greenalphalookup[((oldcolour & 0x7e0) << 3) | negalpha]) |
				(vid_bluealphalookup[((blendcolour & 0x1f) << 8) | params->alpha] + 
				vid_bluealphalookup[((oldcolour & 0x1f) << 8) | negalpha]);
		}

		src += params->source->pitch;
		dst += params->dest->pitch;
		y--;
	}
}


void surface_blit_565A8_565_c(struct blit_params_t *params)
{
	uint8_t *src = get_pixel_addr(params->source, params->source_x, params->source_y);
	uint8_t *alpha_src = get_alpha_pixel_addr(params->source, params->source_x, params->source_y);
	uint8_t *dst = get_pixel_addr(params->dest, params->dest_x, params->dest_y);

	int x, y = params->height;

	while(y)
	{
		for(x = 0; x < params->width; x++)
		{
			uint8_t alpha = alpha_src[x];
			if(alpha == 0)
				continue;
			
			uint16_t oldcolour = ((uint16_t*)dst)[x];
			uint8_t negalpha = ~alpha;
			uint16_t blendcolour = ((uint16_t*)src)[x];

			((uint16_t*)dst)[x] = (vid_redalphalookup[((blendcolour & 0xf800) >> 3) | alpha] + 
				vid_redalphalookup[((oldcolour & 0xf800) >> 3) | negalpha]) |
				(vid_greenalphalookup[((blendcolour & 0x7e0) << 3) | alpha] +
				vid_greenalphalookup[((oldcolour & 0x7e0) << 3) | negalpha]) |
				(vid_bluealphalookup[((blendcolour & 0x1f) << 8) | alpha] + 
				vid_bluealphalookup[((oldcolour & 0x1f) << 8) | negalpha]);
		}

		alpha_src += params->source->alpha_pitch;
		src += params->source->pitch;
		dst += params->dest->pitch;
		y--;
	}
}


void surface_alpha_blit_565A8_565_c(struct blit_params_t *params)
{
	uint8_t *src = get_pixel_addr(params->source, params->source_x, params->source_y);
	uint8_t *alpha_src = get_alpha_pixel_addr(params->source, params->source_x, params->source_y);
	uint8_t *dst = get_pixel_addr(params->dest, params->dest_x, params->dest_y);

	int x, y = params->height;

	while(y)
	{
		for(x = 0; x < params->width; x++)
		{
			uint8_t alpha = vid_alphalookup[alpha_src[x] << 8 | params->alpha];
			if(alpha == 0)
				continue;
			
			uint16_t oldcolour = ((uint16_t*)dst)[x];
			uint8_t negalpha = ~alpha;
			uint16_t blendcolour = ((uint16_t*)src)[x];

			((uint16_t*)dst)[x] = (vid_redalphalookup[((blendcolour & 0xf800) >> 3) | alpha] + 
				vid_redalphalookup[((oldcolour & 0xf800) >> 3) | negalpha]) |
				(vid_greenalphalookup[((blendcolour & 0x7e0) << 3) | alpha] +
				vid_greenalphalookup[((oldcolour & 0x7e0) << 3) | negalpha]) |
				(vid_bluealphalookup[((blendcolour & 0x1f) << 8) | alpha] + 
				vid_bluealphalookup[((oldcolour & 0x1f) << 8) | negalpha]);
		}

		alpha_src += params->source->alpha_pitch;
		src += params->source->pitch;
		dst += params->dest->pitch;
		y--;
	}
}


void (*pixel_alpha_plot_888P8)(struct blit_params_t *params) = pixel_alpha_plot_888_c;
void (*rect_draw_888P8)(struct blit_params_t *params) = rect_draw_888P8_c;
void (*rect_alpha_draw_888P8)(struct blit_params_t *params) = rect_alpha_draw_888P8_c;
void (*surface_blit_A8_888P8)(struct blit_params_t *params) = surface_blit_A8_888P8_c;
void (*surface_alpha_blit_A8_888P8)(struct blit_params_t *params) = surface_alpha_blit_A8_888P8_c;

void (*surface_blit_888_888P8)(struct blit_params_t *params) = surface_blit_888_888P8_c;
void (*surface_alpha_blit_888_888P8)(struct blit_params_t *params) = surface_alpha_blit_888_888P8_c;
void (*surface_blit_888A8_888P8)(struct blit_params_t *params) = surface_blit_888A8_888P8_c;
void (*surface_alpha_blit_888A8_888P8)(struct blit_params_t *params) = 
	surface_alpha_blit_888A8_888P8_c;


void (*pixel_alpha_plot_888)(struct blit_params_t *params) = pixel_alpha_plot_888_c;
void (*rect_draw_888)(struct blit_params_t *params) = rect_draw_888_c;
void (*rect_alpha_draw_888)(struct blit_params_t *params) = rect_alpha_draw_888_c;
void (*surface_blit_A8_888)(struct blit_params_t *params) = surface_blit_A8_888_c;
void (*surface_alpha_blit_A8_888)(struct blit_params_t *params) = surface_alpha_blit_A8_888_c;

void (*surface_blit_888_888)(struct blit_params_t *params) = surface_blit_888_888_c;
void (*surface_alpha_blit_888_888)(struct blit_params_t *params) = surface_alpha_blit_888_888_c;
void (*surface_blit_888A8_888)(struct blit_params_t *params) = surface_blit_888A8_888_c;
void (*surface_alpha_blit_888A8_888)(struct blit_params_t *params) = surface_alpha_blit_888A8_888_c;


void (*pixel_alpha_plot_565)(struct blit_params_t *params) = pixel_alpha_plot_565_c;
void (*rect_draw_565)(struct blit_params_t *params) = rect_draw_565_c;
void (*rect_alpha_draw_565)(struct blit_params_t *params) = rect_alpha_draw_565_c;
void (*surface_blit_A8_565)(struct blit_params_t *params) = surface_blit_A8_565_c;
void (*surface_alpha_blit_A8_565)(struct blit_params_t *params) = surface_alpha_blit_A8_565_c;

void (*surface_blit_888_565)(struct blit_params_t *params) = surface_blit_888_565_c;
void (*surface_alpha_blit_888_565)(struct blit_params_t *params) = surface_alpha_blit_888_565_c;
void (*surface_blit_888A8_565)(struct blit_params_t *params) = surface_blit_888A8_565_c;
void (*surface_alpha_blit_888A8_565)(struct blit_params_t *params) = surface_alpha_blit_888A8_565_c;

void (*surface_blit_565_565)(struct blit_params_t *params) = surface_blit_565_565_c;
void (*surface_alpha_blit_565_565)(struct blit_params_t *params) = surface_alpha_blit_565_565_c;
void (*surface_blit_565A8_565)(struct blit_params_t *params) = surface_blit_565A8_565_c;
void (*surface_alpha_blit_565A8_565)(struct blit_params_t *params) = surface_alpha_blit_565A8_565_c;


int clip_blit_coords(struct blit_params_t *params)
{
	if(params->width == 0 || params->height == 0)
		return 0;

	if((params->dest_x + params->width <= 0) || (params->dest_y + params->height <= 0) ||
		(params->dest_x >= params->dest->width) || (params->dest_y >= params->dest->height))
		return 0;

	if(params->dest_x < 0)
	{
		params->source_x -= params->dest_x;
		params->width += params->dest_x;
		params->dest_x = 0;
	}

	if(params->dest_x + params->width > params->dest->width)
		params->width = params->dest->width - params->dest_x;

	if(params->dest_y < 0)
	{
		params->source_y -= params->dest_y;
		params->height += params->dest_y;
		params->dest_y = 0;
	}

	if(params->dest_y + params->height > params->dest->height)
		params->height = params->dest->height - params->dest_y;

	return 1;
}


void plot_pixel(struct blit_params_t *params)
{
	if(!params->dest)
		return;
	
	if(params->dest_x < 0 || params->dest_x >= params->dest->width ||
		params->dest_y < 0 || params->dest_y >= params->dest->height)
		return;
	
	uint8_t *dst = get_pixel_addr(params->dest, params->dest_x, params->dest_y);
	
	switch(params->dest->flags)
	{
	case SURFACE_24BIT:
	case SURFACE_24BITALPHA8BIT:
	case SURFACE_24BITPADDING8BIT:
		dst[0] = params->red;
		dst[1] = params->green;
		dst[2] = params->blue;
		break;
	
	case SURFACE_16BIT:
	case SURFACE_16BITALPHA8BIT:
		*((uint16_t*)dst) = convert_24bit_to_16bit(params->red, params->green, params->blue);
		break;
	}
}


void alpha_plot_pixel(struct blit_params_t *params)
{
	if(params->alpha == 0)
		return;
	
	if(!params->dest)
		return;
	
	if(params->dest_x < 0 || params->dest_x >= params->dest->width ||
		params->dest_y < 0 || params->dest_y >= params->dest->height)
		return;

	switch(params->dest->flags)
	{
	case SURFACE_24BITPADDING8BIT:
		pixel_alpha_plot_888P8(params);
		break;
	
	case SURFACE_24BIT:
	case SURFACE_24BITALPHA8BIT:
		pixel_alpha_plot_888(params);
		break;
	
	case SURFACE_16BIT:
	case SURFACE_16BITALPHA8BIT:
		pixel_alpha_plot_565(params);
		break;
	}
}


void draw_rect(struct blit_params_t *params)
{
	if(!params->dest)
		return;
	
	if(!clip_blit_coords(params))
		return;

	switch(params->dest->flags)
	{
	case SURFACE_24BITPADDING8BIT:
		rect_draw_888P8(params);
		break;
	
	case SURFACE_24BIT:
	case SURFACE_24BITALPHA8BIT:
		rect_draw_888(params);
		break;
	
	case SURFACE_16BIT:
	case SURFACE_16BITALPHA8BIT:
		rect_draw_565(params);
		break;
	}
}


void alpha_draw_rect(struct blit_params_t *params)
{
	if(params->alpha == 0)
		return;
	
	if(!params->dest)
		return;
	
	if(!clip_blit_coords(params))
		return;

	switch(params->dest->flags)
	{
	case SURFACE_24BITPADDING8BIT:
		rect_alpha_draw_888P8(params);
		break;
	
	case SURFACE_24BIT:
	case SURFACE_24BITALPHA8BIT:
		rect_alpha_draw_888(params);
		break;
	
	case SURFACE_16BIT:
	case SURFACE_16BITALPHA8BIT:
		rect_alpha_draw_565(params);
		break;
	}
}


void surface_blit(struct blit_params_t *params)
{
	if(!clip_blit_coords(params))
		return;
	
	switch(params->source->flags)
	{
	case SURFACE_ALPHA8BIT:
		
		switch(params->dest->flags)
		{
		case SURFACE_24BITPADDING8BIT:
			surface_blit_A8_888P8(params);
			break;
		
		case SURFACE_24BIT:
		case SURFACE_24BITALPHA8BIT:
			surface_blit_A8_888(params);
			break;
		
		case SURFACE_16BIT:
		case SURFACE_16BITALPHA8BIT:
			surface_blit_A8_565(params);
			break;
		}
		
		break;


	case SURFACE_24BIT:
		
		switch(params->dest->flags)
		{
		case SURFACE_24BITPADDING8BIT:
			surface_blit_888_888P8(params);
			break;
		
		case SURFACE_24BIT:
		case SURFACE_24BITALPHA8BIT:
			surface_blit_888_888(params);
			break;
		
		case SURFACE_16BIT:
		case SURFACE_16BITALPHA8BIT:
			surface_blit_888_565(params);
			break;
		}
		
		break;

		
	case SURFACE_24BITALPHA8BIT:
		
		switch(params->dest->flags)
		{
		case SURFACE_24BITPADDING8BIT:
			surface_blit_888A8_888P8(params);
			break;
		
		case SURFACE_24BIT:
		case SURFACE_24BITALPHA8BIT:
			surface_blit_888A8_888(params);
			break;
		
		case SURFACE_16BIT:
		case SURFACE_16BITALPHA8BIT:
			surface_blit_888A8_565(params);
			break;
		}
		
		break;

		
	case SURFACE_16BIT:
		
		switch(params->dest->flags)
		{
		case SURFACE_16BIT:
		case SURFACE_16BITALPHA8BIT:
			surface_blit_565_565(params);
			break;
		}
		
		break;
	
	
	case SURFACE_16BITALPHA8BIT:
		
		switch(params->dest->flags)
		{
		case SURFACE_16BIT:
		case SURFACE_16BITALPHA8BIT:
			surface_blit_565A8_565(params);
			break;
		}
		
		break;
	}
}	


void blit_surface(struct blit_params_t *params)
{
	if(!params->source || !params->dest)
		return;
	
	params->source_x = 0;
	params->source_y = 0;
	params->width = params->source->width;
	params->height = params->source->height;

	surface_blit(params);
}


void blit_partial_surface(struct blit_params_t *params)
{
	if(!params->source || !params->dest)
		return;
	
	surface_blit(params);
}


void surface_alpha_blit(struct blit_params_t *params)
{
	if(!clip_blit_coords(params))
		return;
	
	switch(params->source->flags)
	{
	case SURFACE_ALPHA8BIT:
		
		switch(params->dest->flags)
		{
		case SURFACE_24BITPADDING8BIT:
			surface_alpha_blit_A8_888P8(params);
			break;
		
		case SURFACE_24BIT:
		case SURFACE_24BITALPHA8BIT:
			surface_alpha_blit_A8_888(params);
			break;
		
		case SURFACE_16BIT:
		case SURFACE_16BITALPHA8BIT:
			surface_alpha_blit_A8_565(params);
			break;
		}
		
		break;

		
	case SURFACE_24BIT:
		
		switch(params->dest->flags)
		{
		case SURFACE_24BITPADDING8BIT:
			surface_alpha_blit_888_888P8(params);
			break;
		
		case SURFACE_24BIT:
		case SURFACE_24BITALPHA8BIT:
			surface_alpha_blit_888_888(params);
			break;
		
		case SURFACE_16BIT:
		case SURFACE_16BITALPHA8BIT:
			surface_alpha_blit_888_565(params);
			break;
		}
		
		break;

		
	case SURFACE_24BITALPHA8BIT:
		
		switch(params->dest->flags)
		{
		case SURFACE_24BITPADDING8BIT:
			surface_alpha_blit_888A8_888P8(params);
			break;
		
		case SURFACE_24BIT:
		case SURFACE_24BITALPHA8BIT:
			surface_alpha_blit_888A8_888(params);
			break;
		
		case SURFACE_16BIT:
		case SURFACE_16BITALPHA8BIT:
			surface_alpha_blit_888A8_565(params);
			break;
		}
		
		break;

		
	case SURFACE_16BIT:
		
		switch(params->dest->flags)
		{
		case SURFACE_16BIT:
		case SURFACE_16BITALPHA8BIT:
			surface_alpha_blit_565_565(params);
			break;
		}
		
		break;
	
	
	case SURFACE_16BITALPHA8BIT:
		
		switch(params->dest->flags)
		{
		case SURFACE_16BIT:
		case SURFACE_16BITALPHA8BIT:
			surface_alpha_blit_565A8_565(params);
			break;
		}
		
		break;
	}
}


void alpha_blit_surface(struct blit_params_t *params)
{
	if(params->alpha == 0)
		return;
	
	if(!params->source || !params->dest)
		return;
	
	params->source_x = 0;
	params->source_y = 0;
	params->width = params->source->width;
	params->height = params->source->height;

	surface_alpha_blit(params);
}


void alpha_blit_partial_surface(struct blit_params_t *params)
{
	if(params->alpha == 0)
		return;
	
	if(!params->source || !params->dest)
		return;
	
	surface_alpha_blit(params);
}
