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
#include <math.h>
#include <memory.h>

#include <assert.h>

#include <zlib.h>


#include "vertex.h"
#include "minmax.h"
#include "llist.h"
#include "inout.h"
#include "gsub.h"
#include "nodes.h"
#include "conns.h"
#include "curves.h"
#include "bezier.h"
#include "tiles.h"
#include "worker.h"
#include "interface.h"
#include "bsp.h"
#include "fills.h"
#include "points.h"
#include "conic.h"

#include "main_lock.h"

#define NUMSEGMENTS 200

#define CONN_MAX_SEG_SIZE 7
#define CONN_MAX_SEG_SIZE_SQUARED (CONN_MAX_SEG_SIZE * CONN_MAX_SEG_SIZE)

struct conn_t *conn0 = NULL;


struct conn_t *get_conn_from_sat(struct node_t *node, int sat)
{
	struct conn_t *cconn = conn0;

	while(cconn)
	{
		if((cconn->node1 == node && cconn->sat1 == sat) ||
			(cconn->node2 == node && cconn->sat2 == sat))
			return cconn;

		cconn = cconn->next;
	}

	return NULL;
}


struct conn_t *get_conn_from_node(struct node_t *node)
{
	struct conn_t *cconn = conn0;

	while(cconn)
	{
		if(cconn->node1 == node ||
			cconn->node2 == node)
			return cconn;

		cconn = cconn->next;
	}

	return NULL;
}


struct conn_t *new_conn()
{
	struct conn_t *conn;
	LL_CALLOC_TAIL(struct conn_t, &conn0, &conn);
	return conn;
}



void fix_conic_conn_from_node(struct conn_t *conn, struct node_t *node1)
{
	int sat1, sat2;
	struct node_t *node2;
	
	if(conn->node1 == node1)
	{
		sat1 = conn->sat1;
		sat2 = conn->sat2;
		node2 = conn->node2;
	}
	else
	{
		sat1 = conn->sat2;
		sat2 = conn->sat1;
		node2 = conn->node1;
	}
	
	double ax1 = node1->x;
	double ay1 = node1->y;
	double ax2 = node1->x + node1->sats[sat1].x;
	double ay2 = node1->y + node1->sats[sat1].y;
	
	double bx1 = node2->x;
	double by1 = node2->y;
	double bx2 = node2->x + node2->sats[sat2].x;
	double by2 = node2->y + node2->sats[sat2].y;
	
	double nx = ay1 - ay2;
	double ny = -(ax1 - ax2);
	
	double numer = nx * (bx1 - ax1) + 
		ny * (by1 - ay1);
	
	double denom = (-nx) * (bx2 - bx1) + 
		(-ny) * (by2 - by1);
	
	if(fabs(denom) < 0.1)	// lines are parallel
	{
		ax2 = node1->x - node1->sats[sat1].y;
		ay2 = node1->y + node1->sats[sat1].x;
		
		nx = ay1 - ay2;
		ny = -(ax1 - ax2);
		
		numer = nx * (bx1 - ax1) + 
			ny * (by1 - ay1);
		
		denom = (-nx) * (bx2 - bx1) + 
			(-ny) * (by2 - by1);
		
		double t = numer / denom;
	
		if(finite(t))
		{
			node2->x = bx1 + (bx2 - bx1) * t;
			node2->y = by1 + (by2 - by1) * t;
		}
	}
	else
	{
		double t = numer / denom;
	
		double cx = bx1 + (bx2 - bx1) * t;
		double cy = by1 + (by2 - by1) * t;
		
		double dist = hypot(node1->x - cx, node1->y - cy);
		if(dist == 0.0)
			return;
		
		double dx = node2->x - cx;
		double dy = node2->y - cy;
		double delta_dist = hypot(dx, dy);
		dx /= delta_dist;
		dy /= delta_dist;
		dx *= dist;
		dy *= dist;
		
		if(finite(t))
		{
			node2->x = cx + dx;
			node2->y = cy + dy;
		}
	}
	
	make_node_effective(node1);
	make_node_effective(node2);
}


void fix_conic_conns_from_node(struct node_t *node)
{
	struct conn_t *conn;
		
	for(conn = conn0; conn; conn = conn->next)
	{
		if(conn->type != CONN_TYPE_CONIC)
			continue;
		
		if(conn->node1 == node)
			fix_conic_conn_from_node(conn, node);
		else if(conn->node2 == node)
			fix_conic_conn_from_node(conn, node);
	}
}


int insert_straight_conn(struct node_t *node1, struct node_t *node2)
{
	// make sure we are not connecting a node to itself

	if(node1 == node2)
		return 0;
	
	if(node1->num_conns >= 2 || node2->num_conns >= 2)
		return 0;
	
	int sat1 = 0, sat2 = 0, s = 0;
	
	switch(node1->num_conns)
	{
	case 0:
		sat1 = 0;
		break;

	case 1:
		for(s = 0; node1->sat_conn_type[s] == SAT_CONN_TYPE_UNCONN; s++);
		switch(s)
		{
		case 0: sat1 = 1; break;
		case 1: sat1 = 0; break;
		case 2: sat1 = 3; break;
		case 3: sat1 = 2; break;
		}
		
		break;

	case 2:
		switch(node1->sat_conn_type[0])
		{
		case SAT_CONN_TYPE_UNCONN:
			if(inout(node1->x + node1->sats[2].x, node1->y + node1->sats[2].y, 
				node1->x + node1->sats[3].x, node1->y + node1->sats[3].y, 
				node2->x, node2->y))
				sat1 = 0;
			else
				sat1 = 1;
			break;
		
		default:
			if(inout(node1->x + node1->sats[0].x, node1->y + node1->sats[0].y, 
				node1->x + node1->sats[1].x, node1->y + node1->sats[1].y, 
				node2->x, node2->y))
				sat1 = 3;
			else
				sat1 = 2;
			break;
		}
		break;

	case 3:
		for(sat1 = 0; node1->sat_conn_type[sat1] != SAT_CONN_TYPE_UNCONN; sat1++);
		break;

	case 4:
		return 0;
	}

	switch(node2->num_conns)
	{
	case 0:
		sat2 = 0;
		break;

	case 1:
		for(s = 0; node2->sat_conn_type[s] == SAT_CONN_TYPE_UNCONN; s++);
		switch(s)
		{
		case 0: sat2 = 1; break;
		case 1: sat2 = 0; break;
		case 2: sat2 = 3; break;
		case 3: sat2 = 2; break;
		}
		
		break;

	case 2:
		switch(node2->sat_conn_type[0])
		{
		case SAT_CONN_TYPE_UNCONN:
			if(inout(node2->x + node2->sats[2].x, node2->y + node2->sats[2].y, 
				node2->x + node2->sats[3].x, node2->y + node2->sats[3].y, 
				node1->x, node1->y))
				sat2 = 0;
			else
				sat2 = 1;
			break;
		
		default:
			if(inout(node2->x + node2->sats[0].x, node2->y + node2->sats[0].y, 
				node2->x + node2->sats[1].x, node2->y + node2->sats[1].y, 
				node1->x, node1->y))
				sat2 = 3;
			else
				sat2 = 2;
			break;
		}
		break;

	case 3:
		for(sat2 = 0; node2->sat_conn_type[sat2] != SAT_CONN_TYPE_UNCONN; sat2++);
		break;

	case 4:
		return 0;
	}

	stop_working();

	int opsat1 = 0, opsat2 = 0;
	
	switch(sat1)
	{
	case 0: opsat1 = 1; break;
	case 1: opsat1 = 0; break;
	case 2: opsat1 = 3; break;
	case 3: opsat1 = 2; break;
	}

	switch(sat2)
	{
	case 0: opsat2 = 1; break;
	case 1: opsat2 = 0; break;
	case 2: opsat2 = 3; break;
	case 3: opsat2 = 2; break;
	}
	
	if(node1->sat_conn_type[opsat1] == SAT_CONN_TYPE_UNCONN)
	{
		node1->sats[sat1].x = (node2->x - node1->x) * 0.375;
		node1->sats[sat1].y = (node2->y - node1->y) * 0.375;
	}
		
	if(node2->sat_conn_type[opsat2] == SAT_CONN_TYPE_UNCONN)
	{
		node2->sats[sat2].x = (node1->x - node2->x) * 0.375;
		node2->sats[sat2].y = (node1->y - node2->y) * 0.375;
	}
	
	node1->sat_conn_type[sat1] = SAT_CONN_TYPE_STRAIGHT;
	node1->num_conns++;
	
	node2->sat_conn_type[sat2] = SAT_CONN_TYPE_STRAIGHT;
	node2->num_conns++;
		
	
	make_node_effective(node1);
	make_node_effective(node2);



	struct conn_t *conn = new_conn();

	conn->type = CONN_TYPE_STRAIGHT;

	conn->node1 = node1;
	conn->node2 = node2;
	conn->sat1 = sat1;
	conn->sat2 = sat2;

	add_conn_to_curves(conn);
	
	straighten_from_node(node1);
	straighten_from_node(node2);

	invalidate_bsp_tree();
	
	start_working();
	
	return 1;
}


