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

#define USE_GDK_PIXBUF

#include "prefix.h"

#include "llist.h"
#include "stringbuf.h"
#include "vertex.h"
#include "resource.h"
#include "gsub.h"
#include "nodes.h"
#include "map.h"
#include "conns.h"
#include "points.h"
#include "tiles.h"
#include "worker.h"
#include "interface.h"
#include "main_lock.h"
#include "main.h"
#include "glade.h"
#include "bsp.h"


#define SATELLITE_THRESHOLD 10
#define NODE_THRESHOLD 15

#define SATELLITE_THRESHOLD_SQUARED (SATELLITE_THRESHOLD * SATELLITE_THRESHOLD)
#define NODE_THRESHOLD_SQUARED (NODE_THRESHOLD * NODE_THRESHOLD)

struct node_t *node0 = NULL;
struct node_pointer_t *hover_nodes = NULL;

struct surface_t *s_node, *s_vectsat, *s_widthsat;


struct node_t *new_node()
{
	struct node_t *node;
	LL_CALLOC(struct node_t, &node0, &node);
	return node;
}


int add_node_pointer(struct node_pointer_t **nodep0, struct node_t *node)
{
	if(!nodep0)
		return 0;
	
	if(!*nodep0)
	{
		*nodep0 = malloc(sizeof(struct node_pointer_t));
		(*nodep0)->node = node;
		(*nodep0)->next = NULL;
	}
	else
	{
		struct node_pointer_t *cnodep = *nodep0;

		while(cnodep->next)
		{
			if(cnodep->node == node)
				return 0;

			cnodep = cnodep->next;
		}

		if(cnodep->node != node)
		{
			cnodep->next = malloc(sizeof(struct node_pointer_t));
			cnodep = cnodep->next;
			cnodep->node = node;
			cnodep->next = NULL;
		}
		else
		{
			return 0;
		}
	}

	return 1;
}


void remove_node_pointer(struct node_pointer_t **nodep0, struct node_t *node)
{
	if(!nodep0)
		return;

	struct node_pointer_t *cnodep = *nodep0;

	while(cnodep)
	{
		if(cnodep->node == node)
		{
			LL_REMOVE(struct node_pointer_t, nodep0, cnodep);
			break;
		}

		cnodep = cnodep->next;
	}
}


int node_in_node_pointer_list(struct node_pointer_t *nodep0, struct node_t *node)
{
	struct node_pointer_t *cnodep = nodep0;
		
	while(cnodep)
	{
		if(cnodep->node == node)
			return 1;
		
		cnodep = cnodep->next;
	}
	
	return 0;
}


void insert_node(float x, float y)
{
	// no need to stop working
	
	map_saved = 0;

	struct node_t *cnode = new_node();

	cnode->x = x;
	cnode->y = y;

	cnode->texture_filename = NULL;
	cnode->pre_texture_surface = NULL;
	cnode->texture_surface = NULL;
	cnode->texture_flip_horiz = 0;
	cnode->texture_flip_vert = 0;
	cnode->texture_rotate_left = 0;
	cnode->texture_rotate_right = 0;
	cnode->texture_verts = NULL;
	cnode->texture_tiled = 0;
	
	int p;
	for(p = 0; p < 4; p++)
	{
		cnode->width[p] = 20.0;
		cnode->effective_x[p] = x;
		cnode->effective_y[p] = y;
	}
	
	cnode->sats[0].x = 0.0;
	cnode->sats[0].y = 0.0;
	cnode->sats[1].x = 0.0;
	cnode->sats[1].y = 0.0;
	cnode->sats[2].x = 0.0;
	cnode->sats[2].y = 0.0;
	cnode->sats[3].x = 0.0;
	cnode->sats[3].y = 0.0;
}


void invalidate_node(struct node_t *node)
{
	free(node->texture_verts);
	node->texture_verts = NULL;
	remove_node_from_tiles(node);
	node->texture_tiled = 0;
}


void delete_node(struct node_t *node)
{
	stop_working();
	invalidate_node(node);
	delete_conns(node);
	LL_REMOVE(struct node_t, &node0, node);
	start_working();
	
	node_deleted(node);
}


void fix_satellites(struct node_t *node, uint8_t setsat)
{
	double hyp, theta, cos_theta, sin_theta;
	
	switch(setsat)
	{
	case 0:
		node->sats[1].x = -node->sats[0].x;
		node->sats[1].y = -node->sats[0].y;
		
		hyp = hypot(node->sats[2].x, node->sats[2].y);
			
		theta = atan2(node->sats[0].y, node->sats[0].x);
		
		sincos(theta - M_PI / 2.0, &sin_theta, &cos_theta);
		
		node->sats[2].x = hyp * cos_theta;
		node->sats[2].y = hyp * sin_theta;
			
		node->sats[3].x = -node->sats[2].x;
		node->sats[3].y = -node->sats[2].y;
		break;
		
	case 1:
		node->sats[0].x = -node->sats[1].x;
		node->sats[0].y = -node->sats[1].y;
		
		hyp = hypot(node->sats[2].x, node->sats[2].y);
		
		theta = atan2(node->sats[1].y, node->sats[1].x);
		
		sincos(theta + M_PI / 2.0, &sin_theta, &cos_theta);
		
		node->sats[2].x = hyp * cos_theta;
		node->sats[2].y = hyp * sin_theta;
		
		node->sats[3].x = -node->sats[2].x;
		node->sats[3].y = -node->sats[2].y;
		break;
		
	case 2:
		node->sats[3].x = -node->sats[2].x;
		node->sats[3].y = -node->sats[2].y;
		
		hyp = hypot(node->sats[0].x, node->sats[0].y);
		
		theta = atan2(node->sats[2].y, node->sats[2].x);
		
		sincos(theta + M_PI / 2.0, &sin_theta, &cos_theta);
		
		node->sats[0].x = hyp * cos_theta;
		node->sats[0].y = hyp * sin_theta;
		
		node->sats[1].x = -node->sats[0].x;
		node->sats[1].y = -node->sats[0].y;
		break;
		
	case 3:
		node->sats[2].x = -node->sats[3].x;
		node->sats[2].y = -node->sats[3].y;
		
		hyp = hypot(node->sats[0].x, node->sats[0].y);
		
		theta = atan2(node->sats[3].y, node->sats[3].x);
		
		sincos(theta - M_PI / 2.0, &sin_theta, &cos_theta);
		
		node->sats[0].x = hyp * cos_theta;
		node->sats[0].y = hyp * sin_theta;
		
		node->sats[1].x = -node->sats[0].x;
		node->sats[1].y = -node->sats[0].y;
		break;
	}
}


void straighten_from_node(struct node_t *node)
{
	struct conn_t *cconn = conn0;
		
	while(cconn)
	{
		if(cconn->type == CONN_TYPE_STRAIGHT)
		{
			double hyp, theta, sin_theta, cos_theta;;
			
			if(cconn->node1 == node)
			{
				hyp = hypot(node->sats[cconn->sat1].x, node->sats[cconn->sat1].y);
				
				theta = atan2(cconn->node2->y - node->y, cconn->node2->x - node->x);
				
				sincos(theta, &sin_theta, &cos_theta);
				
				node->sats[cconn->sat1].x = hyp * cos_theta;
				node->sats[cconn->sat1].y = hyp * sin_theta;
				
				fix_satellites(node, cconn->sat1);

				hyp = hypot(cconn->node2->sats[cconn->sat2].x, cconn->node2->sats[cconn->sat2].y);
				
				theta = atan2(node->y - cconn->node2->y, node->x - cconn->node2->x);
				
				sincos(theta, &sin_theta, &cos_theta);
				
				cconn->node2->sats[cconn->sat2].x = hyp * cos_theta;
				cconn->node2->sats[cconn->sat2].y = hyp * sin_theta;
				
				fix_satellites(cconn->node2, cconn->sat2);
				fix_conic_conns_from_node(cconn->node2);
				make_node_effective(cconn->node2);
			}
			else if(cconn->node2 == node)
			{
				hyp = hypot(node->sats[cconn->sat2].x, node->sats[cconn->sat2].y);
				
				theta = atan2(cconn->node1->y - node->y, cconn->node1->x - node->x);
				
				sincos(theta, &sin_theta, &cos_theta);
				
				node->sats[cconn->sat2].x = hyp * cos_theta;
				node->sats[cconn->sat2].y = hyp * sin_theta;
				
				fix_satellites(node, cconn->sat2);

				hyp = hypot(cconn->node1->sats[cconn->sat1].x, cconn->node1->sats[cconn->sat1].y);
				
				theta = atan2(node->y - cconn->node1->y, node->x - cconn->node1->x);
				
				sincos(theta, &sin_theta, &cos_theta);
				
				cconn->node1->sats[cconn->sat1].x = hyp * cos_theta;
				cconn->node1->sats[cconn->sat1].y = hyp * sin_theta;
				
				fix_satellites(cconn->node1, cconn->sat1);
				fix_conic_conns_from_node(cconn->node1);
				make_node_effective(cconn->node1);
			}
		}
		
		cconn = cconn->next;
	}
}


void make_node_effective(struct node_t *node)
{
	switch(node->num_conns)
	{
	case 0:
	case 1:
	case 2:
		{
			node->effective_x[0] = node->x;
			node->effective_y[0] = node->y;
			node->effective_x[1] = node->x;
			node->effective_y[1] = node->y;
			node->effective_x[2] = node->x;
			node->effective_y[2] = node->y;
			node->effective_x[3] = node->x;
			node->effective_y[3] = node->y;
		}
		break;
		
	case 3:
	case 4:
		{
			double length = hypot(node->sats[0].x, node->sats[0].y);
			
			node->effective_x[0] = node->x + (node->sats[0].x / length * node->width[2]); 
			node->effective_y[0] = node->y + (node->sats[0].y / length * node->width[2]); 
		
			length = hypot(node->sats[1].x, node->sats[1].y);
			
			node->effective_x[1] = node->x + (node->sats[1].x / length * node->width[3]); 
			node->effective_y[1] = node->y + (node->sats[1].y / length * node->width[3]); 
		
			length = hypot(node->sats[2].x, node->sats[2].y);
			
			node->effective_x[2] = node->x + (node->sats[2].x / length * node->width[0]); 
			node->effective_y[2] = node->y + (node->sats[2].y / length * node->width[0]); 
		
			length = hypot(node->sats[3].x, node->sats[3].y);
			
			node->effective_x[3] = node->x + (node->sats[3].x / length * node->width[1]); 
			node->effective_y[3] = node->y + (node->sats[3].y / length * node->width[1]); 
		}
		break;
	}
}


