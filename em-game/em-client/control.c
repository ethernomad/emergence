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

#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <stdlib.h>
#include <math.h>
#include <pthread.h>
#include <unistd.h>
#include <errno.h>

#include <sys/epoll.h>
#include <sys/poll.h>

#include "types.h"
#include "stringbuf.h"
#include "buffer.h"
#include "cvar.h"
#include "network.h"
#include "timer.h"
#include "alarm.h"
#include "sgame.h"
#include "main.h"
#include "console.h"
#include "game.h"
#include "control.h"
#include "entry.h"
#include "render.h"
#include "input.h"
#include "servers.h"


int control_kill_pipe[2];

pthread_mutex_t control_mutex = PTHREAD_RECURSIVE_MUTEX_INITIALIZER_NP;
pthread_t control_thread_id;

float mouse_speed = 1.0;


void control_escape(int state)
{
	if(!state)
		return;
	
	if(server_discovery_started)
	{
		stop_server_discovery();
		console_toggle();
	}
	else
	{
		if(r_DrawConsole)
			console_toggle();
		else
			start_server_discovery();
	}
}


void control_enter(int state)
{
	if(r_DrawConsole)
		console_enter(state);
	else
	{
		if(server_discovery_started)
			server_enter(state);
		else
			toggle_ready(state);
	}
}


void control_up(int state)
{
	if(r_DrawConsole)
		next_command(state);
	else
		server_up(state);
}


void control_down(int state)
{
	if(r_DrawConsole)
		prev_command(state);
	else
		server_down(state);
}


