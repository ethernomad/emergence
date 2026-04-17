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
#include <stdio.h>
#include <stdint.h>
#include <memory.h>
#include <math.h>

#include <zlib.h>

#include "prefix.h"

#include "types.h"
#include "resource.h"

#include "cvar.h"
#include "bsp.h"
#include "objects.h"
#include "sgame.h"
#include "fileinfo.h"
#include "user.h"
#include "network.h"
#include "fileinfo.h"
#include "llist.h"
#include "stringbuf.h"
#include "buffer.h"
#include "gsub.h"
#include "render.h"
#include "game.h"
#include "shared/bsp.h"
#include "console.h"
#include "entry.h"
#include "floats.h"
#include "download.h"


struct tile_t
{
	int x, y;	// unzoomed pixels from south west origin always multiple of 50 (200?)
	struct surface_t *surface;
	struct tile_t *next;

} *tile0 = NULL;


int r_DrawBSPTree = 0;

struct string_t *map_name;
int map_size;
uint8_t map_hash[FILEINFO_DIGEST_SIZE];

int downloading_map = 0;
int map_loaded = 0;

int load_map_tiles(gzFile gzfile)
{
	LL_REMOVE_ALL(struct tile_t, &tile0);
	
	int num_tiles;

	if(gzread(gzfile, &num_tiles, 4) != 4)
		goto error;
	
	while(num_tiles--)
	{
		struct tile_t tile;

		if(gzread(gzfile, &tile.x, 4) != 4)
			goto error;

		if(gzread(gzfile, &tile.y, 4) != 4)
			goto error;

		tile.surface = gzread_raw_surface(gzfile);
		
		LL_ADD(struct tile_t, &tile0, &tile);
	}

	return 1;

error:

	return 0;
}


int generate_and_write_scaled_tiles(gzFile gzfile, gzFile gzfileout)
{
	LL_REMOVE_ALL(struct tile_t, &tile0);
	
	int num_tiles;

	if(gzread(gzfile, &num_tiles, 4) != 4)
		goto error;
	
	if(gzwrite(gzfileout, &num_tiles, 4) != 4)
		goto error;
	
	while(num_tiles--)
	{
		struct tile_t tile;

		if(gzread(gzfile, &tile.x, 4) != 4)
			goto error;

		if(gzwrite(gzfileout, &tile.x, 4) != 4)
			goto error;

		if(gzread(gzfile, &tile.y, 4) != 4)
			goto error;

		if(gzwrite(gzfileout, &tile.y, 4) != 4)
			goto error;

		tile.surface = gzread_raw_surface(gzfile);
		
		struct surface_t *surface = resize(tile.surface, tile.surface->width * vid_width / 1600, 
			tile.surface->height * vid_height / 1200);

		free_surface(tile.surface);
		tile.surface = surface;
		
		gzwrite_raw_surface(gzfileout, tile.surface);

		LL_ADD(struct tile_t, &tile0, &tile);
	}

	return 1;

error:

	return 0;
}


void bypass_objects(gzFile gzfile)
{
	uint32_t num_objects;
	
	if(gzread(gzfile, &num_objects, 4) != 4)
		return;
	
	int o;
	for(o = 0; o < num_objects; o++)
	{
		uint8_t type;
	
		if(gzread(gzfile, &type, 1) != 1)
			return;
		
		
		switch(type)
		{
		case OBJECTTYPE_PLASMACANNON:
			gzseek(gzfile, 20, SEEK_CUR);
			break;
		
		case OBJECTTYPE_MINIGUN:
			gzseek(gzfile, 20, SEEK_CUR);
			break;
		
		case OBJECTTYPE_ROCKETLAUNCHER:
			gzseek(gzfile, 20, SEEK_CUR);
			break;
		
		case OBJECTTYPE_RAILS:
			gzseek(gzfile, 20, SEEK_CUR);
			break;
		
		case OBJECTTYPE_SHIELDENERGY:
			gzseek(gzfile, 20, SEEK_CUR);
			break;
		
		case OBJECTTYPE_SPAWNPOINT:
			read_spawn_point(gzfile);
			break;
			
		case OBJECTTYPE_SPEEDUPRAMP:
			read_speedup_ramp(gzfile);
			break;
			
		case OBJECTTYPE_TELEPORTER:
			read_teleporter(gzfile);
			break;
			
		case OBJECTTYPE_GRAVITYWELL:
			read_gravity_well(gzfile);
			break;
		}
	}
}


