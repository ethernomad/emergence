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
#include <stdio.h>
#include <stdint.h>
#include <stdarg.h>

#include "prefix.h"

#include "stringbuf.h"
#include "resource.h"
#include "gsub.h"

uint8_t charlengths[256];
struct surface_t *smallfont;


void init_text()
{
	smallfont = read_png_surface(find_resource("smallfont.png"));

	int i;

	for(i = 0; i != 256; i++)
	{
		switch(i)
		{
		case ';':
		case ':':
		case '.':
		case 'i':
		case 'l':
		case 'j':
			charlengths[i] = 4;
			break;

			charlengths[i] = 5;
			break;

		case ' ':
		case '\\':
		case '/':
		case 'I':
		case 'r':
			charlengths[i] = 6;
			break;

		case 'f':
		case 'k':
		case 't':
			charlengths[i] = 7;
			break;
	
		default:
			charlengths[i] = 8;
			break;
		}
	}
}


int blit_text(int x, int y, uint8_t red, uint8_t green, uint8_t blue, 
	struct surface_t *dest, const char *fmt, ...)
{
	char *text;
	
	va_list ap;
	
	va_start(ap, fmt);
	vasprintf(&text, fmt, ap);
	va_end(ap);
	
	
	struct blit_params_t params;

	params.red = red;
	params.green = green;
	params.blue = blue;
	params.source = smallfont;
	params.dest = dest;
	
	int w = 0;
	char *c = text;
	
	while(*c)
	{
		params.source_x = ((int)*((uint8_t*)c)) << 3;
		params.source_y = 0;
		params.dest_x = x + w;
		params.dest_y = y;
		params.height = 13;
		params.width = charlengths[*((uint8_t*)c)];

		w += params.width;

		blit_partial_surface(&params);
		
		c++;
	}
	
	free(text);

	return w;
}


int blit_text_centered(int x, int y, uint8_t red, uint8_t green, uint8_t blue, 
	struct surface_t *dest, const char *fmt, ...)
{
	char *text;
	
	va_list ap;
	
	va_start(ap, fmt);
	vasprintf(&text, fmt, ap);
	va_end(ap);
	
	
	int w = 0;
	char *c = text;

	while(*c)
	{
		w += charlengths[*((uint8_t*)c)];
		c++;
	}
	
	x -= w / 2;

	
	struct blit_params_t params;

	params.red = red;
	params.green = green;
	params.blue = blue;
	params.source = smallfont;
	params.dest = dest;
	
	w = 0;
	c = text;

	while(*c)
	{
		params.source_x = ((int)*((uint8_t*)c)) << 3;
		params.source_y = 0;
		params.dest_x = x + w;
		params.dest_y = y;
		params.height = 13;
		params.width = charlengths[*((uint8_t*)c)];

		w += params.width;

		blit_partial_surface(&params);
		
		c++;
	}
	
	free(text);
	
	return w;
}


int blit_text_right_aligned(int x, int y, uint8_t red, uint8_t green, uint8_t blue, 
	struct surface_t *dest, const char *fmt, ...)
{
	char *text;
	
	va_list ap;
	
	va_start(ap, fmt);
	vasprintf(&text, fmt, ap);
	va_end(ap);
	
	
	int w = 0;
	char *c = text;

	while(*c)
	{
		w += charlengths[*((uint8_t*)c)];
		c++;
	}
	
	x -= w;

	
	struct blit_params_t params;

	params.red = red;
	params.green = green;
	params.blue = blue;
	params.source = smallfont;
	params.dest = dest;
	
	w = 0;
	c = text;

	while(*c)
	{
		params.source_x = ((int)*((uint8_t*)c)) << 3;
		params.source_y = 0;
		params.dest_x = x + w;
		params.dest_y = y;
		params.height = 13;
		params.width = charlengths[*((uint8_t*)c)];

		w += params.width;

		blit_partial_surface(&params);
		
		c++;
	}
	
	free(text);
	
	return w;
}
