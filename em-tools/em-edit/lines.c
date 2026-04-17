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

#include <zlib.h>
#include <gtk/gtk.h>

#include "llist.h"
#include "stringbuf.h"
#include "inout.h"
#include "points.h"
#include "lines.h"
#include "interface.h"
#include "main.h"
#include "glade.h"

#define LINE_THRESHOLD 5
#define LINE_THRESHOLD_SQUARED (LINE_THRESHOLD * LINE_THRESHOLD)

struct line_t *line0 = NULL;

int new_door_index = 0;

struct line_t *new_line()
{
	struct line_t *line;
	LL_CALLOC(struct line_t, &line0, &line);
	return line;
}


int add_line_pointer(struct line_pointer_t **linep0, struct line_t *line)
{
	if(!linep0)
		return 0;
	
	if(!*linep0)
	{
		*linep0 = malloc(sizeof(struct line_pointer_t));
		(*linep0)->line = line;
		(*linep0)->next = NULL;
	}
	else
	{
		struct line_pointer_t *clinep = *linep0;

		while(clinep->next)
		{
			if(clinep->line == line)
				return 0;

			clinep = clinep->next;
		}

		if(clinep->line != line)
		{
			clinep->next = malloc(sizeof(struct line_pointer_t));
			clinep = clinep->next;
			clinep->line = line;
			clinep->next = NULL;
		}
		else
		{
			return 0;
		}
	}

	return 1;
}


void insert_line(struct point_t *point1, struct point_t *point2)
{
	struct line_t *line = new_line();
	
	line->status = 0;
	
	line->point1 = point1;
	line->point2 = point2;
	
	line->door_red = 0;
	line->door_green = 0;
	line->door_blue = 255;
	line->door_alpha = 128;
	
	line->door_initial_state = DOOR_INITIAL_STATE_OPEN;
	
	line->door_width = 5.0;
	line->door_energy = 60.0;
	
	line->door_open_timeout = 0;
	line->door_close_timeout = 0;
	
	line->door_index = new_door_index++;

	line->switch_red = 0;
	line->switch_green = 0;
	line->switch_blue = 0;
	line->switch_alpha = 0;
	
	line->switch_in_door_close_list = NULL;
	line->switch_in_door_open_list = NULL;
	line->switch_in_door_invert_list = NULL;
	
	line->switch_out_door_close_list = NULL;
	line->switch_out_door_open_list = NULL;
	line->switch_out_door_invert_list = NULL;
	
}


void delete_line(struct line_t *line)
{
	LL_REMOVE(struct line_t, &line0, line);
}


struct line_t *get_line(float x, float y)
{
	struct line_t *line = line0;

	while(line)
	{
		// check circles at ends of line
		
		if(((x - line->point1->x) * (x - line->point1->x) + 
			(y - line->point1->y) * (y - line->point1->y)) < LINE_THRESHOLD_SQUARED)
			return line;
		
		if(((x - line->point2->x) * (x - line->point2->x) + 
			(y - line->point2->y) * (y - line->point2->y)) < LINE_THRESHOLD_SQUARED)
			return line;
		
		
		// see if in potential area
		
		double deltax = line->point2->x - line->point1->x;
		double deltay = line->point2->y - line->point1->y;
		
		if(inout(line->point1->x + deltay, line->point1->y - deltax, 
			line->point1->x - deltay, line->point1->y + deltax, x, y) &&
			inout(line->point2->x - deltay, line->point2->y + deltax, 
			line->point2->x + deltay, line->point2->y - deltax, x, y))
		{
			// check point to line distance
			
			if((fabs((deltax * (line->point1->y - y)) - (deltay * (line->point1->x - x))) / 
				sqrt(deltax * deltax + deltay * deltay)) < LINE_THRESHOLD)
				return line;
		}

		line = line->next;
	}

	return NULL;
}


void on_door_checkbutton_toggled(GtkToggleButton *togglebutton, gpointer user_data)
{
	gboolean sensitive = gtk_toggle_button_get_active(togglebutton);
	
	GtkWidget *dialog = gtk_widget_get_toplevel(GTK_WIDGET(togglebutton));
		
	gtk_widget_set_sensitive(GTK_WIDGET(g_object_get_data(G_OBJECT(dialog), 
		"door_vbox")), sensitive);
}


