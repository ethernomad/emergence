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
#include <assert.h>

#include <gtk/gtk.h>
#include <zlib.h>

#include "prefix.h"

#include "llist.h"
#include "resource.h"
#include "gsub.h"
#include "conns.h"
#include "bezier.h"
#include "nodes.h"
#include "conns.h"
#include "curves.h"
#include "points.h"
#include "interface.h"
#include "main.h"

#define POINT_THRESHOLD 12
#define POINT_THRESHOLD_SQUARED (POINT_THRESHOLD * POINT_THRESHOLD)

struct point_t *point0 = NULL;

struct surface_t *s_pointnode;


struct point_t *new_point()
{
	struct point_t *point;
	LL_CALLOC(struct point_t, &point0, &point);
	return point;
}


int add_point_pointer(struct point_pointer_t **pointp0, struct point_t *point)
{
	if(!pointp0)
		return 0;

	if(!*pointp0)
	{
		*pointp0 = malloc(sizeof(struct point_pointer_t));
		(*pointp0)->point = point;
		(*pointp0)->next = NULL;
	}
	else
	{
		struct point_pointer_t *cpointp = *pointp0;

		while(cpointp->next)
		{
			if(cpointp->point == point)
				return 0;

			cpointp = cpointp->next;
		}

		if(cpointp->point != point)
		{
			cpointp->next = malloc(sizeof(struct point_pointer_t));
			cpointp = cpointp->next;
			cpointp->point = point;
			cpointp->next = NULL;
		}
		else
		{
			return 0;
		}
	}

	return 1;
}


void insert_point(struct curve_t *curve, float x, float y)
{
	struct conn_pointer_t *connp = curve->connp0;
	
	int first = 1;
	double min_dist = 0.0;	// shut up compiler
	double min_pos = 0.0;	// shut up compiler
	double acc_pos = 0.0;
	
	double px = 0.0, py = 0.0, pt = 0.0;	// shut up compiler
	
	struct conn_t *pconn = NULL;
	int pt_index = 0;
	
	while(connp)
	{
		struct t_t *ct = connp->conn->bigt0;
		int t_index = 0;
		
		while(ct)
		{
			double xdelta = x - ct->x1;
			double ydelta = y - ct->y1;
			
			double dist = xdelta * xdelta + ydelta * ydelta;
			
			if(first)
			{
				min_dist = dist;
				min_pos = acc_pos;
				px = ct->x1;
				py = ct->y1;
				pt = 0.0;
				pconn = connp->conn;
				pt_index = t_index;
				first = 0;
			}
			else
			{
				if(dist < min_dist)
				{
					min_dist = dist;
					min_pos = acc_pos;
					px = ct->x1;
					py = ct->y1;
					pt = ct->t1;
					pconn = connp->conn;
					pt_index = t_index;
				}
			}
			
			acc_pos += ct->dist;
			
			if(!ct->next && !connp->next)
			{
				xdelta = x - ct->x2;
				ydelta = y - ct->y2;
				
				dist = xdelta * xdelta + ydelta * ydelta;

				if(first)
				{
					min_dist = dist;
					min_pos = acc_pos;
					px = ct->x2;
					py = ct->y2;
					pt = ct->t2;
					pconn = connp->conn;
					pt_index = t_index;
					first = 0;
				}
				else
				{
					if(dist < min_dist)
					{
						min_dist = dist;
						min_pos = acc_pos;
						px = ct->x2;
						py = ct->y2;
						pt = ct->t2;
						pconn = connp->conn;
						pt_index = t_index;
					}
				}
			}				
			
			t_index++;
			ct = ct->next;
		}
		
		connp = connp->next;
	}
	
	
	
	struct point_t *point = new_point();
	
	point->curve = curve;
	point->pos = min_pos / get_curve_length(curve);
	point->x = px;
	point->y = py;
	
	struct bezier_t bezier;
	
	if(!pconn->orientation)
	{
		bezier.x1 = pconn->node1->effective_x[pconn->sat1];
		bezier.y1 = pconn->node1->effective_y[pconn->sat1];
		bezier.x2 = pconn->node1->effective_x[pconn->sat1] + pconn->node1->sats[pconn->sat1].x;
		bezier.y2 = pconn->node1->effective_y[pconn->sat1] + pconn->node1->sats[pconn->sat1].y;
		bezier.x3 = pconn->node2->effective_x[pconn->sat2] + pconn->node2->sats[pconn->sat2].x;
		bezier.y3 = pconn->node2->effective_y[pconn->sat2] + pconn->node2->sats[pconn->sat2].y;
		bezier.x4 = pconn->node2->effective_x[pconn->sat2];
		bezier.y4 = pconn->node2->effective_y[pconn->sat2];
	}
	else
	{
		bezier.x1 = pconn->node2->effective_x[pconn->sat2];
		bezier.y1 = pconn->node2->effective_y[pconn->sat2];
		bezier.x2 = pconn->node2->effective_x[pconn->sat2] + pconn->node2->sats[pconn->sat2].x;
		bezier.y2 = pconn->node2->effective_y[pconn->sat2] + pconn->node2->sats[pconn->sat2].y;
		bezier.x3 = pconn->node1->effective_x[pconn->sat1] + pconn->node1->sats[pconn->sat1].x;
		bezier.y3 = pconn->node1->effective_y[pconn->sat1] + pconn->node1->sats[pconn->sat1].y;
		bezier.x4 = pconn->node1->effective_x[pconn->sat1];
		bezier.y4 = pconn->node1->effective_y[pconn->sat1];
	}
	
	deltaBRZ(&bezier, pt, &point->deltax, &point->deltay);
	
	double l = hypot(point->deltax, point->deltay);
	
	point->deltax /= l;
	point->deltay /= l;

	int left1, right1, left2, right2;
	get_width_sats(pconn, &left1, &right1, &left2, &right2);
			
	point->left_width = (pconn->node2->width[left2] - pconn->node1->width[left1]) * pt + 
		pconn->node1->width[left1];
	point->right_width = (pconn->node2->width[right2] - pconn->node1->width[right1]) * pt + 
		pconn->node1->width[right1];
	
	point->conn = pconn;
	point->t = pt;
	point->t_index = pt_index;
}


