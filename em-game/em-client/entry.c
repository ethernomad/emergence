/**
 * @file
 * @copyright 1998-2004 Jonathan Brown <jbrown@bluedroplet.com>
 * @license https://www.gnu.org/licenses/gpl-3.0.html
 * @homepage https://github.com/bluedroplet/emergence
 */

#define _GNU_SOURCE
#define _REENTRANT

#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <stdarg.h>
#include <argp.h>
#include <fcntl.h>
#include <unistd.h>
#include <signal.h>
#include <errno.h>
#include <string.h>
#include <sys/time.h>

#include "types.h"
#include "stringbuf.h"
#include "buffer.h"
#include "timer.h"
#include "user.h"
#include "network.h"
#include "main.h"
#include "render.h"
#include "input.h"
#include "console.h"
#include "x.h"
#include "entry.h"
#include "control.h"
#include "sound.h"


void terminate_process()
{
	exit(EXIT_SUCCESS);
}

const char *argp_program_version = "em-client " VERSION;
const char *argp_program_bug_address = "<jbrown@bluedroplet.com>";

static char doc[] = "Emergence Client";

static struct argp_option options[] = {
	{ 0 }
};


static error_t parse_opt (int key, char *arg, struct argp_state *state)
{
	switch (key)
	{
	default:
		return ARGP_ERR_UNKNOWN;
	}

	return 0;
}

static struct argp argp = { options, parse_opt, 0 , doc };


int main(int argc, char *argv[])
{
	argp_parse(&argp, argc, argv, 0, 0, 0);

	init();
	main_thread();
	
	return 0;
}
