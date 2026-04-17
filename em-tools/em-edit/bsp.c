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

#include "llist.h"
#include "minmax.h"
#include "inout.h"
#include "gsub.h"
#include "nodes.h"
#include "map.h"
#include "conns.h"
#include "curves.h"
#include "lines.h"
#include "bezier.h"
#include "points.h"
#include "fills.h"
#include "main_lock.h"
#include "interface.h"
#include "worker.h"

#define BSP_LINE_TYPE_EMPTY	0
#define BSP_LINE_TYPE_CURVE	1
#define BSP_LINE_TYPE_NODE	2
#define BSP_LINE_TYPE_FILL	3

#define BSP_LINE_CONN_LEFT	0
#define BSP_LINE_CONN_RIGHT	1


struct bsp_line_pointer_t
{
	struct bsp_line_t *bsp_line;
	struct bsp_line_pointer_t *next;
};


typedef struct bsp_line_t
{
	float x1, y1, x2, y2;
	uint32_t start_index, end_index;
	int colinear_index;
	struct bsp_line_pointer_t *colinear_friends;
	
	int left_type, right_type;
	
	union
	{
		struct curve_t *curve;
		struct node_t *node;
		struct fill_t *fill;
		
	} left;
	
	union
	{
		struct curve_t *curve;
		struct node_t *node;
		struct fill_t *fill;
		
	} right;
	
	struct conn_t *conn;
	uint32_t t_index;
	int conn_side;
	
	struct line_t *line;
		
	struct bsp_line_t *next;
	
} bsp_line_t;

bsp_line_t *bsp_line0 = NULL, *ui_bsp_line0 = NULL, **generating_bsp_line0 = NULL;


typedef struct bsp_node_t
{
	bsp_line_t *line;
	float tstart, tend;
	float dtstart, dtend;
	struct bsp_node_t *start_cutter, *end_cutter;
	
	int extended_north_by_child;
	int extended_south_by_child;
	int extended_north_by_parent;
	int extended_south_by_parent;
	int extended_north_by_cutter;
	int extended_south_by_cutter;
	
	struct bsp_node_t *left, *right;
		
} bsp_node_t;


typedef struct bsp_tline_t
{
	bsp_line_t *line;
	float tstart, tend;
	uint32_t start_index, end_index;
	struct bsp_node_t *start_cutter, *end_cutter;
	
	struct bsp_tline_t *next;
	
} bsp_tline_t;


int generate_bsp = 0, generate_ui_bsp = 0;
bsp_node_t *ui_bsp_tree = NULL, *bsp_tree = NULL, *generating_bsp_tree = NULL;

int bsp_draw_depth = -1;

int next_bsp_point_index;
int next_bsp_colinear_index;


void add_bsp_line_pointer(struct bsp_line_pointer_t **bsp_linep0, struct bsp_line_t *bsp_line)
{
	struct bsp_line_pointer_t *bsp_linep;
	LL_ALLOC(struct bsp_line_pointer_t, bsp_linep0, &bsp_linep);
	bsp_linep->bsp_line = bsp_line;
}


struct curve_t *get_curve_walk_bsp_node(bsp_node_t *node, float x, float y)
{
	if(!inout(node->line->x1, node->line->y1, node->line->x2, node->line->y2, x, y))
	{
		if(node->left)
		{
			return get_curve_walk_bsp_node(node->left, x, y);
		}
		else
		{
			if(node->line->left_type == BSP_LINE_TYPE_CURVE)
				return node->line->left.curve;
			else
				return NULL;
		}
	}
	else
	{
		if(node->right)
		{
			return get_curve_walk_bsp_node(node->right, x, y);
		}
		else
		{
			if(node->line->right_type == BSP_LINE_TYPE_CURVE)
				return node->line->right.curve;
			else
				return NULL;
		}
	}
	
	return NULL;
}


struct curve_t *get_curve_bsp(float x, float y)
{
	if(ui_bsp_tree)
		return get_curve_walk_bsp_node(ui_bsp_tree, x, y);
	else
		return NULL;
}


struct node_t *get_node_walk_bsp_node(bsp_node_t *node, float x, float y)
{
	if(!inout(node->line->x1, node->line->y1, node->line->x2, node->line->y2, x, y))
	{
		if(node->left)
		{
			return get_node_walk_bsp_node(node->left, x, y);
		}
		else
		{
			if(node->line->left_type == BSP_LINE_TYPE_NODE)
				return node->line->left.node;
			else
				return NULL;
		}
	}
	else
	{
		if(node->right)
		{
			return get_node_walk_bsp_node(node->right, x, y);
		}
		else
		{
			if(node->line->right_type == BSP_LINE_TYPE_NODE)
				return node->line->right.node;
			else
				return NULL;
		}
	}
	
	return NULL;
}


struct node_t *get_node_bsp(float x, float y)
{
	if(ui_bsp_tree)
		return get_node_walk_bsp_node(ui_bsp_tree, x, y);
	else
		return NULL;
}


struct fill_t *get_fill_walk_bsp_node(bsp_node_t *node, float x, float y)
{
	if(!inout(node->line->x1, node->line->y1, node->line->x2, node->line->y2, x, y))
	{
		if(node->left)
		{
			return get_fill_walk_bsp_node(node->left, x, y);
		}
		else
		{
			if(node->line->left_type == BSP_LINE_TYPE_FILL)
				return node->line->left.fill;
			else
				return NULL;
		}
	}
	else
	{
		if(node->right)
		{
			return get_fill_walk_bsp_node(node->right, x, y);
		}
		else
		{
			if(node->line->right_type == BSP_LINE_TYPE_FILL)
				return node->line->right.fill;
			else
				return NULL;
		}
	}
	
	return NULL;
}


struct fill_t *get_fill_bsp(float x, float y)
{
	if(ui_bsp_tree)
		return get_fill_walk_bsp_node(ui_bsp_tree, x, y);
	else
		return NULL;
}


void draw_bsp_node(bsp_node_t *node, int depth)
{
	if(!depth)
		return;
	
	draw_world_clipped_line(node->line->x1 + node->dtstart * (node->line->x2 - node->line->x1), 
		node->line->y1 + node->dtstart * (node->line->y2 - node->line->y1), 
		node->line->x1 + node->dtend * (node->line->x2 - node->line->x1), 
		node->line->y1 + node->dtend * (node->line->y2 - node->line->y1));

	if(node->left)
		draw_bsp_node(node->left, depth - 1);

	if(node->right)
		draw_bsp_node(node->right, depth - 1);
}


void draw_bsp_tree()
{
	if(bsp_tree)
		draw_bsp_node(bsp_tree, bsp_draw_depth);
}


