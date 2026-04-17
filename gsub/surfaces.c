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

#include <stdlib.h>
#include <stdint.h>
#include <math.h>
#include <string.h>

#include <zlib.h>
#include <png.h>

#ifdef USE_GDK_PIXBUF
#include <gdk-pixbuf/gdk-pixbuf.h>
#endif

#include "stringbuf.h"
#include "vertex.h"
#include "gsub.h"

#ifdef _WIN32
#include "math.h"
#endif


void convert_alphafloats_to_alpha8bit_array(float *src, uint8_t *dst, int num)
{
	while(num--)
		*dst++ = (uint8_t)(lround(*src++ * 255.0));
}


void convert_floats_to_16bit_array(float *src, uint16_t *dst, int num)
{
	while(num--)
	{
		*dst++ = (((uint16_t)(lround(src[0] * 31.0))) << 11) | 
			((((uint16_t)(lround(src[1] * 63.0))) & 0x3f) << 5) | 
			(((uint16_t)(lround(src[2] * 31.0))) & 0x1f);

		src += 3;
	}
}


void convert_floats_to_24bit_array(float *src, uint8_t *dst, int num)
{
	while(num--)
	{
		*dst++ = (uint8_t)(lround(*src++ * 255.0));
		*dst++ = (uint8_t)(lround(*src++ * 255.0));
		*dst++ = (uint8_t)(lround(*src++ * 255.0));
	}
}


void convert_24bit_to_16bit_array(uint8_t *src, uint16_t *dst, int num)
{
	int r, d, q;
	
	while(num--)
	{
		r = *src++ * 31;
		d = r / 255;
		q = r % 255;

		uint16_t o;

		if(q < 128)
			o = d << 11;
		else
			o = (d + 1) << 11;

		r = *src++ * 63;
		d = r / 255;
		q = r % 255;

		if(q < 128)
			o |= d << 5;
		else
			o |= (d + 1) << 5;

		r = *src++ * 31;
		d = r / 255;
		q = r % 255;

		if(q < 128)
			o |= d;
		else
			o |= d + 1;

		*dst++ = o;
	}
}


void convert_24bitpad8bit_to_16bit_array(uint8_t *src, uint16_t *dst, int num)
{
	int r, d, q;
	
	while(num--)
	{
		r = *src++ * 31;
		d = r / 255;
		q = r % 255;

		uint16_t o;

		if(q < 128)
			o = d << 11;
		else
			o = (d + 1) << 11;

		r = *src++ * 63;
		d = r / 255;
		q = r % 255;

		if(q < 128)
			o |= d << 5;
		else
			o |= (d + 1) << 5;

		r = *src++ * 31;
		d = r / 255;
		q = r % 255;

		if(q < 128)
			o |= d;
		else
			o |= d + 1;

		*dst++ = o;
		src++;
	}
}


void convert_24bitpad8bit_to_24bit_array(uint8_t *src, uint8_t *dst, int num)
{
	while(num--)
	{
		*dst++ = src[2];
		*dst++ = src[1];
		*dst++ = src[0];
		src += 4;
	}
}


void convert_16bit_to_24bit_array(uint16_t *src, uint8_t *dst, int num)
{
	int r, d, q;
	
	while(num--)
	{
		r = ((*src) >> 11) * 255;
		d = r / 31;
		q = r % 31;

		if(q < 16)
			*dst++ = d;
		else
			*dst++ = d + 1;

		r = (((*src) >> 5) & 0x3f) * 255;
		d = r / 63;
		q = r % 63;

		if(q < 32)
			*dst++ = d;
		else
			*dst++ = d + 1;

		r = ((*src++) & 0x1f) * 255;
		d = r / 31;
		q = r % 31;

		if(q < 16)
			*dst++ = d;
		else
			*dst++ = d + 1;
	}
}


void convert_24bit_to_floats_array(uint8_t *src, float *dst, int num)
{
	while(num--)
	{
		*dst++ = (float)*src++ / 255.0;	// red
		*dst++ = (float)*src++ / 255.0;	// green
		*dst++ = (float)*src++ / 255.0;	// blue
	}
}


void convert_8bit_to_floats_array(uint8_t *src, float *dst, int num)
{
	while(num--)
		*dst++ = (float)*src++ / 255.0;
}


void make_8bit_solid_array(uint8_t *dst, int num)
{
	while(num--)
		*dst++ = 0xff;
}


int get_pitch(int flags, int width)
{
	switch(flags)
	{
	case SURFACE_16BIT:
	case SURFACE_16BITALPHA8BIT:
		return width * 2;

	case SURFACE_24BIT:
	case SURFACE_24BITALPHA8BIT:
		return width * 3;

	case SURFACE_24BITPADDING8BIT:
		return width * 4;
	
	case SURFACE_FLOATS:
	case SURFACE_FLOATSALPHAFLOATS:
		return width * 12;

	default:
		return 0;
	}
}


int get_alpha_pitch(int flags, int width)
{
	switch(flags)
	{
	case SURFACE_ALPHA8BIT:
	case SURFACE_16BITALPHA8BIT:
	case SURFACE_24BITALPHA8BIT:
		return width;

	case SURFACE_ALPHAFLOATS:
	case SURFACE_FLOATSALPHAFLOATS:
		return width * 4;

	default:
		return 0;
	}
}


