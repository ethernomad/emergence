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
#include <string.h>

#include "llist.h"
#include "stringbuf.h"
#include "gsub.h"
#include "ris.h"


struct ris_t *ris0 = NULL;

float ris_multiplier = 1.0;
	

void set_ri_surface_multiplier(float m)
{
	ris_multiplier = m;
	
	struct ris_t *cris = ris0;
		
	while(cris)
	{
		free_surface(cris->surface);
		
		struct surface_t *temp = read_png_surface(cris->filename->text);
		
		cris->surface = resize(temp, temp->width * ris_multiplier, temp->height * ris_multiplier);
		
		free_surface(temp);		
		
		cris = cris->next;
	}
}


struct ris_t *load_ri_surface(char *filename)
{
	struct ris_t ris;
		
	ris.filename = new_string_text(filename);
	
	struct surface_t *temp = read_png_surface(filename);
	
	
	ris.surface = resize(temp, temp->width * ris_multiplier, temp->height * ris_multiplier);
	
	free_surface(temp);
	
	LL_ADD(struct ris_t, &ris0, &ris);
		
	return ris0;
}


void free_ri_surface(struct ris_t *ris)
{
	free_string(ris->filename);
	free_surface(ris->surface);
	
	LL_REMOVE(struct ris_t, &ris0, ris);
}


void kill_ris()
{
	while(ris0)
	{
		free_string(ris0->filename);
		free_surface(ris0->surface);
		
		LL_REMOVE(struct ris_t, &ris0, ris0);
	}
}
