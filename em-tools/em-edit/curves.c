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
#include <stdio.h>
#include <stdint.h>
#include <memory.h>
#include <math.h>

#include <assert.h>

#include <zlib.h>
#include <gtk/gtk.h>

#define USE_GDK_PIXBUF

#include "llist.h"
#include "vertex.h"
#include "polygon.h"
#include "stringbuf.h"
#include "gsub.h"
#include "conns.h"
#include "bezier.h"
#include "curves.h"
#include "tiles.h"
#include "nodes.h"
#include "map.h"
#include "worker.h"
#include "interface.h"
#include "main.h"
#include "glade.h"
#include "points.h"
#include "main_lock.h"
#include "bsp.h"



struct curve_t *curve0 = NULL;


int add_curve_pointer(struct curve_pointer_t **curvep0, struct curve_t *curve)
{
	if(!curvep0)
		return 0;

	if(!*curvep0)
	{
		*curvep0 = malloc(sizeof(struct curve_pointer_t));
		(*curvep0)->curve = curve;
		(*curvep0)->next = NULL;
	}
	else
	{
		struct curve_pointer_t *ccurvep = *curvep0;

		while(ccurvep->next)
		{
			if(ccurvep->curve == curve)
				return 0;

			ccurvep = ccurvep->next;
		}

		if(ccurvep->curve != curve)
		{
			ccurvep->next = malloc(sizeof(struct curve_pointer_t));
			ccurvep = ccurvep->next;
			ccurvep->curve = curve;
			ccurvep->next = NULL;
		}
		else
		{
			return 0;
		}
	}

	return 1;
}


void remove_curve_pointer(struct curve_pointer_t **curvep0, struct curve_t *curve)
{
	if(!curvep0)
		return;

	struct curve_pointer_t *ccurvep = *curvep0;

	while(ccurvep)
	{
		if(ccurvep->curve == curve)
		{
			LL_REMOVE(struct curve_pointer_t, curvep0, ccurvep);
			break;
		}

		ccurvep = ccurvep->next;
	}
}


void free_curve(struct curve_t *curve)
{
	if(!curve)
		return;

	LL_REMOVE_ALL(struct conn_pointer_t, &curve->connp0);
	free_surface(curve->texture_surface);
	free_string(curve->texture_filename);
	LL_REMOVE(struct curve_t, &curve0, curve);
}


struct curve_t *get_curve(struct conn_t *conn)
{
	struct curve_t *ccurve = curve0;

	while(ccurve)
	{
		struct conn_pointer_t *cconnp = ccurve->connp0;

		while(cconnp)
		{
			if(cconnp->conn == conn)
				return ccurve;

			cconnp = cconnp->next;
		}

		ccurve = ccurve->next;
	}

	return NULL;
}


struct conn_pointer_t *get_last_connp(struct conn_pointer_t *connp)
{
	while(connp->next)
		connp = connp->next;

	return connp;
}


void delete_all_verticies_in_curve(struct curve_t *curve)
{
	struct conn_pointer_t *cconnp = curve->connp0;

	while(cconnp)
	{
		if(cconnp->conn->verts)
		{
			free(cconnp->conn->verts);
			cconnp->conn->verts = NULL;
		}
		
		if(cconnp->conn->squished_texture)
		{
			free_surface(cconnp->conn->squished_texture);
			cconnp->conn->squished_texture = NULL;
		}

		cconnp = cconnp->next;
	}
}


