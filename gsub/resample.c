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
#include <string.h>
#include <math.h>

#include "minmax.h"
#include "vertex.h"
#include "polygon.h"
#include "llist.h"
#include "gsub.h"

#ifdef WIN32
#include "win32/math.h"
#endif


struct surface_t *resize_a8(struct surface_t *src_texture, int dst_width, int dst_height)
{
	struct surface_t *dst_texture = new_surface(SURFACE_ALPHA8BIT, dst_width, dst_height);
	if(!dst_texture)
		return NULL;

	int dst_y, dst_x, src_y, src_x;
	
	double height_ratio = (double)dst_height / (double)src_texture->height;
	
	double *src_ys = malloc(sizeof(double) * (src_texture->height + 1));
	
	for(src_y = 0; src_y <= src_texture->height; src_y++)
		src_ys[src_y] = (double)src_y * height_ratio;
		

	double width_ratio = (double)dst_width / (double)src_texture->width;
	
	double *src_xs = malloc(sizeof(double) * (src_texture->width + 1));
	
	for(src_x = 0; src_x <= src_texture->width; src_x++)
		src_xs[src_x] = (double)src_x * width_ratio;
	
	
	int *min_src_xs = malloc(sizeof(int) * dst_width);
	int *max_src_xs = malloc(sizeof(int) * dst_width);
	
	for(dst_x = 0; dst_x < dst_width; dst_x++)
	{
		min_src_xs[dst_x] = (dst_x * src_texture->width) / dst_width;
		
		int w = (dst_x + 1) * src_texture->width;
		
		if(w % dst_width)
			max_src_xs[dst_x] = w / dst_width;
		else
			max_src_xs[dst_x] = w / dst_width - 1;
	}
	
	uint8_t *dst = (uint8_t*)dst_texture->alpha_buf;

	
	for(dst_y = 0; dst_y < dst_height; dst_y++)
	{
		double dy1 = (double)dst_y;
		double dy2 = (double)(dst_y + 1);
		
		int min_src_y = (dst_y * src_texture->height) / dst_height;
		int max_src_y;
		int w = (dst_y + 1) * src_texture->height;
		if(w % dst_height)
			max_src_y = w / dst_height;
		else
			max_src_y = w / dst_height - 1;
		
		for(dst_x = 0; dst_x < dst_width; dst_x++)
		{
			double alpha = 0.0;
			
			double dx1 = (double)dst_x;
			double dx2 = (double)(dst_x + 1);
			
			int min_src_x = min_src_xs[dst_x];
			int max_src_x = max_src_xs[dst_x];
			
			uint8_t *src = get_alpha_pixel_addr(src_texture, min_src_x, min_src_y);
			int addon = src_texture->alpha_pitch - (max_src_x - min_src_x + 1);
			
			for(src_y = min_src_y; src_y <= max_src_y; src_y++)
			{
				for(src_x = min_src_x; src_x <= max_src_x; src_x++)
				{
					double area = (max(dx1, src_xs[src_x]) - min(dx2, src_xs[src_x + 1])) * 
						(max(dy1, src_ys[src_y]) - min(dy2, src_ys[src_y + 1]));
					
					alpha += area * (double)(*src);
					
					src++;
				}
				
				src += addon;
			}
			
			*dst++ = (uint8_t)lround(alpha);
		}
		
		if(gsub_callback)
		{
			if(gsub_callback())
			{
				free(src_ys);
				free(src_xs);
				free(min_src_xs);
				free(max_src_xs);
				free_surface(dst_texture);
				return NULL;
			}
		}
	}
	
	free(src_ys);
	free(src_xs);
	free(min_src_xs);
	free(max_src_xs);
	
	return dst_texture;
}


struct surface_t *resize_888(struct surface_t *src_texture, int dst_width, int dst_height)
{
	struct surface_t *dst_texture = new_surface(SURFACE_24BIT, dst_width, dst_height);
	if(!dst_texture)
		return NULL;

	int dst_y, dst_x, src_y, src_x;
	
	double height_ratio = (double)dst_height / (double)src_texture->height;
	
	double *src_ys = malloc(sizeof(double) * (src_texture->height + 1));
	
	for(src_y = 0; src_y <= src_texture->height; src_y++)
		src_ys[src_y] = (double)src_y * height_ratio;
		

	double width_ratio = (double)dst_width / (double)src_texture->width;
	
	double *src_xs = malloc(sizeof(double) * (src_texture->width + 1));
	
	for(src_x = 0; src_x <= src_texture->width; src_x++)
		src_xs[src_x] = (double)src_x * width_ratio;
	
	
	int *min_src_xs = malloc(sizeof(int) * dst_width);
	int *max_src_xs = malloc(sizeof(int) * dst_width);
	
	for(dst_x = 0; dst_x < dst_width; dst_x++)
	{
		min_src_xs[dst_x] = (dst_x * src_texture->width) / dst_width;
		
		int w = (dst_x + 1) * src_texture->width;
		
		if(w % dst_width)
			max_src_xs[dst_x] = w / dst_width;
		else
			max_src_xs[dst_x] = w / dst_width - 1;
	}
	
