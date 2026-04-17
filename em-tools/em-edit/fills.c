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

#include <zlib.h>
#include <gtk/gtk.h>

#define USE_GDK_PIXBUF

#include "llist.h"
#include "vertex.h"
#include "polygon.h"
#include "inout.h"
#include "stringbuf.h"
#include "gsub.h"
#include "conns.h"
#include "bezier.h"
#include "nodes.h"
#include "curves.h"
#include "points.h"
#include "lines.h"
#include "fills.h"
#include "tiles.h"
#include "worker.h"
#include "interface.h"
#include "glade.h"
#include "worker.h"
#include "main.h"
#include "main_lock.h"
#include "bsp.h"
#include "map.h"


#define FILL_EDGE_DISTANCE 5.0



#define MINIMUM_QUAD_SIZE 1

#define MINIMUM_QUAD_SIZE_SQUARED (MINIMUM_QUAD_SIZE * MINIMUM_QUAD_SIZE)

#define QUAD_A 0
#define QUAD_B 1
#define QUAD_C 2
#define QUAD_D 3


struct fill_line_t
{
	double x1, y1, x2, y2;
	struct fill_line_t *next;
};





struct fill_edge_t *defining_fill = NULL;
	
struct point_t *defining_fill_end_point;

struct fill_t *fill0 = NULL;


int add_fill_pointer(struct fill_pointer_t **fillp0, struct fill_t *fill)
{
	if(!fillp0)
		return 0;

	if(!*fillp0)
	{
		*fillp0 = malloc(sizeof(struct fill_pointer_t));
		(*fillp0)->fill = fill;
		(*fillp0)->next = NULL;
	}
	else
	{
		struct fill_pointer_t *cfillp = *fillp0;

		while(cfillp->next)
		{
			if(cfillp->fill == fill)
				return 0;

			cfillp = cfillp->next;
		}

		if(cfillp->fill != fill)
		{
			cfillp->next = malloc(sizeof(struct fill_pointer_t));
			cfillp = cfillp->next;
			cfillp->fill = fill;
			cfillp->next = NULL;
		}
		else
		{
			return 0;
		}
	}

	return 1;
}


struct fill_t *new_fill()
{
	struct fill_t *fill;
	LL_CALLOC(struct fill_t, &fill0, &fill);
	return fill;
}


void invalidate_fill(struct fill_t *fill)
{
	remove_fill_from_tiles(fill);
	fill->texture_tiled = 0;
	
	
	struct texture_verts_t *st = fill->texture_verts0;

	while(st)
	{
		free_surface(st->surface);
		free(st->verts);
		st = st->next;
	}
	
	struct texture_polys_t *texture_polys = fill->texture_polys0;
	
	while(texture_polys)
	{
		int num = texture_polys->surface->width * texture_polys->surface->height;
		
		free_surface(texture_polys->surface);
		
		struct vertex_ll_t **vertex = texture_polys->polys;
		
		while(num)
		{
			LL_REMOVE_ALL(struct vertex_ll_t, vertex);
			vertex++;
			num--;
		}
			
		texture_polys = texture_polys->next;
	}
	
	LL_REMOVE_ALL(struct texture_verts_t, &fill->texture_verts0);
	LL_REMOVE_ALL(struct texture_polys_t, &fill->texture_polys0);
}


void invalidate_fills_with_point(struct point_t *point)
{
	struct fill_t *fill = fill0;
	
	while(fill)
	{
		struct fill_edge_t *fill_edge = fill->edge0;
		
		while(fill_edge)
		{
			if(fill_edge->point1 == point)
				break;
			
			fill_edge = fill_edge->next;
		}
		
		if(fill_edge)
			invalidate_fill(fill);
		
		fill = fill->next;
	}
}


void start_defining_fill(struct point_t *point)
{
	LL_REMOVE_ALL(struct fill_edge_t, &defining_fill);
	
	defining_fill_end_point = point;
}


void stop_defining_fill()
{
	LL_REMOVE_ALL(struct fill_edge_t, &defining_fill);
}


void make_sure_all_fills_are_clockwise()		// always called while not working
{
	struct fill_t *fill = fill0;
		
	while(fill)
	{
		double area = 0.0;
		
		struct fill_edge_t *fill_edge = fill->edge0;
		
		while(fill_edge)
		{
			area += fill_edge->point2->x * fill_edge->point1->y;
			area -= fill_edge->point1->x * fill_edge->point2->y;
			fill_edge = fill_edge->next;
		}
		
		if(area < 0.0)
		{
			struct fill_edge_t *temp = NULL;
			
			while(fill->edge0)
			{
				fill_edge = fill->edge0;
				
				while(fill_edge->next)
					fill_edge = fill_edge->next;
				
				struct point_t *point = fill_edge->point1;
				fill_edge->point1 = fill_edge->point2;
				fill_edge->point2 = point;
				
				LL_ADD(struct fill_edge_t, &temp, fill_edge);
				LL_REMOVE(struct fill_edge_t, &fill->edge0, fill_edge);
			}
			
			fill->edge0 = temp;
		}
			
		fill = fill->next;
	}
}


void add_point_to_fill(struct point_t *point)
{
	struct fill_edge_t fill_edge;
		
	fill_edge.point1 = defining_fill_end_point;
	fill_edge.point2 = point;
	
	
	// see if we have already made this fill_edge
	
	struct fill_edge_t *cfill_edge = defining_fill;
	int p = 0;
		
	while(cfill_edge)
	{
		if(cfill_edge->point1 == fill_edge.point1 || cfill_edge->point2 == fill_edge.point1)
			p |= 0x1;
			
		if(cfill_edge->point1 == fill_edge.point2 || cfill_edge->point2 == fill_edge.point2)
			p |= 0x2;
		
		if(p == 0x3)		
		{
			fill_edge.follow_curve = 0;
			break;
		}
		
		cfill_edge = cfill_edge->next;
	}
	
	if(!cfill_edge)
		fill_edge.follow_curve = 1;
	
	LL_ADD(struct fill_edge_t, &defining_fill, &fill_edge);
			
	defining_fill_end_point = point;
		
	
	// see if we completed the fill
	
	if(defining_fill)
	{
		if(defining_fill->point1 == defining_fill_end_point)
		{
			stop_working();
			
			struct fill_t *fill = new_fill();
				
			fill->type = FILL_TYPE_SOLID;
			fill->red = 0;
			fill->green = 197;
			fill->blue = 255;
			fill->alpha = 127;
			
			fill->stretch_horiz = 1.0;
			fill->stretch_vert = 1.0;
			fill->offset_horiz = 0.0;
			fill->offset_vert = 0.0;
			
			fill->friction = 0.0;
			
			fill->texture = NULL;
			
			fill->edge0 = defining_fill;
			defining_fill = NULL;
			make_sure_all_fills_are_clockwise();
			invalidate_bsp_tree();
			finished_defining_fill();
			
			start_working();
		}
	}
}


void draw_follow_straight_fill_left_edge_segment(struct conn_t *conn, double t1, double t2)
{
	struct node_t *node1, *node2;
	int left1, right1, left2, right2;
	double x1, y1, x2, y2;
	
	if(!conn->orientation)
	{
		node1 = conn->node1;
		node2 = conn->node2;
		x1 = conn->node1->effective_x[conn->sat1];
		y1 = conn->node1->effective_y[conn->sat1];
		x2 = conn->node2->effective_x[conn->sat2];
		y2 = conn->node2->effective_y[conn->sat2];
		get_width_sats(conn, &left1, &right1, &left2, &right2);
	}
	else
	{
		node1 = conn->node2;
		node2 = conn->node1;
		x1 = conn->node2->effective_x[conn->sat2];
		y1 = conn->node2->effective_y[conn->sat2];
		x2 = conn->node1->effective_x[conn->sat1];
		y2 = conn->node1->effective_y[conn->sat1];
		get_width_sats(conn, &left2, &right2, &left1, &right1);
	}
	
	
	double deltax = x2 - x1;
	double deltay = y2 - y1;
	
	double nx1 = x1 + deltax * t1;
	double ny1 = y1 + deltay * t1;
	double nx2 = x1 + deltax * t2;
	double ny2 = y1 + deltay * t2;
	
	double l = hypot(deltax, deltay);
	
	deltax /= l;
	deltay /= l;
	
	double width = 20.0;
	
	nx1 -= deltay * (width + FILL_EDGE_DISTANCE / zoom);
	ny1 += deltax * (width + FILL_EDGE_DISTANCE / zoom);
	nx2 -= deltay * (width + FILL_EDGE_DISTANCE / zoom);
	ny2 += deltax * (width + FILL_EDGE_DISTANCE / zoom);
	
	draw_world_clipped_line(nx1, ny1, nx2, ny2);
}


void draw_follow_straight_fill_right_edge_segment(struct conn_t *conn, double t1, double t2)
{
	struct node_t *node1, *node2;
	int left1, right1, left2, right2;
	double x1, y1, x2, y2;
	
	if(!conn->orientation)
	{
		node1 = conn->node1;
		node2 = conn->node2;
		x1 = conn->node1->effective_x[conn->sat1];
		y1 = conn->node1->effective_y[conn->sat1];
		x2 = conn->node2->effective_x[conn->sat2];
		y2 = conn->node2->effective_y[conn->sat2];
		get_width_sats(conn, &left1, &right1, &left2, &right2);
	}
	else
	{
		node1 = conn->node2;
		node2 = conn->node1;
		x1 = conn->node2->effective_x[conn->sat2];
		y1 = conn->node2->effective_y[conn->sat2];
		x2 = conn->node1->effective_x[conn->sat1];
		y2 = conn->node1->effective_y[conn->sat1];
		get_width_sats(conn, &left2, &right2, &left1, &right1);
	}
	
	
	double deltax = x2 - x1;
	double deltay = y2 - y1;
	
	double nx1 = x1 + deltax * t1;
	double ny1 = y1 + deltay * t1;
	double nx2 = x1 + deltax * t2;
	double ny2 = y1 + deltay * t2;
	
	double l = hypot(deltax, deltay);
	
	deltax /= l;
	deltay /= l;
	
	double width = 20.0;
	
	nx1 += deltay * (width + FILL_EDGE_DISTANCE / zoom);
	ny1 -= deltax * (width + FILL_EDGE_DISTANCE / zoom);
	nx2 += deltay * (width + FILL_EDGE_DISTANCE / zoom);
	ny2 -= deltax * (width + FILL_EDGE_DISTANCE / zoom);
	
	draw_world_clipped_line(nx1, ny1, nx2, ny2);
}


void draw_follow_curve_fill_left_edge_segment(struct conn_t *conn, double t1, double t2)
{
	struct bezier_t bezier;
	
	// must check orientation because t value is measured from beginning
	
	if(!conn->orientation)
	{
		bezier.x1 = conn->node1->effective_x[conn->sat1];
		bezier.y1 = conn->node1->effective_y[conn->sat1];
		bezier.x2 = conn->node1->effective_x[conn->sat1] + conn->node1->sats[conn->sat1].x;
		bezier.y2 = conn->node1->effective_y[conn->sat1] + conn->node1->sats[conn->sat1].y;
		bezier.x3 = conn->node2->effective_x[conn->sat2] + conn->node2->sats[conn->sat2].x;
		bezier.y3 = conn->node2->effective_y[conn->sat2] + conn->node2->sats[conn->sat2].y;
		bezier.x4 = conn->node2->effective_x[conn->sat2];
		bezier.y4 = conn->node2->effective_y[conn->sat2];
	}
	else
	{
		bezier.x1 = conn->node2->effective_x[conn->sat2];
		bezier.y1 = conn->node2->effective_y[conn->sat2];
		bezier.x2 = conn->node2->effective_x[conn->sat2] + conn->node2->sats[conn->sat2].x;
		bezier.y2 = conn->node2->effective_y[conn->sat2] + conn->node2->sats[conn->sat2].y;
		bezier.x3 = conn->node1->effective_x[conn->sat1] + conn->node1->sats[conn->sat1].x;
		bezier.y3 = conn->node1->effective_y[conn->sat1] + conn->node1->sats[conn->sat1].y;
		bezier.x4 = conn->node1->effective_x[conn->sat1];
		bezier.y4 = conn->node1->effective_y[conn->sat1];
	}
	
	float x, y, deltax, deltay;
	
	BRZ(&bezier, t1, &x, &y);
	deltaBRZ(&bezier, t1, &deltax, &deltay);
	
	double l = hypot(deltax, deltay);
	
	deltax /= l;
	deltay /= l;

	double width = 20.0;
	
	double x1 = x - deltay * (width + FILL_EDGE_DISTANCE / zoom);
	double y1 = y + deltax * (width + FILL_EDGE_DISTANCE / zoom);
	
	
	BRZ(&bezier, t2, &x, &y);
	deltaBRZ(&bezier, t2, &deltax, &deltay);
	
	l = hypot(deltax, deltay);
	
	deltax /= l;
	deltay /= l;

	double x2 = x - deltay * (width + FILL_EDGE_DISTANCE / zoom);
	double y2 = y + deltax * (width + FILL_EDGE_DISTANCE / zoom);
	
	int sx1, sy1, sx2, sy2;
	world_to_screen(x1, y1, &sx1, &sy1);
	world_to_screen(x2, y2, &sx2, &sy2);
	
	if((sx2 - sx1) * (sx2 - sx1) + (sy2 - sy1) * (sy2 - sy1) > MAX_FILL_EDGE_SEGMENT_SIZE_SQUARED)
	{
		double th = (t1 + t2) / 2.0;
		
		draw_follow_curve_fill_left_edge_segment(conn, t1, th);
		draw_follow_curve_fill_left_edge_segment(conn, th, t2);
	}
	else
	{
		draw_world_clipped_line(x1, y1, x2, y2);
	}
}