void add_conn_to_curves(struct conn_t *conn)		// always called while not working
{
	struct curve_t *ccurve = curve0;
	struct node_t *new_exposed_node = NULL;
	int exposed_node_end = 0, connecting_node;


	// search for a curve that either starts or ends
	// with a node from the new connection

	while(ccurve)
	{
		connecting_node = 0;
		
		if((!ccurve->connp0->conn->orientation && ccurve->connp0->conn->node1 == conn->node1) || 
			(ccurve->connp0->conn->orientation && ccurve->connp0->conn->node2 == conn->node1))
			connecting_node = 1;
		else if((!ccurve->connp0->conn->orientation && ccurve->connp0->conn->node1 == conn->node2) ||
			(ccurve->connp0->conn->orientation && ccurve->connp0->conn->node2 == conn->node2))
			connecting_node = 2;
		
		if(connecting_node)
		{
			// check the new connection doesn't attach at a passing through sat
			
			if(!ccurve->connp0->conn->orientation)
			{
				if(ccurve->connp0->conn->sat1 == 0 || ccurve->connp0->conn->sat1 == 1)
				{
					if((connecting_node == 1 && conn->sat1 != 0 && conn->sat1 != 1) ||
						(connecting_node == 2 && conn->sat2 != 0 && conn->sat2 != 1))
					{
						ccurve = ccurve->next;
						continue;
					}
				}
				else
				{
					if((connecting_node == 1 && conn->sat1 != 2 && conn->sat1 != 3) ||
						(connecting_node == 2 && conn->sat2 != 2 && conn->sat2 != 3))
					{
						ccurve = ccurve->next;
						continue;
					}
				}
			}
			else
			{
				if(ccurve->connp0->conn->sat2 == 0 || ccurve->connp0->conn->sat2 == 1)
				{
					if((connecting_node == 1 && conn->sat1 != 0 && conn->sat1 != 1) ||
						(connecting_node == 2 && conn->sat2 != 0 && conn->sat2 != 1))
					{
						ccurve = ccurve->next;
						continue;
					}
				}
				else
				{
					if((connecting_node == 1 && conn->sat1 != 2 && conn->sat1 != 3) ||
						(connecting_node == 2 && conn->sat2 != 2 && conn->sat2 != 3))
					{
						ccurve = ccurve->next;
						continue;
					}
				}
			}

			
			// the conn should be inserted at the beginning of this curve

			struct conn_pointer_t *temp = ccurve->connp0;
			ccurve->connp0 = malloc(sizeof(struct conn_pointer_t));
			ccurve->connp0->conn = conn;
			ccurve->connp0->next = temp;


			// orientate the conn correctly

			switch(connecting_node)
			{
			case 1:
				conn->orientation = 1;
				new_exposed_node = conn->node2;
				break;
			
			case 2:
				conn->orientation = 0;
				new_exposed_node = conn->node1;
				break;
			}

			exposed_node_end = 0;

			break;
		}
		
		struct conn_pointer_t *lconnp = get_last_connp(ccurve->connp0);

		if((!lconnp->conn->orientation && lconnp->conn->node2 == conn->node1) || 
			(lconnp->conn->orientation && lconnp->conn->node1 == conn->node1))
			connecting_node = 1;
		else if((!lconnp->conn->orientation && lconnp->conn->node2 == conn->node2) ||
			(lconnp->conn->orientation && lconnp->conn->node1 == conn->node2))
			connecting_node = 2;
		
		if(connecting_node)
		{
			// check the new connection doesn't attach at a passing through sat
			
			if(!lconnp->conn->orientation)
			{
				if(lconnp->conn->sat2 == 0 || lconnp->conn->sat2 == 1)
				{
					if((connecting_node == 1 && conn->sat1 != 0 && conn->sat1 != 1) ||
						(connecting_node == 2 && conn->sat2 != 0 && conn->sat2 != 1))
					{
						ccurve = ccurve->next;
						continue;
					}
				}
				else
				{
					if((connecting_node == 1 && conn->sat1 != 2 && conn->sat1 != 3) ||
						(connecting_node == 2 && conn->sat2 != 2 && conn->sat2 != 3))
					{
						ccurve = ccurve->next;
						continue;
					}
				}
			}
			else
			{
				if(lconnp->conn->sat1 == 0 || lconnp->conn->sat1 == 1)
				{
					if((connecting_node == 1 && conn->sat1 != 0 && conn->sat1 != 1) ||
						(connecting_node == 2 && conn->sat2 != 0 && conn->sat2 != 1))
					{
						ccurve = ccurve->next;
						continue;
					}
				}
				else
				{
					if((connecting_node == 1 && conn->sat1 != 2 && conn->sat1 != 3) ||
						(connecting_node == 2 && conn->sat2 != 2 && conn->sat2 != 3))
					{
						ccurve = ccurve->next;
						continue;
					}
				}
			}
			

			// the conn should be inserted at the end of this curve

			lconnp->next = malloc(sizeof(struct conn_pointer_t));
			lconnp->next->conn = conn;
			lconnp->next->next = NULL;


			// orientate the conn correctly

			switch(connecting_node)
			{
			case 1:
				conn->orientation = 0;
				new_exposed_node = conn->node2;
				break;
			
			case 2:
				conn->orientation = 1;
				new_exposed_node = conn->node1;
				break;
			}
			
			
			exposed_node_end = 1;

			break;
		}

		ccurve = ccurve->next;
	}


	// if we didn't find a curve, make a new one

	if(!ccurve)
	{
		struct curve_t curve;
		curve.connp0 = malloc(sizeof(struct conn_pointer_t));
		curve.connp0->conn = conn;
		curve.connp0->conn->orientation = 0;
		curve.connp0->next = NULL;
		curve.pre_texture_surface = NULL;
		curve.texture_surface = NULL;
		curve.texture_filename = NULL;
		curve.texture_flip_horiz = 0;
		curve.texture_flip_vert = 0;
		curve.fixed_reps = 1;
		curve.texture_length = 40.0;
		curve.reps = 1.0;
		curve.pixel_offset_horiz = 0;
		curve.pixel_offset_vert = 0;
		curve.fill_type = CURVE_SOLID;
		curve.red = 45;
		curve.green = 153;
		curve.blue = 231;
		curve.alpha = 255;
		curve.width_lock_on = 0;
		curve.width_lock = 0.0;
		curve.texture_rotate_left = 0;
		curve.texture_rotate_right = 0;
		curve.next = NULL;
		
		generate_bigt_values(conn);


		LL_ADD(struct curve_t, &curve0, &curve);
	}
	else
	{
		// we did attach the conn to a curve

		// does the other end also join onto another curve?

		struct curve_t *attached_curve = ccurve;

		ccurve = curve0;
		
		while(ccurve)
		{
			if(ccurve != attached_curve)
			{
				// check the first node on this curve
				
				struct conn_pointer_t *lconnp, *temp, *cconnp;
				int connecting_node = 0;
				
				if(!ccurve->connp0->conn->orientation && 
					ccurve->connp0->conn->node1 == new_exposed_node)
					connecting_node = 1;
				
				if(ccurve->connp0->conn->orientation && 
					ccurve->connp0->conn->node2 == new_exposed_node)
					connecting_node = 2;
				
				if(connecting_node)
				{
					// check the new connection doesn't attach at a passing through sat
					
					if(!ccurve->connp0->conn->orientation)
					{
						if(ccurve->connp0->conn->sat1 == 0 || ccurve->connp0->conn->sat1 == 1)
						{
							if((new_exposed_node == conn->node1 && 
								conn->sat1 != 0 && conn->sat1 != 1) ||
								(new_exposed_node == conn->node2 && 
								conn->sat2 != 0 && conn->sat2 != 1))
							{
								ccurve = ccurve->next;
								continue;
							}
						}
						else
						{
							if((new_exposed_node == conn->node1 && 
								conn->sat1 != 2 && conn->sat1 != 3) ||
								(new_exposed_node == conn->node2 && 
								conn->sat2 != 2 && conn->sat2 != 3))
							{
								ccurve = ccurve->next;
								continue;
							}
						}
					}
					else
					{
						if(ccurve->connp0->conn->sat2 == 0 || ccurve->connp0->conn->sat2 == 1)
						{
							if((new_exposed_node == conn->node1 && 
								conn->sat1 != 0 && conn->sat1 != 1) ||
								(new_exposed_node == conn->node2 && 
								conn->sat2 != 0 && conn->sat2 != 1))
							{
								ccurve = ccurve->next;
								continue;
							}
						}
						else
						{
							if((new_exposed_node == conn->node1 && 
								conn->sat1 != 2 && conn->sat1 != 3) ||
								(new_exposed_node == conn->node2 && 
								conn->sat2 != 2 && conn->sat2 != 3))
							{
								ccurve = ccurve->next;
								continue;
							}
						}
					}
					
					
					// delete the verticies in all the conns in this curve, 
					// as it may be getting a new texture

					delete_all_verticies_in_curve(ccurve);


					// combine attached_curve and ccurve at new_exposed_node

					switch(exposed_node_end)
					{
					case 0:

						cconnp = ccurve->connp0;

						while(cconnp)
						{
							temp = attached_curve->connp0;
							attached_curve->connp0 = malloc(sizeof(struct conn_pointer_t));
							memcpy(attached_curve->connp0, cconnp, sizeof(struct conn_pointer_t));
							attached_curve->connp0->next = temp;

							attached_curve->connp0->conn->orientation = 
								!attached_curve->connp0->conn->orientation;

							cconnp = cconnp->next;
						}

						break;


					case 1:

						lconnp = get_last_connp(attached_curve->connp0);
						lconnp->next = ccurve->connp0;
						ccurve->connp0 = NULL;
	
						break;
					}

					free_curve(ccurve);
					
					break;
				}


				// check the last node on this curve

				lconnp = get_last_connp(ccurve->connp0);

				if((!lconnp->conn->orientation && lconnp->conn->node2 == new_exposed_node) || 
					(lconnp->conn->orientation && lconnp->conn->node1 == new_exposed_node))
				{
					// check the new connection doesn't attach at a passing through sat
					
					if(!lconnp->conn->orientation)
					{
						if(lconnp->conn->sat2 == 0 || lconnp->conn->sat2 == 1)
						{
							if((new_exposed_node == conn->node1 && 
								conn->sat1 != 0 && conn->sat1 != 1) ||
								(new_exposed_node == conn->node2 && 
								conn->sat2 != 0 && conn->sat2 != 1))
							{
								ccurve = ccurve->next;
								continue;
							}
						}
						else
						{
							if((new_exposed_node == conn->node1 && 
								conn->sat1 != 2 && conn->sat1 != 3) ||
								(new_exposed_node == conn->node2 && 
								conn->sat2 != 2 && conn->sat2 != 3))
							{
								ccurve = ccurve->next;
								continue;
							}
						}
					}
					else
					{
						if(lconnp->conn->sat1 == 0 || lconnp->conn->sat1 == 1)
						{
							if((new_exposed_node == conn->node1 && 
								conn->sat1 != 0 && conn->sat1 != 1) ||
								(new_exposed_node == conn->node2 && 
								conn->sat2 != 0 && conn->sat2 != 1))
							{
								ccurve = ccurve->next;
								continue;
							}
						}
						else
						{
							if((new_exposed_node == conn->node1 && 
								conn->sat1 != 2 && conn->sat1 != 3) ||
								(new_exposed_node == conn->node2 && 
								conn->sat2 != 2 && conn->sat2 != 3))
							{
								ccurve = ccurve->next;
								continue;
							}
						}
					}
					
					
					// delete the verticies in all the conns in this curve, 
					// as it may be getting a new texture

					delete_all_verticies_in_curve(ccurve);


					// combine attached_curve and ccurve at new_exposed_node

					switch(exposed_node_end)
					{
					case 0:

						lconnp->next = attached_curve->connp0;
						attached_curve->connp0 = ccurve->connp0;
						ccurve->connp0 = NULL;

						break;


					case 1:

						while(ccurve->connp0)
						{
							lconnp = get_last_connp(ccurve->connp0);

							lconnp->conn->orientation = !lconnp->conn->orientation;
	
							LL_ADD_TAIL(struct conn_pointer_t, &attached_curve->connp0, lconnp);
							LL_REMOVE(struct conn_pointer_t, &ccurve->connp0, lconnp);
						}
					}

					free_curve(ccurve);
					
					break;
				}
			}

			ccurve = ccurve->next;
		}


		// invalidate squished_textures on all conns on this curve

		struct conn_pointer_t *cconnp = attached_curve->connp0;

		while(cconnp)
		{
			LL_REMOVE_ALL(struct t_t, &cconnp->conn->t0);
			LL_REMOVE_ALL(struct t_t, &cconnp->conn->bigt0);
				
			generate_bigt_values(cconnp->conn);
			free(cconnp->conn->verts);
			cconnp->conn->verts = NULL;
			
			if(cconnp->conn->squished_texture)
			{
				free_surface(cconnp->conn->squished_texture);
				cconnp->conn->squished_texture = NULL;
			}
			
			delete_images_of_tiles_with_conn(cconnp->conn);
			cconnp->conn->tiled = 0;

			cconnp = cconnp->next;
		}
	}
}