void gzwrite_bsp_node(gzFile file, struct bsp_node_t *node)
{
	gzwrite(file, &node->line->x1, 4);
	gzwrite(file, &node->line->y1, 4);
	gzwrite(file, &node->line->x2, 4);
	gzwrite(file, &node->line->y2, 4);
	gzwrite(file, &node->tstart, 4);
	gzwrite(file, &node->tend, 4);
	gzwrite(file, &node->dtstart, 4);
	gzwrite(file, &node->dtend, 4);
	
	uint8_t b = 0;
	
	if(node->left)
		b = 1;
	
	if(node->right)
		b |= 2;
	
	gzwrite(file, &b, 1);
	
	if(node->left)
		gzwrite_bsp_node(file, node->left);

	if(node->right)
		gzwrite_bsp_node(file, node->right);
}


void gzwrite_bsp_tree(gzFile file)
{
	if(!bsp_tree)
		return;
	
	gzwrite_bsp_node(file, bsp_tree);
}


void delete_bsp_node(bsp_node_t *node)
{
	if(node->left)
		delete_bsp_node(node->left);

	if(node->right)
		delete_bsp_node(node->right);
	
	free(node);
}


void more_bsp()
{
	bsp_draw_depth++;
}


void less_bsp()
{
	if(bsp_draw_depth > -1)		// really ugly
		bsp_draw_depth--;
}



void delete_all_bsp_lines()
{
	while(*generating_bsp_line0)
	{
		LL_REMOVE_ALL(struct bsp_line_pointer_t, &(*generating_bsp_line0)->colinear_friends);
		LL_REMOVE(struct bsp_line_t, generating_bsp_line0, *generating_bsp_line0);
	}
}


void clear_bsp_trees()	// always called when not working
{
	if(bsp_tree)
	{
		delete_bsp_node(bsp_tree);
		bsp_tree = NULL;
	}
	
	if(ui_bsp_tree)
	{
		delete_bsp_node(ui_bsp_tree);
		ui_bsp_tree = NULL;
	}
	
	generating_bsp_line0 = &bsp_line0;
	delete_all_bsp_lines();
	
	generating_bsp_line0 = &ui_bsp_line0;
	delete_all_bsp_lines();
}


void invalidate_bsp_tree()	// always called when not working
{
	clear_bsp_trees();
	generate_bsp = 1;
	generate_ui_bsp = 1;
}


void finished_generating_bsp_tree()
{
	bsp_tree = generating_bsp_tree;
	generating_bsp_tree = NULL;
	generate_bsp = 0;
}


void finished_generating_ui_bsp_tree()
{
	ui_bsp_tree = generating_bsp_tree;
	generating_bsp_tree = NULL;
	generate_ui_bsp = 0;
}


void init_bsp_tree()
{
	;
}


void kill_bsp_tree()
{
	invalidate_bsp_tree();
}


//
// WORKER THREAD FUNCTIONS
//


void generate_node_bsp_indexes(struct node_t *node)
{
	switch(node->num_conns)
	{
	case 1:
	case 2:
		if(node->sat_conn_type[0] == SAT_CONN_TYPE_UNCONN && 
			node->sat_conn_type[1] == SAT_CONN_TYPE_UNCONN)
		{
			node->bsp_index[0] = node->bsp_index[1] = next_bsp_point_index++;
			node->bsp_index[2] = node->bsp_index[3] = next_bsp_point_index++;
		}
		else
		{
			node->bsp_index[0] = node->bsp_index[2] = next_bsp_point_index++;
			node->bsp_index[1] = node->bsp_index[3] = next_bsp_point_index++;
		}
		break;
	
	case 3:
	case 4:
		node->bsp_index[0] = next_bsp_point_index++;
		node->bsp_index[1] = next_bsp_point_index++;
		node->bsp_index[2] = next_bsp_point_index++;
		node->bsp_index[3] = next_bsp_point_index++;
		break;
	}
}	


void generate_curved_conn_lines(struct curve_t *curve, struct conn_t *conn, 
	uint32_t *start_bsp_points, uint32_t *end_bsp_points)
{
	int left1, right1, left2, right2;
	get_width_sats(conn, &left1, &right1, &left2, &right2);

	double lx1 = conn->bigt0->x1 - conn->bigt0->deltay1 * conn->node1->width[left1];
	double ly1 = conn->bigt0->y1 + conn->bigt0->deltax1 * conn->node1->width[left1];
	double rx1 = conn->bigt0->x1 + conn->bigt0->deltay1 * conn->node1->width[right1];
	double ry1 = conn->bigt0->y1 - conn->bigt0->deltax1 * conn->node1->width[right1];
	
	bsp_line_t bsp_line;
	struct t_t *ct = conn->bigt0;
	
	int t_index = 0;
	uint32_t old_left_index = start_bsp_points[0];
	uint32_t old_right_index = start_bsp_points[1];
	
	uint32_t left_index = 0, right_index = 0;
	
	while(ct)
	{
		if(ct->next)
		{
			left_index = next_bsp_point_index++;
			right_index = next_bsp_point_index++;
		}
		else
		{
			left_index = end_bsp_points[0];
			right_index = end_bsp_points[1];
		}
		
		double leftwidth = (conn->node2->width[left2] - conn->node1->width[left1]) * 
			ct->t2 + conn->node1->width[left1];
		double rightwidth = (conn->node2->width[right2] - conn->node1->width[right1]) * 
			ct->t2 + conn->node1->width[right1];

		double lx2 = ct->x2 - ct->deltay2 * leftwidth;
		double ly2 = ct->y2 + ct->deltax2 * leftwidth;
		double rx2 = ct->x2 + ct->deltay2 * rightwidth;
		double ry2 = ct->y2 - ct->deltax2 * rightwidth;
		
		bsp_line.x1 = lx1;
		bsp_line.y1 = ly1;
		bsp_line.x2 = lx2;
		bsp_line.y2 = ly2;
		bsp_line.start_index = old_left_index;
		bsp_line.end_index = left_index;
		bsp_line.colinear_index = 0;
		bsp_line.colinear_friends = NULL;
		
		bsp_line.left_type = BSP_LINE_TYPE_EMPTY;
		bsp_line.right_type = BSP_LINE_TYPE_CURVE;
		bsp_line.right.curve = curve;
		bsp_line.conn = conn;
		bsp_line.t_index = t_index;
		bsp_line.conn_side = BSP_LINE_CONN_LEFT;
		bsp_line.line = NULL;
		
		LL_ADD(bsp_line_t, generating_bsp_line0, &bsp_line);
		
		bsp_line.x1 = rx1;
		bsp_line.y1 = ry1;
		bsp_line.x2 = rx2;
		bsp_line.y2 = ry2;
		bsp_line.start_index = old_right_index;
		bsp_line.end_index = right_index;
		bsp_line.colinear_index = 0;
		bsp_line.colinear_friends = NULL;

		bsp_line.left_type = BSP_LINE_TYPE_CURVE;
		bsp_line.left.curve = curve;
		bsp_line.right_type = BSP_LINE_TYPE_EMPTY;
		bsp_line.conn = conn;
		bsp_line.t_index = t_index;
		bsp_line.conn_side = BSP_LINE_CONN_RIGHT;
		bsp_line.line = NULL;
		
		LL_ADD(bsp_line_t, generating_bsp_line0, &bsp_line);

		lx1 = lx2;
		ly1 = ly2;
		rx1 = rx2;
		ry1 = ry2;
		
		t_index++;
		
		old_left_index = left_index;
		old_right_index = right_index;
		
		ct = ct->next;
	}
}


