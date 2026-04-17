/**
 * @file
 * @copyright 1998-2004 Jonathan Brown <jbrown@bluedroplet.com>
 * @license https://www.gnu.org/licenses/gpl-3.0.html
 * @homepage https://github.com/bluedroplet/emergence
 */

#ifdef LINUX
#define _GNU_SOURCE
#define _REENTRANT
#endif

#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <math.h>

#include <zlib.h>

#include "inout.h"
#include "bsp.h"


struct bspnode_t *bspnode0 = NULL;


void read_bsp_node(gzFile file, struct bspnode_t *node)
{
	if(gzread(file, &node->x1, 4) != 4)
		goto error;

	if(gzread(file, &node->y1, 4) != 4)
		goto error;

	if(gzread(file, &node->x2, 4) != 4)
		goto error;

	if(gzread(file, &node->y2, 4) != 4)
		goto error;

	if(gzread(file, &node->tstart, 4) != 4)
		goto error;

	if(gzread(file, &node->tend, 4) != 4)
		goto error;

	if(gzread(file, &node->dtstart, 4) != 4)
		goto error;

	if(gzread(file, &node->dtend, 4) != 4)
		goto error;

	uint8_t g;
		
	if(gzread(file, &g, 1) != 1)
		goto error;
	
	if(g & 1)
	{
		node->front = malloc(sizeof(struct bspnode_t));
		read_bsp_node(file, node->front);
	}
	else
	{
		node->front = NULL;
	}
	
	if(g & 2)
	{
		node->back = malloc(sizeof(struct bspnode_t));
		read_bsp_node(file, node->back);
	}
	else
	{
		node->back = NULL;
	}
	
	return;
	
error:
	
	node->front = NULL;
	node->back = NULL;
}


void delete_bsp_node(struct bspnode_t *node)
{
	if(node->front)
		delete_bsp_node(node->front);

	if(node->back)
		delete_bsp_node(node->back);
	
	free(node);
}


int load_bsp_tree(gzFile file)
{
	if(bspnode0)
		delete_bsp_node(bspnode0);
	
	bspnode0 = malloc(sizeof(struct bspnode_t));
	read_bsp_node(file, bspnode0);
	return 1;
}


struct bspnode_t *circle_walk_bsp_node(struct bspnode_t *node, float xdis, float ydis, float r)
{
	struct bspnode_t *cnode;
		
	double x1 = node->x1 - xdis;
	double y1 = node->y1 - ydis;
	double x2 = node->x2 - xdis;
	double y2 = node->y2 - ydis;
	
	
	// circle-line intersection
	
	double dx = x2 - x1;
	double dy = y2 - y1;
	double dr2 = dx * dx + dy * dy;
	double D = x1 * y2 - x2 * y1;
	double discr = r * r * dr2 - D * D;

	if(discr >= 0.0)
	{
		if(fabs(dx) > fabs(dy))
		{
			double thing = dx * sqrt(discr);
	
			double x = (D * dy + thing) / dr2;
			double t = (x + xdis - node->x1) / (node->x2 - node->x1);
			
			if(t > node->tstart && t < node->tend)
				return node;
			
			x = (D * dy - thing) / dr2;
			t = (x + xdis - node->x1) / (node->x2 - node->x1);
			
			if(t > node->tstart && t < node->tend)
				return node;
		}
		else
		{
			double thing = dy * sqrt(discr);
	
			double y = -(D * dx + thing) / dr2;
			double t = (y + ydis - node->y1) / (node->y2 - node->y1);
			
			if(t > node->tstart && t < node->tend)
				return node;
			
			y = -(D * dx - thing) / dr2;
			t = (y + ydis - node->y1) / (node->y2 - node->y1);
			
			if(t > node->tstart && t < node->tend)
				return node;
		}
			
		if(node->front)
		{
			cnode = circle_walk_bsp_node(node->front, xdis, ydis, r);
			if(cnode)
				return cnode;
		}
	
		if(node->back)
		{
			cnode = circle_walk_bsp_node(node->back, xdis, ydis, r);
			if(cnode)
				return cnode;
		}
		
		return NULL;
	}

