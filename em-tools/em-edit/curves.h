/**
 * @file
 * @copyright 1998-2004 Jonathan Brown <jbrown@bluedroplet.com>
 * @license https://www.gnu.org/licenses/gpl-3.0.html
 * @homepage https://github.com/bluedroplet/emergence
 */

#ifndef _INC_CURVES
#define _INC_CURVES

#include <zlib.h>

struct curve_t
{
	uint32_t index;	// for saving
	
	struct conn_pointer_t *connp0;

	struct surface_t *pre_texture_surface, *texture_surface;
	struct string_t *texture_filename;
	
	uint8_t red, green, blue, alpha;
	
	uint8_t fill_type;
	
	uint8_t texture_flip_horiz, texture_flip_vert, 
		texture_rotate_left, texture_rotate_right;

	uint8_t fixed_reps;
	
	uint32_t reps;
	float texture_length;

	int32_t pixel_offset_horiz;
	int32_t pixel_offset_vert;
	
	uint8_t width_lock_on;
	float width_lock;
	
	struct curve_t *next;
};

extern struct curve_t *curve0;

#define CURVE_SOLID		0
#define CURVE_TEXTURE	1


struct curve_pointer_t
{
	struct curve_t *curve;

	struct curve_pointer_t *next;
};


int add_curve_pointer(struct curve_pointer_t **curvep0, struct curve_t *curve);
void remove_curve_pointer(struct curve_pointer_t **curvep0, struct curve_t *curve);
int tile_all_curve_fills();
void draw_curve_ends();

void draw_curve_outlines();

struct curve_t *get_curve_from_index(int index);
double get_curve_length(struct curve_t *curve);

void add_conn_to_curves(struct conn_t *conn);
struct curve_t *get_curve(struct conn_t *conn);
void delete_all_curves();
void remove_conn_from_its_curve(struct conn_t *conn);

void gzwrite_curves(gzFile file);
int gzread_curves(gzFile file);


void run_curve_menu(struct curve_t *curve);


void run_wall_properties_dialog(void *menu, struct curve_t *curve);
void make_wall_texture_paths_relative();


#endif	// _INC_CURVES