int insert_conic_conn(struct node_t *node1, struct node_t *node2)
{
	// make sure we are not connecting a node to itself

	if(node1 == node2)
		return 0;
	
	if(node1->num_conns >= 2 || node2->num_conns >= 2)
		return 0;
	
	int sat1 = 0, sat2 = 0, s = 0;
	
	switch(node1->num_conns)
	{
	case 0:
		return 0;

	case 1:
		for(s = 0; node1->sat_conn_type[s] == SAT_CONN_TYPE_UNCONN; s++);
		switch(s)
		{
		case 0: sat1 = 1; break;
		case 1: sat1 = 0; break;
		case 2: sat1 = 3; break;
		case 3: sat1 = 2; break;
		}
		
		break;

	case 2:
		switch(node1->sat_conn_type[0])
		{
		case SAT_CONN_TYPE_UNCONN:
			if(inout(node1->x + node1->sats[2].x, node1->y + node1->sats[2].y, 
				node1->x + node1->sats[3].x, node1->y + node1->sats[3].y, 
				node2->x, node2->y))
				sat1 = 0;
			else
				sat1 = 1;
			break;
		
		default:
			if(inout(node1->x + node1->sats[0].x, node1->y + node1->sats[0].y, 
				node1->x + node1->sats[1].x, node1->y + node1->sats[1].y, 
				node2->x, node2->y))
				sat1 = 3;
			else
				sat1 = 2;
			break;
		}
		break;

	case 3:
		for(sat1 = 0; node1->sat_conn_type[sat1] != SAT_CONN_TYPE_UNCONN; sat1++);
		break;

	case 4:
		return 0;
	}

	switch(node2->num_conns)
	{
	case 0:
		return 0;

	case 1:
		for(s = 0; node2->sat_conn_type[s] == SAT_CONN_TYPE_UNCONN; s++);
		switch(s)
		{
		case 0: sat2 = 1; break;
		case 1: sat2 = 0; break;
		case 2: sat2 = 3; break;
		case 3: sat2 = 2; break;
		}
		
		break;

	case 2:
		switch(node2->sat_conn_type[0])
		{
		case SAT_CONN_TYPE_UNCONN:
			if(inout(node2->x + node2->sats[2].x, node2->y + node2->sats[2].y, 
				node2->x + node2->sats[3].x, node2->y + node2->sats[3].y, 
				node1->x, node1->y))
				sat2 = 0;
			else
				sat2 = 1;
			break;
		
		default:
			if(inout(node2->x + node2->sats[0].x, node2->y + node2->sats[0].y, 
				node2->x + node2->sats[1].x, node2->y + node2->sats[1].y, 
				node1->x, node1->y))
				sat2 = 3;
			else
				sat2 = 2;
			break;
		}
		break;

	case 3:
		for(sat2 = 0; node2->sat_conn_type[sat2] != SAT_CONN_TYPE_UNCONN; sat2++);
		break;

	case 4:
		return 0;
	}

	stop_working();

	int opsat1 = 0, opsat2 = 0;
	
	switch(sat1)
	{
	case 0: opsat1 = 1; break;
	case 1: opsat1 = 0; break;
	case 2: opsat1 = 3; break;
	case 3: opsat1 = 2; break;
	}

	switch(sat2)
	{
	case 0: opsat2 = 1; break;
	case 1: opsat2 = 0; break;
	case 2: opsat2 = 3; break;
	case 3: opsat2 = 2; break;
	}
	
	if(node1->sat_conn_type[opsat1] == SAT_CONN_TYPE_UNCONN)
	{
		node1->sats[sat1].x = (node2->x - node1->x) * 0.375;
		node1->sats[sat1].y = (node2->y - node1->y) * 0.375;
	}
		
	if(node2->sat_conn_type[opsat2] == SAT_CONN_TYPE_UNCONN)
	{
		node2->sats[sat2].x = (node1->x - node2->x) * 0.375;
		node2->sats[sat2].y = (node1->y - node2->y) * 0.375;
	}
	
	node1->sat_conn_type[sat1] = SAT_CONN_TYPE_CONIC;
	node1->num_conns++;
	
	node2->sat_conn_type[sat2] = SAT_CONN_TYPE_CONIC;
	node2->num_conns++;
		
	


	struct conn_t *conn = new_conn();

	conn->type = CONN_TYPE_CONIC;

	conn->node1 = node1;
	conn->node2 = node2;
	conn->sat1 = sat1;
	conn->sat2 = sat2;

	add_conn_to_curves(conn);
	
	straighten_from_node(node1);
	straighten_from_node(node2);

	fix_conic_conn_from_node(conn, node1);
	
	make_node_effective(node1);
	make_node_effective(node2);

	LL_REMOVE_ALL(struct t_t, &conn->bigt0);
	generate_conic_bigts(conn);
	
	invalidate_bsp_tree();
	
	start_working();
	
	return 1;
}


int insert_bezier_conn(struct node_t *node1, struct node_t *node2)
{
	// make sure we are not connecting a node to itself

	if(node1 == node2)
		return 0;
	
	if(node1->num_conns >= 2 || node2->num_conns >= 2)
		return 0;
	
	int sat1 = 0, sat2 = 0, s = 0, opsat1 = 0, opsat2 = 0;
	
	switch(node1->num_conns)
	{
	case 0:
		sat1 = 0;
		break;

	case 1:
		for(s = 0; node1->sat_conn_type[s] == SAT_CONN_TYPE_UNCONN; s++);
		switch(s)
		{
		case 0: sat1 = 1; break;
		case 1: sat1 = 0; break;
		case 2: sat1 = 3; break;
		case 3: sat1 = 2; break;
		}
		
		break;

	case 2:
		switch(node1->sat_conn_type[0])
		{
		case SAT_CONN_TYPE_UNCONN:
			if(inout(node1->x + node1->sats[2].x, node1->y + node1->sats[2].y, 
				node1->x + node1->sats[3].x, node1->y + node1->sats[3].y, 
				node2->x, node2->y))
				sat1 = 0;
			else
				sat1 = 1;
			break;
		
		default:
			if(inout(node1->x + node1->sats[0].x, node1->y + node1->sats[0].y, 
				node1->x + node1->sats[1].x, node1->y + node1->sats[1].y, 
				node2->x, node2->y))
				sat1 = 3;
			else
				sat1 = 2;
			break;
		}
		break;

	case 3:
		for(sat1 = 0; node1->sat_conn_type[sat1] != SAT_CONN_TYPE_UNCONN; sat1++);
		break;

	case 4:
		return 0;
	}

	switch(node2->num_conns)
	{
	case 0:
		sat2 = 0;
		break;

	case 1:
		for(s = 0; node2->sat_conn_type[s] == SAT_CONN_TYPE_UNCONN; s++);
		switch(s)
		{
		case 0: sat2 = 1; break;
		case 1: sat2 = 0; break;
		case 2: sat2 = 3; break;
		case 3: sat2 = 2; break;
		}
		
		break;

	case 2:
		switch(node2->sat_conn_type[0])
		{
		case SAT_CONN_TYPE_UNCONN:
			if(inout(node2->x + node2->sats[2].x, node2->y + node2->sats[2].y, 
				node2->x + node2->sats[3].x, node2->y + node2->sats[3].y, 
				node1->x, node1->y))
				sat2 = 0;
			else
				sat2 = 1;
			break;
		
		default:
			if(inout(node2->x + node2->sats[0].x, node2->y + node2->sats[0].y, 
				node2->x + node2->sats[1].x, node2->y + node2->sats[1].y, 
				node1->x, node1->y))
				sat2 = 3;
			else
				sat2 = 2;
			break;
		}
		break;

	case 3:
		for(sat2 = 0; node2->sat_conn_type[sat2] != SAT_CONN_TYPE_UNCONN; sat2++);
		break;

	case 4:
		return 0;
	}
	
	switch(sat1)
	{
	case 0: opsat1 = 1; break;
	case 1: opsat1 = 0; break;
	case 2: opsat1 = 3; break;
	case 3: opsat1 = 2; break;
	}

	switch(sat2)
	{
	case 0: opsat2 = 1; break;
	case 1: opsat2 = 0; break;
	case 2: opsat2 = 3; break;
	case 3: opsat2 = 2; break;
	}

	stop_working();
	
	node1->sat_conn_type[sat1] = SAT_CONN_TYPE_BEZIER;
	node1->num_conns++;
	
	node2->sat_conn_type[sat2] = SAT_CONN_TYPE_BEZIER;
	node2->num_conns++;
	
	if(node1->sat_conn_type[opsat1] == SAT_CONN_TYPE_UNCONN && 
		node2->sat_conn_type[opsat2] == SAT_CONN_TYPE_UNCONN)
	{
		node1->sats[sat1].x = (node2->x - node1->x) * 0.375;
		node1->sats[sat1].y = (node2->y - node1->y) * 0.375;
		fix_satellites(node1, sat1);

		node2->sats[sat2].x = (node1->x - node2->x) * 0.375;
		node2->sats[sat2].y = (node1->y - node2->y) * 0.375;
		fix_satellites(node2, sat2);
	}
	else
	{
		if(node1->sat_conn_type[opsat1] == SAT_CONN_TYPE_UNCONN)
		{
			struct node_t *temp_node = node1;
			node1 = node2;
			node2 = temp_node;
			
			int temp_int = sat1;
			sat1 = sat2;
			sat2 = temp_int;
		
			temp_int = opsat1;
			opsat1 = opsat2;
			opsat2 = temp_int;
		
		}
		
		switch(node1->sat_conn_type[opsat1])
		{
		case SAT_CONN_TYPE_STRAIGHT:
			switch(node2->sat_conn_type[opsat2])
			{
			case SAT_CONN_TYPE_UNCONN:
				{
					double dist = cos(atan2((node2->y - node1->y), (node2->x - node1->x)) - 
						atan2(node1->sats[sat1].y, node1->sats[sat1].x)) *
						hypot(node2->x - node1->x, node2->y - node1->y) * 0.75;
				
					double a = hypot(node1->sats[sat1].x, node1->sats[sat1].y);
				
					node1->sats[sat1].x /= a;
					node1->sats[sat1].x *= dist;
					node1->sats[sat1].y /= a;
					node1->sats[sat1].y *= dist;
					fix_satellites(node1, sat1);

					if(inout(node1->x, node1->y, node1->x + node1->sats[sat1].x, 
						node1->y + node1->sats[sat1].y, node2->x, node2->y))
					{
						node2->sats[sat2].x = -node1->sats[sat1].y;
						node2->sats[sat2].y = node1->sats[sat1].x;
					}
					else
					{
						node2->sats[sat2].x = node1->sats[sat1].y;
						node2->sats[sat2].y = -node1->sats[sat1].x;
					}
					
					dist = cos(atan2((node1->y - node2->y), (node1->x - node2->x)) - 
						atan2(node2->sats[sat2].y, node2->sats[sat2].x)) *
						hypot(node1->x - node2->x, node1->y - node2->y) * 0.75;
				
					a = hypot(node2->sats[sat2].x, node2->sats[sat2].y);
				
					node2->sats[sat2].x /= a;
					node2->sats[sat2].x *= dist;
					node2->sats[sat2].y /= a;
					node2->sats[sat2].y *= dist;
					fix_satellites(node2, sat2);
				}
				break;
			
			case SAT_CONN_TYPE_STRAIGHT:
				{
					double dist = cos(atan2((node2->y - node1->y), (node2->x - node1->x)) - 
						atan2(node1->sats[sat1].y, node1->sats[sat1].x)) *
						hypot(node2->x - node1->x, node2->y - node1->y) * 0.75;
				
					double a = hypot(node1->sats[sat1].x, node1->sats[sat1].y);
				
					node1->sats[sat1].x /= a;
					node1->sats[sat1].x *= dist;
					node1->sats[sat1].y /= a;
					node1->sats[sat1].y *= dist;
					fix_satellites(node1, sat1);
				
					dist = cos(atan2((node1->y - node2->y), (node1->x - node2->x)) - 
						atan2(node2->sats[sat2].y, node2->sats[sat2].x)) *
						hypot(node1->x - node2->x, node1->y - node2->y) * 0.75;
				
					a = hypot(node2->sats[sat2].x, node2->sats[sat2].y);
				
					node2->sats[sat2].x /= a;
					node2->sats[sat2].x *= dist;
					node2->sats[sat2].y /= a;
					node2->sats[sat2].y *= dist;
					fix_satellites(node2, sat2);
				}
				
				break;
			
			case SAT_CONN_TYPE_BEZIER:
				{
					double dist = cos(atan2((node2->y - node1->y), (node2->x - node1->x)) - 
						atan2(node1->sats[sat1].y, node1->sats[sat1].x)) *
						hypot(node2->x - node1->x, node2->y - node1->y) * 0.75;
				
					double a = hypot(node1->sats[sat1].x, node1->sats[sat1].y);
				
					node1->sats[sat1].x /= a;
					node1->sats[sat1].x *= dist;
					node1->sats[sat1].y /= a;
					node1->sats[sat1].y *= dist;
					fix_satellites(node1, sat1);
				}
				break;
			}
			break;
		
		case SAT_CONN_TYPE_BEZIER:
			switch(node2->sat_conn_type[opsat2])
			{
			case SAT_CONN_TYPE_UNCONN:
			{
					if(inout(node1->x, node1->y, node1->x + node1->sats[sat1].x, 
						node1->y + node1->sats[sat1].y, node2->x, node2->y))
					{
						node2->sats[sat2].x = -node1->sats[sat1].y;
						node2->sats[sat2].y = node1->sats[sat1].x;
					}
					else
					{
						node2->sats[sat2].x = node1->sats[sat1].y;
						node2->sats[sat2].y = -node1->sats[sat1].x;
					}
					
					double dist = cos(atan2((node1->y - node2->y), (node1->x - node2->x)) - 
						atan2(node2->sats[sat2].y, node2->sats[sat2].x)) *
						hypot(node1->x - node2->x, node1->y - node2->y) * 0.75;
				
					double a = hypot(node2->sats[sat2].x, node2->sats[sat2].y);
				
					node2->sats[sat2].x /= a;
					node2->sats[sat2].x *= dist;
					node2->sats[sat2].y /= a;
					node2->sats[sat2].y *= dist;
					fix_satellites(node2, sat2);
				}
				break;
			
			case SAT_CONN_TYPE_STRAIGHT:
				{
					double dist = cos(atan2((node1->y - node2->y), (node1->x - node2->x)) - 
						atan2(node2->sats[sat2].y, node2->sats[sat2].x)) *
						hypot(node1->x - node2->x, node1->y - node2->y) * 0.75;
				
					double a = hypot(node2->sats[sat2].x, node2->sats[sat2].y);
				
					node2->sats[sat2].x /= a;
					node2->sats[sat2].x *= dist;
					node2->sats[sat2].y /= a;
					node2->sats[sat2].y *= dist;
					fix_satellites(node2, sat2);
				}
				
				break;
			
			case SAT_CONN_TYPE_BEZIER:	// do nothing
				break;
			}
			break;
		}
	}
	
	make_node_effective(node1);
	make_node_effective(node2);
	
	struct conn_t *cconn = new_conn();

	cconn->type = CONN_TYPE_BEZIER;

	cconn->node1 = node1;
	cconn->node2 = node2;
	cconn->sat1 = sat1;
	cconn->sat2 = sat2;

	add_conn_to_curves(cconn);
	
	invalidate_bsp_tree();

	start_working();

	return 1;
}