int count_conns_in_curve(struct curve_t *curve)
{
	int conns = 0;

	struct conn_pointer_t *cconnp = curve->connp0;

	while(cconnp)
	{
		conns++;
		cconnp = cconnp->next;
	}

	return conns;
}


void remove_conn_from_its_curve(struct conn_t *conn)		// always called while not working
{
	// each conn should only be part of one curve
	// so find it

	struct curve_t *curve = get_curve(conn);
	assert(curve);



	// this is a little bit redundant because one of these conns will be deleted

	struct conn_pointer_t *cconnp = curve->connp0;

	while(cconnp)
	{
		if(cconnp->conn->squished_texture)
		{
			free_surface(cconnp->conn->squished_texture);
			cconnp->conn->squished_texture = NULL;
		}
		
		cconnp->conn->tiled = 0;
		remove_conn_from_tiles(cconnp->conn);

		cconnp = cconnp->next;
	}


	int conns = count_conns_in_curve(curve);


	switch(conns)
	{
	case 1:

		// the only conn in this curve is the one we are removing
		// so remove the whole curve

		free_curve(curve);

		break;


	case 2:

		// there are only two conns in this curve and there will only
		// be one left, so the curve does not have to be split

		remove_conn_pointer(&curve->connp0, conn);

		break;


	default:

		// there will be at least two conns left in this curve, so
		// it may have to be split

		if(curve->connp0->conn == conn)
		{
			// the conn is the first on the list, so we don't need to split

			remove_conn_pointer(&curve->connp0, conn);
			break;
		}

		struct conn_pointer_t *lconnp = get_last_connp(curve->connp0);

		if(lconnp->conn == conn)
		{
			// the conn is the last one on the list, so we don't need to split

			remove_conn_pointer(&curve->connp0, conn);
			break;
		}

		struct conn_pointer_t *cconnp = curve->connp0->next;

		while(cconnp->conn != conn)
			cconnp = cconnp->next;

		struct curve_t new_curve;

		new_curve.connp0 = cconnp->next;

		cconnp->next = NULL;

		remove_conn_pointer(&curve->connp0, conn);

		new_curve.pre_texture_surface = NULL;
		new_curve.texture_surface = duplicate_surface(curve->texture_surface);
		new_curve.texture_filename = new_string(curve->texture_filename);
		new_curve.texture_flip_vert = curve->texture_flip_vert;
		new_curve.texture_flip_horiz = curve->texture_flip_horiz;
		new_curve.fixed_reps = curve->fixed_reps;
		new_curve.reps = curve->reps;
		new_curve.texture_length = curve->texture_length;
		new_curve.pixel_offset_horiz = curve->pixel_offset_horiz;
		new_curve.pixel_offset_vert = curve->pixel_offset_vert;

		LL_ADD(struct curve_t, &curve0, &new_curve);

		break;
	}
}