int generate_hover_nodes()
{
	
	struct node_pointer_t *old_hover_nodes = hover_nodes;
	hover_nodes = NULL;
	struct node_t *cnode = node0;
	int n;

	while(cnode)
	{
		int node_screenx, node_screeny;
		world_to_screen(cnode->x, cnode->y, &node_screenx, &node_screeny);

		int deltax = mouse_screenx - node_screenx;
		int deltay = mouse_screeny - node_screeny;

		double dist = (double)deltax * (double)deltax + (double)deltay * (double)deltay;

		if(dist < NODE_THRESHOLD_SQUARED)
		{
			struct node_pointer_t nodep = {cnode, NULL};
			LL_ADD(struct node_pointer_t, &hover_nodes, &nodep);
		}
		else for(n = 0; n < 4; n++)
		{
			if(cnode->sat_conn_type[n] == SAT_CONN_TYPE_BEZIER)
			{
				int sat_screenx, sat_screeny;
				world_to_screen(cnode->x + cnode->sats[n].x, cnode->y + cnode->sats[n].y, 
					&sat_screenx, &sat_screeny);
	
				int deltax = mouse_screenx - sat_screenx;
				int deltay = mouse_screeny - sat_screeny;
	
				double dist = (double)deltax * (double)deltax + (double)deltay * (double)deltay;
			
				if(dist < SATELLITE_THRESHOLD_SQUARED)
				{
					struct node_pointer_t nodep = {cnode, NULL};
					LL_ADD(struct node_pointer_t, &hover_nodes, &nodep);
					break;
				}
			}
		}

		cnode = cnode->next;
	}

	struct node_pointer_t *cnodep1 = hover_nodes, *cnodep2 = old_hover_nodes;


	int update = 1;

	while(1)
	{
		if(!cnodep1 && !cnodep2)
		{
			update = 0;
			break;
		}

		if(!cnodep1 || !cnodep2)
			break;

		if(cnodep1->node != cnodep2->node)
			break;

		cnodep1 = cnodep1->next;
		cnodep2 = cnodep2->next;
	}
	
	LL_REMOVE_ALL(struct node_pointer_t, &old_hover_nodes);


	
	return update;
}


struct node_t *get_node(int x, int y, int *xoffset, int *yoffset)
{
	struct node_t *cnode = node0;

	while(cnode)
	{
		int node_screenx, node_screeny;
		world_to_screen(cnode->x, cnode->y, &node_screenx, &node_screeny);

		int deltax = x - node_screenx;
		int deltay = y - node_screeny;

		double dist = (double)deltax * (double)deltax + (double)deltay * (double)deltay;

		if(dist < NODE_THRESHOLD_SQUARED)
		{
			if(xoffset)
				*xoffset = deltax;

			if(yoffset)
				*yoffset = deltay;

			return cnode;
		}

		cnode = cnode->next;
	}

	return NULL;
}


void get_satellite(int x, int y, struct node_t **node, uint8_t *sat, int *xoffset, int *yoffset)
{
	struct node_t *cnode = node0;
	int n;

	while(cnode)
	{
		for(n = 0; n < 4; n++)
		{
			if(cnode->sat_conn_type[n] == SAT_CONN_TYPE_BEZIER)
			{
				int sat_screenx, sat_screeny;
				world_to_screen(cnode->x + cnode->sats[n].x, cnode->y + cnode->sats[n].y, 
					&sat_screenx, &sat_screeny);
	
				int deltax = x - sat_screenx;
				int deltay = y - sat_screeny;
	
				double dist = (double)deltax * (double)deltax + (double)deltay * (double)deltay;
			
				if(dist < SATELLITE_THRESHOLD_SQUARED)
				{
					if(node)
						*node = cnode;
						
					if(sat)
						*sat = n;
	
					if(xoffset)
						*xoffset = deltax;
	
					if(yoffset)
						*yoffset = deltay;
	
					return;
				}
			}
		}

		cnode = cnode->next;
	}
	
	if(node)
		*node = NULL;
}


void get_width_sat_pos(struct node_t *node, uint8_t sat, float *x, float *y)
{
	double theta = atan2(node->sats[0].y, node->sats[0].x);
	double cos_theta, sin_theta;

	switch(sat)
	{
	case 0:
		sincos(theta + M_PI / 2.0, &sin_theta, &cos_theta);
		*x = node->width[0] * cos_theta;
		*y = node->width[0] * sin_theta;
		break;
		
	case 1:
		sincos(theta - M_PI / 2.0, &sin_theta, &cos_theta);
		*x = node->width[1] * cos_theta;
		*y = node->width[1] * sin_theta;
		break;
		
	case 2:
		sincos(theta, &sin_theta, &cos_theta);
		*x = node->width[2] * cos_theta;
		*y = node->width[2] * sin_theta;
		break;
		
	case 3:
		sincos(theta - M_PI, &sin_theta, &cos_theta);
		*x = node->width[3] * cos_theta;
		*y = node->width[3] * sin_theta;
		break;
	}
}


void get_width_sat(int x, int y, struct node_t **node, uint8_t *sat, int *xoffset, int *yoffset)
{
	struct node_t *cnode = node0;
	
	float worldx, worldy;
	int w;
	screen_to_world(x, y, &worldx, &worldy);

	while(cnode)
	{
		float satx = 0.0, saty = 0.0;

		for(w = 0; w < 4; w++)
		{
			get_width_sat_pos(cnode, w, &satx, &saty);
			
			int sat_screenx, sat_screeny;
			world_to_screen(cnode->x + satx, cnode->y + saty, &sat_screenx, &sat_screeny);

			int deltax = x - sat_screenx;
			int deltay = y - sat_screeny;

			double dist = (double)deltax * (double)deltax + (double)deltay * (double)deltay;
		
			if(dist < SATELLITE_THRESHOLD_SQUARED)
			{
				if(node)
					*node = cnode;
					
				if(sat)
					*sat = w;

				if(xoffset)
					*xoffset = deltax;

				if(yoffset)
					*yoffset = deltay;

				return;
			}
		}
		
		cnode = cnode->next;
	}
	
	*node = NULL;
}


void set_width_sat(struct node_t *node, uint8_t sat, float x, float y)
{
	node->width[sat] = hypot(x, y);
}


void set_sat_dist(struct node_t *node, uint8_t sat, float x, float y)
{
	double dist = hypot(x, y);
	
	double theta = atan2(node->sats[sat].y, node->sats[sat].x);

	double sin_theta, cos_theta;
	sincos(theta, &sin_theta, &cos_theta);
	
	node->sats[sat].x = dist * cos_theta;
	node->sats[sat].y = dist * sin_theta;
	
	fix_satellites(node, sat);
}


void delete_all_nodes()		// always called when not working
{
	LL_REMOVE_ALL(struct node_t, &node0);
}


int check_for_unverticied_nodes()
{
	struct node_t *cnode = node0;
	
	while(cnode)
	{
		if(cnode->texture_surface && !cnode->texture_verts)
			return 1;
		
		cnode = cnode->next;
	}
	
	return 0;
}


int check_for_untiled_node_textures()
{
	struct node_t *cnode = node0;
	
	while(cnode)
	{
		if(cnode->texture_verts && !cnode->texture_tiled)
			return 1;
		
		cnode = cnode->next;
	}
	
	return 0;
}


void finished_tiling_all_node_textures()
{
	struct node_t *cnode = node0;
	
	while(cnode)
	{
		if(cnode->texture_surface && !cnode->texture_tiled)
			cnode->texture_tiled = 1;
		
		cnode = cnode->next;
	}
}


uint32_t count_nodes()
{
	uint32_t num_nodes = 0;
	struct node_t *cnode = node0;

	while(cnode)
	{
		num_nodes++;
		cnode = cnode->next;
	}

	return num_nodes;
}


void gzwrite_nodes(gzFile file)
{
	struct node_t *cnode = node0;
	uint32_t num_nodes = count_nodes();

	while(cnode)
	{
		if(!finite(cnode->x) || !finite(cnode->y))
			num_nodes--;
		
		LL_NEXT(cnode);
	}
	

	gzwrite(file, &num_nodes, 4);

	uint32_t cnode_index = 0;

	cnode = node0;
		
	while(cnode)
	{
		if(!finite(cnode->x) || !finite(cnode->y))
		{
			LL_NEXT(cnode);
			continue;
		}
		
		cnode->index = cnode_index++;
		gzwrite(file, &cnode->x, 4);
		gzwrite(file, &cnode->y, 4);
		gzwrite(file, &cnode->sats, sizeof(struct sat_t) * 4);
		gzwrite(file, &cnode->width[0], 4);
		gzwrite(file, &cnode->width[1], 4);
		gzwrite(file, &cnode->width[2], 4);
		gzwrite(file, &cnode->width[3], 4);
		gzwrite(file, &cnode->fill_type, 1);
		gzwrite_string(file, cnode->texture_filename);
		gzwrite(file, &cnode->texture_flip_horiz, 1);
		gzwrite(file, &cnode->texture_flip_vert, 1);
		gzwrite(file, &cnode->texture_rotate_left, 1);
		gzwrite(file, &cnode->texture_rotate_right, 1);
		
		gzwrite(file, &cnode->sat_conn_type, 4);
		gzwrite(file, &cnode->num_conns, 4);
		
		
		
		cnode = cnode->next;
	}
}