int get_size(int flags, int width, int height)
{
	switch(flags)
	{
	case SURFACE_16BIT:
	case SURFACE_16BITALPHA8BIT:
		return width * height * 2;

	case SURFACE_24BIT:
	case SURFACE_24BITALPHA8BIT:
		return width * height * 3;

	case SURFACE_24BITPADDING8BIT:
		return width * height * 4;
	
	case SURFACE_FLOATS:
	case SURFACE_FLOATSALPHAFLOATS:
		return width * height * 12;

	default:
		return 0;
	}
}


int get_alpha_size(int flags, int width, int height)
{
	switch(flags)
	{
	case SURFACE_ALPHA8BIT:
	case SURFACE_16BITALPHA8BIT:
	case SURFACE_24BITALPHA8BIT:
		return width * height;

	case SURFACE_ALPHAFLOATS:
	case SURFACE_FLOATSALPHAFLOATS:
		return width * height * 4;

	default:
		return 0;
	}
}


void *get_pixel_addr(struct surface_t *surface, int x, int y)
{
	switch(surface->flags)
	{
	case SURFACE_16BIT:
	case SURFACE_16BITALPHA8BIT:
		return &surface->buf[y * surface->pitch + x * 2];

	case SURFACE_24BIT:
	case SURFACE_24BITALPHA8BIT:
		return &surface->buf[y * surface->pitch + x * 3];

	case SURFACE_24BITPADDING8BIT:
		return &surface->buf[y * surface->pitch + x * 4];
	
	case SURFACE_FLOATS:
	case SURFACE_FLOATSALPHAFLOATS:
		return &surface->buf[y * surface->pitch + x * 12];
		
	default:
		return NULL;
	}
}


void *get_alpha_pixel_addr(struct surface_t *surface, int x, int y)
{
	switch(surface->flags)
	{
	case SURFACE_ALPHA8BIT:
	case SURFACE_16BITALPHA8BIT:
	case SURFACE_24BITALPHA8BIT:
		return &surface->alpha_buf[y * surface->alpha_pitch + x];
	
	case SURFACE_ALPHAFLOATS:
	case SURFACE_FLOATSALPHAFLOATS:
		return &surface->alpha_buf[y * surface->alpha_pitch + x * 4];
		
	default:
		return NULL;
	}
}	


struct surface_t *new_surface(int flags, int width, int height)
{
	struct surface_t *surface = malloc(sizeof(struct surface_t));
	if(!surface)
		goto error;
	
	surface->flags = flags;
	surface->pitch = get_pitch(flags, width);
	surface->alpha_pitch = get_alpha_pitch(flags, width);
	surface->width = width;
	surface->height = height;
	surface->buf = NULL;
	surface->alpha_buf = NULL;
	
	int size = get_size(flags, width, height);
	if(size)
	{
		surface->buf = malloc(size);
	
		if(!surface->buf)
			goto error;
	}
	
	int alpha_size = get_alpha_size(flags, width, height);
	if(alpha_size)
	{
		surface->alpha_buf = malloc(alpha_size);
	
		if(!surface->alpha_buf)
			goto error;
	}
	
	return surface;
	
error:
	
	if(surface)
	{
		free(surface->buf);
		free(surface->alpha_buf);
		free(surface);
	}
	
	return NULL;
}


struct surface_t *new_surface_no_buf(int flags, int width, int height)
{
	struct surface_t *surface = malloc(sizeof(struct surface_t));
	if(!surface)
		goto error;
	
	surface->flags = flags;
	surface->pitch = get_pitch(flags, width);
	surface->alpha_pitch = get_alpha_pitch(flags, width);
	surface->width = width;
	surface->height = height;
	surface->buf = NULL;
	surface->alpha_buf = NULL;
	
	return surface;
	
error:
	
	if(surface)
	{
		free(surface->buf);
		free(surface->alpha_buf);
		free(surface);
	}
	
	return NULL;
}


void clear_surface(struct surface_t *surface)
{
	if(!surface)
		return;
	
	if(surface->buf)
	{
		int perfect_pitch = get_pitch(surface->flags, surface->width);
		
		if(surface->pitch == perfect_pitch)
		{
			memset(surface->buf, 0, perfect_pitch * surface->height);
		}
		else
		{
			uint8_t *dst = surface->buf;
			int y = surface->height;
			
			while(y)
			{
				memset(dst, 0, perfect_pitch);
				dst += surface->pitch;
				y--;
			}
		}
	}

	if(surface->alpha_buf)
	{
		int perfect_pitch = get_alpha_pitch(surface->flags, surface->width);
		
		if(surface->alpha_pitch == perfect_pitch)
		{
			memset(surface->alpha_buf, 0, perfect_pitch * surface->height);
		}
		else
		{
			uint8_t *dst = surface->alpha_buf;
			int y = surface->height;
			
			while(y)
			{
				memset(dst, 0, perfect_pitch);
				dst += surface->alpha_pitch;
				y--;
			}
		}
	}
}