void draw_follow_curve_fill_right_edge_segment(struct conn_t *conn, double t1, double t2)
{
	struct bezier_t bezier;
		
	// must check orientation because t value is measured from beginning
	
	if(!conn->orientation)
	{
		bezier.x1 = conn->node1->effective_x[conn->sat1];
		bezier.y1 = conn->node1->effective_y[conn->sat1];
		bezier.x2 = conn->node1->effective_x[conn->sat1] + conn->node1->sats[conn->sat1].x;
		bezier.y2 = conn->node1->effective_y[conn->sat1] + conn->node1->sats[conn->sat1].y;
		bezier.x3 = conn->node2->effective_x[conn->sat2] + conn->node2->sats[conn->sat2].x;
		bezier.y3 = conn->node2->effective_y[conn->sat2] + conn->node2->sats[conn->sat2].y;
		bezier.x4 = conn->node2->effective_x[conn->sat2];
		bezier.y4 = conn->node2->effective_y[conn->sat2];
	}
	else
	{
		bezier.x1 = conn->node2->effective_x[conn->sat2];
		bezier.y1 = conn->node2->effective_y[conn->sat2];
		bezier.x2 = conn->node2->effective_x[conn->sat2] + conn->node2->sats[conn->sat2].x;
		bezier.y2 = conn->node2->effective_y[conn->sat2] + conn->node2->sats[conn->sat2].y;
		bezier.x3 = conn->node1->effective_x[conn->sat1] + conn->node1->sats[conn->sat1].x;
		bezier.y3 = conn->node1->effective_y[conn->sat1] + conn->node1->sats[conn->sat1].y;
		bezier.x4 = conn->node1->effective_x[conn->sat1];
		bezier.y4 = conn->node1->effective_y[conn->sat1];
	}
	
	float x, y, deltax, deltay;
	
	BRZ(&bezier, t1, &x, &y);
	deltaBRZ(&bezier, t1, &deltax, &deltay);
	
	double l = hypot(deltax, deltay);
	
	deltax /= l;
	deltay /= l;

	double width = 20.0;
	
	double x1 = x + deltay * (width + FILL_EDGE_DISTANCE / zoom);
	double y1 = y - deltax * (width + FILL_EDGE_DISTANCE / zoom);
	
	BRZ(&bezier, t2, &x, &y);
	deltaBRZ(&bezier, t2, &deltax, &deltay);
	
	l = hypot(deltax, deltay);
	
	deltax /= l;
	deltay /= l;

	double x2 = x + deltay * (width + FILL_EDGE_DISTANCE / zoom);
	double y2 = y - deltax * (width + FILL_EDGE_DISTANCE / zoom);
	
	int sx1, sy1, sx2, sy2;
	world_to_screen(x1, y1, &sx1, &sy1);
	world_to_screen(x2, y2, &sx2, &sy2);

	if((sx2 - sx1) * (sx2 - sx1) + (sy2 - sy1) * (sy2 - sy1) > MAX_FILL_EDGE_SEGMENT_SIZE_SQUARED)
	{
		double th = (t1 + t2) / 2.0;
		
		draw_follow_curve_fill_right_edge_segment(conn, t1, th);
		draw_follow_curve_fill_right_edge_segment(conn, th, t2);
	}
	else
	{
		draw_world_clipped_line(x1, y1, x2, y2);
	}
}


void draw_fill_edges(struct fill_edge_t *fill_edge0)
{
	struct fill_edge_t *fill_edge = fill_edge0, *last_fill_edge = fill_edge0, *next_fill_edge;

	while(last_fill_edge->next)
		last_fill_edge = last_fill_edge->next;
	
	while(fill_edge)
	{
		if(fill_edge->next)
			next_fill_edge = fill_edge->next;
		else
			next_fill_edge = fill_edge0;
		
		if(fill_edge->follow_curve && fill_edge->point1->curve == fill_edge->point2->curve)
		{
			struct curve_t *curve = fill_edge->point1->curve;
			
			// determine which conn comes first in linked list
			
			struct conn_pointer_t *cconnp = curve->connp0, *first_connp;
			struct conn_t *last_conn;
				
			double first_t, last_t;
			
			while(cconnp)
			{
				if(cconnp->conn == fill_edge->point1->conn)
				{
					first_connp = cconnp;
					last_conn = fill_edge->point2->conn;
					first_t = fill_edge->point1->t;
					last_t = fill_edge->point2->t;
					break;
				}
					
				if(cconnp->conn == fill_edge->point2->conn)
				{
					first_connp = cconnp;
					last_conn = fill_edge->point1->conn;
					first_t = fill_edge->point2->t;
					last_t = fill_edge->point1->t;
					break;
				}
				
				cconnp = cconnp->next;
			}
			
			assert(cconnp);

			
			// determine whether we are drawing on the left or right
			
			if(fill_edge->point2->pos < fill_edge->point1->pos)
			{
				if(first_connp->conn == last_conn)
				{
					switch(last_conn->type)
					{
					case CONN_TYPE_STRAIGHT:
						draw_follow_straight_fill_left_edge_segment(last_conn, 
							fill_edge->point1->t, fill_edge->point2->t);
						break;
					
					case CONN_TYPE_BEZIER:
						draw_follow_curve_fill_left_edge_segment(last_conn, 
							fill_edge->point1->t, fill_edge->point2->t);
						break;
					}
				}
				else
				{
					switch(first_connp->conn->type)
					{
					case CONN_TYPE_STRAIGHT:
						draw_follow_straight_fill_left_edge_segment(first_connp->conn, 
							first_t, 1.0);
						break;
					
					case CONN_TYPE_BEZIER:
						draw_follow_curve_fill_left_edge_segment(first_connp->conn, 
							first_t, 1.0);
						break;
					}
					
					cconnp = first_connp->next;
					
					while(cconnp->conn != last_conn)
					{
						switch(cconnp->conn->type)
						{
						case CONN_TYPE_STRAIGHT:
							draw_follow_straight_fill_left_edge_segment(cconnp->conn, 
								0.0, 1.0);
							break;
						
						case CONN_TYPE_BEZIER:
							draw_follow_curve_fill_left_edge_segment(cconnp->conn, 
								0.0, 1.0);
							break;
						}
						
						cconnp = cconnp->next;
					}

					switch(last_conn->type)
					{
					case CONN_TYPE_STRAIGHT:
						draw_follow_straight_fill_left_edge_segment(last_conn, 
							0.0, last_t);
						break;
					
					case CONN_TYPE_BEZIER:
						draw_follow_curve_fill_left_edge_segment(last_conn, 
							0.0, last_t);
						break;
					}
				}
			}
			else
			{
				if(first_connp->conn == last_conn)
				{
					switch(last_conn->type)
					{
					case CONN_TYPE_STRAIGHT:
						draw_follow_straight_fill_right_edge_segment(last_conn, 
							fill_edge->point1->t, fill_edge->point2->t);
						break;
					
					case CONN_TYPE_BEZIER:
						draw_follow_curve_fill_right_edge_segment(last_conn, 
							fill_edge->point1->t, fill_edge->point2->t);
						break;
					}
				}
				else
				{
					switch(first_connp->conn->type)
					{
					case CONN_TYPE_STRAIGHT:
						draw_follow_straight_fill_right_edge_segment(first_connp->conn, 
							first_t, 1.0);
						break;
					
					case CONN_TYPE_BEZIER:
						draw_follow_curve_fill_right_edge_segment(first_connp->conn, 
							first_t, 1.0);
						break;
					}
					
					cconnp = first_connp->next;
					
					while(cconnp->conn != last_conn)
					{
						switch(cconnp->conn->type)
						{
						case CONN_TYPE_STRAIGHT:
							draw_follow_straight_fill_right_edge_segment(cconnp->conn, 
								0.0, 1.0);
							break;
						
						case CONN_TYPE_BEZIER:
							draw_follow_curve_fill_right_edge_segment(cconnp->conn, 
								0.0, 1.0);
							break;
						}
						
						cconnp = cconnp->next;
					}

					switch(last_conn->type)
					{
					case CONN_TYPE_STRAIGHT:
						draw_follow_straight_fill_right_edge_segment(last_conn, 
							0.0, last_t);
						break;
					
					case CONN_TYPE_BEZIER:
						draw_follow_curve_fill_right_edge_segment(last_conn, 
							0.0, last_t);
						break;
					}
				}
			}
		}
		else		// not following curve
		{
			double x1, y1, x2, y2;
			
			if(last_fill_edge->point2->pos < last_fill_edge->point1->pos)
			{
				x1 = fill_edge->point1->x - 
					fill_edge->point1->deltay * (fill_edge->point1->left_width + 
					FILL_EDGE_DISTANCE / zoom);
				y1 = fill_edge->point1->y + 
					fill_edge->point1->deltax * (fill_edge->point1->left_width + 
					FILL_EDGE_DISTANCE / zoom);
			}
			else
			{
				x1 = fill_edge->point1->x + 
					fill_edge->point1->deltay * (fill_edge->point1->right_width + 
					FILL_EDGE_DISTANCE / zoom);
				y1 = fill_edge->point1->y - 
					fill_edge->point1->deltax * (fill_edge->point1->right_width + 
					FILL_EDGE_DISTANCE / zoom);
			}
					
			
			if(next_fill_edge->point2->pos < next_fill_edge->point1->pos)
			{
				x2 = fill_edge->point2->x - 
					fill_edge->point2->deltay * (fill_edge->point2->left_width + 
					FILL_EDGE_DISTANCE / zoom);
				y2 = fill_edge->point2->y + 
					fill_edge->point2->deltax * (fill_edge->point2->left_width + 
					FILL_EDGE_DISTANCE / zoom);
			}
			else
			{
				x2 = fill_edge->point2->x + 
					fill_edge->point2->deltay * (fill_edge->point2->right_width + 
					FILL_EDGE_DISTANCE / zoom);
				y2 = fill_edge->point2->y - 
					fill_edge->point2->deltax * (fill_edge->point2->right_width + 
					FILL_EDGE_DISTANCE / zoom);
			}
					
			draw_world_clipped_line(x1, y1, x2, y2);
		}
		
		last_fill_edge = fill_edge;
		fill_edge = fill_edge->next;
	}
}


void draw_fills()
{
	struct fill_t *fill = fill0;
		
	while(fill)
	{
		draw_fill_edges(fill->edge0);
		fill = fill->next;
	}
	
	if(defining_fill)
		draw_fill_edges(defining_fill);
}


void update_fill_surface(struct fill_t *fill)		// always called when not working
{
	if(!fill->texture_filename)
		return;
	
	if(!fill->texture_filename->text)
		return;
	
	if(!fill->pre_texture)
	{
		struct string_t *string = arb_rel2abs(fill->texture_filename->text, map_path->text);
		fill->pre_texture = read_gdk_pixbuf_surface_as_24bitalpha8bit(string->text);
		free_string(string);
	}
	
	if(!fill->pre_texture)
		return;

	free_surface(fill->texture);
	
	fill->texture = duplicate_surface(fill->pre_texture);
	
	if(fill->flip_horiz)
		surface_flip_horiz(fill->texture);
	
	if(fill->flip_vert)
		surface_flip_vert(fill->texture);

	if(fill->rotate_left)
		surface_rotate_left(fill->texture);
	
	if(fill->rotate_right)
		surface_rotate_right(fill->texture);
}