int load_map()
{
	int local_map_size;
	uint8_t local_map_hash[FILEINFO_DIGEST_SIZE];
	int r = 0;
	
	game_rendering = 0;
	
	console_print("Loading map ");
	console_print(map_name->text);
	console_print("\n");
	
	render_frame();
	
	
	struct string_t *filename = new_string_string(emergence_home_dir);
	string_cat_text(filename, "/maps/");
	string_cat_text(filename, map_name->text);
	string_cat_text(filename, ".cmap");
	
	if(!get_file_info(filename->text, &local_map_size, local_map_hash))
	{
		r = 0;
		goto next;
	}
	
	if(local_map_size != map_size)
	{
		r = 1;
		goto next;
	}
	
	if(!compare_hashes(local_map_hash, map_hash))
	{
		r = 2;
		goto next;
	}
	
	goto okay;
	
	next:
	free_string(filename);
	struct string_t *temp = new_string_text("stock-maps/");
	string_cat_text(temp, map_name->text);
	string_cat_text(temp, ".cmap");
	
	filename = new_string_text(find_resource(temp->text));
	free_string(temp);
	
	if(!get_file_info(filename->text, &local_map_size, local_map_hash))
	{
		goto fail;
	}
	
	if(local_map_size != map_size)
	{
		if(!r)
			r = 1;
		goto fail;
	}
	
	if(!compare_hashes(local_map_hash, map_hash))
	{
		if(!r)
			r = 2;
		goto fail;
	}
	
	goto okay;
	
	
	fail:
	
	switch(r)
	{
	case 0:
		console_print("Could not find map locally.\n");
		break;
	
	case 1:
		console_print("Map found locally has wrong filesize.\n");
		break;
	
	case 2:
		console_print("Map found locally has wrong checksum.\n");
		break;
	}
	
	if(game_state == GAMESTATE_DEMO)
	{
		game_state = GAMESTATE_DEAD;
	}
	else
	{
		console_print("Attempting to download from server.\n");
		render_frame();
		download_map(map_name->text);
		downloading_map = 1;	// suspend processing of messages from server
	}
	
	return 0;

	okay:;
	
	gzFile gzfile;

	gzfile = gzopen(filename->text, "rb");



	console_print(filename->text);
	console_print("\n");
	
	free_string(filename);

	
	clear_floating_images();
	clear_sgame();
	
	uint16_t format_id;
	
	if(gzread(gzfile, &format_id, 2) != 2)
		return 0;
	
	if(format_id != EMERGENCE_COMPILEDFORMATID)
		return 0;
	
	if(vid_width == 1600)
	{
		console_print("Loading BSP tree\n");
		render_frame();
		load_bsp_tree(gzfile);
		bypass_objects(gzfile);
		console_print("Loading map tiles\n");
		render_frame();
		load_map_tiles(gzfile);
		gzread_floating_images(gzfile);
	}
	else
	{
		struct string_t *cached_filename = new_string_string(emergence_home_dir);
		string_cat_text(cached_filename, "/maps/");
		string_cat_text(cached_filename, map_name->text);
		string_cat_text(cached_filename, ".cmap");
		string_cat_text(cached_filename, "%s%u", ".cache", vid_width);
		
		console_print("Loading BSP tree\n");
		render_frame();
		load_bsp_tree(gzfile);
		bypass_objects(gzfile);
		
		gzFile gzcachedfile = gzopen(cached_filename->text, "rb");
		if(gzcachedfile)
		{
			if(gzread(gzcachedfile, &local_map_size, 4) != 4)
			{
				gzclose(gzcachedfile);
				goto cache;
			}
			
			if(local_map_size != map_size)
			{
				gzclose(gzcachedfile);
				goto cache;
			}
			
			if(gzread(gzcachedfile, local_map_hash, FILEINFO_DIGEST_SIZE) != FILEINFO_DIGEST_SIZE)
			{
				gzclose(gzcachedfile);
				goto cache;
			}
			
			if(!compare_hashes(local_map_hash, map_hash))
			{
				gzclose(gzcachedfile);
				goto cache;
			}
			
			console_print("Loading cached scaled map tiles\n");
			render_frame();
			
			load_map_tiles(gzcachedfile);
			gzread_floating_images(gzcachedfile);
		}
		else
		{
			cache:;
			
			gzcachedfile = gzopen(cached_filename->text, "w9b");
			if(!gzcachedfile)
				return 0;
			
			console_print("Scaling and caching map tiles\n");
			render_frame();
			
			gzwrite(gzcachedfile, &map_size, 4);
			gzwrite(gzcachedfile, map_hash, FILEINFO_DIGEST_SIZE);
			
			if(!generate_and_write_scaled_tiles(gzfile, gzcachedfile))
				return 0;
			
			if(!generate_and_write_scaled_floating_images(gzfile, gzcachedfile))
				return 0;
		}
		
		gzclose(gzcachedfile);
		free_string(cached_filename);
	}
	
	gzclose(gzfile);
	
	create_teleporter_sparkles();

	map_loaded = 1;
	console_print("Map loaded ok.\n");
	render_frame();
	
	game_rendering = 1;
	return 1;
}


int game_process_load_map()
{
	map_loaded = 0;
	map_name = message_reader_read_string();
	map_size = message_reader_read_uint32();
	
	int i;
	for(i = 0; i < FILEINFO_DIGEST_SIZE; i++)
		map_hash[i] = message_reader_read_uint8();
	
	load_map();
	
	return 1;
}


void game_process_map_downloaded()
{
	downloading_map = 0;
	load_map();
}


void game_process_map_download_failed()
{
	downloading_map = 0;
	console_print("Map downloading failed.\n");
}


void reload_map()
{
	if(map_name)
		load_map(map_name->text);
}


void render_radar()
{
	if(r_DrawConsole)
		return;
	
	struct blit_params_t params;
		
	params.red = r_ConsoleRed;
	params.green = r_ConsoleRed;
	params.blue = r_ConsoleRed;
	params.alpha = r_ConsoleAlpha;
	
	params.dest = s_backbuffer;
	params.dest_x = vid_width - vid_width / 4;
	params.dest_y = vid_height - vid_height / 4;
	params.width = vid_width / 4;
	params.height = vid_height / 4;

	alpha_draw_rect(&params);
}


void render_map()
{
	struct tile_t *tile = tile0;
		
	struct blit_params_t params;
		
	params.dest = s_backbuffer;

	while(tile)
	{
		int x, y;

		world_to_screen((double)tile->x, (double)tile->y, &x, &y);


		params.source = tile->surface;
	
		params.dest_x = x;
		params.dest_y = y;
	
		blit_surface(&params);

		tile = tile->next;
	}
	
	if(r_DrawBSPTree)
		draw_bsp_tree();
	
//	render_radar();
}


void init_map_cvars()
{
	create_cvar_int("r_DrawBSPTree", &r_DrawBSPTree, 0);
}