int gzread_nodes(gzFile file)
{
	int num_nodes;
	gzread(file, &num_nodes, 4);

	int cnode_index = 0;

	while(cnode_index < num_nodes)
	{
		struct node_t *cnode = new_node();

		cnode->index = cnode_index++;

		if(gzread(file, &cnode->x, 4) != 4)
			goto error;

		if(gzread(file, &cnode->y, 4) != 4)
			goto error;

		if(gzread(file, &cnode->sats, sizeof(struct sat_t) * 4) != sizeof(struct sat_t) * 4)
			goto error;

		if(gzread(file, &cnode->width[0], 4) != 4)
			goto error;

		if(gzread(file, &cnode->width[1], 4) != 4)
			goto error;

		if(gzread(file, &cnode->width[2], 4) != 4)
			goto error;

		if(gzread(file, &cnode->width[3], 4) != 4)
			goto error;

		if(gzread(file, &cnode->fill_type, 1) != 1)
			goto error;

		cnode->texture_filename = gzread_string(file);
		if(!cnode->texture_filename)
			goto error;
		
		if(gzread(file, &cnode->texture_flip_horiz, 1) != 1)
			goto error;

		if(gzread(file, &cnode->texture_flip_vert, 1) != 1)
			goto error;

		if(gzread(file, &cnode->texture_rotate_left, 1) != 1)
			goto error;

		if(gzread(file, &cnode->texture_rotate_right, 1) != 1)
			goto error;

		if(gzread(file, &cnode->sat_conn_type, 4) != 4)
			goto error;

		if(gzread(file, &cnode->num_conns, 4) != 4)
			goto error;
		
		if(!finite(cnode->x) || !finite(cnode->y))
		{
			LL_REMOVE(struct node_t, &node0, cnode);
		}
		else
		{
			make_node_effective(cnode);
		}
	}

	return 1;

error:

	return 0;
}


struct node_t *get_node_from_index(int index)
{
	struct node_t *cnode = node0;

	while(cnode)
	{
		if(cnode->index == index)
			return cnode;

		cnode = cnode->next;
	}

	return NULL;
}


void draw_nodes()
{
	struct blit_params_t params;
		
	params.dest = s_backbuffer;
	params.source = s_node;
	
	
	struct node_t *cnode = node0;
	
	while(cnode)
	{
		int x, y;
		world_to_screen(cnode->x, cnode->y, &x, &y);
		params.dest_x = x - 3;
		params.dest_y = y - 3;
		blit_surface(&params);

		cnode = cnode->next;
	}

	
	params.source = s_select;
	
	struct node_pointer_t *cnodep = selected_node0;
	
	while(cnodep)
	{
		int x, y;
		world_to_screen(cnodep->node->x, cnodep->node->y, &x, &y);
		params.dest_x = x - 11;
		params.dest_y = y - 11;
		blit_surface(&params);
		
		cnodep = cnodep->next;
	}
}


void draw_sat_lines()
{
	struct node_pointer_t *cnodep = hover_nodes;

	while(cnodep)
	{
		int s;
		
		for(s = 0; s < 4; s++)
		{
			if(cnodep->node->sat_conn_type[s] == SAT_CONN_TYPE_BEZIER)
			{
				draw_world_clipped_line(cnodep->node->x, cnodep->node->y, 
					cnodep->node->x + cnodep->node->sats[s].x, 
					cnodep->node->y + cnodep->node->sats[s].y);
			}
		}

		cnodep = cnodep->next;
	}
}
	

void draw_sats()
{
	struct blit_params_t params;
		
	params.dest = s_backbuffer;
	params.source = s_vectsat;

	struct node_t *cnode = node0;
		
	while(cnode)
	{
		int x, y, n;
		world_to_screen(cnode->x, cnode->y, &x, &y);

		for(n = 0; n < 4; n++)
		{
			if(cnode->sat_conn_type[n] == SAT_CONN_TYPE_BEZIER)
			{
				world_to_screen(cnode->x + cnode->sats[n].x, cnode->y + cnode->sats[n].y, &x, &y);
	
				params.dest_x = x - 3;
				params.dest_y = y - 3;
				blit_surface(&params);
			}
		}

		cnode = cnode->next;
	}
}


void draw_width_sat_lines()
{
	;
}


void draw_width_sats()
{
	struct blit_params_t params;
	params.dest = s_backbuffer;
	params.source = s_widthsat;

	struct node_t *cnode = node0;

	while(cnode)
	{
		int x, y, n;
		float worldx, worldy;
		world_to_screen(cnode->x, cnode->y, &x, &y);

		for(n = 0; n < 4; n++)
		{
			get_width_sat_pos(cnode, n, &worldx, &worldy);

			worldx += cnode->x;
			worldy += cnode->y;

			world_to_screen(worldx, worldy, &x, &y);

			params.dest_x = x - 3;
			params.dest_y = y - 3;
			blit_surface(&params);
		}
	
		cnode = cnode->next;
	}
}


void init_nodes()
{
	s_node = read_png_surface(find_resource("node.png"));
	s_vectsat = read_png_surface(find_resource("vect-sat.png"));
	s_widthsat = read_png_surface(find_resource("width-sat.png"));
}


void kill_nodes()
{
	free_surface(s_node);		s_node = NULL;
	free_surface(s_vectsat);	s_vectsat = NULL;
	free_surface(s_widthsat);	s_widthsat = NULL;
}


void update_node_surface(struct node_t *node)		// always called when not working
{
	if(!node->texture_filename)
		return;
	
	if(!node->texture_filename->text)
		return;
	
	if(!node->pre_texture_surface)
	{
		struct string_t *string = arb_rel2abs(node->texture_filename->text, map_path->text);
		node->pre_texture_surface = read_gdk_pixbuf_surface_as_24bitalpha8bit(string->text);
		free_string(string);
	}

	if(!node->pre_texture_surface)
		return;
	
	if(!node->texture_surface)
	{
		node->texture_surface = duplicate_surface(node->pre_texture_surface);
		
		if(node->texture_flip_horiz)
			surface_flip_horiz(node->texture_surface);
		
		if(node->texture_flip_vert)
			surface_flip_vert(node->texture_surface);

		if(node->texture_rotate_left)
			surface_rotate_left(node->texture_surface);
		
		if(node->texture_rotate_right)
			surface_rotate_right(node->texture_surface);
	}
}


void set_node_sats(struct node_t *node, double angle, 
	double axis1_magnitude, double axis2_magnitude)
{
	double sin_theta, cos_theta;
	sincos((double)angle / (180.0 / M_PI), &sin_theta, &cos_theta);
	node->sats[0].y = sin_theta * axis1_magnitude;
	node->sats[0].x = cos_theta * axis1_magnitude;
	
	sincos((double)angle / (180.0 / M_PI) + M_PI, &sin_theta, &cos_theta);
	node->sats[1].y = sin_theta * axis1_magnitude;
	node->sats[1].x = cos_theta * axis1_magnitude;
	
	sincos((double)angle / (180.0 / M_PI) - M_PI / 2, &sin_theta, &cos_theta);
	node->sats[2].y = sin_theta * axis2_magnitude;
	node->sats[2].x = cos_theta * axis2_magnitude;
	
	sincos((double)angle / (180.0 / M_PI) + M_PI / 2, &sin_theta, &cos_theta);
	node->sats[3].y = sin_theta * axis2_magnitude;
	node->sats[3].x = cos_theta * axis2_magnitude;
}


void on_end_node_texture_entry_changed(GtkEditable *editable, gpointer user_data)
{
	;
}


void on_end_node_flip_horiz_checkbutton_toggled(GtkToggleButton *togglebutton, gpointer user_data)
{
	;
}


void on_end_node_flip_vert_checkbutton_toggled(GtkToggleButton *togglebutton, gpointer user_data)
{
	;
}


void on_end_node_rotate_checkbutton_toggled(GtkToggleButton *togglebutton, gpointer user_data)
{
	gboolean sensitive = gtk_toggle_button_get_active(togglebutton);
	GtkWidget *dialog = gtk_widget_get_toplevel(GTK_WIDGET(togglebutton));
	gtk_widget_set_sensitive(GTK_WIDGET(g_object_get_data(G_OBJECT(dialog), 
		"rotate_hbox")), sensitive);
	
//	struct node_t *node = g_object_get_data(G_OBJECT(dialog), "node");
	
/*	stop_working();

	if(sensitive)
		node->fill_type = NODE_SOLID;
	else
		node->fill_type = NODE_TEXTURE;
	
	invalidate_node(node);
	
	update_client_area();
	
	start_working();
	*/
}


void on_end_node_rotate_left_radiobutton_toggled(GtkToggleButton *togglebutton, gpointer user_data)
{
	;
}


void on_end_node_rotate_right_radiobutton_toggled(GtkToggleButton *togglebutton, gpointer user_data)
{
	;
}


void on_end_node_left_width_spinbutton_value_changed(GtkSpinButton *spinbutton, gpointer user_data)
{
	GtkWidget *dialog = gtk_widget_get_toplevel(GTK_WIDGET(spinbutton));
	
	if(g_object_get_data(G_OBJECT(dialog), "pressed"))
		return;
	
	stop_working();
	
	struct node_t *node = g_object_get_data(G_OBJECT(dialog), "node");
		
	node->width[0] = gtk_spin_button_get_value(spinbutton);
	
	invalidate_conns_with_node(node);
	invalidate_node(node);
	invalidate_bsp_tree();

	update_client_area();
	
	start_working();
}


gboolean on_end_node_left_width_spinbutton_button_press_event(GtkWidget *widget, 
	GdkEventButton *event, gpointer user_data)
{
	GtkWidget *dialog = gtk_widget_get_toplevel(GTK_WIDGET(widget));
	g_object_set_data(G_OBJECT(dialog), "pressed", (gpointer)1);
	return FALSE;
}


gboolean on_end_node_left_width_spinbutton_button_release_event(GtkWidget *widget, 
	GdkEventButton *event, gpointer user_data)
{
	GtkWidget *dialog = gtk_widget_get_toplevel(GTK_WIDGET(widget));
	g_object_set_data(G_OBJECT(dialog), "pressed", (gpointer)0);

	stop_working();
	
	struct node_t *node = g_object_get_data(G_OBJECT(dialog), "node");
		
	node->width[0] = gtk_spin_button_get_value(GTK_SPIN_BUTTON(widget));
	
	invalidate_conns_with_node(node);
	invalidate_node(node);
	invalidate_bsp_tree();

	update_client_area();
	
	start_working();

	return FALSE;
}