void generate_straight_conn_lines(struct curve_t *curve, struct conn_t *conn, 
	uint32_t *start_bsp_points, uint32_t *end_bsp_points, int colinear)
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
	
	bsp_line_t bsp_line;
	
	bsp_line.x1 = lx1;
	bsp_line.y1 = ly1;
	bsp_line.x2 = lx2;
	bsp_line.y2 = ly2;
	bsp_line.start_index = start_bsp_points[0];
	bsp_line.end_index = end_bsp_points[0];
	
	if(colinear)
	{
		bsp_line.colinear_index = (*generating_bsp_line0)->next->colinear_index;
	}
	else
	{
		bsp_line.colinear_index = next_bsp_colinear_index++;
	}
	
	bsp_line.colinear_friends = NULL;
	bsp_line.left_type = BSP_LINE_TYPE_EMPTY;
	bsp_line.right_type = BSP_LINE_TYPE_CURVE;
	bsp_line.right.curve = curve;
	bsp_line.line = NULL;
	
	LL_ADD(bsp_line_t, generating_bsp_line0, &bsp_line);
	
	bsp_line.x1 = rx1;
	bsp_line.y1 = ry1;
	bsp_line.x2 = rx2;
	bsp_line.y2 = ry2;
	bsp_line.start_index = start_bsp_points[1];
	bsp_line.end_index = end_bsp_points[1];
	
	if(colinear)
	{
		bsp_line.colinear_index = (*generating_bsp_line0)->next->colinear_index;
	}
	else
	{
		bsp_line.colinear_index = next_bsp_colinear_index++;
	}
	
	bsp_line.colinear_friends = NULL;
	bsp_line.left_type = BSP_LINE_TYPE_CURVE;
	bsp_line.left.curve = curve;
	bsp_line.right_type = BSP_LINE_TYPE_EMPTY;
	bsp_line.line = NULL;
	
	LL_ADD(bsp_line_t, generating_bsp_line0, &bsp_line);
}


void generate_conn_start_line(struct curve_t *curve, struct conn_t *conn, 
	uint32_t *start_bsp_points)
{
	struct node_t *node;
	int sat;
	int left, right;
	
	if(!conn->orientation)
	{
		node = conn->node1;
		sat = conn->sat1;

		get_width_sats(conn, &left, &right, NULL, NULL);
	}
	else
	{
		node = conn->node2;
		sat = conn->sat2;

		get_width_sats(conn, NULL, NULL, &left, &right);
	}
	
	double deltax, deltay;
	
	deltax = node->sats[sat].x;
	deltay = node->sats[sat].y;

	double length = hypot(deltax, deltay);

	deltax /= length;
	deltay /= length;
	
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
	
	bsp_line_t bsp_line;
	
	bsp_line.x1 = x - deltay * node->width[left];
	bsp_line.y1 = y + deltax * node->width[left];
	bsp_line.x2 = x + deltay * node->width[right];
	bsp_line.y2 = y - deltax * node->width[right];
	bsp_line.start_index = start_bsp_points[0];
	bsp_line.end_index = start_bsp_points[1];
	bsp_line.colinear_index = 0;
	bsp_line.colinear_friends = NULL;
	bsp_line.left_type = BSP_LINE_TYPE_CURVE;
	bsp_line.left.curve = curve;
	bsp_line.right_type = BSP_LINE_TYPE_EMPTY;
	bsp_line.line = NULL;
	
	LL_ADD(bsp_line_t, generating_bsp_line0, &bsp_line);
}


void generate_conn_end_line(struct curve_t *curve, struct conn_t *conn, 
	uint32_t *start_bsp_points)
{
	struct node_t *node;
	int sat;
	int left, right;
	
	if(!conn->orientation)
	{
		node = conn->node2;
		sat = conn->sat2;

		get_width_sats(conn, NULL, NULL, &left, &right);
	}
	else
	{
		node = conn->node1;
		sat = conn->sat1;

		get_width_sats(conn, &left, &right, NULL, NULL);
	}

	double deltax, deltay;
	
	deltax = node->sats[sat].x;
	deltay = node->sats[sat].y;

	double length = hypot(deltax, deltay);

	deltax /= length;
	deltay /= length;
	
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
	
	bsp_line_t bsp_line;
	
	bsp_line.x1 = x - deltay * node->width[left];
	bsp_line.y1 = y + deltax * node->width[left];
	bsp_line.x2 = x + deltay * node->width[right];
	bsp_line.y2 = y - deltax * node->width[right];
	bsp_line.start_index = start_bsp_points[1];
	bsp_line.colinear_index = 0;
	bsp_line.colinear_friends = NULL;
	bsp_line.right_type = BSP_LINE_TYPE_EMPTY;
	bsp_line.left_type = BSP_LINE_TYPE_CURVE;
	bsp_line.left.curve = curve;
	bsp_line.end_index = start_bsp_points[0];
	bsp_line.line = NULL;
	
	LL_ADD(bsp_line_t, generating_bsp_line0, &bsp_line);
}


