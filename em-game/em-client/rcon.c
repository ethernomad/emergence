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

#include <stdint.h>

#include "types.h"
#include "stringbuf.h"
#include "cvar.h"
#include "network.h"
#include "console.h"
#include "rcon.h"


int rconing = RCCON_OUT;


void cf_rcon(char *c)
{
/*	if(net_state != NETSTATE_CONNECTED)
	{
		console_print("You are not connected.\n");
		return;
	}
*/
	if(rconing == RCCON_ENTERING)
	{
		console_print("You are already entering rcon!\n");
		return;
	}

	if(rconing == RCCON_IN)
	{
		console_print("You are already in rcon!\n");
		return;
	}

//	net_write_int(EMNETMSG_ENTERRCON);
//	finished_writing();
	rconing = RCCON_ENTERING;
}


void cf_exit(char *c)
{
/*	if(net_state != NETSTATE_CONNECTED)
	{
		console_print("You are not connected.\n");
		return;
	}
*/
	if(rconing == RCCON_OUT)
	{
		console_print("You are not in rcon!\n");
		return;
	}

//	net_write_int(EMNETMSG_LEAVERCON);
//	finished_writing();
}


void rcon_command(char *text)
{
//	net_write_int(EMNETMSG_RCONMSG);
//	net_write_string(text);
//	finished_writing();
}


void process_inrcon()
{
	rconing = RCCON_IN;
}


void process_outrcon()
{
	rconing = RCCON_OUT;
}


void init_rcon()
{
	create_cvar_command("rcon", cf_rcon);
}