void on_fill_solid_colorpicker_color_set(GtkWidget *colorpicker, 
	guint red, guint green, guint blue, guint alpha, gpointer user_data)
{
	GtkWidget *dialog = gtk_widget_get_toplevel(GTK_WIDGET(colorpicker));
	struct fill_t *fill = g_object_get_data(G_OBJECT(dialog), "fill");
	
	stop_working();
	
	fill->red = red >> 8;
	fill->green = green >> 8;
	fill->blue = blue >> 8;
	fill->alpha = alpha >> 8;
	
	invalidate_fill(fill);
	update_client_area();
	start_working();
}


void on_fill_solid_radiobutton_toggled(GtkToggleButton *togglebutton, gpointer user_data)
{
	gboolean sensitive = gtk_toggle_button_get_active(togglebutton);
	GtkWidget *dialog = gtk_widget_get_toplevel(GTK_WIDGET(togglebutton));
	gtk_widget_set_sensitive(GTK_WIDGET(g_object_get_data(G_OBJECT(dialog), 
		"solid_hbox")), sensitive);
	
	struct fill_t *fill = g_object_get_data(G_OBJECT(dialog), "fill");
	
	stop_working();

	if(sensitive)
		fill->type = FILL_TYPE_SOLID;
	else
		fill->type = FILL_TYPE_TEXTURE;
	
	invalidate_fill(fill);
	update_client_area();
	start_working();
}


void on_fill_flip_horiz_checkbutton_toggled(GtkToggleButton *togglebutton, gpointer user_data)
{
	GtkWidget *dialog = gtk_widget_get_toplevel(GTK_WIDGET(togglebutton));
	struct fill_t *fill = g_object_get_data(G_OBJECT(dialog), "fill");

	stop_working();
	fill->flip_horiz = gtk_toggle_button_get_active(togglebutton);
	update_fill_surface(fill);
	invalidate_fill(fill);
	update_client_area();
	start_working();
}


void on_fill_flip_vert_checkbutton_toggled(GtkToggleButton *togglebutton, gpointer user_data)
{
	GtkWidget *dialog = gtk_widget_get_toplevel(GTK_WIDGET(togglebutton));
	struct fill_t *fill = g_object_get_data(G_OBJECT(dialog), "fill");

	stop_working();
	fill->flip_vert = gtk_toggle_button_get_active(togglebutton);
	update_fill_surface(fill);
	invalidate_fill(fill);
	update_client_area();
	start_working();
}


void on_fill_stretch_horiz_spinbutton_value_changed(GtkSpinButton *spinbutton, gpointer user_data)
{
	GtkWidget *dialog = gtk_widget_get_toplevel(GTK_WIDGET(spinbutton));
	struct fill_t *fill = g_object_get_data(G_OBJECT(dialog), "fill");

	stop_working();
	fill->stretch_horiz = gtk_spin_button_get_value(
		GTK_SPIN_BUTTON(g_object_get_data(G_OBJECT(dialog), "stretch_horiz_spinbutton")));
	invalidate_fill(fill);
	update_client_area();
	start_working();
}


void on_fill_stretch_vert_spinbutton_value_changed(GtkSpinButton *spinbutton, gpointer user_data)
{
	GtkWidget *dialog = gtk_widget_get_toplevel(GTK_WIDGET(spinbutton));
	struct fill_t *fill = g_object_get_data(G_OBJECT(dialog), "fill");

	stop_working();
	fill->stretch_vert = gtk_spin_button_get_value(
		GTK_SPIN_BUTTON(g_object_get_data(G_OBJECT(dialog), "stretch_vert_spinbutton")));
	invalidate_fill(fill);
	update_client_area();
	start_working();
}


void on_fill_offset_horiz_spinbutton_value_changed(GtkSpinButton *spinbutton, gpointer user_data)
{
	GtkWidget *dialog = gtk_widget_get_toplevel(GTK_WIDGET(spinbutton));
	struct fill_t *fill = g_object_get_data(G_OBJECT(dialog), "fill");

	stop_working();
	fill->offset_horiz = gtk_spin_button_get_value(
		GTK_SPIN_BUTTON(g_object_get_data(G_OBJECT(dialog), "offset_horiz_spinbutton")));
	invalidate_fill(fill);
	update_client_area();
	start_working();
}


void on_fill_offset_vert_spinbutton_value_changed(GtkSpinButton *spinbutton, gpointer user_data)
{
	GtkWidget *dialog = gtk_widget_get_toplevel(GTK_WIDGET(spinbutton));
	struct fill_t *fill = g_object_get_data(G_OBJECT(dialog), "fill");

	stop_working();
	fill->offset_vert = gtk_spin_button_get_value(
		GTK_SPIN_BUTTON(g_object_get_data(G_OBJECT(dialog), "offset_vert_spinbutton")));
	invalidate_fill(fill);
	update_client_area();
	start_working();
}


void on_fill_texture_radiobutton_toggled(GtkToggleButton *togglebutton, gpointer user_data)
{
	gboolean sensitive = gtk_toggle_button_get_active(togglebutton);
	GtkWidget *dialog = gtk_widget_get_toplevel(GTK_WIDGET(togglebutton));
	gtk_widget_set_sensitive(GTK_WIDGET(g_object_get_data(G_OBJECT(dialog), 
		"texture_vbox")), sensitive);

	struct fill_t *fill = g_object_get_data(G_OBJECT(dialog), "fill");
	
	stop_working();
	
	if(sensitive)
		fill->type = FILL_TYPE_TEXTURE;
	else
		fill->type = FILL_TYPE_SOLID;

	update_fill_surface(fill);
	invalidate_fill(fill);
	update_client_area();
	start_working();
}


void on_fill_texture_entry_changed(GtkEditable *editable, gpointer user_data)
{
	GtkWidget *dialog = gtk_widget_get_toplevel(GTK_WIDGET(editable));
	struct fill_t *fill = g_object_get_data(G_OBJECT(dialog), "fill");
		
	stop_working();
	
	free_string(fill->texture_filename);
	
	gchar *strval;
	g_object_get(G_OBJECT(editable), "text", &strval, NULL);
	
	fill->texture_filename = arb_abs2rel(strval, map_path->text);
	
	g_free(strval);
	
	free_surface(fill->pre_texture);
	fill->pre_texture = NULL;
	
	free_surface(fill->texture);
	fill->texture = NULL;
	
	update_fill_surface(fill);
	invalidate_fill(fill);
	
	update_client_area();
	
	start_working();
}


void on_fill_friction_spinbutton_value_changed(GtkSpinButton *spinbutton, gpointer user_data)
{
	GtkWidget *dialog = gtk_widget_get_toplevel(GTK_WIDGET(spinbutton));
	struct fill_t *fill = g_object_get_data(G_OBJECT(dialog), "fill");

	stop_working();
	fill->friction = gtk_spin_button_get_value(GTK_SPIN_BUTTON(g_object_get_data(G_OBJECT(dialog), 
		"friction_spinbutton")));
	start_working();
}


void on_fill_rotate_checkbutton_toggled(GtkToggleButton *togglebutton, gpointer user_data)
{
	gboolean sensitive = gtk_toggle_button_get_active(togglebutton);
	GtkWidget *dialog = gtk_widget_get_toplevel(GTK_WIDGET(togglebutton));
	gtk_widget_set_sensitive(GTK_WIDGET(g_object_get_data(G_OBJECT(dialog), 
		"rotate_hbox")), sensitive);
	
	struct fill_t *fill = g_object_get_data(G_OBJECT(dialog), "fill");
		
	stop_working();
	
	if(sensitive)
	{
		if(gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(g_object_get_data(G_OBJECT(dialog), 
			"rotate_left_radiobutton"))))
		{
			fill->rotate_left = 1;
			fill->rotate_right = 0;
		}
		else
		{
			fill->rotate_left = 0;
			fill->rotate_right = 1;
		}
	}
	else
	{
		fill->rotate_left = 0;
		fill->rotate_right = 0;
	}
	
	update_fill_surface(fill);
	invalidate_fill(fill);
	update_client_area();
	start_working();
}


void on_fill_rotate_left_radiobutton_toggled(GtkToggleButton *togglebutton, gpointer user_data)
{
	gboolean on = gtk_toggle_button_get_active(togglebutton);
	GtkWidget *dialog = gtk_widget_get_toplevel(GTK_WIDGET(togglebutton));
	struct fill_t *fill = g_object_get_data(G_OBJECT(dialog), "fill");
	
	if(on)
	{
		stop_working();
		fill->rotate_left = 1;
		fill->rotate_right = 0;
		update_fill_surface(fill);
		invalidate_fill(fill);
		update_client_area();
		start_working();
	}
}


void on_fill_rotate_right_radiobutton_toggled(GtkToggleButton *togglebutton, gpointer user_data)
{
	gboolean on = gtk_toggle_button_get_active(togglebutton);
	GtkWidget *dialog = gtk_widget_get_toplevel(GTK_WIDGET(togglebutton));
	struct fill_t *fill = g_object_get_data(G_OBJECT(dialog), "fill");
	
	if(on)
	{
		stop_working();
		fill->rotate_left = 0;
		fill->rotate_right = 1;
		update_fill_surface(fill);
		invalidate_fill(fill);
		update_client_area();
		start_working();
	}
}


gboolean on_fill_stretch_horiz_spinbutton_button_press_event(GtkWidget *widget, 
	GdkEventButton *event, gpointer user_data)
{
	return FALSE;
}


gboolean on_fill_stretch_horiz_spinbutton_button_release_event(GtkWidget *widget, 
	GdkEventButton *event, gpointer user_data)
{
	return FALSE;
}


gboolean on_fill_stretch_vert_spinbutton_button_press_event(GtkWidget *widget, 
	GdkEventButton *event, gpointer user_data)
{
	return FALSE;
}


gboolean on_fill_stretch_vert_spinbutton_button_release_event(GtkWidget *widget, 
	GdkEventButton *event, gpointer user_data)
{
	return FALSE;
}


gboolean on_fill_offset_horiz_spinbutton_button_press_event(GtkWidget *widget, 
	GdkEventButton *event, gpointer user_data)
{
	return FALSE;
}


gboolean on_fill_offset_horiz_spinbutton_button_release_event(GtkWidget *widget, 
	GdkEventButton *event, gpointer user_data)
{
	return FALSE;
}


gboolean on_fill_offset_vert_spinbutton_button_press_event(GtkWidget *widget, 
	GdkEventButton *event, gpointer user_data)
{
	return FALSE;
}


gboolean on_fill_offset_vert_spinbutton_button_release_event(GtkWidget *widget, 
	GdkEventButton *event, gpointer user_data)
{
	return FALSE;
}


void on_fill_properties_dialog_destroy(GtkObject *object, gpointer user_data)
{
	gtk_main_quit();
}


