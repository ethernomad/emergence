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

#include <gdk/gdkkeysyms.h>
#include <gtk/gtk.h>

#include <assert.h>

#include "prefix.h"

#include "minmax.h"
#include "llist.h"
#include "stringbuf.h"
#include "resource.h"
#include "gsub.h"
#include "nodes.h"
#include "conns.h"
#include "curves.h"
#include "points.h"
#include "lines.h"
#include "objects.h"
#include "cameron.h"
#include "grid.h"
#include "tiles.h"
#include "bsp.h"
#include "fills.h"
#include "map.h"
#include "main.h"
#include "worker.h"
#include "interface.h"
#include "glade.h"

float zoom = 1.0;
float viewx = 0.0, viewy = 0.0;



uint16_t view_state = 0;

uint8_t view_sats_mode;


int mouse_screenx, mouse_screeny;
float mouse_worldx, mouse_worldy;

int left_button_down_screenx, left_button_down_screeny;
float left_button_down_worldx, left_button_down_worldy;

int right_button_down_screenx, right_button_down_screeny;
float right_button_down_worldx, right_button_down_worldy;

int right_button_down_rootx, right_button_down_rooty;


#define MOVEMENT_THRESHOLD 10
#define MOVEMENT_THRESHOLD_SQUARED (MOVEMENT_THRESHOLD * MOVEMENT_THRESHOLD)

uint8_t right_button_in_movement_threshold, left_button_in_movement_threshold;


#define STATE_DEAD						0x0000
#define STATE_DRAGGING_VIEW				0x0001
#define STATE_DRAGGING_NODE				0x0002
#define STATE_DRAGGING_SAT				0x0004
#define STATE_DRAGGING_POINT			0x0008
#define STATE_DRAGGING_OBJECT			0x0010
#define STATE_DEFINING_STRAIGHT_CONN	0x0020
#define STATE_DEFINING_CONIC_CONN		0x0040
#define STATE_DEFINING_BEZIER_CONN		0x0080
#define STATE_DEFINING_LINE				0x0100
#define STATE_DEFINING_FILL				0x0200
#define STATE_SELECTING					0x0400

uint16_t left_state;
uint16_t right_state;

uint8_t was_selecting;

double start_zoom;


struct node_t *dragging_node;
uint8_t dragging_sat;
struct point_t *dragging_point;
struct object_t *dragging_object;

int dragging_xoffset, dragging_yoffset;

struct node_t *joining_node = NULL;
uint8_t joining_sat;
struct point_t *joining_point = NULL;
struct line_pointer_t *fill_lines0;

struct node_pointer_t *selected_node0 = NULL;
struct object_pointer_t *selected_object0 = NULL;

#define LEFT_SHIFT	0x01
#define RIGHT_SHIFT	0x02

uint8_t shift = 0;


struct surface_t *s_select;


void world_to_screen(float worldx, float worldy, int *screenx, int *screeny)
{
	*screenx = (int)floor((worldx - viewx) * zoom) + vid_width / 2;
	*screeny = vid_height / 2 - 1 - (int)floor((worldy - viewy) * zoom);
}


void screen_to_world(int screenx, int screeny, float *worldx, float *worldy)
{
	*worldx = ((double)(screenx - vid_width / 2) + 0.5) / zoom + viewx;
	*worldy = (((double)(vid_height / 2 - 1 - screeny)) + 0.5) / zoom + viewy;
}


void GtkMenuPositionCallback(GtkMenu *menu, gint *x, gint *y, gpointer user_data)
{
	*x = right_button_down_rootx;
	*y = right_button_down_rooty;
}


void draw_world_clipped_line(float x1, float y1, float x2, float y2)
{
	double temp;
	
	if(y1 > y2)
	{
		temp = y1;
		y1 = y2;
		y2 = temp;
		
		temp = x1;
		x1 = x2;
		x2 = temp;
	}
	
	double left = (-vid_width / 2 + 0.5) / zoom + viewx;
	double bottom = (-vid_height / 2 + 0.5) / zoom + viewy;
	
	double right = (vid_width / 2 - 0.5) / zoom + viewx;
	double top = (vid_height / 2 - 0.5) / zoom + viewy;

	if((y2 < bottom) || (y1 > top))
		return;
	
	if(y1 < bottom)
	{
		x1 = x2 + (x1 - x2) * (bottom - y2) / (y1 - y2);
		y1 = bottom;
	}
	
	if(y2 > top)
	{
		x2 = x1 + (x2 - x1) * (top - y1) / (y2 - y1);
		y2 = top;
	}
	
	if(x1 < x2)
	{
		if((x2 < left) || (x1 > right))
			return;
		
		if(x1 < left)
		{
			y1 = y2 + (y1 - y2) * (left - x2) / (x1 - x2);
			x1 = left;
		}
		
		if(x2 > right)
		{
			y2 = y1 + (y2 - y1) * (right - x1) / (x2 - x1);
			x2 = right;
		}
	}
	else
	{
		if((x1 < left) || (x2 > right))
			return;
		
		if(x2 < left)
		{
			y2 = y1 + (y2 - y1) * (left - x1) / (x2 - x1);
			x2 = left;
		}
		
		if(x1 > right)
		{
			y1 = y2 + (y1 - y2) * (right - x2) / (x1 - x2);
			x1 = right;
		}
	}

	struct blit_params_t params;
	params.dest = s_backbuffer;
	params.red = 0xff;
	params.green = 0xff;
	params.blue = 0xff;
	
	world_to_screen(x1, y1, &params.x1, &params.y1);
	world_to_screen(x2, y2, &params.x2, &params.y2);
	
	draw_line(&params);
}


void run_help_dialog()
{
	GtkWidget *dialog = create_help_dialog();
	
	gtk_window_set_transient_for(GTK_WINDOW(dialog), GTK_WINDOW(window));
	gtk_window_set_position(GTK_WINDOW(dialog), GTK_WIN_POS_CENTER_ON_PARENT);
	
	gtk_widget_show(dialog);
	gtk_main();
}	


void run_about_box()
{
	static GtkWidget *about = NULL;

	if (about != NULL)
	{
		gtk_window_present (GTK_WINDOW (about));

		return;
	}

	gchar *authors[] = {
		"Jonathan Brown <jbrown@bluedroplet.com>",
		NULL
	};
	
	gchar *documenters[] = {
		NULL
	};
	
	gchar *translator_credits = "translator_credits";

	GdkPixbuf*  logo_image = gdk_pixbuf_new_from_file(find_resource("pixmaps/emergence.png"), NULL);

	
	about = gtk_about_dialog_new ();
	gtk_about_dialog_set_name (GTK_ABOUT_DIALOG (about), "Emergence Editor");
	gtk_about_dialog_set_version (GTK_ABOUT_DIALOG (about), VERSION);
	gtk_about_dialog_set_copyright (GTK_ABOUT_DIALOG (about), "Copyright \xc2\xa9 2001-2004 Jonathan Brown");
	gtk_about_dialog_set_comments (GTK_ABOUT_DIALOG (about), "Tool for creating Emergence maps");
	gtk_about_dialog_set_authors (GTK_ABOUT_DIALOG (about), (const char **)authors);
	gtk_about_dialog_set_documenters (GTK_ABOUT_DIALOG (about), (const char **)documenters);
	gtk_about_dialog_set_translator_credits (GTK_ABOUT_DIALOG (about),
				strcmp (translator_credits, "translator_credits") != 0 ? 
					(const char *)translator_credits : NULL);
	gtk_about_dialog_set_logo (GTK_ABOUT_DIALOG (about), logo_image);


	gtk_window_set_transient_for (GTK_WINDOW (about), 
			GTK_WINDOW (window));

	gtk_window_set_destroy_with_parent (GTK_WINDOW (about), TRUE);

	if (logo_image != NULL)
		g_object_unref (logo_image);
	
	g_signal_connect (G_OBJECT (about), "destroy",
			  G_CALLBACK (gtk_widget_destroyed), &about);

	gtk_widget_show (about);
}