void generate_curve_lines(struct curve_t *curve)
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
	
	uint32_t start_bsp_points[2];
	uint32_t end_bsp_points[2];

	cconnp = curve->connp0;
	int previous_straight = 0;
	
	while(cconnp)
	{
		struct node_t *node1, *node2;
		int sat1, sat2;
		
		if(!cconnp->conn->orientation)
		{
			node1 = cconnp->conn->node1;
			node2 = cconnp->conn->node2;
			sat1 = cconnp->conn->sat1;
			sat2 = cconnp->conn->sat2;
		}
		else
		{
			node1 = cconnp->conn->node2;
			node2 = cconnp->conn->node1;
			sat1 = cconnp->conn->sat2;
			sat2 = cconnp->conn->sat1;
		}
		
		switch(sat1)
		{
		case 0:
			start_bsp_points[0] = node1->bsp_index[0];
			start_bsp_points[1] = node1->bsp_index[1];
			break;

		case 1:
			start_bsp_points[0] = node1->bsp_index[3];
			start_bsp_points[1] = node1->bsp_index[2];
			break;

		case 2:
			start_bsp_points[0] = node1->bsp_index[1];
			start_bsp_points[1] = node1->bsp_index[3];
			break;

		case 3:
			start_bsp_points[0] = node1->bsp_index[2];
			start_bsp_points[1] = node1->bsp_index[0];
			break;
		}
		
		switch(sat2)
		{
		case 0:
			end_bsp_points[0] = node2->bsp_index[1];
			end_bsp_points[1] = node2->bsp_index[0];
			break;

		case 1:
			end_bsp_points[0] = node2->bsp_index[2];
			end_bsp_points[1] = node2->bsp_index[3];
			break;

		case 2:
			end_bsp_points[0] = node2->bsp_index[3];
			end_bsp_points[1] = node2->bsp_index[1];
			break;

		case 3:
			end_bsp_points[0] = node2->bsp_index[0];
			end_bsp_points[1] = node2->bsp_index[2];
			break;
		}
		
		if(!loop)
		{
			if(cconnp == curve->connp0)
			{
				if(node1->num_conns == 3)
				{
				/*	switch(sat1)
					{
					case 0:
						start_bsp_points[0] = node1->bsp_index[2];
						start_bsp_points[1] = node1->bsp_index[3];
						break;
			
					case 1:
						start_bsp_points[0] = node1->bsp_index[1];
						start_bsp_points[1] = node1->bsp_index[0];
						break;
			
					case 2:
						start_bsp_points[0] = node1->bsp_index[0];
						start_bsp_points[1] = node1->bsp_index[2];
						break;
			
					case 3:
						start_bsp_points[0] = node1->bsp_index[3];
						start_bsp_points[1] = node1->bsp_index[1];
						break;
					}
					*/
				}
				else
				{
					generate_conn_start_line(curve, curve->connp0->conn, start_bsp_points);
				}
			}
		}
		
		switch(cconnp->conn->type)
		{
		case CONN_TYPE_STRAIGHT:
			generate_straight_conn_lines(curve, cconnp->conn, start_bsp_points, end_bsp_points, 
				previous_straight);
			previous_straight = 1;
			break;
		
		case CONN_TYPE_CONIC:
		case CONN_TYPE_BEZIER:
			generate_curved_conn_lines(curve, cconnp->conn, start_bsp_points, end_bsp_points);
			previous_straight = 0;
			break;
		}
		
		if(!loop)
		{
			if(!cconnp->next)
			{
				if(node2->num_conns == 3)
				{
				/*	switch(sat2)
					{
					case 0:
						end_bsp_points[0] = node2->bsp_index[3];
						end_bsp_points[1] = node2->bsp_index[2];
						break;
			
					case 1:
						end_bsp_points[0] = node2->bsp_index[0];
						end_bsp_points[1] = node2->bsp_index[1];
						break;
			
					case 2:
						end_bsp_points[0] = node2->bsp_index[2];
						end_bsp_points[1] = node2->bsp_index[0];
						break;
			
					case 3:
						end_bsp_points[0] = node2->bsp_index[1];
						end_bsp_points[1] = node2->bsp_index[3];
						break;
					}
				*/
				}
				else
				{
					generate_conn_end_line(curve, cconnp->conn, end_bsp_points);
				}
			}
		}
		
		cconnp = cconnp->next;
	}
}
	

/*

	switch(sat)
	{
	case 0:
	case 2:
	
		if(node->texture_width1 != 0.0)
		{
			bsp_line.right_type = BSP_LINE_TYPE_NODE;
			bsp_line.right.node = node;
		}
		else
		{
			bsp_line.right_type = BSP_LINE_TYPE_EMPTY;
		}
	
		break;
			
	case 1:
	case 3:

		if(node->texture_width2 != 0.0)
		{
			bsp_line.right_type = BSP_LINE_TYPE_NODE;
			bsp_line.right.node = node;
		}
		else
		{
			bsp_line.right_type = BSP_LINE_TYPE_EMPTY;
		}
		
		break;
	}
*/


void generate_conn_fill_left_edge_segment_lines(struct fill_t *fill, struct conn_t *conn)
{
	struct bsp_line_t *cline = bsp_line0;
	
	while(cline)
	{
		if(cline->conn == conn && cline->conn_side == BSP_LINE_CONN_LEFT)
		{
			cline->left_type = BSP_LINE_TYPE_FILL;
			cline->left.fill = fill;
		}
		
		cline = cline->next;
	}
}


void generate_conn_fill_right_edge_segment_lines(struct fill_t *fill, struct conn_t *conn)
{
	struct bsp_line_t *cline = bsp_line0;
	
	while(cline)
	{
		if(cline->conn == conn && cline->conn_side == BSP_LINE_CONN_RIGHT)
		{
			cline->right_type = BSP_LINE_TYPE_FILL;
			cline->right.fill = fill;
		}
		
		cline = cline->next;
	}
}


void generate_t1_conn_fill_left_edge_segment_lines(struct fill_t *fill, struct conn_t *conn, 
	uint32_t t1_index)
{
	struct bsp_line_t *cline = bsp_line0;
	
	while(cline)
	{
		if(cline->conn == conn && cline->t_index >= t1_index 
			&& cline->conn_side == BSP_LINE_CONN_LEFT)
		{
			cline->left_type = BSP_LINE_TYPE_FILL;
			cline->left.fill = fill;
		}
		
		cline = cline->next;
	}
}


void generate_t1_conn_fill_right_edge_segment_lines(struct fill_t *fill, struct conn_t *conn, 
	uint32_t t1_index)
{
	struct bsp_line_t *cline = bsp_line0;
	
	while(cline)
	{
		if(cline->conn == conn && cline->t_index >= t1_index 
			&& cline->conn_side == BSP_LINE_CONN_RIGHT)
		{
			cline->right_type = BSP_LINE_TYPE_FILL;
			cline->right.fill = fill;
		}
		
		cline = cline->next;
	}
}


void generate_t2_conn_fill_left_edge_segment_lines(struct fill_t *fill, struct conn_t *conn, 
	uint32_t t2_index)
{
	struct bsp_line_t *cline = bsp_line0;
	
	while(cline)
	{
		if(cline->conn == conn && cline->t_index < t2_index 
			&& cline->conn_side == BSP_LINE_CONN_LEFT)
		{
			cline->left_type = BSP_LINE_TYPE_FILL;
			cline->left.fill = fill;
		}
		
		cline = cline->next;
	}
}


void generate_t2_conn_fill_right_edge_segment_lines(struct fill_t *fill, struct conn_t *conn, 
	uint32_t t2_index)
{
	struct bsp_line_t *cline = bsp_line0;
	
	while(cline)
	{
		if(cline->conn == conn && cline->t_index < t2_index 
			&& cline->conn_side == BSP_LINE_CONN_RIGHT)
		{
			cline->right_type = BSP_LINE_TYPE_FILL;
			cline->right.fill = fill;
		}
		
		cline = cline->next;
	}
}


void generate_t1_t2_conn_fill_left_edge_segment_lines(struct fill_t *fill, struct conn_t *conn, 
	uint32_t t1_index, uint32_t t2_index)
{
	struct bsp_line_t *cline = bsp_line0;
	
	while(cline)
	{
		if(cline->conn == conn && cline->t_index >= t1_index && cline->t_index < t2_index 
			&& cline->conn_side == BSP_LINE_CONN_LEFT)
		{
			cline->left_type = BSP_LINE_TYPE_FILL;
			cline->left.fill = fill;
		}
		
		cline = cline->next;
	}
}


