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
#include <memory.h>
#include <math.h>
#include <assert.h>

#include <zlib.h>
#include <gtk/gtk.h>

#define USE_GDK_PIXBUF

#include "prefix.h"

#include "llist.h"
#include "stringbuf.h"
#include "resource.h"
#include "gsub.h"
#include "worker.h"
#include "main_lock.h"
#include "objects.h"
#include "interface.h"
#include "main.h"
#include "glade.h"
#include "map.h"

struct dynamic_object_texture_t
{
	int non_default;
	struct string_t *filename;
	struct surface_t *texture;		// always loaded if filename is set
	
} dynamic_objects[5];


struct surface_t *s_unowned_plasma_cannon, *s_unowned_minigun, *s_unowned_rocket_launcher;
struct surface_t *s_rails;
struct surface_t *s_spawn_point_placeholder, *s_teleporter_placeholder, 
	*s_gravity_well_placeholder, *s_shield_energy;
struct surface_t *s_scaled_spawn_point_placeholder, *s_scaled_teleporter_placeholder, 
	*s_scaled_gravity_well_placeholder, *s_scaled_shield_energy;
struct surface_t *s_stock_speedup_ramp;


struct object_t *object0 = NULL;
uint32_t next_spawn_index;

struct object_t *working_object;
struct surface_t *working_object_surface;


int object_scale_job;

static gchar *get_texture_filename(GtkWidget *chooser)
{
	return gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(chooser));
}

static void set_texture_filename(GtkWidget *chooser, struct string_t *filename)
{
	if(!filename)
	{
		gtk_file_chooser_unselect_all(GTK_FILE_CHOOSER(chooser));
		return;
	}

	struct string_t *string = arb_rel2abs(filename->text, map_path->text);
	gtk_file_chooser_set_filename(GTK_FILE_CHOOSER(chooser), string->text);
	free_string(string);
}

#define OBJECT_SCALE_JOB_SPAWN_POINT	0
#define OBJECT_SCALE_JOB_TELEPORTER		1
#define OBJECT_SCALE_JOB_GRAVITY_WELL	2
#define OBJECT_SCALE_JOB_SHIELD_ENERGY	3
#define OBJECT_SCALE_JOB_SPECIFIC		4


#define OBJECT_THRESHOLD 15
#define OBJECT_THRESHOLD_SQUARED (OBJECT_THRESHOLD * OBJECT_THRESHOLD)


double degrees_to_radians(double degrees)
{
	return 2 * M_PI - (degrees * M_PI) / 180.0;
}


double radians_to_degrees(double radians)
{
	double degrees = ((2 * M_PI - radians) * 180.0) / M_PI;
	
	while(degrees > 180.0)
		degrees -= 360.0;
	
	while(degrees <= -180.0)
		degrees += 360.0;
	
	if(degrees == -0.0)
		degrees = 0.0;
	
	return degrees;
}


int add_object_pointer(struct object_pointer_t **objectp0, struct object_t *object)
{
	if(!objectp0)
		return 0;
	
	if(!*objectp0)
	{
		*objectp0 = malloc(sizeof(struct object_pointer_t));
		(*objectp0)->object = object;
		(*objectp0)->next = NULL;
	}
	else
	{
		struct object_pointer_t *cobjectp = *objectp0;

		while(cobjectp->next)
		{
			if(cobjectp->object == object)
				return 0;

			cobjectp = cobjectp->next;
		}

		if(cobjectp->object != object)
		{
			cobjectp->next = malloc(sizeof(struct object_pointer_t));
			cobjectp = cobjectp->next;
			cobjectp->object = object;
			cobjectp->next = NULL;
		}
		else
		{
			return 0;
		}
	}

	return 1;
}


void remove_object_pointer(struct object_pointer_t **objectp0, struct object_t *object)
{
	if(!objectp0)
		return;

	struct object_pointer_t *cobjectp = *objectp0;

	while(cobjectp)
	{
		if(cobjectp->object == object)
		{
			LL_REMOVE(struct object_pointer_t, objectp0, cobjectp);
			break;
		}

		cobjectp = cobjectp->next;
	}
}


int object_in_object_pointer_list(struct object_pointer_t *objectp0, struct object_t *object)
{
	struct object_pointer_t *cobjectp = objectp0;
		
	while(cobjectp)
	{
		if(cobjectp->object == object)
			return 1;
		
		cobjectp = cobjectp->next;
	}
	
	return 0;
}


void insert_object(int type, float x, float y)
{
	struct object_t object;
	
	object.type = type;
	object.texture_surface = NULL;
	object.scaled_texture_surface = NULL;
	
	object.x = x;
	object.y = y;
	
	switch(type)
	{
	case OBJECTTYPE_PLASMACANNON:
		object.plasma_cannon_data.plasmas = 40;
		object.plasma_cannon_data.angle = 0.0;
		object.plasma_cannon_data.respawn_delay = 10000;
		break;
	
	case OBJECTTYPE_MINIGUN:
		object.minigun_data.bullets = 80;
		object.minigun_data.angle = 0.0;
		object.minigun_data.respawn_delay = 10000.0;
		break;
	
	case OBJECTTYPE_ROCKETLAUNCHER:
		object.rocket_launcher_data.rockets = 10;
		object.rocket_launcher_data.angle = 0.0;
		object.rocket_launcher_data.respawn_delay = 10000;
		break;
	
	case OBJECTTYPE_RAILS:
		object.rails_data.quantity = 3;
		object.rails_data.angle = 0.0;
		object.rails_data.respawn_delay = 10000;
		break;
	
	case OBJECTTYPE_SHIELDENERGY:
		object.shield_energy_data.energy = 1.0;
		object.shield_energy_data.angle = 0.0;
		object.shield_energy_data.respawn_delay = 10000;
		break;
	
	case OBJECTTYPE_SPAWNPOINT:
		object.spawn_point_data.non_default_texture = 0;
		object.spawn_point_data.texture_filename = NULL;
		object.spawn_point_data.texture_pre_surface = NULL;
		object.spawn_point_data.width = 64.0;
		object.spawn_point_data.height = 64.0;
		object.spawn_point_data.angle = 0.0;
		object.spawn_point_data.teleport_only = 0;
		object.spawn_point_data.index = next_spawn_index++;
		break;
	
	case OBJECTTYPE_SPEEDUPRAMP:
		object.speedup_ramp_data.non_default_texture = 0;
		object.speedup_ramp_data.texture_filename = NULL;
		object.speedup_ramp_data.texture_pre_surface = NULL;
		object.speedup_ramp_data.width = 128.0;
		object.speedup_ramp_data.height = 64.0;
		object.speedup_ramp_data.angle = 0.0;
		object.speedup_ramp_data.activation_width = 128.0;
		object.speedup_ramp_data.boost = 10.0;
		break;
	
	case OBJECTTYPE_TELEPORTER:
		object.teleporter_data.non_default_texture = 0;
		object.teleporter_data.texture_filename = NULL;
		object.teleporter_data.texture_pre_surface = NULL;
		object.teleporter_data.width = 64.0;
		object.teleporter_data.height = 64.0;
		object.teleporter_data.angle = 0.0;
		object.teleporter_data.radius = 100.0;
		object.teleporter_data.sparkles = 400;
		object.teleporter_data.spawn_point_index = 0;
		object.teleporter_data.rotation_type = TELEPORTER_ROTATION_TYPE_ABS;
		object.teleporter_data.rotation_angle = 0.0;
		object.teleporter_data.divider = 0;
		object.teleporter_data.divider_angle = 0.0;
		break;
	
	case OBJECTTYPE_GRAVITYWELL:
		object.gravity_well_data.non_default_texture = 0;
		object.gravity_well_data.texture_filename = NULL;
		object.gravity_well_data.texture_pre_surface = NULL;
		object.gravity_well_data.width = 64.0;
		object.gravity_well_data.height = 64.0;
		object.gravity_well_data.angle = 0.0;
		object.gravity_well_data.strength = 200.0;
		object.gravity_well_data.enclosed = 1;
		break;
	}
	
	LL_ADD(struct object_t, &object0, &object);
		
	start_working();
}


struct object_t *get_object(int x, int y, int *xoffset, int *yoffset)
{
	struct object_t *object = object0;
	
	while(object)
	{
		int object_screenx, object_screeny;
		world_to_screen(object->x, object->y, &object_screenx, &object_screeny);
		
		int deltax = x - object_screenx;
		int deltay = y - object_screeny;
		
		double dist = (double)deltax * (double)deltax + (double)deltay * (double)deltay;

		if(dist < OBJECT_THRESHOLD_SQUARED)
		{
			if(xoffset)
				*xoffset = deltax;

			if(yoffset)
				*yoffset = deltay;

			return object;
		}
		
		object = object->next;
	}
	
	return NULL;
}


void delete_object(struct object_t *object)
{
	free_surface(object->texture_surface);
	free_surface(object->scaled_texture_surface);
	
	switch(object->type)
	{
	case OBJECTTYPE_SPAWNPOINT:
		free_string(object->spawn_point_data.texture_filename);
		free_surface(object->spawn_point_data.texture_pre_surface);
		break;
	
	case OBJECTTYPE_SPEEDUPRAMP:
		free_string(object->speedup_ramp_data.texture_filename);
		free_surface(object->speedup_ramp_data.texture_pre_surface);
		break;
	
	case OBJECTTYPE_TELEPORTER:
		free_string(object->teleporter_data.texture_filename);
		free_surface(object->teleporter_data.texture_pre_surface);
		break;
	
	case OBJECTTYPE_GRAVITYWELL:
		free_string(object->gravity_well_data.texture_filename);
		free_surface(object->gravity_well_data.texture_pre_surface);
		break;
	}
	
	LL_REMOVE(struct object_t, &object0, object);
		
	object_deleted(object);
}


void invalidate_object_type(int type)
{
	struct object_t *object = object0;
		
	while(object)
	{
		if(object->type == type)
		{
			free_surface(object->texture_surface);
			object->texture_surface = NULL;
			
			free_surface(object->scaled_texture_surface);
			object->scaled_texture_surface = NULL;
		}
		
		object = object->next;
	}
}


void invalidate_object(struct object_t *object)
{
	free_surface(object->texture_surface);
	object->texture_surface = NULL;
	
	free_surface(object->scaled_texture_surface);
	object->scaled_texture_surface = NULL;
}


void on_plasma_cannon_texture_checkbutton_toggled(GtkToggleButton *togglebutton, gpointer user_data)
{
	gboolean sensitive = gtk_toggle_button_get_active(togglebutton);
	
	GtkWidget *dialog = gtk_widget_get_toplevel(GTK_WIDGET(togglebutton));
		
	gtk_widget_set_sensitive(GTK_WIDGET(g_object_get_data(G_OBJECT(dialog), 
		"texture_pixmapentry")), sensitive);

	stop_working();
	
	dynamic_objects[OBJECTTYPE_PLASMACANNON].non_default = sensitive;
	
	if(dynamic_objects[OBJECTTYPE_PLASMACANNON].texture)
		invalidate_object_type(OBJECTTYPE_PLASMACANNON);
	
	update_client_area();
	start_working();
}


void on_plasma_cannon_texture_entry_changed(GtkFileChooserButton *chooser, gpointer user_data)
{
	stop_working();

	free_string(dynamic_objects[OBJECTTYPE_PLASMACANNON].filename);
	dynamic_objects[OBJECTTYPE_PLASMACANNON].filename = NULL;
	
	free_surface(dynamic_objects[OBJECTTYPE_PLASMACANNON].texture);
	dynamic_objects[OBJECTTYPE_PLASMACANNON].texture = NULL;
	
	gchar *strval = get_texture_filename(GTK_WIDGET(chooser));
	if(strval)
	{
		dynamic_objects[OBJECTTYPE_PLASMACANNON].filename = arb_abs2rel(strval, map_path->text);
		
		struct string_t *string = arb_rel2abs(dynamic_objects[OBJECTTYPE_PLASMACANNON].filename->text, 
			map_path->text);
		dynamic_objects[OBJECTTYPE_PLASMACANNON].texture = 
			read_gdk_pixbuf_surface_as_24bitalpha8bit(string->text);
		free_string(string);
	}
	
	g_free(strval);
	
	invalidate_object_type(OBJECTTYPE_PLASMACANNON);
	
	update_client_area();
	start_working();
}


void on_plasma_cannon_plasmas_spinbutton_value_changed(GtkSpinButton *spinbutton, 
	gpointer user_data)
{
	GtkWidget *dialog = gtk_widget_get_toplevel(GTK_WIDGET(spinbutton));
	struct object_t *object = g_object_get_data(G_OBJECT(dialog), "object");
		
	object->plasma_cannon_data.plasmas = gtk_spin_button_get_value_as_int(spinbutton);
}





gboolean
on_plasma_cannon_angle_spinbutton_button_press_event
                                        (GtkWidget       *widget,
                                        GdkEventButton  *event,
                                        gpointer         user_data)
{
	return 0;
}

gboolean
on_plasma_cannon_angle_spinbutton_button_release_event
                                        (GtkWidget       *widget,
                                        GdkEventButton  *event,
                                        gpointer         user_data)
{
	return 0;
}


void on_plasma_cannon_angle_spinbutton_value_changed(GtkSpinButton *spinbutton, gpointer user_data)
{
	GtkWidget *dialog = gtk_widget_get_toplevel(GTK_WIDGET(spinbutton));
	struct object_t *object = g_object_get_data(G_OBJECT(dialog), "object");
	
	stop_working();
	
	object->plasma_cannon_data.angle = degrees_to_radians(gtk_spin_button_get_value(spinbutton));

	invalidate_object(object);
	
	update_client_area();
	start_working();
}


void on_plasma_cannon_respawn_delay_spinbutton_value_changed(GtkSpinButton *spinbutton, 
	gpointer user_data)
{
	GtkWidget *dialog = gtk_widget_get_toplevel(GTK_WIDGET(spinbutton));
	struct object_t *object = g_object_get_data(G_OBJECT(dialog), "object");
		
	object->plasma_cannon_data.respawn_delay = gtk_spin_button_get_value_as_int(spinbutton);
}