	uint8_t *dst = dst_texture->buf;

	
	for(dst_y = 0; dst_y < dst_height; dst_y++)
	{
		double dy1 = (double)dst_y;
		double dy2 = (double)(dst_y + 1);
		
		int min_src_y = (dst_y * src_texture->height) / dst_height;
		int max_src_y;
		int w = (dst_y + 1) * src_texture->height;
		if(w % dst_height)
			max_src_y = w / dst_height;
		else
			max_src_y = w / dst_height - 1;
		
		for(dst_x = 0; dst_x < dst_width; dst_x++)
		{
			double red = 0.0, green = 0.0, blue = 0.0;
			
			double dx1 = (double)dst_x;
			double dx2 = (double)(dst_x + 1);
			
			int min_src_x = min_src_xs[dst_x];
			int max_src_x = max_src_xs[dst_x];
			
			uint8_t *src = get_pixel_addr(src_texture, min_src_x, min_src_y);
			int addon = src_texture->pitch - (max_src_x - min_src_x + 1) * 3;
			
			for(src_y = min_src_y; src_y <= max_src_y; src_y++)
			{
				for(src_x = min_src_x; src_x <= max_src_x; src_x++)
				{
					double area = (max(dx1, src_xs[src_x]) - min(dx2, src_xs[src_x + 1])) * 
						(max(dy1, src_ys[src_y]) - min(dy2, src_ys[src_y + 1]));
					
					red += area * get_double_from_8(src[0]);
					green += area * get_double_from_8(src[1]);
					blue += area * get_double_from_8(src[2]);
					
					src += 3;
				}
				
				src += addon;
			}
			
			*dst++ = convert_double_to_8bit(red);
			*dst++ = convert_double_to_8bit(green);
			*dst++ = convert_double_to_8bit(blue);
		}
		
		if(gsub_callback)
		{
			if(gsub_callback())
			{
				free(src_ys);
				free(src_xs);
				free(min_src_xs);
				free(max_src_xs);
				free_surface(dst_texture);
				return NULL;
			}
		}
	}
	
	free(src_ys);
	free(src_xs);
	free(min_src_xs);
	free(max_src_xs);
	
	return dst_texture;
}


struct surface_t *resize_565(struct surface_t *src_texture, int dst_width, int dst_height)
{
	struct surface_t *dst_texture = new_surface(SURFACE_16BIT, dst_width, dst_height);
	if(!dst_texture)
		return NULL;

	int dst_y, dst_x, src_y, src_x;
	
	double height_ratio = (double)dst_height / (double)src_texture->height;
	
	double *src_ys = malloc(sizeof(double) * (src_texture->height + 1));
	
	for(src_y = 0; src_y <= src_texture->height; src_y++)
		src_ys[src_y] = (double)src_y * height_ratio;
		

	double width_ratio = (double)dst_width / (double)src_texture->width;
	
	double *src_xs = malloc(sizeof(double) * (src_texture->width + 1));
	
	for(src_x = 0; src_x <= src_texture->width; src_x++)
		src_xs[src_x] = (double)src_x * width_ratio;
	
	
	int *min_src_xs = malloc(sizeof(int) * dst_width);
	int *max_src_xs = malloc(sizeof(int) * dst_width);
	
	for(dst_x = 0; dst_x < dst_width; dst_x++)
	{
		min_src_xs[dst_x] = (dst_x * src_texture->width) / dst_width;
		
		int w = (dst_x + 1) * src_texture->width;
		
		if(w % dst_width)
			max_src_xs[dst_x] = w / dst_width;
		else
			max_src_xs[dst_x] = w / dst_width - 1;
	}
	
	uint16_t *dst = (uint16_t*)dst_texture->buf;

	
	for(dst_y = 0; dst_y < dst_height; dst_y++)
	{
		double dy1 = (double)dst_y;
		double dy2 = (double)(dst_y + 1);
		
		int min_src_y = (dst_y * src_texture->height) / dst_height;
		int max_src_y;
		int w = (dst_y + 1) * src_texture->height;
		if(w % dst_height)
			max_src_y = w / dst_height;
		else
			max_src_y = w / dst_height - 1;
		
		for(dst_x = 0; dst_x < dst_width; dst_x++)
		{
			double red = 0.0, green = 0.0, blue = 0.0;
			
			double dx1 = (double)dst_x;
			double dx2 = (double)(dst_x + 1);
			
			int min_src_x = min_src_xs[dst_x];
			int max_src_x = max_src_xs[dst_x];
			
			uint16_t *src = get_pixel_addr(src_texture, min_src_x, min_src_y);
			int addon = src_texture->width - (max_src_x - min_src_x + 1);
			
			for(src_y = min_src_y; src_y <= max_src_y; src_y++)
			{
				for(src_x = min_src_x; src_x <= max_src_x; src_x++)
				{
					double area = (max(dx1, src_xs[src_x]) - min(dx2, src_xs[src_x + 1])) * 
						(max(dy1, src_ys[src_y]) - min(dy2, src_ys[src_y + 1]));
					
					red += area * get_double_red(*src);
					green += area * get_double_green(*src);
					blue += area * get_double_blue(*src);
					
					src++;
				}
				
				src += addon;
			}
			
			*dst++ = convert_doubles_to_16bit(red, green, blue);
		}
		
		if(gsub_callback)
		{
			if(gsub_callback())
			{
				free(src_ys);
				free(src_xs);
				free(min_src_xs);
				free(max_src_xs);
				free_surface(dst_texture);
				return NULL;
			}
		}
	}
	
	free(src_ys);
	free(src_xs);
	free(min_src_xs);
	free(max_src_xs);
	
	return dst_texture;
}


struct surface_t *resize_888a8(struct surface_t *src_texture, int dst_width, int dst_height)
{
	struct surface_t *dst_texture = new_surface(SURFACE_24BITALPHA8BIT, dst_width, dst_height);
	if(!dst_texture)
		return NULL;

	int dst_y, dst_x, src_y, src_x;
	
	double height_ratio = (double)dst_height / (double)src_texture->height;
	
	double *src_ys = malloc(sizeof(double) * (src_texture->height + 1));
	
	for(src_y = 0; src_y <= src_texture->height; src_y++)
		src_ys[src_y] = (double)src_y * height_ratio;
		

	double width_ratio = (double)dst_width / (double)src_texture->width;
	
	double *src_xs = malloc(sizeof(double) * (src_texture->width + 1));
	
