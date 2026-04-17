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
#include <math.h>
#include <memory.h>

#include "llist.h"
#include "inout.h"
#include "nodes.h"
#include "conns.h"
#include "conic.h"
#include "worker.h"
#include "main_lock.h"



struct t_t **conic_ct0;
int conic_count;
double conic_length;
int conic_clock;

int cfuck = 0;

struct t_t **conic_big_ct0;
int conic_big_count;
double conic_big_length;
int conic_big_clock;


int conic_t_divide(float cx, float cy, float hyp, 
	float theta1, float theta2, float t1, float t2)
{
	if(++cfuck == 100)
	{
		if(check_stop_callback())
			return 0;
		
		cfuck = 0;
	}
	
	double x1, y1, x2, y2;
	double dx1, dy1, dx2, dy2;
	
	double theta = theta1 + (theta2 - theta1) * t1;
	
	x1 = -sin(theta) * hyp + cx;
	y1 = cos(theta) * hyp + cy;
	
	if(conic_clock)
	{
		dx1 = -cos(theta);
		dy1 = -sin(theta);
	}
	else
	{
		dx1 = cos(theta);
		dy1 = sin(theta);
	}
		
	theta = theta1 + (theta2 - theta1) * t2;
	
	x2 = -sin(theta) * hyp + cx;
	y2 = cos(theta) * hyp + cy;
	if(conic_clock)
	{
		dx2 = -cos(theta);
		dy2 = -sin(theta);
	}
	else
	{
		dx2 = cos(theta);
		dy2 = sin(theta);
	}
	
	double delta_dist = hypot(dx1, dy1);
	dx1 /= delta_dist;
	dy1 /= delta_dist;

	delta_dist = hypot(dx2, dy2);
	dx2 /= delta_dist;
	dy2 /= delta_dist;
	
	if(((x2 - x1) * (x2 - x1) + (y2 - y1) * (y2 - y1)) > 0.25)
	{
		float midt = (t1 + t2) / 2;
		
		if(!conic_t_divide(cx, cy, hyp, theta1, theta2, t1, midt))
			return 0;
		
		if(!conic_t_divide(cx, cy, hyp, theta1, theta2, midt, t2))
			return 0;
	}
	else
	{
		struct t_t t;
		t.t1 = t1;
		t.t2 = t2;
		t.x1 = x1;
		t.y1 = y1;
		t.x2 = x2;
		t.y2 = y2;
		t.deltax1 = dx1;
		t.deltay1 = dy1;
		t.deltax2 = dx2;
		t.deltay2 = dy2;
		t.dist = hypot(x2 - x1, y2 - y1);
		conic_length += t.dist;
		conic_count++;
		
		LL_ADD(struct t_t, conic_ct0, &t);
		conic_ct0 = &(*conic_ct0)->next;
	}
	
	return 1;
}


int generate_conic_ts(struct conn_t *conn)
{
	struct node_t *node1, *node2;
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
	
	double ax1 = node1->x;
	double ay1 = node1->y;
	double ax2 = node1->x - node1->sats[sat1].y;
	double ay2 = node1->y + node1->sats[sat1].x;
	
	double bx1 = node2->x;
	double by1 = node2->y;
	double bx2 = node2->x - node2->sats[sat2].y;
	double by2 = node2->y + node2->sats[sat2].x;
	
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
		if(theta1 < theta2)		// go clockwise
			theta1 += M_PI * 2;
		conic_clock = 0;
	}
	else
	{
		if(theta2 < theta1)		// go anti-clockwise
			theta2 += M_PI * 2;
		conic_clock = 1;
	}
	
	leave_main_lock();

	struct t_t *t0 = NULL;
	conic_ct0 = &t0;
	conic_count = 0;
	conic_length = 0.0;
	
	if(!conic_t_divide(cx, cy, hyp, theta1, theta2, 0, 1))
	{
		LL_REMOVE_ALL(struct t_t, &t0);
		return 0;
	}
	
	if(!worker_try_enter_main_lock())
	{
		LL_REMOVE_ALL(struct t_t, &t0);
		return 0;
	}
	
	conn->t0 = t0;
	conn->t_count = conic_count;
	conn->t_length = conic_length;
	
	return 1;
}


void conic_bigt_divide(float cx, float cy, float hyp, 
	float theta1, float theta2, float t1, float t2)
{
	double x1, y1, x2, y2;
	double dx1, dy1, dx2, dy2;
	
	double theta = theta1 + (theta2 - theta1) * t1;
	
	x1 = -sin(theta) * hyp + cx;
	y1 = cos(theta) * hyp + cy;
	
	if(conic_big_clock)
	{
		dx1 = -cos(theta);
		dy1 = -sin(theta);
	}
	else
	{
		dx1 = cos(theta);
		dy1 = sin(theta);
	}
		
	
	theta = theta1 + (theta2 - theta1) * t2;
	
	x2 = -sin(theta) * hyp + cx;
	y2 = cos(theta) * hyp + cy;
	if(conic_big_clock)
	{
		dx2 = -cos(theta);
		dy2 = -sin(theta);
	}
	else
	{
		dx2 = cos(theta);
		dy2 = sin(theta);
	}
	
	double delta_dist = hypot(dx1, dy1);
	dx1 /= delta_dist;
	dy1 /= delta_dist;

	delta_dist = hypot(dx2, dy2);
	dx2 /= delta_dist;
	dy2 /= delta_dist;
	
	if(((x2 - x1) * (x2 - x1) + (y2 - y1) * (y2 - y1)) > 4096.0)
	{
		float midt = (t1 + t2) / 2;
		
		conic_bigt_divide(cx, cy, hyp, theta1, theta2, t1, midt);
		conic_bigt_divide(cx, cy, hyp, theta1, theta2, midt, t2);
	}
	else
	{
		struct t_t t;
		t.t1 = t1;
		t.t2 = t2;
		t.x1 = x1;
		t.y1 = y1;
		t.x2 = x2;
		t.y2 = y2;
		t.deltax1 = dx1;
		t.deltay1 = dy1;
		t.deltax2 = dx2;
		t.deltay2 = dy2;
		t.dist = hypot(x2 - x1, y2 - y1);
		conic_big_length += t.dist;
		conic_big_count++;
		
		LL_ADD(struct t_t, conic_big_ct0, &t);
		conic_big_ct0 = &(*conic_big_ct0)->next;
	}
}


void generate_conic_bigts(struct conn_t *conn)
{
	struct node_t *node1, *node2;
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
	
	double ax1 = node1->x;
	double ay1 = node1->y;
	double ax2 = node1->x - node1->sats[sat1].y;
	double ay2 = node1->y + node1->sats[sat1].x;
	
	double bx1 = node2->x;
	double by1 = node2->y;
	double bx2 = node2->x - node2->sats[sat2].y;
	double by2 = node2->y + node2->sats[sat2].x;
	
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
		if(theta1 < theta2)		// go clockwise
			theta1 += M_PI * 2;
		conic_big_clock = 0;
	}
	else
	{
		if(theta2 < theta1)		// go anti-clockwise
			theta2 += M_PI * 2;
		conic_big_clock = 1;
	}

	struct t_t *t0 = NULL;
	conic_big_ct0 = &t0;
	conic_big_count = 0;
	conic_big_length = 0.0;
	
	conic_bigt_divide(cx, cy, hyp, theta1, theta2, 0, 1);
	
	conn->bigt0 = t0;
	conn->bigt_count = conic_big_count;
	conn->bigt_length = conic_big_length;
}