struct
{
	char *name;
	void *func;
	void *uifunc;

} controls[] = 
{
	{"",			NULL,	NULL},
	{"escape",		NULL,	control_escape},
	{"1",			NULL,	NULL},
	{"2",			NULL,	NULL},
	{"3",			NULL,	NULL},
	{"4",			NULL,	NULL},
	{"5",			NULL,	NULL},
	{"6",			NULL,	NULL},
	{"7",			NULL,	NULL},
	{"8",			NULL,	NULL},
	{"9",			NULL,	NULL},
	{"0",			NULL,	NULL},
	{"-",			NULL,	NULL},
	{"=",			NULL,	NULL},
	{"backspace",	NULL,	console_backspace},
	{"tab",			NULL,	console_tab},
	{"q",			NULL,	NULL},
	{"w",			NULL,	NULL},
	{"e",			NULL,	NULL},
	{"r",			NULL,	NULL},
	{"t",			NULL,	NULL},
	{"y",			NULL,	NULL},
	{"u",			NULL,	NULL},
	{"i",			NULL,	NULL},
	{"o",			NULL,	NULL},
	{"p",			NULL,	NULL},
	{"[",			NULL,	NULL},
	{"]",			NULL,	NULL},
	{"enter",		NULL,	control_enter},
	{"lctrl",		NULL,	NULL},
	{"a",			NULL,	NULL},
	{"s",			NULL,	NULL},//screenshot},
	{"d",			NULL,	NULL},
	{"f",			NULL,	NULL},
	{"g",			NULL,	NULL},
	{"h",			NULL,	NULL},
	{"j",			NULL,	NULL},
	{"k",			NULL,	NULL},
	{"l",			NULL,	NULL},
	{";",			NULL,	NULL},
	{"'",			NULL,	NULL},
	{"`",			NULL,	control_escape},
	{"lshift",		NULL,	NULL},
	{"\\",			NULL,	NULL},
	{"z",			NULL,	NULL},
	{"x",			NULL,	NULL},
	{"c",			NULL,	NULL},
	{"v",			NULL,	NULL},
	{"b",			NULL,	NULL},
	{"n",			NULL,	NULL},
	{"m",			NULL,	NULL},
	{",",			NULL,	NULL},
	{".",			NULL,	NULL},
	{"/",			NULL,	NULL},
	{"rshift",		NULL,	NULL},
	{"pad*",		NULL,	NULL},
	{"lalt",		NULL,	NULL},
	{"space",		NULL,	NULL},
	{"capslock",	NULL,	NULL},
	{"f1",			NULL,	toggle_help},
	{"f2",			NULL,	NULL},
	{"f3",			NULL,	NULL},
	{"f4",			NULL,	NULL},
	{"f5",			NULL,	NULL},
	{"f6",			NULL,	NULL},
	{"f7",			NULL,	NULL},
	{"f8",			NULL,	NULL},
	{"f9",			NULL,	NULL},
	{"f10",			NULL,	screenshot},
	{"numlock",		NULL,	NULL},
	{"scroll",		NULL,	NULL},
	{"pad7",		NULL,	NULL},
	{"pad8",		NULL,	prev_command},
	{"pad9",		NULL,	NULL},
	{"pad-",		NULL,	NULL},
	{"pad4",		NULL,	NULL},
	{"pad5",		NULL,	NULL},
	{"pad6",		NULL,	NULL},
	{"pad+",		NULL,	NULL},
	{"pad1",		NULL,	NULL},
	{"pad2",		NULL,	next_command},
	{"pad3",		NULL,	NULL},
	{"pad0",		NULL,	NULL},
	{"pad.",		NULL,	NULL},
	{"102",			NULL,	NULL},
	{"f11",			NULL,	NULL},
	{"f12",			NULL,	NULL},
	{"",			NULL,	NULL},
	{"",			NULL,	NULL},
	{"",			NULL,	NULL},
	{"up",			NULL,	control_up},
	{"",			NULL,	NULL},
	{"left",		NULL,	NULL},
	{"",			NULL,	NULL},
	{"right",		NULL,	NULL},
	{"",			NULL,	NULL},
	{"down",		NULL,	control_down},
	{"",			NULL,	NULL},
	{"",			NULL,	NULL},
	{"",			NULL,	NULL},
	{"",			NULL,	NULL},
	{"",			NULL,	NULL},
	{"",			NULL,	NULL},
	{"",			NULL,	NULL},
	{"",			NULL,	NULL},
	{"",			NULL,	NULL},
	{"",			NULL,	NULL},
	{"",			NULL,	NULL},
	{"",			NULL,	NULL},
	{"",			NULL,	NULL},
	{"",			NULL,	NULL},
	{"",			NULL,	NULL},
	{"",			NULL,	NULL},
	{"",			NULL,	NULL},
	{"",			NULL,	NULL},
	{"",			NULL,	NULL},
	{"",			NULL,	NULL},
	{"",			NULL,	NULL},
	{"",			NULL,	NULL},
	{"",			NULL,	NULL},
	{"",			NULL,	NULL},
	{"",			NULL,	NULL},
	{"",			NULL,	NULL},
	{"",			NULL,	NULL},
	{"",			NULL,	NULL},
	{"",			NULL,	NULL},
	{"",			NULL,	NULL},
	{"",			NULL,	NULL},
	{"",			NULL,	NULL},
	{"",			NULL,	NULL},
	{"",			NULL,	NULL},
	{"",			NULL,	NULL},
	{"",			NULL,	NULL},
	{"",			NULL,	NULL},
	{"",			NULL,	NULL},
	{"",			NULL,	NULL},
	{"",			NULL,	NULL},
	{"",			NULL,	NULL},
	{"",			NULL,	NULL},
	{"",			NULL,	NULL},
	{"",			NULL,	NULL},
	{"",			NULL,	NULL},
	{"",			NULL,	NULL},
	{"",			NULL,	NULL},
	{"",			NULL,	NULL},
	{"",			NULL,	NULL},
	{"",			NULL,	NULL},
	{"",			NULL,	NULL},
	{"",			NULL,	NULL},
	{"",			NULL,	NULL},
	{"",			NULL,	NULL},
	{"",			NULL,	NULL},
	{"",			NULL,	NULL},
	{"",			NULL,	NULL},
	{"",			NULL,	NULL},
	{"",			NULL,	NULL},
	{"",			NULL,	NULL},
	{"padenter",	NULL,	console_enter},
	{"rctrl",		NULL,	NULL},
	{"",			NULL,	NULL},
	{"",			NULL,	NULL},
	{"",			NULL,	NULL},
	{"",			NULL,	NULL},
	{"",			NULL,	NULL},
	{"",			NULL,	NULL},
	{"",			NULL,	NULL},
	{"",			NULL,	NULL},
	{"",			NULL,	NULL},
	{"",			NULL,	NULL},
	{"",			NULL,	NULL},
	{"",			NULL,	NULL},
	{"",			NULL,	NULL},
	{"",			NULL,	NULL},
	{"",			NULL,	NULL},
	{"",			NULL,	NULL},
	{"",			NULL,	NULL},
	{"",			NULL,	NULL},
	{"",			NULL,	NULL},
	{"",			NULL,	NULL},
	{"",			NULL,	NULL},
	{"pad,",		NULL,	NULL},
	{"",			NULL,	NULL},
	{"pad/",		NULL,	NULL},
	{"",			NULL,	NULL},
	{"sysrq",		NULL,	NULL},
	{"ralt",		NULL,	NULL},
	{"",			NULL,	NULL},
	{"",			NULL,	NULL},
	{"",			NULL,	NULL},
	{"",			NULL,	NULL},
	{"",			NULL,	NULL},
	{"",			NULL,	NULL},
	{"",			NULL,	NULL},
	{"",			NULL,	NULL},
	{"",			NULL,	NULL},
	{"",			NULL,	NULL},
	{"",			NULL,	NULL},
	{"",			NULL,	NULL},
	{"pause",		NULL,	NULL},
	{"",			NULL,	NULL},
	{"home",		NULL,	NULL},
//	{"up",			NULL,	prev_command},
	{"",			NULL,	prev_command},
	{"pgup",		NULL,	NULL},
	{"",			NULL,	NULL},
//	{"left",		NULL,	NULL},
	{"",			NULL,	NULL},
	{"",			NULL,	NULL},
//	{"right",		NULL,	NULL},
	{"",			NULL,	NULL},
	{"",			NULL,	NULL},
	{"end",			NULL,	NULL},
//	{"down",		NULL,	next_command},
	{"",			NULL,	next_command},
	{"pgdn",		NULL,	NULL},
	{"insert",		NULL,	NULL},
	{"delete",		NULL,	NULL},
	{"",			NULL,	NULL},
	{"",			NULL,	NULL},
	{"",			NULL,	NULL},
	{"",			NULL,	NULL},
	{"",			NULL,	NULL},
	{"",			NULL,	NULL},
	{"",			NULL,	NULL},
	{"lwin",		NULL,	NULL},
	{"rwin",		NULL,	NULL},
	{"apps",		NULL,	NULL},
	{"",			NULL,	NULL},
	{"",			NULL,	NULL},
	{"",			NULL,	NULL},
	{"",			NULL,	NULL},
	{"",			NULL,	NULL},
	{"",			NULL,	NULL},
	{"",			NULL,	NULL},
	{"",			NULL,	NULL},
	{"",			NULL,	NULL},
	{"",			NULL,	NULL},
	{"",			NULL,	NULL},
	{"",			NULL,	NULL},
	{"",			NULL,	NULL},
	{"",			NULL,	NULL},
	{"",			NULL,	NULL},
	{"",			NULL,	NULL},
	{"",			NULL,	NULL},
	{"",			NULL,	NULL},
	{"",			NULL,	NULL},
	{"",			NULL,	NULL},
	{"",			NULL,	NULL},
	{"",			NULL,	NULL},
	{"",			NULL,	NULL},
	{"",			NULL,	NULL},
	{"",			NULL,	NULL},
	{"",			NULL,	NULL},
	{"",			NULL,	NULL},
	{"",			NULL,	NULL},
	{"",			NULL,	NULL},
	{"",			NULL,	NULL},
	{"",			NULL,	NULL},
	{"",			NULL,	NULL},
	{"",			NULL,	NULL},
	{"btn_0",		NULL,	NULL},
	{"btn_1",		NULL,	NULL},//screenshot},
	{"btn_2",		NULL,	NULL},
	{"btn_3",		NULL,	NULL},
	{"btn_4",		NULL,	NULL},
	{"btn_5",		NULL,	NULL},
	{"btn_6",		NULL,	NULL},
	{"btn_7",		NULL,	NULL},
	{"btn_8",		NULL,	NULL},
	{"btn_9",		NULL,	NULL},
	{"btn_10",		NULL,	NULL},
	{"btn_11",		NULL,	NULL},
	{"btn_12",		NULL,	NULL},
	{"btn_13",		NULL,	NULL},
	{"btn_14",		NULL,	NULL},
	{"btn_15",		NULL,	NULL},
	{"btn_16",		NULL,	NULL},
	{"btn_17",		NULL,	NULL},
	{"btn_18",		NULL,	NULL},
	{"btn_19",		NULL,	NULL},
	{"btn_20",		NULL,	NULL},
	{"btn_21",		NULL,	NULL},
	{"btn_22",		NULL,	NULL},
	{"btn_23",		NULL,	NULL},
	{"btn_24",		NULL,	NULL},
	{"btn_25",		NULL,	NULL},
	{"btn_26",		NULL,	NULL},
	{"btn_27",		NULL,	NULL},
	{"btn_28",		NULL,	NULL},
	{"btn_29",		NULL,	NULL},
	{"btn_30",		NULL,	NULL},
	{"btn_31",		NULL,	NULL},
	{"axis_0",		NULL,	NULL},
	{"axis_1",		NULL,	NULL},
	{"axis_2",		NULL,	NULL},
	{"axis_3",		NULL,	NULL},
	{"axis_4",		NULL,	NULL},
	{"axis_5",		NULL,	NULL},
	{"axis_6",		NULL,	NULL},
	{"axis_7",		NULL,	NULL},
	{"axis_8",		NULL,	NULL},
	{"axis_9",		NULL,	NULL},
	{"axis_10",		NULL,	NULL},
	{"axis_11",		NULL,	NULL},
	{"axis_12",		NULL,	NULL},
	{"axis_13",		NULL,	NULL},
	{"axis_14",		NULL,	NULL},
	{"axis_15",		NULL,	NULL},
	{"axis_16",		NULL,	NULL},
	{"axis_17",		NULL,	NULL},
	{"axis_18",		NULL,	NULL},
	{"axis_19",		NULL,	NULL},
	{"axis_20",		NULL,	NULL},
	{"axis_21",		NULL,	NULL},
	{"axis_22",		NULL,	NULL},
	{"axis_23",		NULL,	NULL},
	{"axis_24",		NULL,	NULL},
	{"axis_25",		NULL,	NULL},
	{"axis_26",		NULL,	NULL},
	{"axis_27",		NULL,	NULL},
	{"axis_28",		NULL,	NULL},
	{"axis_29",		NULL,	NULL},
	{"axis_30",		NULL,	NULL},
	{"axis_31",		NULL,	NULL}
};