void on_end_node_right_width_spinbutton_value_changed(GtkSpinButton *spinbutton, gpointer user_data)
{
	GtkWidget *dialog = gtk_widget_get_toplevel(GTK_WIDGET(spinbutton));
	
	if(g_object_get_data(G_OBJECT(dialog), "pressed"))
		return;
	
	stop_working();
	
	struct node_t *node = g_object_get_data(G_OBJECT(dialog), "node");
		
	node->width[3] = gtk_spin_button_get_value(spinbutton);
	
	invalidate_conns_with_node(node);
	invalidate_node(node);
	invalidate_bsp_tree();

	update_client_area();
	
	start_working();
}


gboolean on_end_node_right_width_spinbutton_button_press_event(GtkWidget *widget, 
	GdkEventButton *event, gpointer user_data)
{
	GtkWidget *dialog = gtk_widget_get_toplevel(GTK_WIDGET(widget));
	g_object_set_data(G_OBJECT(dialog), "pressed", (gpointer)1);
	return FALSE;
}


gboolean on_end_node_right_width_spinbutton_button_release_event(GtkWidget *widget, 
	GdkEventButton  *event, gpointer user_data)
{
	GtkWidget *dialog = gtk_widget_get_toplevel(GTK_WIDGET(widget));
	g_object_set_data(G_OBJECT(dialog), "pressed", (gpointer)0);

	stop_working();
	
	struct node_t *node = g_object_get_data(G_OBJECT(dialog), "node");
		
	node->width[3] = gtk_spin_button_get_value(GTK_SPIN_BUTTON(widget));
	
	invalidate_conns_with_node(node);
	invalidate_node(node);
	invalidate_bsp_tree();

	update_client_area();
	
	start_working();

	return FALSE;
}


void on_end_node_texture_checkbutton_toggled(GtkToggleButton *togglebutton, gpointer user_data)
{
	gboolean sensitive = gtk_toggle_button_get_active(togglebutton);
	GtkWidget *dialog = gtk_widget_get_toplevel(GTK_WIDGET(togglebutton));
	gtk_widget_set_sensitive(GTK_WIDGET(g_object_get_data(G_OBJECT(dialog), 
		"texture_vbox")), sensitive);
	
	struct node_t *node = g_object_get_data(G_OBJECT(dialog), "node");
	
	stop_working();

	if(sensitive)
		node->fill_type = NODE_TEXTURE;
	
	invalidate_node(node);
	
	update_client_area();
	
	start_working();
}


void on_end_node_magnitude_spinbutton_value_changed(GtkSpinButton *spinbutton, gpointer user_data)
{
	GtkWidget *dialog = gtk_widget_get_toplevel(GTK_WIDGET(spinbutton));
	
	if(g_object_get_data(G_OBJECT(dialog), "pressed"))
		return;
	
	stop_working();
	
	struct node_t *node = g_object_get_data(G_OBJECT(dialog), "node");
		
	double angle = atan2(node->sats[0].y, node->sats[0].x) * (180.0 / M_PI);
	double distance_axis1 = gtk_spin_button_get_value(spinbutton);
	double distance_axis2 = hypot(node->sats[2].x, node->sats[2].y);
	
	set_node_sats(node, angle, distance_axis1, distance_axis2);
	
	invalidate_node(node);
	invalidate_conns_with_node(node);
	invalidate_bsp_tree();
	
	update_client_area();
	
	start_working();
}


gboolean on_end_node_magnitude_spinbutton_button_press_event(GtkWidget *widget, 
	GdkEventButton *event, gpointer user_data)
{
	GtkWidget *dialog = gtk_widget_get_toplevel(GTK_WIDGET(widget));
	g_object_set_data(G_OBJECT(dialog), "pressed", (gpointer)1);
	return FALSE;
}


gboolean on_end_node_magnitude_spinbutton_button_release_event(GtkWidget *widget, 
	GdkEventButton *event, gpointer user_data)
{
	GtkWidget *dialog = gtk_widget_get_toplevel(widget);
	g_object_set_data(G_OBJECT(dialog), "pressed", (gpointer)0);

	stop_working();
	
	struct node_t *node = g_object_get_data(G_OBJECT(dialog), "node");
		
	double angle = atan2(node->sats[0].y, node->sats[0].x) * (180.0 / M_PI);
	double distance_axis1 = gtk_spin_button_get_value(GTK_SPIN_BUTTON(widget));
	double distance_axis2 = hypot(node->sats[2].x, node->sats[2].y);
	
	set_node_sats(node, angle, distance_axis1, distance_axis2);
	
	invalidate_node(node);
	invalidate_conns_with_node(node);
	invalidate_bsp_tree();
	
	update_client_area();
	
	start_working();
	
	return FALSE;
}


void on_end_node_wall_left_width_spinbutton_value_changed(GtkSpinButton *spinbutton, 
	gpointer user_data)
{
	GtkWidget *dialog = gtk_widget_get_toplevel(GTK_WIDGET(spinbutton));
	
	if(g_object_get_data(G_OBJECT(dialog), "pressed"))
		return;
	
}


gboolean on_end_node_wall_left_width_spinbutton_button_press_event(GtkWidget *widget, 
	GdkEventButton *event, gpointer user_data)
{
	GtkWidget *dialog = gtk_widget_get_toplevel(GTK_WIDGET(widget));
	g_object_set_data(G_OBJECT(dialog), "pressed", (gpointer)1);
	return FALSE;
}


gboolean on_end_node_wall_left_width_spinbutton_button_release_event(GtkWidget *widget, 
	GdkEventButton *event, gpointer user_data)
{
	GtkWidget *dialog = gtk_widget_get_toplevel(GTK_WIDGET(widget));
	g_object_set_data(G_OBJECT(dialog), "pressed", (gpointer)0);
	return FALSE;
}


void on_end_node_wall_right_width_spinbutton_value_changed(GtkSpinButton *spinbutton, 
	gpointer user_data)
{
	GtkWidget *dialog = gtk_widget_get_toplevel(GTK_WIDGET(spinbutton));
	
	if(g_object_get_data(G_OBJECT(dialog), "pressed"))
		return;
	
}


gboolean on_end_node_wall_right_width_spinbutton_button_press_event(GtkWidget *widget, 
	GdkEventButton *event, gpointer user_data)
{
	GtkWidget *dialog = gtk_widget_get_toplevel(GTK_WIDGET(widget));
	g_object_set_data(G_OBJECT(dialog), "pressed", (gpointer)1);
	return FALSE;
}


gboolean on_end_node_wall_right_width_spinbutton_button_release_event(GtkWidget *widget, 
	GdkEventButton *event, gpointer user_data)
{
	GtkWidget *dialog = gtk_widget_get_toplevel(GTK_WIDGET(widget));
	g_object_set_data(G_OBJECT(dialog), "pressed", (gpointer)0);
	return FALSE;
}


void on_end_node_angle_spinbutton_value_changed(GtkSpinButton *spinbutton, gpointer user_data)
{
	GtkWidget *dialog = gtk_widget_get_toplevel(GTK_WIDGET(spinbutton));
	
	if(g_object_get_data(G_OBJECT(dialog), "pressed"))
		return;
	
}


gboolean on_end_node_angle_spinbutton_button_press_event(GtkWidget *widget, 
	GdkEventButton *event, gpointer user_data)
{
	GtkWidget *dialog = gtk_widget_get_toplevel(GTK_WIDGET(widget));
	g_object_set_data(G_OBJECT(dialog), "pressed", (gpointer)1);
	return FALSE;
}


gboolean on_end_node_angle_spinbutton_button_release_event(GtkWidget *widget, 
	GdkEventButton  *event, gpointer user_data)
{
	GtkWidget *dialog = gtk_widget_get_toplevel(GTK_WIDGET(widget));
	g_object_set_data(G_OBJECT(dialog), "pressed", (gpointer)0);
	return FALSE;
}


void on_end_node_properties_dialog_destroy(GtkObject *object, gpointer user_data)
{
	gtk_main_quit();
}


void run_end_node_properties_dialog(struct node_t *node)
{
	GtkWidget *dialog = create_end_node_properties_dialog();
	
	g_object_set_data(G_OBJECT(dialog), "node", node);
	
	gtk_window_set_transient_for(GTK_WINDOW(dialog), GTK_WINDOW(window));
	gtk_window_set_position(GTK_WINDOW(dialog), GTK_WIN_POS_CENTER_ON_PARENT);
	
	if(node->fill_type == NODE_TEXTURE)
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(g_object_get_data(G_OBJECT(dialog), 
			"texture_radiobutton")), TRUE);

	if(node->texture_filename)
	{
		gtk_entry_set_text(GTK_ENTRY(g_object_get_data(G_OBJECT(dialog), 
			"texture_entry")), node->texture_filename->text);
	}
	
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(g_object_get_data(G_OBJECT(dialog), 
		"flip_horiz_checkbutton")), node->texture_flip_horiz);
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(g_object_get_data(G_OBJECT(dialog), 
		"flip_vert_checkbutton")), node->texture_flip_vert);
	
	if(node->texture_rotate_left)
	{
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(g_object_get_data(G_OBJECT(dialog), 
			"rotate_checkbutton")), 1);
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(g_object_get_data(G_OBJECT(dialog), 
			"rotate_left_radiobutton")), 1);
	}

	if(node->texture_rotate_right)
	{
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(g_object_get_data(G_OBJECT(dialog), 
			"rotate_checkbutton")), 1);
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(g_object_get_data(G_OBJECT(dialog), 
			"rotate_right_radiobutton")), 1);
	}
	
	gtk_spin_button_set_value(GTK_SPIN_BUTTON(g_object_get_data(G_OBJECT(dialog), 
		"left_width_spinbutton")), (gdouble)node->width[2]);

	gtk_spin_button_set_value(GTK_SPIN_BUTTON(g_object_get_data(G_OBJECT(dialog), 
		"right_width_spinbutton")), (gdouble)node->width[3]);
	
	gtk_spin_button_set_value(GTK_SPIN_BUTTON(g_object_get_data(G_OBJECT(dialog), 
		"magnitude_spinbutton")), (gdouble)hypot(node->sats[0].x, node->sats[0].y));
	
	gtk_spin_button_set_value(GTK_SPIN_BUTTON(g_object_get_data(G_OBJECT(dialog), 
		"wall_left_width_spinbutton")), (gdouble)node->width[0]);
	
	gtk_spin_button_set_value(GTK_SPIN_BUTTON(g_object_get_data(G_OBJECT(dialog), 
		"wall_right_width_spinbutton")), (gdouble)node->width[1]);

	gtk_spin_button_set_value(GTK_SPIN_BUTTON(g_object_get_data(G_OBJECT(dialog), 
		"angle_spinbutton")), (gdouble)atan2(node->sats[0].y, node->sats[0].x) * (180.0 / M_PI));
	
	gtk_widget_show(dialog);
	gtk_main();
}