void on_switch_checkbutton_toggled(GtkToggleButton *togglebutton, gpointer user_data)
{
	gboolean sensitive = gtk_toggle_button_get_active(togglebutton);
	
	GtkWidget *dialog = gtk_widget_get_toplevel(GTK_WIDGET(togglebutton));
		
	gtk_widget_set_sensitive(GTK_WIDGET(g_object_get_data(G_OBJECT(dialog), 
		"switch_vbox")), sensitive);
}


void on_door_colorpicker_color_set(GtkWidget *colorpicker, 
	guint red, guint green, guint blue, guint alpha, gpointer user_data)
{
	;
}


void on_door_initial_state_open_radiobutton_toggled(GtkToggleButton *togglebutton, 
	gpointer user_data)
{
	;
}


void on_door_initial_state_closed_radiobutton_toggled(GtkToggleButton *togglebutton, 
	gpointer user_data)
{
	;
}


void on_door_energy_spinbutton_value_changed(GtkSpinButton *spinbutton, gpointer user_data)
{
	;
}


void on_door_open_timeout_spinbutton_value_changed(GtkEditable *editable, gpointer user_data)
{
	;
}


void on_door_close_timeout_spinbutton_value_changed(GtkEditable *editable, gpointer user_data)
{
	;
}


void on_door_index_spinbutton_value_changed(GtkEditable *editable, gpointer user_data)
{
	;
}


void on_switch_colorpicker_color_set(GtkWidget *colorpicker, 
	guint red, guint green, guint blue, guint alpha, gpointer user_data)
{
	;
}


void on_switch_on_enter_close_list_entry_changed(GtkEditable *editable, gpointer user_data)
{
	;
}


void on_switch_on_enter_open_list_entry_changed(GtkEditable *editable, gpointer user_data)
{
	;
}


void on_switch_on_enter_invert_list_entry_changed(GtkEditable *editable, gpointer user_data)
{
	;
}


void on_switch_on_leave_close_list_entry_changed(GtkEditable *editable, gpointer user_data)
{
	;
}


void on_switch_on_leave_open_list_entry_changed(GtkEditable *editable, gpointer user_data)
{
	;
}


void on_switch_on_leave_invert_list_entry_changed(GtkEditable *editable, gpointer user_data)
{
	;
}


void on_door_width_spinbutton_value_changed(GtkEditable *editable, gpointer user_data)
{
	;
}


void on_switch_width_spinbutton_value_changed(GtkEditable *editable, gpointer user_data)
{
	;
}