float roll;
int roll_changed;
int rolling_left;
int rolling_right;

void action_thrust(uint32_t state)
{
	if(game_state != GAMESTATE_PLAYING)
		return;

	net_emit_uint8(game_conn, EMMSG_THRUST);

	if(state)
		net_emit_float(game_conn, 1.0f);
	else
		net_emit_float(game_conn, 0.0f);

	net_emit_end_of_stream(game_conn);
}


void action_brake(uint32_t state)
{
	if(game_state != GAMESTATE_PLAYING)
		return;

	if(state)
		net_emit_uint8(game_conn, EMMSG_BRAKE);
	else
		net_emit_uint8(game_conn, EMMSG_NOBRAKE);
	
	net_emit_end_of_stream(game_conn);
}


void action_roll(float val)
{
	roll += val * mouse_speed;
	roll_changed = 1;
}


void action_roll_left(uint32_t state)
{
	if(game_state != GAMESTATE_PLAYING)
		return;

	if(state)
		net_emit_uint8(game_conn, EMMSG_ROLL_LEFT);
	else
		net_emit_uint8(game_conn, EMMSG_NOROLL_LEFT);
	
	net_emit_end_of_stream(game_conn);
}


void action_roll_right(uint32_t state)
{
	if(game_state != GAMESTATE_PLAYING)
		return;

	if(state)
		net_emit_uint8(game_conn, EMMSG_ROLL_RIGHT);
	else
		net_emit_uint8(game_conn, EMMSG_NOROLL_RIGHT);
	
	net_emit_end_of_stream(game_conn);
}