void remove_point(struct point_t *point)
{
	LL_REMOVE(struct point_t, &point0, point);
}


void update_point_positions()
{
	struct point_t *cpoint = point0;

	while(cpoint)
	{
		double pt = 0.0;
		struct conn_t *pconn = NULL;
		uint32_t pt_index = 0;
		double px = 0.0, py = 0.0;
		
		double curve_length = get_curve_length(cpoint->curve);

		int stop = 0;
		double lastpos = 0.0;
		
		double realpos = cpoint->pos * curve_length;
				
		struct conn_pointer_t *cconnp = cpoint->curve->connp0;

		while(cconnp)
		{
			struct t_t *ct = cconnp->conn->bigt0;
			int t_index = 0;
				
			assert(ct);
			
			while(ct)
			{
				lastpos += ct->dist;
				
				if(realpos < lastpos)
				{
					px = ct->x1;
					py = ct->y1;
					pconn = cconnp->conn;
					pt = ct->t1;
					pt_index = t_index;
					stop = 1;
					break;
				}
				
				if(!ct->next && !cconnp->next)
				{
					px = ct->x2;
					py = ct->y2;
					pconn = cconnp->conn;
					pt_index = t_index + 1;
					pt = ct->t2;
					stop = 1;
					break;
				}
				
				t_index++;
				ct = ct->next;
			}
			
			if(stop)
				break;
			
			cconnp = cconnp->next;
		}
			

		cpoint->x = px;
		cpoint->y = py;

		struct bezier_t bezier;
		
		if(!pconn->orientation)
		{
			bezier.x1 = pconn->node1->effective_x[pconn->sat1];
			bezier.y1 = pconn->node1->effective_y[pconn->sat1];
			bezier.x2 = pconn->node1->effective_x[pconn->sat1] + pconn->node1->sats[pconn->sat1].x;
			bezier.y2 = pconn->node1->effective_y[pconn->sat1] + pconn->node1->sats[pconn->sat1].y;
			bezier.x3 = pconn->node2->effective_x[pconn->sat2] + pconn->node2->sats[pconn->sat2].x;
			bezier.y3 = pconn->node2->effective_y[pconn->sat2] + pconn->node2->sats[pconn->sat2].y;
			bezier.x4 = pconn->node2->effective_x[pconn->sat2];
			bezier.y4 = pconn->node2->effective_y[pconn->sat2];
		}
		else
		{
			bezier.x1 = pconn->node2->effective_x[pconn->sat2];
			bezier.y1 = pconn->node2->effective_y[pconn->sat2];
			bezier.x2 = pconn->node2->effective_x[pconn->sat2] + pconn->node2->sats[pconn->sat2].x;
			bezier.y2 = pconn->node2->effective_y[pconn->sat2] + pconn->node2->sats[pconn->sat2].y;
			bezier.x3 = pconn->node1->effective_x[pconn->sat1] + pconn->node1->sats[pconn->sat1].x;
			bezier.y3 = pconn->node1->effective_y[pconn->sat1] + pconn->node1->sats[pconn->sat1].y;
			bezier.x4 = pconn->node1->effective_x[pconn->sat1];
			bezier.y4 = pconn->node1->effective_y[pconn->sat1];
		}
				
		deltaBRZ(&bezier, pt, &cpoint->deltax, &cpoint->deltay);
		double l = hypot(cpoint->deltax, cpoint->deltay);
		cpoint->deltax /= l;
		cpoint->deltay /= l;
	
		int left1, right1, left2, right2;
		get_width_sats(pconn, &left1, &right1, &left2, &right2);
		
		cpoint->left_width = (pconn->node2->width[left2] - pconn->node1->width[left1]) * pt + 
			pconn->node1->width[left1];
		cpoint->right_width = (pconn->node2->width[right2] - pconn->node1->width[right1]) * pt + 
			pconn->node1->width[right1];
	
		cpoint->conn = pconn;
		cpoint->t = pt;
		cpoint->t_index = pt_index;
		
		cpoint = cpoint->next;
	}
}