void menu_insert_plasma_cannon()
{
	if(view_state & VIEW_GRID)
	{
		float outx, outy;
		snap_to_grid(right_button_down_worldx, right_button_down_worldy, &outx, &outy);
		insert_object(OBJECTTYPE_PLASMACANNON, outx, outy);
	}
	else
	{
		insert_object(OBJECTTYPE_PLASMACANNON, right_button_down_worldx, right_button_down_worldy);
	}

	update_client_area();
}


void menu_insert_minigun()
{
	if(view_state & VIEW_GRID)
	{
		float outx, outy;
		snap_to_grid(right_button_down_worldx, right_button_down_worldy, &outx, &outy);
		insert_object(OBJECTTYPE_MINIGUN, outx, outy);
	}
	else
	{
		insert_object(OBJECTTYPE_MINIGUN, right_button_down_worldx, right_button_down_worldy);
	}

	update_client_area();
}


void menu_insert_rocket_launcher()
{
	if(view_state & VIEW_GRID)
	{
		float outx, outy;
		snap_to_grid(right_button_down_worldx, right_button_down_worldy, &outx, &outy);
		insert_object(OBJECTTYPE_ROCKETLAUNCHER, outx, outy);
	}
	else
	{
		insert_object(OBJECTTYPE_ROCKETLAUNCHER, 
			right_button_down_worldx, right_button_down_worldy);
	}

	update_client_area();
}


void menu_insert_rails()
{
	if(view_state & VIEW_GRID)
	{
		float outx, outy;
		snap_to_grid(right_button_down_worldx, right_button_down_worldy, &outx, &outy);
		insert_object(OBJECTTYPE_RAILS, outx, outy);
	}
	else
	{
		insert_object(OBJECTTYPE_RAILS, right_button_down_worldx, right_button_down_worldy);
	}

	update_client_area();
}


void menu_insert_shield_energy()
{
	if(view_state & VIEW_GRID)
	{
		float outx, outy;
		snap_to_grid(right_button_down_worldx, right_button_down_worldy, &outx, &outy);
		insert_object(OBJECTTYPE_SHIELDENERGY, outx, outy);
	}
	else
	{
		insert_object(OBJECTTYPE_SHIELDENERGY, right_button_down_worldx, right_button_down_worldy);
	}

	update_client_area();
}


void menu_insert_spawn_point()
{
	if(view_state & VIEW_GRID)
	{
		float outx, outy;
		snap_to_grid(right_button_down_worldx, right_button_down_worldy, &outx, &outy);
		insert_object(OBJECTTYPE_SPAWNPOINT, outx, outy);
	}
	else
	{
		insert_object(OBJECTTYPE_SPAWNPOINT, right_button_down_worldx, right_button_down_worldy);
	}

	update_client_area();
}


void menu_insert_speedup_ramp()
{
	if(view_state & VIEW_GRID)
	{
		float outx, outy;
		snap_to_grid(right_button_down_worldx, right_button_down_worldy, &outx, &outy);
		insert_object(OBJECTTYPE_SPEEDUPRAMP, outx, outy);
	}
	else
	{
		insert_object(OBJECTTYPE_SPEEDUPRAMP, right_button_down_worldx, right_button_down_worldy);
	}

	update_client_area();
}


void menu_insert_teleporter()
{
	if(view_state & VIEW_GRID)
	{
		float outx, outy;
		snap_to_grid(right_button_down_worldx, right_button_down_worldy, &outx, &outy);
		insert_object(OBJECTTYPE_TELEPORTER, outx, outy);
	}
	else
	{
		insert_object(OBJECTTYPE_TELEPORTER, right_button_down_worldx, right_button_down_worldy);
	}

	update_client_area();
}


void menu_insert_gravity_well()
{
	if(view_state & VIEW_GRID)
	{
		float outx, outy;
		snap_to_grid(right_button_down_worldx, right_button_down_worldy, &outx, &outy);
		insert_object(OBJECTTYPE_GRAVITYWELL, outx, outy);
	}
	else
	{
		insert_object(OBJECTTYPE_GRAVITYWELL, right_button_down_worldx, right_button_down_worldy);
	}

	update_client_area();
}


void menu_insert_node()
{
	stop_working();
	
	if(view_state & VIEW_GRID)
	{
		float outx, outy;
		snap_to_grid(right_button_down_worldx, right_button_down_worldy, &outx, &outy);
		insert_node(outx, outy);
	}
	else
	{
		insert_node(right_button_down_worldx, right_button_down_worldy);
	}
	
	start_working();

	generate_hover_nodes();
	update_client_area();
}


void menu_fit_map()
{
	stop_working();
	scale_map_to_window();
	invalidate_all_scaled_tiles();
	invalidate_all_scaled_objects();
	start_working();
	update_client_area();
}


void menu_zoom_100()
{
	stop_working();
	zoom = 1.0;
	invalidate_all_scaled_tiles();
	invalidate_all_scaled_objects();
	start_working();
	update_client_area();
}


void menu_zoom_fix()
{
	stop_working();
	zoom = 1400.0 / 1600.0;
	invalidate_all_scaled_tiles();
	invalidate_all_scaled_objects();
	start_working();
	update_client_area();
}


void new_map()
{
	stop_working();
	clear_map();
	view_state = VIEW_OBJECTS | VIEW_TILES | VIEW_OUTLINES | 
		VIEW_SWITCHES_AND_DOORS | VIEW_NODES;
	view_sats_mode = SATS_VECT;
	LL_REMOVE_ALL(struct node_pointer_t, &selected_node0);
	LL_REMOVE_ALL(struct object_pointer_t, &selected_object0);
	clear_bsp_trees();
	zoom = 1.0;
	viewx = 0.0;
	viewy = 0.0;
	update_client_area();
}


int save_map()
{
	if(string_isempty(map_filename))
	{
		return run_save_dialog();
	}
	else
	{	
		return map_save();
	}
}


void menu_new_map()
{
	map_active = 1;
	new_map();
}


void menu_open_map()
{
	run_open_dialog();
}


void menu_active_new_map()
{
	if(!map_saved)
	{
		switch(run_not_saved_dialog())
		{
		case GTK_RESPONSE_YES:
			if(save_map())
				new_map();
			break;
		
		case GTK_RESPONSE_NO:
			new_map();
			break;
		
		case GTK_RESPONSE_CANCEL:
			break;
		}
	}
	else
	{
		new_map();
	}
}


void menu_active_open_map()
{
	if(!map_saved)
	{
		switch(run_not_saved_dialog())
		{
		case GTK_RESPONSE_YES:
			if(save_map())
				run_open_dialog();
			break;
		
		case GTK_RESPONSE_NO:
			run_open_dialog();
			break;
		
		case GTK_RESPONSE_CANCEL:
			break;
		}
	}
	else
	{
		run_open_dialog();
	}
}


void menu_active_save()
{
	map_save();
}


void menu_active_saveas()
{
	run_save_dialog();
}


void menu_active_properties()
{
	;
}


void menu_active_compile()
{
	if(compiling)
		return;

	if(!map_filename->text[0])
	{
		if(run_save_first_dialog() == GTK_RESPONSE_CANCEL)
			return;
	}

	if(compiled)
	{
		compile();
		return;
	}

	compiling = 1;

	display_compile_dialog();
}


void view_all_map()	// always called when not working
{
	view_state = VIEW_OBJECTS | VIEW_TILES | VIEW_SWITCHES_AND_DOORS;
	
	scale_map_to_window();
	invalidate_all_scaled_tiles();
	invalidate_all_scaled_objects();
}


void quit()
{
	if(!map_saved)
	{
		switch(run_not_saved_dialog())
		{
		case GTK_RESPONSE_YES:
			if(save_map())
				destroy_window();
			break;
		
		case GTK_RESPONSE_NO:
			destroy_window();
			break;
		
		case GTK_RESPONSE_CANCEL:
			break;
		}
	}
	else
	{
		destroy_window();
	}
}