void delete_conn(struct conn_t *conn)		// always called while not working
{
	remove_conn_from_its_curve(conn);

	LL_REMOVE_ALL(struct t_t, &conn->t0);
	free(conn->verts);
	free_surface(conn->squished_texture);
	
	conn->node1->sat_conn_type[conn->sat1] = SAT_CONN_TYPE_UNCONN;
	conn->node1->num_conns--;

	conn->node2->sat_conn_type[conn->sat2] = SAT_CONN_TYPE_UNCONN;
	conn->node2->num_conns--;
	
	LL_REMOVE(struct conn_t, &conn0, conn);
}


void delete_conns(struct node_t *node)		// always called while not working
{
	struct conn_t *conn = conn0;

	while(conn)
	{
		struct conn_t *next = conn->next;
		
		if(conn->node1 == node || 
			conn->node2 == node)
		{
			delete_conn(conn);
		}

		conn = next;
	}
}


int add_conn_pointer(struct conn_pointer_t **connp0, struct conn_t *conn)
{
	if(!connp0)
		return 0;

	if(!*connp0)
	{
		*connp0 = malloc(sizeof(struct conn_pointer_t));
		(*connp0)->conn = conn;
		(*connp0)->next = NULL;
	}
	else
	{
		struct conn_pointer_t *cconnp = *connp0;

		while(cconnp->next)
		{
			if(cconnp->conn == conn)
				return 0;

			cconnp = cconnp->next;
		}

		if(cconnp->conn != conn)
		{
			cconnp->next = malloc(sizeof(struct conn_pointer_t));
			cconnp = cconnp->next;
			cconnp->conn = conn;
			cconnp->next = NULL;
		}
		else
		{
			return 0;
		}
	}

	return 1;
}


void remove_conn_pointer(struct conn_pointer_t **connp0, struct conn_t *conn)
{
	if(!connp0)
		return;

	struct conn_pointer_t *cconnp = *connp0;

	while(cconnp)
	{
		if(cconnp->conn == conn)
		{
			LL_REMOVE(struct conn_pointer_t, connp0, cconnp);
			break;
		}

		cconnp = cconnp->next;
	}
}


int node_in_straight_conn(struct node_t *node)
{
	struct conn_t *cconn = conn0;
		
	while(cconn)
	{
		if(cconn->type == CONN_TYPE_STRAIGHT &&
			(cconn->node1 == node || 
			cconn->node2 == node))
			return 1;
		
		cconn = cconn->next;
	}
	
	return 0;
}


uint32_t count_conn_pointers(struct conn_pointer_t *connp0)
{
	uint32_t num_pointers = 0;
	struct conn_pointer_t *cconnp = connp0;

	while(cconnp)
	{
		num_pointers++;
		cconnp = cconnp->next;
	}

	return num_pointers;
}


int conn_in_connp_list(struct conn_pointer_t *connp0, struct conn_t *conn)
{
	struct conn_pointer_t *cconnp = connp0;

	while(cconnp)
	{
		if(cconnp->conn == conn)
		{
			return 1;
		}

		cconnp = cconnp->next;
	}
	
	return 0;
}


void get_width_sats(struct conn_t *conn, int *left1, int *right1, int *left2, int *right2)
{
	switch(conn->sat1)
	{
	case 0:
		if(left1)
			*left1 = 0;
		
		if(right1)
			*right1 = 1;
		break;

	case 1:
		if(left1)
			*left1 = 1;
		
		if(right1)
			*right1 = 0;
		break;

	case 2:
		if(left1)
			*left1 = 2;
		
		if(right1)
			*right1 = 3;
		break;

	case 3:
		if(left1)
			*left1 = 3;
		
		if(right1)
			*right1 = 2;
		break;

	default:
		assert(0);
		break;
	}

	switch(conn->sat2)
	{
	case 0:
		if(left2)
			*left2 = 1;
	
		if(right2)
			*right2 = 0;
		break;

	case 1:
		if(left2)
			*left2 = 0;
	
		if(right2)
			*right2 = 1;
		break;

	case 2:
		if(left2)
			*left2 = 3;
	
		if(right2)
			*right2 = 2;
		break;

	case 3:
		if(left2)
			*left2 = 2;
	
		if(right2)
			*right2 = 3;
		break;

	default:
		assert(0);
		break;
	}
}


void generate_t_values_for_conns_with_node(struct node_t *node)
{
	struct conn_t *cconn = conn0;
	
	while(cconn)
	{
		if(cconn->node1 == node || cconn->node2 == node)
		{
			LL_REMOVE_ALL(struct t_t, &cconn->bigt0);
			generate_bigt_values(cconn);
		}
		
		cconn = cconn->next;
	}
}


