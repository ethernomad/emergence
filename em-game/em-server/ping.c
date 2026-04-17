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
#include <stdlib.h>
#include <memory.h>

#include "types.h"
#include "stringbuf.h"
#include "buffer.h"
#include "llist.h"
#include "timer.h"
#include "network.h"
#include "ping.h"
#include "game.h"
/*

void ping_all_clients()
{
	struct player_t *cplayer = player0;
	
	while(cplayer)
	{
		struct ping_t ping;
		
		ping.index = cplayer->next_ping++;
		ping.time = get_double_time();
		
//		net_write_int(EMNETMSG_PING);
//		net_write_uint32(ping.index);
//		net_write_uint32(game_tick);
//		net_finished_writing(cplayer->conn);
		
		LL_ADD(struct ping_t, &cplayer->ping0, &ping);
		
		cplayer = cplayer->next;
	}
}


void process_pong(struct player_t *player, struct buffer_t *buffer)
{
//	net_write_int(EMNETMSG_PANG);
//	net_write_uint32(buffer_read_uint32(buffer));
//	net_finished_writing(player->conn);
}


*/