void on_straight_through_node_magnitude_spinbutton_value_changed(GtkSpinButton *spinbutton, 
	gpointer user_data)
{
	GtkWidget *dialog = gtk_widget_get_toplevel(GTK_WIDGET(spinbutton));
	
	if(g_object_get_data(G_OBJECT(dialog), "pressed"))
		return;
	
	stop_working();
	
	struct node_t *node = g_object_get_data(G_OBJECT(dialog), "node");
		
	double angle = atan2(node->sats[0].y, node->sats[0].x) * (180.0 / M_PI);
	double distance_axis1 = gtk_spin_button_get_value(spinbutton);
	double distance_axis2 = hypot(node->sats[2].x, node->sats[2].y);
	
	set_node_sats(node, angle, distance_axis1, distance_axis2);
	
	invalidate_node(node);
	invalidate_conns_with_node(node);
	invalidate_bsp_tree();
	
	update_client_area();
	
	start_working();
}


gboolean on_straight_through_node_magnitude_spinbutton_button_press_event(GtkWidget *widget, 
	GdkEventButton *event, gpointer user_data)
{
	GtkWidget *dialog = gtk_widget_get_toplevel(GTK_WIDGET(widget));
	g_object_set_data(G_OBJECT(dialog), "pressed", (gpointer)1);
	return FALSE;
}


gboolean on_straight_through_node_magnitude_spinbutton_button_release_event(GtkWidget *widget, 
	GdkEventButton *event, gpointer user_data)
{
	GtkWidget *dialog = gtk_widget_get_toplevel(GTK_WIDGET(widget));
	g_object_set_data(G_OBJECT(dialog), "pressed", (gpointer)0);

	stop_working();
	
	struct node_t *node = g_object_get_data(G_OBJECT(dialog), "node");
		
	double angle = atan2(node->sats[0].y, node->sats[0].x) * (180.0 / M_PI);
	double distance_axis1 = gtk_spin_button_get_value(GTK_SPIN_BUTTON(widget));
	double distance_axis2 = hypot(node->sats[2].x, node->sats[2].y);
	
	set_node_sats(node, angle, distance_axis1, distance_axis2);
	
	invalidate_node(node);
	invalidate_conns_with_node(node);
	invalidate_bsp_tree();
	
	update_client_area();
	
	start_working();

	return FALSE;
}


void on_straight_through_node_wall_left_width_spinbutton_value_changed(GtkSpinButton *spinbutton, 
	gpointer user_data)
{
	GtkWidget *dialog = gtk_widget_get_toplevel(GTK_WIDGET(spinbutton));
	
	if(g_object_get_data(G_OBJECT(dialog), "pressed"))
		return;
	
	stop_working();
	
	struct node_t *node = g_object_get_data(G_OBJECT(dialog), "node");
		
	node->width[0] = gtk_spin_button_get_value(spinbutton);
	
	invalidate_conns_with_node(node);
	invalidate_node(node);
	invalidate_bsp_tree();

	update_client_area();
	
	start_working();
}


gboolean on_straight_through_node_wall_left_width_spinbutton_button_press_event(GtkWidget *widget, 
	GdkEventButton *event, gpointer user_data)
{
	GtkWidget *dialog = gtk_widget_get_toplevel(GTK_WIDGET(widget));
	g_object_set_data(G_OBJECT(dialog), "pressed", (gpointer)1);
	return FALSE;
}


gboolean on_straight_through_node_wall_left_width_spinbutton_button_release_event(GtkWidget *widget, 
	GdkEventButton *event, gpointer user_data)
{
	GtkWidget *dialog = gtk_widget_get_toplevel(GTK_WIDGET(widget));
	g_object_set_data(G_OBJECT(dialog), "pressed", (gpointer)0);

	stop_working();
	
	struct node_t *node = g_object_get_data(G_OBJECT(dialog), "node");
		
	node->width[0] = gtk_spin_button_get_value(GTK_SPIN_BUTTON(widget));
	
	invalidate_conns_with_node(node);
	invalidate_node(node);
	invalidate_bsp_tree();

	update_client_area();
	
	start_working();

	return FALSE;
}


gboolean on_straight_through_node_wall_right_width_spinbutton_button_press_event(GtkWidget *widget, 
	GdkEventButton *event, gpointer user_data)
{
	GtkWidget *dialog = gtk_widget_get_toplevel(GTK_WIDGET(widget));
	g_object_set_data(G_OBJECT(dialog), "pressed", (gpointer)1);
	return FALSE;
}


gboolean on_straight_through_node_wall_right_width_spinbutton_button_release_event(
	GtkWidget *widget, GdkEventButton *event, gpointer user_data)
{
	GtkWidget *dialog = gtk_widget_get_toplevel(GTK_WIDGET(widget));
	g_object_set_data(G_OBJECT(dialog), "pressed", (gpointer)0);

	stop_working();
	
	struct node_t *node = g_object_get_data(G_OBJECT(dialog), "node");
		
	node->width[2] = gtk_spin_button_get_value(GTK_SPIN_BUTTON(widget));
	
	invalidate_conns_with_node(node);
	invalidate_node(node);
	invalidate_bsp_tree();

	update_client_area();
	
	start_working();

	return FALSE;
}


void on_straight_through_node_wall_right_width_spinbutton_value_changed(GtkSpinButton *spinbutton, 
	gpointer user_data)
{
	GtkWidget *dialog = gtk_widget_get_toplevel(GTK_WIDGET(spinbutton));
	
	if(g_object_get_data(G_OBJECT(dialog), "pressed"))
		return;
	
	stop_working();
	
	struct node_t *node = g_object_get_data(G_OBJECT(dialog), "node");
		
	node->width[2] = gtk_spin_button_get_value(spinbutton);
	
	invalidate_conns_with_node(node);
	invalidate_node(node);
	invalidate_bsp_tree();

	update_client_area();
	
	start_working();
}


void on_straight_through_node_angle_spinbutton_value_changed(GtkSpinButton *spinbutton, 
	gpointer user_data)
{
	GtkWidget *dialog = gtk_widget_get_toplevel(GTK_WIDGET(spinbutton));
	
	if(g_object_get_data(G_OBJECT(dialog), "pressed"))
		return;
	
	stop_working();
	
	struct node_t *node = g_object_get_data(G_OBJECT(dialog), "node");
		
	double angle = gtk_spin_button_get_value(GTK_SPIN_BUTTON(g_object_get_data(G_OBJECT(dialog), 
		"angle_spinbutton")));
	
	double distance_axis1 = hypot(node->sats[0].x, node->sats[0].y);
	double distance_axis2 = hypot(node->sats[2].x, node->sats[2].y);

	double sin_theta, cos_theta;
	sincos((double)angle / (180.0 / M_PI), &sin_theta, &cos_theta);
	node->sats[0].y = sin_theta * distance_axis1;
	node->sats[0].x = cos_theta * distance_axis1;
	
	sincos((double)angle / (180.0 / M_PI) + M_PI, &sin_theta, &cos_theta);
	node->sats[1].y = sin_theta * distance_axis1;
	node->sats[1].x = cos_theta * distance_axis1;
	
	sincos((double)angle / (180.0 / M_PI) - M_PI / 2, &sin_theta, &cos_theta);
	node->sats[2].y = sin_theta * distance_axis2;
	node->sats[2].x = cos_theta * distance_axis2;
	
	sincos((double)angle / (180.0 / M_PI) + M_PI / 2, &sin_theta, &cos_theta);
	node->sats[3].y = sin_theta * distance_axis2;
	node->sats[3].x = cos_theta * distance_axis2;
	
	make_node_effective(node);
	invalidate_conns_with_node(node);
	invalidate_node(node);
	invalidate_bsp_tree();

	update_client_area();
	start_working();
}


gboolean on_straight_through_node_angle_spinbutton_button_press_event(GtkWidget *widget, 
	GdkEventButton *event, gpointer user_data)
{
	GtkWidget *dialog = gtk_widget_get_toplevel(GTK_WIDGET(widget));
	g_object_set_data(G_OBJECT(dialog), "pressed", (gpointer)1);
	return FALSE;
}


gboolean on_straight_through_node_angle_spinbutton_button_release_event(GtkWidget *widget, 
	GdkEventButton *event, gpointer user_data)
{
	GtkWidget *dialog = gtk_widget_get_toplevel(GTK_WIDGET(widget));
	g_object_set_data(G_OBJECT(dialog), "pressed", (gpointer)0);

	stop_working();
	
	struct node_t *node = g_object_get_data(G_OBJECT(dialog), "node");
		
	double angle = gtk_spin_button_get_value(GTK_SPIN_BUTTON(g_object_get_data(G_OBJECT(dialog), 
		"angle_spinbutton")));
	
	double distance_axis1 = hypot(node->sats[0].x, node->sats[0].y);
	double distance_axis2 = hypot(node->sats[2].x, node->sats[2].y);

	double sin_theta, cos_theta;
	sincos((double)angle / (180.0 / M_PI), &sin_theta, &cos_theta);
	node->sats[0].y = sin_theta * distance_axis1;
	node->sats[0].x = cos_theta * distance_axis1;
	
	sincos((double)angle / (180.0 / M_PI) + M_PI, &sin_theta, &cos_theta);
	node->sats[1].y = sin_theta * distance_axis1;
	node->sats[1].x = cos_theta * distance_axis1;
	
	sincos((double)angle / (180.0 / M_PI) - M_PI / 2, &sin_theta, &cos_theta);
	node->sats[2].y = sin_theta * distance_axis2;
	node->sats[2].x = cos_theta * distance_axis2;
	
	sincos((double)angle / (180.0 / M_PI) + M_PI / 2, &sin_theta, &cos_theta);
	node->sats[3].y = sin_theta * distance_axis2;
	node->sats[3].x = cos_theta * distance_axis2;
	
	make_node_effective(node);
	invalidate_conns_with_node(node);
	invalidate_node(node);
	invalidate_bsp_tree();

	update_client_area();
	start_working();

	return FALSE;
}


