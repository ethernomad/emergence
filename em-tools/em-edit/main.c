/**
 * @file
 * @copyright 1998-2004 Jonathan Brown <jbrown@bluedroplet.com>
 * @license https://www.gnu.org/licenses/gpl-3.0.html
 * @homepage https://github.com/bluedroplet/emergence
 */

#define _GNU_SOURCE
#ifndef _REENTRANT
#define _REENTRANT
#endif

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <unistd.h>
#include <time.h>

#include <gtk/gtk.h>

#include "prefix.h"

#include "minmax.h"
#include "user.h"
#include "gsub.h"
#include "interface.h"
#include "map.h"
#include "cameron.h"
#include "worker.h"
#include "worker_thread.h"

GdkVisual *visual;
GdkGC *gc;
GtkWidget *window;
GtkWidget *drawing_area;
GdkImage *image;

uint16_t *preimage;


struct surface_t *s_backbuffer;

int vid_width, vid_height;

void destroy_window()
{
	gtk_widget_destroy(window);
}


void draw()
{
	clear_surface(s_backbuffer);
	draw_all();
}	


void update_client_area()
{
	if(image)
	{
		draw();
		gdk_draw_image(drawing_area->window, gc, image, 0, 0, 0, 0, vid_width, vid_height);
	}
}


gint on_key_press(GtkWidget *widget, GdkEventKey *event, gpointer callback_data)
{

	switch(event->type)
	{
	case GDK_KEY_PRESS:
		key_pressed(event->keyval);
		break;
	
	case GDK_KEY_RELEASE:
		key_released(event->keyval);
		break;
	
	default:
		break;
	}
	
	return TRUE;
}


gint on_button_press(GtkWidget *widget, GdkEventButton *event, gpointer callback_data)
{
	switch(event->button)
	{
	case 1:
		left_button_down((int)event->x, (int)event->y);
		break;

	case 3:
		right_button_down((int)event->x, (int)event->y, (int)event->x_root, (int)event->y_root);
		break;
	}

	return TRUE;
}


gint on_button_release(GtkWidget *widget, GdkEventButton *event, gpointer callback_data)
{
	switch(event->button)
	{
	case 1:
		left_button_up((int)event->x, (int)event->y);
		break;

	case 3:
		right_button_up((int)event->x, (int)event->y);
		break;
	}
	
	return TRUE;
}


gint on_scroll(GtkWidget *widget, GdkEventScroll *event, gpointer callback_data)
{
	switch(event->direction)
	{
	case GDK_SCROLL_UP:
		mouse_wheel_up((int)event->x, (int)event->y);
		break;

	case GDK_SCROLL_DOWN:
		mouse_wheel_down((int)event->x, (int)event->y);
		break;
	
	case GDK_SCROLL_LEFT:
		break;
	
	case GDK_SCROLL_RIGHT:
		break;
	}
	
	return TRUE;
}


gint on_motion_notify(GtkWidget *widget, GdkEventMotion *event, gpointer callback_data)
{
	int x, y;
	
	if(event->is_hint)
		gdk_window_get_pointer (event->window, &x, &y, NULL);
	else
	{
		x = (int)event->x;
		y = (int)event->y;
	}
	
	mouse_move(x, y);
	
	return TRUE;
}


gint on_expose(GtkWidget *widget, GdkEventExpose *event, gpointer callback_data)
{
	if(image)
	{
		gdk_draw_image(drawing_area->window, gc, image, 0, 0, 0, 0, vid_width, vid_height);
	}
	
	return TRUE;
}