void on_door_switch_properties_dialog_destroy(GtkObject *dialog, gpointer user_data)
{
	struct line_t *line = g_object_get_data(G_OBJECT(dialog), "line");

	if(gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(g_object_get_data(G_OBJECT(dialog), 
		"door_checkbutton"))))
		line->status |= LINE_STATUS_DOOR;
	else
		line->status &= ~LINE_STATUS_DOOR;

	if(gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(g_object_get_data(G_OBJECT(dialog), 
		"switch_checkbutton"))))
		line->status |= LINE_STATUS_SWITCH;
	else
		line->status &= ~LINE_STATUS_SWITCH;
	
	{ GdkColor _c; gtk_color_button_get_color(GTK_COLOR_BUTTON(g_object_get_data(G_OBJECT(dialog), 
		"door_colorpicker")), &_c); line->door_red=_c.red>>8; line->door_green=_c.green>>8; line->door_blue=_c.blue>>8; line->door_alpha=gtk_color_button_get_alpha(GTK_COLOR_BUTTON(g_object_get_data(G_OBJECT(dialog), 
		"door_colorpicker")))>>8; }
	
	line->door_width = 
		gtk_spin_button_get_value(GTK_SPIN_BUTTON(g_object_get_data(G_OBJECT(dialog), 
		"door_width_spinbutton")));
	
	line->door_energy = 
		gtk_spin_button_get_value(GTK_SPIN_BUTTON(g_object_get_data(G_OBJECT(dialog), 
		"door_energy_spinbutton")));
	
	if(gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(g_object_get_data(G_OBJECT(dialog), 
		"door_initial_state_open_radiobutton"))))
		line->door_initial_state = DOOR_INITIAL_STATE_OPEN;
	else
		line->door_initial_state = DOOR_INITIAL_STATE_CLOSED;
	
	line->door_open_timeout = 
		gtk_spin_button_get_value(GTK_SPIN_BUTTON(g_object_get_data(G_OBJECT(dialog), 
		"door_open_timeout_spinbutton")));
	
	line->door_close_timeout = 
		gtk_spin_button_get_value(GTK_SPIN_BUTTON(g_object_get_data(G_OBJECT(dialog), 
		"door_close_timeout_spinbutton")));
	
	line->door_index = gtk_spin_button_get_value(GTK_SPIN_BUTTON(g_object_get_data(G_OBJECT(dialog), 
		"door_index_spinbutton")));
	
	{ GdkColor _c; gtk_color_button_get_color(GTK_COLOR_BUTTON(g_object_get_data(G_OBJECT(dialog), 
		"switch_colorpicker")), &_c); line->switch_red=_c.red>>8; line->switch_green=_c.green>>8; line->switch_blue=_c.blue>>8; line->switch_alpha=gtk_color_button_get_alpha(GTK_COLOR_BUTTON(g_object_get_data(G_OBJECT(dialog), 
		"switch_colorpicker")))>>8; }
	
	line->switch_width = 
		gtk_spin_button_get_value(GTK_SPIN_BUTTON(g_object_get_data(G_OBJECT(dialog), 
		"switch_width_spinbutton")));

	
	LL_REMOVE_ALL(struct line_pointer_t, &line->switch_in_door_close_list);
	
	struct string_t *string = 
		new_string_text(gtk_entry_get_text(GTK_ENTRY(g_object_get_data(G_OBJECT(dialog), 
		"switch_on_enter_close_list_entry"))));
	
	char *token = strtok(string->text, 
		" qwertyuiopasdfghjklzxcvbnm!\"£$%^&*()-_=+[];'#,./<>?:@~{}\\|");
	
	while(token)
	{
		int index = atoi(token);
		struct line_t *cline = line0;
			
		while(cline)
		{
			if(cline->door_index == index)
			{
				add_line_pointer(&line->switch_in_door_close_list, cline);
				break;
			}
			
			cline = cline->next;
		}
		
		token = strtok(NULL, " qwertyuiopasdfghjklzxcvbnm!\"£$%^&*()-_=+[];'#,./<>?:@~{}\\|");
	}

	
	LL_REMOVE_ALL(struct line_pointer_t, &line->switch_in_door_open_list);
	
	string = new_string_text(gtk_entry_get_text(GTK_ENTRY(g_object_get_data(G_OBJECT(dialog), 
		"switch_on_enter_open_list_entry"))));
	
	token = strtok(string->text, " qwertyuiopasdfghjklzxcvbnm!\"£$%^&*()-_=+[];'#,./<>?:@~{}\\|");
	
	while(token)
	{
		int index = atoi(token);
		struct line_t *cline = line0;
			
		while(cline)
		{
			if(cline->door_index == index)
			{
				add_line_pointer(&line->switch_in_door_open_list, cline);
				break;
			}
			
			cline = cline->next;
		}
		
		token = strtok(NULL, " qwertyuiopasdfghjklzxcvbnm!\"£$%^&*()-_=+[];'#,./<>?:@~{}\\|");
	}

	
	LL_REMOVE_ALL(struct line_pointer_t, &line->switch_in_door_invert_list);
	
	string = new_string_text(gtk_entry_get_text(GTK_ENTRY(g_object_get_data(G_OBJECT(dialog), 
		"switch_on_enter_invert_list_entry"))));
	
	token = strtok(string->text, " qwertyuiopasdfghjklzxcvbnm!\"£$%^&*()-_=+[];'#,./<>?:@~{}\\|");
	
	while(token)
	{
		int index = atoi(token);
		struct line_t *cline = line0;
			
		while(cline)
		{
			if(cline->door_index == index)
			{
				add_line_pointer(&line->switch_in_door_invert_list, cline);
				break;
			}
			
			cline = cline->next;
		}
		
		token = strtok(NULL, " qwertyuiopasdfghjklzxcvbnm!\"£$%^&*()-_=+[];'#,./<>?:@~{}\\|");
	}

	
	LL_REMOVE_ALL(struct line_pointer_t, &line->switch_out_door_close_list);
	
	string = new_string_text(gtk_entry_get_text(GTK_ENTRY(g_object_get_data(G_OBJECT(dialog), 
		"switch_on_leave_close_list_entry"))));
	
	token = strtok(string->text, " qwertyuiopasdfghjklzxcvbnm!\"£$%^&*()-_=+[];'#,./<>?:@~{}\\|");
	
	while(token)
	{
		int index = atoi(token);
		struct line_t *cline = line0;
			
		while(cline)
		{
			if(cline->door_index == index)
			{
				add_line_pointer(&line->switch_out_door_close_list, cline);
				break;
			}
			
			cline = cline->next;
		}
		
		token = strtok(NULL, " qwertyuiopasdfghjklzxcvbnm!\"£$%^&*()-_=+[];'#,./<>?:@~{}\\|");
	}

	
	LL_REMOVE_ALL(struct line_pointer_t, &line->switch_out_door_open_list);
	
	string = new_string_text(gtk_entry_get_text(GTK_ENTRY(g_object_get_data(G_OBJECT(dialog), 
		"switch_on_leave_open_list_entry"))));
	
	token = strtok(string->text, " qwertyuiopasdfghjklzxcvbnm!\"£$%^&*()-_=+[];'#,./<>?:@~{}\\|");
	
	while(token)
	{
		int index = atoi(token);
		struct line_t *cline = line0;
			
		while(cline)
		{
			if(cline->door_index == index)
			{
				add_line_pointer(&line->switch_out_door_open_list, cline);
				break;
			}
			
			cline = cline->next;
		}
		
		token = strtok(NULL, " qwertyuiopasdfghjklzxcvbnm!\"£$%^&*()-_=+[];'#,./<>?:@~{}\\|");
	}

	
	LL_REMOVE_ALL(struct line_pointer_t, &line->switch_out_door_invert_list);
	
	string = new_string_text(gtk_entry_get_text(GTK_ENTRY(g_object_get_data(G_OBJECT(dialog), 
		"switch_on_leave_invert_list_entry"))));
	
	token = strtok(string->text, " qwertyuiopasdfghjklzxcvbnm!\"£$%^&*()-_=+[];'#,./<>?:@~{}\\|");
	
	while(token)
	{
		int index = atoi(token);
		struct line_t *cline = line0;
			
		while(cline)
		{
			if(cline->door_index == index)
			{
				add_line_pointer(&line->switch_out_door_invert_list, cline);
				break;
			}
			
			cline = cline->next;
		}
		
		token = strtok(NULL, " qwertyuiopasdfghjklzxcvbnm!\"£$%^&*()-_=+[];'#,./<>?:@~{}\\|");
	}

	gtk_main_quit();
}