void generate_t1_t2_conn_fill_right_edge_segment_lines(struct fill_t *fill, struct conn_t *conn, 
	uint32_t t1_index, uint32_t t2_index)
{
	struct bsp_line_t *cline = bsp_line0;
	
	while(cline)
	{
		if(cline->conn == conn && cline->t_index >= t1_index && cline->t_index < t2_index 
			&& cline->conn_side == BSP_LINE_CONN_RIGHT)
		{
			cline->right_type = BSP_LINE_TYPE_FILL;
			cline->right.fill = fill;
		}
		
		cline = cline->next;
	}
}


void generate_fill_lines(struct fill_t *fill)
{
	struct fill_edge_t *fill_edge0 = fill->edge0;
		
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
				
			uint32_t first_t_index, last_t_index;
			
			while(cconnp)
			{
				if(cconnp->conn == fill_edge->point1->conn)
				{
					first_connp = cconnp;
					last_conn = fill_edge->point2->conn;
					first_t_index = fill_edge->point1->t_index;
					last_t_index = fill_edge->point2->t_index;
					break;
				}
					
				if(cconnp->conn == fill_edge->point2->conn)
				{
					first_connp = cconnp;
					last_conn = fill_edge->point1->conn;
					first_t_index = fill_edge->point2->t_index;
					last_t_index = fill_edge->point1->t_index;
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
					generate_t1_t2_conn_fill_left_edge_segment_lines(fill, last_conn, 
						fill_edge->point1->t_index, fill_edge->point2->t_index);
				}
				else
				{
					generate_t1_conn_fill_left_edge_segment_lines(fill, first_connp->conn, 
						first_t_index);
					
					cconnp = first_connp->next;
					
					while(cconnp->conn != last_conn)
					{
						generate_conn_fill_left_edge_segment_lines(fill, cconnp->conn);
						cconnp = cconnp->next;
					}

					generate_t2_conn_fill_left_edge_segment_lines(fill, last_conn, last_t_index);
				}
			}
			else
			{
				if(first_connp->conn == last_conn)
				{
					generate_t1_t2_conn_fill_right_edge_segment_lines(fill, last_conn, 
						fill_edge->point1->t_index, fill_edge->point2->t_index);
				}
				else
				{
					generate_t1_conn_fill_right_edge_segment_lines(fill, first_connp->conn, 
						first_t_index);
					
					cconnp = first_connp->next;
					
					while(cconnp->conn != last_conn)
					{
						generate_conn_fill_right_edge_segment_lines(fill, cconnp->conn);
						cconnp = cconnp->next;
					}

					generate_t2_conn_fill_right_edge_segment_lines(fill, last_conn, last_t_index);
				}
			}
		}
		else		// not following curve
		{
			bsp_line_t bsp_line;
			
			if(last_fill_edge->point2->pos < last_fill_edge->point1->pos)
			{
				bsp_line.x1 = fill_edge->point1->x - fill_edge->point1->deltay * 
					fill_edge->point1->left_width;
				bsp_line.y1 = fill_edge->point1->y + fill_edge->point1->deltax * 
					fill_edge->point1->left_width;

				struct bsp_line_t *cline = bsp_line0;
		
				while(cline)
				{
					if(cline->conn == fill_edge->point1->conn && cline->t_index == 
						fill_edge->point1->t_index && cline->conn_side == BSP_LINE_CONN_LEFT)
					{
						bsp_line.start_index = cline->start_index;
						break;
					}
					
					cline = cline->next;
				}
				
				assert(cline);
			}
			else
			{
				bsp_line.x1 = fill_edge->point1->x + fill_edge->point1->deltay * 
					fill_edge->point1->right_width;
				bsp_line.y1 = fill_edge->point1->y - fill_edge->point1->deltax * 
					fill_edge->point1->right_width;

				struct bsp_line_t *cline = bsp_line0;
		
				while(cline)
				{
					if(cline->conn == fill_edge->point1->conn && cline->t_index == 
						fill_edge->point1->t_index && cline->conn_side == BSP_LINE_CONN_RIGHT)
					{
						bsp_line.start_index = cline->start_index;
						break;
					}
					
					cline = cline->next;
				}
				
				assert(cline);
			}
			
			if(next_fill_edge->point2->pos < next_fill_edge->point1->pos)
			{
				bsp_line.x2 = fill_edge->point2->x - fill_edge->point2->deltay * 
					fill_edge->point2->left_width;
				bsp_line.y2 = fill_edge->point2->y + fill_edge->point2->deltax * 
					fill_edge->point2->left_width;

				struct bsp_line_t *cline = bsp_line0;
		
				while(cline)
				{
					if(cline->conn == fill_edge->point2->conn && cline->t_index == 
						fill_edge->point2->t_index && cline->conn_side == BSP_LINE_CONN_LEFT)
					{
						bsp_line.end_index = cline->start_index;
						break;
					}
					
					cline = cline->next;
				}
				
				assert(cline);
			}
			else
			{
				bsp_line.x2 = fill_edge->point2->x + fill_edge->point2->deltay * 
					fill_edge->point2->right_width;
				bsp_line.y2 = fill_edge->point2->y - fill_edge->point2->deltax * 
					fill_edge->point2->right_width;

				struct bsp_line_t *cline = bsp_line0;
		
				while(cline)
				{
					if(cline->conn == fill_edge->point2->conn && cline->t_index == 
						fill_edge->point2->t_index && cline->conn_side == BSP_LINE_CONN_RIGHT)
					{
						bsp_line.end_index = cline->start_index;
						break;
					}
					
					cline = cline->next;
				}
				
				assert(cline);
			}
			
			bsp_line.colinear_index = 0;
			bsp_line.colinear_friends = NULL;
			bsp_line.right_type = BSP_LINE_TYPE_FILL;
			bsp_line.right.fill = fill;
			bsp_line.left_type = BSP_LINE_TYPE_EMPTY;
			bsp_line.conn = NULL;
			bsp_line.t_index = 0;
			bsp_line.conn_side = 0;
			bsp_line.line = NULL;
			
			LL_ADD(bsp_line_t, generating_bsp_line0, &bsp_line);
		}
		
		last_fill_edge = fill_edge;
		fill_edge = fill_edge->next;
	}
}


int generate_lines()
{
	next_bsp_point_index = 1;
	next_bsp_colinear_index = 1;

	struct node_t *node = node0;
		
	while(node)
	{
		generate_node_bsp_indexes(node);
		
		if(in_lock_check_stop_callback())
			return 0;
		
		node = node->next;
	}
	
	
	struct curve_t *curve = curve0;
		
	while(curve)
	{
		generate_curve_lines(curve);
		
		if(in_lock_check_stop_callback())
			return 0;
		
		curve = curve->next;
	}
	
	
	struct fill_t *fill = fill0;
		
	while(fill)
	{
		generate_fill_lines(fill);

		if(in_lock_check_stop_callback())
			return 0;
		
		fill = fill->next;
	}
	
	return 1;
}