void run_fill_properties_dialog(void *menu, struct fill_t *fill)
{
	GtkWidget *dialog = create_fill_properties_dialog();
	
	g_object_set_data(G_OBJECT(dialog), "fill", fill);
	
	gtk_window_set_transient_for(GTK_WINDOW(dialog), GTK_WINDOW(window));
	gtk_window_set_position(GTK_WINDOW(dialog), GTK_WIN_POS_CENTER_ON_PARENT);
	
	if(fill->type == FILL_TYPE_SOLID)
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(g_object_get_data(G_OBJECT(dialog), 
			"solid_radiobutton")), TRUE);
	else
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(g_object_get_data(G_OBJECT(dialog), 
			"texture_radiobutton")), TRUE);

	{ GdkColor _c = {0, fill->red<<8, fill->green<<8, fill->blue<<8}; gtk_color_button_set_color(GTK_COLOR_BUTTON(g_object_get_data(G_OBJECT(dialog), 
		"solid_colorpicker")), &_c); gtk_color_button_set_alpha(GTK_COLOR_BUTTON(g_object_get_data(G_OBJECT(dialog), 
		"solid_colorpicker")), fill->alpha<<8); }
	
	if(fill->texture_filename)
	{
		gtk_entry_set_text(GTK_ENTRY(g_object_get_data(G_OBJECT(dialog), 
			"texture_entry")), fill->texture_filename->text);
	}
	
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(g_object_get_data(G_OBJECT(dialog), 
		"flip_horiz_checkbutton")), fill->flip_horiz);
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(g_object_get_data(G_OBJECT(dialog), 
		"flip_vert_checkbutton")), fill->flip_vert);
	
	if(fill->rotate_left)
	{
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(g_object_get_data(G_OBJECT(dialog), 
			"rotate_checkbutton")), 1);
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(g_object_get_data(G_OBJECT(dialog), 
			"rotate_left_radiobutton")), 1);
	}

	if(fill->rotate_right)
	{
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(g_object_get_data(G_OBJECT(dialog), 
			"rotate_checkbutton")), 1);
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(g_object_get_data(G_OBJECT(dialog), 
			"rotate_right_radiobutton")), 1);
	}

	gtk_spin_button_set_value(GTK_SPIN_BUTTON(g_object_get_data(G_OBJECT(dialog), 
		"stretch_horiz_spinbutton")), (gfloat)fill->stretch_horiz);
	gtk_spin_button_set_value(GTK_SPIN_BUTTON(g_object_get_data(G_OBJECT(dialog), 
		"stretch_vert_spinbutton")), (gfloat)fill->stretch_vert);
	gtk_spin_button_set_value(GTK_SPIN_BUTTON(g_object_get_data(G_OBJECT(dialog), 
		"offset_horiz_spinbutton")), (gfloat)fill->offset_horiz);
	gtk_spin_button_set_value(GTK_SPIN_BUTTON(g_object_get_data(G_OBJECT(dialog), 
		"offset_vert_spinbutton")), (gfloat)fill->offset_vert);
	gtk_spin_button_set_value(GTK_SPIN_BUTTON(g_object_get_data(G_OBJECT(dialog), 
		"friction_spinbutton")), (gfloat)fill->friction);
	
	gtk_widget_show(dialog);
	gtk_main();
}


int check_for_unverticied_fills()
{
	struct fill_t *cfill = fill0;
	
	while(cfill)
	{
		switch(cfill->type)
		{
		case FILL_TYPE_SOLID:
			if(!cfill->texture_verts0 && !cfill->texture_polys0)
				return 1;
			break;
		
		case FILL_TYPE_TEXTURE:
			if(cfill->texture && !cfill->texture_verts0 && !cfill->texture_polys0)
				return 1;
			break;
		}
		
		cfill = cfill->next;
	}
	
	return 0;
}


int check_for_untiled_fills()
{
	struct fill_t *cfill = fill0;
	
	while(cfill)
	{
		switch(cfill->type)
		{
		case FILL_TYPE_SOLID:
			if((cfill->texture_verts0 || cfill->texture_polys0) && !cfill->texture_tiled)
				return 1;
			break;
		
		case FILL_TYPE_TEXTURE:
			if(cfill->texture && (cfill->texture_verts0 || cfill->texture_polys0) && 
				!cfill->texture_tiled)
				return 1;
			break;
		}
		
		cfill = cfill->next;
	}
	
	return 0;
}


void finished_tiling_all_fills()
{
	struct fill_t *cfill = fill0;
	
	while(cfill)
	{
		if((cfill->texture_verts0 || cfill->texture_polys0) && !cfill->texture_tiled)
			cfill->texture_tiled = 1;
		
		cfill = cfill->next;
	}
}


uint32_t count_fills()
{
	uint32_t num_fills = 0;
	struct fill_t *cfill = fill0;

	while(cfill)
	{
		num_fills++;
		cfill = cfill->next;
	}

	return num_fills;
}


uint32_t count_fill_edges(struct fill_edge_t *fill_edge0)
{
	uint32_t num_fill_edges = 0;
	struct fill_edge_t *cfill_edge = fill_edge0;

	while(cfill_edge)
	{
		num_fill_edges++;
		cfill_edge = cfill_edge->next;
	}

	return num_fill_edges;
}


int gzread_fills(gzFile file)
{
	uint32_t num_fills;
	if(gzread(file, &num_fills, 4) != 4)
		goto error;

	while(num_fills)
	{
		struct fill_t *cfill = new_fill();

		if(gzread(file, &cfill->type, 1) != 1)
			goto error;

		if(gzread(file, &cfill->red, 1) != 1)
			goto error;

		if(gzread(file, &cfill->green, 1) != 1)
			goto error;

		if(gzread(file, &cfill->blue, 1) != 1)
			goto error;

		if(gzread(file, &cfill->alpha, 1) != 1)
			goto error;

		cfill->texture_filename = gzread_string(file);
		if(!cfill->texture_filename)
			goto error;
		
		if(gzread(file, &cfill->flip_horiz, 1) != 1)
			goto error;

		if(gzread(file, &cfill->flip_vert, 1) != 1)
			goto error;

		if(gzread(file, &cfill->rotate_left, 1) != 1)
			goto error;

		if(gzread(file, &cfill->rotate_right, 1) != 1)
			goto error;


		if(gzread(file, &cfill->stretch_horiz, 4) != 4)
			goto error;

		if(gzread(file, &cfill->stretch_vert, 4) != 4)
			goto error;

		if(gzread(file, &cfill->offset_horiz, 4) != 4)
			goto error;

		if(gzread(file, &cfill->offset_vert, 4) != 4)
			goto error;

		if(gzread(file, &cfill->friction, 4) != 4)
			goto error;

		
		uint32_t num_fill_edges;
		if(gzread(file, &num_fill_edges, 4) != 4)
			goto error;

		while(num_fill_edges)
		{
			struct fill_edge_t fill_edge;
	
			uint32_t point_index;
			if(gzread(file, &point_index, 4) != 4)
				goto error;
	
			fill_edge.point1 = get_point_from_index(point_index);
			if(!fill_edge.point1)
				goto error;
			
			if(gzread(file, &point_index, 4) != 4)
				goto error;
	
			fill_edge.point2 = get_point_from_index(point_index);
			if(!fill_edge.point2)
				goto error;

			if(gzread(file, &fill_edge.follow_curve, 4) != 4)
				goto error;
				
			LL_ADD(struct fill_edge_t, &cfill->edge0, &fill_edge);
			
			num_fill_edges--;
		}
		
		update_fill_surface(cfill);

		num_fills--;
	}
	
	return 1;

error:

	return 0;
}


void gzwrite_fills(gzFile file)
{
	uint32_t num_fills = count_fills();
	gzwrite(file, &num_fills, 4);

	struct fill_t *cfill = fill0;

	while(cfill)
	{
		gzwrite(file, &cfill->type, 1);
		gzwrite(file, &cfill->red, 1);
		gzwrite(file, &cfill->green, 1);
		gzwrite(file, &cfill->blue, 1);
		gzwrite(file, &cfill->alpha, 1);
		
		gzwrite_string(file, cfill->texture_filename);
		
		gzwrite(file, &cfill->flip_horiz, 1);
		gzwrite(file, &cfill->flip_vert, 1);
		gzwrite(file, &cfill->rotate_left, 1);
		gzwrite(file, &cfill->rotate_right, 1);
		
		gzwrite(file, &cfill->stretch_horiz, 4);
		gzwrite(file, &cfill->stretch_vert, 4);
		gzwrite(file, &cfill->offset_horiz, 4);
		gzwrite(file, &cfill->offset_vert, 4);

		gzwrite(file, &cfill->friction, 4);
		
		uint32_t num_fill_edges = count_fill_edges(cfill->edge0);
		gzwrite(file, &num_fill_edges, 4);

		struct fill_edge_t *cfill_edge = cfill->edge0;
	
		while(cfill_edge)
		{
			gzwrite(file, &cfill_edge->point1->index, 4);
			gzwrite(file, &cfill_edge->point2->index, 4);
			gzwrite(file, &cfill_edge->follow_curve, 4);
		
			cfill_edge = cfill_edge->next;
		}
		
		cfill = cfill->next;
	}
}


void delete_all_fills()		// always called when not working
{
	struct fill_t *fill = fill0;
		
	while(fill)
	{
		free_string(fill->texture_filename);
		free_surface(fill->pre_texture);
		free_surface(fill->texture);
		LL_REMOVE_ALL(struct fill_edge_t, &fill->edge0);
		LL_REMOVE_ALL(struct texture_verts_t, &fill->texture_verts0);
		LL_REMOVE_ALL(struct texture_polys_t, &fill->texture_polys0);
			
		fill = fill->next;
	}
	
	LL_REMOVE_ALL(struct fill_t, &fill0);
}
	

//
// WORKER THREAD FUNCTIONS
//


void generate_quad_verticies(struct fill_t *fill, 
	double minx, double miny, double maxx, double maxy)
{
//	maxx -= fill->stretch_horiz;
//	maxy -= fill->stretch_vert;
	
	struct texture_verts_t texture_verts;
	
	int min_pixelx = (int)floor((minx - fill->offset_horiz) / fill->stretch_horiz);
	int min_pixely = (int)floor((miny - fill->offset_vert) / fill->stretch_vert);
	int max_pixelx = (int)floor((maxx - fill->offset_horiz) / fill->stretch_horiz);
	int max_pixely = (int)floor((maxy - fill->offset_vert) / fill->stretch_vert);
	int width = max_pixelx - min_pixelx + 1;
	int height = max_pixely - min_pixely + 1;
	
	texture_verts.surface = new_surface(SURFACE_FLOATSALPHAFLOATS, width, height);
	texture_verts.verts = malloc((width + 1) * (height + 1) * sizeof(struct vertex_t));
	
	int xpos, ypos;
	
	float *dst = (float*)texture_verts.surface->buf;
	float *alpha_dst = (float*)texture_verts.surface->alpha_buf;
	
	
	struct vertex_t *cvert = texture_verts.verts;
	
	// do top left vert
	
	cvert->x = minx;
	cvert->y = maxy;
	cvert++;
	
	
	// do top row of verts
	
	for(xpos = 1; xpos < width; xpos++)
	{
		cvert->x = (min_pixelx + xpos) * fill->stretch_horiz + fill->offset_horiz;
		cvert->y = maxy;
		cvert++;
	}
	
	
	// do top right vert
	
	cvert->x = maxx;
	cvert->y = maxy;
	cvert++;
	
	
	// the middle of the verts

	for(ypos = height - 1; ypos > 0; ypos--)
	{
		// do leftmost vert
		
		cvert->x = minx;
		cvert->y = (min_pixely + ypos) * fill->stretch_vert + fill->offset_vert;
		cvert++;
		
		
		for(xpos = 1; xpos < width; xpos++)
		{
			cvert->x = (min_pixelx + xpos) * fill->stretch_horiz + fill->offset_horiz;
			cvert->y = (min_pixely + ypos) * fill->stretch_vert + fill->offset_vert;
			cvert++;
		}

		
		// do rightmost vert
		
		cvert->x = maxx;
		cvert->y = (min_pixely + ypos) * fill->stretch_vert + fill->offset_vert;
		cvert++;
	}
	
	
	// do bottom left vert
	
	cvert->x = minx;
	cvert->y = miny;
	cvert++;
	
	
	// do bottom row of verts
	
	for(xpos = 1; xpos < width; xpos++)
	{
		cvert->x = (min_pixelx + xpos) * fill->stretch_horiz + fill->offset_horiz;
		cvert->y = miny;
		cvert++;
	}
	
	
	// do bottom right vert
	
	cvert->x = maxx;
	cvert->y = miny;
	cvert++;
	
	
	switch(fill->type)
	{
	case FILL_TYPE_SOLID:

	for(ypos = height - 1; ypos >= 0; ypos--)
		{
			for(xpos = 0; xpos < width; xpos++)
			{
				*dst++ = fill->red / 255.0;
				*dst++ = fill->green / 255.0;
				*dst++ = fill->blue / 255.0;
				*alpha_dst++ = fill->alpha / 255.0;
			}
		}

		break;
	
	case FILL_TYPE_TEXTURE:
			
		for(ypos = height - 1; ypos >= 0; ypos--)
		{
			for(xpos = 0; xpos < width; xpos++)
			{
				int ny = min_pixely + ypos;
				int nx = min_pixelx + xpos;
				
				while(ny < 0)
					ny += fill->texture->height;
				
				while(nx < 0)
					nx += fill->texture->width;
				
				int offset = (fill->texture->height - 1 - 
					(ny % fill->texture->height)) * 
					fill->texture->width + 
					nx % fill->texture->width;
				
				*dst++ = ((uint8_t*)fill->texture->buf)[offset * 3] / 255.0;
				*dst++ = ((uint8_t*)fill->texture->buf)[offset * 3 + 1] / 255.0;
				*dst++ = ((uint8_t*)fill->texture->buf)[offset * 3 + 2] / 255.0;
				*alpha_dst++ = ((uint8_t*)fill->texture->alpha_buf)[offset] / 255.0;
			}
		}
		
		break;
	}
	
	LL_ADD(struct texture_verts_t, &fill->texture_verts0, &texture_verts);
}	