void run_space_menu()
{
	GtkWidget *space_menu, *object_menu, *insert_menu, 
		*zoom_menu, *view_menu, *map_menu, *menu_items;
	
	space_menu = gtk_menu_new();
	
	if(map_active)
	{
		insert_menu = gtk_menu_new();
	
		object_menu = gtk_menu_new();
	
		menu_items = gtk_menu_item_new_with_label("Plasma Cannon");
		gtk_signal_connect(GTK_OBJECT(menu_items), "activate", 
			GTK_SIGNAL_FUNC(menu_insert_plasma_cannon), NULL);
		gtk_menu_append(GTK_MENU(object_menu), menu_items);
		gtk_widget_show(menu_items);
		
		menu_items = gtk_menu_item_new_with_label("Minigun");
		gtk_signal_connect(GTK_OBJECT(menu_items), "activate", 
			GTK_SIGNAL_FUNC(menu_insert_minigun), NULL);
		gtk_menu_append(GTK_MENU(object_menu), menu_items);
		gtk_widget_show(menu_items);
		
		menu_items = gtk_menu_item_new_with_label("Rocket Launcher");
		gtk_signal_connect(GTK_OBJECT(menu_items), "activate", 
			GTK_SIGNAL_FUNC(menu_insert_rocket_launcher), NULL);
		gtk_menu_append(GTK_MENU(object_menu), menu_items);
		gtk_widget_show(menu_items);
		
		menu_items = gtk_menu_item_new_with_label("Rails");
		gtk_signal_connect(GTK_OBJECT(menu_items), "activate", 
			GTK_SIGNAL_FUNC(menu_insert_rails), NULL);
		gtk_menu_append(GTK_MENU(object_menu), menu_items);
		gtk_widget_show(menu_items);
		
		menu_items = gtk_menu_item_new_with_label("Shield Energy");
		gtk_signal_connect(GTK_OBJECT(menu_items), "activate", 
			GTK_SIGNAL_FUNC(menu_insert_shield_energy), NULL);
		gtk_menu_append(GTK_MENU(object_menu), menu_items);
		gtk_widget_show(menu_items);
		
		menu_items = gtk_menu_item_new_with_label("Spawn Point");
		gtk_signal_connect(GTK_OBJECT(menu_items), "activate", 
			GTK_SIGNAL_FUNC(menu_insert_spawn_point), NULL);
		gtk_menu_append(GTK_MENU(object_menu), menu_items);
		gtk_widget_show(menu_items);
		
		menu_items = gtk_menu_item_new_with_label("Speedup Ramp");
		gtk_signal_connect(GTK_OBJECT(menu_items), "activate", 
			GTK_SIGNAL_FUNC(menu_insert_speedup_ramp), NULL);
		gtk_menu_append(GTK_MENU(object_menu), menu_items);
		gtk_widget_show(menu_items);
	
		menu_items = gtk_menu_item_new_with_label("Teleporter");
		gtk_signal_connect(GTK_OBJECT(menu_items), "activate", 
			GTK_SIGNAL_FUNC(menu_insert_teleporter), NULL);
		gtk_menu_append(GTK_MENU(object_menu), menu_items);
		gtk_widget_show(menu_items);
		
		menu_items = gtk_menu_item_new_with_label("Gravity Well");
		gtk_signal_connect(GTK_OBJECT(menu_items), "activate", 
			GTK_SIGNAL_FUNC(menu_insert_gravity_well), NULL);
		gtk_menu_append(GTK_MENU(object_menu), menu_items);
		gtk_widget_show(menu_items);
		
		
		menu_items = gtk_menu_item_new_with_label ("Object");
		gtk_menu_item_set_submenu(GTK_MENU_ITEM(menu_items), object_menu);
		gtk_menu_append(GTK_MENU(insert_menu), menu_items);
		gtk_widget_show(menu_items);
		
		menu_items = gtk_menu_item_new_with_label("Node");
		gtk_signal_connect(GTK_OBJECT(menu_items), "activate", 
			GTK_SIGNAL_FUNC(menu_insert_node), NULL);
		gtk_menu_append(GTK_MENU(insert_menu), menu_items);
		gtk_widget_show(menu_items);
	
		menu_items = gtk_menu_item_new_with_label ("Insert");
		gtk_menu_item_set_submenu(GTK_MENU_ITEM(menu_items), insert_menu);
		gtk_menu_append(GTK_MENU(space_menu), menu_items);
		gtk_widget_show(menu_items);
	
	
		view_menu = gtk_menu_new();
	
		menu_items = gtk_check_menu_item_new_with_label("Grid");
		gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(menu_items), view_state & VIEW_GRID);
		gtk_signal_connect(GTK_OBJECT(menu_items), "activate", 
			GTK_SIGNAL_FUNC(invert_view_grid), NULL);
		gtk_menu_append(GTK_MENU(view_menu), menu_items);
		gtk_widget_show(menu_items);
	
		menu_items = gtk_check_menu_item_new_with_label("Objects");
		gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(menu_items), view_state & VIEW_OBJECTS);
		gtk_signal_connect(GTK_OBJECT(menu_items), "activate", 
			GTK_SIGNAL_FUNC(invert_view_objects), NULL);
		gtk_menu_append(GTK_MENU(view_menu), menu_items);
		gtk_widget_show(menu_items);
		
		menu_items = gtk_check_menu_item_new_with_label("Tiles");
		gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(menu_items), view_state & VIEW_TILES);
		gtk_signal_connect(GTK_OBJECT(menu_items), "activate", 
			GTK_SIGNAL_FUNC(invert_view_tiles), NULL);
		gtk_menu_append(GTK_MENU(view_menu), menu_items);
		gtk_widget_show(menu_items);
		
		menu_items = gtk_check_menu_item_new_with_label("Boxes");
		gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(menu_items), view_state & VIEW_BOXES);
		gtk_signal_connect(GTK_OBJECT(menu_items), "activate", 
			GTK_SIGNAL_FUNC(invert_view_boxes), NULL);
		gtk_menu_append(GTK_MENU(view_menu), menu_items);
		gtk_widget_show(menu_items);
	
		menu_items = gtk_check_menu_item_new_with_label("BSP Tree");
		gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(menu_items), view_state & VIEW_BSP_TREE);
		gtk_signal_connect(GTK_OBJECT(menu_items), "activate", 
			GTK_SIGNAL_FUNC(invert_view_bsp_tree), NULL);
		gtk_menu_append(GTK_MENU(view_menu), menu_items);
		gtk_widget_show(menu_items);
		
		menu_items = gtk_check_menu_item_new_with_label("Outlines");
		gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(menu_items), view_state & VIEW_OUTLINES);
		gtk_signal_connect(GTK_OBJECT(menu_items), "activate", 
			GTK_SIGNAL_FUNC(invert_view_outlines), NULL);
		gtk_menu_append(GTK_MENU(view_menu), menu_items);
		gtk_widget_show(menu_items);
		
		menu_items = gtk_check_menu_item_new_with_label("Nodes");
		gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(menu_items), view_state & VIEW_NODES);
		gtk_signal_connect(GTK_OBJECT(menu_items), "activate", 
			GTK_SIGNAL_FUNC(invert_view_nodes), NULL);
		gtk_menu_append(GTK_MENU(view_menu), menu_items);
		gtk_widget_show(menu_items);
	
		menu_items = gtk_separator_menu_item_new();
		gtk_menu_append(GTK_MENU(view_menu), menu_items);
		gtk_widget_show(menu_items);
	
		GtkWidget *vector_radio = gtk_radio_menu_item_new_with_label(NULL, "Vector Sats");
		GSList *group = gtk_radio_menu_item_get_group(GTK_RADIO_MENU_ITEM(vector_radio));
		gtk_menu_append(GTK_MENU(view_menu), vector_radio);
		gtk_widget_show(vector_radio);
		
		GtkWidget *width_radio = gtk_radio_menu_item_new_with_label(group, "Width Sats");
		gtk_menu_append(GTK_MENU(view_menu), width_radio);
		gtk_widget_show(width_radio);

		gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(vector_radio), 
			view_sats_mode == SATS_VECT);
		gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(width_radio), 
			view_sats_mode == SATS_WIDTH);
		gtk_signal_connect(GTK_OBJECT(vector_radio), "activate", 
			GTK_SIGNAL_FUNC(view_vect_sats), NULL);
		gtk_signal_connect(GTK_OBJECT(width_radio), "activate", 
			GTK_SIGNAL_FUNC(view_width_sats), NULL);


		menu_items = gtk_menu_item_new_with_label ("View");
		gtk_menu_item_set_submenu(GTK_MENU_ITEM(menu_items), view_menu);
		gtk_menu_append(GTK_MENU(space_menu), menu_items);
		gtk_widget_show(menu_items);
		
		
		zoom_menu = gtk_menu_new();
	
		menu_items = gtk_menu_item_new_with_label("Fit Map");
		gtk_signal_connect(GTK_OBJECT(menu_items), "activate", 
			GTK_SIGNAL_FUNC(menu_fit_map), NULL);
		gtk_menu_append(GTK_MENU(zoom_menu), menu_items);
		gtk_widget_show(menu_items);
		
	/*	menu_items = gtk_menu_item_new_with_label("0.8");
		gtk_signal_connect(GTK_OBJECT(menu_items), "activate", 
			GTK_SIGNAL_FUNC(menu_zoom_fix), NULL);
		gtk_menu_append(GTK_MENU(zoom_menu), menu_items);
		gtk_widget_show(menu_items);
	*/
		menu_items = gtk_menu_item_new_with_label("1.0");
		gtk_signal_connect(GTK_OBJECT(menu_items), "activate", 
			GTK_SIGNAL_FUNC(menu_zoom_100), NULL);
		gtk_menu_append(GTK_MENU(zoom_menu), menu_items);
		gtk_widget_show(menu_items);
		
		menu_items = gtk_menu_item_new_with_label ("Zoom");
		gtk_menu_item_set_submenu(GTK_MENU_ITEM(menu_items), zoom_menu);
		gtk_menu_append(GTK_MENU(space_menu), menu_items);
		gtk_widget_show(menu_items);
	}
	
	if(!map_active)
	{
		menu_items = gtk_menu_item_new_with_label ("New Map");
		gtk_signal_connect(GTK_OBJECT(menu_items), "activate", 
			GTK_SIGNAL_FUNC(menu_new_map), NULL);
		gtk_menu_append(GTK_MENU(space_menu), menu_items);
		gtk_widget_show(menu_items);
	
		menu_items = gtk_menu_item_new_with_label ("Open...");
		gtk_signal_connect(GTK_OBJECT(menu_items), "activate", 
			GTK_SIGNAL_FUNC(menu_open_map), NULL);
		gtk_menu_append(GTK_MENU(space_menu), menu_items);
		gtk_widget_show(menu_items);
	}

	
	if(map_active)
	{
		map_menu = gtk_menu_new();		
		
		menu_items = gtk_menu_item_new_with_label ("Clear...");
		gtk_signal_connect(GTK_OBJECT(menu_items), "activate", 
			GTK_SIGNAL_FUNC(menu_active_new_map), NULL);
		gtk_menu_append(GTK_MENU(map_menu), menu_items);
		gtk_widget_show(menu_items);
	
		menu_items = gtk_menu_item_new_with_label ("Open...");
		gtk_signal_connect(GTK_OBJECT(menu_items), "activate", 
			GTK_SIGNAL_FUNC(menu_active_open_map), NULL);
		gtk_menu_append(GTK_MENU(map_menu), menu_items);
		gtk_widget_show(menu_items);
		
		if(map_filename->text[0])
		{
			menu_items = gtk_menu_item_new_with_label ("Save");
			gtk_signal_connect(GTK_OBJECT(menu_items), "activate", 
				GTK_SIGNAL_FUNC(menu_active_save), NULL);
			gtk_menu_append(GTK_MENU(map_menu), menu_items);
			gtk_widget_show(menu_items);
		}
		
		menu_items = gtk_menu_item_new_with_label("Save As...");
		gtk_signal_connect(GTK_OBJECT(menu_items), "activate", 
			GTK_SIGNAL_FUNC(menu_active_saveas), NULL);
		gtk_menu_append(GTK_MENU(map_menu), menu_items);
		gtk_widget_show(menu_items);
		
/*		menu_items = gtk_menu_item_new_with_label("Properties...");
		gtk_signal_connect(GTK_OBJECT(menu_items), "activate", 
			GTK_SIGNAL_FUNC(menu_active_saveas), NULL);
		gtk_menu_append(GTK_MENU(map_menu), menu_items);
		gtk_widget_show(menu_items);
*/		
		menu_items = gtk_menu_item_new_with_label("Compile");
		gtk_signal_connect(GTK_OBJECT(menu_items), "activate", 
			GTK_SIGNAL_FUNC(menu_active_compile), NULL);
		gtk_menu_append(GTK_MENU(map_menu), menu_items);
		gtk_widget_show(menu_items);
		
		menu_items = gtk_menu_item_new_with_label ("Map");
		gtk_menu_item_set_submenu(GTK_MENU_ITEM(menu_items), map_menu);
		gtk_menu_append(GTK_MENU(space_menu), menu_items);
		gtk_widget_show(menu_items);
	}

	
	menu_items = gtk_menu_item_new_with_label("Help...");
	gtk_signal_connect(GTK_OBJECT(menu_items), "activate", 
		GTK_SIGNAL_FUNC(run_help_dialog), NULL);
	gtk_menu_append(GTK_MENU(space_menu), menu_items);
	gtk_widget_show(menu_items);
	
	menu_items = gtk_menu_item_new_with_label("About...");
	gtk_signal_connect(GTK_OBJECT(menu_items), "activate", 
		GTK_SIGNAL_FUNC(run_about_box), NULL);
	gtk_menu_append(GTK_MENU(space_menu), menu_items);
	gtk_widget_show(menu_items);
	
	menu_items = gtk_menu_item_new_with_label("Quit");
	gtk_signal_connect(GTK_OBJECT(menu_items), "activate", 
		GTK_SIGNAL_FUNC(quit), NULL);
	gtk_menu_append(GTK_MENU(space_menu), menu_items);
	gtk_widget_show(menu_items);
	
	gtk_menu_popup(GTK_MENU(space_menu), NULL, NULL, 
		(GtkMenuPositionFunc)(GtkMenuPositionCallback), NULL, 0, 0);
}