void on_plasma_cannon_properties_dialog_destroy(GtkObject *dialog, gpointer user_data)
{
	gtk_main_quit();
}


void run_plasma_cannon_properties_dialog(void *menu, struct object_t *object)
{
	GtkWidget *dialog = create_plasma_cannon_properties_dialog();
	
	g_object_set_data(G_OBJECT(dialog), "object", object);
	
	gtk_window_set_transient_for(GTK_WINDOW(dialog), GTK_WINDOW(window));
	gtk_window_set_position(GTK_WINDOW(dialog), GTK_WIN_POS_CENTER_ON_PARENT);
	
	GtkWidget *texture_checkbutton = g_object_get_data(G_OBJECT(dialog), "texture_checkbutton");
	
	if(dynamic_objects[OBJECTTYPE_PLASMACANNON].non_default)
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(texture_checkbutton), TRUE);
		
	if(dynamic_objects[OBJECTTYPE_PLASMACANNON].filename)
		set_texture_filename(GTK_WIDGET(g_object_get_data(G_OBJECT(dialog), "texture_entry")), 
			dynamic_objects[OBJECTTYPE_PLASMACANNON].filename);
	
	gtk_spin_button_set_value(GTK_SPIN_BUTTON(g_object_get_data(G_OBJECT(dialog), 
		"plasmas_spinbutton")), (gfloat)object->plasma_cannon_data.plasmas);
	
	gtk_spin_button_set_value(GTK_SPIN_BUTTON(g_object_get_data(G_OBJECT(dialog), 
		"angle_spinbutton")), radians_to_degrees(object->plasma_cannon_data.angle));
	
	gtk_spin_button_set_value(GTK_SPIN_BUTTON(g_object_get_data(G_OBJECT(dialog), 
		"respawn_delay_spinbutton")), (gfloat)object->plasma_cannon_data.respawn_delay);
	
	gtk_widget_show(dialog);
	gtk_main();
}



void on_minigun_texture_checkbutton_toggled(GtkToggleButton *togglebutton, gpointer user_data)
{
	gboolean sensitive = gtk_toggle_button_get_active(togglebutton);
	
	GtkWidget *dialog = gtk_widget_get_toplevel(GTK_WIDGET(togglebutton));
		
	gtk_widget_set_sensitive(GTK_WIDGET(g_object_get_data(G_OBJECT(dialog), 
		"texture_pixmapentry")), sensitive);

	stop_working();
	
	dynamic_objects[OBJECTTYPE_MINIGUN].non_default = sensitive;
	
	if(dynamic_objects[OBJECTTYPE_MINIGUN].texture)
		invalidate_object_type(OBJECTTYPE_MINIGUN);
	
	update_client_area();
	start_working();
}


void on_minigun_texture_entry_changed(GtkFileChooserButton *chooser, gpointer user_data)
{
	stop_working();

	free_string(dynamic_objects[OBJECTTYPE_MINIGUN].filename);
	dynamic_objects[OBJECTTYPE_MINIGUN].filename = NULL;
	
	free_surface(dynamic_objects[OBJECTTYPE_MINIGUN].texture);
	dynamic_objects[OBJECTTYPE_MINIGUN].texture = NULL;
	
	gchar *strval = get_texture_filename(GTK_WIDGET(chooser));
	if(strval)
	{
		dynamic_objects[OBJECTTYPE_MINIGUN].filename = arb_abs2rel(strval, map_path->text);
		
		struct string_t *string = 
			arb_rel2abs(dynamic_objects[OBJECTTYPE_MINIGUN].filename->text, map_path->text);
		dynamic_objects[OBJECTTYPE_MINIGUN].texture = 
			read_gdk_pixbuf_surface_as_24bitalpha8bit(string->text);
		free_string(string);
	}
	
	g_free(strval);
	
	invalidate_object_type(OBJECTTYPE_MINIGUN);
	
	update_client_area();
	start_working();
}


void on_minigun_bullets_spinbutton_value_changed(GtkSpinButton *spinbutton, gpointer user_data)
{
	GtkWidget *dialog = gtk_widget_get_toplevel(GTK_WIDGET(spinbutton));
	struct object_t *object = g_object_get_data(G_OBJECT(dialog), "object");
		
	object->minigun_data.bullets = gtk_spin_button_get_value_as_int(spinbutton);
}


void on_minigun_angle_spinbutton_value_changed(GtkSpinButton *spinbutton, gpointer user_data)
{
	GtkWidget *dialog = gtk_widget_get_toplevel(GTK_WIDGET(spinbutton));
	struct object_t *object = g_object_get_data(G_OBJECT(dialog), "object");
	
	stop_working();
	
	object->minigun_data.angle = degrees_to_radians(gtk_spin_button_get_value(spinbutton));

	invalidate_object(object);
	
	update_client_area();
	start_working();
}


void on_minigun_respawn_delay_spinbutton_value_changed(GtkSpinButton *spinbutton, 
	gpointer user_data)
{
	GtkWidget *dialog = gtk_widget_get_toplevel(GTK_WIDGET(spinbutton));
	struct object_t *object = g_object_get_data(G_OBJECT(dialog), "object");
		
	object->minigun_data.respawn_delay = gtk_spin_button_get_value_as_int(spinbutton);
}


void on_minigun_properties_dialog_destroy(GtkObject *dialog, gpointer user_data)
{
	gtk_main_quit();
}


void run_minigun_properties_dialog(void *menu, struct object_t *object)
{
	GtkWidget *dialog = create_minigun_properties_dialog();
	g_object_set_data(G_OBJECT(dialog), "object", object);
	
	gtk_window_set_transient_for(GTK_WINDOW(dialog), GTK_WINDOW(window));
	gtk_window_set_position(GTK_WINDOW(dialog), GTK_WIN_POS_CENTER_ON_PARENT);
	
	GtkWidget *texture_checkbutton = g_object_get_data(G_OBJECT(dialog), "texture_checkbutton");
	
	if(dynamic_objects[OBJECTTYPE_MINIGUN].non_default)
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(texture_checkbutton), TRUE);
		
	if(dynamic_objects[OBJECTTYPE_MINIGUN].filename)
		set_texture_filename(GTK_WIDGET(g_object_get_data(G_OBJECT(dialog), "texture_entry")), 
			dynamic_objects[OBJECTTYPE_MINIGUN].filename);

	gtk_spin_button_set_value(GTK_SPIN_BUTTON(g_object_get_data(G_OBJECT(dialog), 
		"bullets_spinbutton")), (gfloat)object->minigun_data.bullets);
	
	gtk_spin_button_set_value(GTK_SPIN_BUTTON(g_object_get_data(G_OBJECT(dialog), 
		"angle_spinbutton")), radians_to_degrees(object->minigun_data.angle));
	
	gtk_spin_button_set_value(GTK_SPIN_BUTTON(g_object_get_data(G_OBJECT(dialog), 
		"respawn_delay_spinbutton")), (gfloat)object->minigun_data.respawn_delay);
	
	gtk_widget_show(dialog);
	gtk_main();
}



void on_rocket_launcher_texture_checkbutton_toggled(GtkToggleButton *togglebutton, 
	gpointer user_data)
{
	gboolean sensitive = gtk_toggle_button_get_active(togglebutton);
	
	GtkWidget *dialog = gtk_widget_get_toplevel(GTK_WIDGET(togglebutton));
		
	gtk_widget_set_sensitive(GTK_WIDGET(g_object_get_data(G_OBJECT(dialog), 
		"texture_pixmapentry")), sensitive);

	stop_working();
	
	dynamic_objects[OBJECTTYPE_ROCKETLAUNCHER].non_default = sensitive;
	
	if(dynamic_objects[OBJECTTYPE_ROCKETLAUNCHER].texture)
		invalidate_object_type(OBJECTTYPE_ROCKETLAUNCHER);
	
	update_client_area();
	start_working();
}


void on_rocket_launcher_texture_entry_changed(GtkFileChooserButton *chooser, gpointer user_data)
{
	stop_working();

	free_string(dynamic_objects[OBJECTTYPE_ROCKETLAUNCHER].filename);
	dynamic_objects[OBJECTTYPE_ROCKETLAUNCHER].filename = NULL;
	
	free_surface(dynamic_objects[OBJECTTYPE_ROCKETLAUNCHER].texture);
	dynamic_objects[OBJECTTYPE_ROCKETLAUNCHER].texture = NULL;
	
	gchar *strval = get_texture_filename(GTK_WIDGET(chooser));
	if(strval)
	{
		dynamic_objects[OBJECTTYPE_ROCKETLAUNCHER].filename = 
			arb_abs2rel(strval, map_path->text);
		
		struct string_t *string = arb_rel2abs(
			dynamic_objects[OBJECTTYPE_ROCKETLAUNCHER].filename->text, map_path->text);
		dynamic_objects[OBJECTTYPE_ROCKETLAUNCHER].texture = 
			read_gdk_pixbuf_surface_as_24bitalpha8bit(string->text);
		free_string(string);
	}
	
	g_free(strval);
	
	invalidate_object_type(OBJECTTYPE_ROCKETLAUNCHER);
	
	update_client_area();
	start_working();
}


void on_rocket_launcher_rockets_spinbutton_value_changed(GtkSpinButton *spinbutton, 
	gpointer user_data)
{
	GtkWidget *dialog = gtk_widget_get_toplevel(GTK_WIDGET(spinbutton));
	struct object_t *object = g_object_get_data(G_OBJECT(dialog), "object");
		
	object->rocket_launcher_data.rockets = gtk_spin_button_get_value_as_int(spinbutton);
}


void on_rocket_launcher_angle_spinbutton_value_changed(GtkSpinButton *spinbutton, 
	gpointer user_data)
{
	GtkWidget *dialog = gtk_widget_get_toplevel(GTK_WIDGET(spinbutton));
	struct object_t *object = g_object_get_data(G_OBJECT(dialog), "object");
	
	stop_working();
	
	int value = gtk_spin_button_get_value(spinbutton);
	
/*	if(value == 180)
	{
		value = -180;
		gtk_spin_button_set_value(spinbutton, value);
	}
*/	
	object->rocket_launcher_data.angle = degrees_to_radians(value);
	
	invalidate_object(object);
	
	update_client_area();
	start_working();
}


void on_rocket_launcher_respawn_delay_spinbutton_value_changed(GtkSpinButton *spinbutton, 
	gpointer user_data)
{
	GtkWidget *dialog = gtk_widget_get_toplevel(GTK_WIDGET(spinbutton));
	struct object_t *object = g_object_get_data(G_OBJECT(dialog), "object");
		
	object->rocket_launcher_data.respawn_delay = gtk_spin_button_get_value_as_int(spinbutton);
}


void on_rocket_launcher_properties_dialog_destroy(GtkObject *dialog, gpointer user_data)
{
	gtk_main_quit();
}


void run_rocket_launcher_properties_dialog(void *menu, struct object_t *object)
{
	GtkWidget *dialog = create_rocket_launcher_properties_dialog();
	g_object_set_data(G_OBJECT(dialog), "object", object);
	
	gtk_window_set_transient_for(GTK_WINDOW(dialog), GTK_WINDOW(window));
	gtk_window_set_position(GTK_WINDOW(dialog), GTK_WIN_POS_CENTER_ON_PARENT);

	GtkWidget *texture_checkbutton = g_object_get_data(G_OBJECT(dialog), "texture_checkbutton");
	
	if(dynamic_objects[OBJECTTYPE_ROCKETLAUNCHER].non_default)
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(texture_checkbutton), TRUE);
		
	if(dynamic_objects[OBJECTTYPE_ROCKETLAUNCHER].filename)
		set_texture_filename(GTK_WIDGET(g_object_get_data(G_OBJECT(dialog), "texture_entry")), 
			dynamic_objects[OBJECTTYPE_ROCKETLAUNCHER].filename);

	gtk_spin_button_set_value(GTK_SPIN_BUTTON(g_object_get_data(G_OBJECT(dialog), 
		"rockets_spinbutton")), (gfloat)object->rocket_launcher_data.rockets);
	
	gtk_spin_button_set_value(GTK_SPIN_BUTTON(g_object_get_data(G_OBJECT(dialog), 
		"angle_spinbutton")), radians_to_degrees(object->rocket_launcher_data.angle));
	
	gtk_spin_button_set_value(GTK_SPIN_BUTTON(g_object_get_data(G_OBJECT(dialog), 
		"respawn_delay_spinbutton")), (gfloat)object->rocket_launcher_data.respawn_delay);
	
	gtk_widget_show(dialog);
	gtk_main();
}



void on_rails_texture_checkbutton_toggled(GtkToggleButton *togglebutton, gpointer user_data)
{
	gboolean sensitive = gtk_toggle_button_get_active(togglebutton);
	
	GtkWidget *dialog = gtk_widget_get_toplevel(GTK_WIDGET(togglebutton));
		
	gtk_widget_set_sensitive(GTK_WIDGET(g_object_get_data(G_OBJECT(dialog), 
		"texture_pixmapentry")), sensitive);

	stop_working();
	
	dynamic_objects[OBJECTTYPE_RAILS].non_default = sensitive;
	
	if(dynamic_objects[OBJECTTYPE_RAILS].texture)
		invalidate_object_type(OBJECTTYPE_RAILS);
	
	update_client_area();
	start_working();
}


