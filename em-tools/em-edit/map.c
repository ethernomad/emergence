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
#include <unistd.h>

#include <gtk/gtk.h>
#include <zlib.h>

#include "stringbuf.h"
#include "user.h"
#include "rel2abs.h"
#include "abs2rel.h"
#include "gsub.h"
#include "nodes.h"
#include "conns.h"
#include "curves.h"
#include "points.h"
#include "lines.h"
#include "objects.h"
#include "bsp.h"
#include "tiles.h"
#include "fills.h"
#include "map.h"
#include "main.h"
#include "worker.h"
#include "glade.h"
#include "interface.h"
#include "floats.h"

uint8_t map_active = 0;
uint8_t map_saved = 1;
uint8_t compiling = 0;

GtkWidget *compiling_dialog;


struct string_t *map_filename;
struct string_t *map_path;

void clear_map()		// always called when not working
{
	delete_all_nodes();
	delete_all_conns();
	delete_all_curves();
	delete_all_points();
	delete_all_fills();
	delete_all_lines();
	delete_all_tiles();
	delete_all_objects();

	map_saved = 1;
	string_clear(map_filename);
	string_clear(map_path);
	char *cwd =  get_current_dir_name();
	string_cat_text(map_path, cwd);
	free(cwd);
}


void compile()
{
	struct string_t *compile_filename = new_string_string(emergence_home_dir);
	string_cat_text(compile_filename, "/maps/");
	
	int slashes = 0;
	char *cc = map_filename->text;
	
	while(*cc)
	{
		if(*cc++ == '/')
			slashes++;
	}
	
	cc = map_filename->text;
	
	while(slashes)
	{
		if(*cc++ == '/')
			slashes--;
	}
	
		
	while(*cc && *cc != '.')
		string_cat_char(compile_filename, *cc++);

	string_cat_text(compile_filename, ".cmap");

	gzFile file = gzopen(compile_filename->text, "wb9");

	uint16_t format_id = EMERGENCE_COMPILEDFORMATID;
	
	gzwrite(file, &format_id, 2);
	
	gzwrite_bsp_tree(file);
	gzwrite_objects_compiled(file);
	
	int num_tiles = count_tiles();
	gzwrite(file, &num_tiles, 4);

	struct tile_t *ctile = clean_tile0;

	while(ctile)
	{
		gzwrite(file, &ctile->x1, 4);
		gzwrite(file, &ctile->y2, 4);

		gzwrite_raw_surface(file, ctile->surface);

		ctile = ctile->next;
	}

	gzwrite_floating_images(file);
	gzclose(file);
}


void init_map()
{
	map_filename = new_string();
	map_path = new_string();
}


void kill_map()
{
	delete_all_nodes();
	delete_all_conns();
	delete_all_curves();
	delete_all_points();
	delete_all_fills();
	delete_all_lines();
	delete_all_tiles();
	delete_all_objects();
	
	free_string(map_filename);
	free_string(map_path);
}


void set_map_path()
{
	string_clear(map_path);
	
	// count how many '/'s are in map_filename
	
	int slashes = 0;
	char *cc = map_filename->text;
	
	while(*cc)
	{
		if(*cc++ == '/')
			slashes++;
	}
	
	cc = map_filename->text;
	
	while(slashes)
	{
		string_cat_char(map_path, *cc);
		
		if(*cc++ == '/')
			slashes--;
	}
}


struct string_t *arb_rel2abs(char *path, char *base)
{
	size_t size = 100;
	char *buffer = (char*)malloc(size);
	if(!buffer)
		return NULL;
	
	if(rel2abs(path, base, buffer, size) == buffer)
	{
		struct string_t *string = malloc(sizeof(struct string_t));
		string->text = buffer;
		return string;
	}
	
	while(1)
	{
		size *= 2;
		
		char *new_buffer = (char*)realloc(buffer, size);
		if(!new_buffer)
		{
			free(buffer);
			return NULL;
		}
		
		buffer = new_buffer;
		
		if(rel2abs(path, base, buffer, size) == buffer)
		{
			struct string_t *string = malloc(sizeof(struct string_t));
			string->text = buffer;
			return string;
		}
		
		if(errno != ERANGE)
			return NULL;
	}
}


struct string_t *arb_abs2rel(char *path, char *base)
{
	size_t size = 100;
	char *buffer = (char*)malloc(size);
	if(!buffer)
		return NULL;
	
	if(abs2rel(path, base, buffer, size) == buffer)
	{
		struct string_t *string = malloc(sizeof(struct string_t));
		string->text = buffer;
		return string;
	}
	
	while(1)
	{
		size *= 2;
		
		char *new_buffer = (char*)realloc(buffer, size);
		if(!new_buffer)
		{
			free(buffer);
			return NULL;
		}
		
		buffer = new_buffer;
		
		if(abs2rel(path, base, buffer, size) == buffer)
		{
			struct string_t *string = malloc(sizeof(struct string_t));
			string->text = buffer;
			return string;
		}
			
		if(errno != ERANGE)
			return NULL;
	}
}