void draw_selection_box()
{
	draw_world_clipped_line(left_button_down_worldx, left_button_down_worldy, 
		mouse_worldx, left_button_down_worldy);
	
	draw_world_clipped_line(mouse_worldx, left_button_down_worldy, 
		mouse_worldx, mouse_worldy);

	draw_world_clipped_line(mouse_worldx, mouse_worldy, 
		left_button_down_worldx, mouse_worldy);

	draw_world_clipped_line(left_button_down_worldx, mouse_worldy, 
		left_button_down_worldx, left_button_down_worldy);
}


void draw_all()
{
	if(!map_active)
	{
		draw_cameron();
		return;
	}
	
	if(view_state & VIEW_GRID)
		draw_grid();

	if(view_state & VIEW_OBJECTS)
		draw_objects();
	
	if(!(left_state & (STATE_DRAGGING_NODE | STATE_DRAGGING_SAT | STATE_DRAGGING_POINT | 
		STATE_DEFINING_STRAIGHT_CONN | STATE_DEFINING_CONIC_CONN | STATE_DEFINING_BEZIER_CONN | 
		STATE_DEFINING_LINE | STATE_DEFINING_FILL)))
	{
		if(view_state & VIEW_TILES)
			draw_tiles();
		
		if(view_state & VIEW_BOXES)
			draw_boxes();
	}

	if(view_state & VIEW_BSP_TREE)
		draw_bsp_tree();
	
	if(view_state & VIEW_OUTLINES || left_state & (STATE_DRAGGING_NODE | STATE_DRAGGING_SAT | 
		STATE_DRAGGING_POINT | STATE_DEFINING_STRAIGHT_CONN | STATE_DEFINING_CONIC_CONN | 
		STATE_DEFINING_BEZIER_CONN | STATE_DEFINING_LINE | STATE_DEFINING_FILL))
	{
		draw_curve_outlines();
		
		draw_fills();
		
		if(view_state & VIEW_SWITCHES_AND_DOORS)
			draw_some_lines();
		else
			draw_all_lines();
	}

	if(view_state & VIEW_SWITCHES_AND_DOORS)
		draw_switches_and_doors();
	
	if(left_state == STATE_SELECTING)
		draw_selection_box();
	
	if(view_state & VIEW_NODES)
	{
		switch(view_sats_mode)
		{
		case SATS_VECT:
			draw_sat_lines();
			break;
	
		case SATS_WIDTH:
			draw_width_sat_lines();
			break;
		}
		
		draw_nodes();
		
		switch(view_sats_mode)
		{
		case SATS_VECT:
			draw_sats();
			break;
	
		case SATS_WIDTH:
			draw_width_sats();
			break;
		}
		
		draw_points();
	}
}