void run_door_switch_properties_dialog(void *menu, struct line_t *line)
{
	GtkWidget *dialog = create_door_switch_properties_dialog();
	
	g_object_set_data(G_OBJECT(dialog), "line", line);
	
	gtk_window_set_transient_for(GTK_WINDOW(dialog), GTK_WINDOW(window));
	gtk_window_set_position(GTK_WINDOW(dialog), GTK_WIN_POS_CENTER_ON_PARENT);
	
	if(line->status & LINE_STATUS_DOOR)
	{
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(g_object_get_data(G_OBJECT(dialog), 
			"door_checkbutton")), TRUE);
	}

	{ GdkColor _c = {0, line->door_red<<8, line->door_green<<8, line->door_blue<<8}; gtk_color_button_set_color(GTK_COLOR_BUTTON(g_object_get_data(G_OBJECT(dialog), 
		"door_colorpicker")), &_c); gtk_color_button_set_alpha(GTK_COLOR_BUTTON(g_object_get_data(G_OBJECT(dialog), 
		"door_colorpicker")), line->door_alpha<<8); }
	
	gtk_spin_button_set_value(GTK_SPIN_BUTTON(g_object_get_data(G_OBJECT(dialog), 
		"door_width_spinbutton")), (gfloat)line->door_width);
	
	gtk_spin_button_set_value(GTK_SPIN_BUTTON(g_object_get_data(G_OBJECT(dialog), 
		"door_energy_spinbutton")), (gfloat)line->door_energy);
	
	switch(line->door_initial_state)
	{
	case DOOR_INITIAL_STATE_OPEN:
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(g_object_get_data(G_OBJECT(dialog), 
			"door_initial_state_open_radiobutton")), TRUE);
		break;
	
	case DOOR_INITIAL_STATE_CLOSED:
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(g_object_get_data(G_OBJECT(dialog), 
			"door_initial_state_closed_radiobutton")), TRUE);
		break;
	}

	gtk_spin_button_set_value(GTK_SPIN_BUTTON(g_object_get_data(G_OBJECT(dialog), 
		"door_open_timeout_spinbutton")), (gfloat)line->door_open_timeout);
	
	gtk_spin_button_set_value(GTK_SPIN_BUTTON(g_object_get_data(G_OBJECT(dialog), 
		"door_close_timeout_spinbutton")), (gfloat)line->door_close_timeout);
	
	gtk_spin_button_set_value(GTK_SPIN_BUTTON(g_object_get_data(G_OBJECT(dialog), 
		"door_index_spinbutton")), (gfloat)line->door_index);
	
	if(line->status & LINE_STATUS_SWITCH)
	{
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(g_object_get_data(G_OBJECT(dialog), 
			"switch_checkbutton")), TRUE);
	}
	
	{ GdkColor _c = {0, line->switch_red<<8, line->switch_green<<8, line->switch_blue<<8}; gtk_color_button_set_color(GTK_COLOR_BUTTON(g_object_get_data(G_OBJECT(dialog), 
		"switch_colorpicker")), &_c); gtk_color_button_set_alpha(GTK_COLOR_BUTTON(g_object_get_data(G_OBJECT(dialog), 
		"switch_colorpicker")), line->switch_alpha<<8); }
	
	gtk_spin_button_set_value(GTK_SPIN_BUTTON(g_object_get_data(G_OBJECT(dialog), 
		"switch_width_spinbutton")), (gfloat)line->switch_width);
	

	
	struct string_t *string = new_string();
		
	struct line_pointer_t *linep = line->switch_in_door_close_list;
	
	if(linep)
	{
		string_cat_int(string, linep->line->door_index);
		linep = linep->next;
	}
	
	while(linep)
	{
		string_cat_char(string, ' ');
		string_cat_int(string, linep->line->door_index);
		linep = linep->next;
	}
	
	gtk_entry_set_text(GTK_ENTRY(g_object_get_data(G_OBJECT(dialog), 
		"switch_on_enter_close_list_entry")), string->text);
	
	string_clear(string);
	
	linep = line->switch_in_door_open_list;
	
	if(linep)
	{
		string_cat_int(string, linep->line->door_index);
		linep = linep->next;
	}
	
	while(linep)
	{
		string_cat_char(string, ' ');
		string_cat_int(string, linep->line->door_index);
		linep = linep->next;
	}
	
	gtk_entry_set_text(GTK_ENTRY(g_object_get_data(G_OBJECT(dialog), 
		"switch_on_enter_open_list_entry")), string->text);
	
	string_clear(string);
	
	linep = line->switch_in_door_invert_list;
	
	if(linep)
	{
		string_cat_int(string, linep->line->door_index);
		linep = linep->next;
	}
	
	while(linep)
	{
		string_cat_char(string, ' ');
		string_cat_int(string, linep->line->door_index);
		linep = linep->next;
	}
	
	gtk_entry_set_text(GTK_ENTRY(g_object_get_data(G_OBJECT(dialog), 
		"switch_on_enter_invert_list_entry")), string->text);
	
	string_clear(string);
	
	linep = line->switch_out_door_close_list;
	
	if(linep)
	{
		string_cat_int(string, linep->line->door_index);
		linep = linep->next;
	}
	
	while(linep)
	{
		string_cat_char(string, ' ');
		string_cat_int(string, linep->line->door_index);
		linep = linep->next;
	}
	
	gtk_entry_set_text(GTK_ENTRY(g_object_get_data(G_OBJECT(dialog), 
		"switch_on_leave_close_list_entry")), string->text);
	
	string_clear(string);
	
	linep = line->switch_out_door_open_list;
	
	if(linep)
	{
		string_cat_int(string, linep->line->door_index);
		linep = linep->next;
	}
	
	while(linep)
	{
		string_cat_char(string, ' ');
		string_cat_int(string, linep->line->door_index);
		linep = linep->next;
	}
	
	gtk_entry_set_text(GTK_ENTRY(g_object_get_data(G_OBJECT(dialog), 
		"switch_on_leave_open_list_entry")), string->text);
	
	string_clear(string);
	
	linep = line->switch_out_door_invert_list;
	
	if(linep)
	{
		string_cat_int(string, linep->line->door_index);
		linep = linep->next;
	}
	
	while(linep)
	{
		string_cat_char(string, ' ');
		string_cat_int(string, linep->line->door_index);
		linep = linep->next;
	}
	
	gtk_entry_set_text(GTK_ENTRY(g_object_get_data(G_OBJECT(dialog), 
		"switch_on_leave_invert_list_entry")), string->text);
	
	free_string(string);
	
	gtk_widget_show(dialog);
	gtk_main();
}