int create_colinear_friends()
{
	struct bsp_line_t *line1, *line2;
	
	for(line1 = *generating_bsp_line0; line1; line1 = line1->next)
	{
		for(line2 = *generating_bsp_line0; line2; line2 = line2->next)
		{
			if(line1 == line2)
				continue;
			
			if(line1->colinear_index != 0 && 
				line1->colinear_index == line2->colinear_index)
			{
				add_bsp_line_pointer(&line1->colinear_friends, line2);
			}
		}
	}
	
	return 1;
}


void create_space(bsp_tline_t **space)
{
	bsp_tline_t bsp_tline = {NULL, 0.0, 1.0, 0, 0, NULL, NULL};
	
	bsp_line_t *cbsp_line = *generating_bsp_line0;
	
	while(cbsp_line)
	{
		bsp_tline.line = cbsp_line;
		bsp_tline.start_index = cbsp_line->start_index;
		bsp_tline.end_index = cbsp_line->end_index;
		LL_ADD(bsp_tline_t, space, &bsp_tline);
		cbsp_line = cbsp_line->next;
	}
}


bsp_tline_t *find_most_central_line(bsp_tline_t *space)
{
	double x = 0.0, y = 0.0;
	int numlines = 0;
	bsp_tline_t *cline = space;

	while(cline)
	{
		x += cline->line->x1 + cline->line->x2;
		y += cline->line->y1 + cline->line->y2;
		
		numlines++;
		
		cline = cline->next;
	}

	x /= numlines * 2;
	y /= numlines * 2;

	double nx = (space->line->x1 + space->line->x2) / 2;
	double ny = (space->line->y1 + space->line->y2) / 2;

	double ndist = (nx - x) * (nx - x) + (ny - y) * (ny - y);

	bsp_tline_t *nline = space;

	cline = space->next;

	while(cline)
	{
		double cx = (cline->line->x1 + cline->line->x2) / 2;
		double cy = (cline->line->y1 + cline->line->y2) / 2;
		
		double cdist = (cx - x) * (cx - x) + (cy - y) * (cy - y);
		
		if(cdist < ndist)
		{
			ndist = cdist;
			nline = cline;
		}
		
		cline = cline->next;
	}

	return nline;
}


bsp_tline_t *find_least_cutting_line(bsp_tline_t *space)
{
	bsp_tline_t *cutter, *cuttee;
	bsp_tline_t *least_cutting = NULL;
	int least_cuts = 0;

	for(cutter = space; cutter; cutter = cutter->next)
	{
		int cuts = 0;
		
		for(cuttee = space; cuttee; cuttee = cuttee->next)
		{
			if(cutter == cuttee)
				continue;
			
			double nx = cutter->line->y1 - cutter->line->y2;
			double ny = -(cutter->line->x1 - cutter->line->x2);
			
			double numer = nx * (cuttee->line->x1 - cutter->line->x1) + 
				ny * (cuttee->line->y1 - cutter->line->y1);
			
			double denom = (-nx) * (cuttee->line->x2 - cuttee->line->x1) + 
				(-ny) * (cuttee->line->y2 - cuttee->line->y1);
			
			if(denom == 0.0)	// lines are parallel
			{
				continue;
			}
			else
			{
				double t = numer / denom;
					
				if((cuttee->start_index == cutter->start_index) || 
					(cuttee->start_index == cutter->end_index))
				{
					;
				}
				else if((cuttee->end_index == cutter->start_index) || 
					(cuttee->end_index == cutter->end_index))
				{
					;
				}
				else if((t > cuttee->tend) || (t < cuttee->tstart))
				{
					;
				}
				else	// the line segment must be split
				{
					cuts++;
				}
			}
		}
		
		if(!least_cutting || cuts < least_cuts)
		{
			least_cutting = cutter;
			least_cuts = cuts;
		}
	}

	return least_cutting;
}


bsp_tline_t *find_most_even_cutting_line(bsp_tline_t *space)
{
	bsp_tline_t *cutter, *cuttee;
	bsp_tline_t *most_even = NULL;
	int least_diff = 0;

	for(cutter = space; cutter; cutter = cutter->next)
	{
		int left = 0, right = 0;
		
		for(cuttee = space; cuttee; cuttee = cuttee->next)
		{
			if(cutter == cuttee)
				continue;
			
			double nx = cutter->line->y1 - cutter->line->y2;
			double ny = -(cutter->line->x1 - cutter->line->x2);
			
			double numer = nx * (cuttee->line->x1 - cutter->line->x1) + 
				ny * (cuttee->line->y1 - cutter->line->y1);
			
			double denom = (-nx) * (cuttee->line->x2 - cuttee->line->x1) + 
				(-ny) * (cuttee->line->y2 - cuttee->line->y1);
			
			if(denom == 0.0)	// lines are parallel
			{
				if(numer < 0.0)	// current line in front of dividing line
				{
					right++;
				}
				else					// current line behind dividing line
				{
					left++;
				}
			}
			else
			{
				double t = numer / denom;
					
				double ct = (cuttee->tstart + cuttee->tend) / 2.0;
				double new_numer = nx * ((cuttee->line->x1 + ct * 
					(cuttee->line->x2 - cuttee->line->x1)) - cutter->line->x1) + 
					ny * ((cuttee->line->y1 + ct * 
					(cuttee->line->y2 - cuttee->line->y1)) - cutter->line->y1);
					
				if((cuttee->start_index == cutter->start_index) || 
					(cuttee->start_index == cutter->end_index))
				{
					if(new_numer < 0.0)	// current line in front of dividing line
					{
						right++;
					}
					else
					{
						left++;
					}
				}
				else if((cuttee->end_index == cutter->start_index) || 
					(cuttee->end_index == cutter->end_index))
				{
					if(new_numer < 0.0)	// current line in front of dividing line
					{
						right++;
					}
					else
					{
						left++;
					}
				}
				else if((t > cuttee->tend) || (t < cuttee->tstart))
				{
					if(new_numer < 0.0)	// current line in front of dividing line
					{
						right++;
					}
					else
					{
						left++;
					}
				}
				else	// the line segment must be split
				{
					right++, left++;
				}
			}
		}
		
		int diff = abs(right - left);
		
		if(!most_even || diff < least_diff)
		{
			most_even = cutter;
			least_diff = diff;
		}
	}

	return most_even;
}



bsp_tline_t *(*bsp_heuristic)(bsp_tline_t *space);