void invalidate_conns_with_node(struct node_t *node)		// always called while not working
{
	struct conn_t *cconn = conn0;
	
	while(cconn)
	{
		if(cconn->node1 == node || cconn->node2 == node)
		{
			// remove squished_textures from all conns on the curve that this conn is on
			
			struct curve_t *curve = get_curve(cconn);
			assert(curve);
			
			struct conn_pointer_t *cconnp = curve->connp0;
			
			while(cconnp)
			{
				LL_REMOVE_ALL(struct t_t, &cconnp->conn->t0);
				LL_REMOVE_ALL(struct t_t, &cconnp->conn->bigt0);
				generate_bigt_values(cconnp->conn);
			
				free(cconnp->conn->verts);
				cconnp->conn->verts = NULL;
				cconnp->conn->tiled = 0;
				
				remove_conn_from_tiles(cconnp->conn);
				
				free_surface(cconnp->conn->squished_texture);
				cconnp->conn->squished_texture = NULL;
				
				cconnp = cconnp->next;
			}
			
			struct point_t *point = point0;
				
			while(point)
			{
				if(point->curve == curve)
					invalidate_fills_with_point(point);
				
				point = point->next;
			}
		}
		
		cconn = cconn->next;
	}
}	


void draw_straight_conn(struct conn_t *conn)
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
	
	double length = hypot(deltax, deltay);
	
	deltax /= length;
	deltay /= length;
	
	double leftx = deltax * node1->width[left1];
	double lefty = deltay * node1->width[left1];

	double rightx = deltax * node1->width[right1];
	double righty = deltay * node1->width[right1];

	double lx1 = x1 - lefty;
	double ly1 = y1 + leftx;
	double rx1 = x1 + righty;
	double ry1 = y1 - rightx;

	leftx = deltax * node2->width[left2];
	lefty = deltay * node2->width[left2];

	rightx = deltax * node2->width[right2];
	righty = deltay * node2->width[right2];

	double lx2 = x2 - lefty;
	double ly2 = y2 + leftx;
	double rx2 = x2 + righty;
	double ry2 = y2 - rightx;
	
	draw_world_clipped_line(lx1, ly1, lx2, ly2);
	draw_world_clipped_line(rx1, ry1, rx2, ry2);
}


void draw_conic_conn_sub_left(struct conn_t *conn, float cx, float cy, float hyp, 
	float theta1, float theta2, float t1, float t2)
{
	double x1, y1, x2, y2;
	double dx1, dy1, dx2, dy2;
	int sx1, sy1, sx2, sy2;
	
	double theta = theta1 + (theta2 - theta1) * t1;
	
	x1 = -sin(theta) * hyp + cx;
	y1 = cos(theta) * hyp + cy;
	dx1 = -sin(theta);
	dy1 = cos(theta);
	
	theta = theta1 + (theta2 - theta1) * t2;
	
	x2 = -sin(theta) * hyp + cx;
	y2 = cos(theta) * hyp + cy;
	dx2 = -sin(theta);
	dy2 = cos(theta);
	
	double delta_dist = hypot(dx1, dy1);
	dx1 /= delta_dist;
	dy1 /= delta_dist;

	delta_dist = hypot(dx2, dy2);
	dx2 /= delta_dist;
	dy2 /= delta_dist;
	
	int left1, right1, left2, right2;
	get_width_sats(conn, &left1, &right1, &left2, &right2);

	double leftwidth = (conn->node2->width[left2] - conn->node1->width[left1]) * t1 + 
		conn->node1->width[left1];

	x1 += dx1 * leftwidth;
	y1 += dy1 * leftwidth;
	
	leftwidth = (conn->node2->width[left2] - conn->node1->width[left1]) * t2 + 
		conn->node1->width[left1];

	x2 += dx2 * leftwidth;
	y2 += dy2 * leftwidth;
	
	world_to_screen(x1, y1, &sx1, &sy1);
	world_to_screen(x2, y2, &sx2, &sy2);
	
	if(((sx2 - sx1) * (sx2 - sx1) + (sy2 - sy1) * (sy2 - sy1)) > CONN_MAX_SEG_SIZE_SQUARED)
	{
		float midt = (t1 + t2) / 2;
		
		draw_conic_conn_sub_left(conn, cx, cy, hyp, theta1, theta2, t1, midt);
		draw_conic_conn_sub_left(conn, cx, cy, hyp, theta1, theta2, midt, t2);
	}
	else
	{
		draw_world_clipped_line(x1, y1, x2, y2);
	}
}


void draw_conic_conn_sub_right(struct conn_t *conn, float cx, float cy, float hyp, 
	float theta1, float theta2, float t1, float t2)
{
	double x1, y1, x2, y2;
	double dx1, dy1, dx2, dy2;
	int sx1, sy1, sx2, sy2;
	
	double theta = theta1 + (theta2 - theta1) * t1;
	
	x1 = -sin(theta) * hyp + cx;
	y1 = cos(theta) * hyp + cy;
	dx1 = -sin(theta);
	dy1 = cos(theta);
	
	theta = theta1 + (theta2 - theta1) * t2;
	
	x2 = -sin(theta) * hyp + cx;
	y2 = cos(theta) * hyp + cy;
	dx2 = -sin(theta);
	dy2 = cos(theta);
	
	double delta_dist = hypot(dx1, dy1);
	dx1 /= delta_dist;
	dy1 /= delta_dist;

	delta_dist = hypot(dx2, dy2);
	dx2 /= delta_dist;
	dy2 /= delta_dist;
	
	int left1, right1, left2, right2;
	get_width_sats(conn, &left1, &right1, &left2, &right2);

	double rightwidth = (conn->node2->width[left2] - conn->node1->width[left1]) * t1 + 
		conn->node1->width[left1];

	x1 -= dx1 * rightwidth;
	y1 -= dy1 * rightwidth;
	
	rightwidth = (conn->node2->width[left2] - conn->node1->width[left1]) * t2 + 
		conn->node1->width[left1];

	x2 -= dx2 * rightwidth;
	y2 -= dy2 * rightwidth;
	
	world_to_screen(x1, y1, &sx1, &sy1);
	world_to_screen(x2, y2, &sx2, &sy2);
	
	if(((sx2 - sx1) * (sx2 - sx1) + (sy2 - sy1) * (sy2 - sy1)) > CONN_MAX_SEG_SIZE_SQUARED)
	{
		float midt = (t1 + t2) / 2;
		
		draw_conic_conn_sub_right(conn, cx, cy, hyp, theta1, theta2, t1, midt);
		draw_conic_conn_sub_right(conn, cx, cy, hyp, theta1, theta2, midt, t2);
	}
	else
	{
		draw_world_clipped_line(x1, y1, x2, y2);
	}
}


void draw_conic_conn(struct conn_t *conn)
{
	double ax1 = conn->node1->x;
	double ay1 = conn->node1->y;
	double ax2 = conn->node1->x - conn->node1->sats[conn->sat1].y;
	double ay2 = conn->node1->y + conn->node1->sats[conn->sat1].x;
	
	double bx1 = conn->node2->x;
	double by1 = conn->node2->y;
	double bx2 = conn->node2->x - conn->node2->sats[conn->sat2].y;
	double by2 = conn->node2->y + conn->node2->sats[conn->sat2].x;
	
	double nx = ay1 - ay2;
	double ny = -(ax1 - ax2);
	
	double numer = nx * (bx1 - ax1) + 
		ny * (by1 - ay1);
	
	double denom = (-nx) * (bx2 - bx1) + 
		(-ny) * (by2 - by1);
	
	double cx, cy;
	
	if(fabs(denom) < 0.1)	// lines are parallel
	{
		cx = (ax1 + bx1) / 2.0;
		cy = (ay1 + by1) / 2.0;
	}
	else
	{
		double t = numer / denom;
	
		cx = bx1 + (bx2 - bx1) * t;
		cy = by1 + (by2 - by1) * t;
	}
	
	double theta1 = atan2(-(conn->node1->x - cx), conn->node1->y - cy);
	double hyp = hypot(conn->node1->x - cx, conn->node1->y - cy);
	
	double theta2 = atan2(-(conn->node2->x - cx), conn->node2->y - cy);
	
	if(inout(conn->node1->x, conn->node1->y, 
		conn->node1->x + conn->node1->sats[conn->sat1].x,
		conn->node1->y + conn->node1->sats[conn->sat1].y, cx, cy))
	{
		if(theta1 < theta2)
			theta1 += M_PI * 2;
	}
	else
	{
		if(theta2 < theta1)
			theta2 += M_PI * 2;
	}

	draw_conic_conn_sub_left(conn, cx, cy, hyp, theta1, theta2, 0, 1);
	draw_conic_conn_sub_right(conn, cx, cy, hyp, theta1, theta2, 0, 1);
}


int draw_bezier_conn_recursion_count;

void draw_bezier_conn_sub_left(struct conn_t *conn, struct bezier_t *bezier, double t1, double t2)
{
	if(draw_bezier_conn_recursion_count++ > 200)
		return;
	
	float x1, y1, x2, y2;
	float dx1, dy1, dx2, dy2;
	int sx1, sy1, sx2, sy2;
	
	BRZ(bezier, t1, &x1, &y1);
	BRZ(bezier, t2, &x2, &y2);
	
	deltaBRZ(bezier, t1, &dx1, &dy1);
	deltaBRZ(bezier, t2, &dx2, &dy2);

	double delta_dist = hypot(dx1, dy1);
	dx1 /= delta_dist;
	dy1 /= delta_dist;

	delta_dist = hypot(dx2, dy2);
	dx2 /= delta_dist;
	dy2 /= delta_dist;
	
	int left1, right1, left2, right2;
	get_width_sats(conn, &left1, &right1, &left2, &right2);

	double leftwidth = (conn->node2->width[left2] - conn->node1->width[left1]) * t1 + 
		conn->node1->width[left1];

	x1 -= dy1 * leftwidth;
	y1 += dx1 * leftwidth;
	
	world_to_screen(x1, y1, &sx1, &sy1);
	
	leftwidth = (conn->node2->width[left2] - conn->node1->width[left1]) * t2 + 
		conn->node1->width[left1];

	x2 -= dy2 * leftwidth;
	y2 += dx2 * leftwidth;
	
	world_to_screen(x2, y2, &sx2, &sy2);
	
	if(((sx2 - sx1) * (sx2 - sx1) + (sy2 - sy1) * (sy2 - sy1)) > CONN_MAX_SEG_SIZE_SQUARED)
	{
		double midt = (t1 + t2) / 2;
		
		draw_bezier_conn_sub_left(conn, bezier, t1, midt);
		draw_bezier_conn_sub_left(conn, bezier, midt, t2);
	}
	else
	{
		draw_world_clipped_line(x1, y1, x2, y2);
	}
}