void draw_some_lines()
{
	struct line_t *cline = line0;
		
	while(cline)
	{
		draw_world_clipped_line(cline->point1->x, cline->point1->y, 
			cline->point2->x, cline->point2->y);
		cline = cline->next;
	}
}


void draw_all_lines()
{
	draw_some_lines();
}


void draw_switches_and_doors()
{
	;
}


uint32_t count_line_pointers(struct line_pointer_t *linep0)
{
	uint32_t num_pointers = 0;
	struct line_pointer_t *clinep = linep0;

	while(clinep)
	{
		num_pointers++;
		clinep = clinep->next;
	}

	return num_pointers;
}


struct line_t *get_line_from_index(uint32_t index)
{
	struct line_t *cline = line0;

	while(cline)
	{
		if(cline->door_index == index)
			return cline;

		cline = cline->next;
	}

	return NULL;
}


void gzwrite_line_pointer_list(gzFile file, struct line_pointer_t *linep0)
{
	uint32_t num_lines = count_line_pointers(linep0);
	gzwrite(file, &num_lines, 4);

	struct line_pointer_t *clinep = linep0;

	while(clinep)
	{
		gzwrite(file, &clinep->line->door_index, 4);
		clinep = clinep->next;
	}
}


int gzread_line_pointer_list(gzFile file, struct line_pointer_t **linep0)
{
	uint32_t num_lines;
	gzread(file, &num_lines, 4);

	while(num_lines)
	{
		struct line_pointer_t linep;

		uint32_t line_index;
		if(gzread(file, &line_index, 4) != 4)
			goto error;

		linep.line = get_line_from_index(line_index);
		if(!linep.line)
			goto error;

		LL_ADD(struct line_pointer_t, linep0, &linep);

		num_lines--;
	}

	return 1;

error:

	LL_REMOVE_ALL(struct line_pointer_t, linep0);
	return 0;
}