	for(src_x = 0; src_x <= src_texture->width; src_x++)
		src_xs[src_x] = (double)src_x * width_ratio;
	
	
	int *min_src_xs = malloc(sizeof(int) * dst_width);
	int *max_src_xs = malloc(sizeof(int) * dst_width);
	
	for(dst_x = 0; dst_x < dst_width; dst_x++)
	{
		min_src_xs[dst_x] = (dst_x * src_texture->width) / dst_width;
		
		int w = (dst_x + 1) * src_texture->width;
		
		if(w % dst_width)
			max_src_xs[dst_x] = w / dst_width;
		else
			max_src_xs[dst_x] = w / dst_width - 1;
	}
	
	if(gsub_callback)
	{
		if(gsub_callback())
		{
			free(src_ys);
			free(src_xs);
			free(min_src_xs);
			free(max_src_xs);
			free_surface(dst_texture);
			return NULL;
		}
	}
	
	uint8_t *dst = dst_texture->buf;
	uint8_t *alpha_dst = dst_texture->alpha_buf;

	
	for(dst_y = 0; dst_y < dst_height; dst_y++)
	{
		double dy1 = (double)dst_y;
		double dy2 = (double)(dst_y + 1);
		
		int min_src_y = (dst_y * src_texture->height) / dst_height;
		int max_src_y;
		int w = (dst_y + 1) * src_texture->height;
		if(w % dst_height)
			max_src_y = w / dst_height;
		else
			max_src_y = w / dst_height - 1;
		
		for(dst_x = 0; dst_x < dst_width; dst_x++)
		{
			double red = 0.0, green = 0.0, blue = 0.0, alpha = 0.0;
			
			double dx1 = (double)dst_x;
			double dx2 = (double)(dst_x + 1);
			
			int min_src_x = min_src_xs[dst_x];
			int max_src_x = max_src_xs[dst_x];
			
			uint8_t *src = get_pixel_addr(src_texture, min_src_x, min_src_y);
			uint8_t *alpha_src = get_alpha_pixel_addr(src_texture, min_src_x, min_src_y);
			int addon = src_texture->pitch - (max_src_x - min_src_x + 1) * 3;
			int alpha_addon = src_texture->alpha_pitch - (max_src_x - min_src_x + 1);
			
			for(src_y = min_src_y; src_y <= max_src_y; src_y++)
			{
				for(src_x = min_src_x; src_x <= max_src_x; src_x++)
				{
					double area = (max(dx1, src_xs[src_x]) - min(dx2, src_xs[src_x + 1])) * 
						(max(dy1, src_ys[src_y]) - min(dy2, src_ys[src_y + 1]));
					
					red += area * get_double_from_8(src[0]) * get_double_from_8(*alpha_src);
					green += area * get_double_from_8(src[1]) * get_double_from_8(*alpha_src);
					blue += area * get_double_from_8(src[2]) * get_double_from_8(*alpha_src);
					alpha += area * get_double_from_8(*alpha_src);
					
					src += 3;
					alpha_src++;
				}
				
				src += addon;
				alpha_src += alpha_addon;
			}
			
			*dst++ = convert_double_to_8bit(red / alpha);
			*dst++ = convert_double_to_8bit(green / alpha);
			*dst++ = convert_double_to_8bit(blue / alpha);
			*alpha_dst++ = convert_double_to_8bit(alpha);
		}
		
		if(gsub_callback)
		{
			if(gsub_callback())
			{
				free(src_ys);
				free(src_xs);
				free(min_src_xs);
				free(max_src_xs);
				free_surface(dst_texture);
				return NULL;
			}
		}
	}
	
	free(src_ys);
	free(src_xs);
	free(min_src_xs);
	free(max_src_xs);
	
	return dst_texture;
}


struct surface_t *resize_565a8(struct surface_t *src_texture, int dst_width, int dst_height)
{
	struct surface_t *dst_texture = new_surface(SURFACE_16BITALPHA8BIT, dst_width, dst_height);
	if(!dst_texture)
		return NULL;

	int dst_y, dst_x, src_y, src_x;
	
	double height_ratio = (double)dst_height / (double)src_texture->height;
	
	double *src_ys = malloc(sizeof(double) * (src_texture->height + 1));
	
	for(src_y = 0; src_y <= src_texture->height; src_y++)
		src_ys[src_y] = (double)src_y * height_ratio;
		

	double width_ratio = (double)dst_width / (double)src_texture->width;
	
	double *src_xs = malloc(sizeof(double) * (src_texture->width + 1));
	
	for(src_x = 0; src_x <= src_texture->width; src_x++)
		src_xs[src_x] = (double)src_x * width_ratio;
	
	
	int *min_src_xs = malloc(sizeof(int) * dst_width);
	int *max_src_xs = malloc(sizeof(int) * dst_width);
	
	for(dst_x = 0; dst_x < dst_width; dst_x++)
	{
		min_src_xs[dst_x] = (dst_x * src_texture->width) / dst_width;
		
		int w = (dst_x + 1) * src_texture->width;
		
		if(w % dst_width)
			max_src_xs[dst_x] = w / dst_width;
		else
			max_src_xs[dst_x] = w / dst_width - 1;
	}
	
	if(gsub_callback)
	{
		if(gsub_callback())
		{
			free(src_ys);
			free(src_xs);
			free(min_src_xs);
			free(max_src_xs);
			free_surface(dst_texture);
			return NULL;
		}
	}
	