void on_rails_texture_pixmapentry_changed(GtkFileChooserButton *chooser, gpointer user_data)
{
	stop_working();

	free_string(dynamic_objects[OBJECTTYPE_RAILS].filename);
	dynamic_objects[OBJECTTYPE_RAILS].filename = NULL;
	
	free_surface(dynamic_objects[OBJECTTYPE_RAILS].texture);
	dynamic_objects[OBJECTTYPE_RAILS].texture = NULL;
	
	gchar *strval = get_texture_filename(GTK_WIDGET(chooser));
	if(strval)
	{
		dynamic_objects[OBJECTTYPE_RAILS].filename = arb_abs2rel(strval, map_path->text);
		
		struct string_t *string = arb_rel2abs(dynamic_objects[OBJECTTYPE_RAILS].filename->text, 
			map_path->text);
		dynamic_objects[OBJECTTYPE_RAILS].texture = 
			read_gdk_pixbuf_surface_as_24bitalpha8bit(string->text);
		free_string(string);
	}
	
	g_free(strval);
	
	invalidate_object_type(OBJECTTYPE_RAILS);
	
	update_client_area();
	start_working();
}


void on_rails_quantity_spinbutton_value_changed(GtkSpinButton *spinbutton, gpointer user_data)
{
	GtkWidget *dialog = gtk_widget_get_toplevel(GTK_WIDGET(spinbutton));
	struct object_t *object = g_object_get_data(G_OBJECT(dialog), "object");
		
	object->rails_data.quantity = gtk_spin_button_get_value_as_int(spinbutton);
}


void on_rails_angle_spinbutton_value_changed(GtkSpinButton *spinbutton, gpointer user_data)
{
	GtkWidget *dialog = gtk_widget_get_toplevel(GTK_WIDGET(spinbutton));
	struct object_t *object = g_object_get_data(G_OBJECT(dialog), "object");
	
	stop_working();
	
	object->rails_data.angle = degrees_to_radians(gtk_spin_button_get_value(spinbutton));

	invalidate_object_type(OBJECTTYPE_RAILS);
	
	update_client_area();
	start_working();
}


void on_rails_respawn_delay_spinbutton_value_changed(GtkSpinButton *spinbutton, gpointer user_data)
{
	GtkWidget *dialog = gtk_widget_get_toplevel(GTK_WIDGET(spinbutton));
	struct object_t *object = g_object_get_data(G_OBJECT(dialog), "object");
		
	object->rails_data.respawn_delay = gtk_spin_button_get_value_as_int(spinbutton);
}


void on_rails_properties_dialog_destroy(GtkObject *dialog, gpointer user_data)
{
	gtk_main_quit();
}


void run_rails_properties_dialog(void *menu, struct object_t *object)
{
	GtkWidget *dialog = create_rails_properties_dialog();
	g_object_set_data(G_OBJECT(dialog), "object", object);
	
	gtk_window_set_transient_for(GTK_WINDOW(dialog), GTK_WINDOW(window));
	gtk_window_set_position(GTK_WINDOW(dialog), GTK_WIN_POS_CENTER_ON_PARENT);

	GtkWidget *texture_checkbutton = g_object_get_data(G_OBJECT(dialog), "texture_checkbutton");
	
	if(dynamic_objects[OBJECTTYPE_RAILS].non_default)
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(texture_checkbutton), TRUE);
	
	if(dynamic_objects[OBJECTTYPE_RAILS].filename)
		set_texture_filename(GTK_WIDGET(g_object_get_data(G_OBJECT(dialog), "texture_entry")), 
			dynamic_objects[OBJECTTYPE_RAILS].filename);

	gtk_spin_button_set_value(GTK_SPIN_BUTTON(g_object_get_data(G_OBJECT(dialog), 
		"quantity_spinbutton")), (gfloat)object->rails_data.quantity);
	
	gtk_spin_button_set_value(GTK_SPIN_BUTTON(g_object_get_data(G_OBJECT(dialog), 
		"angle_spinbutton")), radians_to_degrees(object->rails_data.angle));
	
	gtk_spin_button_set_value(GTK_SPIN_BUTTON(g_object_get_data(G_OBJECT(dialog), 
		"respawn_delay_spinbutton")), (gfloat)object->rails_data.respawn_delay);
	
	gtk_widget_show(dialog);
	gtk_main();
}



void on_shield_energy_texture_checkbutton_toggled(GtkToggleButton *togglebutton, gpointer user_data)
{
	gboolean sensitive = gtk_toggle_button_get_active(togglebutton);
	
	GtkWidget *dialog = gtk_widget_get_toplevel(GTK_WIDGET(togglebutton));
		
	gtk_widget_set_sensitive(GTK_WIDGET(g_object_get_data(G_OBJECT(dialog), 
		"texture_pixmapentry")), sensitive);

	stop_working();
	
	dynamic_objects[OBJECTTYPE_SHIELDENERGY].non_default = sensitive;
	
	if(dynamic_objects[OBJECTTYPE_SHIELDENERGY].texture)
		invalidate_object_type(OBJECTTYPE_SHIELDENERGY);
	
	update_client_area();
	start_working();
}


void on_shield_energy_texture_entry_changed(GtkFileChooserButton *chooser, gpointer user_data)
{
	stop_working();

	free_string(dynamic_objects[OBJECTTYPE_SHIELDENERGY].filename);
	dynamic_objects[OBJECTTYPE_SHIELDENERGY].filename = NULL;
	
	free_surface(dynamic_objects[OBJECTTYPE_SHIELDENERGY].texture);
	dynamic_objects[OBJECTTYPE_SHIELDENERGY].texture = NULL;
	
	gchar *strval = get_texture_filename(GTK_WIDGET(chooser));
	if(strval)
	{
		dynamic_objects[OBJECTTYPE_SHIELDENERGY].filename = arb_abs2rel(strval, map_path->text);
		
		struct string_t *string = arb_rel2abs(dynamic_objects[OBJECTTYPE_SHIELDENERGY].filename->text, 
			map_path->text);
		dynamic_objects[OBJECTTYPE_SHIELDENERGY].texture = 
			read_gdk_pixbuf_surface_as_24bitalpha8bit(string->text);
		free_string(string);
	}
	
	g_free(strval);
	
	invalidate_object_type(OBJECTTYPE_SHIELDENERGY);
	
	update_client_area();
	start_working();
}


void on_shield_energy_energy_spinbutton_value_changed(GtkSpinButton *spinbutton, gpointer user_data)
{
	GtkWidget *dialog = gtk_widget_get_toplevel(GTK_WIDGET(spinbutton));
	struct object_t *object = g_object_get_data(G_OBJECT(dialog), "object");
		
	object->shield_energy_data.energy = gtk_spin_button_get_value(spinbutton) / 100.0;
}


void on_shield_energy_angle_spinbutton_value_changed(GtkSpinButton *spinbutton, gpointer user_data)
{
	GtkWidget *dialog = gtk_widget_get_toplevel(GTK_WIDGET(spinbutton));
	struct object_t *object = g_object_get_data(G_OBJECT(dialog), "object");
	
	stop_working();
	
	object->shield_energy_data.angle = degrees_to_radians(gtk_spin_button_get_value(spinbutton));

	invalidate_object_type(OBJECTTYPE_SHIELDENERGY);
	
	update_client_area();
	start_working();
}


void on_shield_energy_respawn_delay_spinbutton_value_changed(GtkSpinButton *spinbutton, 
	gpointer user_data)
{
	GtkWidget *dialog = gtk_widget_get_toplevel(GTK_WIDGET(spinbutton));
	struct object_t *object = g_object_get_data(G_OBJECT(dialog), "object");
		
	object->shield_energy_data.respawn_delay = gtk_spin_button_get_value_as_int(spinbutton);
}


void on_shield_energy_properties_dialog_destroy(GtkObject *dialog, gpointer user_data)
{
	gtk_main_quit();
}


void run_shield_energy_properties_dialog(void *menu, struct object_t *object)
{
	GtkWidget *dialog = create_shield_energy_properties_dialog();
	g_object_set_data(G_OBJECT(dialog), "object", object);
	
	gtk_window_set_transient_for(GTK_WINDOW(dialog), GTK_WINDOW(window));
	gtk_window_set_position(GTK_WINDOW(dialog), GTK_WIN_POS_CENTER_ON_PARENT);

	GtkWidget *texture_checkbutton = g_object_get_data(G_OBJECT(dialog), "texture_checkbutton");
	
	if(dynamic_objects[OBJECTTYPE_SHIELDENERGY].non_default)
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(texture_checkbutton), TRUE);
		
	if(dynamic_objects[OBJECTTYPE_SHIELDENERGY].filename)
		set_texture_filename(GTK_WIDGET(g_object_get_data(G_OBJECT(dialog), "texture_entry")), 
			dynamic_objects[OBJECTTYPE_SHIELDENERGY].filename);

	gtk_spin_button_set_value(GTK_SPIN_BUTTON(g_object_get_data(G_OBJECT(dialog), 
		"energy_spinbutton")), (gfloat)object->shield_energy_data.energy * 100.0);
	
	gtk_spin_button_set_value(GTK_SPIN_BUTTON(g_object_get_data(G_OBJECT(dialog), 
		"angle_spinbutton")), radians_to_degrees(object->shield_energy_data.angle));
	
	gtk_spin_button_set_value(GTK_SPIN_BUTTON(g_object_get_data(G_OBJECT(dialog), 
		"respawn_delay_spinbutton")), (gfloat)object->shield_energy_data.respawn_delay);
	
	gtk_widget_show(dialog);
	gtk_main();
}



void on_spawn_point_texture_checkbutton_toggled(GtkToggleButton *togglebutton, gpointer user_data)
{
	gboolean sensitive = gtk_toggle_button_get_active(togglebutton);
	
	GtkWidget *dialog = gtk_widget_get_toplevel(GTK_WIDGET(togglebutton));
		
	gtk_widget_set_sensitive(GTK_WIDGET(g_object_get_data(G_OBJECT(dialog), 
		"texture_vbox")), sensitive);

	struct object_t *object = g_object_get_data(G_OBJECT(dialog), "object");
		
	stop_working();
	
	object->spawn_point_data.non_default_texture = sensitive;
	
	if(object->spawn_point_data.texture_pre_surface)
		invalidate_object(object);
	
	update_client_area();
	start_working();
}


void on_spawn_point_texture_entry_changed(GtkFileChooserButton *chooser, gpointer user_data)
{
	GtkWidget *dialog = gtk_widget_get_toplevel(GTK_WIDGET(chooser));
	struct object_t *object = g_object_get_data(G_OBJECT(dialog), "object");

	stop_working();

	free_string(object->spawn_point_data.texture_filename);
	object->spawn_point_data.texture_filename = NULL;
	
	free_surface(object->spawn_point_data.texture_pre_surface);
	object->spawn_point_data.texture_pre_surface = NULL;
	
	gchar *strval = get_texture_filename(GTK_WIDGET(chooser));
	if(strval)
	{
		object->spawn_point_data.texture_filename = arb_abs2rel(strval, map_path->text);
		
		struct string_t *string = arb_rel2abs(object->spawn_point_data.texture_filename->text, 
			map_path->text);
		object->spawn_point_data.texture_pre_surface = 
			read_gdk_pixbuf_surface_as_24bitalpha8bit(string->text);
		free_string(string);
	}
	
	g_free(strval);
	
	invalidate_object(object);
	
	update_client_area();
	start_working();
}


void on_spawn_point_width_spinbutton_value_changed(GtkSpinButton *spinbutton, gpointer user_data)
{
	GtkWidget *dialog = gtk_widget_get_toplevel(GTK_WIDGET(spinbutton));
	struct object_t *object = g_object_get_data(G_OBJECT(dialog), "object");

	stop_working();

	object->spawn_point_data.width = gtk_spin_button_get_value(spinbutton);

	invalidate_object(object);
	
	update_client_area();
	start_working();
}


void on_spawn_point_height_spinbutton_value_changed(GtkSpinButton *spinbutton, gpointer user_data)
{
	GtkWidget *dialog = gtk_widget_get_toplevel(GTK_WIDGET(spinbutton));
	struct object_t *object = g_object_get_data(G_OBJECT(dialog), "object");

	stop_working();

	object->spawn_point_data.height = gtk_spin_button_get_value(spinbutton);

	invalidate_object(object);
	
	update_client_area();
	start_working();
}


void on_spawn_point_angle_spinbutton_value_changed(GtkSpinButton *spinbutton, gpointer user_data)
{
	GtkWidget *dialog = gtk_widget_get_toplevel(GTK_WIDGET(spinbutton));
	struct object_t *object = g_object_get_data(G_OBJECT(dialog), "object");

	stop_working();

	object->spawn_point_data.angle = degrees_to_radians(gtk_spin_button_get_value(spinbutton));

	invalidate_object(object);
	
	update_client_area();
	start_working();
}


void on_spawn_point_teleport_only_checkbutton_toggled(GtkToggleButton *togglebutton, 
	gpointer user_data)
{
	GtkWidget *dialog = gtk_widget_get_toplevel(GTK_WIDGET(togglebutton));
	struct object_t *object = g_object_get_data(G_OBJECT(dialog), "object");
		
	object->spawn_point_data.teleport_only = gtk_toggle_button_get_active(togglebutton);
}


void on_spawn_point_index_spinbutton_value_changed(GtkSpinButton *spinbutton, gpointer user_data)
{
	GtkWidget *dialog = gtk_widget_get_toplevel(GTK_WIDGET(spinbutton));
	struct object_t *object = g_object_get_data(G_OBJECT(dialog), "object");
		
	object->spawn_point_data.index = gtk_spin_button_get_value_as_int(spinbutton);
}


void on_spawn_point_properties_dialog_destroy(GtkObject *dialog, gpointer user_data)
{
	gtk_main_quit();
}


