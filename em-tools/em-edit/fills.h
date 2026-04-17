/**
 * @file
 * @copyright 1998-2004 Jonathan Brown <jbrown@bluedroplet.com>
 * @license https://www.gnu.org/licenses/gpl-3.0.html
 * @homepage https://github.com/bluedroplet/emergence
 */

#ifndef _INC_FILLS
#define _INC_FILLS

#include <zlib.h>


struct fill_edge_t
{
	struct point_t *point1, *point2;
	int follow_curve;
	
	struct fill_edge_t *next;
};



struct fill_edge_pointer_t
{
	struct fill_edge_t *edge;
	struct fill_edge_pointer_t * next;
};


struct fill_t
{
	uint8_t type;
	
	uint8_t red, green, blue, alpha;
	
	struct string_t *texture_filename;
	struct surface_t *pre_texture, *texture;
		
	uint8_t flip_horiz, flip_vert;
	uint8_t rotate_left, rotate_right;
	
	float stretch_horiz, stretch_vert;
	float offset_horiz, offset_vert;
	
	float friction;
	
	struct fill_edge_t *edge0;
			
	struct texture_verts_t *texture_verts0;
	struct texture_polys_t *texture_polys0;
		
	uint8_t texture_tiled;
		
	struct fill_t *next;
};

#define FILL_TYPE_SOLID 0
#define FILL_TYPE_TEXTURE 1

#define FILL_EDGE_SIDE_LEFT		0
#define FILL_EDGE_SIDE_RIGHT	1

struct fill_pointer_t
{
	struct fill_t *fill;
	struct fill_pointer_t *next;
};



int add_fill_pointer(struct fill_pointer_t **fillp0, struct fill_t *fill);

void start_defining_fill(struct point_t *point);
void stop_defining_fill();
void add_point_to_fill(struct point_t *point);
void draw_fills();
int check_for_unverticied_fills();
int check_for_untiled_fills();
void finished_tiling_all_fills();

void invalidate_fills_with_point(struct point_t *point);
void generate_fill_verticies();
void make_sure_all_fills_are_clockwise();
void run_fill_properties_dialog(void *menu, struct fill_t *fill);

void gzwrite_fills(gzFile file);
int gzread_fills(gzFile file);

void delete_all_fills();

int tile_all_fills();

void run_fill_menu(struct fill_t *fill);

extern struct fill_t *fill0;

#define MAX_FILL_EDGE_SEGMENT_SIZE 7
#define MAX_FILL_EDGE_SEGMENT_SIZE_SQUARED (MAX_FILL_EDGE_SEGMENT_SIZE * MAX_FILL_EDGE_SEGMENT_SIZE)

#endif	// _INC_FILLS