	uint16_t *dst = (uint16_t*)dst_texture->buf;
	uint8_t *alpha_dst = (uint8_t*)dst_texture->alpha_buf;

	
	for(dst_y = 0; dst_y < dst_height; dst_y++)
	{
		double dy1 = (double)dst_y;
		double dy2 = (double)(dst_y + 1);
		
		int min_src_y = (dst_y * src_texture->height) / dst_height;
		int max_src_y;
		int w = (dst_y + 1) * src_texture->height;
		if(w % dst_height)
			max_src_y = w / dst_height;
		else
			max_src_y = w / dst_height - 1;
		
		for(dst_x = 0; dst_x < dst_width; dst_x++)
		{
			double red = 0.0, green = 0.0, blue = 0.0, alpha = 0.0;
			
			double dx1 = (double)dst_x;
			double dx2 = (double)(dst_x + 1);
			
			int min_src_x = min_src_xs[dst_x];
			int max_src_x = max_src_xs[dst_x];
			
			uint16_t *src = get_pixel_addr(src_texture, min_src_x, min_src_y);
			uint8_t *alpha_src = get_alpha_pixel_addr(src_texture, min_src_x, min_src_y);
			int addon = src_texture->width - (max_src_x - min_src_x + 1);
			
			for(src_y = min_src_y; src_y <= max_src_y; src_y++)
			{
				for(src_x = min_src_x; src_x <= max_src_x; src_x++)
				{
					double area = (max(dx1, src_xs[src_x]) - min(dx2, src_xs[src_x + 1])) * 
						(max(dy1, src_ys[src_y]) - min(dy2, src_ys[src_y + 1]));
					
					red += area * get_double_red(*src) * get_double_from_8(*alpha_src);
					green += area * get_double_green(*src) * get_double_from_8(*alpha_src);
					blue += area * get_double_blue(*src) * get_double_from_8(*alpha_src);
					alpha += area * get_double_from_8(*alpha_src);
					
					src++;
					alpha_src++;
				}
				
				src += addon;
				alpha_src += addon;
			}
			
			*dst++ = convert_doubles_to_16bit(red / alpha, green / alpha, blue / alpha);
			*alpha_dst++ = convert_double_to_8bit(alpha);
		}
		
		if(gsub_callback)
		{
			if(gsub_callback())
			{
				free(src_ys);
				free(src_xs);
				free(min_src_xs);
				free(max_src_xs);
				free_surface(dst_texture);
				return NULL;
			}
		}
	}
	
	free(src_ys);
	free(src_xs);
	free(min_src_xs);
	free(max_src_xs);
	
	return dst_texture;
}


/*
struct surface_t *resize(struct surface_t *src_texture, int dst_width, int dst_height)
{
	if(src_texture->flags != SURFACE_16BIT)
		return NULL;

	struct surface_t *dst_texture = new_surface(SURFACE_16BIT, dst_width, dst_height);
	if(!dst_texture)
		return NULL;

	int total_area = dst_height * dst_width;

	uint16_t *dst = (uint16_t*)dst_texture->buf;

	int dst_y, dstx, src_y, src_x;
	
	for(dst_y = 0; dst_y < dst_height; dst_y++)
	{
		double y1 = (double)dst_y / (double)dst_height;
		double y3 = (double)(dst_y + 1) / (double)dst_height;
		
		int min_src_y = (dst_y * src_texture->height) / dst_height;
		int max_src_y = ((dst_y + 1) * src_texture->height) / dst_height;
		
		if(((dst_y + 1) * src_texture->height) % dst_height == 0)
			max_src_y--;
		
		for(dst_x = 0; dst_x < dst_width; dst_x++)
		{
			double red = 0.0f, green = 0.0f, blue = 0.0f;
			
			double x1 = (double)dst_x / (double)dst_width;
			double x3 = (double)(dst_x + 1) / (double)dst_width;
			
			int min_src_x = (dst_x * src_texture->width) / dst_width;
			int max_src_x = ((dst_x + 1) * src_texture->width) / dst_width;
			
			if(((dst_x + 1) * src_texture->width) % dst_width == 0)
				max_src_x--;
			
			uint16_t *src = &((uint16_t*)src_texture->buf)
				[min_src_y * src_texture->width + min_src_x];
			int addon = src_texture->width - (max_src_x - min_src_x + 1);
			
			for(src_y = min_src_y; src_y <= max_src_y; src_y++)
			{
				double y2 = (double)src_y / (double)src_texture->height;
				double y4 = (double)(src_y + 1) / (double)src_texture->height;
				
				for(src_x = min_src_x; src_x <= max_src_x; src_x++)
				{
					double x2 = (double)src_x / (double)src_texture->width;
					double x4 = (double)(src_x + 1) / (double)src_texture->width;
					
					double area = (max(x1, x2) - min(x3, x4)) * (max(y1, y2) - min(y3, y4)) * 
						total_area;
					
					red += area * get_double_red(*src);
					green += area * get_double_green(*src);
					blue += area * get_double_blue(*src);
					
					src++;
				}
				
				src += addon;
			}

			*dst++ = convert_doubles_to_16bit(red, green, blue);
		}

		if(gsub_callback)
		{
			if(gsub_callback())
			{
				free_surface(dst_texture);
				return NULL;
			}
		}
	}

	return dst_texture;
}
*/


struct surface_t *resize(struct surface_t *src_texture, int dst_width, int dst_height)
{
	if(!src_texture)
		return NULL;
	
	if(src_texture->width == dst_width && src_texture->height == dst_height)
		return duplicate_surface(src_texture);
	
	switch(src_texture->flags)
	{
	case SURFACE_ALPHA8BIT:
		return resize_a8(src_texture, dst_width, dst_height);
	
	case SURFACE_24BIT:
		return resize_888(src_texture, dst_width, dst_height);
	
	case SURFACE_24BITALPHA8BIT:
		return resize_888a8(src_texture, dst_width, dst_height);
	
	case SURFACE_16BIT:
		return resize_565(src_texture, dst_width, dst_height);
		
	case SURFACE_16BITALPHA8BIT:
		return resize_565a8(src_texture, dst_width, dst_height);
	}
	
