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

#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include <stdint.h>

#include "types.h"
#include "stringbuf.h"
#include "buffer.h"
#include "game.h"

struct player_t *console_rcon = NULL;


void console_print(const char *fmt, ...)
{
	char *msg;
	
	va_list ap;
	
	va_start(ap, fmt);
	vasprintf(&msg, fmt, ap);
	va_end(ap);

//	if(console_rcon)
	//	print_on_client(console_rcon, msg);
//	else
		printf(msg);
	
	free(msg);
}