void draw_bezier_conn_sub_right(struct conn_t *conn, struct bezier_t *bezier, double t1, double t2)
{
	if(draw_bezier_conn_recursion_count++ > 200)
		return;
	
	float x1, y1, x2, y2;
	float dx1, dy1, dx2, dy2;
	int sx1, sy1, sx2, sy2;
	
	BRZ(bezier, t1, &x1, &y1);
	BRZ(bezier, t2, &x2, &y2);
	
	deltaBRZ(bezier, t1, &dx1, &dy1);
	deltaBRZ(bezier, t2, &dx2, &dy2);

	double delta_dist = hypot(dx1, dy1);
	dx1 /= delta_dist;
	dy1 /= delta_dist;

	delta_dist = hypot(dx2, dy2);
	dx2 /= delta_dist;
	dy2 /= delta_dist;
	
	int left1, right1, left2, right2;
	get_width_sats(conn, &left1, &right1, &left2, &right2);

	double rightwidth = (conn->node2->width[right2] - conn->node1->width[right1]) * t1 + 
		conn->node1->width[right1];

	x1 += dy1 * rightwidth;
	y1 -= dx1 * rightwidth;
	
	world_to_screen(x1, y1, &sx1, &sy1);
	
	rightwidth = (conn->node2->width[right2] - conn->node1->width[right1]) * t2 + 
		conn->node1->width[right1];

	x2 += dy2 * rightwidth;
	y2 -= dx2 * rightwidth;
	
	world_to_screen(x2, y2, &sx2, &sy2);
	
	if(((sx2 - sx1) * (sx2 - sx1) + (sy2 - sy1) * (sy2 - sy1)) > CONN_MAX_SEG_SIZE_SQUARED)
	{
		double midt = (t1 + t2) / 2;
		
		draw_bezier_conn_sub_right(conn, bezier, t1, midt);
		draw_bezier_conn_sub_right(conn, bezier, midt, t2);
	}
	else
	{
		draw_world_clipped_line(x1, y1, x2, y2);
	}
}


void draw_bezier_conn(struct conn_t *conn)
{
	struct bezier_t bezier;
	
	bezier.x1 = conn->node1->effective_x[conn->sat1];
	bezier.y1 = conn->node1->effective_y[conn->sat1];
	bezier.x2 = conn->node1->effective_x[conn->sat1] + conn->node1->sats[conn->sat1].x;
	bezier.y2 = conn->node1->effective_y[conn->sat1] + conn->node1->sats[conn->sat1].y;
	bezier.x3 = conn->node2->effective_x[conn->sat2] + conn->node2->sats[conn->sat2].x;
	bezier.y3 = conn->node2->effective_y[conn->sat2] + conn->node2->sats[conn->sat2].y;
	bezier.x4 = conn->node2->effective_x[conn->sat2];
	bezier.y4 = conn->node2->effective_y[conn->sat2];
	
//	if(bezier.x1 == bezier.x4 && bezier.y1 == bezier.y4)
//		return;
	
	draw_bezier_conn_recursion_count = 0;
	draw_bezier_conn_sub_left(conn, &bezier, 0, 1);
	
	draw_bezier_conn_recursion_count = 0;
	draw_bezier_conn_sub_right(conn, &bezier, 0, 1);
}


void draw_conn(struct conn_t *conn)
{
	switch(conn->type)
	{
	case CONN_TYPE_STRAIGHT:
		draw_straight_conn(conn);
		break;

	case CONN_TYPE_CONIC:
		draw_conic_conn(conn);
		break;
	
	case CONN_TYPE_BEZIER:
		draw_bezier_conn(conn);
		break;
	}
}


void get_conn_limits(struct conn_t *conn, float *minx, float *maxx, float *miny, float *maxy)
{
	// TODO : handle straight connections separately
	
	struct bezier_t bezier;

	bezier.x1 = conn->node1->effective_x[conn->sat1];
	bezier.y1 = conn->node1->effective_y[conn->sat1];
	bezier.x2 = conn->node1->effective_x[conn->sat1] + conn->node1->sats[conn->sat1].x;
	bezier.y2 = conn->node1->effective_y[conn->sat1] + conn->node1->sats[conn->sat1].y;
	bezier.x3 = conn->node2->effective_x[conn->sat2] + conn->node2->sats[conn->sat2].x;
	bezier.y3 = conn->node2->effective_y[conn->sat2] + conn->node2->sats[conn->sat2].y;
	bezier.x4 = conn->node2->effective_x[conn->sat2];
	bezier.y4 = conn->node2->effective_y[conn->sat2];
		
	int left1, right1, left2, right2;

	get_width_sats(conn, &left1, &right1, &left2, &right2);

	float x, y, deltax, deltay;
	double leftx, lefty, rightx, righty;

	BRZ(&bezier, 0.0f, &x, &y);
	deltaBRZ(&bezier, 0.0f, &deltax, &deltay);

	double length = hypot(deltax, deltay);

	deltax /= length;
	deltay /= length;

	leftx = deltax * conn->node1->width[left1];
	lefty = deltay * conn->node1->width[left1];

	rightx = deltax * conn->node1->width[right1];
	righty = deltay * conn->node1->width[right1];

	*minx = *maxx = x - lefty;
	*miny = *maxy = y + leftx;
	
	double lx = x + righty;
	
	if(lx < *minx)
		*minx = lx;
	
	if(lx > *maxx)
		*maxx = lx;
	
	double ly = y - rightx;

	if(ly < *miny)
		*miny = ly;
	
	if(ly > *maxy)
		*maxy = ly;

	int s;

	for(s = 1; s <= NUMSEGMENTS; s++)
	{
		double t = (double)s / NUMSEGMENTS;
		
		BRZ(&bezier, t, &x, &y);
		deltaBRZ(&bezier, t, &deltax, &deltay);
		
		length = hypot(deltax, deltay);
		
		deltax /= length;
		deltay /= length;
		
		double leftwidth = (conn->node2->width[left2] - conn->node1->width[left1]) * t + 
			conn->node1->width[left1];
		double rightwidth = (conn->node2->width[right2] - conn->node1->width[right1]) * t + 
			conn->node1->width[right1];
		
		leftx = deltax * leftwidth;
		lefty = deltay * leftwidth;
		
		rightx = deltax * rightwidth;
		righty = deltay * rightwidth;
		
		lx = x - lefty;
		
		if(lx < *minx)
			*minx = lx;
		
		if(lx > *maxx)
			*maxx = lx;
		
		ly = y + leftx;
		
		if(ly < *miny)
			*miny = ly;
		
		if(ly > *maxy)
			*maxy = ly;
		
		lx = x + righty;
		
		if(lx < *minx)
			*minx = lx;
		
		if(lx > *maxx)
			*maxx = lx;
		
		ly = y - rightx;
		
		if(ly < *miny)
			*miny = ly;
		
		if(ly > *maxy)
			*maxy = ly;
	}
}


void update_fully_rendered_conns()
{
	struct conn_t *cconn = conn0;

	while(cconn)
	{
		cconn->fully_rendered = conn_fully_rendered(cconn);
		cconn = cconn->next;
	}
}


void delete_all_conns()		// always called when not working
{
	struct conn_t *cconn = conn0;

	while(cconn)
	{
		LL_REMOVE_ALL(struct t_t, &cconn->t0);
		free(cconn->verts);
		free_surface(cconn->squished_texture);

		cconn = cconn->next;
	}

	LL_REMOVE_ALL(struct conn_t, &conn0);
}


uint32_t count_conns()
{
	uint32_t num_conns = 0;
	struct conn_t *cconn = conn0;

	while(cconn)
	{
		num_conns++;
		cconn = cconn->next;
	}

	return num_conns;
}


void gzwrite_conns(gzFile file)
{
	uint32_t num_conns = count_conns();
	gzwrite(file, &num_conns, 4);

	struct conn_t *cconn = conn0;
	uint32_t conn_index = 0;

	while(cconn)
	{
		cconn->index = conn_index++;
		gzwrite(file, &cconn->orientation, 1);
		gzwrite(file, &cconn->type, 1);
		gzwrite(file, &cconn->node1->index, 4);
		gzwrite(file, &cconn->node2->index, 4);
		gzwrite(file, &cconn->sat1, 1);
		gzwrite(file, &cconn->sat2, 1);
		
		cconn = cconn->next;

	}
}


