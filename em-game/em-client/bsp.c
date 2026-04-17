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

#include "shared/bsp.h"
#include "line.h"
#include "gsub.h"


void draw_bsp_node(struct bspnode_t *node)
{
	draw_world_clipped_line(node->x1 + node->dtstart * (node->x2 - node->x1), 
		node->y1 + node->dtstart * (node->y2 - node->y1), 
		node->x1 + node->dtend * (node->x2 - node->x1), 
		node->y1 + node->dtend * (node->y2 - node->y1));

	if(node->front)
		draw_bsp_node(node->front);

	if(node->back)
		draw_bsp_node(node->back);
}


void draw_bsp_tree()
{
	if(bspnode0)
		draw_bsp_node(bspnode0);
}