int lines_intersect(double ax1, double ay1, double ax2, double ay2, 
	double bx1, double by1, double bx2, double by2)
{
	double nx = ay1 - ay2;
	double ny = -(ax1 - ax2);
	double denom = (-nx) * (bx2 - bx1) + (-ny) * (by2 - by1);
	
	if(denom != 0.0)
	{
		double numer = nx * (bx1 - ax1) + ny * (by1 - ay1);
		double t = numer / denom;
		
		if((t >= 0.0) && (t < 1.0))
		{
			nx = by1 - by2;
			ny = -(bx1 - bx2);
			denom = (-nx) * (ax2 - ax1) + (-ny) * (ay2 - ay1);
			
			if(denom != 0.0)
			{
				numer = nx * (ax1 - bx1) + ny * (ay1 - by1);
				t = numer / denom;
				
				if((t >= 0.0) && (t < 1.0))
					return 1;
			}
		}
	}
	
	return 0;
}


int process_quad(struct fill_t *fill, double minx, double miny, double maxx, double maxy, 
	int center_inside, struct fill_line_t *fill_line0)
{
	if(in_lock_check_stop_callback())
		return 0;
	
	if((maxx - minx) * (maxy - miny) < MINIMUM_QUAD_SIZE_SQUARED)
	{
		// generate a polygon for each pixel
		
		struct texture_polys_t texture_polys;
		
		int min_pixelx = (int)floor((minx - fill->offset_horiz) / fill->stretch_horiz);
		int min_pixely = (int)floor((miny - fill->offset_vert) / fill->stretch_vert);
		int max_pixelx = (int)floor((maxx - fill->offset_horiz) / fill->stretch_horiz);
		int max_pixely = (int)floor((maxy - fill->offset_vert) / fill->stretch_vert);
		int width = max_pixelx - min_pixelx + 1;
		int height = max_pixely - min_pixely + 1;
		
		texture_polys.surface = new_surface(SURFACE_FLOATSALPHAFLOATS, width, height);
		texture_polys.polys = calloc(width * height, sizeof(struct vertex_ll_t*));
		
		struct vertex_t bottom_left = {minx, miny};
		struct vertex_t top_left = {minx, maxy};
		struct vertex_t top_right = {maxx, maxy};
		struct vertex_t bottom_right = {maxx, miny};
		
		int xpos, ypos;
		
		float *dst = (float*)texture_polys.surface->buf;
		float *alpha_dst = (float*)texture_polys.surface->alpha_buf;
		
		struct vertex_ll_t **poly = texture_polys.polys;
		
		for(ypos = height - 1; ypos >= 0; ypos--)
		{
			for(xpos = 0; xpos < width; xpos++)
			{
				switch(fill->type)
				{
				case FILL_TYPE_SOLID:
			
					*dst++ = fill->red / 255.0;
					*dst++ = fill->green / 255.0;
					*dst++ = fill->blue / 255.0;
					*alpha_dst++ = fill->alpha / 255.0;
			
					break;
				
				case FILL_TYPE_TEXTURE:
				{
					int ny = min_pixely + ypos;
					int nx = min_pixelx + xpos;
					
					while(ny < 0)
						ny += fill->texture->height;
					
					while(nx < 0)
						nx += fill->texture->width;
				
					int offset = (fill->texture->height - 1 - 
						(ny % fill->texture->height)) * 
						fill->texture->width + 
						nx % fill->texture->width;
					
					*dst++ = ((uint8_t*)fill->texture->buf)[offset * 3] / 255.0;
					*dst++ = ((uint8_t*)fill->texture->buf)[offset * 3 + 1] / 255.0;
					*dst++ = ((uint8_t*)fill->texture->buf)[offset * 3 + 2] / 255.0;
					*alpha_dst++ = ((uint8_t*)fill->texture->alpha_buf)[offset] / 255.0;
				}
				
					break;
				}
				
				
				struct vertex_ll_t vertex;
				
				vertex.x = (min_pixelx + xpos) * fill->stretch_horiz + fill->offset_horiz;
				vertex.y = (min_pixely + ypos) * fill->stretch_vert + fill->offset_vert;
				LL_ADD(struct vertex_ll_t, poly, &vertex);

				vertex.x = (min_pixelx + xpos) * fill->stretch_horiz + fill->offset_horiz;
				vertex.y = (min_pixely + ypos + 1) * fill->stretch_vert + fill->offset_vert;
				LL_ADD(struct vertex_ll_t, poly, &vertex);

				vertex.x = (min_pixelx + xpos + 1) * fill->stretch_horiz + fill->offset_horiz;
				vertex.y = (min_pixely + ypos + 1) * fill->stretch_vert + fill->offset_vert;
				LL_ADD(struct vertex_ll_t, poly, &vertex);

				vertex.x = (min_pixelx + xpos + 1) * fill->stretch_horiz + fill->offset_horiz;
				vertex.y = (min_pixely + ypos) * fill->stretch_vert + fill->offset_vert;
				LL_ADD(struct vertex_ll_t, poly, &vertex);
					
				if(xpos == 0)
					poly_arb_line_clip(poly, &bottom_left, &top_left);
				
				if(ypos == height - 1)
					poly_arb_line_clip(poly, &top_left, &top_right);
				
				if(xpos == width - 1)
					poly_arb_line_clip(poly, &top_right, &bottom_right);

				if(ypos == 0)
					poly_arb_line_clip(poly, &bottom_right, &bottom_left);
				
				struct fill_line_t *cfill_line = fill_line0;
					
				while(cfill_line)
				{
					struct vertex_t v1 = {cfill_line->x1, cfill_line->y1};
					struct vertex_t v2 = {cfill_line->x2, cfill_line->y2};
					
					poly_arb_line_clip(poly, &v1, &v2);
					
					if(*poly == NULL)
						break;
					
					cfill_line = cfill_line->next;
				}
				
				poly++;
			}
		}
	
		LL_ADD(struct texture_polys_t, &fill->texture_polys0, &texture_polys);
	}
	else
	{
		// split in four
		
		double midx = (maxx + minx) / 2.0;
		double midy = (maxy + miny) / 2.0;
		
		struct fill_line_t *fill_line0A = NULL;
		struct fill_line_t *fill_line0B = NULL;
		struct fill_line_t *fill_line0C = NULL;
		struct fill_line_t *fill_line0D = NULL;
			
		struct fill_line_t *cfill_line = fill_line0;
			
		while(cfill_line)
		{
			// work out which quadrant each line end is in
			
			int quad1, quad2;
			
			if(cfill_line->x1 < midx)
			{
				if(cfill_line->y1 < midy)
					quad1 = QUAD_C;
				else
					quad1 = QUAD_A;
			}
			else
			{
				if(cfill_line->y1 < midy)
					quad1 = QUAD_D;
				else
					quad1 = QUAD_B;
			}
			
			if(cfill_line->x2 < midx)
			{
				if(cfill_line->y2 < midy)
					quad2 = QUAD_C;
				else
					quad2 = QUAD_A;
			}
			else
			{
				if(cfill_line->y2 < midy)
					quad2 = QUAD_D;
				else
					quad2 = QUAD_B;
			}
			
			
			// put line in correct list, dividing if necessary
			
			struct fill_line_t fill_line;
			
			switch(quad1)
			{
			case QUAD_A:
				
				switch(quad2)
				{
				case QUAD_A:
					LL_ADD(struct fill_line_t, &fill_line0A, cfill_line);
					break;
				
				case QUAD_B:
					fill_line.x1 = cfill_line->x1;
					fill_line.y1 = cfill_line->y1;
					fill_line.x2 = midx;
					fill_line.y2 = cfill_line->y1 + 
						(cfill_line->y2 - cfill_line->y1) * (midx - cfill_line->x1) /
						(cfill_line->x2 - cfill_line->x1);
					LL_ADD(struct fill_line_t, &fill_line0A, &fill_line);
						
					fill_line.x1 = midx;
					fill_line.y1 = fill_line.y2;
					fill_line.x2 = cfill_line->x2;
					fill_line.y2 = cfill_line->y2;
					LL_ADD(struct fill_line_t, &fill_line0B, &fill_line);
					break;
				
				case QUAD_C:
					fill_line.x1 = cfill_line->x1;
					fill_line.y1 = cfill_line->y1;
					fill_line.x2 = cfill_line->x1 + 
						(cfill_line->x2 - cfill_line->x1) * (midy - cfill_line->y1) /
						(cfill_line->y2 - cfill_line->y1);
					fill_line.y2 = midy;
					LL_ADD(struct fill_line_t, &fill_line0A, &fill_line);
						
					fill_line.x1 = fill_line.x2;
					fill_line.y1 = midy;
					fill_line.x2 = cfill_line->x2;
					fill_line.y2 = cfill_line->y2;
					LL_ADD(struct fill_line_t, &fill_line0C, &fill_line);
					break;

				case QUAD_D:
					// see if it goes through C or B

					if(cfill_line->y1 + 
						(cfill_line->y2 - cfill_line->y1) * (midx - cfill_line->x1) /
						(cfill_line->x2 - cfill_line->x1) < midy)
					{
						// goes through C
						fill_line.x1 = cfill_line->x1;
						fill_line.y1 = cfill_line->y1;
						fill_line.x2 = cfill_line->x1 + 
							(cfill_line->x2 - cfill_line->x1) * (midy - cfill_line->y1) /
							(cfill_line->y2 - cfill_line->y1);
						fill_line.y2 = midy;
						LL_ADD(struct fill_line_t, &fill_line0A, &fill_line);
						
						fill_line.x1 = fill_line.x2;
						fill_line.y1 = midy;
						fill_line.x2 = midx;
						fill_line.y2 = cfill_line->y1 + 
							(cfill_line->y2 - cfill_line->y1) * (midx - cfill_line->x1) /
							(cfill_line->x2 - cfill_line->x1);
						LL_ADD(struct fill_line_t, &fill_line0C, &fill_line);
							
						fill_line.x1 = midx;
						fill_line.y1 = fill_line.y2;
						fill_line.x2 = cfill_line->x2;
						fill_line.y2 = cfill_line->y2;
						LL_ADD(struct fill_line_t, &fill_line0D, &fill_line);
					}
					else
					{
						// goes through B
						fill_line.x1 = cfill_line->x1;
						fill_line.y1 = cfill_line->y1;
						fill_line.x2 = midx;
						fill_line.y2 = cfill_line->y1 + 
							(cfill_line->y2 - cfill_line->y1) * (midx - cfill_line->x1) /
							(cfill_line->x2 - cfill_line->x1);
						LL_ADD(struct fill_line_t, &fill_line0A, &fill_line);
						
						fill_line.x1 = midx;
						fill_line.y1 = fill_line.y2;
						fill_line.x2 = cfill_line->x1 + 
							(cfill_line->x2 - cfill_line->x1) * (midy - cfill_line->y1) /
							(cfill_line->y2 - cfill_line->y1);
						fill_line.y2 = midy;
						LL_ADD(struct fill_line_t, &fill_line0B, &fill_line);
							
						fill_line.x1 = fill_line.x2;
						fill_line.y1 = midy;
						fill_line.x2 = cfill_line->x2;
						fill_line.y2 = cfill_line->y2;
						LL_ADD(struct fill_line_t, &fill_line0D, &fill_line);
					}
					
					break;
				}
				
				break;
			
			case QUAD_B:
				
				switch(quad2)
				{
				case QUAD_A:
					fill_line.x1 = cfill_line->x1;
					fill_line.y1 = cfill_line->y1;
					fill_line.x2 = midx;
					fill_line.y2 = cfill_line->y1 + 
						(cfill_line->y2 - cfill_line->y1) * (midx - cfill_line->x1) /
						(cfill_line->x2 - cfill_line->x1);
					LL_ADD(struct fill_line_t, &fill_line0B, &fill_line);
						
					fill_line.x1 = midx;
					fill_line.y1 = fill_line.y2;
					fill_line.x2 = cfill_line->x2;
					fill_line.y2 = cfill_line->y2;
					LL_ADD(struct fill_line_t, &fill_line0A, &fill_line);
					break;
				
				
				case QUAD_B:
					LL_ADD(struct fill_line_t, &fill_line0B, cfill_line);
					break;
				
				case QUAD_C:
					// see if it goes through D or A
				
					if(cfill_line->y1 + 
						(cfill_line->y2 - cfill_line->y1) * (midx - cfill_line->x1) /
						(cfill_line->x2 - cfill_line->x1) < midy)
					{
						// goes through D
						fill_line.x1 = cfill_line->x1;
						fill_line.y1 = cfill_line->y1;
						fill_line.x2 = cfill_line->x1 + 
							(cfill_line->x2 - cfill_line->x1) * (midy - cfill_line->y1) /
							(cfill_line->y2 - cfill_line->y1);
						fill_line.y2 = midy;
						LL_ADD(struct fill_line_t, &fill_line0B, &fill_line);
						
						fill_line.x1 = fill_line.x2;
						fill_line.y1 = midy;
						fill_line.x2 = midx;
						fill_line.y2 = cfill_line->y1 + 
							(cfill_line->y2 - cfill_line->y1) * (midx - cfill_line->x1) /
							(cfill_line->x2 - cfill_line->x1);
						LL_ADD(struct fill_line_t, &fill_line0D, &fill_line);
							
						fill_line.x1 = midx;
						fill_line.y1 = fill_line.y2;
						fill_line.x2 = cfill_line->x2;
						fill_line.y2 = cfill_line->y2;
						LL_ADD(struct fill_line_t, &fill_line0C, &fill_line);
					}
					else
					{
						// goes through A
						fill_line.x1 = cfill_line->x1;
						fill_line.y1 = cfill_line->y1;
						fill_line.x2 = midx;
						fill_line.y2 = cfill_line->y1 + 
							(cfill_line->y2 - cfill_line->y1) * (midx - cfill_line->x1) /
							(cfill_line->x2 - cfill_line->x1);
						LL_ADD(struct fill_line_t, &fill_line0B, &fill_line);
						
						fill_line.x1 = midx;
						fill_line.y1 = fill_line.y2;
						fill_line.x2 = cfill_line->x1 + 
							(cfill_line->x2 - cfill_line->x1) * (midy - cfill_line->y1) /
							(cfill_line->y2 - cfill_line->y1);
						fill_line.y2 = midy;
						LL_ADD(struct fill_line_t, &fill_line0A, &fill_line);
							
						fill_line.x1 = fill_line.x2;
						fill_line.y1 = midy;
						fill_line.x2 = cfill_line->x2;
						fill_line.y2 = cfill_line->y2;
						LL_ADD(struct fill_line_t, &fill_line0C, &fill_line);
					}
				
					break;
			
				case QUAD_D:
					fill_line.x1 = cfill_line->x1;
					fill_line.y1 = cfill_line->y1;
					fill_line.x2 = cfill_line->x1 + 
						(cfill_line->x2 - cfill_line->x1) * (midy - cfill_line->y1) /
						(cfill_line->y2 - cfill_line->y1);
					fill_line.y2 = midy;
					LL_ADD(struct fill_line_t, &fill_line0B, &fill_line);
						
					fill_line.x1 = fill_line.x2;
					fill_line.y1 = midy;
					fill_line.x2 = cfill_line->x2;
					fill_line.y2 = cfill_line->y2;
					LL_ADD(struct fill_line_t, &fill_line0D, &fill_line);
					break;
				}
				
				break;
			
			case QUAD_C:
				
				switch(quad2)
				{
				case QUAD_A:
					fill_line.x1 = cfill_line->x1;
					fill_line.y1 = cfill_line->y1;
					fill_line.x2 = cfill_line->x1 + 
						(cfill_line->x2 - cfill_line->x1) * (midy - cfill_line->y1) /
						(cfill_line->y2 - cfill_line->y1);
					fill_line.y2 = midy;
					LL_ADD(struct fill_line_t, &fill_line0C, &fill_line);
						
					fill_line.x1 = fill_line.x2;
					fill_line.y1 = midy;
					fill_line.x2 = cfill_line->x2;
					fill_line.y2 = cfill_line->y2;
					LL_ADD(struct fill_line_t, &fill_line0A, &fill_line);
					break;
				
				case QUAD_B:
					// see if it goes through D or A
				
					if(cfill_line->y1 + 
						(cfill_line->y2 - cfill_line->y1) * (midx - cfill_line->x1) /
						(cfill_line->x2 - cfill_line->x1) < midy)
					{
						// goes through D
						fill_line.x1 = cfill_line->x1;
						fill_line.y1 = cfill_line->y1;
						fill_line.x2 = midx;
						fill_line.y2 = cfill_line->y1 + 
							(cfill_line->y2 - cfill_line->y1) * (midx - cfill_line->x1) /
							(cfill_line->x2 - cfill_line->x1);
						LL_ADD(struct fill_line_t, &fill_line0C, &fill_line);
						
						fill_line.x1 = midx;
						fill_line.y1 = fill_line.y2;
						fill_line.x2 = cfill_line->x1 + 
							(cfill_line->x2 - cfill_line->x1) * (midy - cfill_line->y1) /
							(cfill_line->y2 - cfill_line->y1);
						fill_line.y2 = midy;
						LL_ADD(struct fill_line_t, &fill_line0D, &fill_line);
							
						fill_line.x1 = fill_line.x2;
						fill_line.y1 = midy;
						fill_line.x2 = cfill_line->x2;
						fill_line.y2 = cfill_line->y2;
						LL_ADD(struct fill_line_t, &fill_line0B, &fill_line);
					}
					else
					{
						// goes through A
						fill_line.x1 = cfill_line->x1;
						fill_line.y1 = cfill_line->y1;
						fill_line.x2 = cfill_line->x1 + 
							(cfill_line->x2 - cfill_line->x1) * (midy - cfill_line->y1) /
							(cfill_line->y2 - cfill_line->y1);
						fill_line.y2 = midy;
						LL_ADD(struct fill_line_t, &fill_line0C, &fill_line);
						
						fill_line.x1 = fill_line.x2;
						fill_line.y1 = midy;
						fill_line.x2 = midx;
						fill_line.y2 = cfill_line->y1 + 
							(cfill_line->y2 - cfill_line->y1) * (midx - cfill_line->x1) /
							(cfill_line->x2 - cfill_line->x1);
						LL_ADD(struct fill_line_t, &fill_line0A, &fill_line);
							
						fill_line.x1 = midx;
						fill_line.y1 = fill_line.y2;
						fill_line.x2 = cfill_line->x2;
						fill_line.y2 = cfill_line->y2;
						LL_ADD(struct fill_line_t, &fill_line0B, &fill_line);
					}
					
					break;
				
				case QUAD_C:
					LL_ADD(struct fill_line_t, &fill_line0C, cfill_line);
					break;
			
				case QUAD_D:
					fill_line.x1 = cfill_line->x1;
					fill_line.y1 = cfill_line->y1;
					fill_line.x2 = midx;
					fill_line.y2 = cfill_line->y1 + 
						(cfill_line->y2 - cfill_line->y1) * (midx - cfill_line->x1) /
						(cfill_line->x2 - cfill_line->x1);
					LL_ADD(struct fill_line_t, &fill_line0C, &fill_line);
						
					fill_line.x1 = midx;
					fill_line.y1 = fill_line.y2;
					fill_line.x2 = cfill_line->x2;
					fill_line.y2 = cfill_line->y2;
					LL_ADD(struct fill_line_t, &fill_line0D, &fill_line);
					break;
				}
				
				break;
			
			case QUAD_D:
				
				switch(quad2)
				{
				case QUAD_A:
					// see if it goes through C or B
				
					if(cfill_line->y1 + 
						(cfill_line->y2 - cfill_line->y1) * (midx - cfill_line->x1) /
						(cfill_line->x2 - cfill_line->x1) < midy)
					{
						// goes through C
						fill_line.x1 = cfill_line->x1;
						fill_line.y1 = cfill_line->y1;
						fill_line.x2 = midx;
						fill_line.y2 = cfill_line->y1 + 
							(cfill_line->y2 - cfill_line->y1) * (midx - cfill_line->x1) /
							(cfill_line->x2 - cfill_line->x1);
						LL_ADD(struct fill_line_t, &fill_line0D, &fill_line);
						
						fill_line.x1 = midx;
						fill_line.y1 = fill_line.y2;
						fill_line.x2 = cfill_line->x1 + 
							(cfill_line->x2 - cfill_line->x1) * (midy - cfill_line->y1) /
							(cfill_line->y2 - cfill_line->y1);
						fill_line.y2 = midy;
						LL_ADD(struct fill_line_t, &fill_line0C, &fill_line);
							
						fill_line.x1 = fill_line.x2;
						fill_line.y1 = midy;
						fill_line.x2 = cfill_line->x2;
						fill_line.y2 = cfill_line->y2;
						LL_ADD(struct fill_line_t, &fill_line0A, &fill_line);
					}
					else
					{
						// goes through B
						fill_line.x1 = cfill_line->x1;
						fill_line.y1 = cfill_line->y1;
						fill_line.x2 = cfill_line->x1 + 
							(cfill_line->x2 - cfill_line->x1) * (midy - cfill_line->y1) /
							(cfill_line->y2 - cfill_line->y1);
						fill_line.y2 = midy;
						LL_ADD(struct fill_line_t, &fill_line0D, &fill_line);
						
						fill_line.x1 = fill_line.x2;
						fill_line.y1 = midy;
						fill_line.x2 = midx;
						fill_line.y2 = cfill_line->y1 + 
							(cfill_line->y2 - cfill_line->y1) * (midx - cfill_line->x1) /
							(cfill_line->x2 - cfill_line->x1);
						LL_ADD(struct fill_line_t, &fill_line0B, &fill_line);
							
						fill_line.x1 = midx;
						fill_line.y1 = fill_line.y2;
						fill_line.x2 = cfill_line->x2;
						fill_line.y2 = cfill_line->y2;
						LL_ADD(struct fill_line_t, &fill_line0A, &fill_line);
					}
				
					break;
				
				case QUAD_B:
					fill_line.x1 = cfill_line->x1;
					fill_line.y1 = cfill_line->y1;
					fill_line.x2 = cfill_line->x1 + 
						(cfill_line->x2 - cfill_line->x1) * (midy - cfill_line->y1) /
						(cfill_line->y2 - cfill_line->y1);
					fill_line.y2 = midy;
					LL_ADD(struct fill_line_t, &fill_line0D, &fill_line);
					
					fill_line.x1 = fill_line.x2;
					fill_line.y1 = midy;
					fill_line.x2 = cfill_line->x2;
					fill_line.y2 = cfill_line->y2;
					LL_ADD(struct fill_line_t, &fill_line0B, &fill_line);
					break;
				
				case QUAD_C:
					fill_line.x1 = cfill_line->x1;
					fill_line.y1 = cfill_line->y1;
					fill_line.x2 = midx;
					fill_line.y2 = cfill_line->y1 + 
						(cfill_line->y2 - cfill_line->y1) * (midx - cfill_line->x1) /
						(cfill_line->x2 - cfill_line->x1);
					LL_ADD(struct fill_line_t, &fill_line0D, &fill_line);
						
					fill_line.x1 = midx;
					fill_line.y1 = fill_line.y2;
					fill_line.x2 = cfill_line->x2;
					fill_line.y2 = cfill_line->y2;
					LL_ADD(struct fill_line_t, &fill_line0C, &fill_line);
					break;
			
				case QUAD_D:
					LL_ADD(struct fill_line_t, &fill_line0D, cfill_line);
					break;
				}
				
				break;
			}
			
			cfill_line = cfill_line->next;
		}

		if(!fill_line0A)
		{
			// determine if quadrant is inside or outside boundary
			if(center_inside)
			{
				// this quadrant is fully inside
				generate_quad_verticies(fill, minx, midy, midx, maxy);
			}
		}
		else
		{
			// determine if new center is inside or outside
			
			double new_midx = (minx + midx) / 2;
			double new_midy = (midy + maxy) / 2;
			
			cfill_line = fill_line0A;
			
			int new_inside = center_inside;
			
			while(cfill_line)
			{
				if(lines_intersect(midx, midy, new_midx, new_midy, 
					cfill_line->x1, cfill_line->y1, cfill_line->x2, cfill_line->y2))
					new_inside = !new_inside;

				cfill_line = cfill_line->next;
			}
			
			if(!process_quad(fill, minx, midy, midx, maxy, new_inside, fill_line0A))
			{
				LL_REMOVE_ALL(struct fill_line_t, &fill_line0A);
				LL_REMOVE_ALL(struct fill_line_t, &fill_line0B);
				LL_REMOVE_ALL(struct fill_line_t, &fill_line0C);
				LL_REMOVE_ALL(struct fill_line_t, &fill_line0D);
				return 0;
			}
		}
		
		LL_REMOVE_ALL(struct fill_line_t, &fill_line0A);
		
		if(!fill_line0B)
		{
			// determine if quadrant is inside or outside boundary
			if(center_inside)
			{
				// this quadrant is fully inside
				generate_quad_verticies(fill, midx, midy, maxx, maxy);
			}
		}
		else
		{
			// determine if new center is inside or outside
			
			double new_midx = (midx + maxx) / 2;
			double new_midy = (midy + maxy) / 2;
			
			cfill_line = fill_line0B;
			
			int new_inside = center_inside;
			
			while(cfill_line)
			{
				if(lines_intersect(midx, midy, new_midx, new_midy, 
					cfill_line->x1, cfill_line->y1, cfill_line->x2, cfill_line->y2))
					new_inside = !new_inside;

				cfill_line = cfill_line->next;
			}
			
			if(!process_quad(fill, midx, midy, maxx, maxy, new_inside, fill_line0B))
			{
				LL_REMOVE_ALL(struct fill_line_t, &fill_line0B);
				LL_REMOVE_ALL(struct fill_line_t, &fill_line0C);
				LL_REMOVE_ALL(struct fill_line_t, &fill_line0D);
				return 0;
			}
		}
		
		LL_REMOVE_ALL(struct fill_line_t, &fill_line0B);
		
		if(!fill_line0C)
		{
			// determine if quadrant is inside or outside boundary
			if(center_inside)
			{
				// this quadrant is fully inside
				generate_quad_verticies(fill, minx, miny, midx, midy);
			}
		}
		else
		{
			// determine if new center is inside or outside
			
			double new_midx = (minx + midx) / 2;
			double new_midy = (miny + midy) / 2;
			
			cfill_line = fill_line0C;
			
			int new_inside = center_inside;
			
			while(cfill_line)
			{
				if(lines_intersect(midx, midy, new_midx, new_midy, 
					cfill_line->x1, cfill_line->y1, cfill_line->x2, cfill_line->y2))
					new_inside = !new_inside;

				cfill_line = cfill_line->next;
			}
			
			if(!process_quad(fill, minx, miny, midx, midy, new_inside, fill_line0C))
			{
				LL_REMOVE_ALL(struct fill_line_t, &fill_line0C);
				LL_REMOVE_ALL(struct fill_line_t, &fill_line0D);
				return 0;
			}
				
		}
		
		LL_REMOVE_ALL(struct fill_line_t, &fill_line0C);
		
		if(!fill_line0D)
		{
			// determine if quadrant is inside or outside boundary
			if(center_inside)
			{
				generate_quad_verticies(fill, midx, miny, maxx, midy);
			}
		}
		else
		{
			// determine if new center is inside or outside
			
			double new_midx = (midx + maxx) / 2;
			double new_midy = (miny + midy) / 2;
			
			cfill_line = fill_line0D;
			
			int new_inside = center_inside;
			
			while(cfill_line)
			{
				if(lines_intersect(midx, midy, new_midx, new_midy, 
					cfill_line->x1, cfill_line->y1, cfill_line->x2, cfill_line->y2))
					new_inside = !new_inside;

				cfill_line = cfill_line->next;
			}
			
			if(!process_quad(fill, midx, miny, maxx, midy, new_inside, fill_line0D))
			{
				LL_REMOVE_ALL(struct fill_line_t, &fill_line0D);
				return 0;
			}
				
		}
		
		LL_REMOVE_ALL(struct fill_line_t, &fill_line0D);
	}
	
	return 1;
}