void on_straight_through_node_properties_dialog_destroy(GtkObject *object, gpointer user_data)
{
	gtk_main_quit();
}


void run_straight_through_node_properties_dialog(struct node_t *node)
{
	GtkWidget *dialog = create_straight_through_node_properties_dialog();
	
	g_object_set_data(G_OBJECT(dialog), "node", node);
	g_object_set_data(G_OBJECT(dialog), "pressed", 0);
	
	gtk_window_set_transient_for(GTK_WINDOW(dialog), GTK_WINDOW(window));
	gtk_window_set_position(GTK_WINDOW(dialog), GTK_WIN_POS_CENTER_ON_PARENT);
	
	gtk_spin_button_set_value(GTK_SPIN_BUTTON(g_object_get_data(G_OBJECT(dialog), 
		"magnitude_spinbutton")), (gdouble)hypot(node->sats[0].x, node->sats[0].y));
	
	gtk_spin_button_set_value(GTK_SPIN_BUTTON(g_object_get_data(G_OBJECT(dialog), 
		"wall_left_width_spinbutton")), (gdouble)node->width[0]);
	
	gtk_spin_button_set_value(GTK_SPIN_BUTTON(g_object_get_data(G_OBJECT(dialog), 
		"wall_right_width_spinbutton")), (gdouble)node->width[1]);

	gtk_spin_button_set_value(GTK_SPIN_BUTTON(g_object_get_data(G_OBJECT(dialog), 
		"angle_spinbutton")), (gdouble)atan2(node->sats[0].y, node->sats[0].x) * (180.0 / M_PI));

	gtk_widget_show(dialog);
	gtk_main();
}


gboolean on_crossover_node_axis1_mag_spinbutton_button_press_event(GtkWidget *widget, 
	GdkEventButton *event, gpointer user_data)
{
	GtkWidget *dialog = gtk_widget_get_toplevel(GTK_WIDGET(widget));
	g_object_set_data(G_OBJECT(dialog), "pressed", (gpointer)1);
	return FALSE;
}


gboolean on_crossover_node_axis1_mag_spinbutton_button_release_event(GtkWidget *widget, 
	GdkEventButton *event, gpointer user_data)
{
	GtkWidget *dialog = gtk_widget_get_toplevel(GTK_WIDGET(widget));
	g_object_set_data(G_OBJECT(dialog), "pressed", (gpointer)0);

	stop_working();
	
	struct node_t *node = g_object_get_data(G_OBJECT(dialog), "node");
		
	double angle = atan2(node->sats[0].y, node->sats[0].x) * (180.0 / M_PI);
	double distance_axis1 = gtk_spin_button_get_value(GTK_SPIN_BUTTON(widget));
	double distance_axis2 = hypot(node->sats[2].x, node->sats[2].y);
	
	set_node_sats(node, angle, distance_axis1, distance_axis2);
	
	invalidate_node(node);
	invalidate_conns_with_node(node);
	invalidate_bsp_tree();
	
	update_client_area();
	
	start_working();
	
	return FALSE;
}


gboolean on_crossover_node_axis1_left_width_spinbutton_button_press_event(GtkWidget *widget, 
	GdkEventButton *event, gpointer user_data)
{
	GtkWidget *dialog = gtk_widget_get_toplevel(GTK_WIDGET(widget));
	g_object_set_data(G_OBJECT(dialog), "pressed", (gpointer)1);
	return FALSE;
}


gboolean on_crossover_node_axis1_left_width_spinbutton_button_release_event(GtkWidget *widget, 
	GdkEventButton *event, gpointer user_data)
{
	GtkWidget *dialog = gtk_widget_get_toplevel(GTK_WIDGET(widget));
	g_object_set_data(G_OBJECT(dialog), "pressed", (gpointer)0);

	stop_working();
	
	struct node_t *node = g_object_get_data(G_OBJECT(dialog), "node");
		
	node->width[0] = gtk_spin_button_get_value(GTK_SPIN_BUTTON(widget));
	
	invalidate_conns_with_node(node);
	invalidate_node(node);
	invalidate_bsp_tree();

	update_client_area();
	
	start_working();
	
	return FALSE;
}


gboolean on_crossover_node_axis1_right_width_spinbutton_button_press_event(GtkWidget *widget, 
	GdkEventButton *event, gpointer user_data)
{
	GtkWidget *dialog = gtk_widget_get_toplevel(GTK_WIDGET(widget));
	g_object_set_data(G_OBJECT(dialog), "pressed", (gpointer)1);
	return FALSE;
}


gboolean on_crossover_node_axis1_right_width_spinbutton_button_release_event(GtkWidget *widget, 
	GdkEventButton *event, gpointer user_data)
{
	GtkWidget *dialog = gtk_widget_get_toplevel(widget);
	g_object_set_data(G_OBJECT(dialog), "pressed", (gpointer)0);

	stop_working();
	
	struct node_t *node = g_object_get_data(G_OBJECT(dialog), "node");
		
	node->width[3] = gtk_spin_button_get_value(GTK_SPIN_BUTTON(widget));
	
	invalidate_conns_with_node(node);
	invalidate_node(node);
	invalidate_bsp_tree();

	update_client_area();
	
	start_working();

	return FALSE;
}


gboolean on_crossover_node_axis2_mag_spinbutton_button_press_event(GtkWidget *widget, 
	GdkEventButton *event, gpointer user_data)
{
	GtkWidget *dialog = gtk_widget_get_toplevel(GTK_WIDGET(widget));
	g_object_set_data(G_OBJECT(dialog), "pressed", (gpointer)1);
	return FALSE;
}


gboolean on_crossover_node_axis2_mag_spinbutton_button_release_event(GtkWidget *widget, 
	GdkEventButton *event, gpointer user_data)
{
	GtkWidget *dialog = gtk_widget_get_toplevel(GTK_WIDGET(widget));
	g_object_set_data(G_OBJECT(dialog), "pressed", (gpointer)0);

	stop_working();
	
	struct node_t *node = g_object_get_data(G_OBJECT(dialog), "node");
		
	double angle = atan2(node->sats[0].y, node->sats[0].x) * (180.0 / M_PI);
	double distance_axis1 = hypot(node->sats[0].x, node->sats[0].y);
	double distance_axis2 = gtk_spin_button_get_value(GTK_SPIN_BUTTON(widget));
	
	set_node_sats(node, angle, distance_axis1, distance_axis2);
	
	invalidate_node(node);
	invalidate_conns_with_node(node);
	invalidate_bsp_tree();
	
	update_client_area();
	
	start_working();
	
	return FALSE;
}


gboolean on_crossover_node_axis2_left_width_spinbutton_button_press_event(GtkWidget *widget, 
	GdkEventButton *event, gpointer user_data)
{
	GtkWidget *dialog = gtk_widget_get_toplevel(GTK_WIDGET(widget));
	g_object_set_data(G_OBJECT(dialog), "pressed", (gpointer)1);
	return FALSE;
}


gboolean on_crossover_node_axis2_left_width_spinbutton_button_release_event(GtkWidget *widget, 
	GdkEventButton *event, gpointer user_data)
{
	GtkWidget *dialog = gtk_widget_get_toplevel(GTK_WIDGET(widget));
	g_object_set_data(G_OBJECT(dialog), "pressed", (gpointer)0);
	return FALSE;
}


gboolean on_crossover_node_axis2_right_width_spinbutton_button_press_event(GtkWidget *widget, 
	GdkEventButton *event, gpointer user_data)
{
	GtkWidget *dialog = gtk_widget_get_toplevel(GTK_WIDGET(widget));
	g_object_set_data(G_OBJECT(dialog), "pressed", (gpointer)1);
	return FALSE;
}


gboolean on_crossover_node_axis2_right_width_spinbutton_button_release_event(GtkWidget *widget, 
	GdkEventButton *event, gpointer user_data)
{
	GtkWidget *dialog = gtk_widget_get_toplevel(GTK_WIDGET(widget));
	g_object_set_data(G_OBJECT(dialog), "pressed", (gpointer)0);
	return FALSE;
}


gboolean on_crossover_node_angle_spinbutton_button_press_event(GtkWidget *widget, 
	GdkEventButton *event, gpointer user_data)
{
	GtkWidget *dialog = gtk_widget_get_toplevel(GTK_WIDGET(widget));
	g_object_set_data(G_OBJECT(dialog), "pressed", (gpointer)1);
	return FALSE;
}


gboolean on_crossover_node_angle_spinbutton_button_release_event(GtkWidget *widget, 
	GdkEventButton *event, gpointer user_data)
{
	GtkWidget *dialog = gtk_widget_get_toplevel(GTK_WIDGET(widget));
	g_object_set_data(G_OBJECT(dialog), "pressed", (gpointer)0);
	return FALSE;
}


void on_crossover_node_properties_dialog_destroy(GtkObject *object, gpointer user_data)
{
	gtk_main_quit();
}


void on_crossover_node_blend_radiobutton_toggled(GtkToggleButton *togglebutton, gpointer user_data)
{
	gboolean sensitive = gtk_toggle_button_get_active(togglebutton);
	GtkWidget *dialog = gtk_widget_get_toplevel(GTK_WIDGET(togglebutton));
	gtk_widget_set_sensitive(GTK_WIDGET(g_object_get_data(G_OBJECT(dialog), 
		"blend_vbox")), sensitive);
	
//	struct node_t *node = g_object_get_data(G_OBJECT(dialog), "node");
	
/*	stop_working();

	if(sensitive)
		node->fill_type = NODE_SOLID;
	else
		node->fill_type = NODE_TEXTURE;
	
	invalidate_node(node);
	
	update_client_area();
	
	start_working();
	*/
}