uint32_t count_lines()
{
	uint32_t num_lines = 0;
	struct line_t *cline = line0;

	while(cline)
	{
		num_lines++;
		cline = cline->next;
	}

	return num_lines;
}


void gzwrite_lines(gzFile file)
{
	uint32_t num_lines = count_lines();
	gzwrite(file, &num_lines, 4);

	struct line_t *cline = line0;

	while(cline)
	{
		gzwrite(file, &cline->status, 1);
		gzwrite(file, &cline->point1->index, 4);
		gzwrite(file, &cline->point2->index, 4);
		
		gzwrite(file, &cline->door_red, 1);
		gzwrite(file, &cline->door_green, 1);
		gzwrite(file, &cline->door_blue, 1);
		gzwrite(file, &cline->door_alpha, 1);

		gzwrite(file, &cline->door_width, 4);
		gzwrite(file, &cline->door_energy, 4);
		
		gzwrite(file, &cline->door_initial_state, 1);
		gzwrite(file, &cline->door_open_timeout, 2);
		gzwrite(file, &cline->door_close_timeout, 2);
		gzwrite(file, &cline->door_index, 2);
		
		gzwrite(file, &cline->switch_red, 1);
		gzwrite(file, &cline->switch_green, 1);
		gzwrite(file, &cline->switch_blue, 1);
		gzwrite(file, &cline->switch_alpha, 1);
		gzwrite(file, &cline->switch_width, 4);
		
		gzwrite_line_pointer_list(file, cline->switch_in_door_close_list);
		gzwrite_line_pointer_list(file, cline->switch_in_door_open_list);
		gzwrite_line_pointer_list(file, cline->switch_in_door_invert_list);
		gzwrite_line_pointer_list(file, cline->switch_out_door_close_list);
		gzwrite_line_pointer_list(file, cline->switch_out_door_open_list);
		gzwrite_line_pointer_list(file, cline->switch_out_door_invert_list);
		
		cline = cline->next;
	}
}
	