int gzread_conns(gzFile file)
{
	uint32_t num_conns;
	if(gzread(file, &num_conns, 4) != 4)
		goto error;

	uint32_t cconn_index = 0;

	while(cconn_index < num_conns)
	{
		struct conn_t *cconn = new_conn();

		cconn->index = cconn_index;

		if(gzread(file, &cconn->orientation, 1) != 1)
			goto error;

		if(gzread(file, &cconn->type, 1) != 1)
			goto error;

		int node_index;
		if(gzread(file, &node_index, 4) != 4)
			goto error;

		cconn->node1 = get_node_from_index(node_index);
		if(!cconn->node1)
			goto error;

		if(gzread(file, &node_index, 4) != 4)
			goto error;

		cconn->node2 = get_node_from_index(node_index);
		if(!cconn->node2)
			goto error;

		if(gzread(file, &cconn->sat1, 1) != 1)
			goto error;

		if(gzread(file, &cconn->sat2, 1) != 1)
			goto error;
		
		generate_bigt_values(cconn);

		cconn_index++;
	}

	return 1;

error:

	return 0;
}


void gzwrite_conn_pointer_list(gzFile file, struct conn_pointer_t *connp0)
{
	uint32_t num_conns = count_conn_pointers(connp0);
	gzwrite(file, &num_conns, 4);

	struct conn_pointer_t *cconnp = connp0;

	while(cconnp)
	{
		gzwrite(file, &cconnp->conn->index, 4);
		cconnp = cconnp->next;
	}
}


struct conn_t *get_conn_from_index(uint32_t index)
{
	struct conn_t *cconn = conn0;

	while(cconn)
	{
		if(cconn->index == index)
			return cconn;

		cconn = cconn->next;
	}

	return NULL;
}


struct conn_pointer_t *gzread_conn_pointer_list(gzFile file)
{
	uint32_t num_conns;
	gzread(file, &num_conns, 4);

	struct conn_pointer_t *connp0 = NULL;

	while(num_conns)
	{
		struct conn_pointer_t connp;

		uint32_t conn_index;
		if(gzread(file, &conn_index, 4) != 4)
			goto error;

		connp.conn = get_conn_from_index(conn_index);
		if(!connp.conn)
			goto error;

		LL_ADD_TAIL(struct conn_pointer_t, &connp0, &connp);

		num_conns--;
	}

	return connp0;

error:

	LL_REMOVE_ALL(struct conn_pointer_t, &connp0);
	return 0;
}


int check_for_unverticied_conns()
{
	struct conn_t *cconn = conn0;
	
	while(cconn)
	{
		struct curve_t *curve = get_curve(cconn);
		assert(curve);
		
		switch(curve->fill_type)
		{
		case CURVE_SOLID:
			if(!cconn->verts)
				return 1;
			break;
			
		case CURVE_TEXTURE:
			if(curve->texture_surface && !cconn->verts)
				return 1;
			break;
		}
		
		cconn = cconn->next;
	}
	
	return 0;
}


int check_for_unsquished_conns()
{
	struct conn_t *cconn = conn0;
	
	while(cconn)
	{
		struct curve_t *curve = get_curve(cconn);
		assert(curve);
		
		switch(curve->fill_type)
		{
		case CURVE_SOLID:
			if(!cconn->squished_texture)
				return 1;
			break;
			
		case CURVE_TEXTURE:
			if(curve->texture_surface && !cconn->squished_texture)
				return 1;
			break;
		}
		
		cconn = cconn->next;
	}
	
	return 0;
}


int check_for_untiled_conns()
{
	struct conn_t *cconn = conn0;
	
	while(cconn)
	{
		struct curve_t *curve = get_curve(cconn);
		assert(curve);
		
		switch(curve->fill_type)
		{
		case CURVE_SOLID:
			if(!cconn->tiled)
				return 1;
			break;
			
		case CURVE_TEXTURE:
			if(curve->texture_surface && !cconn->tiled)
				return 1;
			break;
		}
		
		cconn = cconn->next;
	}
	
	return 0;
}


void finished_tiling_all_conns()
{
	struct conn_t *cconn = conn0;
	
	while(cconn)
	{
		struct curve_t *curve = get_curve(cconn);
		assert(curve);
		
		if(!cconn->tiled)
			cconn->tiled = 1;
		
		assert(cconn->squished_texture);
		
		cconn = cconn->next;
	}
}


int generate_t_values(struct conn_t *conn)
{
	struct bezier_t bezier;
	
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

	struct t_t *t0;
	int count;
	float length;

	leave_main_lock();
	
	if(!generate_bezier_ts(&bezier, &t0, &count, &length))
		return 0;
	
	if(!worker_try_enter_main_lock())
	{
		LL_REMOVE_ALL(struct t_t, &t0);
		return 0;
	}
	
	conn->t0 = t0;
	conn->t_count = count;
	conn->t_length = length;
	
	return 1;
}


void generate_bigt_values(struct conn_t *conn)
{
	switch(conn->type)
	{
	case CONN_TYPE_STRAIGHT:
	
		{	
			struct node_t *node1, *node2;
			
			if(!conn->orientation)
			{
				node1 = conn->node1;
				node2 = conn->node2;
			}
			else
			{
				node1 = conn->node2;
				node2 = conn->node1;
			}
			
			conn->bigt_length = hypot(node2->x - node1->x, node2->y - node1->y);
			conn->bigt_count = conn->bigt_length / 64.0;
			
			int i = conn->bigt_count;
			
			double t = 0.0;
			double t_incr = 1.0 / (double)conn->bigt_count;
			
			double x = node1->x, y = node1->y;
			double x_incr = (node2->x - node1->x) / conn->bigt_count;
			double y_incr = (node2->y - node1->y) / conn->bigt_count;
			
			double l = hypot(x_incr, y_incr);
			
			double deltax = x_incr / l;
			double deltay = y_incr / l;
			
			struct t_t **ct0 = &(conn->bigt0);
			
			while(i)
			{
				struct t_t ct;
					
				ct.t1 = t;
				t += t_incr;
				ct.t2 = t;
				
				ct.x1 = x;
				ct.y1 = y;
				x += x_incr;
				y += y_incr;
				ct.x2 = x;
				ct.y2 = y;
				
				ct.dist = conn->bigt_length / conn->bigt_count;
				
				ct.deltax1 = deltax;
				ct.deltay1 = deltay;
				ct.deltax2 = deltax;
				ct.deltay2 = deltay;
	
				LL_ADD(struct t_t, ct0, &ct);
				ct0 = &(*ct0)->next;
	
				i--;
			}
		}
		
		break;
		
		
	case CONN_TYPE_CONIC:
		
		
		generate_conic_bigts(conn);
		
		break;
		
		
		
		
	case CONN_TYPE_BEZIER:
		
		{
			struct bezier_t bezier;
			
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
		
			struct t_t *t0;
			int count;
			float length;
		
			generate_bezier_bigts(&bezier, &t0, &count, &length);
			
			conn->bigt0 = t0;
			conn->bigt_count = count;
			conn->bigt_length = length;
		}
		
		break;
		
	}
}


//
// WORKER THREAD FUNCTIONS
//


int tile_all_conns()
{
	if(!worker_try_enter_main_lock())
		return 0;

	struct conn_t *conn = conn0;

	while(conn)
	{
		struct curve_t *curve = get_curve(conn);
		assert(curve);
		
		switch(curve->fill_type)
		{
		case CURVE_SOLID:
			if(!conn->tiled)
			{
				worker_leave_main_lock();
				
				if(!tile_conn(conn))
					return 0;
				
				if(!worker_try_enter_main_lock())
					return 0;
			}
			break;
			
		case CURVE_TEXTURE:
			if(curve->texture_surface && !conn->tiled)
			{
				worker_leave_main_lock();
				
				if(!tile_conn(conn))
					return 0;
				
				if(!worker_try_enter_main_lock())
					return 0;
			}
			break;
		}
		
		conn = conn->next;
	}
	
	worker_leave_main_lock();
	
	return 1;
}


int generate_straight_verticies(struct conn_t *conn)
{
	struct node_t *node1, *node2;
	int left1, right1, left2, right2;
	int sat1, sat2;
	
	if(!conn->orientation)
	{
		node1 = conn->node1;
		node2 = conn->node2;
		sat1 = conn->sat1;
		sat2 = conn->sat2;
	}
	else
	{
		node1 = conn->node2;
		node2 = conn->node1;
		sat1 = conn->sat2;
		sat2 = conn->sat1;
	}

	struct curve_t *curve = get_curve(conn);
	assert(curve);

	
	int height = 0;

	switch(curve->fill_type)
	{
	case CURVE_SOLID:
		height = 100;
		break;
	
	case CURVE_TEXTURE:
		height = curve->texture_surface->height;
		break;
	}
	
	get_width_sats(conn, &left1, &right1, &left2, &right2);
	double deltax = node2->effective_x[sat2] - node1->effective_x[sat1];
	double deltay = node2->effective_y[sat2] - node1->effective_y[sat1];

	conn->t_length = hypot(deltax, deltay);
	conn->t_count = conn->t_length * 2.0;
	
	deltax /= conn->t_length;
	deltay /= conn->t_length;


	struct vertex_t *verts = malloc(sizeof(struct vertex_t) * (conn->t_count + 1) * (height + 1));
	struct vertex_t *cvert = verts;

	
	int v, s;
	
	for(s = 0; s <= conn->t_count; s++)
	{
		double t = ((double)s / conn->t_count);
		
		double x = node1->effective_x[sat1] + 
			(node2->effective_x[sat2] - node1->effective_x[sat1]) * t;
		double y = node1->effective_y[sat1] + 
			(node2->effective_y[sat2] - node1->effective_y[sat1]) * t;

		double rightwidth = (node2->width[right2] - node1->width[right1]) * t + 
			node1->width[right1];
		double fullwidth = rightwidth + (node2->width[left2] - node1->width[left1]) * t + 
			node1->width[left1];
		
		for(v = 0; v <= height; v++)
		{
			cvert->x = (float)(x - deltay * (fullwidth * ((double)v / height) - rightwidth));
			cvert->y = (float)(y + deltax * (fullwidth * ((double)v / height) - rightwidth));
	
			cvert++;
		}
		
		if(in_lock_check_stop_callback())
		{
			free(verts);
			return 0;
		}
	}

	conn->verts = verts;
	return 1;
}