void on_crossover_node_texture_entry_changed(GtkEditable *editable, gpointer user_data)
{
	GtkWidget *dialog = gtk_widget_get_toplevel(GTK_WIDGET(editable));
	struct node_t *node = g_object_get_data(G_OBJECT(dialog), "node");
		
	stop_working();
	
	free_string(node->texture_filename);
	
	gchar *strval;
	g_object_get(G_OBJECT(editable), "text", &strval, NULL);
	
	node->texture_filename = arb_abs2rel(strval, map_path->text);
	
	g_free(strval);
	
	free_surface(node->pre_texture_surface);
	node->pre_texture_surface = NULL;
	
	free_surface(node->texture_surface);
	node->texture_surface = NULL;
	
	update_node_surface(node);
	invalidate_node(node);
	
	update_client_area();
	
	start_working();
}


void on_crossover_node_flip_horiz_checkbutton_toggled(GtkToggleButton *togglebutton, 
	gpointer user_data)
{
	GtkWidget *dialog = gtk_widget_get_toplevel(GTK_WIDGET(togglebutton));
	struct node_t *node = g_object_get_data(G_OBJECT(dialog), "node");

	stop_working();
	
	node->texture_flip_horiz = gtk_toggle_button_get_active(togglebutton);
	
	free_surface(node->texture_surface);
	node->texture_surface = NULL;
	
	update_node_surface(node);
	invalidate_node(node);
	
	update_client_area();
	
	start_working();
}


void on_crossover_node_flip_vert_checkbutton_toggled(GtkToggleButton *togglebutton, 
	gpointer user_data)
{
	GtkWidget *dialog = gtk_widget_get_toplevel(GTK_WIDGET(togglebutton));
	struct node_t *node = g_object_get_data(G_OBJECT(dialog), "node");

	stop_working();
	
	node->texture_flip_vert = gtk_toggle_button_get_active(togglebutton);
	
	free_surface(node->texture_surface);
	node->texture_surface = NULL;
	
	update_node_surface(node);
	invalidate_node(node);
	
	update_client_area();
	
	start_working();
}


void on_crossover_node_rotate_checkbutton_toggled(GtkToggleButton *togglebutton, 
	gpointer user_data)
{
	gboolean sensitive = gtk_toggle_button_get_active(togglebutton);
	GtkWidget *dialog = gtk_widget_get_toplevel(GTK_WIDGET(togglebutton));
	gtk_widget_set_sensitive(GTK_WIDGET(g_object_get_data(G_OBJECT(dialog), 
		"rotate_hbox")), sensitive);
	
	struct node_t *node = g_object_get_data(G_OBJECT(dialog), "node");
		
	stop_working();
	
	if(sensitive)
	{
		if(gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(g_object_get_data(G_OBJECT(dialog), 
			"rotate_left_radiobutton"))))
		{
			node->texture_rotate_left = 1;
			node->texture_rotate_right = 0;
		}
		else
		{
			node->texture_rotate_left = 0;
			node->texture_rotate_right = 1;
		}
	}
	else
	{
		node->texture_rotate_left = 0;
		node->texture_rotate_right = 0;
	}
	
	free_surface(node->texture_surface);
	node->texture_surface = NULL;
	
	update_node_surface(node);
	invalidate_node(node);
	
	update_client_area();
	
	start_working();
}


void on_crossover_node_rotate_left_radiobutton_toggled(GtkToggleButton *togglebutton, 
	gpointer user_data)
{
	gboolean on = gtk_toggle_button_get_active(togglebutton);
	GtkWidget *dialog = gtk_widget_get_toplevel(GTK_WIDGET(togglebutton));
	struct node_t *node = g_object_get_data(G_OBJECT(dialog), "node");
	
	if(on)
	{
		stop_working();
		node->texture_rotate_left = 1;
		node->texture_rotate_right = 0;
		
		free_surface(node->texture_surface);
		node->texture_surface = NULL;
		
		update_node_surface(node);
		invalidate_node(node);
	
		update_client_area();
	
		start_working();
	}
}


void on_crossover_node_rotate_right_radiobutton_toggled(GtkToggleButton *togglebutton, 
	gpointer user_data)
{
	gboolean on = gtk_toggle_button_get_active(togglebutton);
	GtkWidget *dialog = gtk_widget_get_toplevel(GTK_WIDGET(togglebutton));
	struct node_t *node = g_object_get_data(G_OBJECT(dialog), "node");
	
	if(on)
	{
		stop_working();
		
		node->texture_rotate_left = 0;
		node->texture_rotate_right = 1;
		
		free_surface(node->texture_surface);
		node->texture_surface = NULL;
		
		update_node_surface(node);
		invalidate_node(node);
	
		update_client_area();
	
		start_working();
	}
}


void on_crossover_node_texture_radiobutton_toggled(GtkToggleButton *togglebutton, 
	gpointer user_data)
{
	gboolean sensitive = gtk_toggle_button_get_active(togglebutton);
	GtkWidget *dialog = gtk_widget_get_toplevel(GTK_WIDGET(togglebutton));
	gtk_widget_set_sensitive(GTK_WIDGET(g_object_get_data(G_OBJECT(dialog), 
		"texture_vbox")), sensitive);
	
//	struct node_t *node = g_object_get_data(G_OBJECT(dialog), "node");
	
/*	stop_working();

	if(sensitive)
		node->fill_type = NODE_SOLID;
	else
		node->fill_type = NODE_TEXTURE;
	
	invalidate_node(node);
	
	update_client_area();
	
	start_working();
	*/
}


void on_crossover_node_axis1_mag_spinbutton_value_changed(GtkSpinButton *spinbutton, 
	gpointer user_data)
{
	GtkWidget *dialog = gtk_widget_get_toplevel(GTK_WIDGET(spinbutton));
	
	if(g_object_get_data(G_OBJECT(dialog), "pressed"))
		return;
	
	stop_working();
	
	struct node_t *node = g_object_get_data(G_OBJECT(dialog), "node");
		
	double angle = atan2(node->sats[0].y, node->sats[0].x) * (180.0 / M_PI);
	double distance_axis1 = gtk_spin_button_get_value(spinbutton);
	double distance_axis2 = hypot(node->sats[2].x, node->sats[2].y);
	
	set_node_sats(node, angle, distance_axis1, distance_axis2);
	
	invalidate_node(node);
	invalidate_conns_with_node(node);
	invalidate_bsp_tree();
	
	update_client_area();
	
	start_working();
}


void on_crossover_node_axis1_left_width_spinbutton_value_changed(GtkSpinButton *spinbutton, 
	gpointer user_data)
{
	GtkWidget *dialog = gtk_widget_get_toplevel(GTK_WIDGET(spinbutton));
	
	if(g_object_get_data(G_OBJECT(dialog), "pressed"))
		return;
	
	stop_working();
	
	struct node_t *node = g_object_get_data(G_OBJECT(dialog), "node");
		
	node->width[0] = gtk_spin_button_get_value(spinbutton);
	
	invalidate_conns_with_node(node);
	invalidate_node(node);
	invalidate_bsp_tree();

	update_client_area();
	
	start_working();
}


void on_crossover_node_axis1_right_width_spinbutton_value_changed(GtkSpinButton *spinbutton, 
	gpointer user_data)
{
	GtkWidget *dialog = gtk_widget_get_toplevel(GTK_WIDGET(spinbutton));
	
	if(g_object_get_data(G_OBJECT(dialog), "pressed"))
		return;
	
	stop_working();
	
	struct node_t *node = g_object_get_data(G_OBJECT(dialog), "node");
		
	node->width[3] = gtk_spin_button_get_value(spinbutton);
	
	invalidate_conns_with_node(node);
	invalidate_node(node);
	invalidate_bsp_tree();

	update_client_area();
	
	start_working();
}


void on_crossover_node_axis2_mag_spinbutton_value_changed(GtkSpinButton *spinbutton, 
	gpointer user_data)
{
	GtkWidget *dialog = gtk_widget_get_toplevel(GTK_WIDGET(spinbutton));
	
	if(g_object_get_data(G_OBJECT(dialog), "pressed"))
		return;
	
	stop_working();
	
	struct node_t *node = g_object_get_data(G_OBJECT(dialog), "node");
		
	double angle = atan2(node->sats[0].y, node->sats[0].x) * (180.0 / M_PI);
	double distance_axis1 = hypot(node->sats[0].x, node->sats[0].y);
	double distance_axis2 = gtk_spin_button_get_value(spinbutton);
	
	set_node_sats(node, angle, distance_axis1, distance_axis2);
	
	invalidate_node(node);
	invalidate_conns_with_node(node);
	invalidate_bsp_tree();
	
	update_client_area();
	
	start_working();
}


void on_crossover_node_axis2_left_width_spinbutton_value_changed(GtkSpinButton *spinbutton, 
	gpointer user_data)
{
	GtkWidget *dialog = gtk_widget_get_toplevel(GTK_WIDGET(spinbutton));
	
	if(g_object_get_data(G_OBJECT(dialog), "pressed"))
		return;
	
}


void on_crossover_node_axis2_right_width_spinbutton_value_changed(GtkSpinButton *spinbutton, 
	gpointer user_data)
{
	GtkWidget *dialog = gtk_widget_get_toplevel(GTK_WIDGET(spinbutton));
	
	if(g_object_get_data(G_OBJECT(dialog), "pressed"))
		return;
	
}


void on_crossover_node_angle_spinbutton_value_changed(GtkSpinButton *spinbutton, 
	gpointer user_data)
{
	GtkWidget *dialog = gtk_widget_get_toplevel(GTK_WIDGET(spinbutton));
	
	if(g_object_get_data(G_OBJECT(dialog), "pressed"))
		return;
	
}