void invert_view_grid()
{
	view_state ^= VIEW_GRID;
	
	if(!mouse_move(mouse_screenx, mouse_screeny))
		update_client_area();
}	


void invert_view_objects()
{
	view_state ^= VIEW_OBJECTS;

	if(left_state == STATE_DRAGGING_OBJECT)
		left_state = STATE_DEAD;
	
	update_client_area();
}	


void invert_view_tiles()
{
	view_state ^= VIEW_TILES;
	update_client_area();
}	


void invert_view_boxes()
{
	view_state ^= VIEW_BOXES;
	update_client_area();
}	


void invert_view_bsp_tree()
{
	view_state ^= VIEW_BSP_TREE;
	update_client_area();
}	


void invert_view_outlines()
{
	view_state ^= VIEW_OUTLINES;

	if(left_state == STATE_DEFINING_FILL)
		left_state = STATE_DEAD;
	
	update_client_area();
}	


void invert_view_switches_and_doors()
{
	view_state ^= VIEW_SWITCHES_AND_DOORS;

	if(!(view_state & VIEW_OUTLINES) && (left_state == STATE_DEFINING_FILL))
		left_state = STATE_DEAD;
	
	update_client_area();
}	


void invert_view_nodes()
{
	view_state ^= VIEW_NODES;

	if(left_state & (STATE_DRAGGING_NODE | STATE_DRAGGING_SAT | STATE_DRAGGING_POINT | 
		STATE_DEFINING_STRAIGHT_CONN | STATE_DEFINING_CONIC_CONN | STATE_DEFINING_BEZIER_CONN | 
		STATE_DEFINING_LINE))
	{
		left_state = STATE_DEAD;
	}
	
	update_client_area();
}


void view_vect_sats()
{
	view_sats_mode = SATS_VECT;
	update_client_area();
}


void view_width_sats()
{
	view_sats_mode = SATS_WIDTH;
	update_client_area();
}


void key_pressed(uint32_t key)
{
	switch(key)
	{
	case GDK_Shift_L:
		shift |= LEFT_SHIFT;
		return;
		
	case GDK_Shift_R:
		shift |= RIGHT_SHIFT;
		return;
	}
		
			
	if(!map_active)
		return;
	

	switch(key)
	{
	case GDK_g:
	case GDK_G:
		invert_view_grid();
		break;
	
	case GDK_o:
	case GDK_O:
		invert_view_objects();
		break;

	case GDK_t:
	case GDK_T:
		invert_view_tiles();
		break;

	case GDK_x:
	case GDK_X:
		invert_view_boxes();
		break;
		
	case GDK_b:
	case GDK_B:
		invert_view_bsp_tree();
		break;
	
	case GDK_c:
	case GDK_C:
		invert_view_outlines();
		break;
	
	case GDK_d:
	case GDK_D:
		invert_view_switches_and_doors();
		break;
	
	case GDK_n:
	case GDK_N:
		invert_view_nodes();
		break;
		
	case GDK_s:
	case GDK_S:
//		view_vect_sats();
		break;
		
	case GDK_w:
	case GDK_W:
//		view_width_sats();
		break;

	case GDK_bracketleft:
	case GDK_braceleft:
		increase_grid_granularity();
		update_client_area();
		break;

	case GDK_bracketright:
	case GDK_braceright:
		decrease_grid_granularity();
		update_client_area();
		break;

	case GDK_less:
	case GDK_comma:
		less_bsp();
		update_client_area();
		break;

	case GDK_greater:
	case GDK_period:
		more_bsp();
		update_client_area();
		break;
	}
}


void key_released(uint32_t key)
{
	switch(key)
	{
	case GDK_Shift_L:
		shift &= ~LEFT_SHIFT;
		break;
	
	case GDK_Shift_R:
		shift &= ~RIGHT_SHIFT;
		break;
	}
}


void update_movement_thresholds()
{
	int deltax, deltay;
	double dist_squared;
	
	if(left_button_in_movement_threshold)
	{
		deltax = mouse_screenx - left_button_down_screenx;
		deltay = mouse_screeny - left_button_down_screeny;
	
		dist_squared = (double)deltax * (double)deltax + (double)deltay * (double)deltay;
	
		if(dist_squared > MOVEMENT_THRESHOLD_SQUARED)
			left_button_in_movement_threshold = 0;
	}
	
	if(right_button_in_movement_threshold)
	{
		deltax = mouse_screenx - right_button_down_screenx;
		deltay = mouse_screeny - right_button_down_screeny;
		
		dist_squared = (double)deltax * (double)deltax + (double)deltay * (double)deltay;
	
		if(dist_squared > MOVEMENT_THRESHOLD_SQUARED)
			right_button_in_movement_threshold = 0;
	}
}