void run_spawn_point_properties_dialog(void *menu, struct object_t *object)
{
	GtkWidget *dialog = create_spawn_point_properties_dialog();
	g_object_set_data(G_OBJECT(dialog), "object", object);
	
	gtk_window_set_transient_for(GTK_WINDOW(dialog), GTK_WINDOW(window));
	gtk_window_set_position(GTK_WINDOW(dialog), GTK_WIN_POS_CENTER_ON_PARENT);

	GtkWidget *texture_checkbutton = g_object_get_data(G_OBJECT(dialog), "texture_checkbutton");
	
	if(object->spawn_point_data.non_default_texture)
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(texture_checkbutton), TRUE);
		
	if(object->spawn_point_data.texture_filename)
		set_texture_filename(GTK_WIDGET(g_object_get_data(G_OBJECT(dialog), "texture_entry")), 
			object->spawn_point_data.texture_filename);

	gtk_spin_button_set_value(GTK_SPIN_BUTTON(g_object_get_data(G_OBJECT(dialog), 
		"width_spinbutton")), (gfloat)object->spawn_point_data.width);
	
	gtk_spin_button_set_value(GTK_SPIN_BUTTON(g_object_get_data(G_OBJECT(dialog), 
		"height_spinbutton")), (gfloat)object->spawn_point_data.height);
	
	gtk_spin_button_set_value(GTK_SPIN_BUTTON(g_object_get_data(G_OBJECT(dialog), 
		"angle_spinbutton")), radians_to_degrees(object->spawn_point_data.angle));
	
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(g_object_get_data(G_OBJECT(dialog), 
		"teleport_only_checkbutton")), (gboolean)object->spawn_point_data.teleport_only);

	gtk_spin_button_set_value(GTK_SPIN_BUTTON(g_object_get_data(G_OBJECT(dialog), 
		"index_spinbutton")), (gfloat)object->spawn_point_data.index);
	
	gtk_widget_show(dialog);
	gtk_main();
}



void on_speedup_ramp_texture_checkbutton_toggled(GtkToggleButton *togglebutton, gpointer user_data)
{
	gboolean sensitive = gtk_toggle_button_get_active(togglebutton);
	
	GtkWidget *dialog = gtk_widget_get_toplevel(GTK_WIDGET(togglebutton));
		
	gtk_widget_set_sensitive(GTK_WIDGET(g_object_get_data(G_OBJECT(dialog), 
		"texture_pixmapentry")), sensitive);

	struct object_t *object = g_object_get_data(G_OBJECT(dialog), "object");
		
	stop_working();
	
	object->speedup_ramp_data.non_default_texture = sensitive;
	
	if(object->speedup_ramp_data.texture_pre_surface)
		invalidate_object(object);
	
	update_client_area();
	start_working();
}


void on_speedup_ramp_texture_entry_changed(GtkFileChooserButton *chooser, gpointer user_data)
{
	GtkWidget *dialog = gtk_widget_get_toplevel(GTK_WIDGET(chooser));
	struct object_t *object = g_object_get_data(G_OBJECT(dialog), "object");

	stop_working();

	free_string(object->speedup_ramp_data.texture_filename);
	object->speedup_ramp_data.texture_filename = NULL;
	
	free_surface(object->speedup_ramp_data.texture_pre_surface);
	object->speedup_ramp_data.texture_pre_surface = NULL;
	
	gchar *strval = get_texture_filename(GTK_WIDGET(chooser));
	if(strval)
	{
		object->speedup_ramp_data.texture_filename = arb_abs2rel(strval, map_path->text);
		
		struct string_t *string = arb_rel2abs(object->speedup_ramp_data.texture_filename->text, 
			map_path->text);
		object->speedup_ramp_data.texture_pre_surface = 
			read_gdk_pixbuf_surface_as_24bitalpha8bit(string->text);
		free_string(string);
	}
	
	g_free(strval);
	
	invalidate_object(object);
	
	update_client_area();
	start_working();
}


void on_speedup_ramp_width_spinbutton_value_changed(GtkSpinButton *spinbutton, gpointer user_data)
{
	GtkWidget *dialog = gtk_widget_get_toplevel(GTK_WIDGET(spinbutton));
	struct object_t *object = g_object_get_data(G_OBJECT(dialog), "object");

	stop_working();

	object->speedup_ramp_data.width = gtk_spin_button_get_value(spinbutton);

	invalidate_object(object);
	
	update_client_area();
	start_working();
}


void on_speedup_ramp_height_spinbutton_value_changed(GtkSpinButton *spinbutton, gpointer user_data)
{
	GtkWidget *dialog = gtk_widget_get_toplevel(GTK_WIDGET(spinbutton));
	struct object_t *object = g_object_get_data(G_OBJECT(dialog), "object");

	stop_working();

	object->speedup_ramp_data.height = gtk_spin_button_get_value(spinbutton);

	invalidate_object(object);
	
	update_client_area();
	start_working();
}


void on_speedup_ramp_angle_spinbutton_value_changed(GtkSpinButton *spinbutton, gpointer user_data)
{
	GtkWidget *dialog = gtk_widget_get_toplevel(GTK_WIDGET(spinbutton));
	struct object_t *object = g_object_get_data(G_OBJECT(dialog), "object");

	stop_working();

	object->speedup_ramp_data.angle = degrees_to_radians(gtk_spin_button_get_value(spinbutton));

	invalidate_object(object);
	
	update_client_area();
	start_working();
}


void on_speedup_ramp_activation_width_spinbutton_value_changed(GtkSpinButton *spinbutton, 
	gpointer user_data)
{
	GtkWidget *dialog = gtk_widget_get_toplevel(GTK_WIDGET(spinbutton));
	struct object_t *object = g_object_get_data(G_OBJECT(dialog), "object");

	object->speedup_ramp_data.activation_width = gtk_spin_button_get_value(spinbutton);
}


void on_speedup_ramp_boost_spinbutton_value_changed(GtkSpinButton *spinbutton, gpointer user_data)
{
	GtkWidget *dialog = gtk_widget_get_toplevel(GTK_WIDGET(spinbutton));
	struct object_t *object = g_object_get_data(G_OBJECT(dialog), "object");

	object->speedup_ramp_data.boost = gtk_spin_button_get_value(spinbutton);
}


void on_speedup_ramp_properties_dialog_destroy(GtkObject *dialog, gpointer user_data)
{
	gtk_main_quit();
}


void run_speedup_ramp_properties_dialog(void *menu, struct object_t *object)
{
	GtkWidget *dialog = create_speedup_ramp_properties_dialog();
	g_object_set_data(G_OBJECT(dialog), "object", object);
	
	gtk_window_set_transient_for(GTK_WINDOW(dialog), GTK_WINDOW(window));
	gtk_window_set_position(GTK_WINDOW(dialog), GTK_WIN_POS_CENTER_ON_PARENT);

	GtkWidget *texture_checkbutton = g_object_get_data(G_OBJECT(dialog), "texture_checkbutton");
	
	if(object->spawn_point_data.non_default_texture)
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(texture_checkbutton), TRUE);
	
	if(object->speedup_ramp_data.texture_filename)
		set_texture_filename(GTK_WIDGET(g_object_get_data(G_OBJECT(dialog), "texture_entry")), 
			object->speedup_ramp_data.texture_filename);

	gtk_spin_button_set_value(GTK_SPIN_BUTTON(g_object_get_data(G_OBJECT(dialog), 
		"width_spinbutton")), (gfloat)object->speedup_ramp_data.width);
	
	gtk_spin_button_set_value(GTK_SPIN_BUTTON(g_object_get_data(G_OBJECT(dialog), 
		"height_spinbutton")), (gfloat)object->speedup_ramp_data.height);
	
	gtk_spin_button_set_value(GTK_SPIN_BUTTON(g_object_get_data(G_OBJECT(dialog), 
		"angle_spinbutton")), radians_to_degrees(object->speedup_ramp_data.angle));
	
	gtk_spin_button_set_value(GTK_SPIN_BUTTON(g_object_get_data(G_OBJECT(dialog), 
		"activation_width_spinbutton")), (gfloat)object->speedup_ramp_data.activation_width);
	
	gtk_spin_button_set_value(GTK_SPIN_BUTTON(g_object_get_data(G_OBJECT(dialog), 
		"boost_spinbutton")), (gfloat)object->speedup_ramp_data.boost);
	
	
	gtk_widget_show(dialog);
	gtk_main();
}



void on_teleporter_texture_checkbutton_toggled(GtkToggleButton *togglebutton, gpointer user_data)
{
	gboolean sensitive = gtk_toggle_button_get_active(togglebutton);
	
	GtkWidget *dialog = gtk_widget_get_toplevel(GTK_WIDGET(togglebutton));
		
	gtk_widget_set_sensitive(GTK_WIDGET(g_object_get_data(G_OBJECT(dialog), 
		"texture_vbox")), sensitive);

	struct object_t *object = g_object_get_data(G_OBJECT(dialog), "object");
		
	stop_working();
	
	object->teleporter_data.non_default_texture = sensitive;
	
	if(object->teleporter_data.texture_pre_surface)
		invalidate_object(object);
	
	update_client_area();
	start_working();
}


void on_teleporter_texture_entry_changed(GtkFileChooserButton *chooser, gpointer user_data)
{
	GtkWidget *dialog = gtk_widget_get_toplevel(GTK_WIDGET(chooser));
	struct object_t *object = g_object_get_data(G_OBJECT(dialog), "object");

	stop_working();

	free_string(object->teleporter_data.texture_filename);
	object->teleporter_data.texture_filename = NULL;
	
	free_surface(object->teleporter_data.texture_pre_surface);
	object->teleporter_data.texture_pre_surface = NULL;
	
	gchar *strval = get_texture_filename(GTK_WIDGET(chooser));
	if(strval)
	{
		object->teleporter_data.texture_filename = arb_abs2rel(strval, map_path->text);
		
		struct string_t *string = arb_rel2abs(object->teleporter_data.texture_filename->text, 
			map_path->text);
		object->teleporter_data.texture_pre_surface = 
			read_gdk_pixbuf_surface_as_24bitalpha8bit(string->text);
		free_string(string);
	}
	
	g_free(strval);
	
	invalidate_object(object);
	
	update_client_area();
	start_working();
}


void on_teleporter_width_spinbutton_value_changed(GtkSpinButton *spinbutton, gpointer user_data)
{
	GtkWidget *dialog = gtk_widget_get_toplevel(GTK_WIDGET(spinbutton));
	struct object_t *object = g_object_get_data(G_OBJECT(dialog), "object");

	stop_working();

	object->teleporter_data.width = gtk_spin_button_get_value(spinbutton);

	invalidate_object(object);
	
	update_client_area();
	start_working();
}


void on_teleporter_height_spinbutton_value_changed(GtkSpinButton *spinbutton, gpointer user_data)
{
	GtkWidget *dialog = gtk_widget_get_toplevel(GTK_WIDGET(spinbutton));
	struct object_t *object = g_object_get_data(G_OBJECT(dialog), "object");

	stop_working();

	object->teleporter_data.height = gtk_spin_button_get_value(spinbutton);

	invalidate_object(object);
	
	update_client_area();
	start_working();
}


void on_teleporter_angle_spinbutton_value_changed(GtkSpinButton *spinbutton, gpointer user_data)
{
	GtkWidget *dialog = gtk_widget_get_toplevel(GTK_WIDGET(spinbutton));
	struct object_t *object = g_object_get_data(G_OBJECT(dialog), "object");

	stop_working();

	object->teleporter_data.angle = degrees_to_radians(gtk_spin_button_get_value(spinbutton));

	invalidate_object(object);
	
	update_client_area();
	start_working();
}


void on_teleporter_radius_spinbutton_value_changed(GtkSpinButton *spinbutton, gpointer user_data)
{
	GtkWidget *dialog = gtk_widget_get_toplevel(GTK_WIDGET(spinbutton));
	struct object_t *object = g_object_get_data(G_OBJECT(dialog), "object");

	object->teleporter_data.radius = gtk_spin_button_get_value(spinbutton);
}


void on_teleporter_sparkles_spinbutton_value_changed(GtkSpinButton *spinbutton, gpointer user_data)
{
	GtkWidget *dialog = gtk_widget_get_toplevel(GTK_WIDGET(spinbutton));
	struct object_t *object = g_object_get_data(G_OBJECT(dialog), "object");

	object->teleporter_data.sparkles = gtk_spin_button_get_value_as_int(spinbutton);
}


void on_teleporter_spawn_point_index_spinbutton_value_changed(GtkSpinButton *spinbutton, 
	gpointer user_data)
{
	GtkWidget *dialog = gtk_widget_get_toplevel(GTK_WIDGET(spinbutton));
	struct object_t *object = g_object_get_data(G_OBJECT(dialog), "object");

	object->teleporter_data.spawn_point_index = gtk_spin_button_get_value_as_int(spinbutton);
}


void on_teleporter_rotate_type_abs_radiobutton_toggled(GtkToggleButton *togglebutton, 
	gpointer user_data)
{
	gboolean on = gtk_toggle_button_get_active(togglebutton);
	GtkWidget *dialog = gtk_widget_get_toplevel(GTK_WIDGET(togglebutton));
	struct object_t *object = g_object_get_data(G_OBJECT(dialog), "object");

	if(on)
		object->teleporter_data.rotation_type = TELEPORTER_ROTATION_TYPE_ABS;
}


void on_teleporter_rotate_type_rel_radiobutton_toggled(GtkToggleButton *togglebutton, 
	gpointer user_data)
{
	gboolean on = gtk_toggle_button_get_active(togglebutton);
	GtkWidget *dialog = gtk_widget_get_toplevel(GTK_WIDGET(togglebutton));

	gtk_widget_set_sensitive(GTK_WIDGET(g_object_get_data(G_OBJECT(dialog), 
		"divider_angle_hbox")), on);

	struct object_t *object = g_object_get_data(G_OBJECT(dialog), "object");

	if(on)
		object->teleporter_data.rotation_type = TELEPORTER_ROTATION_TYPE_REL;
}


void on_teleporter_rotation_angle_spinbutton_value_changed(GtkSpinButton *spinbutton, 
	gpointer user_data)
{
	GtkWidget *dialog = gtk_widget_get_toplevel(GTK_WIDGET(spinbutton));
	struct object_t *object = g_object_get_data(G_OBJECT(dialog), "object");

	object->teleporter_data.rotation_angle = 
		gtk_spin_button_get_value(spinbutton) / (180.0 / M_PI) + M_PI;
}