void run_crossover_node_properties_dialog(struct node_t *node)
{
	GtkWidget *dialog = create_crossover_node_properties_dialog();
	
	g_object_set_data(G_OBJECT(dialog), "node", node);
	
	gtk_window_set_transient_for(GTK_WINDOW(dialog), GTK_WINDOW(window));
	gtk_window_set_position(GTK_WINDOW(dialog), GTK_WIN_POS_CENTER_ON_PARENT);
	
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(g_object_get_data(G_OBJECT(dialog), 
		"texture_radiobutton")), TRUE);

	if(node->texture_filename)
	{
		gtk_entry_set_text(GTK_ENTRY(g_object_get_data(G_OBJECT(dialog), 
			"texture_entry")), node->texture_filename->text);
	}
	
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(g_object_get_data(G_OBJECT(dialog), 
		"flip_horiz_checkbutton")), node->texture_flip_horiz);
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(g_object_get_data(G_OBJECT(dialog), 
		"flip_vert_checkbutton")), node->texture_flip_vert);
	
	if(node->texture_rotate_left)
	{
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(g_object_get_data(G_OBJECT(dialog), 
			"rotate_checkbutton")), 1);
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(g_object_get_data(G_OBJECT(dialog), 
			"rotate_left_radiobutton")), 1);
	}

	if(node->texture_rotate_right)
	{
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(g_object_get_data(G_OBJECT(dialog), 
			"rotate_checkbutton")), 1);
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(g_object_get_data(G_OBJECT(dialog), 
			"rotate_right_radiobutton")), 1);
	}
	
	gtk_spin_button_set_value(GTK_SPIN_BUTTON(g_object_get_data(G_OBJECT(dialog), 
		"axis1_mag_spinbutton")), (gdouble)hypot(node->sats[0].x, node->sats[0].y));
	
	gtk_spin_button_set_value(GTK_SPIN_BUTTON(g_object_get_data(G_OBJECT(dialog), 
		"axis2_mag_spinbutton")), (gdouble)hypot(node->sats[2].x, node->sats[2].y));
	
	gtk_spin_button_set_value(GTK_SPIN_BUTTON(g_object_get_data(G_OBJECT(dialog), 
		"axis1_left_width_spinbutton")), (gdouble)node->width[0]);
	
	gtk_spin_button_set_value(GTK_SPIN_BUTTON(g_object_get_data(G_OBJECT(dialog), 
		"axis1_right_width_spinbutton")), (gdouble)node->width[1]);

	gtk_spin_button_set_value(GTK_SPIN_BUTTON(g_object_get_data(G_OBJECT(dialog), 
		"axis2_left_width_spinbutton")), (gdouble)node->width[2]);

	gtk_spin_button_set_value(GTK_SPIN_BUTTON(g_object_get_data(G_OBJECT(dialog), 
		"axis2_right_width_spinbutton")), (gdouble)node->width[3]);
	
	gtk_spin_button_set_value(GTK_SPIN_BUTTON(g_object_get_data(G_OBJECT(dialog), 
		"angle_spinbutton")), (gdouble)atan2(node->sats[0].y, node->sats[0].x) * (180.0 / M_PI));
	
	gtk_widget_show(dialog);
	gtk_main();
}


void run_node_properties_dialog(void *menu, struct node_t *node)
{
	switch(node->num_conns)
	{
	case 1:
		run_end_node_properties_dialog(node);
		break;
	
	case 2:
		run_straight_through_node_properties_dialog(node);
		break;
	
	case 3:
	case 4:
		run_crossover_node_properties_dialog(node);
		break;
	}
}


void menu_delete_node(GtkWidget *menu, struct node_t *node)
{
	delete_node(node);
	generate_hover_nodes();
	update_client_area();
}


void node_menu_connect_straight(GtkWidget *menu, struct node_t *node)
{
	connect_straight(node);
}


void node_menu_connect_conic(GtkWidget *menu, struct node_t *node)
{
	connect_conic(node);
}


void node_menu_connect_bezier(GtkWidget *menu, struct node_t *node)
{
	connect_bezier(node);
}


void run_node_menu(struct node_t *node)
{
	GtkWidget *menu;
	GtkWidget *menu_items;
	menu = gtk_menu_new();

/*
	struct conn_t *conn = get_conn_from_node(node);

	if(conn)
	{
		struct curve_t *curve = get_curve(conn);
		assert(curve);

		menu_items = gtk_menu_item_new_with_label("Wall Properties");
		gtk_signal_connect(GTK_OBJECT(menu_items), "activate", 
			GTK_SIGNAL_FUNC(run_wall_properties_dialog), curve);
		gtk_menu_append(GTK_MENU(menu), menu_items);
		gtk_widget_show(menu_items);

//		menu_items = gtk_menu_item_new_with_label("Start Texture Here");
//		gtk_signal_connect(GTK_OBJECT(menu_items), "activate", GTK_SIGNAL_FUNC(delete_node), NULL);
//		gtk_menu_append(GTK_MENU(menu), menu_items);
//		gtk_widget_show(menu_items);

		// TODO : if curve is complete then have "Start Texture Here Clockwise" and 
		// "Start Texture Here Anticlockwise" should "Start Texture Here" only appear 
		// if the texture doesn't start here ??
	}
*/

	if(node->num_conns < 2)
	{
		menu_items = gtk_menu_item_new_with_label("Straight Connect");
		gtk_signal_connect(GTK_OBJECT(menu_items), "activate", 
			GTK_SIGNAL_FUNC(node_menu_connect_straight), node);
		gtk_menu_append(GTK_MENU(menu), menu_items);
		gtk_widget_show(menu_items);
		
		menu_items = gtk_menu_item_new_with_label("Conic Connect");
		gtk_signal_connect(GTK_OBJECT(menu_items), "activate", 
			GTK_SIGNAL_FUNC(node_menu_connect_conic), node);
		gtk_menu_append(GTK_MENU (menu), menu_items);
		gtk_widget_show(menu_items);
	
		menu_items = gtk_menu_item_new_with_label("Bezier Connect");
		gtk_signal_connect(GTK_OBJECT(menu_items), "activate", 
			GTK_SIGNAL_FUNC(node_menu_connect_bezier), node);
		gtk_menu_append(GTK_MENU (menu), menu_items);
		gtk_widget_show(menu_items);
	}


/*	if(node->num_conns > 0)
	{
		menu_items = gtk_menu_item_new_with_label("Properties");
		gtk_signal_connect(GTK_OBJECT(menu_items), "activate", 
			GTK_SIGNAL_FUNC(run_node_properties_dialog), node);
		gtk_menu_append(GTK_MENU(menu), menu_items);
		gtk_widget_show(menu_items);
	}
*/	
	menu_items = gtk_menu_item_new_with_label("Delete Node");
	gtk_signal_connect(GTK_OBJECT(menu_items), "activate", 
		GTK_SIGNAL_FUNC(menu_delete_node), node);
	gtk_menu_append(GTK_MENU(menu), menu_items);
	gtk_widget_show(menu_items);

	gtk_menu_popup (GTK_MENU(menu), NULL, NULL, (GtkMenuPositionFunc)(NULL), NULL, 0, 0);
}


void sat_menu_disconnect(GtkWidget *menu, struct conn_t *conn)
{
	stop_working();
	delete_conn(conn);
	start_working();
}


void run_sat_menu(struct node_t *node, int sat)
{
	struct conn_t *conn = get_conn_from_sat(node, sat);
		
	GtkWidget *menu;
	GtkWidget *menu_items;
	menu = gtk_menu_new();

	menu_items = gtk_menu_item_new_with_label("Disconnect");
	gtk_signal_connect(GTK_OBJECT(menu_items), "activate", GTK_SIGNAL_FUNC(sat_menu_disconnect), 
		conn);
	gtk_menu_append(GTK_MENU(menu), menu_items);
	gtk_widget_show(menu_items);

	gtk_menu_popup (GTK_MENU(menu), NULL, NULL, (GtkMenuPositionFunc)(NULL), NULL, 0, 0);
}


//
// WORKER THREAD FUNCTIONS
//


int generate_verticies_for_node(struct node_t *node)
{
	struct vertex_t *vert = node->texture_verts = malloc(sizeof(struct vertex_t) * 
		(node->texture_surface->width + 1) * (node->texture_surface->height + 1));
	
	double length = hypot(node->sats[2].x, node->sats[2].y);
	double right_x = node->sats[2].x / length * 
		(node->width[1] + node->width[3]) / node->texture_surface->width;
	double right_y = node->sats[2].y / length * 
		(node->width[1] + node->width[3]) / node->texture_surface->width;
	
	length = hypot(node->sats[1].x, node->sats[1].y);
	double down_x = node->sats[1].x / length * 
		(node->width[2] + node->width[0]) / node->texture_surface->height;
	double down_y = node->sats[1].y / length * 
		(node->width[2] + node->width[0]) / node->texture_surface->height;
	
	double top_left_x = node->x - (right_x * node->texture_surface->width / 2.0) - 
		(down_x * node->texture_surface->height / 2.0);
	
	double top_left_y = node->y - (right_y * node->texture_surface->width / 2.0) - 
		(down_y * node->texture_surface->height / 2.0);
	
	int x, y;
	
	double left_x = top_left_x;
	double left_y = top_left_y;
	
	for(y = 0; y <= node->texture_surface->height; y++)
	{
		double vx = left_x;
		double vy = left_y;
		
		for(x = 0; x <= node->texture_surface->width; x++)
		{
			vert->x = vx;
			vert->y = vy;
			vert++;
			
			vx += right_x;
			vy += right_y;
		}
		
		left_x += down_x;
		left_y += down_y;
		
		if(in_lock_check_stop_callback())
			return 0;
	}

	return 1;
}


void generate_verticies_for_all_nodes()
{
	if(!worker_try_enter_main_lock())
		return;
	
	struct node_t *cnode = node0;
	
	while(cnode)
	{
		if(!cnode->texture_verts && cnode->texture_surface)
		{
			if(!generate_verticies_for_node(cnode))
				return;
		}
		
		cnode = cnode->next;
	}

	worker_leave_main_lock();
}


int tile_all_node_textures()
{
	if(!worker_try_enter_main_lock())
		return 0;

	struct node_t *node = node0;

	while(node)
	{
		if(node->texture_verts && !node->texture_tiled)
		{
			worker_leave_main_lock();
			
			if(!tile_node(node))
				return 0;
			
			if(!worker_try_enter_main_lock())
				return 0;
		}
		
		node = node->next;
	}
	
	worker_leave_main_lock();
	
	return 1;
}
