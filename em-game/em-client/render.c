/**
 * @file
 * @copyright 1998-2004 Jonathan Brown <jbrown@bluedroplet.com>
 * @license https://www.gnu.org/licenses/gpl-3.0.html
 * @homepage https://github.com/bluedroplet/emergence
 */

#define _GNU_SOURCE
#define _REENTRANT

#include <stdint.h>
#include <signal.h>
#include <math.h>

#include "types.h"
#include "timer.h"
#include "stringbuf.h"
#include "buffer.h"
#include "cvar.h"
#include "gsub.h"
#include "render.h"
#include "game.h"
#include "console.h"
#include "input.h"
#include "entry.h"
#include "x.h"
#include "screenshot.h"
#include "ris.h"
#include "servers.h"


int g_VidMode = 2;
int g_VSync = 1;
int r_OutputFC = 0;
int r_DrawFPS = 1;
int r_FPSRed = 0;
int r_FPSGreen = 0;
int r_FPSBlue = 0;
int r_FPSColour = 0x576b;


uint32_t vid_width, vid_height;
struct surface_t *s_backbuffer;

float fr_update_time, fr_start_time, fr_old_time;
float frame_time, last_frame_start_time;

uint32_t frame, fr_old_frame;
struct string_t *fr_text;
int screenshot_next_frame;


void calc_r_FPSColour()
{
	r_FPSColour = convert_24bit_to_16bit(r_FPSRed, r_FPSGreen, r_FPSBlue);
}


void qc_r_FPSRed(int val)
{
	r_FPSRed = val;
	calc_r_FPSColour();
}


void qc_r_FPSGreen(int val)
{
	r_FPSGreen = val;
	calc_r_FPSColour();
}


void qc_r_FPSBlue(int val)
{
	r_FPSBlue = val;
	calc_r_FPSColour();
}


void init_fr()
{
	last_frame_start_time = get_wall_time();
	fr_old_frame = 0;
	fr_old_time = fr_start_time = last_frame_start_time;
	fr_update_time = fr_start_time + 1.0;
	fr_text = new_string();
}


void render_fr()
{
	if(last_frame_start_time >= fr_update_time)
	{
		string_clear(fr_text);
		string_cat_double(fr_text, (double)(frame - fr_old_frame) / (last_frame_start_time - fr_old_time), 4);

		fr_old_frame = frame;
		fr_old_time = last_frame_start_time;

		fr_update_time = (floor(last_frame_start_time - fr_start_time) + 1.0) + fr_start_time;
	}

	blit_text(0, 0, 0xff, 0xff, 0xff, s_backbuffer, fr_text->text);
}


//
// INTERFACE FUNCTIONS
//


void init_render_cvars()
{
	create_cvar_int("r_OutputFC", &r_OutputFC, 0);
//	create_cvar_int("r_DrawConsole", &r_DrawConsole, 0);
	create_cvar_int("r_DrawFPS", &r_DrawFPS, 0);
	create_cvar_int("r_FPSRed", &r_FPSRed, 0);
	create_cvar_int("r_FPSGreen", &r_FPSGreen, 0);
	create_cvar_int("r_FPSBlue", &r_FPSBlue, 0);
	create_cvar_int("r_FPSColour", &r_FPSColour, 0);
	create_cvar_int("frame", &frame, CVAR_PROTECTED);
	
	create_x_cvars();
}


void init_render()
{
	init_gsub();
	init_x();
	
	frame = 0;
	
	init_fr();
}


void kill_render()
{
	kill_x();
}


void output_fc()
{
	struct string_t *string = new_string_text("Frame: ");
	string_cat_int(string, frame);
	string_cat_char(string, '\n');
	console_print(string->text);
	free_string(string);
}

void screenshot(int state)
{
	if(!state)
		return;
	
	screenshot_next_frame = 1;
}


void render_frame()
{
	clear_surface(s_backbuffer);
	
	render_game();
	render_servers();
	
	
//	if(r_DrawFPS)
		render_fr();
	render_console();
	
	update_frame_buffer();
	frame++;
	
	if(screenshot_next_frame)
	{
		take_screenshot();
		screenshot_next_frame = 0;
	}
	
	double time = get_wall_time();
	frame_time = time - last_frame_start_time;
	last_frame_start_time = time;
}