void delete_all_curves()
{
	struct curve_t *ccurve = curve0;

	while(ccurve)
	{
		LL_REMOVE_ALL(struct conn_pointer_t, &ccurve->connp0);
		free_surface(ccurve->pre_texture_surface);
		free_surface(ccurve->texture_surface);
		free_string(ccurve->texture_filename);
		
		ccurve = ccurve->next;
	}

	LL_REMOVE_ALL(struct curve_t, &curve0);

	curve0 = NULL;
}


void draw_curve_ends()
{
	struct curve_t *curve = curve0;
		
	while(curve)
	{
		// find out if wall is continuous loop
		
		struct conn_pointer_t *cconnp = curve->connp0;
			
		while(cconnp->next)
			cconnp = cconnp->next;
		
		int loop = 0;
		
		switch(curve->connp0->conn->orientation)
		{
		case 0:
			switch(cconnp->conn->orientation)
			{
			case 0:
				if(curve->connp0->conn->node1 == cconnp->conn->node2)
					loop = 1;
				break;
			
			case 1:
				if(curve->connp0->conn->node1 == cconnp->conn->node1)
					loop = 1;
				break;
			}
			break;
		
		case 1:
			switch(cconnp->conn->orientation)
			{
			case 0:
				if(curve->connp0->conn->node2 == cconnp->conn->node2)
					loop = 1;
				break;
			
			case 1:
				if(curve->connp0->conn->node2 == cconnp->conn->node1)
					loop = 1;
				break;
			}
			break;
		}
		
		if(!loop)
		{
			struct conn_t *conn = curve->connp0->conn;
			
			struct node_t *node, *node2;
			int sat;
			int left1, right1, left2, right2;
			
			if(!conn->orientation)
			{
				node = conn->node1;
				node2 = conn->node2;
				sat = conn->sat1;
			}
			else
			{
				node = conn->node2;
				node2 = conn->node1;
				sat = conn->sat2;
			}
	
			get_width_sats(conn, &left1, &right1, &left2, &right2);
			double deltax = 0.0, deltay = 0.0, leftx, lefty, rightx, righty;
			
			switch(conn->type)
			{
			case CONN_TYPE_STRAIGHT:
				deltax = node2->x - node->x;
				deltay = node2->y - node->y;
				break;
				
			case CONN_TYPE_BEZIER:
				deltax = node->sats[sat].x;
				deltay = node->sats[sat].y;
				break;
			}
			
	
			double length = hypot(deltax, deltay);
	
			deltax /= length;
			deltay /= length;
			
			leftx = deltax * node->width[left1];
			lefty = deltay * node->width[left1];
			
			rightx = deltax * node->width[right1];
			righty = deltay * node->width[right1];
			
			if(node->num_conns == 3)
			{
				switch(sat)
				{
				case 0: sat = 1; break;
				case 1: sat = 0; break;
				case 2: sat = 3; break;
				case 3: sat = 2; break;
				}
			}
			
			double x = node->effective_x[sat];
			double y = node->effective_y[sat];
			
			draw_world_clipped_line(x - lefty, y + leftx, x + righty, y - rightx);
			
			struct conn_pointer_t *connp = get_last_connp(curve->connp0);
			conn = connp->conn;
			
			if(!conn->orientation)
			{
				node = conn->node2;
				node2 = conn->node1;
				sat = conn->sat2;
			}
			else
			{
				node = conn->node1;
				node2 = conn->node2;
				sat = conn->sat1;
			}
			
			get_width_sats(conn, &left1, &right1, &left2, &right2);
			
			switch(conn->type)
			{
			case CONN_TYPE_STRAIGHT:
				deltax = node->x - node2->x;
				deltay = node->y - node2->y;
				break;
				
			case CONN_TYPE_BEZIER:
				deltax = node->sats[sat].x;
				deltay = node->sats[sat].y;
				break;
			}
			
			length = hypot(deltax, deltay);
			
			deltax /= length;
			deltay /= length;
			
			leftx = deltax * node->width[left1];
			lefty = deltay * node->width[left1];
			
			rightx = deltax * node->width[right1];
			righty = deltay * node->width[right1];
			
			if(node->num_conns == 3)
			{
				switch(sat)
				{
				case 0: sat = 1; break;
				case 1: sat = 0; break;
				case 2: sat = 3; break;
				case 3: sat = 2; break;
				}
			}
			
			x = node->effective_x[sat];
			y = node->effective_y[sat];
			
			draw_world_clipped_line(x - lefty, y + leftx, x + righty, y - rightx);
		}
		
		curve = curve->next;
	}
}


void draw_curve_outlines()
{
	struct curve_t *ccurve = curve0;

	while(ccurve)
	{
		struct conn_pointer_t *cconnp = ccurve->connp0;

		while(cconnp)
		{
			draw_conn(cconnp->conn);
			cconnp = cconnp->next;
		}

		ccurve = ccurve->next;
		
		draw_curve_ends(ccurve);
	}
}


double get_curve_length(struct curve_t *curve)
{
	struct conn_pointer_t *cconnp = curve->connp0;
	
	double length = 0.0;
	
	while(cconnp)
	{
		length += cconnp->conn->bigt_length;
		cconnp = cconnp->next;
	}
	
	return length;
}


uint32_t count_curves()
{
	uint32_t num_curves = 0;
	struct curve_t *ccurve = curve0;

	while(ccurve)
	{
		num_curves++;
		ccurve = ccurve->next;
	}

	return num_curves;
}