void on_teleporter_divider_angle_checkbutton_toggled(GtkToggleButton *togglebutton, 
	gpointer user_data)
{
	gboolean sensitive = gtk_toggle_button_get_active(togglebutton);
	GtkWidget *dialog = gtk_widget_get_toplevel(GTK_WIDGET(togglebutton));

	gtk_widget_set_sensitive(GTK_WIDGET(g_object_get_data(G_OBJECT(dialog), 
		"divider_angle_spinbutton")), sensitive);
	
	struct object_t *object = g_object_get_data(G_OBJECT(dialog), "object");

	object->teleporter_data.divider = gtk_toggle_button_get_active(togglebutton);
}


void on_teleporter_divider_angle_spinbutton_value_changed(GtkSpinButton *spinbutton, 
	gpointer user_data)
{
	GtkWidget *dialog = gtk_widget_get_toplevel(GTK_WIDGET(spinbutton));
	struct object_t *object = g_object_get_data(G_OBJECT(dialog), "object");

	object->teleporter_data.divider_angle = 
		gtk_spin_button_get_value(spinbutton) / (180.0 / M_PI) + M_PI;
}


void on_teleporter_properties_dialog_destroy(GtkObject *dialog, gpointer user_data)
{
	gtk_main_quit();
}


void run_teleporter_properties_dialog(void *menu, struct object_t *object)
{
	GtkWidget *dialog = create_teleporter_properties_dialog();
	g_object_set_data(G_OBJECT(dialog), "object", object);
	
	gtk_window_set_transient_for(GTK_WINDOW(dialog), GTK_WINDOW(window));
	gtk_window_set_position(GTK_WINDOW(dialog), GTK_WIN_POS_CENTER_ON_PARENT);

	if(object->teleporter_data.non_default_texture)
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(g_object_get_data(G_OBJECT(dialog), 
			"texture_checkbutton")), TRUE);
	
	if(object->teleporter_data.texture_filename)
		set_texture_filename(GTK_WIDGET(g_object_get_data(G_OBJECT(dialog), "texture_entry")), 
			object->teleporter_data.texture_filename);

	gtk_spin_button_set_value(GTK_SPIN_BUTTON(g_object_get_data(G_OBJECT(dialog), 
		"width_spinbutton")), (gfloat)object->teleporter_data.width);
	
	gtk_spin_button_set_value(GTK_SPIN_BUTTON(g_object_get_data(G_OBJECT(dialog), 
		"height_spinbutton")), (gfloat)object->teleporter_data.height);
	
	gtk_spin_button_set_value(GTK_SPIN_BUTTON(g_object_get_data(G_OBJECT(dialog), 
		"angle_spinbutton")), radians_to_degrees(object->teleporter_data.angle));
	
	gtk_spin_button_set_value(GTK_SPIN_BUTTON(g_object_get_data(G_OBJECT(dialog), 
		"radius_spinbutton")), (gfloat)object->teleporter_data.radius);
	
	gtk_spin_button_set_value(GTK_SPIN_BUTTON(g_object_get_data(G_OBJECT(dialog), 
		"sparkles_spinbutton")), (gfloat)object->teleporter_data.sparkles);
	
	gtk_spin_button_set_value(GTK_SPIN_BUTTON(g_object_get_data(G_OBJECT(dialog), 
		"spawn_point_index_spinbutton")), (gfloat)object->teleporter_data.spawn_point_index);
	
	if(object->teleporter_data.rotation_type == TELEPORTER_ROTATION_TYPE_ABS)
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(g_object_get_data(G_OBJECT(dialog), 
			"rotate_type_abs_radiobutton")), TRUE);
	else
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(g_object_get_data(G_OBJECT(dialog), 
			"rotate_type_rel_radiobutton")), TRUE);

	gtk_spin_button_set_value(GTK_SPIN_BUTTON(g_object_get_data(G_OBJECT(dialog), 
		"rotation_angle_spinbutton")), (gfloat)(object->teleporter_data.rotation_angle - M_PI) * 
		(180.0 / M_PI));
	
	if(object->teleporter_data.divider)
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(g_object_get_data(G_OBJECT(dialog), 
			"divider_angle_checkbutton")), TRUE);
	
	gtk_spin_button_set_value(GTK_SPIN_BUTTON(g_object_get_data(G_OBJECT(dialog), 
		"divider_angle_spinbutton")), (gfloat)(object->teleporter_data.divider_angle - M_PI) * 
		(180.0 / M_PI));
	
	
	gtk_widget_show(dialog);
	gtk_main();
}



void on_gravity_well_texture_checkbutton_toggled(GtkToggleButton *togglebutton, gpointer user_data)
{
	gboolean sensitive = gtk_toggle_button_get_active(togglebutton);
	
	GtkWidget *dialog = gtk_widget_get_toplevel(GTK_WIDGET(togglebutton));
		
	gtk_widget_set_sensitive(GTK_WIDGET(g_object_get_data(G_OBJECT(dialog), 
		"texture_vbox")), sensitive);

	struct object_t *object = g_object_get_data(G_OBJECT(dialog), "object");
		
	stop_working();
	
	object->gravity_well_data.non_default_texture = sensitive;
	
	if(object->gravity_well_data.texture_pre_surface)
		invalidate_object(object);
	
	update_client_area();
	start_working();
}


void on_gravity_well_texture_entry_changed(GtkFileChooserButton *chooser, gpointer user_data)
{
	GtkWidget *dialog = gtk_widget_get_toplevel(GTK_WIDGET(chooser));
	struct object_t *object = g_object_get_data(G_OBJECT(dialog), "object");

	stop_working();

	free_string(object->gravity_well_data.texture_filename);
	object->gravity_well_data.texture_filename = NULL;
	
	free_surface(object->gravity_well_data.texture_pre_surface);
	object->gravity_well_data.texture_pre_surface = NULL;
	
	gchar *strval = get_texture_filename(GTK_WIDGET(chooser));
	if(strval)
	{
		object->gravity_well_data.texture_filename = arb_abs2rel(strval, map_path->text);
		
		struct string_t *string = arb_rel2abs(object->gravity_well_data.texture_filename->text, 
			map_path->text);
		object->gravity_well_data.texture_pre_surface = 
			read_gdk_pixbuf_surface_as_24bitalpha8bit(string->text);
		free_string(string);
	}
	
	g_free(strval);
	
	invalidate_object(object);
	
	update_client_area();
	start_working();
}


void on_gravity_well_width_spinbutton_value_changed(GtkSpinButton *spinbutton, gpointer user_data)
{
	GtkWidget *dialog = gtk_widget_get_toplevel(GTK_WIDGET(spinbutton));
	struct object_t *object = g_object_get_data(G_OBJECT(dialog), "object");

	stop_working();

	object->gravity_well_data.width = gtk_spin_button_get_value(spinbutton);

	invalidate_object(object);
	
	update_client_area();
	start_working();
}


void on_gravity_well_height_spinbutton_value_changed(GtkSpinButton *spinbutton, gpointer user_data)
{
	GtkWidget *dialog = gtk_widget_get_toplevel(GTK_WIDGET(spinbutton));
	struct object_t *object = g_object_get_data(G_OBJECT(dialog), "object");

	stop_working();

	object->gravity_well_data.height = gtk_spin_button_get_value(spinbutton);

	invalidate_object(object);
	
	update_client_area();
	start_working();
}


void on_gravity_well_angle_spinbutton_value_changed(GtkSpinButton *spinbutton, gpointer user_data)
{
	GtkWidget *dialog = gtk_widget_get_toplevel(GTK_WIDGET(spinbutton));
	struct object_t *object = g_object_get_data(G_OBJECT(dialog), "object");

	stop_working();

	object->gravity_well_data.angle = degrees_to_radians(gtk_spin_button_get_value(spinbutton));

	invalidate_object(object);
	
	update_client_area();
	start_working();
}


void on_gravity_well_strength_spinbutton_value_changed(GtkSpinButton *spinbutton, 
	gpointer user_data)
{
	GtkWidget *dialog = gtk_widget_get_toplevel(GTK_WIDGET(spinbutton));
	struct object_t *object = g_object_get_data(G_OBJECT(dialog), "object");

	object->gravity_well_data.strength = gtk_spin_button_get_value(spinbutton);
}


void on_gravity_well_enclosed_checkbutton_toggled(GtkToggleButton *togglebutton, gpointer user_data)
{
	GtkWidget *dialog = gtk_widget_get_toplevel(GTK_WIDGET(togglebutton));
	struct object_t *object = g_object_get_data(G_OBJECT(dialog), "object");
		
	object->gravity_well_data.enclosed = gtk_toggle_button_get_active(togglebutton);
}


void on_gravity_well_properties_dialog_destroy(GtkObject *dialog, gpointer user_data)
{
	gtk_main_quit();
}


void run_gravity_well_properties_dialog(void *menu, struct object_t *object)
{
	GtkWidget *dialog = create_gravity_well_properties_dialog();
	g_object_set_data(G_OBJECT(dialog), "object", object);
	
	gtk_window_set_transient_for(GTK_WINDOW(dialog), GTK_WINDOW(window));
	gtk_window_set_position(GTK_WINDOW(dialog), GTK_WIN_POS_CENTER_ON_PARENT);

	GtkWidget *texture_checkbutton = g_object_get_data(G_OBJECT(dialog), "texture_checkbutton");
	
	if(object->gravity_well_data.non_default_texture)
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(texture_checkbutton), TRUE);
	
	if(object->gravity_well_data.texture_filename)
		set_texture_filename(GTK_WIDGET(g_object_get_data(G_OBJECT(dialog), "texture_entry")), 
			object->gravity_well_data.texture_filename);

	gtk_spin_button_set_value(GTK_SPIN_BUTTON(g_object_get_data(G_OBJECT(dialog), 
		"width_spinbutton")), (gfloat)object->gravity_well_data.width);
	
	gtk_spin_button_set_value(GTK_SPIN_BUTTON(g_object_get_data(G_OBJECT(dialog), 
		"height_spinbutton")), (gfloat)object->gravity_well_data.height);
	
	gtk_spin_button_set_value(GTK_SPIN_BUTTON(g_object_get_data(G_OBJECT(dialog), 
		"angle_spinbutton")), radians_to_degrees(object->gravity_well_data.angle));
	
	gtk_spin_button_set_value(GTK_SPIN_BUTTON(g_object_get_data(G_OBJECT(dialog), 
		"strength_spinbutton")), (gfloat)object->gravity_well_data.strength);
	
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(g_object_get_data(G_OBJECT(dialog), 
		"enclosed_checkbutton")), (gboolean)object->gravity_well_data.enclosed);


	gtk_widget_show(dialog);
	gtk_main();
}



void menu_delete_object(GtkWidget *menu, struct object_t *object)
{
	delete_object(object);
	update_client_area();
}


void run_object_menu(struct object_t *object)
{
	GtkWidget *menu;
	GtkWidget *menu_items = NULL;
	menu = gtk_menu_new();

	switch(object->type)
	{
	case OBJECTTYPE_PLASMACANNON:
		menu_items = gtk_menu_item_new_with_label("Plasma Cannon Properties");
		g_signal_connect(G_OBJECT(menu_items), "activate", 
			GTK_SIGNAL_FUNC(run_plasma_cannon_properties_dialog), object);
		break;
	
	case OBJECTTYPE_MINIGUN:
		menu_items = gtk_menu_item_new_with_label("Minigun Properties");
		g_signal_connect(G_OBJECT(menu_items), "activate", 
			GTK_SIGNAL_FUNC(run_minigun_properties_dialog), object);
		break;
	
	case OBJECTTYPE_ROCKETLAUNCHER:
		menu_items = gtk_menu_item_new_with_label("Rocket Launcher Properties");
		g_signal_connect(G_OBJECT(menu_items), "activate", 
			GTK_SIGNAL_FUNC(run_rocket_launcher_properties_dialog), object);
		break;
	
	case OBJECTTYPE_RAILS:
		menu_items = gtk_menu_item_new_with_label("Rails Properties");
		g_signal_connect(G_OBJECT(menu_items), "activate", 
			GTK_SIGNAL_FUNC(run_rails_properties_dialog), object);
		break;
	
	case OBJECTTYPE_SHIELDENERGY:
		menu_items = gtk_menu_item_new_with_label("Shield Energy Properties");
		g_signal_connect(G_OBJECT(menu_items), "activate", 
			GTK_SIGNAL_FUNC(run_shield_energy_properties_dialog), object);
		break;
	
	case OBJECTTYPE_SPAWNPOINT:
		menu_items = gtk_menu_item_new_with_label("Spawn Point Properties");
		g_signal_connect(G_OBJECT(menu_items), "activate", 
			GTK_SIGNAL_FUNC(run_spawn_point_properties_dialog), object);
		break;
	
	case OBJECTTYPE_SPEEDUPRAMP:
		menu_items = gtk_menu_item_new_with_label("Speedup Ramp Properties");
		g_signal_connect(G_OBJECT(menu_items), "activate", 
			GTK_SIGNAL_FUNC(run_speedup_ramp_properties_dialog), object);
		break;
	
	case OBJECTTYPE_TELEPORTER:
		menu_items = gtk_menu_item_new_with_label("Teleporter Properties");
		g_signal_connect(G_OBJECT(menu_items), "activate", 
			GTK_SIGNAL_FUNC(run_teleporter_properties_dialog), object);
		break;
	
	case OBJECTTYPE_GRAVITYWELL:
		menu_items = gtk_menu_item_new_with_label("Gravity Well Properties");
		g_signal_connect(G_OBJECT(menu_items), "activate", 
			GTK_SIGNAL_FUNC(run_gravity_well_properties_dialog), object);
		break;
	}
	
	gtk_menu_append(GTK_MENU(menu), menu_items);
	gtk_widget_show(menu_items);
	
	switch(object->type)
	{
	case OBJECTTYPE_PLASMACANNON:
		menu_items = gtk_menu_item_new_with_label("Delete Plasma Cannon");
		break;
	
	case OBJECTTYPE_MINIGUN:
		menu_items = gtk_menu_item_new_with_label("Delete Minigun");
		break;
	
	case OBJECTTYPE_ROCKETLAUNCHER:
		menu_items = gtk_menu_item_new_with_label("Delete Rocket Launcher");
		break;
	
	case OBJECTTYPE_RAILS:
		menu_items = gtk_menu_item_new_with_label("Delete Rails");
		break;
	
	case OBJECTTYPE_SHIELDENERGY:
		menu_items = gtk_menu_item_new_with_label("Delete Shield Energy");
		break;
	
	case OBJECTTYPE_SPAWNPOINT:
		menu_items = gtk_menu_item_new_with_label("Delete Spawn Point");
		break;
	
	case OBJECTTYPE_SPEEDUPRAMP:
		menu_items = gtk_menu_item_new_with_label("Delete Speedup Ramp");
		break;
	
	case OBJECTTYPE_TELEPORTER:
		menu_items = gtk_menu_item_new_with_label("Delete Teleporter");
		break;
	
	case OBJECTTYPE_GRAVITYWELL:
		menu_items = gtk_menu_item_new_with_label("Delete Gravity Well");
		break;
	}
	
	g_signal_connect(G_OBJECT(menu_items), "activate", GTK_SIGNAL_FUNC(menu_delete_object), object);
	gtk_menu_append(GTK_MENU(menu), menu_items);
	gtk_widget_show(menu_items);
	
	gtk_menu_popup (GTK_MENU(menu), NULL, NULL, (GtkMenuPositionFunc)(NULL), NULL, 0, 0);
}