	return duplicate_surface(src_texture);
}


struct surface_t *crap_resize(struct surface_t *src_texture, int dst_width, int dst_height)
{
	if(src_texture->flags != SURFACE_16BIT)
		return NULL;

	struct surface_t *dst_texture = new_surface(SURFACE_16BIT, dst_width, dst_height);
	if(!dst_texture)
		return NULL;

	uint16_t *dst = (uint16_t*)dst_texture->buf;

	int *xverts = malloc(dst_width * 4);

	int dst_x;
	for(dst_x = 0; dst_x < dst_width; dst_x++)
		xverts[dst_x] = (dst_x * src_texture->width) / dst_width;

	int dst_y;
	for(dst_y = 0; dst_y < dst_height; dst_y++)
	{
		uint16_t *src = &((uint16_t*)src_texture->buf)
			[(dst_y * src_texture->height) / dst_height * src_texture->width];
		
		for(dst_x = 0; dst_x < dst_width; dst_x++)
			*dst++ = src[xverts[dst_x]];
		
		if(gsub_callback)
		{
			if(gsub_callback())
			{
				free_surface(dst_texture);
				return NULL;
			}
		}
	}

	free(xverts);

	return dst_texture;
}



#define sourcex ((double)src_xpos - (double)in_surface->width / 2.0)
#define sourcey ((double)src_ypos - (double)in_surface->height / 2.0)

#define scalex (sourcex * factorx)
#define scaley (sourcey * factory)

#define destx (scalex * cos_theta - scaley * sin_theta)
#define desty (scalex * sin_theta + scaley * cos_theta)


struct surface_t *rotate_surface(struct surface_t *in_surface, 
	int scale_width, int scale_height, double theta)
{
	if(!in_surface)
		return NULL;
	
	if(in_surface->flags != SURFACE_24BITALPHA8BIT)
		return NULL;
	
	int out_width = (int)ceil(hypot((double)scale_width, (double)scale_height));
	
	struct surface_t *out_surface = new_surface(SURFACE_FLOATSALPHAFLOATS, out_width, out_width);
	if(!out_surface)
		return NULL;
	
	clear_surface(out_surface);
	
	double factorx = (double)scale_width / (double)in_surface->width;
	double factory = (double)scale_height / (double)in_surface->height;
	
	struct vertex_t *upper_row = malloc((in_surface->width + 1) * sizeof(struct vertex_t));
	struct vertex_t *lower_row = malloc((in_surface->width + 1) * sizeof(struct vertex_t));

	double sin_theta, cos_theta;
	
	sincos(theta, &sin_theta, &cos_theta);

	int src_ypos = 0, src_xpos;

	for(src_xpos = 0; src_xpos < (in_surface->width + 1); src_xpos++)
	{
		lower_row[src_xpos].x = (float)((destx) + (double)out_width / 2.0);
		lower_row[src_xpos].y = (float)((desty) + (double)out_width / 2.0);
	}
	
	if(gsub_callback)
	{
		if(gsub_callback())
		{
			free(upper_row);
			free(lower_row);
			free_surface(out_surface);
			return NULL;
		}
	}

	for(src_ypos = 0; src_ypos < in_surface->height; src_ypos++)
	{
		src_ypos++;

		for(src_xpos = 0; src_xpos < (in_surface->width + 1); src_xpos++)
		{
			upper_row[src_xpos].x = (float)((destx) + (double)out_width / 2.0);
			upper_row[src_xpos].y = (float)((desty) + (double)out_width / 2.0);
		}

		src_ypos--;

		for(src_xpos = 0; src_xpos < in_surface->width; src_xpos++)
		{
			// obtain verticies in c/w order for source pixel

			struct polygon_t src_poly;
			src_poly.vertex[0] = lower_row[src_xpos];
			src_poly.vertex[1] = upper_row[src_xpos];
			src_poly.vertex[2] = upper_row[src_xpos + 1];
			src_poly.vertex[3] = lower_row[src_xpos + 1];

			src_poly.numverts = 4;


			// determine dest block to be tested

			int x = (int)floorf(src_poly.vertex[0].x);
			int y = (int)floorf(src_poly.vertex[0].y);

			int min_xpos = x;
			int max_xpos = x;
			int min_ypos = y;
			int max_ypos = y;

			int i;
			
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
				max_ypos < 0 || min_ypos >= out_width)
				continue;


			// trim dest block

			min_xpos = max(min_xpos, 0);
			min_ypos = max(min_ypos, 0);
			max_xpos = min(max_xpos, out_width - 1);
			max_ypos = min(max_ypos, out_width - 1);

				
			uint8_t *src = &((uint8_t*)in_surface->alpha_buf)
				[(in_surface->height - src_ypos - 1) * in_surface->width + src_xpos];
			double alpha = (double)(*src) / 255.0;
			
			src = &((uint8_t*)in_surface->buf)
				[((in_surface->height - src_ypos - 1) * in_surface->width + src_xpos) * 3];
			double red = ((double)(*src++) / 255.0) * alpha;
			double green = ((double)(*src++) / 255.0) * alpha;
			double blue = ((double)(*src) / 255.0) * alpha;

			int dst_ypos, dst_xpos;
			
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

					float *dst = &((float*)out_surface->buf)
						[((out_width - dst_ypos - 1) * out_width + dst_xpos) * 3];

					*dst++ += (float)(red * area);
					*dst++ += (float)(green * area);
					*dst += (float)(blue * area);

					((float*)out_surface->alpha_buf)
						[(out_width - dst_ypos - 1) * out_width + dst_xpos] += 
						(float)(alpha * area);
				}
			}
		}

		if(gsub_callback)
		{
			if(gsub_callback())
			{
				free(upper_row);
				free(lower_row);
				free_surface(out_surface);
				return NULL;
			}
		}
		
		struct vertex_t *temp = upper_row;
		upper_row = lower_row;
		lower_row = temp;
	}
	

	free(upper_row);
	free(lower_row);
	
	int x;
	for(x = 0; x < out_width * out_width; x++)
	{
		float *dst = &((float*)out_surface->buf)[x * 3];
		float src = ((float*)out_surface->alpha_buf)[x];

		dst[0] /= src;
		dst[1] /= src;
		dst[2] /= src;
	}

	if(gsub_callback)
	{
		if(gsub_callback())
		{
			free_surface(out_surface);
			return NULL;
		}
	}
		
	convert_surface_to_24bitalpha8bit(out_surface);
	
	return out_surface;
}