void convert_surface_to_alpha8bit(struct surface_t *surface)
{
	if(!surface)
		return;

	if(surface->flags == SURFACE_ALPHA8BIT)
		return;

	uint8_t *new_alpha_buf = malloc(get_alpha_size(SURFACE_ALPHA8BIT, 
		surface->width, surface->height));
	if(!new_alpha_buf)
		return;

	switch(surface->flags)
	{
	case SURFACE_ALPHAFLOATS:
		convert_alphafloats_to_alpha8bit_array((float*)surface->alpha_buf, new_alpha_buf, 
			surface->width * surface->height);
		break;
	}

	free(surface->buf);
	surface->buf = NULL;
	free(surface->alpha_buf);
	surface->alpha_buf = new_alpha_buf;
	surface->flags = SURFACE_ALPHA8BIT;
	surface->pitch = get_pitch(surface->flags, surface->width);
	surface->alpha_pitch = get_alpha_pitch(surface->flags, surface->width);
}


void convert_surface_to_16bit(struct surface_t *surface)
{
	if(!surface)
		return;

	if(surface->flags == SURFACE_16BIT)
		return;

	uint8_t *new_buf = malloc(get_size(SURFACE_16BIT, surface->width, surface->height));
	if(!new_buf)
		return;

	switch(surface->flags)
	{
	case SURFACE_24BIT:
		convert_24bit_to_16bit_array(surface->buf, (uint16_t*)new_buf, 
			surface->width * surface->height);
		break;

	case SURFACE_FLOATS:
		convert_floats_to_16bit_array((float*)surface->buf, (uint16_t*)new_buf, 
			surface->width * surface->height);
		break;
	}

	free(surface->buf);
	surface->buf = new_buf;
	free(surface->alpha_buf);
	surface->alpha_buf = NULL;
	surface->flags = SURFACE_16BIT;
	surface->pitch = get_pitch(surface->flags, surface->width);
	surface->alpha_pitch = get_alpha_pitch(surface->flags, surface->width);
}


void convert_surface_to_16bitalpha8bit(struct surface_t *surface)
{
	if(!surface)
		return;

	if(surface->flags == SURFACE_16BITALPHA8BIT)
		return;

	uint8_t *new_buf = malloc(get_size(SURFACE_16BITALPHA8BIT, surface->width, surface->height));
	if(!new_buf)
		return;

	switch(surface->flags)
	{
	case SURFACE_24BITALPHA8BIT:
		convert_24bit_to_16bit_array(surface->buf, (uint16_t*)new_buf, 
			surface->width * surface->height);
		break;
	
	
	case SURFACE_FLOATSALPHAFLOATS:
		
		convert_floats_to_16bit_array((float*)surface->buf, (uint16_t*)new_buf,
			surface->width * surface->height);
	
		uint8_t *new_alpha_buf = malloc(get_alpha_size(SURFACE_16BITALPHA8BIT, 
			surface->width, surface->height));
	
		convert_alphafloats_to_alpha8bit_array((float*)surface->alpha_buf, new_alpha_buf,
			surface->width * surface->height);
	
		free(surface->alpha_buf);
		surface->alpha_buf = new_alpha_buf;
	
		break;
	}

	free(surface->buf);
	surface->buf = new_buf;
	surface->flags = SURFACE_16BITALPHA8BIT;
	surface->pitch = get_pitch(surface->flags, surface->width);
	surface->alpha_pitch = get_alpha_pitch(surface->flags, surface->width);
}


void convert_surface_to_24bitalpha8bit(struct surface_t *surface)
{
	if(!surface)
		return;
	
	if(surface->flags == SURFACE_24BITALPHA8BIT)
		return;

	uint8_t *new_alpha_buf = malloc(get_alpha_size(SURFACE_24BITALPHA8BIT, 
		surface->width, surface->height));
	
	if(!new_alpha_buf)
		return;
	
	switch(surface->flags)
	{
	case SURFACE_24BIT:
	
		make_8bit_solid_array(new_alpha_buf, get_alpha_size(SURFACE_24BITALPHA8BIT, 
			surface->width, surface->height));
		break;
	
	case SURFACE_FLOATSALPHAFLOATS:
	{	
	
		uint8_t *new_buf = malloc(get_size(SURFACE_24BITALPHA8BIT, 
			surface->width, surface->height));
		
		if(!new_buf)
		{
			free(new_alpha_buf);
			return;
		}
	
		convert_floats_to_24bit_array((float*)surface->buf, new_buf,
			surface->width * surface->height);
	
		convert_alphafloats_to_alpha8bit_array((float*)surface->alpha_buf, new_alpha_buf,
			surface->width * surface->height);
	
		free(surface->buf);
		surface->buf = new_buf;
		
	}
		
		break;
	}
	
	free(surface->alpha_buf);
	surface->alpha_buf = new_alpha_buf;
	surface->flags = SURFACE_24BITALPHA8BIT;
	surface->pitch = get_pitch(surface->flags, surface->width);
	surface->alpha_pitch = get_alpha_pitch(surface->flags, surface->width);
}


void convert_surface_to_24bit(struct surface_t* surface)
{
	if(!surface)
		return;

	if(surface->flags == SURFACE_24BIT)
		return;

	uint8_t *new_buf = malloc(get_size(SURFACE_24BIT, surface->width, surface->height));
	if(!new_buf)
		return;

	switch(surface->flags)
	{
	case SURFACE_16BIT:
		convert_16bit_to_24bit_array((uint16_t*)surface->buf, new_buf, 
			surface->width * surface->height);
		break;
	}

	free(surface->buf);
	surface->buf = new_buf;
	free(surface->alpha_buf);
	surface->alpha_buf = NULL;
	surface->flags = SURFACE_24BIT;
	surface->pitch = get_pitch(surface->flags, surface->width);
	surface->alpha_pitch = get_alpha_pitch(surface->flags, surface->width);
}