int split_space(bsp_tline_t *space, bsp_node_t **node)
{
	bsp_tline_t *cutter = bsp_heuristic(space);
	
	*node = malloc(sizeof(bsp_node_t));

	(*node)->line = cutter->line;
	(*node)->tstart = cutter->tstart;
	(*node)->tend = cutter->tend;
	(*node)->dtstart = cutter->tstart;
	(*node)->dtend = cutter->tend;
	(*node)->start_cutter = cutter->start_cutter;
	(*node)->end_cutter = cutter->end_cutter;
	
	(*node)->extended_north_by_child = 0;
	(*node)->extended_south_by_child = 0;
	(*node)->extended_north_by_parent = 0;
	(*node)->extended_south_by_parent = 0;
	(*node)->extended_north_by_cutter = 0;
	(*node)->extended_south_by_cutter = 0;

	(*node)->left = NULL;
	(*node)->right = NULL;

	bsp_tline_t *left = NULL, *right = NULL, *cline;

	// go through each line and put it in either front or back

	for(cline = space; cline; cline = cline->next)
	{
		if(cline == cutter)		// skip the cutting line
			continue;
		
		if(cutter->line->colinear_index != 0 && 
			cutter->line->colinear_index == cline->line->colinear_index)
			continue;			// skip colinear line
		
		double nx = cutter->line->y1 - cutter->line->y2;
		double ny = -(cutter->line->x1 - cutter->line->x2);
		
		double numer = nx * (cline->line->x1 - cutter->line->x1) + 
			ny * (cline->line->y1 - cutter->line->y1);
		
		double denom = (-nx) * (cline->line->x2 - cline->line->x1) + 
			(-ny) * (cline->line->y2 - cline->line->y1);
		
		if(denom == 0.0)	// lines are parallel
		{
			if(numer < 0.0)	// current line in front of dividing line
			{
				LL_ADD(bsp_tline_t, &right, cline);
			}
			else if(numer > 0.0)	// current line behind dividing line
			{
				LL_ADD(bsp_tline_t, &left, cline);
			}
			
			continue;
		}
		
		double t = numer / denom;
		
		double ct = (cline->tstart + cline->tend) / 2.0;
		
		double new_numer = nx * ((cline->line->x1 + ct * (cline->line->x2 - cline->line->x1)) - 
			cutter->line->x1) + 
			ny * ((cline->line->y1 + ct * (cline->line->y2 - cline->line->y1)) - 
			cutter->line->y1);
		
		struct bsp_line_pointer_t *colinear_line;
			
		if((cline->start_index != 0) && ((cline->start_index == cutter->start_index) || 
			(cline->start_index == cutter->end_index)))
		{
			cline->start_cutter = *node;
			
			if(new_numer < 0.0)	// current line in front of dividing line
			{
				LL_ADD(bsp_tline_t, &right, cline);
			}
			else
			{
				LL_ADD(bsp_tline_t, &left, cline);
			}
			
			continue;
		}
		
		if((cline->end_index != 0) && ((cline->end_index == cutter->start_index) || 
			(cline->end_index == cutter->end_index)))
		{
			cline->end_cutter = *node;
			
			if(new_numer < 0.0)	// current line in front of dividing line
			{
				LL_ADD(bsp_tline_t, &right, cline);
			}
			else
			{
				LL_ADD(bsp_tline_t, &left, cline);
			}
			
			continue;
		}
		
		if(cutter->line->colinear_index)
		{
			int stop = 0;
			
			for(colinear_line = cutter->line->colinear_friends; colinear_line; 
				colinear_line = colinear_line->next)
			{
				if(colinear_line->bsp_line->start_index != 0)
				{
					if((colinear_line->bsp_line->start_index == cline->start_index) || 
						(colinear_line->bsp_line->start_index == cline->end_index))
					{
						if(new_numer < 0.0)	// current line in front of dividing line
						{
							LL_ADD(bsp_tline_t, &right, cline);
						}
						else
						{
							LL_ADD(bsp_tline_t, &left, cline);
						}
						
						stop = 1;
						break;
					}
				}
				
				if(colinear_line->bsp_line->end_index != 0)
				{
					if((colinear_line->bsp_line->end_index == cline->start_index) || 
						(colinear_line->bsp_line->end_index == cline->end_index))
					{
						if(new_numer < 0.0)	// current line in front of dividing line
						{
							LL_ADD(bsp_tline_t, &right, cline);
						}
						else
						{
							LL_ADD(bsp_tline_t, &left, cline);
						}
						
						stop = 1;
						break;
					}
				}
			}
			
			if(stop)
				continue;
		}
		
		if(cline->line->colinear_index)
		{
			int stop = 0;
			
			for(colinear_line = cline->line->colinear_friends; colinear_line; 
				colinear_line = colinear_line->next)
			{
				if(colinear_line->bsp_line->start_index != 0)
				{
					if((colinear_line->bsp_line->start_index == cutter->start_index) || 
						(colinear_line->bsp_line->start_index == cutter->end_index))
					{
					
						if(new_numer < 0.0)	// current line in front of dividing line
						{
							LL_ADD(bsp_tline_t, &right, cline);
						}
						else
						{
							LL_ADD(bsp_tline_t, &left, cline);
						}
						
						stop = 1;
						break;
					}
				}
				
				if(colinear_line->bsp_line->end_index != 0)
				{
					if((colinear_line->bsp_line->end_index == cutter->start_index) || 
						(colinear_line->bsp_line->end_index == cutter->end_index))
					{
						if(new_numer < 0.0)	// current line in front of dividing line
						{
							LL_ADD(bsp_tline_t, &right, cline);
						}
						else
						{
							LL_ADD(bsp_tline_t, &left, cline);
						}
						
						stop = 1;
						break;
					}
				}
			}
			
			if(stop)
				continue;
		}
		
		if((t > cline->tend) || (t < cline->tstart))
		{
			if(new_numer < 0.0)
			{
				LL_ADD(bsp_tline_t, &right, cline);
			}
			else
			{
				LL_ADD(bsp_tline_t, &left, cline);
			}
			
			continue;
		}
		
		// the line segment must be split
		struct bsp_tline_t nline = {cline->line, t, cline->tend, 
				0, cline->end_index, *node, cline->end_cutter};
		
		cline->tend = t;
		cline->end_index = 0;
		cline->end_cutter = *node;
		
		if(numer < 0.0)
		{
			LL_ADD(bsp_tline_t, &right, cline);
			LL_ADD(bsp_tline_t, &left, &nline);
		}
		else
		{
			LL_ADD(bsp_tline_t, &right, &nline);
			LL_ADD(bsp_tline_t, &left, cline);
		}
	}

	LL_REMOVE_ALL(bsp_tline_t, &space);

	if(check_stop_callback())
	{
		LL_REMOVE_ALL(bsp_tline_t, &left);
		LL_REMOVE_ALL(bsp_tline_t, &right);
		return 0;
	}
	
	if(left)
	{
		if(!split_space(left, &(*node)->left))
		{
			LL_REMOVE_ALL(bsp_tline_t, &right);
			return 0;
		}
	}

	if(right)
	{
		if(!split_space(right, &(*node)->right))
			return 0;
	}
	
	return 1;
}