	if(!inout(node->x1, node->y1, node->x2, node->y2, xdis, ydis))
	{
		if(node->front)
		{
			cnode = circle_walk_bsp_node(node->front, xdis, ydis, r);
			if(cnode)
				return cnode;
		}
	}
	else
	{
		if(node->back)
		{
			cnode = circle_walk_bsp_node(node->back, xdis, ydis, r);
			if(cnode)
				return cnode;
		}
	}
	
	return NULL;
}


struct bspnode_t *circle_walk_bsp_tree(float xdis, float ydis, float r)
{
	if(bspnode0)
		return circle_walk_bsp_node(bspnode0, xdis, ydis, r);
	else
		return NULL;
}


struct bspnode_t *line_walk_bsp_node(struct bspnode_t *node, float x1, float y1, 
	float x2, float y2)
{
	struct bspnode_t *cnode;
	
	double nx = y1 - y2;
	double ny = -(x1 - x2);
	
	double denom = (-nx) * (node->x2 - node->x1) + 
		(-ny) * (node->y2 - node->y1);
	
	if(denom != 0.0)
	{
		double numer = nx * (node->x1 - x1) + 
			ny * (node->y1 - y1);
		
		double t1 = numer / denom;	// interection on node
		
		nx = node->y1 - node->y2;
		ny = -(node->x1 - node->x2);
		
		denom = (-nx) * (x2 - x1) + 
			(-ny) * (y2 - y1);
		
		if(denom != 0.0)
		{
			numer = nx * (x1 - node->x1) + 
				ny * (y1 - node->y1);
		
			double t2 = numer / denom;	// intersection on line
		
			if(t1 >= node->tstart && t1 <= node->tend)
			{
				if(t2 >= 0.0 && t2 <= 1.0)
				{
					return node;
				}
				else
				{
					if(numer > 0.0)		// line in front of node
					{
						if(node->front)
						{
							cnode = line_walk_bsp_node(node->front, x1, y1, x2, y2);
							if(cnode)
								return cnode;
						}
					}
					else
					{
						if(node->back)
						{
							cnode = line_walk_bsp_node(node->back, x1, y1, x2, y2);
							if(cnode)
								return cnode;
						}
					}
				}
			}
			else
			{
				if(t2 >= 0.0 && t2 <= 1.0)
				{
					if(node->front)
					{
						cnode = line_walk_bsp_node(node->front, x1, y1, x2, y2);
						if(cnode)
							return cnode;
					}
					
					if(node->back)
					{
						cnode = line_walk_bsp_node(node->back, x1, y1, x2, y2);
						if(cnode)
							return cnode;
					}
				}
				else
				{
					if(numer > 0.0)		// line in front of node
					{
						if(node->front)
						{
							cnode = line_walk_bsp_node(node->front, x1, y1, x2, y2);
							if(cnode)
								return cnode;
						}
					}
					else
					{
						if(node->back)
						{
							cnode = line_walk_bsp_node(node->back, x1, y1, x2, y2);
							if(cnode)
								return cnode;
						}
					}
				}
			}
		}
	}
	
	return NULL;
}


struct bspnode_t *line_walk_bsp_tree(float x1, float y1, float x2, float y2)
{
	if(bspnode0)
		return line_walk_bsp_node(bspnode0, x1, y1, x2, y2);
	else
		return NULL;
}

float nearest_wall_dist;
float nearest_wall_x;
float nearest_wall_y;
int nearest_wall_first;