struct surface_t *duplicate_surface_to_24bit(struct surface_t *in)
{
	if(!in)
		return NULL;

	struct surface_t *out = new_surface(SURFACE_24BIT, in->width, in->height);
	if(!out)
		return NULL;
	
	switch(in->flags)
	{
	case SURFACE_24BITPADDING8BIT:
		convert_24bitpad8bit_to_24bit_array((uint8_t*)in->buf, out->buf, 
			in->width * in->height);
		break;
		
	case SURFACE_16BIT:
		convert_16bit_to_24bit_array((uint16_t*)in->buf, out->buf, 
			in->width * in->height);
		break;
	}
	
	return out;
}


void convert_surface_to_floats(struct surface_t* surface)
{
	if(surface->flags == SURFACE_FLOATS)
		return;

	uint8_t *new_buf = malloc(get_size(SURFACE_FLOATS, surface->width, surface->height));
	if(!new_buf)
		return;

	switch(surface->flags)
	{
	case SURFACE_24BIT:
		convert_24bit_to_floats_array(surface->buf, (float*)new_buf, 
			surface->width * surface->height);
		break;
	}

	free(surface->buf);
	surface->buf = new_buf;
	free(surface->alpha_buf);
	surface->alpha_buf = NULL;
	surface->flags = SURFACE_FLOATS;
	surface->pitch = get_pitch(surface->flags, surface->width);
	surface->alpha_pitch = get_alpha_pitch(surface->flags, surface->width);
}


void convert_surface_to_floatsalphafloats(struct surface_t* surface)
{
	if(surface->flags == SURFACE_FLOATSALPHAFLOATS)
		return;

	uint8_t *new_buf = malloc(get_size(SURFACE_FLOATSALPHAFLOATS, surface->width, surface->height));
	if(!new_buf)
		return;

	uint8_t *new_alpha_buf = malloc(get_alpha_size(SURFACE_FLOATSALPHAFLOATS, 
		surface->width, surface->height));
	
	if(!new_alpha_buf)
	{
		free(new_buf);
		return;
	}

	switch(surface->flags)
	{
	case SURFACE_24BITALPHA8BIT:
		
		convert_24bit_to_floats_array(surface->buf, (float*)new_buf, 
			surface->width * surface->height);
	
		convert_8bit_to_floats_array(surface->alpha_buf, (float*)new_alpha_buf, 
				surface->width * surface->height);
		break;
	}

	free(surface->buf);
	surface->buf = new_buf;
	free(surface->alpha_buf);
	surface->alpha_buf = new_alpha_buf;
	surface->flags = SURFACE_FLOATS;
	surface->pitch = get_pitch(surface->flags, surface->width);
	surface->alpha_pitch = get_alpha_pitch(surface->flags, surface->width);
}


struct surface_t *duplicate_surface(struct surface_t *in)	// doesn't take account of pitch
{
	if(!in)
		return NULL;

	struct surface_t *out = new_surface(in->flags, in->width, in->height);
	if(!out)
		return NULL;
	
	int size = get_size(in->flags, in->width, in->height);
	if(size)
		memcpy(out->buf, in->buf, size);
	
	int alpha_size = get_alpha_size(in->flags, in->width, in->height);
	if(alpha_size)
		memcpy(out->alpha_buf, in->alpha_buf, alpha_size);

	return out;
}


void surface_flip_horiz(struct surface_t *surface)
{
	if(!surface)
		return;

	if(surface->flags != SURFACE_16BIT && surface->flags != SURFACE_16BITALPHA8BIT &&
		surface->flags != SURFACE_24BIT && surface->flags != SURFACE_24BITALPHA8BIT &&
		surface->flags != SURFACE_ALPHA8BIT)
		return;

	int x, y;
	
	switch(surface->flags)
	{
	case SURFACE_16BIT:
	case SURFACE_16BITALPHA8BIT:
		{
			uint8_t *out = malloc(get_size(surface->flags, surface->width, surface->height));
			uint16_t *dst = (uint16_t*)out;
	
			for(y = 0; y < surface->height; y++)
			{
				for(x = 0; x < surface->width; x++)
				{
					*dst++ = ((uint16_t*)surface->buf)[(y + 1) * surface->width - 1 - x];
				}
			}
		
			free(surface->buf);
			surface->buf = out;
		}
		break;
	
	case SURFACE_24BIT:
	case SURFACE_24BITALPHA8BIT:
		{
			uint8_t *out = malloc(get_size(surface->flags, surface->width, surface->height));
			uint8_t *dst = out;
	
			for(y = 0; y < surface->height; y++)
			{
				for(x = 0; x < surface->width; x++)
				{
					uint8_t *src = &(surface->buf[((y + 1) * surface->width - 1 - x) * 3]);
					
					*dst++ = *src++;
					*dst++ = *src++;
					*dst++ = *src;
				}
			}
		
			free(surface->buf);
			surface->buf = out;
		}
		break;
	}
	
	switch(surface->flags)
	{
	case SURFACE_ALPHA8BIT:
	case SURFACE_16BITALPHA8BIT:
	case SURFACE_24BITALPHA8BIT:
		{
			uint8_t *alpha_out = malloc(get_alpha_size(surface->flags, 
				surface->width, surface->height));
			uint8_t *dst = alpha_out;
			
			for(y = 0; y < surface->height; y++)
			{
				for(x = 0; x < surface->width; x++)
				{
					*dst++ = (surface->alpha_buf)[(y + 1) * surface->width - 1 - x];
				}
			}
		
			free(surface->alpha_buf);
			surface->alpha_buf = alpha_out;
		}
		break;
	}
}


