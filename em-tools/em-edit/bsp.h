/**
 * @file
 * @copyright 1998-2004 Jonathan Brown <jbrown@bluedroplet.com>
 * @license https://www.gnu.org/licenses/gpl-3.0.html
 * @homepage https://github.com/bluedroplet/emergence
 */

#ifndef _INC_BSP
#define _INC_BSP

#include <zlib.h>

void invalidate_bsp_tree();
void generate_bsp_tree();
void generate_ui_bsp_tree();
void gzwrite_bsp_tree(gzFile file);
void draw_bsp_tree();
void kill_bsp_tree();

extern int generate_bsp, generate_ui_bsp;
struct curve_t *get_curve_bsp(float x, float y);
struct node_t *get_node_bsp(float x, float y);
struct fill_t *get_fill_bsp(float x, float y);
void finished_generating_bsp_tree();
void finished_generating_ui_bsp_tree();
void clear_bsp_trees();
void more_bsp();
void less_bsp();

#endif	// _INC_BSP