void update_curve_surface(struct curve_t *curve)		// always called when not working
{
	if(!curve->texture_filename)
		return;
	
	if(!curve->texture_filename->text)
		return;
	
	if(!curve->pre_texture_surface)
	{
		struct string_t *string = arb_rel2abs(curve->texture_filename->text, map_path->text);
		curve->pre_texture_surface = read_gdk_pixbuf_surface_as_24bitalpha8bit(string->text);
		free_string(string);
	}
	
	if(!curve->pre_texture_surface)
		return;
	
	if(!curve->texture_surface)
	{
		curve->texture_surface = duplicate_surface(curve->pre_texture_surface);
		
		if(curve->texture_flip_horiz)
			surface_flip_horiz(curve->texture_surface);
		
		if(curve->texture_flip_vert)
			surface_flip_vert(curve->texture_surface);

		if(curve->texture_rotate_left)
			surface_rotate_left(curve->texture_surface);
		
		if(curve->texture_rotate_right)
			surface_rotate_right(curve->texture_surface);

		if(curve->pixel_offset_horiz != 0)
			surface_slide_horiz(curve->texture_surface, curve->pixel_offset_horiz);
		
		if(curve->pixel_offset_vert != 0)
			surface_slide_vert(curve->texture_surface, curve->pixel_offset_vert);
	}
}


void gzwrite_curves(gzFile file)
{
	uint32_t num_curves = count_curves();
	gzwrite(file, &num_curves, 4);

	struct curve_t *ccurve = curve0;
	uint32_t curve_index = 0;

	while(ccurve)
	{
		ccurve->index = curve_index++;
		
		gzwrite_conn_pointer_list(file, ccurve->connp0);
		
		gzwrite_string(file, ccurve->texture_filename);
		
		gzwrite(file, &ccurve->red, 1);
		gzwrite(file, &ccurve->green, 1);
		gzwrite(file, &ccurve->blue, 1);
		gzwrite(file, &ccurve->alpha, 1);
		
		gzwrite(file, &ccurve->fill_type, 1);
		
		gzwrite(file, &ccurve->texture_flip_horiz, 1);
		gzwrite(file, &ccurve->texture_flip_vert, 1);
		gzwrite(file, &ccurve->texture_rotate_left, 1);
		gzwrite(file, &ccurve->texture_rotate_right, 1);
		
		gzwrite(file, &ccurve->fixed_reps, 1);
		
		gzwrite(file, &ccurve->reps, 4);
		gzwrite(file, &ccurve->texture_length, 4);
		gzwrite(file, &ccurve->pixel_offset_horiz, 4);
		gzwrite(file, &ccurve->pixel_offset_vert, 4);
		
		ccurve = ccurve->next;
	}
}


int gzread_curves(gzFile file)
{
	uint32_t num_curves;
	gzread(file, &num_curves, 4);

	uint32_t ccurve_index = 0;

	while(ccurve_index < num_curves)
	{
		struct curve_t curve;
		memset(&curve, 0, sizeof(struct curve_t));
		
		curve.index = ccurve_index;
		
		curve.connp0 = gzread_conn_pointer_list(file);
		if(!curve.connp0)
			goto error;
		
		curve.texture_filename = gzread_string(file);
		if(!curve.texture_filename)
			goto error;
		
		if(gzread(file, &curve.red, 1) != 1)
			goto error;
		
		if(gzread(file, &curve.green, 1) != 1)
			goto error;
		
		if(gzread(file, &curve.blue, 1) != 1)
			goto error;
		
		if(gzread(file, &curve.alpha, 1) != 1)
			goto error;
		
		if(gzread(file, &curve.fill_type, 1) != 1)
			goto error;
				
		
		
		if(gzread(file, &curve.texture_flip_horiz, 1) != 1)
			goto error;
		
		if(gzread(file, &curve.texture_flip_vert, 1) != 1)
			goto error;
		
		if(gzread(file, &curve.texture_rotate_left, 1) != 1)
			goto error;
		
		if(gzread(file, &curve.texture_rotate_right, 1) != 1)
			goto error;
		

		if(gzread(file, &curve.fixed_reps, 1) != 1)
			goto error;
		
		if(gzread(file, &curve.reps, 4) != 4)
			goto error;
		
		if(gzread(file, &curve.texture_length, 4) != 4)
			goto error;
		
		if(gzread(file, &curve.pixel_offset_horiz, 4) != 4)
			goto error;
		
		if(gzread(file, &curve.pixel_offset_vert, 4) != 4)
			goto error;
		

		update_curve_surface(&curve);
			
		LL_ADD(struct curve_t, &curve0, &curve);
		ccurve_index++;
	}

	return 1;

error:

	return 0;
}


struct curve_t *get_curve_from_index(int index)
{
	struct curve_t *ccurve = curve0;

	while(ccurve)
	{
		if(ccurve->index == index)
			return ccurve;

		ccurve = ccurve->next;
	}

	return NULL;
}


void invalidate_curve(struct curve_t *curve)		// always called when not working
{
	struct conn_pointer_t *connp = curve->connp0;

	while(connp)
	{
		// new squished_texture and tiles

		free(connp->conn->verts);
		connp->conn->verts = NULL;
				
		free_surface(connp->conn->squished_texture);
		connp->conn->squished_texture = NULL;

		remove_conn_from_tiles(connp->conn);		// a bit extreme in some circumstances
		connp->conn->tiled = 0;
		
//		delete_images_of_tiles_with_conn(connp->conn);	// should only do this if not new texture
		connp = connp->next;
	}
}


void on_wall_solid_colorpicker_color_set(GtkWidget *colorpicker, 
	guint red, guint green, guint blue, guint alpha, gpointer user_data)
{
	GtkWidget *dialog = gtk_widget_get_toplevel(GTK_WIDGET(colorpicker));
	struct curve_t *curve = g_object_get_data(G_OBJECT(dialog), "curve");
	
	stop_working();
	
	curve->red = red >> 8;
	curve->green = green >> 8;
	curve->blue = blue >> 8;
	curve->alpha = alpha >> 8;
	
	invalidate_curve(curve);
	update_client_area();
	start_working();
}


void on_wall_solid_radiobutton_toggled(GtkToggleButton *togglebutton, gpointer user_data)
{
	gboolean sensitive = gtk_toggle_button_get_active(togglebutton);
	GtkWidget *dialog = gtk_widget_get_toplevel(GTK_WIDGET(togglebutton));
	gtk_widget_set_sensitive(GTK_WIDGET(g_object_get_data(G_OBJECT(dialog), 
		"solid_hbox")), sensitive);
	
	struct curve_t *curve = g_object_get_data(G_OBJECT(dialog), "curve");
	
	stop_working();

	if(sensitive)
		curve->fill_type = CURVE_SOLID;
	else
		curve->fill_type = CURVE_TEXTURE;
	
	invalidate_curve(curve);
	update_client_area();
	start_working();
}