void surface_flip_vert(struct surface_t *surface)
{
	if(!surface)
		return;

	if(surface->flags != SURFACE_16BIT && surface->flags != SURFACE_16BITALPHA8BIT &&
		surface->flags != SURFACE_24BIT && surface->flags != SURFACE_24BITALPHA8BIT &&
		surface->flags != SURFACE_ALPHA8BIT)
		return;

	int y;
	
	switch(surface->flags)
	{
	case SURFACE_16BIT:
	case SURFACE_16BITALPHA8BIT:
		{
			uint8_t *out = malloc(get_size(surface->flags, surface->width, surface->height));
			
			for(y = 0; y < surface->height; y++)
			{
				memcpy(&((uint16_t*)out)[y * surface->width], 
					&((uint16_t*)surface->buf)[(surface->height - 1 - y) * surface->width], 
					surface->width * 2);
			}
		
			free(surface->buf);
			surface->buf = out;
		}
		break;
	
	case SURFACE_24BIT:
	case SURFACE_24BITALPHA8BIT:
		{
			uint8_t *out = malloc(get_size(surface->flags, surface->width, surface->height));
			
			for(y = 0; y < surface->height; y++)
			{
				memcpy(&out[y * surface->width * 3], &surface->buf[(surface->height - 1 - y) * 
					surface->width * 3], surface->width * 3);
			}
		
			free(surface->buf);
			surface->buf = out;
		}
		break;
	}
	
	switch(surface->flags)
	{
	case SURFACE_ALPHA8BIT:
	case SURFACE_16BITALPHA8BIT:
	case SURFACE_24BITALPHA8BIT:
		{
			uint8_t *alpha_out = malloc(get_alpha_size(surface->flags, 
				surface->width, surface->height));
			
			for(y = 0; y < surface->height; y++)
			{
				memcpy(&alpha_out[y * surface->width], 
					&(surface->alpha_buf)[(surface->height - 1 - y) * surface->width], 
					surface->width);
			}
		
			free(surface->alpha_buf);
			surface->alpha_buf = alpha_out;
		}
		break;
	}
}


void surface_rotate_left(struct surface_t *surface)
{
	if(!surface)
		return;

	if(surface->flags != SURFACE_16BIT && surface->flags != SURFACE_16BITALPHA8BIT &&
		surface->flags != SURFACE_24BIT && surface->flags != SURFACE_24BITALPHA8BIT &&
		surface->flags != SURFACE_ALPHA8BIT)
		return;

	int x, y;
	
	switch(surface->flags)
	{
	case SURFACE_16BIT:
	case SURFACE_16BITALPHA8BIT:
		{
			uint8_t *out = malloc(get_size(surface->flags, surface->width, surface->height));
			uint16_t *dst = (uint16_t*)out;
	
			for(y = 0; y < surface->width; y++)
			{
				for(x = 0; x < surface->height; x++)
				{
					*dst++ = ((uint16_t*)surface->buf)[x * surface->width + 
						surface->width - 1 - y];
				}
			}
		
			free(surface->buf);
			surface->buf = out;
		}
		break;
	
	case SURFACE_24BIT:
	case SURFACE_24BITALPHA8BIT:
		{
			uint8_t *out = malloc(get_size(surface->flags, surface->width, surface->height));
			uint8_t *dst = out;
	
			for(y = 0; y < surface->width; y++)
			{
				for(x = 0; x < surface->height; x++)
				{
					uint8_t *src = &(surface->buf[(x * surface->width + 
						surface->width - 1 - y) * 3]);
					
					*dst++ = *src++;
					*dst++ = *src++;
					*dst++ = *src;
				}
			}
		
			free(surface->buf);
			surface->buf = out;
		}
		break;
	}
	
	switch(surface->flags)
	{
	case SURFACE_ALPHA8BIT:
	case SURFACE_16BITALPHA8BIT:
	case SURFACE_24BITALPHA8BIT:
		{
			uint8_t *alpha_out = malloc(get_alpha_size(surface->flags, 
				surface->width, surface->height));
			uint8_t *dst = alpha_out;
			
			for(y = 0; y < surface->width; y++)
			{
				for(x = 0; x < surface->height; x++)
				{
					*dst++ = surface->alpha_buf[x * surface->width + 
						surface->width - 1 - y];
				}
			}
		
			free(surface->alpha_buf);
			surface->alpha_buf = alpha_out;
		}
		break;
	}
	
	int temp = surface->width;
	surface->width = surface->height;
	surface->height = temp;
	surface->pitch = get_pitch(surface->flags, surface->width);
	surface->alpha_pitch = get_alpha_pitch(surface->flags, surface->width);
}


