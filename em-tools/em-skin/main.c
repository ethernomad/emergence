/**
 * @file
 * @copyright 1998-2004 Jonathan Brown <jbrown@bluedroplet.com>
 * @license https://www.gnu.org/licenses/gpl-3.0.html
 * @homepage https://github.com/bluedroplet/emergence
 */

#ifdef LINUX
#define _GNU_SOURCE
#endif

#include <stdlib.h>
#include <stdint.h>
#include <error.h>
#include <argp.h>

#include <zlib.h>

#include "gsub.h"
#include "stringbuf.h"
#include "user.h"



void skin(char *skin)
{
	struct string_t *filename = new_string_text("%s.skin", skin);
	
	gzFile gzfile = gzopen(filename->text, "wb9");
	if(!gzfile)
		return;
	
	free_string(filename);
	struct surface_t *surface;
		
	filename = new_string_text("%s/craft.png", skin);
	surface	= read_png_surface_as_24bitalpha8bit(filename->text);
	gzwrite_raw_surface(gzfile, surface);
	free_surface(surface);
	free_string(filename);
	
	filename = new_string_text("%s/rocket-launcher.png", skin);
	surface	= read_png_surface_as_24bitalpha8bit(filename->text);
	gzwrite_raw_surface(gzfile, surface);
	free_surface(surface);
	free_string(filename);
	
	filename = new_string_text("%s/minigun.png", skin);
	surface	= read_png_surface_as_24bitalpha8bit(filename->text);
	gzwrite_raw_surface(gzfile, surface);
	free_surface(surface);
	free_string(filename);
	
	filename = new_string_text("%s/plasma-cannon.png", skin);
	surface	= read_png_surface_as_24bitalpha8bit(filename->text);
	gzwrite_raw_surface(gzfile, surface);
	free_surface(surface);
	free_string(filename);

	gzclose(gzfile);
}
	

const char *argp_program_version = "em-skin " VERSION;
const char *argp_program_bug_address = "<jbrown@bluedroplet.com>";
static char doc[] = "Generates Emergence skin files from exploded directories";
static char args_doc[] = "directories";

static error_t parse_opt (int key, char *arg, struct argp_state *state)
{
	switch (key)
	{
	case ARGP_KEY_ARG:

		skin(arg);
		break;

	case ARGP_KEY_END:
		if (state->arg_num < 1)
		/* Not enough arguments. */
			argp_usage (state);

		break;


	default:
		return ARGP_ERR_UNKNOWN;
	}

	return 0;
}


static struct argp argp = { NULL, parse_opt, args_doc, doc };

int main (int argc, char **argv)
{
	init_user();
	argp_parse(&argp, argc, argv, 0, 0, NULL);
	exit(0);
}