void on_wall_flip_horiz_checkbutton_toggled(GtkToggleButton *togglebutton, gpointer user_data)
{
	GtkWidget *dialog = gtk_widget_get_toplevel(GTK_WIDGET(togglebutton));
	struct curve_t *curve = g_object_get_data(G_OBJECT(dialog), "curve");

	stop_working();
	
	curve->texture_flip_horiz = gtk_toggle_button_get_active(togglebutton);
	
	free_surface(curve->texture_surface);
	curve->texture_surface = NULL;
	
	update_curve_surface(curve);
	invalidate_curve(curve);
	
	update_client_area();
	
	start_working();
}


void on_wall_flip_vert_checkbutton_toggled(GtkToggleButton *togglebutton, gpointer user_data)
{
	GtkWidget *dialog = gtk_widget_get_toplevel(GTK_WIDGET(togglebutton));
	struct curve_t *curve = g_object_get_data(G_OBJECT(dialog), "curve");

	stop_working();
	
	curve->texture_flip_vert = gtk_toggle_button_get_active(togglebutton);
	
	free_surface(curve->texture_surface);
	curve->texture_surface = NULL;
	
	update_curve_surface(curve);
	invalidate_curve(curve);
	
	update_client_area();
	
	start_working();
}


void on_wall_rotate_checkbutton_toggled(GtkToggleButton *togglebutton, gpointer user_data)
{
	gboolean sensitive = gtk_toggle_button_get_active(togglebutton);
	GtkWidget *dialog = gtk_widget_get_toplevel(GTK_WIDGET(togglebutton));
	gtk_widget_set_sensitive(GTK_WIDGET(g_object_get_data(G_OBJECT(dialog), 
		"rotate_hbox")), sensitive);
	
	struct curve_t *curve = g_object_get_data(G_OBJECT(dialog), "curve");
		
	stop_working();
	
	if(sensitive)
	{
		if(gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(g_object_get_data(G_OBJECT(dialog), 
			"rotate_left_radiobutton"))))
		{
			curve->texture_rotate_left = 1;
			curve->texture_rotate_right = 0;
		}
		else
		{
			curve->texture_rotate_left = 0;
			curve->texture_rotate_right = 1;
		}
	}
	else
	{
		curve->texture_rotate_left = 0;
		curve->texture_rotate_right = 0;
	}
	
	free_surface(curve->texture_surface);
	curve->texture_surface = NULL;
	
	update_curve_surface(curve);
	invalidate_curve(curve);
	
	update_client_area();

	start_working();
}


void on_wall_rotate_left_radiobutton_toggled(GtkToggleButton *togglebutton, gpointer user_data)
{
	gboolean on = gtk_toggle_button_get_active(togglebutton);
	GtkWidget *dialog = gtk_widget_get_toplevel(GTK_WIDGET(togglebutton));
	struct curve_t *curve = g_object_get_data(G_OBJECT(dialog), "curve");
	
	if(on)
	{
		stop_working();
		curve->texture_rotate_left = 1;
		curve->texture_rotate_right = 0;
		
		free_surface(curve->texture_surface);
		curve->texture_surface = NULL;
		
		update_curve_surface(curve);
		invalidate_curve(curve);
		
		update_client_area();
		
		start_working();
	}
}


void on_wall_rotate_right_radiobutton_toggled(GtkToggleButton *togglebutton, gpointer user_data)
{
	gboolean on = gtk_toggle_button_get_active(togglebutton);
	GtkWidget *dialog = gtk_widget_get_toplevel(GTK_WIDGET(togglebutton));
	struct curve_t *curve = g_object_get_data(G_OBJECT(dialog), "curve");
	
	if(on)
	{
		stop_working();
		curve->texture_rotate_left = 0;
		curve->texture_rotate_right = 1;
			
		free_surface(curve->texture_surface);
		curve->texture_surface = NULL;
		
		update_curve_surface(curve);
		invalidate_curve(curve);
		
		update_client_area();

		start_working();
	}
}


void on_wall_offset_horiz_spinbutton_value_changed(GtkSpinButton *spinbutton, gpointer user_data)
{
	GtkWidget *dialog = gtk_widget_get_toplevel(GTK_WIDGET(spinbutton));
	struct curve_t *curve = g_object_get_data(G_OBJECT(dialog), "curve");

	stop_working();
	
	curve->pixel_offset_horiz = gtk_spin_button_get_value_as_int(spinbutton);
	
	free_surface(curve->texture_surface);
	curve->texture_surface = NULL;
	
	update_curve_surface(curve);
	invalidate_curve(curve);
	
	update_client_area();
	
	start_working();
}


void on_wall_offset_vert_spinbutton_value_changed(GtkSpinButton *spinbutton, gpointer user_data)
{
	GtkWidget *dialog = gtk_widget_get_toplevel(GTK_WIDGET(spinbutton));
	struct curve_t *curve = g_object_get_data(G_OBJECT(dialog), "curve");

	stop_working();
	
	curve->pixel_offset_vert = gtk_spin_button_get_value_as_int(spinbutton);
	
	free_surface(curve->texture_surface);
	curve->texture_surface = NULL;
	
	update_curve_surface(curve);
	invalidate_curve(curve);
	
	update_client_area();
	
	start_working();
}


void on_wall_fixed_width_radiobutton_toggled(GtkToggleButton *togglebutton, gpointer user_data)
{
	gboolean sensitive = gtk_toggle_button_get_active(togglebutton);
	GtkWidget *dialog = gtk_widget_get_toplevel(GTK_WIDGET(togglebutton));
	gtk_widget_set_sensitive(GTK_WIDGET(g_object_get_data(G_OBJECT(dialog), 
		"fixed_width_spinbutton")), sensitive);

	struct curve_t *curve = g_object_get_data(G_OBJECT(dialog), "curve");
		
	stop_working();
	
	curve->fixed_reps = !sensitive;
	
	invalidate_curve(curve);
	
	start_working();
}


void on_wall_fixed_width_spinbutton_value_changed(GtkSpinButton *spinbutton, gpointer user_data)
{
	GtkWidget *dialog = gtk_widget_get_toplevel(GTK_WIDGET(spinbutton));
	struct curve_t *curve = g_object_get_data(G_OBJECT(dialog), "curve");

	stop_working();
	
	curve->texture_length = gtk_spin_button_get_value(spinbutton);
	
	invalidate_curve(curve);
	
	start_working();
}


