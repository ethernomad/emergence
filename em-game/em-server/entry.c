/**
 * @file
 * @copyright 1998-2004 Jonathan Brown <jbrown@bluedroplet.com>
 * @license https://www.gnu.org/licenses/gpl-3.0.html
 * @homepage https://github.com/bluedroplet/emergence
 */

#define _GNU_SOURCE
#define _REENTRANT

#include <sys/types.h>
#include <sys/time.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <unistd.h>
#include <signal.h>
#include <fcntl.h>
#include <argp.h>
#include <pthread.h>

#include "types.h"
#include "stringbuf.h"
#include "buffer.h"
#include "timer.h"
#include "network.h"
#include "cvar.h"
#include "main.h"
#include "game.h"
#include "console.h"

int as_daemon = 0;


void terminate_process()
{
	exit(EXIT_SUCCESS);
}


void go_daemon()
{
	console_print("Daemonizing...\n");
	
	pid_t pid = fork();

	if(pid != 0)
		exit(EXIT_SUCCESS);

	// Become session leader
	setsid();
	
	fclose(stdin);
	fclose(stdout);
	fclose(stderr);
	
//	stdout = fopen("nfsv.log", "w");
}


int arg_max_players = -1;
struct string_t *arg_script = NULL;
struct string_t *arg_map = NULL;

const char *argp_program_version = "em-server " VERSION;
const char *argp_program_bug_address = "<jbrown@bluedroplet.com>";

static char doc[] = "Emergence Server";

static struct argp_option options[] = {
	{"daemonize",	'd',	0,			0, "Run in the background"},
	{"exec",		'e',	"SCRIPT",	0, "Execute a script on startup"},
	{"map",			'm',	"NAME",		0, "Map to load (don't execute any script)"},
	{"players",		'n',	"NUM",		0, "Maximum number of players"},
	{"port",		'p',	"PORT",		0, "Port to listen on"},
	{ 0 }
};


static error_t parse_opt (int key, char *arg, struct argp_state *state)
{
	switch (key)
	{
	case 'd':
		as_daemon = 1;
		break;
	
	case 'e':
		if(arg_script)
			free_string(arg_script);
		arg_script = new_string_text(arg);
		break;
	
	case 'm':
		if(arg_map)
			free_string(arg_map);
		arg_map = new_string_text(arg);
		break;
	
	case 'n':
		arg_max_players = atoi(arg);
		break;

	case 'p':
		net_set_listen_port(atoi(arg));
		break;

	default:
		return ARGP_ERR_UNKNOWN;
	}

	return 0;
}

static struct argp argp = { options, parse_opt, 0, doc };


int main(int argc, char *argv[])
{
	argp_parse(&argp, argc, argv, 0, 0, 0);

	if(as_daemon)
		go_daemon();
	
	init();

	main_thread();
	
	return 0;
}