void extend_descendants_by_ancestor(bsp_node_t *ancestor, bsp_node_t *descendant)
{
	if(descendant->left)
		extend_descendants_by_ancestor(ancestor, descendant->left);
	
	if(descendant->right)
		extend_descendants_by_ancestor(ancestor, descendant->right);
	
	
	// extend descendant by ancestor

	double nx = ancestor->line->y1 - ancestor->line->y2;
	double ny = -(ancestor->line->x1 - ancestor->line->x2);
	
	double denom = (-nx) * (descendant->line->x2 - descendant->line->x1) + 
		(-ny) * (descendant->line->y2 - descendant->line->y1);
	
	if(denom != 0.0)
	{
		double numer = nx * (descendant->line->x1 - ancestor->line->x1) + 
			ny * (descendant->line->y1 - ancestor->line->y1);
		
		double t = numer / denom;
		
		if(!descendant->extended_south_by_cutter)
		{
			if(descendant->start_cutter == ancestor)
			{
				descendant->extended_south_by_parent = 1;
				descendant->extended_south_by_cutter = 1;
				descendant->dtstart = descendant->tstart;
			}
			else if(!descendant->extended_south_by_parent)
			{
				if(t < descendant->tstart)
				{
					descendant->dtstart = t;
					descendant->extended_south_by_parent = 1;
				}
			}
			else
			{
				if((t > descendant->dtstart) && (t < descendant->tstart))
					descendant->dtstart = t;
			}
		}
		
		if(!descendant->extended_north_by_cutter)
		{
			if(descendant->end_cutter == ancestor)
			{
				descendant->extended_north_by_parent = 1;
				descendant->extended_north_by_cutter = 1;
				descendant->dtend = descendant->tend;
			}
			else if(!descendant->extended_north_by_parent)
			{
				if(t > descendant->tend)
				{
					descendant->dtend = t;
					descendant->extended_north_by_parent = 1;
				}
			}
			else
			{
				if((t < descendant->dtend) && (t > descendant->tend))
					descendant->dtend = t;
			}
		}
	}
}


int extend_descendants(bsp_node_t *node)
{
	if(node->left)
	{
		if(!extend_descendants(node->left))
			return 0;

		extend_descendants_by_ancestor(node, node->left);
	}
	
	if(node->right)
	{
		if(!extend_descendants(node->right))
			return 0;

		extend_descendants_by_ancestor(node, node->right);
	}
	
	if(check_stop_callback())
		return 0;
	
	return 1;
}


void extend_ancestor_by_descendants(bsp_node_t *ancestor, bsp_node_t *descendant)
{
	if(descendant->left)
		extend_ancestor_by_descendants(ancestor, descendant->left);
	
	if(descendant->right)
		extend_ancestor_by_descendants(ancestor, descendant->right);
	
	
	// extend ancestor by descendant
	
	double nx = descendant->line->y1 - descendant->line->y2;
	double ny = -(descendant->line->x1 - descendant->line->x2);
	
	double denom = (-nx) * (ancestor->line->x2 - ancestor->line->x1) + 
		(-ny) * (ancestor->line->y2 - ancestor->line->y1);
	
	if(denom != 0.0)
	{
		double numer = nx * (ancestor->line->x1 - descendant->line->x1) + 
			ny * (ancestor->line->y1 - descendant->line->y1);
		
		double t = numer / denom;
		
		if(!ancestor->extended_south_by_parent)
		{
			if(!ancestor->extended_south_by_child)
			{
				if(t < ancestor->tstart)
				{
					ancestor->dtstart = t;
					ancestor->extended_south_by_child = 1;
				}
			}
			else
			{
				if((t < ancestor->dtstart) && (t < ancestor->tstart))
					ancestor->dtstart = t;
			}
		}
		
		if(!ancestor->extended_north_by_parent)
		{
			if(!ancestor->extended_north_by_child)
			{
				if(t > ancestor->tend)
				{
					ancestor->dtend = t;
					ancestor->extended_north_by_child = 1;
				}
			}
			else
			{
				if((t > ancestor->dtend) && (t > ancestor->tend))
					ancestor->dtend = t;
			}
		}
	}
}


int extend_ancestors(bsp_node_t *node)
{
	if(node->left)
	{
		if(!extend_ancestors(node->left))
			return 0;

		extend_ancestor_by_descendants(node, node->left);
	}
	
	if(node->right)
	{
		if(!extend_ancestors(node->right))
			return 0;

		extend_ancestor_by_descendants(node, node->right);
	}
	
	if(check_stop_callback())
		return 0;
	
	return 1;
}


void generate_bsp_tree()
{
	if(!worker_try_enter_main_lock())
		return;
	
	generating_bsp_line0 = &bsp_line0;
	
	if(!generate_lines())
	{
		delete_all_bsp_lines();
		return;
	}
	
	if(!bsp_line0)
	{
		worker_leave_main_lock();
		return;
	}
	
	if(!create_colinear_friends())
	{
		delete_all_bsp_lines();
		return;
	}
	
	bsp_tline_t *space = NULL;
	
	create_space(&space);
	
	worker_leave_main_lock();
	
	if(check_stop_callback())
	{
		delete_all_bsp_lines();
		LL_REMOVE_ALL(bsp_tline_t, &space);
		return;
	}
	
	bsp_heuristic = find_most_even_cutting_line;
	
	if(!split_space(space, &generating_bsp_tree))
	{
		delete_all_bsp_lines();
		delete_bsp_node(generating_bsp_tree);
		generating_bsp_tree = NULL;
		return;
	}

	if(!extend_descendants(generating_bsp_tree))
	{
		delete_all_bsp_lines();
		delete_bsp_node(generating_bsp_tree);
		generating_bsp_tree = NULL;
		return;
	}
	
	if(!extend_ancestors(generating_bsp_tree))
	{
		delete_all_bsp_lines();
		delete_bsp_node(generating_bsp_tree);
		generating_bsp_tree = NULL;
		return;
	}
}


void generate_ui_bsp_tree()
{
	if(!worker_try_enter_main_lock())
		return;
	
	generating_bsp_line0 = &ui_bsp_line0;
	
	if(!generate_lines())
	{
		delete_all_bsp_lines();
		return;
	}
	
	if(!ui_bsp_line0)
	{
		worker_leave_main_lock();
		return;
	}
	
	if(!create_colinear_friends())
	{
		delete_all_bsp_lines();
		return;
	}
	
	bsp_tline_t *space = NULL;
	
	create_space(&space);
	
	worker_leave_main_lock();
	
	if(check_stop_callback())
	{
		delete_all_bsp_lines();
		LL_REMOVE_ALL(bsp_tline_t, &space);
		return;
	}
	
	bsp_heuristic = find_most_central_line;
	
	if(!split_space(space, &generating_bsp_tree))
	{
		delete_all_bsp_lines();
		delete_bsp_node(generating_bsp_tree);
		generating_bsp_tree = NULL;
		return;
	}

/*	if(!extend_descendants(generating_bsp_tree))
	{
		delete_all_bsp_lines();
		delete_bsp_node(generating_bsp_tree);
		generating_bsp_tree = NULL;
		return;
	}
	
	if(!extend_ancestors(generating_bsp_tree))
	{
		delete_all_bsp_lines();
		delete_bsp_node(generating_bsp_tree);
		generating_bsp_tree = NULL;
		return;
	}
	*/
}