void left_button_down(int x, int y)
{
	left_button_down_screenx = mouse_screenx = x;
	left_button_down_screeny = mouse_screeny = y;
	
	screen_to_world(x, y, &mouse_worldx, &mouse_worldy);

	left_button_down_worldx = mouse_worldx;
	left_button_down_worldy = mouse_worldy;
	
	update_movement_thresholds();
	left_button_in_movement_threshold = 1;

	if(!map_active)
		return;
	
	if(right_state == STATE_DRAGGING_VIEW)
	{
		left_state = STATE_DRAGGING_VIEW;
		start_zoom = zoom;
		return;
	}
	
	
	struct node_t *node;
	uint8_t sat;
	struct point_t *point;
	struct conn_t *conn;
	struct object_t *object;
	
	switch(left_state)
	{
	case STATE_DEAD:
		
		if(view_state & VIEW_NODES)
		{
			// see if we are clicking on a point
			
			point = get_point(x, y, &dragging_xoffset, &dragging_yoffset);
			
			if(point)
			{
				left_state = STATE_DRAGGING_POINT;
				dragging_point = point;
				break;
			}
			
			
			// see if we are clicking on a satellite
		
			switch(view_sats_mode)
			{
			case SATS_VECT:
				get_satellite(x, y, &node, &sat, &dragging_xoffset, &dragging_yoffset);
				break;
					
			case SATS_WIDTH:
				get_width_sat(x, y, &node, &sat, &dragging_xoffset, &dragging_yoffset);
				break;
			}

			if(node)
			{
				left_state = STATE_DRAGGING_SAT;
				dragging_node = node;
				dragging_sat = sat;
				update_client_area();
				break;
			}
			
			
			// see if we are clicking on a node
		
			node = get_node(x, y, &dragging_xoffset, &dragging_yoffset);
		
			if(node)
			{
				if(shift)
				{
					if(node_in_node_pointer_list(selected_node0, node))
					{
						remove_node_pointer(&selected_node0, node);
					}
					else
					{
						add_node_pointer(&selected_node0, node);
						left_state = STATE_DRAGGING_NODE;
						dragging_node = node;
						was_selecting = 1;
					}
				}
				else
				{
					if(!node_in_node_pointer_list(selected_node0, node))
					{
						LL_REMOVE_ALL(struct node_pointer_t, &selected_node0);
						LL_REMOVE_ALL(struct object_pointer_t, &selected_object0);
					}
					
					left_state = STATE_DRAGGING_NODE;
					dragging_node = node;
					was_selecting = 0;
				}
				
				update_client_area();
				break;
			}
		}
		
		
		if(view_state & (VIEW_TILES | VIEW_OUTLINES))
		{
			// see if we are clicking on a curve
			
			conn = NULL;//get_conn(left_button_down_worldx, left_button_down_worldy);
			
			if(conn)
			{
				struct curve_t *curve = get_curve(conn);
				assert(curve);
				
				if(!shift)
				{
					LL_REMOVE_ALL(struct node_pointer_t, &selected_node0);
					LL_REMOVE_ALL(struct object_pointer_t, &selected_object0);
				}
					
				
				struct conn_pointer_t *cconnp = curve->connp0;
					
				while(cconnp)
				{
					add_node_pointer(&selected_node0, cconnp->conn->node1);		// inefficient
					add_node_pointer(&selected_node0, cconnp->conn->node2);
					cconnp = cconnp->next;
				}
				
				
				left_state = STATE_DRAGGING_NODE;
				
				dragging_node = curve->connp0->conn->node1;
				
				int node_screenx, node_screeny;
				world_to_screen(curve->connp0->conn->node1->x, curve->connp0->conn->node1->y, 
					&node_screenx, &node_screeny);
				
				dragging_xoffset = x - node_screenx;
				dragging_yoffset = y - node_screeny;
				
				update_client_area();
				break;
			}
		}
		
		
		if(view_state & VIEW_OBJECTS)
		{
			// see if we are clicking on an object
			
			object = get_object(x, y, &dragging_xoffset, &dragging_yoffset);
			
			if(object)
			{
				if(shift)
				{
					if(object_in_object_pointer_list(selected_object0, object))
					{
						remove_object_pointer(&selected_object0, object);
					}
					else
					{
						add_object_pointer(&selected_object0, object);
						left_state = STATE_DRAGGING_OBJECT;
						dragging_object = object;
						was_selecting = 1;
					}
				}
				else
				{
					if(!object_in_object_pointer_list(selected_object0, object))
					{
						LL_REMOVE_ALL(struct node_pointer_t, &selected_node0);
						LL_REMOVE_ALL(struct object_pointer_t, &selected_object0);
					}
					
					left_state = STATE_DRAGGING_OBJECT;
					dragging_object = object;
					was_selecting = 0;
				}
				
				update_client_area();
				break;
			}
		}
		
		
		// not clicking on anything in particular, so start a selection box
		
		left_state = STATE_SELECTING;
		break;

	
	case STATE_DEFINING_STRAIGHT_CONN:
		
		node = get_node(x, y, NULL, NULL);
	
		if(node)
		{

			if(insert_straight_conn(joining_node, node))
			{
				map_saved = 0;
				update_client_area();
			}
		}
		
		break;
	
		
	case STATE_DEFINING_CONIC_CONN:
		
		node = get_node(x, y, NULL, NULL);
	
		if(node)
		{
			if(insert_conic_conn(joining_node, node))
			{
				map_saved = 0;
				update_client_area();
			}
		}
		
		break;
		
	
	case STATE_DEFINING_BEZIER_CONN:
		
		node = get_node(x, y, NULL, NULL);
	
		if(node)
		{
			if(insert_bezier_conn(joining_node, node))
			{
				map_saved = 0;
				update_client_area();
			}
		}
		
		break;
		
	
	case STATE_DEFINING_LINE:
		
		point = get_point(x, y, NULL, NULL);
			
		if(point)
		{
			insert_line(joining_point, point);
			map_saved = 0;
			joining_point = point;
			update_client_area();
		}
		
		break;
	
		
	case STATE_DEFINING_FILL:
	
		point = get_point(x, y, NULL, NULL);
			
		if(point)
		{
			// add point to fill
			add_point_to_fill(point);
			update_client_area();
		}
	
		break;
	}
}


void left_button_up(int x, int y)
{
	mouse_screenx = x;
	mouse_screeny = y;
	
	screen_to_world(x, y, &mouse_worldx, &mouse_worldy);

	update_movement_thresholds();
	
	if(!map_active)
		return;
	
	int update = 0;

	switch(left_state)
	{
	case STATE_DRAGGING_VIEW:
		right_button_down_worldx = mouse_worldx;
		right_button_down_worldy = mouse_worldy;
		left_state = STATE_DEAD;
		break;
	
	case STATE_DRAGGING_NODE:
		stop_working();
	
		// if we are dragging selected nodes, then invalidate all of them
	
		if(selected_node0)
		{
			struct node_pointer_t *nodep = selected_node0;
				
			while(nodep)
			{
				invalidate_conns_with_node(nodep->node);
				invalidate_node(nodep->node);
				nodep = nodep->next;
			}
		}
		else	// otherwise just invalidate the single one
		{
			invalidate_conns_with_node(dragging_node);
			invalidate_node(dragging_node);
		}
			
		update_point_positions();
		invalidate_bsp_tree();
		make_sure_all_fills_are_clockwise();
		start_working();

		
		if(left_button_in_movement_threshold && selected_node0 && !was_selecting)
		{
			LL_REMOVE_ALL(struct node_pointer_t, &selected_node0);
			LL_REMOVE_ALL(struct object_pointer_t, &selected_object0);
		}
		
		left_state = STATE_DEAD;
		update = 1;
		break;
	
	case STATE_DRAGGING_SAT:
		// invalidate shit
	
		stop_working();
		invalidate_conns_with_node(dragging_node);
		invalidate_node(dragging_node);
		update_point_positions();
		invalidate_bsp_tree();
		make_sure_all_fills_are_clockwise();
		start_working();
		
		left_state = STATE_DEAD;
		update = 1;
		break;
	
	case STATE_DRAGGING_POINT:
		stop_working();
		make_sure_all_fills_are_clockwise();
		invalidate_bsp_tree();
		invalidate_fills_with_point(dragging_point);
		start_working();

		left_state = STATE_DEAD;
		update = 1;
		break;
	
	case STATE_DRAGGING_OBJECT:
		
		if(left_button_in_movement_threshold && selected_object0 && !was_selecting)
		{
			LL_REMOVE_ALL(struct node_pointer_t, &selected_node0);
			LL_REMOVE_ALL(struct object_pointer_t, &selected_object0);
		}
		
		left_state = STATE_DEAD;
		update = 1;
		break;
	
	case STATE_SELECTING:
		// drop a node if mouse has not been moved outside threshold
		
		if(left_button_in_movement_threshold)
		{
			if(view_state & VIEW_GRID)
			{
				float outx, outy;
				snap_to_grid(left_button_down_worldx, left_button_down_worldy, &outx, &outy);
				insert_node(outx, outy);
			}
			else
			{
				insert_node(left_button_down_worldx, left_button_down_worldy);
			}
			
			generate_hover_nodes();
		}
			
		left_state = STATE_DEAD;
		update = 1;
		break;
		
	case STATE_DEFINING_STRAIGHT_CONN:
		left_state = STATE_DEAD;
		break;
		
	case STATE_DEFINING_CONIC_CONN:
		left_state = STATE_DEAD;
		break;
		
	case STATE_DEFINING_BEZIER_CONN:
		left_state = STATE_DEAD;
		break;
		
	case STATE_DEFINING_LINE:
		break;
		
	case STATE_DEFINING_FILL:
		break;
	}	
	
	if(update)
		update_client_area();
}