struct point_t *get_point(int x, int y, int *xoffset, int *yoffset)
{
	struct point_t *cpoint = point0;

	while(cpoint)
	{
		int point_screenx, point_screeny;
		world_to_screen(cpoint->x, cpoint->y, &point_screenx, &point_screeny);

		int deltax = x - point_screenx;
		int deltay = y - point_screeny;

		double dist_squared = (double)deltax * (double)deltax + (double)deltay * (double)deltay;

		if(dist_squared < POINT_THRESHOLD_SQUARED)
		{
			if(xoffset)
				*xoffset = deltax;

			if(yoffset)
				*yoffset = deltay;

			return cpoint;
		}

		cpoint = cpoint->next;
	}

	return NULL;
}


void move_point(struct point_t *point, float x, float y)
{
	struct conn_pointer_t *connp = point->curve->connp0;
	
	int first = 1;
	double min_dist = 0.0;	// shut up compiler
	double min_pos = 0.0;	// shut up compiler
	double acc_pos = 0.0;
	
	double px = 0.0, py = 0.0, pt = 0.0;	// shut up compiler
	
	struct conn_t *pconn = NULL;
	int pt_index = 0;
	
	while(connp)
	{
		struct t_t *ct = connp->conn->bigt0;
		int t_index = 0;
		
		while(ct)
		{
			double xdelta = x - ct->x1;
			double ydelta = y - ct->y1;
			
			double dist = xdelta * xdelta + ydelta * ydelta;
			
			if(first)
			{
				min_dist = dist;
				min_pos = acc_pos;
				px = ct->x1;
				py = ct->y1;
				pt = 0.0;
				pconn = connp->conn;
				pt_index = t_index;
				first = 0;
			}
			else
			{
				if(dist < min_dist)
				{
					min_dist = dist;
					min_pos = acc_pos;
					px = ct->x1;
					py = ct->y1;
					pt = ct->t1;
					pt_index = t_index;
					pconn = connp->conn;
				}
			}
			
			acc_pos += ct->dist;
			
			if(!ct->next && !connp->next)
			{
				xdelta = x - ct->x2;
				ydelta = y - ct->y2;
				
				dist = xdelta * xdelta + ydelta * ydelta;

				if(first)
				{
					min_dist = dist;
					min_pos = acc_pos;
					px = ct->x2;
					py = ct->y2;
					pt = ct->t2;
					pconn = connp->conn;
					pt_index = t_index;
					first = 0;
				}
				else
				{
					if(dist < min_dist)
					{
						min_dist = dist;
						min_pos = acc_pos;
						px = ct->x2;
						py = ct->y2;
						pt = ct->t2;
						pconn = connp->conn;
						pt_index = t_index;
					}
				}
			}				
			
			t_index++;
			ct = ct->next;
		}
		
		connp = connp->next;
	}
	

	point->pos = min_pos / get_curve_length(point->curve);
	point->x = px;
	point->y = py;
	
	struct bezier_t bezier;
	
	if(!pconn->orientation)
	{
		bezier.x1 = pconn->node1->effective_x[pconn->sat1];
		bezier.y1 = pconn->node1->effective_y[pconn->sat1];
		bezier.x2 = pconn->node1->effective_x[pconn->sat1] + pconn->node1->sats[pconn->sat1].x;
		bezier.y2 = pconn->node1->effective_y[pconn->sat1] + pconn->node1->sats[pconn->sat1].y;
		bezier.x3 = pconn->node2->effective_x[pconn->sat2] + pconn->node2->sats[pconn->sat2].x;
		bezier.y3 = pconn->node2->effective_y[pconn->sat2] + pconn->node2->sats[pconn->sat2].y;
		bezier.x4 = pconn->node2->effective_x[pconn->sat2];
		bezier.y4 = pconn->node2->effective_y[pconn->sat2];
	}
	else
	{
		bezier.x1 = pconn->node2->effective_x[pconn->sat2];
		bezier.y1 = pconn->node2->effective_y[pconn->sat2];
		bezier.x2 = pconn->node2->effective_x[pconn->sat2] + pconn->node2->sats[pconn->sat2].x;
		bezier.y2 = pconn->node2->effective_y[pconn->sat2] + pconn->node2->sats[pconn->sat2].y;
		bezier.x3 = pconn->node1->effective_x[pconn->sat1] + pconn->node1->sats[pconn->sat1].x;
		bezier.y3 = pconn->node1->effective_y[pconn->sat1] + pconn->node1->sats[pconn->sat1].y;
		bezier.x4 = pconn->node1->effective_x[pconn->sat1];
		bezier.y4 = pconn->node1->effective_y[pconn->sat1];
	}
	
	deltaBRZ(&bezier, pt, &point->deltax, &point->deltay);
	
	double l = hypot(point->deltax, point->deltay);
	
	point->deltax /= l;
	point->deltay /= l;

	int left1, right1, left2, right2;
	get_width_sats(pconn, &left1, &right1, &left2, &right2);
			
	point->left_width = (pconn->node2->width[left2] - pconn->node1->width[left1]) * pt + 
		pconn->node1->width[left1];
	point->right_width = (pconn->node2->width[right2] - pconn->node1->width[right1]) * pt + 
		pconn->node1->width[right1];

	point->conn = pconn;
	point->t_index = pt_index;
	point->t = pt;
}