gint on_configure(GtkWidget *widget, GdkEventConfigure *event, gpointer callback_data)
{
	if(!gc)
		gc = gdk_gc_new(drawing_area->window);

	if(vid_width == event->width && vid_height == event->height)
		return TRUE;

	if(event->width == 0 || event->height == 0)
	{
		if(image)
		{
			gdk_image_destroy(image);
			image = NULL;
		}
		
		if(visual->depth == 24)
		{
			free_surface(s_backbuffer);
			s_backbuffer = NULL;
		}
		
		return FALSE;
	}
	
	vid_width = event->width;
	vid_height = event->height;

	window_size_changed();
	
	if(image)
		gdk_image_destroy(image);

	image = gdk_image_new(GDK_IMAGE_FASTEST, visual, vid_width, vid_height); // presume it is black

	if(visual->depth == 24)
	{
		s_backbuffer = new_surface_no_buf(SURFACE_24BITPADDING8BIT, vid_width, vid_height);
	}
	else
	{
		s_backbuffer = new_surface_no_buf(SURFACE_16BIT, vid_width, vid_height);
	}
	
	s_backbuffer->buf = image->mem;
	s_backbuffer->pitch = image->bpl;
	
	draw();

	return TRUE;
}


gint on_delete(GtkWidget *widget, GdkEvent *event, gpointer data)
{
	quit();
	return TRUE;
}


gint on_destroy(GtkWidget *widget, gpointer data)
{
	kill_worker();
	kill_cameron();
	kill_map();
	kill_interface();
	kill_gsub();
	
	gtk_main_quit();
	return TRUE;
}


void on_worker_callback(gpointer data, gint source, GdkInputCondition condition)
{
	char c;
	
	read(mypipe[0], &c, 1);
	job_complete_callback();
}


int main(int argc, char *argv[])
{
	gtk_init(&argc, &argv);
	
	init_user();
	init_gsub();
	init_interface();
	init_map();
	init_cameron();
	init_worker();

	visual = gdk_visual_get_system();
	
	window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
	gtk_window_set_title(GTK_WINDOW(window), "Emergence Editor");
	gtk_window_set_default_size(GTK_WINDOW(window), 512, 512);
	drawing_area = gtk_drawing_area_new();
	gtk_container_add(GTK_CONTAINER(window), drawing_area);
	gtk_window_set_position(GTK_WINDOW(window), GTK_WIN_POS_CENTER);
	
	g_signal_connect(G_OBJECT(window), "key-press-event", G_CALLBACK(on_key_press), NULL);
	g_signal_connect(G_OBJECT(window), "key-release-event", G_CALLBACK(on_key_press), NULL);
	g_signal_connect(G_OBJECT(window), "delete-event", G_CALLBACK(on_delete), NULL);
 	g_signal_connect(G_OBJECT(window), "destroy", G_CALLBACK(on_destroy), NULL);
	g_signal_connect(G_OBJECT(drawing_area), "button-press-event", 
		G_CALLBACK(on_button_press), NULL);
	g_signal_connect(G_OBJECT(drawing_area), "button-release-event", 
		G_CALLBACK(on_button_release), NULL);
	g_signal_connect(G_OBJECT(drawing_area), "scroll-event", G_CALLBACK(on_scroll), NULL);
	g_signal_connect(G_OBJECT(drawing_area), "motion-notify-event", 
		G_CALLBACK(on_motion_notify), NULL);
	g_signal_connect(G_OBJECT(drawing_area), "expose-event", G_CALLBACK(on_expose), NULL);
	g_signal_connect(G_OBJECT(drawing_area), "configure-event", G_CALLBACK(on_configure), NULL);

	gtk_widget_set_events(drawing_area, GDK_EXPOSURE_MASK
			| GDK_KEY_PRESS_MASK
			| GDK_KEY_RELEASE_MASK
			| GDK_BUTTON_PRESS_MASK
			| GDK_BUTTON_RELEASE_MASK
			| GDK_SCROLL_MASK
			| GDK_POINTER_MOTION_MASK
			| GDK_POINTER_MOTION_HINT_MASK);

	gtk_widget_show_all(window);

	gdk_input_add(mypipe[0], GDK_INPUT_READ, on_worker_callback, NULL);

	gtk_main();
	
	return EXIT_SUCCESS;
}