void surface_rotate_right(struct surface_t *surface)
{
	if(!surface)
		return;

	if(surface->flags != SURFACE_16BIT && surface->flags != SURFACE_16BITALPHA8BIT &&
		surface->flags != SURFACE_24BIT && surface->flags != SURFACE_24BITALPHA8BIT &&
		surface->flags != SURFACE_ALPHA8BIT)
		return;

	int x, y;
	
	switch(surface->flags)
	{
	case SURFACE_16BIT:
	case SURFACE_16BITALPHA8BIT:
		{
			uint8_t *out = malloc(get_size(surface->flags, surface->width, surface->height));
			uint16_t *dst = (uint16_t*)out;
	
			for(y = 0; y < surface->width; y++)
			{
				for(x = 0; x < surface->height; x++)
				{
					*dst++ = ((uint16_t*)surface->buf)[(surface->height - 1 - x) * 
						surface->width + y];
				}
			}
		
			free(surface->buf);
			surface->buf = out;
		}
		break;
	
	case SURFACE_24BIT:
	case SURFACE_24BITALPHA8BIT:
		{
			uint8_t *out = malloc(get_size(surface->flags, surface->width, surface->height));
			uint8_t *dst = out;
	
			for(y = 0; y < surface->width; y++)
			{
				for(x = 0; x < surface->height; x++)
				{
					uint8_t *src = &surface->buf[((surface->height - 1 - x) * 
						surface->width + y) * 3];
					
					*dst++ = *src++;
					*dst++ = *src++;
					*dst++ = *src;
				}
			}
		
			free(surface->buf);
			surface->buf = out;
		}
		break;
	}
	
	switch(surface->flags)
	{
	case SURFACE_ALPHA8BIT:
	case SURFACE_16BITALPHA8BIT:
	case SURFACE_24BITALPHA8BIT:
		{
			uint8_t *alpha_out = malloc(get_alpha_size(surface->flags, 
				surface->width, surface->height));
			uint8_t *dst = alpha_out;
			
			for(y = 0; y < surface->width; y++)
			{
				for(x = 0; x < surface->height; x++)
				{
					*dst++ = surface->alpha_buf[(surface->height - 1 - x) * 
						surface->width + y];
				}
			}
		
			free(surface->alpha_buf);
			surface->alpha_buf = alpha_out;
		}
		break;
	}
	
	int temp = surface->width;
	surface->width = surface->height;
	surface->height = temp;
	surface->pitch = get_pitch(surface->flags, surface->width);
	surface->alpha_pitch = get_alpha_pitch(surface->flags, surface->width);
}


void surface_slide_horiz(struct surface_t *surface, int pixels)
{
	if(!surface)
		return;

	if(surface->flags != SURFACE_16BIT)
		return;

	uint16_t *out = malloc(get_size(surface->flags, surface->width, surface->height));
	if(!out)
		return;
	
	int *src_xs = malloc(sizeof(int) * surface->width);
	if(!src_xs)
	{
		free(out);
		return;
	}
	
	int x, y;
	
	for(x = 0; x < surface->width; x++)
	{
		int src_x = (x - pixels) % surface->width;
		
		if(src_x < 0)
			src_x += surface->width;
		
		src_xs[x] = src_x;
	}

	for(y = 0; y < surface->height; y++)
	{
		for(x = 0; x < surface->width; x++)
		{
			out[y * surface->width + x] = ((uint16_t*)surface->buf)[y * surface->width + src_xs[x]];
		}
	}

	free(src_xs);
	free(surface->buf);
	surface->buf = (uint8_t*)out;
}


void surface_slide_vert(struct surface_t *surface, int pixels)
{
	if(!surface)
		return;

	if(surface->flags != SURFACE_16BIT)
		return;

	uint16_t *out = malloc(get_size(surface->flags, surface->width, surface->height));
	if(!out)
		return;
	
	int y;

	for(y = 0; y < surface->height; y++)
	{
		int src_y = (y - pixels) % surface->height;
		
		if(src_y < 0)
			src_y += surface->height;
		
		memcpy(&out[y * surface->width], &((uint16_t*)surface->buf)[src_y * surface->width], 
			surface->width * 2);
	}

	free(surface->buf);
	surface->buf = (uint8_t*)out;
}


void free_surface(struct surface_t *surface)
{
	if(!surface)
		return;

	free(surface->buf);
	free(surface->alpha_buf);
	free(surface);
}


struct surface_t *read_png(FILE *file)
{
	uint8_t header[8];
	fread(header, 1, 8, file);
	if(png_sig_cmp(header, 0, 8))
		return NULL;

	png_structp png_ptr = png_create_read_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
	if(!png_ptr)
		return NULL;

	png_infop info_ptr = png_create_info_struct(png_ptr);
	if(!info_ptr)
	{
		png_destroy_read_struct(&png_ptr, NULL, NULL);
		return NULL;
	}

	png_init_io(png_ptr, file);
	png_set_sig_bytes(png_ptr, 8);
	png_read_png(png_ptr, info_ptr, PNG_TRANSFORM_IDENTITY, NULL);

	uint32_t width = png_get_image_width(png_ptr, info_ptr);
	uint32_t height = png_get_image_height(png_ptr, info_ptr);
	uint8_t bit_depth = png_get_bit_depth(png_ptr, info_ptr);
	uint8_t color_type = png_get_color_type(png_ptr, info_ptr);
	