struct point_t *get_point_from_index(uint32_t index)
{
	struct point_t *cpoint = point0;

	while(cpoint)
	{
		if(cpoint->index == index)
			return cpoint;

		cpoint = cpoint->next;
	}

	return NULL;
}


uint32_t count_point_pointers(struct point_pointer_t *pointp0)
{
	uint32_t num_pointers = 0;
	struct point_pointer_t *cpointp = pointp0;

	while(cpointp)
	{
		num_pointers++;
		cpointp = cpointp->next;
	}

	return num_pointers;
}

void gzwrite_point_pointer_list(gzFile file, struct point_pointer_t *pointp0)
{
	uint32_t num_points = count_point_pointers(pointp0);
	gzwrite(file, &num_points, 4);

	struct point_pointer_t *cpointp = pointp0;

	while(cpointp)
	{
		gzwrite(file, &cpointp->point->index, 4);
		cpointp = cpointp->next;
	}
}


uint32_t count_points()
{
	uint32_t num_points = 0;
	struct point_t *cpoint = point0;

	while(cpoint)
	{
		num_points++;
		cpoint = cpoint->next;
	}

	return num_points;
}


void gzwrite_points(gzFile file)
{
	uint32_t num_points = count_points();
	gzwrite(file, &num_points, 4);

	struct point_t *cpoint = point0;
	uint32_t point_index = 0;

	while(cpoint)
	{
		cpoint->index = point_index++;
		gzwrite(file, &cpoint->curve->index, 4);
		gzwrite(file, &cpoint->pos, 4);
		
		cpoint = cpoint->next;

	}
}