void draw_objects()
{
	struct blit_params_t params;
	params.dest = s_backbuffer;
	
	struct object_t *object = object0;
		
	while(object)
	{
		int x, y;
		world_to_screen(object->x, object->y, &x, &y);
		
		if(zoom == 1.0)
		{
			switch(object->type)
			{
			case OBJECTTYPE_SPAWNPOINT:
				if(object->texture_surface || (object->spawn_point_data.non_default_texture && 
					object->spawn_point_data.texture_pre_surface))
					goto normal;
				
				if(s_spawn_point_placeholder)
				{
					params.source = s_spawn_point_placeholder;
					params.dest_x = x - s_spawn_point_placeholder->width / 2;
					params.dest_y = y - s_spawn_point_placeholder->height / 2;
					blit_surface(&params);
				}
				break;
			
			case OBJECTTYPE_TELEPORTER:
				if(object->texture_surface || (object->teleporter_data.non_default_texture && 
					object->teleporter_data.texture_pre_surface))
					goto normal;
				
				if(s_teleporter_placeholder)
				{
					params.source = s_teleporter_placeholder;
					params.dest_x = x - s_teleporter_placeholder->width / 2;
					params.dest_y = y - s_teleporter_placeholder->height / 2;
					blit_surface(&params);
				}
				break;
			
			case OBJECTTYPE_GRAVITYWELL:
				if(object->texture_surface || (object->gravity_well_data.non_default_texture && 
					object->gravity_well_data.texture_pre_surface))
					goto normal;
				
				if(s_gravity_well_placeholder)
				{
					params.source = s_gravity_well_placeholder;
					params.dest_x = x - s_gravity_well_placeholder->width / 2;
					params.dest_y = y - s_gravity_well_placeholder->height / 2;
					blit_surface(&params);
				}
				break;
			
			case OBJECTTYPE_SHIELDENERGY:
				
				if(s_shield_energy)
				{
					params.source = s_shield_energy;
					params.dest_x = x - s_shield_energy->width / 2;
					params.dest_y = y - s_shield_energy->height / 2;
					params.red = params.green = params.blue = 0xff;
					blit_surface(&params);
				}
				break;

			normal:
			default:
				if(object->texture_surface)
				{
					params.source = object->texture_surface;
					params.dest_x = x - object->texture_surface->width / 2;
					params.dest_y = y - object->texture_surface->height / 2;
					blit_surface(&params);
				}
				break;
			}
		}
		else
		{
			switch(object->type)
			{
			case OBJECTTYPE_SPAWNPOINT:
				if(object->texture_surface)
					goto normal_zoomed;
				
				if(s_scaled_spawn_point_placeholder)
				{
					params.source = s_scaled_spawn_point_placeholder;
					params.dest_x = x - s_scaled_spawn_point_placeholder->width / 2;
					params.dest_y = y - s_scaled_spawn_point_placeholder->height / 2;
					blit_surface(&params);
				}
				break;
			
			case OBJECTTYPE_TELEPORTER:
				if(object->texture_surface)
					goto normal_zoomed;
				
				if(s_scaled_teleporter_placeholder)
				{
					params.source = s_scaled_teleporter_placeholder;
					params.dest_x = x - s_scaled_teleporter_placeholder->width / 2;
					params.dest_y = y - s_scaled_teleporter_placeholder->height / 2;
					blit_surface(&params);
				}
				break;
			
			case OBJECTTYPE_GRAVITYWELL:
				if(object->texture_surface)
					goto normal_zoomed;
				
				if(s_scaled_gravity_well_placeholder)
				{
					params.source = s_scaled_gravity_well_placeholder;
					params.dest_x = x - s_scaled_gravity_well_placeholder->width / 2;
					params.dest_y = y - s_scaled_gravity_well_placeholder->height / 2;
					blit_surface(&params);
				}
				break;
			
			case OBJECTTYPE_SHIELDENERGY:
				
				if(s_scaled_shield_energy)
				{
					params.source = s_scaled_shield_energy;
					params.dest_x = x - s_scaled_shield_energy->width / 2;
					params.dest_y = y - s_scaled_shield_energy->height / 2;
					params.red = params.green = params.blue = 0xff;
					blit_surface(&params);
				}
				break;
			
			normal_zoomed:
			default:
				if(object->scaled_texture_surface)
				{
					params.source = object->scaled_texture_surface;
					params.dest_x = x - object->scaled_texture_surface->width / 2;
					params.dest_y = y - object->scaled_texture_surface->height / 2;
					blit_surface(&params);
				}
				break;
			}
		}
		
		object = object->next;
	}
	
	struct object_pointer_t *cobjectp = selected_object0;
	params.source = s_select;
	
	while(cobjectp)
	{
		int x, y;
		world_to_screen(cobjectp->object->x, cobjectp->object->y, &x, &y);
		params.dest_x = x - 11;
		params.dest_y = y - 11;
		blit_surface(&params);
		
		cobjectp = cobjectp->next;
	}
}


void invalidate_all_scaled_objects()	// always called fom main thread while not working
{
	free_surface(s_scaled_spawn_point_placeholder);
	s_scaled_spawn_point_placeholder = NULL;
	
	free_surface(s_scaled_teleporter_placeholder);
	s_scaled_teleporter_placeholder = NULL;
	
	free_surface(s_scaled_gravity_well_placeholder);
	s_scaled_gravity_well_placeholder = NULL;
	
	free_surface(s_scaled_shield_energy);
	s_scaled_shield_energy = NULL;
	
	struct object_t *object = object0;
		
	while(object)
	{
		if(object->scaled_texture_surface)
		{
			free_surface(object->scaled_texture_surface);
			object->scaled_texture_surface = NULL;
		}
		
		object = object->next;
	}
}


uint32_t count_objects()
{
	uint32_t num_objects = 0;
	struct object_t *object = object0;

	while(object)
	{
		num_objects++;
		object = object->next;
	}

	return num_objects;
}
	

void gzwrite_objects(gzFile file)
{
	int o;
	
	for(o = 0; o < 5; o++)
	{
		gzwrite(file, &dynamic_objects[o].non_default, 4);
		
		if(dynamic_objects[o].non_default)
			gzwrite_string(file, dynamic_objects[o].filename);
	}
		
	
	uint32_t num_objects = count_objects();
	gzwrite(file, &num_objects, 4);

	struct object_t *object = object0;

	while(object)
	{
		gzwrite(file, &object->type, 1);
		gzwrite(file, &object->x, 4);
		gzwrite(file, &object->y, 4);
		
		switch(object->type)
		{
		case OBJECTTYPE_PLASMACANNON:
			gzwrite(file, &object->plasma_cannon_data.plasmas, 4);
			gzwrite(file, &object->plasma_cannon_data.angle, 4);
			gzwrite(file, &object->plasma_cannon_data.respawn_delay, 4);
			break;
		
		case OBJECTTYPE_MINIGUN:
			gzwrite(file, &object->minigun_data.bullets, 4);
			gzwrite(file, &object->minigun_data.angle, 4);
			gzwrite(file, &object->minigun_data.respawn_delay, 4);
			break;
		
		case OBJECTTYPE_ROCKETLAUNCHER:
			gzwrite(file, &object->rocket_launcher_data.rockets, 4);
			gzwrite(file, &object->rocket_launcher_data.angle, 4);
			gzwrite(file, &object->rocket_launcher_data.respawn_delay, 4);
			break;
		
		case OBJECTTYPE_RAILS:
			gzwrite(file, &object->rails_data.quantity, 4);
			gzwrite(file, &object->rails_data.angle, 4);
			gzwrite(file, &object->rails_data.respawn_delay, 4);
			break;
		
		case OBJECTTYPE_SHIELDENERGY:
			gzwrite(file, &object->shield_energy_data.energy, 4);
			gzwrite(file, &object->shield_energy_data.angle, 4);
			gzwrite(file, &object->shield_energy_data.respawn_delay, 4);
			break;
		
		case OBJECTTYPE_SPAWNPOINT:
			gzwrite(file, &object->spawn_point_data.non_default_texture, 4);
			gzwrite_string(file, object->spawn_point_data.texture_filename);
			gzwrite(file, &object->spawn_point_data.width, 4);
			gzwrite(file, &object->spawn_point_data.height, 4);
			gzwrite(file, &object->spawn_point_data.angle, 4);
			gzwrite(file, &object->spawn_point_data.teleport_only, 4);
			gzwrite(file, &object->spawn_point_data.index, 4);
			break;
		
		case OBJECTTYPE_SPEEDUPRAMP:
			gzwrite(file, &object->speedup_ramp_data.non_default_texture, 4);
			gzwrite_string(file, object->speedup_ramp_data.texture_filename);
			gzwrite(file, &object->speedup_ramp_data.width, 4);
			gzwrite(file, &object->speedup_ramp_data.height, 4);
			gzwrite(file, &object->speedup_ramp_data.angle, 4);
			gzwrite(file, &object->speedup_ramp_data.activation_width, 4);
			gzwrite(file, &object->speedup_ramp_data.boost, 4);
			break;
		
		case OBJECTTYPE_TELEPORTER:
			gzwrite(file, &object->teleporter_data.non_default_texture, 4);
			gzwrite_string(file, object->spawn_point_data.texture_filename);
			gzwrite(file, &object->teleporter_data.width, 4);
			gzwrite(file, &object->teleporter_data.height, 4);
			gzwrite(file, &object->teleporter_data.angle, 4);
			gzwrite(file, &object->teleporter_data.radius, 4);
			gzwrite(file, &object->teleporter_data.sparkles, 4);
			gzwrite(file, &object->teleporter_data.spawn_point_index, 4);
			gzwrite(file, &object->teleporter_data.rotation_type, 4);
			gzwrite(file, &object->teleporter_data.rotation_angle, 4);
			gzwrite(file, &object->teleporter_data.divider, 4);
			gzwrite(file, &object->teleporter_data.divider_angle, 4);
			break;
		
		case OBJECTTYPE_GRAVITYWELL:
			gzwrite(file, &object->gravity_well_data.non_default_texture, 4);
			gzwrite_string(file, object->spawn_point_data.texture_filename);
			gzwrite(file, &object->gravity_well_data.width, 4);
			gzwrite(file, &object->gravity_well_data.height, 4);
			gzwrite(file, &object->gravity_well_data.angle, 4);
			gzwrite(file, &object->gravity_well_data.strength, 4);
			gzwrite(file, &object->gravity_well_data.enclosed, 4);
			break;
		}		
		
		object = object->next;
	}
}


void gzwrite_objects_compiled(gzFile file)
{
	uint32_t num_objects = count_objects();
	gzwrite(file, &num_objects, 4);

	struct object_t *object = object0;

	while(object)
	{
		gzwrite(file, &object->type, 1);
		gzwrite(file, &object->x, 4);
		gzwrite(file, &object->y, 4);
		
		switch(object->type)
		{
		case OBJECTTYPE_PLASMACANNON:
			gzwrite(file, &object->plasma_cannon_data.plasmas, 4);
			gzwrite(file, &object->plasma_cannon_data.angle, 4);
			gzwrite(file, &object->plasma_cannon_data.respawn_delay, 4);
			break;
		
		case OBJECTTYPE_MINIGUN:
			gzwrite(file, &object->minigun_data.bullets, 4);
			gzwrite(file, &object->minigun_data.angle, 4);
			gzwrite(file, &object->minigun_data.respawn_delay, 4);
			break;
		
		case OBJECTTYPE_ROCKETLAUNCHER:
			gzwrite(file, &object->rocket_launcher_data.rockets, 4);
			gzwrite(file, &object->rocket_launcher_data.angle, 4);
			gzwrite(file, &object->rocket_launcher_data.respawn_delay, 4);
			break;
		
		case OBJECTTYPE_RAILS:
			gzwrite(file, &object->rails_data.quantity, 4);
			gzwrite(file, &object->rails_data.angle, 4);
			gzwrite(file, &object->rails_data.respawn_delay, 4);
			break;
		
		case OBJECTTYPE_SHIELDENERGY:
			gzwrite(file, &object->shield_energy_data.energy, 4);
			gzwrite(file, &object->shield_energy_data.angle, 4);
			gzwrite(file, &object->shield_energy_data.respawn_delay, 4);
			break;
		
		case OBJECTTYPE_SPAWNPOINT:
			gzwrite(file, &object->spawn_point_data.angle, 4);
			gzwrite(file, &object->spawn_point_data.teleport_only, 4);
			gzwrite(file, &object->spawn_point_data.index, 4);
			break;
		
		case OBJECTTYPE_SPEEDUPRAMP:
			gzwrite(file, &object->speedup_ramp_data.angle, 4);
			gzwrite(file, &object->speedup_ramp_data.activation_width, 4);
			gzwrite(file, &object->speedup_ramp_data.boost, 4);
			break;
		
		case OBJECTTYPE_TELEPORTER:
			gzwrite(file, &object->teleporter_data.radius, 4);
			gzwrite(file, &object->teleporter_data.sparkles, 4);
			gzwrite(file, &object->teleporter_data.spawn_point_index, 4);
			gzwrite(file, &object->teleporter_data.rotation_type, 4);
			gzwrite(file, &object->teleporter_data.rotation_angle, 4);
			gzwrite(file, &object->teleporter_data.divider, 4);
			gzwrite(file, &object->teleporter_data.divider_angle, 4);
			break;
		
		case OBJECTTYPE_GRAVITYWELL:
			gzwrite(file, &object->gravity_well_data.strength, 4);
			gzwrite(file, &object->gravity_well_data.enclosed, 4);
			break;
		}		
		
		object = object->next;
	}
}