/*
int quad_tree_fill(struct fill_t *fill)
{
	// make list of lines for this fill and
	// work out min and max for x and y
	
	struct fill_line_t *fill_line0 = NULL;
	double minx = 0.0, miny = 0.0, maxx = 0.0, maxy = 0.0;	// shut up compiler
	
	struct fill_edge_t *cedge = fill->edge0;
	int first = 1;
		
	while(cedge)
	{
		struct fill_line_t fill_line = {cedge->point1->x, cedge->point1->y, 
			cedge->point2->x, cedge->point2->y};
		
		if(first)
		{
			minx = maxx = fill_line.x1;
			miny = maxy = fill_line.y1;
			first = 0;
		}
		else
		{
			if(fill_line.x1 < minx)
				minx = fill_line.x1;
			
			if(fill_line.y1 < miny)
				miny = fill_line.y1;
			
			if(fill_line.x1 > maxx)
				maxx = fill_line.x1;
			
			if(fill_line.y1 > maxy)
				maxy = fill_line.y1;
		}
		
		if(fill_line.x2 < minx)
			minx = fill_line.x2;
		
		if(fill_line.y2 < miny)
			miny = fill_line.y2;
		
		if(fill_line.x2 > maxx)
			maxx = fill_line.x2;
		
		if(fill_line.y2 > maxy)
			maxy = fill_line.y2;
		
		LL_ADD(struct fill_line_t, &fill_line0, &fill_line);
		cedge = cedge->next;
	}

	if(!process_quad(fill, minx, miny, maxx, maxy, 1, fill_line0))	// presume center is inside
	{
		LL_REMOVE_ALL(struct fill_line_t, &fill_line0);
		return 0;
	}
	
	LL_REMOVE_ALL(struct fill_line_t, &fill_line0);
		
	return 1;
}
*/



