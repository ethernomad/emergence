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

#include "vertex.h"
#include "llist.h"
#include "conns.h"
#include "bezier.h"
#include "worker.h"

void BRZ(struct bezier_t *bezier, float t, float *x, float *y)
{
	double B0 = (1 - t) * (1 - t) * (1 - t);
	double B1 = 3 * t * (1 - t) * (1 - t);
	double B2 = 3 * t * t * (1 - t);
	double B3 = t * t * t;

	*x = bezier->x1 * B0 + bezier->x2 * B1 + bezier->x3 * B2 + bezier->x4 * B3;
	*y = bezier->y1 * B0 + bezier->y2 * B1 + bezier->y3 * B2 + bezier->y4 * B3;
}


void deltaBRZ(struct bezier_t *bezier, float t, float *x, float *y)
{
	double B0 = -3 + 6 * t - 3 * t * t;
	double B1 = 3 - 12 * t + 9 * t * t;
	double B2 = 6 * t - 9 * t * t;
	double B3 = 3 * t * t;

	*x = bezier->x1 * B0 + bezier->x2 * B1 + bezier->x3 * B2 + bezier->x4 * B3;
	*y = bezier->y1 * B0 + bezier->y2 * B1 + bezier->y3 * B2 + bezier->y4 * B3;
}


struct t_t **ct0;
struct bezier_t *bezier;
int count;
double length;

int fuck = 0;

int bezier_t_divide(struct t_t *t)
{
	if(++fuck == 100)
	{
		if(check_stop_callback())
			return 0;
		
		fuck = 0;
	}
		
	
	double deltax = t->x2 - t->x1;
	double deltay = t->y2 - t->y1;

	double dist_squared = deltax * deltax + deltay * deltay;

	if(dist_squared > 0.25)
	{
		struct t_t nt;

		double ht = (t->t1 + t->t2) / 2.0;

		nt.t1 = t->t1;
		nt.x1 = t->x1;
		nt.y1 = t->y1;

		nt.t2 = ht;
		BRZ(bezier, nt.t2, &nt.x2, &nt.y2);

		bezier_t_divide(&nt);

		nt.t1 = ht;
		BRZ(bezier, nt.t1, &nt.x1, &nt.y1);

		nt.t2 = t->t2;
		nt.x2 = t->x2;
		nt.y2 = t->y2;

		if(!bezier_t_divide(&nt))
			return 0;
	}
	else
	{
		t->dist = sqrt(dist_squared);
		length += t->dist;
		count++;
		deltaBRZ(bezier, t->t1, &t->deltax1, &t->deltay1);
		deltaBRZ(bezier, t->t2, &t->deltax2, &t->deltay2);
		double delta_dist = hypot(t->deltax1, t->deltay1);
		t->deltax1 /= delta_dist;
		t->deltay1 /= delta_dist;
		delta_dist = hypot(t->deltax2, t->deltay2);
		t->deltax2 /= delta_dist;
		t->deltay2 /= delta_dist;
		LL_ADD(struct t_t, ct0, t);
		ct0 = &(*ct0)->next;
	}
	
	return 1;
}


int generate_bezier_ts(struct bezier_t *in_bezier, 
	struct t_t **out_t0, int *out_count, float *out_length)
{
	struct t_t *t0 = NULL;
	ct0 = &t0;
	bezier = in_bezier;
	count = 0;
	length = 0.0;

	struct t_t t;

	t.t1 = 0.0;
	t.t2 = 1.0;
	BRZ(bezier, t.t1, &t.x1, &t.y1);
	BRZ(bezier, t.t2, &t.x2, &t.y2);
	
	if(!bezier_t_divide(&t))
	{
		LL_REMOVE_ALL(struct t_t, &t0);
		return 0;
	}

	*out_t0 = t0;
	*out_count = count;
	*out_length = length;
	
	return 1;
}


void bezier_bigt_divide(struct t_t *t)
{
	double deltax = t->x2 - t->x1;
	double deltay = t->y2 - t->y1;

	double dist_squared = deltax * deltax + deltay * deltay;

	if(dist_squared > 4096.0)
	{
		struct t_t nt;

		double ht = (t->t1 + t->t2) / 2.0;

		nt.t1 = t->t1;
		nt.x1 = t->x1;
		nt.y1 = t->y1;

		nt.t2 = ht;
		BRZ(bezier, nt.t2, &nt.x2, &nt.y2);

		bezier_bigt_divide(&nt);

		nt.t1 = ht;
		BRZ(bezier, nt.t1, &nt.x1, &nt.y1);

		nt.t2 = t->t2;
		nt.x2 = t->x2;
		nt.y2 = t->y2;

		bezier_bigt_divide(&nt);
	}
	else
	{
		t->dist = sqrt(dist_squared);
		length += t->dist;
		count++;
		deltaBRZ(bezier, t->t1, &t->deltax1, &t->deltay1);
		deltaBRZ(bezier, t->t2, &t->deltax2, &t->deltay2);
		double delta_dist = hypot(t->deltax1, t->deltay1);
		t->deltax1 /= delta_dist;
		t->deltay1 /= delta_dist;
		delta_dist = hypot(t->deltax2, t->deltay2);
		t->deltax2 /= delta_dist;
		t->deltay2 /= delta_dist;
		LL_ADD(struct t_t, ct0, t);
		ct0 = &(*ct0)->next;
	}
}


void generate_bezier_bigts(struct bezier_t *in_bezier, 
	struct t_t **out_t0, int *out_count, float *out_length)
{
	struct t_t *t0 = NULL;
	ct0 = &t0;
	bezier = in_bezier;
	count = 0;
	length = 0.0;

	struct t_t t;

	t.t1 = 0.0;
	t.t2 = 1.0;
	BRZ(bezier, t.t1, &t.x1, &t.y1);
	BRZ(bezier, t.t2, &t.x2, &t.y2);
	
	bezier_bigt_divide(&t);

	*out_t0 = t0;
	*out_count = count;
	*out_length = length;
}