void on_specify_filename_dialog_response(GtkDialog *dialog, gint response_id, gpointer user_data)
{
	;
}




int run_not_saved_dialog()
{
	GtkWidget *dialog = create_not_saved_dialog();
	
	gtk_window_set_transient_for(GTK_WINDOW(dialog), GTK_WINDOW(window));
	gtk_window_set_position(GTK_WINDOW(dialog), GTK_WIN_POS_CENTER_ON_PARENT);
	
	int ret = gtk_dialog_run(GTK_DIALOG(dialog));
	
	gtk_widget_destroy(GTK_WIDGET(dialog));
	
	return ret;
}	


void run_file_not_found_dialog(GtkWindow *parent)
{
	GtkWidget *dialog = create_file_not_found_dialog();
	
	gtk_window_set_transient_for(GTK_WINDOW(dialog), parent);
	gtk_window_set_position(GTK_WINDOW(dialog), GTK_WIN_POS_CENTER_ON_PARENT);
	
	gtk_widget_show(dialog);
	gtk_main();
}	


void run_corrupt_file_dialog(GtkWindow *parent)
{
	GtkWidget *dialog = create_corrupt_file_dialog();
	
	gtk_window_set_transient_for(GTK_WINDOW(dialog), parent);
	gtk_window_set_position(GTK_WINDOW(dialog), GTK_WIN_POS_CENTER_ON_PARENT);
	
	gtk_widget_show(dialog);
	gtk_main();
}	


void display_compile_dialog()
{
	compiling_dialog = create_map_compiling_dialog();
	
	gtk_window_set_transient_for(GTK_WINDOW(compiling_dialog), GTK_WINDOW(window));
	gtk_window_set_position(GTK_WINDOW(compiling_dialog), GTK_WIN_POS_CENTER_ON_PARENT);
	
	gtk_widget_show_all(compiling_dialog);
}	


void destroy_compile_dialog()
{
	gtk_widget_destroy(compiling_dialog);
}	


int try_load_map(char *filename)
{
	gzFile gzfile = gzopen(filename, "rb");
	if(!gzfile)
		return -1;		// do something
	
	uint16_t fork_id, format_id;
	
	if(gzread(gzfile, &fork_id, 2) != 2)
		goto error;
	
	if(gzread(gzfile, &format_id, 2) != 2)
		goto error;
	
	if(fork_id != EMERGENCE_FORKID ||
		format_id != EMERGENCE_FORMATID)
		goto error;	// do something else
	
	clear_map();
	
	string_clear(map_filename);
	string_cat_text(map_filename, filename);
	set_map_path();
	
	if(!gzread_nodes(gzfile))
	{
		printf("a\n");
		goto error;
	}

	if(!gzread_conns(gzfile))
	{
		printf("b\n");
		goto error;
	}
	
	if(!gzread_curves(gzfile))
	{
		printf("ca\n");
		goto error;
	}

	if(!gzread_points(gzfile))
	{
		printf("d\n");
		goto error;
	}

	if(!gzread_fills(gzfile))
	{
		printf("e\n");
		goto error;
	}

	if(!gzread_lines(gzfile))
	{
		printf("f\n");
		goto error;
	}

	if(!gzread_objects(gzfile))
	{
		printf("g\n");
		goto error;
	}

	gzclose(gzfile);
	
	invalidate_bsp_tree();
	map_active = 1;

	return 0;

error:

	gzclose(gzfile);
	clear_map();
	map_active = 0;
	
	return -2;
}


void open_dialog_ok(char *filename)
{
/*	gtk_widget_set_sensitive(GTK_WIDGET(fs), 0);
	
	while(gtk_events_pending())
		gtk_main_iteration_do(0);
*/	
	stop_working();
	
	switch(try_load_map(filename))
	{
	case 0:
		view_all_map();
		break;
		
	case -1:
		run_file_not_found_dialog(GTK_WINDOW(window));
		break;
	
	case -2:
		run_corrupt_file_dialog(GTK_WINDOW(window));
		break;
	}
	
/*	gtk_widget_destroy(GTK_WIDGET(fs));
	
	while(gtk_events_pending())
		gtk_main_iteration_do(0);
*/	
	update_client_area();
	
	start_working();
}