struct fill_line_t *quad_fill_line0;
double quad_minx, quad_miny, quad_maxx, quad_maxy;
int quad_first = 1;

void add_quad_tree_fill_line(double *x1, double *y1, double *x2, double *y2)
{
	struct fill_line_t fill_line = {*x1, *y1, *x2, *y2};
	
	if(quad_first)
	{
		quad_minx = quad_maxx = fill_line.x1;
		quad_miny = quad_maxy = fill_line.y1;
		quad_first = 0;
	}
	else
	{
		if(fill_line.x1 < quad_minx)
			quad_minx = fill_line.x1;
		
		if(fill_line.y1 < quad_miny)
			quad_miny = fill_line.y1;
		
		if(fill_line.x1 > quad_maxx)
			quad_maxx = fill_line.x1;
		
		if(fill_line.y1 > quad_maxy)
			quad_maxy = fill_line.y1;
	}
	
	if(fill_line.x2 < quad_minx)
		quad_minx = fill_line.x2;
	
	if(fill_line.y2 < quad_miny)
		quad_miny = fill_line.y2;
	
	if(fill_line.x2 > quad_maxx)
		quad_maxx = fill_line.x2;
	
	if(fill_line.y2 > quad_maxy)
		quad_maxy = fill_line.y2;
	
	LL_ADD(struct fill_line_t, &quad_fill_line0, &fill_line);
}


void add_follow_straight_fill_left_edge_segment(struct conn_t *conn, double t1, double t2)
{
	struct node_t *node1, *node2;
	int left1, right1, left2, right2;
	double x1, y1, x2, y2;
	
	if(!conn->orientation)
	{
		node1 = conn->node1;
		node2 = conn->node2;
		x1 = conn->node1->effective_x[conn->sat1];
		y1 = conn->node1->effective_y[conn->sat1];
		x2 = conn->node2->effective_x[conn->sat2];
		y2 = conn->node2->effective_y[conn->sat2];
		get_width_sats(conn, &left1, &right1, &left2, &right2);
	}
	else
	{
		node1 = conn->node2;
		node2 = conn->node1;
		x1 = conn->node2->effective_x[conn->sat2];
		y1 = conn->node2->effective_y[conn->sat2];
		x2 = conn->node1->effective_x[conn->sat1];
		y2 = conn->node1->effective_y[conn->sat1];
		get_width_sats(conn, &left2, &right2, &left1, &right1);
	}
	
	
	double deltax = x2 - x1;
	double deltay = y2 - y1;
	
	double nx1 = x1 + deltax * t1;
	double ny1 = y1 + deltay * t1;
	double nx2 = x1 + deltax * t2;
	double ny2 = y1 + deltay * t2;
	
	double l = hypot(deltax, deltay);
	
	deltax /= l;
	deltay /= l;
	
	double width = 20.0;
	
	nx1 -= deltay * width;
	ny1 += deltax * width;
	nx2 -= deltay * width;
	ny2 += deltax * width;
	
	add_quad_tree_fill_line(&nx1, &ny1, &nx2, &ny2);
}


void add_follow_straight_fill_right_edge_segment(struct conn_t *conn, double t1, double t2)
{
	struct node_t *node1, *node2;
	int left1, right1, left2, right2;
	double x1, y1, x2, y2;
	
	if(!conn->orientation)
	{
		node1 = conn->node1;
		node2 = conn->node2;
		x1 = conn->node1->effective_x[conn->sat1];
		y1 = conn->node1->effective_y[conn->sat1];
		x2 = conn->node2->effective_x[conn->sat2];
		y2 = conn->node2->effective_y[conn->sat2];
		get_width_sats(conn, &left1, &right1, &left2, &right2);
	}
	else
	{
		node1 = conn->node2;
		node2 = conn->node1;
		x1 = conn->node2->effective_x[conn->sat2];
		y1 = conn->node2->effective_y[conn->sat2];
		x2 = conn->node1->effective_x[conn->sat1];
		y2 = conn->node1->effective_y[conn->sat1];
		get_width_sats(conn, &left2, &right2, &left1, &right1);
	}
	
	
	double deltax = x2 - x1;
	double deltay = y2 - y1;
	
	double nx1 = x1 + deltax * t1;
	double ny1 = y1 + deltay * t1;
	double nx2 = x1 + deltax * t2;
	double ny2 = y1 + deltay * t2;
	
	double l = hypot(deltax, deltay);
	
	deltax /= l;
	deltay /= l;
	
	double width = 20.0;
	
	nx1 += deltay * width;
	ny1 -= deltax * width;
	nx2 += deltay * width;
	ny2 -= deltax * width;
	
	add_quad_tree_fill_line(&nx1, &ny1, &nx2, &ny2);
}


void add_follow_curve_fill_left_edge_segment(struct conn_t *conn, double t1, double t2)
{
	struct bezier_t bezier;
	
	// must check orientation because t value is measured from beginning
	
	if(!conn->orientation)
	{
		bezier.x1 = conn->node1->effective_x[conn->sat1];
		bezier.y1 = conn->node1->effective_y[conn->sat1];
		bezier.x2 = conn->node1->effective_x[conn->sat1] + conn->node1->sats[conn->sat1].x;
		bezier.y2 = conn->node1->effective_y[conn->sat1] + conn->node1->sats[conn->sat1].y;
		bezier.x3 = conn->node2->effective_x[conn->sat2] + conn->node2->sats[conn->sat2].x;
		bezier.y3 = conn->node2->effective_y[conn->sat2] + conn->node2->sats[conn->sat2].y;
		bezier.x4 = conn->node2->effective_x[conn->sat2];
		bezier.y4 = conn->node2->effective_y[conn->sat2];
	}
	else
	{
		bezier.x1 = conn->node2->effective_x[conn->sat2];
		bezier.y1 = conn->node2->effective_y[conn->sat2];
		bezier.x2 = conn->node2->effective_x[conn->sat2] + conn->node2->sats[conn->sat2].x;
		bezier.y2 = conn->node2->effective_y[conn->sat2] + conn->node2->sats[conn->sat2].y;
		bezier.x3 = conn->node1->effective_x[conn->sat1] + conn->node1->sats[conn->sat1].x;
		bezier.y3 = conn->node1->effective_y[conn->sat1] + conn->node1->sats[conn->sat1].y;
		bezier.x4 = conn->node1->effective_x[conn->sat1];
		bezier.y4 = conn->node1->effective_y[conn->sat1];
	}
	
	struct t_t *ct = conn->t0;
		
	while(ct)
	{
		if(ct->t2 >= t1 && ct->t1 <= t2)
		{
			float deltax, deltay;
			deltaBRZ(&bezier, ct->t1, &deltax, &deltay);
			
			double l = hypot(deltax, deltay);
			
			deltax /= l;
			deltay /= l;
		
			double width = 20.0;
			
			double x1 = ct->x1 - deltay * width;
			double y1 = ct->y1 + deltax * width;
			
			deltaBRZ(&bezier, ct->t2, &deltax, &deltay);
			
			l = hypot(deltax, deltay);
			
			deltax /= l;
			deltay /= l;
		
			double x2 = ct->x2 - deltay * width;
			double y2 = ct->y2 + deltax * width;
			
			add_quad_tree_fill_line(&x1, &y1, &x2, &y2);
		}
		
		ct = ct->next;
	}
}