void right_button_down(int x, int y, int rootx, int rooty)
{
	right_button_down_screenx = mouse_screenx = x;
	right_button_down_screeny = mouse_screeny = y;
	
	screen_to_world(x, y, &mouse_worldx, &mouse_worldy);
	
	right_button_down_worldx = mouse_worldx;
	right_button_down_worldy = mouse_worldy;
	
	right_button_down_rootx = rootx;
	right_button_down_rooty = rooty;
	
	update_movement_thresholds();
	right_button_in_movement_threshold = 1;
	
	if(!map_active)
		return;
	
	right_state = STATE_DRAGGING_VIEW;
	
	if(left_state == STATE_SELECTING)
	{
		left_state = STATE_DRAGGING_VIEW;
		left_button_down_worldx = right_button_down_worldx;
		left_button_down_worldy = right_button_down_worldy;
		start_zoom = zoom;
	}
}


void right_button_up(int x, int y)
{
	mouse_screenx = x;
	mouse_screeny = y;
	
	screen_to_world(x, y, &mouse_worldx, &mouse_worldy);

	update_movement_thresholds();
	
	if(!map_active)
	{
		if(right_button_in_movement_threshold)
			run_space_menu();
		
		return;
	}

	struct node_t *node;
	uint8_t sat;
	struct point_t *point;
	struct curve_t *curve;
	struct fill_t *fill;
	struct object_t *object;
	struct line_t *line;
	
	
	// throw up menu if mouse has not been moved outside threshold

	if(right_button_in_movement_threshold)
	{
		if(left_state == STATE_DEFINING_FILL)
		{
			stop_defining_fill();
			update_client_area();
		}
		
		left_state = STATE_DEAD;
		
		if(view_state & VIEW_NODES)
		{
			node = get_node(right_button_down_screenx, right_button_down_screeny, NULL, NULL);
			
			if(node)
			{
				run_node_menu(node);
				goto end;
			}
		
		
			get_satellite(x, y, &node, &sat, NULL, NULL);
			
			if(node)
			{
				run_sat_menu(node, sat);
				goto end;
			}
			
			
			point = get_point(x, y, NULL, NULL);
				
			if(point)
			{
				run_point_menu(point);
				goto end;
			}
		}
		
		if(view_state & (VIEW_TILES | VIEW_OUTLINES))
		{
			line = get_line(right_button_down_worldx, right_button_down_worldy);
			
			if(line)
			{
				run_line_menu(line);
				goto end;
			}
			
			curve = get_curve_bsp(right_button_down_worldx, right_button_down_worldy);
				
			if(curve)
			{
				run_curve_menu(curve);
				goto end;
			}
			
			fill = get_fill_bsp(right_button_down_worldx, right_button_down_worldy);
				
			if(fill)
			{
				run_fill_menu(fill);
				goto end;
			}
		}
	
		if(view_state & VIEW_OBJECTS)
		{
			object = get_object(right_button_down_screenx, right_button_down_screeny, NULL, NULL);
				
			if(object)
			{
				run_object_menu(object);
				goto end;
			}
		}
		
		run_space_menu();
	}

	end:
	// the view cannot be dragged

	right_state = STATE_DEAD;
}


void mouse_wheel_up(int x, int y)
{	
	mouse_screenx = x;
	mouse_screeny = y;
	
	update_movement_thresholds();
	
	if(!map_active || (left_state == STATE_DRAGGING_VIEW && right_state == STATE_DRAGGING_VIEW))
	{
		screen_to_world(x, y, &mouse_worldx, &mouse_worldy);
		return;
	}

	float wx, wy, nwx, nwy;
	screen_to_world(x, y, &wx, &wy);

	stop_working();
	zoom *= 2;
	invalidate_all_scaled_tiles();
	invalidate_all_scaled_objects();
	start_working();

	screen_to_world(x, y, &nwx, &nwy);

	viewx -= nwx - wx;
	viewy -= nwy - wy;

	screen_to_world(x, y, &mouse_worldx, &mouse_worldy);
	
	update_fully_rendered_conns();
	generate_hover_nodes();

	update_client_area();
}


void mouse_wheel_down(int x, int y)
{
	mouse_screenx = x;
	mouse_screeny = y;
	
	update_movement_thresholds();
	
	if(!map_active || (left_state == STATE_DRAGGING_VIEW && right_state == STATE_DRAGGING_VIEW))
	{
		screen_to_world(x, y, &mouse_worldx, &mouse_worldy);
		return;
	}
	
	float wx, wy, nwx, nwy;
	screen_to_world(x, y, &wx, &wy);

	stop_working();
	zoom /= 2;
	invalidate_all_scaled_tiles();
	invalidate_all_scaled_objects();
	start_working();

	screen_to_world(x, y, &nwx, &nwy);

	viewx -= nwx - wx;
	viewy -= nwy - wy;

	screen_to_world(x, y, &mouse_worldx, &mouse_worldy);
	
	update_fully_rendered_conns();
	generate_hover_nodes();

	update_client_area();
}


void move_selected(double dragx, double dragy)
{
	// move all selected nodes by drag distance
	
	struct node_pointer_t *cnodep = selected_node0;
	
	while(cnodep)
	{
		cnodep->node->x += dragx;
		cnodep->node->y += dragy;
		
		cnodep = cnodep->next;
	}
	
	
	cnodep = selected_node0;
		
	while(cnodep)
	{
		straighten_from_node(cnodep->node);
		fix_conic_conns_from_node(dragging_node);
		cnodep = cnodep->next;
	}
	
	
	cnodep = selected_node0;
		
	while(cnodep)
	{
		make_node_effective(cnodep->node);
		cnodep = cnodep->next;
	}

	// move all selected objects by drag distance
	
	struct object_pointer_t *cobjectp = selected_object0;
	
	while(cobjectp)
	{
		cobjectp->object->x += dragx;
		cobjectp->object->y += dragy;
		
		cobjectp = cobjectp->next;
	}
}