/*
void rail_walk_bsp_node(struct bspnode_t *node, float x1, float y1, float x2, float y2)
{
	double nx = y1 - y2;
	double ny = -(x1 - x2);
	
	double denom = (-nx) * (node->x2 - node->x1) + 
		(-ny) * (node->y2 - node->y1);
	
	if(denom != 0.0)
	{
		double numer = nx * (node->x1 - x1) + 
			ny * (node->y1 - y1);
		
		double t1 = numer / denom;	// interection on node
		
		nx = node->y1 - node->y2;
		ny = -(node->x1 - node->x2);
		
		denom = (-nx) * (x2 - x1) + 
			(-ny) * (y2 - y1);
		
		if(denom != 0.0)
		{
			numer = nx * (x1 - node->x1) + 
				ny * (y1 - node->y1);
		
			double t2 = numer / denom;	// intersection on line
		
			if(t1 > node->tstart && t1 < node->tend)
			{
				if(t2 > 0.0 && t2 < 1.0)
				{
					double x = node->x1 + t1 * (node->x2 - node->x1);
					double y = node->y1 + t1 * (node->y2 - node->y1);
					
					double dist = hypot(x - x1, y - y1);
					
					if(nearest_wall_first || dist < nearest_wall_dist)
					{
						nearest_wall_first = 0;
						nearest_wall_dist = dist;
						nearest_wall_x = x;
						nearest_wall_y = y;
					}
				}
				else
				{
					if(numer > 0.0)		// line in front of node
					{
						if(node->front)
						{
							rail_walk_bsp_node(node->front, x1, y1, x2, y2);
						}
					}
					else
					{
						if(node->back)
						{
							rail_walk_bsp_node(node->back, x1, y1, x2, y2);
						}
					}
				}
			}
			else
			{
				if(t2 >= 0.0 && t2 <= 1.0)
				{
					if(node->front)
					{
						rail_walk_bsp_node(node->front, x1, y1, x2, y2);
					}
					
					if(node->back)
					{
						rail_walk_bsp_node(node->back, x1, y1, x2, y2);
					}
				}
				else
				{
					if(numer > 0.0)		// line in front of node
					{
						if(node->front)
						{
							rail_walk_bsp_node(node->front, x1, y1, x2, y2);
						}
					}
					else
					{
						if(node->back)
						{
							rail_walk_bsp_node(node->back, x1, y1, x2, y2);
						}
					}
				}
			}
		}
	}
}
*/


void rail_walk_bsp_node(struct bspnode_t *node, float x1, float y1, float x2, float y2)
{
	if(node->front)
		rail_walk_bsp_node(node->front, x1, y1, x2, y2);

	if(node->back)
		rail_walk_bsp_node(node->back, x1, y1, x2, y2);
	
	double nx = y1 - y2;
	double ny = -(x1 - x2);
	
	double denom = (-nx) * (node->x2 - node->x1) + 
		(-ny) * (node->y2 - node->y1);
	
	if(denom != 0.0)
	{
		double numer = nx * (node->x1 - x1) + 
			ny * (node->y1 - y1);
		
		double t = numer / denom;
		
		if(t > node->tstart && t < node->tend)
		{
			nx = node->y1 - node->y2;
			ny = -(node->x1 - node->x2);
			
			denom = (-nx) * (x2 - x1) + 
				(-ny) * (y2 - y1);
			
			if(denom != 0.0)
			{
				numer = nx * (x1 - node->x1) + 
					ny * (y1 - node->y1);
			
				double nt = numer / denom;
				
				if(nt > 0.0 && nt < 1.0)
				{
					double x = node->x1 + t * (node->x2 - node->x1);
					double y = node->y1 + t * (node->y2 - node->y1);
					
					double dist = hypot(x - x1, y - y1);
					
					if(nearest_wall_first || dist < nearest_wall_dist)
					{
						nearest_wall_first = 0;
						nearest_wall_dist = dist;
						nearest_wall_x = x;
						nearest_wall_y = y;
					}
				}
			}
		}
	}
}


void rail_walk_bsp(float x1, float y1, float x2, float y2, float *out_x, float *out_y)
{
	nearest_wall_first = 1;

	if(bspnode0)
		rail_walk_bsp_node(bspnode0, x1, y1, x2, y2);
	
	if(nearest_wall_first)
		return;
	
	*out_x = nearest_wall_x;
	*out_y = nearest_wall_y;
}
