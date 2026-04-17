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

#include <netinet/in.h>
#include <stdint.h>
#include <time.h>
#include <string.h>

#include "stringbuf.h"
#include "buffer.h"
#include "servers.h"
#include "sgame.h"
#include "network.h"
#include "llist.h"
#include "minmax.h"
#include "game.h"



void process_servers(struct buffer_t *stream)
{
	uint32_t num = buffer_read_uint32(stream);
	
	struct sockaddr_in sockaddr;
	time_t t;
	
	while(num--)
	{
		sockaddr.sin_addr.s_addr = buffer_read_uint32(stream);
		sockaddr.sin_port = buffer_read_uint16(stream);
		t = buffer_read_uint32(stream);
		
		add_new_server(&rumoured_servers, &sockaddr, t);
	}
	
	save_rumoured_servers();
}


void servers_process_serverinfo(struct sockaddr_in *sockaddr)
{
	struct buffer_t *buffer = new_buffer();
		
	buffer_cat_uint8(buffer, EM_PROTO_VER);
	buffer_cat_text_max(buffer, hostname, 32);
	buffer_cat_string_max(buffer, mapname, 20);
	buffer_cat_uint8(buffer, num_players);
	buffer_cat_uint8(buffer, max_players);
	
	#ifdef NONAUTHENTICATING
	buffer_cat_uint8(buffer, 0);
	#else
	buffer_cat_uint8(buffer, 1);
	#endif
	
	uint16_t servers = min(45, count_servers());
	
	buffer_cat_uint16(buffer, servers);
	
	struct server_t *cserver = rumoured_servers;
		
	while(servers--)
	{
		buffer_cat_uint32(buffer, cserver->ip);
		buffer_cat_uint16(buffer, cserver->port);
		buffer_cat_uint32(buffer, cserver->time);
		
		LL_NEXT(cserver);
	}
	
	net_emit_server_info(sockaddr, buffer->buf, buffer->writepos);
	
	free_buffer(buffer);
}


void init_servers()
{
	load_rumoured_servers();
}


void kill_servers()
{
	save_rumoured_servers();
}