void action_fire_left(uint32_t state)
{
	if(game_state != GAMESTATE_PLAYING)
		return;

	net_emit_uint8(game_conn, EMMSG_FIRELEFT);
	net_emit_uint8(game_conn, (uint8_t)state);
	net_emit_end_of_stream(game_conn);
}


void action_fire_right(uint32_t state)
{
	if(game_state != GAMESTATE_PLAYING)
		return;

	net_emit_uint8(game_conn, EMMSG_FIRERIGHT);
	net_emit_uint8(game_conn, (uint8_t)state);
	net_emit_end_of_stream(game_conn);
}


void action_fire_rail(uint32_t state)
{
	if(game_state != GAMESTATE_PLAYING)
		return;

	if(!state)
		return;

	net_emit_uint8(game_conn, EMMSG_FIRERAIL);
	net_emit_end_of_stream(game_conn);
}


void action_drop_mine(uint32_t state)
{
	if(game_state != GAMESTATE_PLAYING)
		return;

	if(!state)
		return;

	net_emit_uint8(game_conn, EMMSG_DROPMINE);
	net_emit_end_of_stream(game_conn);
}


struct
{
	char *name;
	void *bool_func;
	void *cont_func;

} actions[] = 
{
	{"thrust",		action_thrust,		NULL}, 
	{"brake",		action_brake,		NULL}, 
	{"roll",		NULL,				action_roll}, 
	{"roll_left",	action_roll_left,	NULL}, 
	{"roll_right",	action_roll_right,	NULL}, 
	{"fire_left",	action_fire_left,	NULL}, 
	{"fire_right",	action_fire_right,	NULL}, 
	{"fire_rail",	action_fire_rail,	NULL}, 
	{"drop_mine",	action_drop_mine,	NULL}
};