int count_object_floating_images()
{
	struct object_t *cobject = object0;
	int c = 0;
		
	while(cobject)
	{
		switch(cobject->type)
		{
		case OBJECTTYPE_SPEEDUPRAMP:
			c++;
			break;
		}
		
		cobject = cobject->next;
	}
	
	return c;
}


void gzwrite_object_floating_images(gzFile file)
{
	struct object_t *cobject = object0;
		
	while(cobject)
	{
		switch(cobject->type)
		{
		case OBJECTTYPE_SPEEDUPRAMP:
			gzwrite(file, &cobject->x, 4);
			gzwrite(file, &cobject->y, 4);
			gzwrite_raw_surface(file, cobject->texture_surface);
			break;
		}
		
		cobject = cobject->next;
	}
}


int gzread_objects(gzFile file)
{
	int o;
	struct string_t *string = NULL;
	
	for(o = 0; o < 5; o++)
	{
		if(gzread(file, &dynamic_objects[o].non_default, 4) != 4)
			goto error;
		
		if(dynamic_objects[o].non_default)
		{
			dynamic_objects[o].filename = gzread_string(file);
			if(!dynamic_objects[o].filename)
				goto error;
			
			string = arb_rel2abs(dynamic_objects[o].filename->text, map_path->text);
			dynamic_objects[o].texture = read_gdk_pixbuf_surface_as_24bitalpha8bit(string->text);
			free_string(string);
			
			assert(dynamic_objects[o].texture);
		}
	}
		
	
	uint32_t num_objects;
	if(gzread(file, &num_objects, 4) != 4)
		goto error;

	while(num_objects--)
	{
		struct object_t object;
			
		memset(&object, 0, sizeof(object));
		
		if(gzread(file, &object.type, 1) != 1)
			goto error;

		if(gzread(file, &object.x, 4) != 4)
			goto error;
		
		if(gzread(file, &object.y, 4) != 4)
			goto error;
		
		
		switch(object.type)
		{
		case OBJECTTYPE_PLASMACANNON:
			if(gzread(file, &object.plasma_cannon_data.plasmas, 4) != 4)
				goto error;
			
			if(gzread(file, &object.plasma_cannon_data.angle, 4) != 4)
				goto error;
			
			if(gzread(file, &object.plasma_cannon_data.respawn_delay, 4) != 4)
				goto error;
			
			break;
		
		case OBJECTTYPE_MINIGUN:
			if(gzread(file, &object.minigun_data.bullets, 4) != 4)
				goto error;
			
			if(gzread(file, &object.minigun_data.angle, 4) != 4)
				goto error;
			
			if(gzread(file, &object.minigun_data.respawn_delay, 4) != 4)
				goto error;
			
			break;
		
		case OBJECTTYPE_ROCKETLAUNCHER:
			if(gzread(file, &object.rocket_launcher_data.rockets, 4) != 4)
				goto error;
			
			if(gzread(file, &object.rocket_launcher_data.angle, 4) != 4)
				goto error;
			
			if(gzread(file, &object.rocket_launcher_data.respawn_delay, 4) != 4)
				goto error;
			
			break;
		
		case OBJECTTYPE_RAILS:
			if(gzread(file, &object.rails_data.quantity, 4) != 4)
				goto error;
			
			if(gzread(file, &object.rails_data.angle, 4) != 4)
				goto error;
			
			if(gzread(file, &object.rails_data.respawn_delay, 4) != 4)
				goto error;
			
			break;
		
		case OBJECTTYPE_SHIELDENERGY:
			if(gzread(file, &object.shield_energy_data.energy, 4) != 4)
				goto error;
			
			if(gzread(file, &object.shield_energy_data.angle, 4) != 4)
				goto error;
			
			if(gzread(file, &object.shield_energy_data.respawn_delay, 4) != 4)
				goto error;
			
			break;
		
		case OBJECTTYPE_SPAWNPOINT:
			if(gzread(file, &object.spawn_point_data.non_default_texture, 4) != 4)
				goto error;
			
			object.spawn_point_data.texture_filename = gzread_string(file);
			if(!object.spawn_point_data.texture_filename)
				goto error;
			
			string = arb_rel2abs(object.spawn_point_data.texture_filename->text, map_path->text);
			object.spawn_point_data.texture_pre_surface = 
				read_gdk_pixbuf_surface_as_24bitalpha8bit(string->text);
			free_string(string);
			
			if(gzread(file, &object.spawn_point_data.width, 4) != 4)
				goto error;
			
			if(gzread(file, &object.spawn_point_data.height, 4) != 4)
				goto error;
			
			if(gzread(file, &object.spawn_point_data.angle, 4) != 4)
				goto error;
			
			if(gzread(file, &object.spawn_point_data.teleport_only, 4) != 4)
				goto error;
			
			if(gzread(file, &object.spawn_point_data.index, 4) != 4)
				goto error;
			
			break;
		
		case OBJECTTYPE_SPEEDUPRAMP:
			if(gzread(file, &object.speedup_ramp_data.non_default_texture, 4) != 4)
				goto error;
			
			object.speedup_ramp_data.texture_filename = gzread_string(file);
			if(!object.speedup_ramp_data.texture_filename)
				goto error;
			
			string = arb_rel2abs(object.speedup_ramp_data.texture_filename->text, map_path->text);
			object.speedup_ramp_data.texture_pre_surface = 
				read_gdk_pixbuf_surface_as_24bitalpha8bit(string->text);
			free_string(string);

			if(gzread(file, &object.speedup_ramp_data.width, 4) != 4)
				goto error;
			
			if(gzread(file, &object.speedup_ramp_data.height, 4) != 4)
				goto error;
			
			if(gzread(file, &object.speedup_ramp_data.angle, 4) != 4)
				goto error;
			
			if(gzread(file, &object.speedup_ramp_data.activation_width, 4) != 4)
				goto error;
			
			if(gzread(file, &object.speedup_ramp_data.boost, 4) != 4)
				goto error;
			
			break;
		
		case OBJECTTYPE_TELEPORTER:
			if(gzread(file, &object.teleporter_data.non_default_texture, 4) != 4)
				goto error;
			
			object.teleporter_data.texture_filename = gzread_string(file);
			if(!object.teleporter_data.texture_filename)
				goto error;
			
			string = arb_rel2abs(object.teleporter_data.texture_filename->text, map_path->text);
			object.teleporter_data.texture_pre_surface = 
				read_gdk_pixbuf_surface_as_24bitalpha8bit(string->text);
			free_string(string);
			
			if(gzread(file, &object.teleporter_data.width, 4) != 4)
				goto error;
			
			if(gzread(file, &object.teleporter_data.height, 4) != 4)
				goto error;
			
			if(gzread(file, &object.teleporter_data.angle, 4) != 4)
				goto error;
			
			if(gzread(file, &object.teleporter_data.radius, 4) != 4)
				goto error;
			
			if(gzread(file, &object.teleporter_data.sparkles, 4) != 4)
				goto error;
			
			if(gzread(file, &object.teleporter_data.spawn_point_index, 4) != 4)
				goto error;
			
			if(gzread(file, &object.teleporter_data.rotation_type, 4) != 4)
				goto error;
			
			if(gzread(file, &object.teleporter_data.rotation_angle, 4) != 4)
				goto error;
			
			if(gzread(file, &object.teleporter_data.divider, 4) != 4)
				goto error;
			
			if(gzread(file, &object.teleporter_data.divider_angle, 4) != 4)
				goto error;
			
			break;
		
		case OBJECTTYPE_GRAVITYWELL:
			if(gzread(file, &object.gravity_well_data.non_default_texture, 4) != 4)
				goto error;
			
			object.gravity_well_data.texture_filename = gzread_string(file);
			if(!object.gravity_well_data.texture_filename)
				goto error;
			
			string = arb_rel2abs(object.gravity_well_data.texture_filename->text, map_path->text);
			object.gravity_well_data.texture_pre_surface = 
				read_gdk_pixbuf_surface_as_24bitalpha8bit(string->text);
			free_string(string);
			
			if(gzread(file, &object.gravity_well_data.width, 4) != 4)
				goto error;
			
			if(gzread(file, &object.gravity_well_data.height, 4) != 4)
				goto error;
			
			if(gzread(file, &object.gravity_well_data.angle, 4) != 4)
				goto error;
			
			if(gzread(file, &object.gravity_well_data.strength, 4) != 4)
				goto error;
			
			if(gzread(file, &object.gravity_well_data.enclosed, 4) != 4)
				goto error;
			
			break;
		}
		
		LL_ADD(struct object_t, &object0, &object);
	}

	
	return 1;

error:

	return 0;
	
	
}


void delete_all_objects()
{
	int o;
	
	for(o = 0; o < 5; o++)
	{
		dynamic_objects[o].non_default = 0;
		
		free_string(dynamic_objects[o].filename);
		dynamic_objects[o].filename = NULL;
		free_surface(dynamic_objects[o].texture);
		dynamic_objects[o].texture = NULL;
	}
		
	while(object0)
		delete_object(object0);
}


void init_objects()
{
	s_unowned_plasma_cannon = read_png_surface(find_resource(
		"stock-object-textures/unowned-plasma-cannon.png"));
	s_unowned_minigun = read_png_surface(find_resource(
		"stock-object-textures/unowned-minigun.png"));
	s_unowned_rocket_launcher = read_png_surface(find_resource(
		"stock-object-textures/unowned-rocket-launcher.png"));
		
	s_rails = read_png_surface(find_resource(
		"stock-object-textures/rails.png"));
	s_spawn_point_placeholder = read_png_surface(find_resource(
		"spawn-point-placeholder.png"));
	s_teleporter_placeholder = read_png_surface(find_resource(
		"teleporter-placeholder.png"));
	s_gravity_well_placeholder = read_png_surface(find_resource(
		"gravity-well-placeholder.png"));
	s_shield_energy = read_png_surface(find_resource(
		"stock-object-textures/shield-pickup.png"));
	
	s_stock_speedup_ramp = read_gdk_pixbuf_surface_as_24bitalpha8bit(find_resource(
		"stock-speedup-ramp.svg"));
}


void kill_objects()
{
	delete_all_objects();

	free_surface(s_unowned_plasma_cannon);		s_unowned_plasma_cannon = NULL;
	free_surface(s_unowned_minigun);			s_unowned_minigun = NULL;
	free_surface(s_unowned_rocket_launcher);	s_unowned_rocket_launcher = NULL;
		
	free_surface(s_rails);						s_rails = NULL;
	free_surface(s_shield_energy);				s_shield_energy = NULL;
	
	free_surface(s_spawn_point_placeholder);	s_spawn_point_placeholder = NULL;
	free_surface(s_teleporter_placeholder);		s_teleporter_placeholder = NULL;
	free_surface(s_gravity_well_placeholder);	s_gravity_well_placeholder = NULL;
		
	free_surface(s_scaled_spawn_point_placeholder);		s_scaled_spawn_point_placeholder = NULL;
	free_surface(s_scaled_teleporter_placeholder);		s_scaled_teleporter_placeholder = NULL;
	free_surface(s_scaled_gravity_well_placeholder);	s_scaled_gravity_well_placeholder = NULL;
	
	free_surface(s_stock_speedup_ramp);			s_stock_speedup_ramp = NULL;
}


int check_for_unresampled_objects()
{
	struct object_t *cobject = object0;
	
	while(cobject)
	{
		int stop = 0;
		
		switch(cobject->type)
		{
		case OBJECTTYPE_SHIELDENERGY:
			stop = 1;
			break;
		
		case OBJECTTYPE_SPAWNPOINT:
			if(!cobject->spawn_point_data.texture_pre_surface)
				stop = 1;
			break;
		
		case OBJECTTYPE_TELEPORTER:
			if(!cobject->teleporter_data.texture_pre_surface)
				stop = 1;
			break;
		
		case OBJECTTYPE_GRAVITYWELL:
			if(!cobject->teleporter_data.texture_pre_surface)
				stop = 1;
			break;
		}
		
		if(stop)
		{
			cobject = cobject->next;
			continue;
		}
		
		if(!cobject->texture_surface)
		{
			working_object = cobject;
			return 1;
		}
		
		cobject = cobject->next;
	}
	
	return 0;
}


void finished_resampling_object()
{
	working_object->texture_surface = working_object_surface;
}


