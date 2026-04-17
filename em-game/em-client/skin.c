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
#include <zlib.h>
#include <math.h>

#include <sys/stat.h>

#include "prefix.h"

#include "resource.h"
#include "stringbuf.h"
#include "buffer.h"
#include "llist.h"
#include "user.h"
#include "network.h"
#include "sgame.h"
#include "gsub.h"
#include "entry.h"
#include "skin.h"
#include "console.h"
#include "rotate.h"
#include "game.h"
#include "render.h"

struct skin_t *skin0 = NULL;


int load_skin(struct skin_t *skin)
{
	game_rendering = 0;
	
	console_print("loading skin\n");
	render_frame();
	game_rendering = 1;
	
	struct string_t *filename = new_string_string(emergence_home_dir);
	string_cat_text(filename, "/skins/");	
	string_cat_string(filename, skin->name);
	string_cat_text(filename, ".skin.cache");
	string_cat_int(filename, vid_width);
	
//	console_print(filename->text);
//	console_print("\n");
	
//	console_print("skin: %i\n", skin->index);
	
	gzFile gzfile = gzopen(filename->text, "rb");
	if(gzfile)
	{
		skin->craft = gzread_raw_surface(gzfile);
		skin->plasma_cannon = gzread_raw_surface(gzfile);
		skin->minigun = gzread_raw_surface(gzfile);
		skin->rocket_launcher = gzread_raw_surface(gzfile);
		gzclose(gzfile);
		free_string(filename);
		return 1;
	}
	
	free_string(filename);
	
	filename = new_string_string(emergence_home_dir);
	string_cat_text(filename, "/skins/");	
	string_cat_string(filename, skin->name);
	string_cat_text(filename, ".skin");
	
	gzfile = gzopen(filename->text, "rb");
	if(!gzfile)
	{
		console_print("Could not load skin: %s\n", filename->text);
		free_string(filename);
		return 0;
	}
	
	free_string(filename);
	

	struct rotate_t craft_rot;
	craft_rot.in_surface = gzread_raw_surface(gzfile);
	craft_rot.out_surface = &skin->craft;
	craft_rot.next = NULL;
	
	int width = (int)ceil((double)vid_width / 20.0);
	
	do_rotate(&craft_rot, width, width, ROTATIONS);
	
	struct rotate_t rocket_launcher_rot;
	rocket_launcher_rot.in_surface = gzread_raw_surface(gzfile);
	rocket_launcher_rot.out_surface = &skin->rocket_launcher;
	rocket_launcher_rot.next = NULL;
	
	struct rotate_t minigun_rot;
	minigun_rot.in_surface = gzread_raw_surface(gzfile);
	minigun_rot.out_surface = &skin->minigun;
	minigun_rot.next = &rocket_launcher_rot;
	
	struct rotate_t plasma_cannon_rot;
	plasma_cannon_rot.in_surface = gzread_raw_surface(gzfile);
	plasma_cannon_rot.out_surface = &skin->plasma_cannon;
	plasma_cannon_rot.next = &minigun_rot;
	
	width = (int)ceil((double)vid_width / 32.0);

	do_rotate(&plasma_cannon_rot, width, width, ROTATIONS);
	
	gzclose(gzfile);
	
	filename = new_string_string(emergence_home_dir);
	string_cat_text(filename, "/skins/");	
	string_cat_string(filename, skin->name);
	string_cat_text(filename, ".skin.cache");
	string_cat_int(filename, vid_width);
	
	gzfile = gzopen(filename->text, "wb9");
	if(!gzfile)
		return 0;

	gzwrite_raw_surface(gzfile, skin->craft);
	gzwrite_raw_surface(gzfile, skin->plasma_cannon);
	gzwrite_raw_surface(gzfile, skin->minigun);
	gzwrite_raw_surface(gzfile, skin->rocket_launcher);
	gzclose(gzfile);
	
	free_string(filename);
	
	return 1;
}


int game_process_load_skin()
{
	struct skin_t skin;
		
	skin.name = message_reader_read_string();
	skin.index = message_reader_read_uint32();
	
	if(load_skin(&skin))
	{
		LL_ADD(struct skin_t, &skin0, &skin);
		return 1;
	}
	
	return 0;
}

/*
int game_process_demo_load_skin()
{
	struct skin_t skin;
		
	skin.name = gzread_string(gzdemo);
	gzread(gzdemo, &skin.index, 4);
	
	if(load_skin(&skin))
	{
		LL_ADD(struct skin_t, &skin0, &skin);
		return 1;
	}
	
	return 0;
}
*/

void reload_skins()
{
	struct skin_t *cskin = skin0;
		
	while(cskin)
	{
		free_surface(cskin->craft);
		cskin->craft = NULL;
		free_surface(cskin->rocket_launcher);
		cskin->rocket_launcher = NULL;
		free_surface(cskin->minigun);
		cskin->minigun = NULL;
		free_surface(cskin->plasma_cannon);
		cskin->plasma_cannon = NULL;
	/*	free_surface(cskin->plasma);
		cskin->plasma = NULL;
		free_surface(cskin->rocket);
		cskin->rocket = NULL;
	*/	
		load_skin(cskin);
		
		cskin = cskin->next;
	}
	
	console_print("reloaded skins\n");
}


void clear_skins()
{
	struct skin_t *cskin = skin0;
		
	while(cskin)
	{
		free_surface(cskin->craft);
		cskin->craft = NULL;
		free_surface(cskin->rocket_launcher);
		cskin->rocket_launcher = NULL;
		free_surface(cskin->minigun);
		cskin->minigun = NULL;
		free_surface(cskin->plasma_cannon);
		cskin->plasma_cannon = NULL;
	/*	free_surface(cskin->plasma);
		cskin->plasma = NULL;
		free_surface(cskin->rocket);
		cskin->rocket = NULL;
	*/	
		
		cskin = cskin->next;
	}
	
	LL_REMOVE_ALL(struct skin_t, &skin0);
}

struct surface_t *skin_get_craft_surface(uint32_t index)
{
	struct skin_t *skin = skin0;
		
	while(skin)
	{
		if(skin->index == index)
			return skin->craft;
		
		skin = skin->next;
	}
	
	return NULL;
}


struct surface_t *skin_get_rocket_launcher_surface(uint32_t index)
{
	struct skin_t *skin = skin0;
		
	while(skin)
	{
		if(skin->index == index)
			return skin->rocket_launcher;
		
		skin = skin->next;
	}
	
	return NULL;
}


struct surface_t *skin_get_minigun_surface(uint32_t index)
{
	struct skin_t *skin = skin0;
		
	while(skin)
	{
		if(skin->index == index)
			return skin->minigun;
		
		skin = skin->next;
	}
	
	return NULL;
}


struct surface_t *skin_get_plasma_cannon_surface(uint32_t index)
{
	struct skin_t *skin = skin0;
		
	while(skin)
	{
		if(skin->index == index)
			return skin->plasma_cannon;
		
		skin = skin->next;
	}
	
	return NULL;
}


void init_skin()
{
	struct stat buf;
		
	struct string_t *a = new_string_string(emergence_home_dir);
	
	string_cat_text(a, "/skins/default.skin");

		
	if(stat(a->text, &buf) == -1)
	{
		struct string_t *c = new_string_text("cp ");
		string_cat_text(c, find_resource("stock-skins/default.skin"));
		string_cat_char(c, ' ');
		string_cat_string(c, a);
		
		system(c->text);
		
		free_string(c);
	}
	
	free_string(a);
}
