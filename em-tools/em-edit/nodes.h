/**
 * @file
 * @copyright 1998-2004 Jonathan Brown <jbrown@bluedroplet.com>
 * @license https://www.gnu.org/licenses/gpl-3.0.html
 * @homepage https://github.com/bluedroplet/emergence
 */

#ifndef _INC_NODES
#define _INC_NODES

#include <zlib.h>

struct sat_t
{
	float x, y;
};

struct node_t
{
	uint32_t index;	// for saving/loading

	float x, y;

	struct sat_t sats[4];

	float width[4];	// wall
	
	uint8_t fill_type;
	
	struct string_t *texture_filename;
	struct surface_t *pre_texture_surface, *texture_surface;
	uint8_t texture_flip_horiz, texture_flip_vert, 
		texture_rotate_left, texture_rotate_right;
	struct vertex_t *texture_verts;
	
	int texture_tiled;
	
	float effective_x[4], effective_y[4];
	
	uint8_t sat_conn_type[4];
	int num_conns;
	
	uint32_t bsp_index[4];

	struct node_t *next;
};


struct node_pointer_t
{
	struct node_t *node;
	struct node_pointer_t *next;
};


#define NODE_NOTHING	0
#define NODE_TEXTURE	1

#define SAT_CONN_TYPE_UNCONN	0
#define SAT_CONN_TYPE_STRAIGHT	1
#define SAT_CONN_TYPE_CONIC		2
#define SAT_CONN_TYPE_BEZIER	3

int add_node_pointer(struct node_pointer_t **nodep0, struct node_t *node);
void remove_node_pointer(struct node_pointer_t **nodep0, struct node_t *node);
int node_in_node_pointer_list(struct node_pointer_t *nodep0, struct node_t *node);

void get_satellite(int x, int y, struct node_t **node, uint8_t *sat, int *xoffset, int *yoffset);
void get_width_sat(int x, int y, struct node_t **node, uint8_t *sat, int *xoffset, int *yoffset);
void set_width_sat(struct node_t *node, uint8_t sat, float x, float y);
void get_width_sat_pos(struct node_t *node, uint8_t sat, float *x, float *y);
struct node_t *get_node(int x, int y, int *xoffset, int *y_offset);
void fix_satellites(struct node_t *node, uint8_t setsat);
void straighten_from_node(struct node_t *node);
void make_node_effective(struct node_t *node);
void set_sat_dist(struct node_t *node, uint8_t sat, float x, float y);
void insert_node(float x, float y);
void delete_node(struct node_t *node);
void delete_all_nodes();
struct node_t *get_node_from_index(int index);
int generate_hover_nodes();

int tile_all_node_textures();
int check_for_unverticied_nodes();
void generate_verticies_for_all_nodes();
int check_for_untiled_node_textures();
void finished_tiling_all_node_textures();

void invalidate_node(struct node_t *node);

void run_node_menu(struct node_t *node);

void draw_nodes();
void draw_sat_lines();
void draw_sats();
void draw_width_sat_lines();
void draw_width_sats();

extern struct node_t *node0;

void gzwrite_nodes(gzFile file);
int gzread_nodes(gzFile file);

void init_nodes();
void kill_nodes();


void run_sat_menu(struct node_t *node, int sat);


#endif	// _INC_NODES