int gzread_lines(gzFile file)
{
	uint32_t num_lines;
	if(gzread(file, &num_lines, 4) != 4)
		goto error;

	while(num_lines)
	{
		struct line_t *cline = new_line();
		memset(cline, 0, sizeof(struct line_t));

		if(gzread(file, &cline->status, 1) != 1)
			goto error;

		uint32_t point_index;
		if(gzread(file, &point_index, 4) != 4)
			goto error;

		cline->point1 = get_point_from_index(point_index);
		if(!cline->point1)
			goto error;

		if(gzread(file, &point_index, 4) != 4)
			goto error;

		cline->point2 = get_point_from_index(point_index);
		if(!cline->point2)
			goto error;

		if(gzread(file, &cline->door_red, 1) != 1)
			goto error;

		if(gzread(file, &cline->door_green, 1) != 1)
			goto error;

		if(gzread(file, &cline->door_blue, 1) != 1)
			goto error;

		if(gzread(file, &cline->door_alpha, 1) != 1)
			goto error;

		
		if(gzread(file, &cline->door_width, 4) != 4)
			goto error;

		if(gzread(file, &cline->door_energy, 4) != 4)
			goto error;

		
		if(gzread(file, &cline->door_initial_state, 1) != 1)
			goto error;

		if(gzread(file, &cline->door_open_timeout, 2) != 2)
			goto error;

		if(gzread(file, &cline->door_close_timeout, 2) != 2)
			goto error;

		if(gzread(file, &cline->door_index, 2) != 2)
			goto error;

		
		if(gzread(file, &cline->switch_red, 1) != 1)
			goto error;

		if(gzread(file, &cline->switch_green, 1) != 1)
			goto error;

		if(gzread(file, &cline->switch_blue, 1) != 1)
			goto error;

		if(gzread(file, &cline->switch_alpha, 1) != 1)
			goto error;

		if(gzread(file, &cline->switch_width, 4) != 4)
			goto error;


		if(!gzread_line_pointer_list(file, &cline->switch_in_door_close_list))
			goto error;

		if(!gzread_line_pointer_list(file, &cline->switch_in_door_open_list))
			goto error;

		if(!gzread_line_pointer_list(file, &cline->switch_in_door_invert_list))
			goto error;

		if(!gzread_line_pointer_list(file, &cline->switch_out_door_close_list))
			goto error;

		if(!gzread_line_pointer_list(file, &cline->switch_out_door_open_list))
			goto error;

		if(!gzread_line_pointer_list(file, &cline->switch_out_door_invert_list))
			goto error;

		num_lines--;
	}

	return 1;

error:

	return 0;
}


void delete_all_lines()		// always called when not working
{
	LL_REMOVE_ALL(struct line_t, &line0);
}




void menu_delete_line(GtkWidget *menu, struct line_t *line)
{
	delete_line(line);
	update_client_area();
}


void run_line_menu(struct line_t *line)
{
	GtkWidget *menu;
	GtkWidget *menu_items;
	menu = gtk_menu_new();

	menu_items = gtk_menu_item_new_with_label("Door/Switch Properties");
	gtk_signal_connect(GTK_OBJECT(menu_items), "activate", 
		GTK_SIGNAL_FUNC(run_door_switch_properties_dialog), line);
	gtk_menu_append(GTK_MENU(menu), menu_items);
	gtk_widget_show(menu_items);
	
	menu_items = gtk_menu_item_new_with_label("Delete Door/Switch");
	gtk_signal_connect(GTK_OBJECT(menu_items), "activate", 
		GTK_SIGNAL_FUNC(menu_delete_line), line);
	gtk_menu_append(GTK_MENU(menu), menu_items);
	gtk_widget_show(menu_items);

	gtk_menu_popup (GTK_MENU(menu), NULL, NULL, (GtkMenuPositionFunc)(NULL), NULL, 0, 0);
}