void on_wall_fixed_reps_radiobutton_toggled(GtkToggleButton *togglebutton, gpointer user_data)
{
	gboolean sensitive = gtk_toggle_button_get_active(togglebutton);
	GtkWidget *dialog = gtk_widget_get_toplevel(GTK_WIDGET(togglebutton));
	gtk_widget_set_sensitive(GTK_WIDGET(g_object_get_data(G_OBJECT(dialog), 
		"fixed_reps_spinbutton")), sensitive);
	
	struct curve_t *curve = g_object_get_data(G_OBJECT(dialog), "curve");
		
	stop_working();
	
	curve->fixed_reps = sensitive;
	
	invalidate_curve(curve);
	
	start_working();
}


void on_wall_fixed_reps_spinbutton_value_changed(GtkSpinButton *spinbutton, gpointer user_data)
{
	GtkWidget *dialog = gtk_widget_get_toplevel(GTK_WIDGET(spinbutton));
	struct curve_t *curve = g_object_get_data(G_OBJECT(dialog), "curve");
		
	stop_working();
	
	curve->reps = gtk_spin_button_get_value_as_int(spinbutton);
	
	invalidate_curve(curve);
	
	start_working();
}


void on_wall_texture_radiobutton_toggled (GtkToggleButton *togglebutton, gpointer user_data)
{
	gboolean sensitive = gtk_toggle_button_get_active(togglebutton);
	GtkWidget *dialog = gtk_widget_get_toplevel(GTK_WIDGET(togglebutton));
	gtk_widget_set_sensitive(GTK_WIDGET(g_object_get_data(G_OBJECT(dialog), 
		"texture_vbox")), sensitive);
	
	struct curve_t *curve = g_object_get_data(G_OBJECT(dialog), "curve");
	
	stop_working();

	if(sensitive)
		curve->fill_type = CURVE_TEXTURE;
	else
		curve->fill_type = CURVE_SOLID;
	
	free_surface(curve->pre_texture_surface);
	curve->pre_texture_surface = NULL;
	
	free_surface(curve->texture_surface);
	curve->texture_surface = NULL;
	
	update_curve_surface(curve);
	invalidate_curve(curve);
	
	update_client_area();

	start_working();
}


void on_wall_width_lock_checkbutton_toggled(GtkToggleButton *togglebutton, gpointer user_data)
{
	gboolean sensitive = gtk_toggle_button_get_active(togglebutton);
	GtkWidget *dialog = gtk_widget_get_toplevel(GTK_WIDGET(togglebutton));
	gtk_widget_set_sensitive(GTK_WIDGET(g_object_get_data(G_OBJECT(dialog), 
		"width_lock_spinbutton")), sensitive);

	struct curve_t *curve = g_object_get_data(G_OBJECT(dialog), "curve");
		
	stop_working();
	
	curve->width_lock_on = sensitive;
	
	start_working();
}


void on_wall_width_lock_spinbutton_value_changed(GtkSpinButton *spinbutton, gpointer user_data)
{
	GtkWidget *dialog = gtk_widget_get_toplevel(GTK_WIDGET(spinbutton));
	struct curve_t *curve = g_object_get_data(G_OBJECT(dialog), "curve");

	stop_working();
	
	curve->width_lock = gtk_spin_button_get_value(spinbutton);
	
	start_working();
}


void on_wall_texture_entry_changed(GtkEditable *editable, gpointer user_data)
{
	GtkWidget *dialog = gtk_widget_get_toplevel(GTK_WIDGET(editable));
	struct curve_t *curve = g_object_get_data(G_OBJECT(dialog), "curve");
		
	stop_working();
	
	free_string(curve->texture_filename);
	
	gchar *strval;
	g_object_get(G_OBJECT(editable), "text", &strval, NULL);
	
	if(map_filename->text[0])
		curve->texture_filename = arb_abs2rel(strval, map_path->text);
	else
		curve->texture_filename = new_string_text(strval);
	
	g_free(strval);
	
	free_surface(curve->pre_texture_surface);
	curve->pre_texture_surface = NULL;
	
	free_surface(curve->texture_surface);
	curve->texture_surface = NULL;
	
	update_curve_surface(curve);
	invalidate_curve(curve);
	
	update_client_area();
	
	start_working();
}


void on_wall_properties_dialog_destroy(GtkObject *object, gpointer user_data)
{
	gtk_main_quit();
}


void run_wall_properties_dialog(void *menu, struct curve_t *curve)
{
	GtkWidget *dialog = create_wall_properties_dialog();
	
	g_object_set_data(G_OBJECT(dialog), "curve", curve);
	
	gtk_window_set_transient_for(GTK_WINDOW(dialog), GTK_WINDOW(window));
	gtk_window_set_position(GTK_WINDOW(dialog), GTK_WIN_POS_CENTER_ON_PARENT);
	
	if(curve->fill_type == CURVE_SOLID)
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(g_object_get_data(G_OBJECT(dialog), 
			"solid_radiobutton")), TRUE);
	else
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(g_object_get_data(G_OBJECT(dialog), 
			"texture_radiobutton")), TRUE);

	{ GdkColor _c = {0, curve->red<<8, curve->green<<8, curve->blue<<8}; gtk_color_button_set_color(GTK_COLOR_BUTTON(g_object_get_data(G_OBJECT(dialog), 
		"solid_colorpicker")), &_c); gtk_color_button_set_alpha(GTK_COLOR_BUTTON(g_object_get_data(G_OBJECT(dialog), 
		"solid_colorpicker")), curve->alpha<<8); }
	
	if(curve->texture_filename)
	{
		gtk_entry_set_text(GTK_ENTRY(g_object_get_data(G_OBJECT(dialog), 
			"texture_entry")), curve->texture_filename->text);
	}
	
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(g_object_get_data(G_OBJECT(dialog), 
		"flip_horiz_checkbutton")), curve->texture_flip_horiz);
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(g_object_get_data(G_OBJECT(dialog), 
		"flip_vert_checkbutton")), curve->texture_flip_vert);
	
	if(curve->texture_rotate_left)
	{
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(g_object_get_data(G_OBJECT(dialog), 
			"rotate_checkbutton")), 1);
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(g_object_get_data(G_OBJECT(dialog), 
			"rotate_left_radiobutton")), 1);
	}

	if(curve->texture_rotate_right)
	{
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(g_object_get_data(G_OBJECT(dialog), 
			"rotate_checkbutton")), 1);
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(g_object_get_data(G_OBJECT(dialog), 
			"rotate_right_radiobutton")), 1);
	}

	gtk_spin_button_set_value(GTK_SPIN_BUTTON(g_object_get_data(G_OBJECT(dialog), 
		"offset_horiz_spinbutton")), curve->pixel_offset_horiz);
	
	gtk_spin_button_set_value(GTK_SPIN_BUTTON(g_object_get_data(G_OBJECT(dialog), 
		"offset_vert_spinbutton")), curve->pixel_offset_vert);
	
	if(curve->fixed_reps)
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(g_object_get_data(G_OBJECT(dialog), 
			"fixed_reps_radiobutton")), 1);
	else
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(g_object_get_data(G_OBJECT(dialog), 
			"fixed_width_radiobutton")), 1);
	
	gtk_spin_button_set_value(GTK_SPIN_BUTTON(g_object_get_data(G_OBJECT(dialog), 
		"fixed_reps_spinbutton")), curve->reps);
	
	gtk_spin_button_set_value(GTK_SPIN_BUTTON(g_object_get_data(G_OBJECT(dialog), 
		"fixed_width_spinbutton")), curve->texture_length);
	
	if(curve->width_lock_on)
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(g_object_get_data(G_OBJECT(dialog), 
			"width_lock_checkbutton")), 1);
	
	gtk_spin_button_set_value(GTK_SPIN_BUTTON(g_object_get_data(G_OBJECT(dialog), 
		"width_lock_spinbutton")), curve->width_lock);
	
	gtk_widget_show(dialog);
	gtk_main();
}