int generate_curved_verticies(struct conn_t *conn)
{
	if(!conn->t0)
	{
		switch(conn->type)
		{
		case CONN_TYPE_CONIC:
			if(!generate_conic_ts(conn))
				return 0;
			break;
			
		case CONN_TYPE_BEZIER:
			if(!generate_t_values(conn))
				return 0;
			break;
		}
	}
					
	struct curve_t *curve = get_curve(conn);
	assert(curve);


	struct bezier_t bezier;
	
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

	int height = 0;

	switch(curve->fill_type)
	{
	case CURVE_SOLID:
		height = 100;
		break;
	
	case CURVE_TEXTURE:
		height = curve->texture_surface->height;
		break;
	}

	int width = conn->t_count;

	struct vertex_t *verts = malloc(sizeof(struct vertex_t) * (width + 1) * (height + 1));
	struct vertex_t *cvert = verts;


	struct node_t *node1, *node2;
	int left1, right1, left2, right2;
	
	if(!conn->orientation)
	{
		node1 = conn->node1;
		node2 = conn->node2;
	}
	else
	{
		node1 = conn->node2;
		node2 = conn->node1;
	}
		
	get_width_sats(conn, &left1, &right1, &left2, &right2);
	

	double fullwidth = node1->width[left1] + node1->width[right1];
	int v;

	struct t_t *ct = conn->t0;

	for(v = 0; v <= height; v++)
	{
		cvert->x = (float)(ct->x1 - 
			ct->deltay1 * (fullwidth * ((double)v / height) - node1->width[right1]));
		cvert->y = (float)(ct->y1 + 
			ct->deltax1 * (fullwidth * ((double)v / height) - node1->width[right1]));

		cvert++;
	}

	while(ct)
	{
		double rightwidth = (node2->width[right2] - node1->width[right1]) * ct->t2 + 
			node1->width[right1];
		fullwidth = rightwidth + (node2->width[left2] - node1->width[left1]) * ct->t2 + 
			node1->width[left1];

		int v;

		for(v = 0; v <= height; v++)
		{
			cvert->x = (float)(ct->x2 - 
				ct->deltay2 * (fullwidth * ((double)v / height) - rightwidth));
			cvert->y = (float)(ct->y2 + 
				ct->deltax2 * (fullwidth * ((double)v / height) - rightwidth));

			cvert++;
		}

		if(in_lock_check_stop_callback())
		{
			free(verts);
			return 0;
		}
		
		ct = ct->next;
	}
	
	conn->verts = verts;

	return 1;
}


void generate_verticies_for_all_conns()
{
	if(!worker_try_enter_main_lock())
		return;

	struct conn_t *cconn = conn0;

	while(cconn)
	{
		struct curve_t *curve = get_curve(cconn);
		assert(curve);
		
		switch(curve->fill_type)
		{
		case CURVE_SOLID:
			if(!cconn->verts)
			{
				switch(cconn->type)
				{
				case CONN_TYPE_STRAIGHT:
					if(!generate_straight_verticies(cconn))
						return;
					break;
					
				case CONN_TYPE_CONIC:
				case CONN_TYPE_BEZIER:
					if(!generate_curved_verticies(cconn))
						return;
					break;
				}
			}
			break;
			
		case CURVE_TEXTURE:
			if(curve->texture_surface && !cconn->verts)
			{
				switch(cconn->type)
				{
				case CONN_TYPE_STRAIGHT:
					if(!generate_straight_verticies(cconn))
						return;
					break;
					
				case CONN_TYPE_CONIC:
				case CONN_TYPE_BEZIER:
					if(!generate_curved_verticies(cconn))
						return;
					break;
				}
			}
			break;
		}

		cconn = cconn->next;
	}
	
	worker_leave_main_lock();
}


int generate_curved_squished_texture(struct conn_t *conn, struct curve_t *curve)
{
	double curve_length = 0.0, dist = 0.0;

	struct conn_pointer_t *cconnp = curve->connp0;

	while(cconnp)
	{
		if(cconnp->conn == conn)
			dist = curve_length;

		curve_length += cconnp->conn->t_length;
		cconnp = cconnp->next;
	}

	double pixel_length;
	
	if(curve->fixed_reps)
		pixel_length = (curve_length / curve->reps) / curve->texture_surface->width;
	else
		pixel_length = curve->texture_length / curve->texture_surface->width;

	struct surface_t *squished_texture = new_surface(SURFACE_16BIT, 
		curve->texture_surface->height, conn->t_count);

	int y, dstx, srcx;
	
	int *srcxs = malloc(sizeof(int) * (conn->t_count + 1));
	double *dists = malloc(sizeof(double) * (conn->t_count + 1));

	struct t_t *ct = conn->t0;
	
	for(dstx = 0; dstx < conn->t_count; dstx++)
	{
		srcxs[dstx] = (int)(dist / pixel_length);
		dists[dstx] = dist;

		dist += ct->dist;
		ct = ct->next;
	}
	
	srcxs[conn->t_count] = (int)(dist / pixel_length);
	dists[conn->t_count] = dist;

	for(y = 0; y < curve->texture_surface->height; y++)
	{
		ct = conn->t0;
		
		for(dstx = 0; dstx < conn->t_count; dstx++)
		{
			double red = 0.0, green = 0.0, blue = 0.0;
			
			for(srcx = srcxs[dstx]; srcx <= srcxs[dstx + 1]; srcx++)
			{
				double x1 = max(srcx * pixel_length, dists[dstx]);
				double x2 = min((srcx + 1) * pixel_length, dists[dstx + 1]);
				double rel = (x2 - x1) / ct->dist;
				
				int msrcx = srcx % curve->texture_surface->width;

				red += rel * get_double_red(((uint16_t*)curve->texture_surface->buf)
					[y * curve->texture_surface->width + msrcx]);
				green += rel * get_double_green(((uint16_t*)curve->texture_surface->buf)
					[y * curve->texture_surface->width + msrcx]);
				blue += rel * get_double_blue(((uint16_t*)curve->texture_surface->buf)
					[y * curve->texture_surface->width + msrcx]);
			}
			
			((uint16_t*)squished_texture->buf)[dstx * curve->texture_surface->height + y] = 
				convert_doubles_to_16bit(red, green, blue);
			
			ct = ct->next;
		}
		
		if(in_lock_check_stop_callback())
		{
			free(srcxs);
			free(dists);
			free_surface(squished_texture);
			return 0;
		}
	}

	conn->squished_texture = squished_texture;
	
	free(srcxs);
	free(dists);

	return 1;
}
	

int generate_straight_squished_texture(struct conn_t *conn, struct curve_t *curve)
{
	double curve_length = 0.0, dist = 0.0;

	struct conn_pointer_t *cconnp = curve->connp0;

	while(cconnp)
	{
		if(cconnp->conn == conn)
			dist = curve_length;

		curve_length += cconnp->conn->t_length;
		cconnp = cconnp->next;
	}

	double pixel_length;
	
	if(curve->fixed_reps)
		pixel_length = (curve_length / curve->reps) / curve->texture_surface->width;
	else
		pixel_length = curve->texture_length / curve->texture_surface->width;

	struct surface_t *squished_texture = new_surface(SURFACE_16BIT, 
		conn->t_count, curve->texture_surface->height);

	int y, dstx, srcx;
	
	int *srcxs = malloc(sizeof(int) * (conn->t_count + 1));
	double *dists = malloc(sizeof(double) * (conn->t_count + 1));

	
	for(dstx = 0; dstx < conn->t_count; dstx++)
	{
		srcxs[dstx] = (int)(dist / pixel_length);
		dists[dstx] = dist;

		dist += conn->t_length / conn->t_count;
	}
	
	srcxs[conn->t_count] = (int)(dist / pixel_length);
	dists[conn->t_count] = dist;

	for(y = 0; y < curve->texture_surface->height; y++)
	{
		for(dstx = 0; dstx < conn->t_count; dstx++)
		{
			double red = 0.0, green = 0.0, blue = 0.0;
			
			for(srcx = srcxs[dstx]; srcx <= srcxs[dstx + 1]; srcx++)
			{
				double x1 = max(srcx * pixel_length, dists[dstx]);
				double x2 = min((srcx + 1) * pixel_length, dists[dstx + 1]);
				double rel = (x2 - x1) / (conn->t_length / conn->t_count);
				
				int msrcx = srcx % curve->texture_surface->width;

				red += rel * get_double_red(((uint16_t*)curve->texture_surface->buf)
					[y * curve->texture_surface->width + msrcx]);
				green += rel * get_double_green(((uint16_t*)curve->texture_surface->buf)
					[y * curve->texture_surface->width + msrcx]);
				blue += rel * get_double_blue(((uint16_t*)curve->texture_surface->buf)
					[y * curve->texture_surface->width + msrcx]);
			}
			
			((uint16_t*)squished_texture->buf)[y * conn->t_count + dstx] = 
				convert_doubles_to_16bit(red, green, blue);
		}

		if(in_lock_check_stop_callback())
		{
			free(srcxs);
			free(dists);
			free_surface(squished_texture);
			return 0;
		}
	}

	conn->squished_texture = squished_texture;
	
	free(srcxs);
	free(dists);

	return 1;
}
	