int numactions = 9;


#define CONTROL_TICK_INTERVAL 0.05


double next_control_tick;

void process_control_alarm()
{
	double time = get_wall_time();
		
	if(time > next_control_tick)
	{
		pthread_mutex_lock(&control_mutex);
		
		if(game_state == GAMESTATE_PLAYING)
		{
			pthread_mutex_lock(&control_mutex);
			
			if(roll_changed)
			{
				net_emit_uint8(game_conn, EMMSG_ROLL);
				net_emit_float(game_conn, roll);
				net_emit_end_of_stream(game_conn);
				roll_changed = 0;
				roll = 0.0;
			}
		
			if(rolling_left)
			{
				net_emit_uint8(game_conn, EMMSG_ROLL);
				net_emit_float(game_conn, -0.2);
				net_emit_end_of_stream(game_conn);
			}
			
			if(rolling_right)
			{
				net_emit_uint8(game_conn, EMMSG_ROLL);
				net_emit_float(game_conn, 0.2);
				net_emit_end_of_stream(game_conn);
			}
			
			pthread_mutex_unlock(&control_mutex);
		}
		
		next_control_tick = ((int)(time / CONTROL_TICK_INTERVAL) + 1) * 
			(double)CONTROL_TICK_INTERVAL;
		
		pthread_mutex_unlock(&control_mutex);
	}
}


int shift = 0;