void make_wall_texture_paths_relative()
{
	struct curve_t *ccurve = curve0;
		
	while(ccurve)
	{
		if(ccurve->texture_filename)
		{
			struct string_t *s = arb_abs2rel(ccurve->texture_filename->text, map_path->text);
			free_string(ccurve->texture_filename);
			ccurve->texture_filename = s;
		}
		
		ccurve = ccurve->next;
	}
}




gboolean
on_wall_offset_horiz_spinbutton_button_press_event
                                        (GtkWidget       *widget,
                                        GdkEventButton  *event,
                                        gpointer         user_data)
{

  return FALSE;
}


gboolean
on_wall_offset_horiz_spinbutton_button_release_event
                                        (GtkWidget       *widget,
                                        GdkEventButton  *event,
                                        gpointer         user_data)
{

  return FALSE;
}


gboolean
on_wall_offset_vert_spinbutton_button_press_event
                                        (GtkWidget       *widget,
                                        GdkEventButton  *event,
                                        gpointer         user_data)
{

  return FALSE;
}


gboolean
on_wall_offset_vert_spinbutton_button_release_event
                                        (GtkWidget       *widget,
                                        GdkEventButton  *event,
                                        gpointer         user_data)
{

  return FALSE;
}


gboolean
on_wall_fixed_reps_spinbutton_button_press_event
                                        (GtkWidget       *widget,
                                        GdkEventButton  *event,
                                        gpointer         user_data)
{

  return FALSE;
}


gboolean
on_wall_fixed_reps_spinbutton_button_release_event
                                        (GtkWidget       *widget,
                                        GdkEventButton  *event,
                                        gpointer         user_data)
{

  return FALSE;
}


gboolean
on_wall_fixed_width_spinbutton_button_press_event
                                        (GtkWidget       *widget,
                                        GdkEventButton  *event,
                                        gpointer         user_data)
{

  return FALSE;
}


gboolean
on_wall_fixed_width_spinbutton_button_release_event
                                        (GtkWidget       *widget,
                                        GdkEventButton  *event,
                                        gpointer         user_data)
{

  return FALSE;
}


gboolean
on_wall_width_lock_spinbutton_button_press_event
                                        (GtkWidget       *widget,
                                        GdkEventButton  *event,
                                        gpointer         user_data)
{

  return FALSE;
}


gboolean
on_wall_width_lock_spinbutton_button_release_event
                                        (GtkWidget       *widget,
                                        GdkEventButton  *event,
                                        gpointer         user_data)
{

  return FALSE;
}


void menu_insert_point(GtkWidget *menu, struct curve_t *curve)
{
	insert_point(curve, right_button_down_worldx, right_button_down_worldy);
	update_client_area();
}


void menu_remove_wall(GtkWidget *menu, struct curve_t *curve)
{
	stop_working();
	
	struct conn_pointer_t *connp = curve->connp0, *next;
	struct node_t *node1, *node2;
		
	if(!connp->conn->orientation)
		node1 = connp->conn->node1;
	else
		node1 = connp->conn->node2;
	
	int first = 1;
	
	while(connp)
	{
		if(!connp->conn->orientation)
			node2 = connp->conn->node2;
		else
			node2 = connp->conn->node1;
		
		next = connp->next;
		delete_conn(connp->conn);
		connp = next;
		
		if(first)
		{
			invalidate_node(node1);
			LL_REMOVE(struct node_t, &node0, node1);
			node_deleted(node1);
			first = 0;
		}

		invalidate_node(node2);
		LL_REMOVE(struct node_t, &node0, node2);
		node_deleted(node2);
	}
	
	invalidate_bsp_tree();
	start_working();
	
	update_client_area();
}


void run_curve_menu(struct curve_t *curve)
{
	GtkWidget *menu;
	GtkWidget *menu_items;
	menu = gtk_menu_new();

	menu_items = gtk_menu_item_new_with_label("Wall Properties");
	gtk_signal_connect(GTK_OBJECT(menu_items), "activate", 
		GTK_SIGNAL_FUNC(run_wall_properties_dialog), curve);
	gtk_menu_append(GTK_MENU(menu), menu_items);
	gtk_widget_show(menu_items);

/*	menu_items = gtk_menu_item_new_with_label("Insert Point");
	gtk_signal_connect(GTK_OBJECT(menu_items), "activate", 
		GTK_SIGNAL_FUNC(menu_insert_point), curve);
	gtk_menu_append(GTK_MENU(menu), menu_items);
	gtk_widget_show(menu_items);
*/
	
	menu_items = gtk_menu_item_new_with_label("Remove Wall");
	gtk_signal_connect(GTK_OBJECT(menu_items), "activate", 
		GTK_SIGNAL_FUNC(menu_remove_wall), curve);
	gtk_menu_append(GTK_MENU(menu), menu_items);
	gtk_widget_show(menu_items);
	
	gtk_menu_popup (GTK_MENU(menu), NULL, NULL, (GtkMenuPositionFunc)(NULL), NULL, 0, 0);
}