	uint8_t **row_pointers = png_get_rows(png_ptr, info_ptr);

	if(bit_depth != 8)
	{
		png_destroy_read_struct(&png_ptr, &info_ptr, NULL);
		return NULL;
	}

	struct surface_t *surface = NULL;
	uint8_t *dst, *alpha_dst;
	int x, y;
	
	switch(color_type)
	{
	case PNG_COLOR_TYPE_RGB:
		

		surface = new_surface(SURFACE_24BIT, width, height);
		if(!surface)
			break;

		dst = surface->buf;

		for(y = 0; y < height; y++)
		{
			uint8_t *src = row_pointers[y];
			
			for(x = 0; x < width; x++)
			{
				*dst++ = *src++;	// use memcpy
				*dst++ = *src++;
				*dst++ = *src++;
			}
		}

		break;

	
	case PNG_COLOR_TYPE_RGB_ALPHA:

		surface = new_surface(SURFACE_24BITALPHA8BIT, width, height);
		if(!surface)
			break;

		dst = surface->buf;
		alpha_dst = surface->alpha_buf;
		
		for(y = 0; y < height; y++)
		{
			uint8_t *src = row_pointers[y];
			
			for(x = 0; x < width; x++)
			{
				*dst++ = *src++;
				*dst++ = *src++;
				*dst++ = *src++;
				*alpha_dst++ = *src++;
			}
		}

		break;
	
	
	case PNG_COLOR_TYPE_GRAY:
	case PNG_COLOR_TYPE_GRAY_ALPHA:
		
		surface = new_surface(SURFACE_ALPHA8BIT, width, height);
		if(!surface)
			break;

		dst = surface->alpha_buf;
		
		for(y = 0; y < height; y++)
		{
			memcpy(dst, row_pointers[y], width);
			dst += width;
		}

		break;
	}

	png_destroy_read_struct(&png_ptr, &info_ptr, NULL);
	return surface;
}


struct surface_t *read_png_surface(char *filename)
{
	FILE *file = fopen(filename, "rb");
	if(!file)
		return NULL;

	struct surface_t *surface = read_png(file);

	fclose(file);
	return surface;
}


struct surface_t *read_png_surface_as_16bit(char *filename)
{
	struct surface_t *surface = read_png_surface(filename);
	convert_surface_to_16bit(surface);
	return surface;
}


struct surface_t *read_png_surface_as_16bitalpha8bit(char *filename)
{
	struct surface_t *surface = read_png_surface(filename);
	convert_surface_to_16bitalpha8bit(surface);
	return surface;
}


struct surface_t *read_png_surface_as_24bitalpha8bit(char *filename)
{
	struct surface_t *surface = read_png_surface(filename);
	convert_surface_to_24bitalpha8bit(surface);
	return surface;
}


struct surface_t *read_png_surface_as_floats(char *filename)
{
	struct surface_t *surface = read_png_surface(filename);
	convert_surface_to_floats(surface);
	return surface;
}


struct surface_t *read_png_surface_as_floatsalphafloats(char *filename)
{
	struct surface_t *surface = read_png_surface(filename);
	convert_surface_to_floatsalphafloats(surface);
	return surface;
}


#ifdef USE_GDK_PIXBUF

struct surface_t *read_gdk_pixbuf_surface(char *filename)
{
	struct surface_t *surface = NULL;
	GdkPixbuf *gdk_pixbuf = NULL;

	gdk_pixbuf = gdk_pixbuf_new_from_file(filename, NULL);
	
	if(!gdk_pixbuf)
		goto error;
	
	if(gdk_pixbuf_get_bits_per_sample(gdk_pixbuf) != 8)
		goto error;
	
	int width = gdk_pixbuf_get_width(gdk_pixbuf);
	int height = gdk_pixbuf_get_height(gdk_pixbuf);
	int pitch = gdk_pixbuf_get_rowstride(gdk_pixbuf);
	uint8_t *gdk_pixbuf_pixels = gdk_pixbuf_get_pixels(gdk_pixbuf);
		
	uint8_t *dst, *alpha_dst;
	int x, y;

	if(gdk_pixbuf_get_has_alpha(gdk_pixbuf))
	{
		if(gdk_pixbuf_get_n_channels(gdk_pixbuf) != 4)
			goto error;

		surface = new_surface(SURFACE_24BITALPHA8BIT, width, height);
		
		if(!surface)
			goto error;

		dst = surface->buf;
		alpha_dst = surface->alpha_buf;
		
		for(y = 0; y < height; y++)
		{
			uint8_t *src = &gdk_pixbuf_pixels[y * pitch];
			
			for(x = 0; x < width; x++)
			{
				*dst++ = *src++;
				*dst++ = *src++;
				*dst++ = *src++;
				*alpha_dst++ = *src++;
			}
		}
	}
	else
	{
		if(gdk_pixbuf_get_n_channels(gdk_pixbuf) != 3)
			goto error;

		surface = new_surface(SURFACE_24BIT, width, height);
		
		if(!surface)
			goto error;

		dst = surface->buf;
		
		for(y = 0; y < height; y++)
		{
			uint8_t *src = &gdk_pixbuf_pixels[y * pitch];
			
			for(x = 0; x < width; x++)
			{
				*dst++ = *src++;
				*dst++ = *src++;
				*dst++ = *src++;
			}
		}
	}
	
error:

	g_object_unref(gdk_pixbuf);
	return surface;
}


struct surface_t *read_gdk_pixbuf_surface_as_16bit(char *filename)
{
	struct surface_t *surface = read_gdk_pixbuf_surface(filename);
	convert_surface_to_16bit(surface);
	return surface;
}


struct surface_t *read_gdk_pixbuf_surface_as_16bitalpha8bit(char *filename)
{
	struct surface_t *surface = read_gdk_pixbuf_surface(filename);
	convert_surface_to_16bitalpha8bit(surface);
	return surface;
}


struct surface_t *read_gdk_pixbuf_surface_as_24bitalpha8bit(char *filename)
{
	struct surface_t *surface = read_gdk_pixbuf_surface(filename);
	convert_surface_to_24bitalpha8bit(surface);
	return surface;
}


struct surface_t *read_gdk_pixbuf_surface_as_floats(char *filename)
{
	struct surface_t *surface = read_gdk_pixbuf_surface(filename);
	convert_surface_to_floats(surface);
	return surface;
}


struct surface_t *read_gdk_pixbuf_surface_as_floatsalphafloats(char *filename)
{
	struct surface_t *surface = read_gdk_pixbuf_surface(filename);
	convert_surface_to_floatsalphafloats(surface);
	return surface;
}

#endif


struct surface_t *gzread_raw_surface(gzFile file)
{
	int flags, width, height;
	struct surface_t *surface;

	if(gzread(file, &flags, 4) != 4)
		goto error;

	if(gzread(file, &width, 4) != 4)
		goto error;

	if(gzread(file, &height, 4) != 4)
		goto error;

	surface = new_surface(flags, width, height);
	if(!surface)
		return NULL;
	
	int size = get_size(flags, width, height);
	
	if(size)
		if(gzread(file, surface->buf, size) != size)
			goto error;

	size = get_alpha_size(flags, width, height);
	
	if(size)
		if(gzread(file, surface->alpha_buf, size) != size)
			goto error;

	return surface;
	
error:

	return NULL;
}


int write_png(struct surface_t *surface, FILE *file)
{
	png_structp png_ptr = png_create_write_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
	if(!png_ptr)
		return 0;

	png_infop info_ptr = png_create_info_struct(png_ptr);
	if(!info_ptr)
	{
		png_destroy_write_struct(&png_ptr, NULL);
		return 0;
	}

	png_init_io(png_ptr, file);



	switch(surface->flags)
	{
	case SURFACE_24BIT:
	{
	
	
		png_set_IHDR(png_ptr, info_ptr, surface->width, surface->height, 8, PNG_COLOR_TYPE_RGB,
		PNG_INTERLACE_NONE, PNG_COMPRESSION_TYPE_BASE, PNG_FILTER_TYPE_BASE);
	
		png_bytepp row_pointers = malloc(sizeof(png_bytep) * surface->height);
		if(!row_pointers)
		{
			png_destroy_write_struct(&png_ptr, &info_ptr);
			return 0;
		}
			
		int y;
	
		for(y = 0; y < surface->height; y++)
			row_pointers[y] = &(((png_bytep)surface->buf)[y * surface->width * 3]);
	
		png_set_rows(png_ptr, info_ptr, row_pointers);
		png_write_png(png_ptr, info_ptr, PNG_TRANSFORM_IDENTITY, NULL);
		png_destroy_write_struct(&png_ptr, &info_ptr);
	
		free(row_pointers);
	}
	break;
	
	case SURFACE_ALPHA8BIT:
	{
		png_set_IHDR(png_ptr, info_ptr, surface->width, surface->height, 8, PNG_COLOR_TYPE_GRAY,
		PNG_INTERLACE_NONE, PNG_COMPRESSION_TYPE_BASE, PNG_FILTER_TYPE_BASE);
	
		png_bytepp row_pointers = malloc(sizeof(png_bytep) * surface->height);
		if(!row_pointers)
		{
			png_destroy_write_struct(&png_ptr, &info_ptr);
			return 0;
		}
			
		int y;
	
		for(y = 0; y < surface->height; y++)
			row_pointers[y] = &(((png_bytep)surface->alpha_buf)[y * surface->width]);
	
		png_set_rows(png_ptr, info_ptr, row_pointers);
		png_write_png(png_ptr, info_ptr, PNG_TRANSFORM_IDENTITY, NULL);
		png_destroy_write_struct(&png_ptr, &info_ptr);
	
		free(row_pointers);
	}

	break;	
}

	return 1;
}


int write_png_surface(struct surface_t *surface, char *filename)
{
	FILE *file = fopen(filename, "wb");
	if(!file)
		return 0;

	int result = write_png(surface, file);

	fclose(file);

	return result;
}


int gzwrite_raw_surface(gzFile file, struct surface_t *surface)
{
	gzwrite(file, &surface->flags, 4);
	gzwrite(file, &surface->width, 4);
	gzwrite(file, &surface->height, 4);
	
	int size = get_size(surface->flags, surface->width, surface->height);
	if(size)
		gzwrite(file, surface->buf, size);
	
	size = get_alpha_size(surface->flags, surface->width, surface->height);
	if(size)
		gzwrite(file, surface->alpha_buf, size);

	return 1;
}