int gzread_points(gzFile file)
{
	uint32_t num_points;
	if(gzread(file, &num_points, 4) != 4)
		goto error;

	uint32_t cpoint_index = 0;

	while(cpoint_index < num_points)
	{
		struct point_t *cpoint = new_point();

		cpoint->index = cpoint_index;

		uint32_t curve_index;
		if(gzread(file, &curve_index, 4) != 4)
			goto error;

		cpoint->curve = get_curve_from_index(curve_index);
		if(!cpoint->curve)
			goto error;

		if(gzread(file, &cpoint->pos, 4) != 4)
			goto error;

		cpoint_index++;
	}
	
	update_point_positions();

	return 1;

error:

	return 0;
}


void delete_all_points()		// always called when not working
{
	LL_REMOVE_ALL(struct point_t, &point0);
}


void draw_points()
{
	struct blit_params_t params;
	params.source = s_pointnode;
	params.dest = s_backbuffer;
	
	struct point_t *point = point0;
		
	while(point)
	{
		int x, y;
		world_to_screen(point->x, point->y, &x, &y);
		params.dest_x = x - 2;
		params.dest_y = y - 2;
		blit_surface(&params);

		point = point->next;
	}
}


void init_points()
{
	s_pointnode = read_png_surface(find_resource("point.png"));
}


void kill_points()
{
	free_surface(s_pointnode);
}






void menu_join_point(GtkWidget *menu, struct point_t *point)
{
	join_point(point);
}


void menu_define_fill(GtkWidget *menu, struct point_t *point)
{
	define_fill(point);
}


void run_point_menu(struct point_t *point)
{
	GtkWidget *menu;
	GtkWidget *menu_items;
	menu = gtk_menu_new();

	menu_items = gtk_menu_item_new_with_label("Define Door/Switch");
	gtk_signal_connect(GTK_OBJECT(menu_items), "activate", 
		GTK_SIGNAL_FUNC(menu_join_point), point);
	gtk_menu_append(GTK_MENU(menu), menu_items);
	gtk_widget_show(menu_items);
	
	menu_items = gtk_menu_item_new_with_label("Define Fill");
	gtk_signal_connect(GTK_OBJECT(menu_items), "activate", 
		GTK_SIGNAL_FUNC(menu_define_fill), point);
	gtk_menu_append(GTK_MENU(menu), menu_items);
	gtk_widget_show(menu_items);
	
	gtk_menu_popup (GTK_MENU(menu), NULL, NULL, (GtkMenuPositionFunc)(NULL), NULL, 0, 0);
}
