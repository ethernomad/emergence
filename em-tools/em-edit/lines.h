/**
 * @file
 * @copyright 1998-2004 Jonathan Brown <jbrown@bluedroplet.com>
 * @license https://www.gnu.org/licenses/gpl-3.0.html
 * @homepage https://github.com/bluedroplet/emergence
 */

#ifndef _INC_LINES
#define _INC_LINES

#include <zlib.h>

struct line_pointer_t
{
	struct line_t *line;
	struct line_pointer_t *next;
};


struct line_t
{
	uint8_t status;	// door and/or switch may be active

	struct point_t *point1, *point2;
	
	uint8_t door_red, door_green, door_blue, door_alpha;
	float door_width;
	float door_energy;
	uint8_t door_initial_state;
	uint16_t door_open_timeout;
	uint16_t door_close_timeout;
	uint16_t door_index;
	
	uint8_t switch_red, switch_green, switch_blue, switch_alpha;
	float switch_width;

	struct line_pointer_t *switch_in_door_close_list, 
		*switch_in_door_open_list, *switch_in_door_invert_list;
	
	struct line_pointer_t *switch_out_door_close_list, 
		*switch_out_door_open_list, *switch_out_door_invert_list;
	
	struct line_t *next;
};


#define LINE_STATUS_DOOR 1
#define LINE_STATUS_SWITCH 2

#define DOOR_INITIAL_STATE_OPEN 1
#define DOOR_INITIAL_STATE_CLOSED 2


void insert_line(struct point_t *point1, struct point_t *point2);
void insert_follow_curve_line(struct point_t *point1, struct point_t *point2);
struct line_t *get_line(float x, float y);
void draw_some_lines();
void draw_all_lines();
void draw_switches_and_doors();
void delete_line(struct line_t *line);
void run_door_switch_properties_dialog(void *menu, struct line_t *line);

uint32_t count_lines();
uint32_t count_line_pointers(struct line_pointer_t *linep0);

void gzwrite_line_pointer_list(gzFile file, struct line_pointer_t *linep0);
void gzwrite_lines(gzFile file);
int gzread_lines(gzFile file);

void run_line_menu(struct line_t *line);


void delete_all_lines();

#endif	// _INC_LINES