char get_ascii(int key)
{
	switch(key)
	{
	case 53:
		return '/';
	
	case 57:
		return ' ';

	case 11:
		return '0';
	
	case 2:
		return '1';
	
	case 3:
		return '2';
	
	case 4:
		return '3';
	
	case 5:
		return '4';
	
	case 6:
		return '5';
	
	case 7:
		return '6';
	
	case 8:
		return '7';
	
	case 9:
		return '8';
	
	case 10:
		return '9';

	case 52:
		return '.';

	case 0x0c:
		if(shift)
			return '_';
		else
			return '-';

	case 0x0d:
		if(shift)
			return '+';
		else
			return '=';

	case 0x27:
		if(shift)
			return ':';
		else
			return ';';

	case 0x1e:
		return 'a' - shift;

	case 0x30:
		return 'b' - shift;

	case 0x2e:
		return 'c' - shift;

	case 0x20:
		return 'd' - shift;

	case 0x12:
		return 'e' - shift;

	case 0x21:
		return 'f' - shift;

	case 0x22:
		return 'g' - shift;

	case 0x23:
		return 'h' - shift;

	case 0x17:
		return 'i' - shift;

	case 0x24:
		return 'j' - shift;

	case 0x25:
		return 'k' - shift;

	case 0x26:
		return 'l' - shift;

	case 0x32:
		return 'm' - shift;

	case 0x31:
		return 'n' - shift;

	case 0x18:
		return 'o' - shift;

	case 0x19:
		return 'p' - shift;

	case 0x10:
		return 'q' - shift;

	case 0x13:
		return 'r' - shift;

	case 0x1f:
		return 's' - shift;

	case 0x14:
		return 't' - shift;

	case 0x16:
		return 'u' - shift;

	case 0x2f:
		return 'v' - shift;

	case 0x11:
		return 'w' - shift;

	case 0x2d:
		return 'x' - shift;

	case 0x15:
		return 'y' - shift;

	case 0x2c:
		return 'z' - shift;

	default:
		return '\0';
	}
}


void process_keypress(uint32_t key, int state)
{
	pthread_mutex_lock(&control_mutex);		// called from x thread
	
	if(key >= 256)
		goto end;
	
	void (*func)(int);

	func = controls[key].uifunc;
	if(func)
		func(state);
	
	if(!r_DrawConsole && !server_discovery_started)
	{
		func = controls[key].func;
		if(func)
			func(state);
	//	else
	//		console_print("%u\n", key);
	}
	
	if(key == 42 || key == 54)
	{
		if(state)
			shift = 0x20;
		else
			shift = 0x0;
	}

	if(!state)
		goto end;
	
	if(state)
	{
		char c = get_ascii(key);

		if(c)
		{
			console_keypress(c);
		}
	}

	end:
	pthread_mutex_unlock(&control_mutex);
}


void process_button(uint32_t control, int state)
{
	pthread_mutex_lock(&control_mutex);
	
	if(control >= 32)
		return;

	control += 256;
	
	void (*func)(int);

	func = controls[control].uifunc;

	if(func)
		func(state);

	func = controls[control].func;

	if(func)
		func(state);

	pthread_mutex_unlock(&control_mutex);
}


void process_axis(uint32_t axis, float val)
{
	pthread_mutex_lock(&control_mutex);
	
	if(axis >= 32)
		return;

	void (*func)(float);

	func = controls[axis + 288].func;
	
	if(func)
		func(val);

	pthread_mutex_unlock(&control_mutex);
}


void cf_controls(char *params)
{
	int c;

	for(c = 0; c < 320; c++)
	{
		if(controls[c].name[0] != '\0')
		{
			console_print(controls[c].name);
			console_print("\n");
		}
	}
}


void cf_actions(char *params)
{
	int a;

	for(a = 0; a < numactions; a++)
	{
		if(actions[a].name[0] != '\0')
		{
			console_print(actions[a].name);
			console_print("\n");
		}
	}
}	


void cf_bind(char *params)
{
	pthread_mutex_lock(&control_mutex);
	
	char *token = strtok(params, " ");
	
	if(!token)
	{
		console_print("usage: bind <control> <action>\n");
		goto end;
	}

	int c;

	for(c = 0; c < 320; c++)
	{
		if(strcmp(controls[c].name, token) == 0)
			break;
	}

	if(c == 320)
	{
		struct string_t *s = new_string_text("control \"");
		string_cat_text(s, token);
		string_cat_text(s, "\" not found\n");
		
		console_print(s->text);
		
		free_string(s);
		
		goto end;
	}

	token = strtok(NULL, " ");

	if(!token)
	{
		controls[c].func = NULL;
		goto end;
	}

	int a;

	for(a = 0; a < numactions; a++)
	{
		if(strcmp(actions[a].name, token) == 0)
			break;
	}

	if(a == numactions)
	{
		struct string_t *s = new_string_text("action \"");
		string_cat_text(s, token);
		string_cat_text(s, "\" not found\n");
		
		console_print(s->text);
		
		free_string(s);
		
		goto end;
	}
	
	if(c < 288)
	{
		if(!actions[a].bool_func)
		{
			console_print("Boolean controls must be bound to boolean actions.\n");
			goto end;
		}
		
		controls[c].func = actions[a].bool_func;
	}

	if(c >= 288)
	{
		if(!actions[a].cont_func)
		{
			console_print("Continuous controls must be bound to continuous actions.\n");
			goto end;
		}
		
		controls[c].func = actions[a].cont_func;
	}

	end:
	pthread_mutex_unlock(&control_mutex);
}