int mouse_move(int x, int y)
{
	mouse_screenx = x;
	mouse_screeny = y;

	screen_to_world(x, y, &mouse_worldx, &mouse_worldy);
	
	update_movement_thresholds();
	
	if(!map_active)
		return 0;

	// if we are dragging the view, then update the view coords accordingly

	if(right_state == STATE_DRAGGING_VIEW)
	{
		if(left_state == STATE_DRAGGING_VIEW)
		{
			float wx, wy, nwx, nwy;
			screen_to_world(left_button_down_screenx, left_button_down_screeny, &wx, &wy);
		
			stop_working();
			zoom = start_zoom / pow(1.01, mouse_screeny - left_button_down_screeny);
			invalidate_all_scaled_tiles();
			invalidate_all_scaled_objects();
			start_working();
		
			screen_to_world(left_button_down_screenx, left_button_down_screeny, &nwx, &nwy);
		
			viewx -= nwx - wx;
			viewy -= nwy - wy;
		
			update_fully_rendered_conns();
		}
		else
		{
			float wx, wy;
			screen_to_world(x, y, &wx, &wy);
			
			viewx -= wx - right_button_down_worldx;
			viewy -= wy - right_button_down_worldy;
		}
		
		generate_hover_nodes();
		update_client_area();
		return 1;
	}
	
	int update = 0;
	float worldx, worldy;
	
	
	switch(left_state)
	{
	case STATE_DRAGGING_NODE:
		// get world coords from screen coords
		
		screen_to_world(x - dragging_xoffset, y - dragging_yoffset, &worldx, &worldy);
		
		if(view_state & VIEW_GRID)
			snap_to_grid(worldx, worldy, &worldx, &worldy);
		
		
		switch(job_type)
		{
		case JOB_TYPE_BSP:
		case JOB_TYPE_FILL_VERTICIES:
		case JOB_TYPE_CONN_VERTICIES:
		case JOB_TYPE_CONN_SQUISH:
			stop_working();
		}
		
		
		// a race its a race i hope i win
		
		if(selected_node0)
		{
			move_selected(worldx - dragging_node->x, worldy - dragging_node->y);
		}
		else
		{
			dragging_node->x = worldx;
			dragging_node->y = worldy;
			straighten_from_node(dragging_node);
			fix_conic_conns_from_node(dragging_node);
			make_node_effective(dragging_node);
		}
		
		generate_t_values_for_conns_with_node(dragging_node);
		update_point_positions();
		
		switch(job_type)
		{
		case JOB_TYPE_BSP:
		case JOB_TYPE_FILL_VERTICIES:
		case JOB_TYPE_CONN_VERTICIES:
		case JOB_TYPE_CONN_SQUISH:
			start_working();
		}
		
		map_saved = 0;
		update = 1;
		break;
	
	case STATE_DRAGGING_SAT:

		// get world coords from screen coords
		
		screen_to_world(x - dragging_xoffset, y - dragging_yoffset, &worldx, &worldy);
		
		if(view_state & VIEW_GRID)
			snap_to_grid(worldx, worldy, &worldx, &worldy);
		
	//	stop_working();		// use a separate lock for this?
		
		switch(job_type)
		{
		case JOB_TYPE_BSP:
		case JOB_TYPE_FILL_VERTICIES:
		case JOB_TYPE_CONN_VERTICIES:
		case JOB_TYPE_CONN_SQUISH:
			stop_working();
		}
		
		// a race a race you have to pace yourself 
		
		switch(view_sats_mode)
		{
		case SATS_VECT:
			
			if(node_in_straight_conn(dragging_node))
			{
				set_sat_dist(dragging_node, dragging_sat, 
					worldx - dragging_node->x, worldy - dragging_node->y);
			}
			else
			{
				dragging_node->sats[dragging_sat].x = worldx - dragging_node->x;
				dragging_node->sats[dragging_sat].y = worldy - dragging_node->y;
	
				
				// update the rest of the satellites on this node
	
				fix_satellites(dragging_node, dragging_sat);
				straighten_from_node(dragging_node);
				fix_conic_conns_from_node(dragging_node);
			//	make_node_effective(dragging_node);
			}
	
			break;

			
		case SATS_WIDTH:
		
			set_width_sat(dragging_node, dragging_sat, 
				worldx - dragging_node->x, worldy - dragging_node->y);

			break;
		}

		generate_t_values_for_conns_with_node(dragging_node);
		update_point_positions();
		
		switch(job_type)
		{
		case JOB_TYPE_BSP:
		case JOB_TYPE_FILL_VERTICIES:
		case JOB_TYPE_CONN_VERTICIES:
		case JOB_TYPE_CONN_SQUISH:
			start_working();
		}
		
	//	start_working();

		map_saved = 0;
		update = 1;
		break;
	
	case STATE_DRAGGING_POINT:
		
		// carbohydrate is important pasta is good breathing is important
	
		// get world coords from screen coords
		
		screen_to_world(x - dragging_xoffset, y - dragging_yoffset, &worldx, &worldy);
		
		move_point(dragging_point, worldx, worldy);
				
		map_saved = 0;
		update = 1;
		break;
	
	case STATE_DRAGGING_OBJECT:
		
		screen_to_world(x - dragging_xoffset, y - dragging_yoffset, &worldx, &worldy);
		
		if(view_state & VIEW_GRID)
			snap_to_grid(worldx, worldy, &worldx, &worldy);
		
		if(selected_object0)
		{
			move_selected(worldx - dragging_object->x, worldy - dragging_object->y);
		}
		else
		{
			dragging_object->x = worldx;
			dragging_object->y = worldy;
		}
		
		map_saved = 0;
		update = 1;
		break;
	
	case STATE_SELECTING:
		LL_REMOVE_ALL(struct node_pointer_t, &selected_node0);
		struct node_t *node = node0;
			
		while(node)
		{
			if(((node->x > left_button_down_worldx && node->x < mouse_worldx) || 
				(node->x > mouse_worldx && node->x < left_button_down_worldx)) &&
				((node->y > left_button_down_worldy && node->y < mouse_worldy) || 
				(node->y > mouse_worldy && node->y < left_button_down_worldy)))
			{
				add_node_pointer(&selected_node0, node);
			}
			
			node = node->next;
		}
		
		LL_REMOVE_ALL(struct object_pointer_t, &selected_object0);
		struct object_t *object = object0;
			
		while(object)
		{
			if(((object->x > left_button_down_worldx && object->x < mouse_worldx) || 
				(object->x > mouse_worldx && object->x < left_button_down_worldx)) &&
				((object->y > left_button_down_worldy && object->y < mouse_worldy) || 
				(object->y > mouse_worldy && object->y < left_button_down_worldy)))
			{
				add_object_pointer(&selected_object0, object);
			}
			
			object = object->next;
		}
		
		update = 1;
		break;
	}	
	

	if(generate_hover_nodes())
		update = 1;
	

	if(update)
	{
		update_client_area();
		return 1;
	}
	
	return 0;
}


void window_size_changed()
{
	if(!map_active)
		process_cameron();
}


void scale_map_to_window()	// called while not working
{
	if(!conn0)
		return;

	struct conn_t *cconn = conn0;

	float minx, miny, maxx, maxy;

	get_conn_limits(cconn, &minx, &maxx, &miny, &maxy);

	cconn = cconn->next;

	while(cconn)
	{
		float cminx, cminy, cmaxx, cmaxy;
	
		get_conn_limits(cconn, &cminx, &cmaxx, &cminy, &cmaxy);
		
		if(cminx < minx)
			minx = cminx;

		if(cmaxx > maxx)
			maxx = cmaxx;

		if(cminy < miny)
			miny = cminy;

		if(cmaxy > maxy)
			maxy = cmaxy;

		cconn = cconn->next;
	}

	viewx = (minx + maxx) / 2.0;
	viewy = (miny + maxy) / 2.0;

	double zoomx = vid_width / (maxx - minx);
	double zoomy = vid_height / (maxy - miny);

	zoom = min(zoomx, zoomy) * 0.9;

	invalidate_all_scaled_objects();
	update_fully_rendered_conns();
}


void node_deleted(struct node_t *node)
{
	if(left_state & (STATE_DRAGGING_NODE | STATE_DRAGGING_SAT))
	{
		if(dragging_node == node)
		{
			left_state = STATE_DEAD;
		}
	}
	else
	{
		if(left_state & (STATE_DEFINING_STRAIGHT_CONN | STATE_DEFINING_CONIC_CONN | 
			STATE_DEFINING_BEZIER_CONN))
		{
			if(joining_node == node)
			{
				left_state = STATE_DEAD;
			}
		}
	}
	
	remove_node_pointer(&selected_node0, node);
	generate_hover_nodes();
}


void object_deleted(struct object_t *object)
{
	if(left_state & STATE_DRAGGING_OBJECT)
	{
		if(dragging_object == object)
		{
			left_state = STATE_DEAD;
		}
	}
	
	remove_object_pointer(&selected_object0, object);
}


void connect_straight(struct node_t *node)
{
	joining_node = node;
	left_state = STATE_DEFINING_STRAIGHT_CONN;
}


void connect_conic(struct node_t *node)
{
	joining_node = node;
	left_state = STATE_DEFINING_CONIC_CONN;
}


void connect_bezier(struct node_t *node)
{
	joining_node = node;
	left_state = STATE_DEFINING_BEZIER_CONN;
}


void join_point(struct point_t *point)
{
	joining_point = point;
	left_state = STATE_DEFINING_LINE;
}	


void define_fill(struct point_t *point)
{
	left_state = STATE_DEFINING_FILL;
	start_defining_fill(point);
}


void finished_defining_fill()
{
	left_state = STATE_DEAD;
}


void init_interface()
{
	s_select = read_png_surface(find_resource("select.png"));
	init_nodes();
	init_points();
	init_objects();
}



void kill_interface()
{
	free_surface(s_select);	s_select = NULL;
	kill_nodes();
	kill_objects();
}