struct surface_t *multiple_resample(struct texture_verts_t *texture_verts0, 
	struct texture_polys_t *texture_polys0, 
	int dst_width, int dst_height, int dst_posx, int dst_posy)
{
	struct surface_t *out_surface = new_surface(SURFACE_FLOATSALPHAFLOATS, dst_width, dst_height);
	if(!out_surface)
		return NULL;
	
	clear_surface(out_surface);

	struct texture_verts_t *texture_verts = texture_verts0;

	while(texture_verts)
	{
		int src_ypos;
		for(src_ypos = 0; src_ypos < texture_verts->surface->height; src_ypos++)
		{
			int src_xpos;
			for(src_xpos = 0; src_xpos < texture_verts->surface->width; src_xpos++)
			{
				// obtain verticies in c/w order for source pixel

				struct polygon_t src_poly;

				src_poly.vertex[0] = texture_verts->verts
					[src_ypos * (texture_verts->surface->width + 1) + src_xpos];
				src_poly.vertex[1] = texture_verts->verts
					[src_ypos * (texture_verts->surface->width + 1) + src_xpos + 1];
				src_poly.vertex[2] = texture_verts->verts
					[(src_ypos + 1) * (texture_verts->surface->width + 1) + src_xpos + 1];
				src_poly.vertex[3] = texture_verts->verts
					[(src_ypos + 1) * (texture_verts->surface->width + 1) + src_xpos];

				src_poly.numverts = 4;


				// determine dest block to be tested

				int x = (int)floorf(src_poly.vertex[0].x);
				int y = (int)floorf(src_poly.vertex[0].y);

				int min_xpos = x;
				int max_xpos = x;
				int min_ypos = y;
				int max_ypos = y;

				int i;
				for(i = 1; i < 4; i++)
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

				if(max_xpos < dst_posx || min_xpos >= dst_posx + dst_width || 
					max_ypos < dst_posy || min_ypos >= dst_posy + dst_height)
					continue;


				// trim dest block

				min_xpos = max(min_xpos, dst_posx);
				min_ypos = max(min_ypos, dst_posy);
				max_xpos = min(max_xpos, dst_posx + dst_width - 1);
				max_ypos = min(max_ypos, dst_posy + dst_height - 1);


				uint8_t *src = &((uint8_t*)texture_verts->surface->buf)
					[(src_ypos * texture_verts->surface->width + src_xpos) * 3];
				uint8_t *alpha_src = 0;
				if(texture_verts->surface->flags == SURFACE_24BITALPHA8BIT)
					alpha_src = &((uint8_t*)texture_verts->surface->alpha_buf)
						[src_ypos * texture_verts->surface->width + src_xpos];

				int dst_ypos, dst_xpos;
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
						
						
						// calculate overlap area

						double area = poly_clip_area(&dst_poly, &src_poly);

						
						// early out for zero-influence dest pixels

						if(area == 0.0)
							continue;


						// actuate pixel intensity

						float *dst = &((float*)out_surface->buf)
							[((dst_ypos - dst_posy) * dst_width + dst_xpos - dst_posx) * 3];
						float alpha;
						
						switch(texture_verts->surface->flags)
						{
						case SURFACE_24BIT:
							*dst++ += (float)(get_double_from_8(src[0]) * area);
							*dst++ += (float)(get_double_from_8(src[1]) * area);
							*dst += (float)(get_double_from_8(src[2]) * area);
							((float*)out_surface->alpha_buf)
								[(dst_ypos - dst_posy) * dst_width + dst_xpos - dst_posx] += area;
							break;
						
						case SURFACE_24BITALPHA8BIT:
							alpha = get_double_from_8(*alpha_src);
							*dst++ += (float)(get_double_from_8(src[0]) * area * alpha);
							*dst++ += (float)(get_double_from_8(src[1]) * area * alpha);
							*dst += (float)(get_double_from_8(src[2]) * area * alpha);
							((float*)out_surface->alpha_buf)
								[(dst_ypos - dst_posy) * dst_width + dst_xpos - dst_posx] += 
								area * alpha;
							break;
						}						
					}
				}
			}

			if(gsub_callback)
			{
				if(gsub_callback())
				{
					free_surface(out_surface);
					return NULL;
				}
			}
		}

		texture_verts = texture_verts->next;
	}

	/*
	struct texture_polys_t *texture_polys = texture_polys0;

	while(texture_polys)
	{
		struct polygon_t *src_poly = texture_polys->polys;
		
		int src_xpos;
		for(src_xpos = 0; src_xpos < texture_polys->surface->width; src_xpos++)
		{
			int src_ypos;
			for(src_ypos = 0; src_ypos < texture_polys->surface->height; src_ypos++)
			{
				// determine dest block to be tested
	
				int x = (int)floorf(src_poly->vertex[0].x);
				int y = (int)floorf(src_poly->vertex[0].y);
	
				int min_xpos = x;
				int max_xpos = x;
				int min_ypos = y;
				int max_ypos = y;
	
				int i;
				for(i = 1; i != src_poly->numverts; i++)
				{
					x = (int)floorf(src_poly->vertex[i].x);
					y = (int)floorf(src_poly->vertex[i].y);
	
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
	
				if(max_xpos < dst_posx || min_xpos >= dst_posx + dst_width || 
					max_ypos < dst_posy || min_ypos >= dst_posy + dst_height)
				{
					src_poly++;
					continue;
				}

	
				// trim dest block
	
				min_xpos = max(min_xpos, dst_posx);
				min_ypos = max(min_ypos, dst_posy);
				max_xpos = min(max_xpos, dst_posx + dst_width - 1);
				max_ypos = min(max_ypos, dst_posy + dst_height - 1);
	
	
				uint16_t *src = &((uint16_t*)texture_polys->surface->buf)
					[src_ypos * texture_polys->surface->width + src_xpos];
				uint8_t *alpha_src = 0;
				if(texture_verts->surface->flags == SURFACE_16BITALPHA8BIT)
					alpha_src = &((uint8_t*)texture_verts->surface->alpha_buf)
						[src_ypos * texture_verts->surface->width + src_xpos];
	
				int dst_ypos, dst_xpos;
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
	
	
						// calculate overlap area

						double area = poly_clip_area(&dst_poly, src_poly);


						// early out for zero-influence dest pixels
	
						if(area == 0.0)
							continue;
	
	
						// actuate pixel intensity
	
						float *dst = &((float*)out_surface->buf)
							[((dst_ypos - dst_posy) * dst_width + dst_xpos - dst_posx) * 3];
						float alpha;
						
						switch(texture_verts->surface->flags)
						{
						case SURFACE_16BIT:
							*dst++ += (float)(get_double_red(*src) * area);
							*dst++ += (float)(get_double_green(*src) * area);
							*dst += (float)(get_double_blue(*src) * area);
							((float*)out_surface->alpha_buf)
								[(dst_ypos - dst_posy) * dst_width + dst_xpos - dst_posx] += area;
							break;
						
						case SURFACE_16BITALPHA8BIT:
							alpha = get_double_alpha(*alpha_src);
							*dst++ += (float)(get_double_red(*src) * area * alpha);
							*dst++ += (float)(get_double_green(*src) * area * alpha);
							*dst += (float)(get_double_blue(*src) * area * alpha);
							((float*)out_surface->alpha_buf)
								[(dst_ypos - dst_posy) * dst_width + dst_xpos - dst_posx] += 
								area * alpha;
							break;
						}						
					}
				}
	
				if(gsub_callback)
				{
					if(gsub_callback())
					{
						free_surface(out_surface);
						return NULL;
					}
				}
				
				src_poly++;
			}
		
		}

		texture_polys = texture_polys->next;
	}
	*/
	

	struct texture_polys_t *texture_polys = texture_polys0;

	while(texture_polys)
	{
		struct vertex_ll_t **src_poly = texture_polys->polys;
		
		int src_ypos;
		for(src_ypos = 0; src_ypos < texture_polys->surface->height; src_ypos++)
		{
			int src_xpos;
			for(src_xpos = 0; src_xpos < texture_polys->surface->width; src_xpos++)
			{
				// determine dest block to be tested
				
				struct vertex_ll_t *cvert = *src_poly;
					
				if(!cvert)
				{
					src_poly++;
					continue;
				}
					
	
				int x = (int)floorf(cvert->x);
				int y = (int)floorf(cvert->y);
				cvert = cvert->next;
	
				int min_xpos = x;
				int max_xpos = x;
				int min_ypos = y;
				int max_ypos = y;

				while(cvert)
				{
					x = (int)floorf(cvert->x);
					y = (int)floorf(cvert->y);
	
					if(x < min_xpos)
						min_xpos = x;
	
					if(x > max_xpos)
						max_xpos = x;
	
					if(y < min_ypos)
						min_ypos = y;
	
					if(y > max_ypos)
						max_ypos = y;

					cvert = cvert->next;
				}
	
	
				// early out for zero influence source pixels
	
				if(max_xpos < dst_posx || min_xpos >= dst_posx + dst_width || 
					max_ypos < dst_posy || min_ypos >= dst_posy + dst_height)
				{
					src_poly++;
					continue;
				}

	
				// trim dest block
	
				min_xpos = max(min_xpos, dst_posx);
				min_ypos = max(min_ypos, dst_posy);
				max_xpos = min(max_xpos, dst_posx + dst_width - 1);
				max_ypos = min(max_ypos, dst_posy + dst_height - 1);
	
	
				uint8_t *src = &((uint8_t*)texture_polys->surface->buf)
					[(src_ypos * texture_polys->surface->width + src_xpos) * 3];
				uint8_t *alpha_src = 0;
				if(texture_polys->surface->flags == SURFACE_24BITALPHA8BIT)
					alpha_src = &((uint8_t*)texture_polys->surface->alpha_buf)
						[src_ypos * texture_polys->surface->width + src_xpos];
	
				int dst_ypos, dst_xpos;
				for(dst_ypos = min_ypos; dst_ypos <= max_ypos; dst_ypos++)
				{
					for(dst_xpos = min_xpos; dst_xpos <= max_xpos; dst_xpos++)
					{
						// generate vertexes in c/w order for destination pixel polygon
	
						float x = (float)dst_xpos;
						float y = (float)dst_ypos;
	
						struct vertex_ll_t *dst_poly = NULL;
	
						struct vertex_ll_t vertex;
						
						vertex.x = x;
						vertex.y = y;
						LL_ADD(struct vertex_ll_t, &dst_poly, &vertex);
						
						vertex.x = x;
						vertex.y = y + 1.0f;
						LL_ADD(struct vertex_ll_t, &dst_poly, &vertex);
						
						vertex.x = x + 1.0f;
						vertex.y = y + 1.0f;
						LL_ADD(struct vertex_ll_t, &dst_poly, &vertex);
						
						vertex.x = x + 1.0f;
						vertex.y = y;
						LL_ADD(struct vertex_ll_t, &dst_poly, &vertex);
	
						// calculate overlap area
						
						double area = poly_arb_clip_area(&dst_poly, *src_poly);
						
						LL_REMOVE_ALL(struct vertex_ll_t, &dst_poly);


						// early out for zero-influence dest pixels
	
						if(area == 0.0)
							continue;
	
	
						// actuate pixel intensity
						
	
						float *dst = &((float*)out_surface->buf)
							[((dst_ypos - dst_posy) * dst_width + dst_xpos - dst_posx) * 3];
						float alpha;
						
						switch(texture_polys->surface->flags)
						{
						case SURFACE_24BIT:
							*dst++ += (float)(get_double_from_8(src[0]) * area);
							*dst++ += (float)(get_double_from_8(src[1]) * area);
							*dst += (float)(get_double_from_8(src[2]) * area);
							((float*)out_surface->alpha_buf)
								[(dst_ypos - dst_posy) * dst_width + dst_xpos - dst_posx] += area;
							break;
						
						case SURFACE_24BITALPHA8BIT:
							alpha = get_double_from_8(*alpha_src);
							*dst++ += (float)(get_double_from_8(src[0]) * area * alpha);
							*dst++ += (float)(get_double_from_8(src[1]) * area * alpha);
							*dst += (float)(get_double_from_8(src[2]) * area * alpha);
							((float*)out_surface->alpha_buf)
								[(dst_ypos - dst_posy) * dst_width + dst_xpos - dst_posx] += 
									area * alpha;
							break;
						}						
					}
				}
	
				if(gsub_callback)
				{
					if(gsub_callback())
					{
						free_surface(out_surface);
						return NULL;
					}
				}
				
				src_poly++;
			}
		
		}

		texture_polys = texture_polys->next;
	}


	int x;

	for(x = 0; x < out_surface->width * out_surface->height; x++)
	{
		float *dst = &((float*)out_surface->buf)[x * 3];
		float src = ((float*)out_surface->alpha_buf)[x];

		dst[0] /= src;
		dst[1] /= src;
		dst[2] /= src;
	}


	return out_surface;
}