int check_for_unscaled_objects()
{
	if(zoom != 1.0)
	{
		if(s_spawn_point_placeholder && !s_scaled_spawn_point_placeholder)
		{
			object_scale_job = OBJECT_SCALE_JOB_SPAWN_POINT;
			return 1;
		}
			
		if(s_teleporter_placeholder && !s_scaled_teleporter_placeholder)
		{
			object_scale_job = OBJECT_SCALE_JOB_TELEPORTER;
			return 1;
		}
			
		if(s_gravity_well_placeholder && !s_scaled_gravity_well_placeholder)
		{
			object_scale_job = OBJECT_SCALE_JOB_GRAVITY_WELL;
			return 1;
		}
		
		if(s_shield_energy && !s_scaled_shield_energy)
		{
			object_scale_job = OBJECT_SCALE_JOB_SHIELD_ENERGY;
			return 1;
		}
		
		struct object_t *cobject = object0;
		
		while(cobject)
		{
			if(cobject->texture_surface && !cobject->scaled_texture_surface)
			{
				working_object = cobject;
				object_scale_job = OBJECT_SCALE_JOB_SPECIFIC;
				return 1;
			}
			
			cobject = cobject->next;
		}
	}
	
	return 0;
}


void finished_scaling_object()
{
	switch(object_scale_job)
	{
	case OBJECT_SCALE_JOB_SPAWN_POINT:
		s_scaled_spawn_point_placeholder = working_object_surface;
		break;
	
	case OBJECT_SCALE_JOB_TELEPORTER:
		s_scaled_teleporter_placeholder = working_object_surface;
		break;
	
	case OBJECT_SCALE_JOB_GRAVITY_WELL:
		s_scaled_gravity_well_placeholder = working_object_surface;
		break;
	
	case OBJECT_SCALE_JOB_SHIELD_ENERGY:
		s_scaled_shield_energy = working_object_surface;
		break;
	
	case OBJECT_SCALE_JOB_SPECIFIC:
		working_object->scaled_texture_surface = working_object_surface;
		break;
	}
}





//
// WORKER THREAD FUNCTIONS
//


void resample_object()
{
	if(!worker_try_enter_main_lock())
		return;

	switch(working_object->type)
	{
	case OBJECTTYPE_PLASMACANNON:
		
		if(dynamic_objects[OBJECTTYPE_PLASMACANNON].non_default && 
			dynamic_objects[OBJECTTYPE_PLASMACANNON].texture)
		{
			working_object_surface = 
				leave_main_lock_and_rotate_surface(dynamic_objects[OBJECTTYPE_PLASMACANNON].texture, 
				64, 64, working_object->plasma_cannon_data.angle);
		}
		else
		{
			working_object_surface = leave_main_lock_and_rotate_surface(s_unowned_plasma_cannon, 
				64, 64, working_object->plasma_cannon_data.angle);
		}
		break;
	
		
	case OBJECTTYPE_MINIGUN:
		
		if(dynamic_objects[OBJECTTYPE_MINIGUN].non_default && 
			dynamic_objects[OBJECTTYPE_MINIGUN].texture)
		{
			working_object_surface = leave_main_lock_and_rotate_surface(
				dynamic_objects[OBJECTTYPE_MINIGUN].texture, 
				64, 64, working_object->minigun_data.angle);
		}
		else
		{
			working_object_surface = leave_main_lock_and_rotate_surface(s_unowned_minigun, 64, 64, 
				working_object->minigun_data.angle);
		}
		break;
	
		
	case OBJECTTYPE_ROCKETLAUNCHER:
		
		if(dynamic_objects[OBJECTTYPE_ROCKETLAUNCHER].non_default && 
			dynamic_objects[OBJECTTYPE_ROCKETLAUNCHER].texture)
		{
			working_object_surface = 
				leave_main_lock_and_rotate_surface(
				dynamic_objects[OBJECTTYPE_ROCKETLAUNCHER].texture, 
				64, 64, working_object->rocket_launcher_data.angle);
		}
		else
		{
			working_object_surface = leave_main_lock_and_rotate_surface(
				s_unowned_rocket_launcher, 64, 64, 
				working_object->rocket_launcher_data.angle);
		}
		break;
	
		
	case OBJECTTYPE_RAILS:
		
		if(dynamic_objects[OBJECTTYPE_RAILS].non_default && 
			dynamic_objects[OBJECTTYPE_RAILS].texture)
		{
			working_object_surface = leave_main_lock_and_rotate_surface(
				dynamic_objects[OBJECTTYPE_RAILS].texture, 
				64, 64, working_object->rails_data.angle);
		}
		else
		{
			working_object_surface = leave_main_lock_and_rotate_surface(s_rails, 64, 64, 
				working_object->rails_data.angle);
		}
		break;
	
		
	case OBJECTTYPE_SPAWNPOINT:
		working_object_surface = 
			leave_main_lock_and_rotate_surface(working_object->spawn_point_data.texture_pre_surface, 
			working_object->spawn_point_data.width, 
			working_object->spawn_point_data.height,
			working_object->spawn_point_data.angle);
		break;
	
	
	case OBJECTTYPE_SPEEDUPRAMP:
	
		if(working_object->speedup_ramp_data.texture_pre_surface && 
			working_object->speedup_ramp_data.non_default_texture)
		{
			working_object_surface = 
				leave_main_lock_and_rotate_surface(
				working_object->speedup_ramp_data.texture_pre_surface, 
				working_object->speedup_ramp_data.width, 
				working_object->speedup_ramp_data.height,
				working_object->speedup_ramp_data.angle);
		}
		else
		{
			working_object_surface = leave_main_lock_and_rotate_surface(s_stock_speedup_ramp, 
				working_object->speedup_ramp_data.width, 
				working_object->speedup_ramp_data.height,
				working_object->speedup_ramp_data.angle);
		}
		break;
	
		
	case OBJECTTYPE_TELEPORTER:
		working_object_surface = 
			leave_main_lock_and_rotate_surface(working_object->teleporter_data.texture_pre_surface, 
			working_object->teleporter_data.width, 
			working_object->teleporter_data.height,
			working_object->teleporter_data.angle);
		break;
	
	
	case OBJECTTYPE_GRAVITYWELL:
		working_object_surface = 
			leave_main_lock_and_rotate_surface(
			working_object->gravity_well_data.texture_pre_surface, 
			working_object->gravity_well_data.width, 
			working_object->gravity_well_data.height,
			working_object->gravity_well_data.angle);
		break;
	}
}


void scale_object()
{
	if(!worker_try_enter_main_lock())
		return;

	switch(object_scale_job)
	{
	case OBJECT_SCALE_JOB_SPAWN_POINT:
		working_object_surface = leave_main_lock_and_resize_surface(s_spawn_point_placeholder, 
			lround((double)s_spawn_point_placeholder->width * zoom), 
			lround((double)s_spawn_point_placeholder->height * zoom));
		break;
	
	case OBJECT_SCALE_JOB_TELEPORTER:
		working_object_surface = leave_main_lock_and_resize_surface(s_teleporter_placeholder, 
			lround((double)s_teleporter_placeholder->width * zoom), 
			lround((double)s_teleporter_placeholder->height * zoom));
		break;
	
	case OBJECT_SCALE_JOB_GRAVITY_WELL:
		working_object_surface = leave_main_lock_and_resize_surface(s_gravity_well_placeholder, 
			lround((double)s_gravity_well_placeholder->width * zoom), 
			lround((double)s_gravity_well_placeholder->height * zoom));
		break;
	
	case OBJECT_SCALE_JOB_SHIELD_ENERGY:
		working_object_surface = leave_main_lock_and_resize_surface(s_shield_energy, 
			lround((double)s_shield_energy->width * zoom), 
			lround((double)s_shield_energy->height * zoom));
		break;
	
	case OBJECT_SCALE_JOB_SPECIFIC:
		working_object_surface = leave_main_lock_and_resize_surface(working_object->texture_surface, 
			lround(working_object->texture_surface->width * zoom), 
			lround(working_object->texture_surface->height * zoom));
		break;
	}
}



























gboolean
on_rocket_launcher_angle_spinbutton_button_press_event
                                        (GtkWidget       *widget,
                                        GdkEventButton  *event,
                                        gpointer         user_data)
{

  return FALSE;
}


gboolean
on_rocket_launcher_angle_spinbutton_button_release_event
                                        (GtkWidget       *widget,
                                        GdkEventButton  *event,
                                        gpointer         user_data)
{

  return FALSE;
}


gboolean
on_speedup_ramp_width_spinbutton_button_press_event
                                        (GtkWidget       *widget,
                                        GdkEventButton  *event,
                                        gpointer         user_data)
{

  return FALSE;
}


gboolean
on_speedup_ramp_width_spinbutton_button_release_event
                                        (GtkWidget       *widget,
                                        GdkEventButton  *event,
                                        gpointer         user_data)
{

  return FALSE;
}


gboolean
on_speedup_ramp_height_spinbutton_button_press_event
                                        (GtkWidget       *widget,
                                        GdkEventButton  *event,
                                        gpointer         user_data)
{

  return FALSE;
}


gboolean
on_speedup_ramp_height_spinbutton_button_release_event
                                        (GtkWidget       *widget,
                                        GdkEventButton  *event,
                                        gpointer         user_data)
{

  return FALSE;
}


gboolean
on_speedup_ramp_angle_spinbutton_button_press_event
                                        (GtkWidget       *widget,
                                        GdkEventButton  *event,
                                        gpointer         user_data)
{

  return FALSE;
}


gboolean
on_speedup_ramp_angle_spinbutton_button_release_event
                                        (GtkWidget       *widget,
                                        GdkEventButton  *event,
                                        gpointer         user_data)
{

  return FALSE;
}


gboolean
on_minigun_angle_spinbutton_button_press_event
                                        (GtkWidget       *widget,
                                        GdkEventButton  *event,
                                        gpointer         user_data)
{

  return FALSE;
}


gboolean
on_minigun_angle_spinbutton_button_release_event
                                        (GtkWidget       *widget,
                                        GdkEventButton  *event,
                                        gpointer         user_data)
{

  return FALSE;
}


gboolean
on_rails_angle_spinbutton_button_press_event
                                        (GtkWidget       *widget,
                                        GdkEventButton  *event,
                                        gpointer         user_data)
{

  return FALSE;
}


gboolean
on_rails_angle_spinbutton_button_release_event
                                        (GtkWidget       *widget,
                                        GdkEventButton  *event,
                                        gpointer         user_data)
{

  return FALSE;
}


gboolean
on_shield_energy_angle_spinbutton_button_press_event
                                        (GtkWidget       *widget,
                                        GdkEventButton  *event,
                                        gpointer         user_data)
{

  return FALSE;
}


gboolean
on_shield_energy_angle_spinbutton_button_release_event
                                        (GtkWidget       *widget,
                                        GdkEventButton  *event,
                                        gpointer         user_data)
{

  return FALSE;
}


gboolean
on_spawn_point_width_spinbutton_button_press_event
                                        (GtkWidget       *widget,
                                        GdkEventButton  *event,
                                        gpointer         user_data)
{

  return FALSE;
}


gboolean
on_spawn_point_width_spinbutton_button_release_event
                                        (GtkWidget       *widget,
                                        GdkEventButton  *event,
                                        gpointer         user_data)
{

  return FALSE;
}


gboolean
on_spawn_point_height_spinbutton_button_press_event
                                        (GtkWidget       *widget,
                                        GdkEventButton  *event,
                                        gpointer         user_data)
{

  return FALSE;
}


gboolean
on_spawn_point_height_spinbutton_button_release_event
                                        (GtkWidget       *widget,
                                        GdkEventButton  *event,
                                        gpointer         user_data)
{

  return FALSE;
}


gboolean
on_spawn_point_angle_spinbutton_button_press_event
                                        (GtkWidget       *widget,
                                        GdkEventButton  *event,
                                        gpointer         user_data)
{

  return FALSE;
}


gboolean
on_spawn_point_angle_spinbutton_button_release_event
                                        (GtkWidget       *widget,
                                        GdkEventButton  *event,
                                        gpointer         user_data)
{

  return FALSE;
}

gboolean
on_gravity_well_width_spinbutton_button_press_event
                                        (GtkWidget       *widget,
                                        GdkEventButton  *event,
                                        gpointer         user_data)
{

  return FALSE;
}


gboolean
on_gravity_well_width_spinbutton_button_release_event
                                        (GtkWidget       *widget,
                                        GdkEventButton  *event,
                                        gpointer         user_data)
{

  return FALSE;
}


gboolean
on_gravity_well_height_spinbutton_button_press_event
                                        (GtkWidget       *widget,
                                        GdkEventButton  *event,
                                        gpointer         user_data)
{

  return FALSE;
}


gboolean
on_gravity_well_height_spinbutton_button_release_event
                                        (GtkWidget       *widget,
                                        GdkEventButton  *event,
                                        gpointer         user_data)
{

  return FALSE;
}


gboolean
on_gravity_well_angle_spinbutton_button_press_event
                                        (GtkWidget       *widget,
                                        GdkEventButton  *event,
                                        gpointer         user_data)
{

  return FALSE;
}


gboolean
on_gravity_well_angle_spinbutton_button_release_event
                                        (GtkWidget       *widget,
                                        GdkEventButton  *event,
                                        gpointer         user_data)
{

  return FALSE;
}


gboolean
on_teleporter_width_spinbutton_button_press_event
                                        (GtkWidget       *widget,
                                        GdkEventButton  *event,
                                        gpointer         user_data)
{

  return FALSE;
}


gboolean
on_teleporter_width_spinbutton_button_release_event
                                        (GtkWidget       *widget,
                                        GdkEventButton  *event,
                                        gpointer         user_data)
{

  return FALSE;
}


gboolean
on_teleporter_height_spinbutton_button_press_event
                                        (GtkWidget       *widget,
                                        GdkEventButton  *event,
                                        gpointer         user_data)
{

  return FALSE;
}


gboolean
on_teleporter_height_spinbutton_button_release_event
                                        (GtkWidget       *widget,
                                        GdkEventButton  *event,
                                        gpointer         user_data)
{

  return FALSE;
}


gboolean
on_teleporter_angle_spinbutton_button_press_event
                                        (GtkWidget       *widget,
                                        GdkEventButton  *event,
                                        gpointer         user_data)
{

  return FALSE;
}


gboolean
on_teleporter_angle_spinbutton_button_release_event
                                        (GtkWidget       *widget,
                                        GdkEventButton  *event,
                                        gpointer         user_data)
{

  return FALSE;
}