void dump_bindings(FILE *file)
{
	pthread_mutex_lock(&control_mutex);
	
	int c, a;

	for(c = 0; c < 320; c++)
	{
		if(controls[c].func)
		{
			for(a = 0; a < numactions; a++)
			{
				if(actions[a].bool_func == controls[c].func || 
					actions[a].cont_func == controls[c].func)
					break;
			}
			
			if(a == numactions)
				assert(0);
			
			struct string_t *s = new_string_text("bind ");
			string_cat_text(s, controls[c].name);
			string_cat_char(s, ' ');
			string_cat_text(s, actions[a].name);
			string_cat_char(s, '\n');
			
			fputs(s->text, file);
			
			free_string(s);
		}
	}

	pthread_mutex_unlock(&control_mutex);
}


void *control_thread(void *a)
{
	struct pollfd *fds;
	int fdcount;
	
	fdcount = 2;
	
	fds = calloc(sizeof(struct pollfd), fdcount);
	
	fds[0].fd = input_fd; 				fds[0].events = POLLIN;
	fds[1].fd = control_kill_pipe[0];	fds[1].events = POLLIN;
	

	while(1)
	{
		if(TEMP_FAILURE_RETRY(poll(fds, fdcount, -1)) == -1)
		{
			free(fds);
			pthread_exit(NULL);
		}

		if(fds[0].revents & POLLIN)
			process_input();
		
		if(fds[1].revents & POLLIN)
		{
			free(fds);
			pthread_exit(NULL);
		}
	}
}


/*
void *control_thread(void *a)
{
	int epoll_fd = epoll_create(3);
	
	struct epoll_event ev;
	
	ev.events = EPOLLIN | EPOLLET;
	ev.data.u32 = 0;
	if(epoll_ctl(epoll_fd, EPOLL_CTL_ADD, input_fd, &ev) == -1)
		printf("err\n");

	ev.events = EPOLLIN | EPOLLET;
	ev.data.u32 = 1;
	if(epoll_ctl(epoll_fd, EPOLL_CTL_ADD, control_timer_fd, &ev) == -1)
		printf("err2\n");

	ev.events = EPOLLIN | EPOLLET;
	ev.data.u32 = 2;
	epoll_ctl(epoll_fd, EPOLL_CTL_ADD, control_kill_pipe[0], &ev);

	while(1)
	{
		epoll_wait(epoll_fd, &ev, 1, -1);
		
		switch(ev.data.u32)
		{
		case 0:
			pthread_mutex_lock(&control_mutex);
			process_input();
			pthread_mutex_unlock(&control_mutex);
			break;
		
		case 1:
			process_control_alarm();
			break;
		
		case 2:
			pthread_exit(NULL);
			break;
		}
	}
}
*/


void create_control_cvars()
{
	create_cvar_command("bind", cf_bind);
	create_cvar_command("controls", cf_controls);
	create_cvar_command("actions", cf_actions);
	
	create_cvar_float("mouse_speed", &mouse_speed, 0);
}


void init_control()
{
	double time = get_wall_time();
	next_control_tick = ((int)(time / CONTROL_TICK_INTERVAL) + 1) * (double)CONTROL_TICK_INTERVAL;
	
	create_alarm_listener(process_control_alarm);
	pipe(control_kill_pipe);
	pthread_create(&control_thread_id, NULL, control_thread, NULL);
}


void kill_control()
{
	char c;
	TEMP_FAILURE_RETRY(write(control_kill_pipe[1], &c, 1));
	pthread_join(control_thread_id, NULL);
	close(control_kill_pipe[0]);
	close(control_kill_pipe[1]);
}