void run_open_dialog()
{
	GtkWidget *file_chooser = gtk_file_chooser_dialog_new("Open Map", GTK_WINDOW(window), 
		GTK_FILE_CHOOSER_ACTION_OPEN,
		GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
		GTK_STOCK_OPEN, GTK_RESPONSE_ACCEPT, NULL);
	
	gtk_dialog_set_default_response (GTK_DIALOG (file_chooser), GTK_RESPONSE_ACCEPT);

	GtkFileFilter *filter;
	
	/* Filters */
	filter = gtk_file_filter_new ();
	gtk_file_filter_set_name (filter, "All Emergence Maps");
	gtk_file_filter_add_pattern (filter, "*.map");
	gtk_file_chooser_add_filter (GTK_FILE_CHOOSER (file_chooser), filter);

	/* Make this filter the default */
	gtk_file_chooser_set_filter (GTK_FILE_CHOOSER (file_chooser), filter);

	filter = gtk_file_filter_new ();
	gtk_file_filter_set_name (filter, "All Files");
	gtk_file_filter_add_pattern (filter, "*");
	gtk_file_chooser_add_filter (GTK_FILE_CHOOSER (file_chooser), filter);



	

if (gtk_dialog_run (GTK_DIALOG (file_chooser)) == GTK_RESPONSE_ACCEPT)
  {
    char *filename;

    filename = gtk_file_chooser_get_filename (GTK_FILE_CHOOSER (file_chooser));
    open_dialog_ok (filename);
    g_free (filename);
  }

  
gtk_widget_destroy (file_chooser);

}


int map_save()
{
	gzFile gzfile = gzopen(map_filename->text, "wb9");
	if(!gzfile)
		return 0;
	
	uint16_t fork_id = EMERGENCE_FORKID, format_id = EMERGENCE_FORMATID;
	
	gzwrite(gzfile, &fork_id, 2);
	gzwrite(gzfile, &format_id, 2);
	
	gzwrite_nodes(gzfile);
	gzwrite_conns(gzfile);
	gzwrite_curves(gzfile);
	gzwrite_points(gzfile);
	gzwrite_fills(gzfile);
	gzwrite_lines(gzfile);
	gzwrite_objects(gzfile);

	gzclose(gzfile);

	map_saved = 1;

	return 1;
}


void save_dialog_ok(char *filename)
{
/*	gtk_widget_set_sensitive(GTK_WIDGET(file_selection), 0);
	
	while(gtk_events_pending())
		gtk_main_iteration_do(0);
*/	
	stop_working();
	
	int r = 0;
	
	if(!map_filename->text[0])
		r = 1;

	string_clear(map_filename);
	char *cc = filename;
		
	while(*cc && *cc != '.')
		string_cat_char(map_filename, *cc++);

	string_cat_text(map_filename, ".map");


	set_map_path();
	
	if(r)
		make_wall_texture_paths_relative();
	
	map_save();
	
	start_working();
}


gboolean cancel(GtkFileSelection *file_selection)
{
	gtk_main_quit();
	return 1;
}


int run_save_dialog()	// returns 0 if user canceled
{
	GtkWidget *file_chooser = gtk_file_chooser_dialog_new("Save Map", GTK_WINDOW(window), 
		GTK_FILE_CHOOSER_ACTION_SAVE,
		GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL, 
		GTK_STOCK_SAVE, GTK_RESPONSE_ACCEPT, NULL);
	
	gtk_dialog_set_default_response (GTK_DIALOG (file_chooser), GTK_RESPONSE_ACCEPT);

	GtkFileFilter *filter;
	
	/* Filters */
	filter = gtk_file_filter_new ();
	gtk_file_filter_set_name (filter, "All Emergence Maps");
	gtk_file_filter_add_pattern (filter, "*.map");
	gtk_file_chooser_add_filter (GTK_FILE_CHOOSER (file_chooser), filter);

	/* Make this filter the default */
	gtk_file_chooser_set_filter (GTK_FILE_CHOOSER (file_chooser), filter);

	filter = gtk_file_filter_new ();
	gtk_file_filter_set_name (filter, "All Files");
	gtk_file_filter_add_pattern (filter, "*");
	gtk_file_chooser_add_filter (GTK_FILE_CHOOSER (file_chooser), filter);

	gchar *current_folder = gtk_file_chooser_get_current_folder(GTK_FILE_CHOOSER (file_chooser));
	
	struct string_t *rel_filename = arb_abs2rel(map_filename->text, current_folder);

	if(rel_filename)
		gtk_file_chooser_set_current_name (GTK_FILE_CHOOSER (file_chooser), rel_filename->text);
	
	g_free(current_folder);
	free_string(rel_filename);


	int retval;

if (gtk_dialog_run (GTK_DIALOG (file_chooser)) == GTK_RESPONSE_ACCEPT)
  {
    char *filename;

    filename = gtk_file_chooser_get_filename (GTK_FILE_CHOOSER (file_chooser));
    save_dialog_ok (filename);
    g_free (filename);
	  retval = 1;
  }
  else
	  retval = 0;

  
gtk_widget_destroy (file_chooser);


return retval;
}


int run_save_first_dialog()
{
	GtkWidget *dialog = create_specify_filename_dialog();
	
	gtk_window_set_transient_for(GTK_WINDOW(dialog), GTK_WINDOW(window));
	gtk_window_set_position(GTK_WINDOW(dialog), GTK_WIN_POS_CENTER_ON_PARENT);
	
	int ret = gtk_dialog_run(GTK_DIALOG(dialog));
	
	gtk_widget_destroy(GTK_WIDGET(dialog));
	
	
	if(ret == GTK_RESPONSE_YES)
		ret = run_save_dialog();
	
	return ret;
}
