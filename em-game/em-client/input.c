/**
 * @file
 * @copyright 1998-2004 Jonathan Brown <jbrown@bluedroplet.com>
 * @license https://www.gnu.org/licenses/gpl-3.0.html
 * @homepage https://github.com/bluedroplet/emergence
 */

#define _GNU_SOURCE
#define _REENTRANT

#include <signal.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <errno.h>

#include <linux/input.h>

#include "types.h"
#include "cvar.h"
#include "timer.h"
#include "stringbuf.h"
#include "console.h"
#include "main.h"
#include "control.h"
#include "entry.h"
#include "x.h"


int input_fd = -1;

/*
void process_input()
{
	struct input_event event;
	
	while(1)
	{
		if(read(input_fd, &event, sizeof(struct input_event)) == -1)
			break;
			
		switch(event.type)
		{
		case EV_KEY:
			process_button(event.code - BTN_MOUSE, event.value);
			break;
		
		case EV_REL:
			process_axis(event.code, (float)*(int*)&event.value);
			break;
		}
	}
}
*/

int old_buttons = 0;

void process_input()
{
	char data[3];
	
//	while(1)
	{
		TEMP_FAILURE_RETRY(read(input_fd, data, 3));

		if((data[0] & 1) != (old_buttons & 1))
			process_button(0, data[0] & 1);
			
		if((data[0] & 2) != (old_buttons & 2))
			process_button(1, data[0] & 2);

		if((data[0] & 4) != (old_buttons & 4))
			process_button(2, data[0] & 4);
		
		old_buttons = data[0];
		
		int a = data[1];
		
		if(a)
			process_axis(0, (float)a);
		
		a = data[2];
		
		if(a)
			process_axis(1, (float)a);
	}
}

/*
void create_input_cvars()
{
	create_cvar_string("input_dev", "/dev/input/mice", 0);
}
*/

void init_input()
{
	console_print("Opening mouse device: ");
	
	input_fd = open("/dev/input/mice", O_RDONLY);
	if(input_fd < 0)
			goto error;
	
/*	if(fcntl(input_fd, F_SETFL, O_NONBLOCK) == -1)
	{
		close(input_fd);
		goto error;
	}
*/	
	console_print("ok\n");
	
	return;
	
error:
	
	console_print("fail\nFalling back to X11 mouse support.\n");
	use_x_mouse = 1;
}


void kill_input()
{
}