void add_follow_curve_fill_right_edge_segment(struct conn_t *conn, double t1, double t2)
{
	struct bezier_t bezier;
	
	// must check orientation because t value is measured from beginning
	
	if(!conn->orientation)
	{
		bezier.x1 = conn->node1->effective_x[conn->sat1];
		bezier.y1 = conn->node1->effective_y[conn->sat1];
		bezier.x2 = conn->node1->effective_x[conn->sat1] + conn->node1->sats[conn->sat1].x;
		bezier.y2 = conn->node1->effective_y[conn->sat1] + conn->node1->sats[conn->sat1].y;
		bezier.x3 = conn->node2->effective_x[conn->sat2] + conn->node2->sats[conn->sat2].x;
		bezier.y3 = conn->node2->effective_y[conn->sat2] + conn->node2->sats[conn->sat2].y;
		bezier.x4 = conn->node2->effective_x[conn->sat2];
		bezier.y4 = conn->node2->effective_y[conn->sat2];
	}
	else
	{
		bezier.x1 = conn->node2->effective_x[conn->sat2];
		bezier.y1 = conn->node2->effective_y[conn->sat2];
		bezier.x2 = conn->node2->effective_x[conn->sat2] + conn->node2->sats[conn->sat2].x;
		bezier.y2 = conn->node2->effective_y[conn->sat2] + conn->node2->sats[conn->sat2].y;
		bezier.x3 = conn->node1->effective_x[conn->sat1] + conn->node1->sats[conn->sat1].x;
		bezier.y3 = conn->node1->effective_y[conn->sat1] + conn->node1->sats[conn->sat1].y;
		bezier.x4 = conn->node1->effective_x[conn->sat1];
		bezier.y4 = conn->node1->effective_y[conn->sat1];
	}
	
	struct t_t *ct = conn->t0;
		
	while(ct)
	{
		if(ct->t2 >= t1 && ct->t1 <= t2)
		{
			float deltax, deltay;
			deltaBRZ(&bezier, ct->t1, &deltax, &deltay);
			
			double l = hypot(deltax, deltay);
			
			deltax /= l;
			deltay /= l;
		
			double width = 20.0;
			
			double x1 = ct->x1 + deltay * width;
			double y1 = ct->y1 - deltax * width;
			
			deltaBRZ(&bezier, ct->t2, &deltax, &deltay);
			
			l = hypot(deltax, deltay);
			
			deltax /= l;
			deltay /= l;
		
			double x2 = ct->x2 + deltay * width;
			double y2 = ct->y2 - deltax * width;
			
			add_quad_tree_fill_line(&x1, &y1, &x2, &y2);
		}
		
		ct = ct->next;
	}
}


int quad_tree_fill(struct fill_t *fill)
{
	quad_first = 1;
	
	// make list of lines for this fill and
	// work out min and max for x and y
	
	struct fill_edge_t *fill_edge = fill->edge0, *last_fill_edge = fill->edge0, *next_fill_edge;

	while(last_fill_edge->next)
		last_fill_edge = last_fill_edge->next;
	
	while(fill_edge)
	{
		if(fill_edge->next)
			next_fill_edge = fill_edge->next;
		else
			next_fill_edge = fill->edge0;
		
		if(fill_edge->follow_curve && fill_edge->point1->curve == fill_edge->point2->curve)
		{
			struct curve_t *curve = fill_edge->point1->curve;
			
			// determine whether we are drawing on the left or right
			
			if(fill_edge->point1->pos < fill_edge->point2->pos)
			{
				if(fill_edge->point1->conn == fill_edge->point2->conn)
				{
					switch(fill_edge->point1->conn->type)
					{
					case CONN_TYPE_STRAIGHT:
						add_follow_straight_fill_right_edge_segment(fill_edge->point1->conn, 
							fill_edge->point1->t, fill_edge->point2->t);
						break;
					
					case CONN_TYPE_BEZIER:
						add_follow_curve_fill_right_edge_segment(fill_edge->point1->conn, 
							fill_edge->point1->t, fill_edge->point2->t);
						break;
					}
				}
				else
				{
					switch(fill_edge->point1->conn->type)
					{
					case CONN_TYPE_STRAIGHT:
						add_follow_straight_fill_right_edge_segment(fill_edge->point1->conn, 
							fill_edge->point1->t, 1.0);
						break;
					
					case CONN_TYPE_BEZIER:
						add_follow_curve_fill_right_edge_segment(fill_edge->point1->conn, 
							fill_edge->point1->t, 1.0);
						break;
					}
					
					struct conn_pointer_t *cconnp = curve->connp0;
						
					while(cconnp->conn != fill_edge->point1->conn)
						cconnp = cconnp->next;
					
					cconnp = cconnp->next;
					
					while(cconnp->conn != fill_edge->point2->conn)
					{
						switch(cconnp->conn->type)
						{
						case CONN_TYPE_STRAIGHT:
							add_follow_straight_fill_right_edge_segment(cconnp->conn, 
								0.0, 1.0);
							break;
						
						case CONN_TYPE_BEZIER:
							add_follow_curve_fill_right_edge_segment(cconnp->conn, 
								0.0, 1.0);
							break;
						}
						
						cconnp = cconnp->next;
					}
					
					switch(fill_edge->point2->conn->type)
					{
					case CONN_TYPE_STRAIGHT:
						add_follow_straight_fill_right_edge_segment(fill_edge->point2->conn, 
							0.0, fill_edge->point2->t);
						break;
					
					case CONN_TYPE_BEZIER:
						add_follow_curve_fill_right_edge_segment(fill_edge->point2->conn, 
							0.0, fill_edge->point2->t);
						break;
					}
				}
			}
			else
			{
				if(fill_edge->point1->conn == fill_edge->point2->conn)
				{
					switch(fill_edge->point1->conn->type)
					{
					case CONN_TYPE_STRAIGHT:
						add_follow_straight_fill_left_edge_segment(fill_edge->point1->conn, 
							fill_edge->point1->t, fill_edge->point2->t);
						break;
					
					case CONN_TYPE_BEZIER:
						add_follow_curve_fill_left_edge_segment(fill_edge->point1->conn, 
							fill_edge->point1->t, fill_edge->point2->t);
						break;
					}
				}
				else
				{
					switch(fill_edge->point1->conn->type)
					{
					case CONN_TYPE_STRAIGHT:
						add_follow_straight_fill_left_edge_segment(fill_edge->point1->conn, 
							fill_edge->point1->t, 1.0);
						break;
					
					case CONN_TYPE_BEZIER:
						add_follow_curve_fill_left_edge_segment(fill_edge->point1->conn, 
							fill_edge->point1->t, 1.0);
						break;
					}
					
					struct conn_pointer_t *cconnp = curve->connp0;
						
					while(cconnp->conn != fill_edge->point1->conn)
						cconnp = cconnp->next;
					
					cconnp = cconnp->next;
					
					while(cconnp->conn != fill_edge->point2->conn)
					{
						switch(cconnp->conn->type)
						{
						case CONN_TYPE_STRAIGHT:
							add_follow_straight_fill_left_edge_segment(cconnp->conn, 0.0, 1.0);
							break;
						
						case CONN_TYPE_BEZIER:
							add_follow_curve_fill_left_edge_segment(cconnp->conn, 0.0, 1.0);
							break;
						}
						
						cconnp = cconnp->next;
					}
					
					switch(fill_edge->point2->conn->type)
					{
					case CONN_TYPE_STRAIGHT:
						add_follow_straight_fill_left_edge_segment(fill_edge->point2->conn, 
							0.0, fill_edge->point2->t);
						break;
					
					case CONN_TYPE_BEZIER:
						add_follow_curve_fill_left_edge_segment(fill_edge->point2->conn, 
							0.0, fill_edge->point2->t);
						break;
					}
				}
			}
		}
		else		// not following curve
		{
			double x1, y1, x2, y2;
			
			if(last_fill_edge->point2->pos < last_fill_edge->point1->pos)
			{
				x1 = fill_edge->point1->x - 
					fill_edge->point1->deltay * fill_edge->point1->left_width;
				y1 = fill_edge->point1->y + 
					fill_edge->point1->deltax * fill_edge->point1->left_width;
			}
			else
			{
				x1 = fill_edge->point1->x + 
					fill_edge->point1->deltay * fill_edge->point1->right_width;
				y1 = fill_edge->point1->y - 
					fill_edge->point1->deltax * fill_edge->point1->right_width;
			}
					
			
			if(next_fill_edge->point2->pos < next_fill_edge->point1->pos)
			{
				x2 = fill_edge->point2->x - 
					fill_edge->point2->deltay * fill_edge->point2->left_width;
				y2 = fill_edge->point2->y + 
					fill_edge->point2->deltax * fill_edge->point2->left_width;
			}
			else
			{
				x2 = fill_edge->point2->x + 
					fill_edge->point2->deltay * fill_edge->point2->right_width;
				y2 = fill_edge->point2->y - 
					fill_edge->point2->deltax * fill_edge->point2->right_width;
			}
					
			add_quad_tree_fill_line(&x1, &y1, &x2, &y2);
		}
		
		last_fill_edge = fill_edge;
		fill_edge = fill_edge->next;
	}


	if(!process_quad(fill, quad_minx, quad_miny, quad_maxx, quad_maxy, 
		1, quad_fill_line0))	// presume center is inside
	{
		LL_REMOVE_ALL(struct fill_line_t, &quad_fill_line0);
		return 0;
	}
	
	LL_REMOVE_ALL(struct fill_line_t, &quad_fill_line0);
		
	return 1;
}


void generate_fill_verticies()
{
	if(!worker_try_enter_main_lock())
		return;
	
	struct fill_t *cfill = fill0;
	
	while(cfill)
	{
		switch(cfill->type)
		{
		case FILL_TYPE_SOLID:
			if(!cfill->texture_verts0 && !cfill->texture_polys0)
				if(!quad_tree_fill(cfill))
					return;
			break;
		
		case FILL_TYPE_TEXTURE:
			if(cfill->texture && !cfill->texture_verts0 && !cfill->texture_polys0)
				if(!quad_tree_fill(cfill))
					return;
			break;
		}
		
		cfill = cfill->next;
	}
	
	worker_leave_main_lock();
}


int tile_all_fills()
{
	if(!worker_try_enter_main_lock())
		return 0;

	struct fill_t *fill = fill0;

	while(fill)
	{
		if((fill->texture_verts0 || fill->texture_polys0) && !fill->texture_tiled)
		{
			worker_leave_main_lock();
			
			if(!tile_fill(fill))
				return 0;
			
			if(!worker_try_enter_main_lock())
				return 0;
		}
		
		fill = fill->next;
	}
	
	worker_leave_main_lock();
	
	return 1;
}




void run_fill_menu(struct fill_t *fill)
{
	GtkWidget *menu;
	GtkWidget *menu_items;
	menu = gtk_menu_new();

	menu_items = gtk_menu_item_new_with_label("Fill Properties");
	gtk_signal_connect(GTK_OBJECT(menu_items), "activate", 
		GTK_SIGNAL_FUNC(run_fill_properties_dialog), fill);
	gtk_menu_append(GTK_MENU(menu), menu_items);
	gtk_widget_show(menu_items);

	menu_items = gtk_menu_item_new_with_label("Add Line");
//	gtk_signal_connect(GTK_OBJECT(menu_items), "activate", 
//		GTK_SIGNAL_FUNC(menu_insert_point), curve);
	gtk_menu_append(GTK_MENU(menu), menu_items);
	gtk_widget_show(menu_items);
	
	menu_items = gtk_menu_item_new_with_label("Remove Line");
//	gtk_signal_connect(GTK_OBJECT(menu_items), "activate", 
//		GTK_SIGNAL_FUNC(menu_insert_point), curve);
	gtk_menu_append(GTK_MENU(menu), menu_items);
	gtk_widget_show(menu_items);
	
	menu_items = gtk_menu_item_new_with_label("Remove Fill");
//	gtk_signal_connect(GTK_OBJECT(menu_items), "activate", 
//		GTK_SIGNAL_FUNC(menu_insert_point), curve);
	gtk_menu_append(GTK_MENU(menu), menu_items);
	gtk_widget_show(menu_items);
	
	gtk_menu_popup (GTK_MENU(menu), NULL, NULL, (GtkMenuPositionFunc)(NULL), NULL, 0, 0);
}