/*
float *resample(float *src_pixels, vertex_t *src_verts, int src_width, int src_height, 
			  int dst_width, int dst_height)
{
	float *dst_pixels = (float*)calloc(dst_width * dst_height * 3, 4);

	for(int src_ypos = 0; src_ypos < src_height; src_ypos++)
	{
		for(int src_xpos = 0; src_xpos < src_width; src_xpos++)
		{
			// obtain verticies in c/w order for source pixel

			polygon_t src_poly;

			src_poly.vertex[0] = src_verts[(src_xpos * (src_height + 1)) + src_ypos];
			src_poly.vertex[1] = src_verts[(src_xpos * (src_height + 1)) + src_ypos + 1];
			src_poly.vertex[2] = src_verts[(src_xpos + 1) * (src_height + 1) + src_ypos + 1];
			src_poly.vertex[3] = src_verts[(src_xpos + 1) * (src_height + 1) + src_ypos];

			src_poly.numverts = 4;


			// determine dest block to be tested

			int x = (int)floorf(src_poly.vertex[0].x);
			int y = src_height - 1 - (int)floorf(src_poly.vertex[0].y);

			int min_xpos = x;
			int max_xpos = x;
			int min_ypos = y;
			int max_ypos = y;

			for(int i = 1; i != src_poly.numverts; i++)
			{
				x = (int)floorf(src_poly.vertex[i].x);
				y = src_height - 1 - (int)floorf(src_poly.vertex[i].y);

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

			if(max_xpos < 0 || min_xpos >= dst_width || 
				max_ypos < 0 || min_ypos >= dst_height)
				continue;


			// trim dest block

			min_xpos = max(min_xpos, 0);
			min_ypos = max(min_ypos, 0);
			max_xpos = min(max_xpos, dst_width);
			min_ypos = min(max_ypos, dst_height);


			float *src = &src_pixels[(src_ypos * src_width + src_xpos) * 3];

			for(int dst_ypos = min_ypos; dst_ypos <= max_ypos; dst_ypos++)
			{
				for(int dst_xpos = min_xpos; dst_xpos <= max_xpos; dst_xpos++)
				{
					// generate vertexes in c/w order for destination pixel polygon

					float x = (float)dst_xpos;
					float y = dst_height - 1 - (float)dst_ypos;

					polygon_t dst_poly;

					dst_poly.vertex[0].x = x;
					dst_poly.vertex[0].y = y;
					dst_poly.vertex[1].x = x;
					dst_poly.vertex[1].y = y + 1.0f;
					dst_poly.vertex[2].x = x + 1.0f;
					dst_poly.vertex[2].y = y + 1.0f;
					dst_poly.vertex[3].x = x + 1.0f;
					dst_poly.vertex[3].y = y;

					dst_poly.numverts = 4;


					// clip dest pixel against source pixel

					polyclip(&dst_poly, &src_poly);


					// calculate area of clipped dest pixel

					float area = polyarea(&dst_poly);


					// early out for zero-influence dest pixels

					if(area == 0.0f)
						continue;


					// actuate pixel intensity

					float *dst = &dst_pixels[(dst_ypos * dst_width + dst_xpos) * 3];

					*dst++ += src[0] * area;
					*dst++ += src[1] * area;
					*dst += src[2] * area;
				}
			}
		}

		if(gsub_callback)
		{
			if(gsub_callback())
			{
				free(dst_pixels);
				return NULL;
			}
		}
	}

	return dst_pixels;
}
*/