int generate_curved_squished_texture_with_alpha(struct conn_t *conn, struct curve_t *curve)
{
	switch(curve->fill_type)
	{
	case CURVE_SOLID:
		{
			struct surface_t *squished_texture = new_surface(SURFACE_24BITALPHA8BIT, 
				100, conn->t_count);
			
			int i;
			
			for(i = 0; i < conn->t_count * 100; i++)
			{
				((uint8_t*)squished_texture->buf)[i * 3] = curve->red;
				((uint8_t*)squished_texture->buf)[i * 3 + 1] = curve->green;
				((uint8_t*)squished_texture->buf)[i * 3 + 2] = curve->blue;
				((uint8_t*)squished_texture->alpha_buf)[i] = curve->alpha;
			}
			
			conn->squished_texture = squished_texture;
		}
		
		break;
	
	case CURVE_TEXTURE:
		{
			double curve_length = 0.0, dist = 0.0;
		
			struct conn_pointer_t *cconnp = curve->connp0;
		
			while(cconnp)
			{
				if(cconnp->conn == conn)
					dist = curve_length;
		
				curve_length += cconnp->conn->t_length;
				cconnp = cconnp->next;
			}
			
			double pixel_length;
			
			if(curve->fixed_reps)
				pixel_length = (curve_length / curve->reps) / curve->texture_surface->width;
			else
				pixel_length = curve->texture_length / curve->texture_surface->width;
		
			struct surface_t *squished_texture = new_surface(SURFACE_24BITALPHA8BIT, 
					curve->texture_surface->height, conn->t_count);
		
			int y, dstx, srcx;
			
			int *srcxs = malloc(sizeof(int) * (conn->t_count + 1));
			double *dists = malloc(sizeof(double) * (conn->t_count + 1));
		
			struct t_t *ct = conn->t0;
			
			for(dstx = 0; dstx < conn->t_count; dstx++)
			{
				srcxs[dstx] = (int)(dist / pixel_length);
				dists[dstx] = dist;
		
				dist += ct->dist;
				ct = ct->next;
			}
			
			srcxs[conn->t_count] = (int)(dist / pixel_length);
			dists[conn->t_count] = dist;
		
			for(y = 0; y < curve->texture_surface->height; y++)
			{
				ct = conn->t0;
				
				for(dstx = 0; dstx < conn->t_count; dstx++)
				{
					double red = 0.0, green = 0.0, blue = 0.0, alpha = 0.0;
					
					for(srcx = srcxs[dstx]; srcx <= srcxs[dstx + 1]; srcx++)
					{
						double x1 = max(srcx * pixel_length, dists[dstx]);
						double x2 = min((srcx + 1) * pixel_length, dists[dstx + 1]);
						double rel = (x2 - x1) / ct->dist;
						
						int msrcx = srcx % curve->texture_surface->width;
						
						double src_alpha = (double)((uint8_t*)curve->texture_surface->alpha_buf)
							[y * curve->texture_surface->width + msrcx] / 255.0;
		
						red += rel * get_double_from_8(((uint8_t*)curve->texture_surface->buf)
							[(y * curve->texture_surface->width + msrcx) * 3]) * src_alpha;
						green += rel * get_double_from_8(((uint8_t*)curve->texture_surface->buf)
							[(y * curve->texture_surface->width + msrcx) * 3 + 1]) * src_alpha;
						blue += rel * get_double_from_8(((uint8_t*)curve->texture_surface->buf)
							[(y * curve->texture_surface->width + msrcx)* 3 + 2]) * src_alpha;
						alpha += rel * src_alpha;
					}
					
					((uint8_t*)squished_texture->buf)
						[(dstx * curve->texture_surface->height + y) * 3] = 
						convert_double_to_8bit(red / alpha);
					
					((uint8_t*)squished_texture->buf)
						[(dstx * curve->texture_surface->height + y) * 3 + 1] = 
						convert_double_to_8bit(green / alpha);
					
					((uint8_t*)squished_texture->buf)
						[(dstx * curve->texture_surface->height + y) * 3 + 2] = 
						convert_double_to_8bit(blue / alpha);
					
					((uint8_t*)squished_texture->alpha_buf)
						[dstx * curve->texture_surface->height + y] = 
						convert_double_to_8bit(alpha);
					
					ct = ct->next;
				}
				
				if(in_lock_check_stop_callback())
				{
					free(srcxs);
					free(dists);
					free_surface(squished_texture);
					return 0;
				}
			}
		
			conn->squished_texture = squished_texture;
			
			free(srcxs);
			free(dists);
		}
		
		break;
	}

	return 1;
}
	

int generate_straight_squished_texture_with_alpha(struct conn_t *conn, struct curve_t *curve)
{
	switch(curve->fill_type)
	{
	case CURVE_SOLID:
		{
			struct surface_t *squished_texture = new_surface(SURFACE_24BITALPHA8BIT, 
				100, conn->t_count);
			
			int i;
			
			for(i = 0; i < conn->t_count * 100; i++)
			{
				((uint8_t*)squished_texture->buf)[i * 3] = curve->red;
				((uint8_t*)squished_texture->buf)[i * 3 + 1] = curve->green;
				((uint8_t*)squished_texture->buf)[i * 3 + 2] = curve->blue;
				((uint8_t*)squished_texture->alpha_buf)[i] = curve->alpha;
			}
			
			conn->squished_texture = squished_texture;
		}
		
		break;
	
	case CURVE_TEXTURE:
		{
			double curve_length = 0.0, dist = 0.0;
		
			struct conn_pointer_t *cconnp = curve->connp0;
		
			while(cconnp)
			{
				if(cconnp->conn == conn)
					dist = curve_length;
		
				curve_length += cconnp->conn->t_length;
				cconnp = cconnp->next;
			}
		
			double pixel_length;
			
			if(curve->fixed_reps)
				pixel_length = (curve_length / curve->reps) / curve->texture_surface->width;
			else
				pixel_length = curve->texture_length / curve->texture_surface->width;
		
			struct surface_t *squished_texture = new_surface(SURFACE_24BITALPHA8BIT, 
				curve->texture_surface->height, conn->t_count);
		
			int y, dstx, srcx;
			
			int *srcxs = malloc(sizeof(int) * (conn->t_count + 1));
			double *dists = malloc(sizeof(double) * (conn->t_count + 1));
		
			
			for(dstx = 0; dstx < conn->t_count; dstx++)
			{
				srcxs[dstx] = (int)(dist / pixel_length);
				dists[dstx] = dist;
		
				dist += conn->t_length / conn->t_count;
			}
			
			srcxs[conn->t_count] = (int)(dist / pixel_length);
			dists[conn->t_count] = dist;
		
			for(y = 0; y < curve->texture_surface->height; y++)
			{
				for(dstx = 0; dstx < conn->t_count; dstx++)
				{
					double red = 0.0, green = 0.0, blue = 0.0, alpha = 0.0;
					
					for(srcx = srcxs[dstx]; srcx <= srcxs[dstx + 1]; srcx++)
					{
						double x1 = max(srcx * pixel_length, dists[dstx]);
						double x2 = min((srcx + 1) * pixel_length, dists[dstx + 1]);
						double rel = (x2 - x1) / (conn->t_length / conn->t_count);
						
						int msrcx = srcx % curve->texture_surface->width;
		
						double src_alpha = 
							get_double_from_8(((uint8_t*)curve->texture_surface->alpha_buf)
							[y * curve->texture_surface->width + msrcx]);
						
						red += rel * get_double_from_8(((uint8_t*)curve->texture_surface->buf)
							[(y * curve->texture_surface->width + msrcx) * 3]) * src_alpha;
						green += rel * get_double_from_8(((uint8_t*)curve->texture_surface->buf)
							[(y * curve->texture_surface->width + msrcx) * 3 + 1]) * src_alpha;
						blue += rel * get_double_from_8(((uint8_t*)curve->texture_surface->buf)
							[(y * curve->texture_surface->width + msrcx)* 3 + 2]) * src_alpha;
						alpha += rel * src_alpha;
					}
					
					((uint8_t*)squished_texture->buf)
						[(dstx * curve->texture_surface->height + y) * 3] = 
						convert_double_to_8bit(red / alpha);
					
					((uint8_t*)squished_texture->buf)
						[(dstx * curve->texture_surface->height + y) * 3 + 1] = 
						convert_double_to_8bit(green / alpha);
					
					((uint8_t*)squished_texture->buf)
						[(dstx * curve->texture_surface->height + y) * 3 + 2] = 
						convert_double_to_8bit(blue / alpha);
					
					((uint8_t*)squished_texture->alpha_buf)
						[dstx * curve->texture_surface->height + y] = 
						convert_double_to_8bit(alpha);
				}
		
				if(in_lock_check_stop_callback())
				{
					free(srcxs);
					free(dists);
					free_surface(squished_texture);
					return 0;
				}
			}
		
			conn->squished_texture = squished_texture;
			
			
			free(srcxs);
			free(dists);
		}
		
		break;
	}

	return 1;
}


void generate_squished_textures_for_all_conns()
{
	if(!worker_try_enter_main_lock())
		return;

	struct conn_t *cconn = conn0;

	while(cconn)
	{
		struct curve_t *curve = get_curve(cconn);
		assert(curve);
		

		switch(curve->fill_type)
		{
		case CURVE_SOLID:
			if(!cconn->squished_texture)
			{
				switch(cconn->type)
				{
				case CONN_TYPE_STRAIGHT:
					if(!generate_straight_squished_texture_with_alpha(cconn, curve))
						return;
					break;

				case CONN_TYPE_CONIC:
				case CONN_TYPE_BEZIER:
					if(!generate_curved_squished_texture_with_alpha(cconn, curve))
						return;
					break;
				}
			}
			break;
			
		case CURVE_TEXTURE:
			if(curve->texture_surface && !cconn->squished_texture)
			{
				switch(cconn->type)
				{
				case CONN_TYPE_STRAIGHT:
					if(!generate_straight_squished_texture_with_alpha(cconn, curve))
						return;
					break;

				case CONN_TYPE_CONIC:
				case CONN_TYPE_BEZIER:
					if(!generate_curved_squished_texture_with_alpha(cconn, curve))
						return;
					break;
				}
			}
			break;
		}

		cconn = cconn->next;
	}
	
	worker_leave_main_lock();
}
