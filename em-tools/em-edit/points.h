/**
 * @file
 * @copyright 1998-2004 Jonathan Brown <jbrown@bluedroplet.com>
 * @license https://www.gnu.org/licenses/gpl-3.0.html
 * @homepage https://github.com/bluedroplet/emergence
 */

#ifndef _INC_POINTS
#define _INC_POINTS

#include <zlib.h>

struct point_t
{
	uint32_t index;	// for saving

	struct curve_t *curve;

	float pos;		// 0..1
	
	float x, y;
	float deltax, deltay;
	float left_width, right_width;
	
	struct conn_t *conn;
	float t;
	int t_index;
	
	struct point_t *next;
};


struct point_pointer_t
{
	struct point_t *point;

	struct point_pointer_t *next;
};

void insert_point(struct curve_t *curve, float x, float y);
void remove_point(struct point_t *point);
int add_point_pointer(struct point_pointer_t **pointp0, struct point_t *point);
void update_point_positions();
void move_point(struct point_t *point, float x, float y);
struct point_t *get_point(int x, int y, int *xoffset, int *yoffset);

uint32_t count_point_pointers(struct point_pointer_t *pointp0);
uint32_t count_points();
	
void gzwrite_point_pointer_list(gzFile file, struct point_pointer_t *pointp0);
void gzwrite_points(gzFile file);
int gzread_points(gzFile file);

struct point_t *get_point_from_index(uint32_t index);
void draw_points();

void delete_all_points();
void run_point_menu(struct point_t *point);

extern struct point_t *point0;

void init_points();
void kill_points();

#endif	// _INC_POINTS
