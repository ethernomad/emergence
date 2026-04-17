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

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/poll.h>

#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <unistd.h>
#include <pthread.h>
#include <errno.h>
#include <string.h>
#include <math.h>

#include "alarm.h"
#include "timer.h"
#include "servers.h"
#include "network.h"
#include "sgame.h"
#include "user.h"
#include "stringbuf.h"
#include "buffer.h"
#include "llist.h"
#include "minmax.h"
#include "gsub.h"
#include "render.h"
#include "console.h"


int servers_kill_pipe[2] = {-1, -1};
int servers_info_pipe[2] = {-1, -1};

pthread_t servers_thread_id;
pthread_mutex_t servers_mutex = PTHREAD_MUTEX_INITIALIZER;

struct server_t *unqueried_servers = NULL;

int selected_server;
int top_server_visible;


struct queried_server_t
{
	uint32_t ip;
	uint16_t port;
	float stamp;
	
	struct queried_server_t *next;
	
} *queried_servers = NULL;


struct found_server_t
{
	uint32_t ip;
	uint16_t port;
	
	float ping;

	struct string_t *host_name;
	struct string_t *map_name;
	
	int num_players;
	int max_players;
	int authenticating;
	
	struct found_server_t *next;
		
} *found_servers = NULL;


int num_servers_unqueried;
int num_servers_queried;
int num_servers_found;
int num_servers_new_proto;


int server_discovery_started = 0;


float servers_first_output;
float servers_next_output;

#define SERVER_QUERY_OUTPUT_RATE	20


struct queried_server_t *find_queried_server(uint32_t ip, uint16_t port)
{
	struct queried_server_t *cserver = queried_servers;
		
	while(cserver)
	{
		if(cserver->ip == ip &&
			cserver->port == port)
			return cserver;
		
		LL_NEXT(cserver);
	}
	
	return NULL;
}


void output_server_query()
{
	struct sockaddr_in sockaddr;
	struct queried_server_t queried_server;	
	
	if(unqueried_servers)
	{
		sockaddr.sin_family = AF_INET;
		sockaddr.sin_addr.s_addr = unqueried_servers->ip;
		sockaddr.sin_port = unqueried_servers->port;
			
		net_emit_server_info(&sockaddr, NULL, 0);
		
		queried_server.ip = unqueried_servers->ip;
		queried_server.port = unqueried_servers->port;
		queried_server.stamp = get_wall_time();
		
		LL_ADD(struct queried_server_t, &queried_servers, &queried_server);
		LL_REMOVE(struct server_t, &unqueried_servers, unqueried_servers);
		
		num_servers_unqueried--;
		num_servers_queried++;
	}
}


void servers_alarm()
{
	if(!server_discovery_started)
		return;
	
	float time = get_wall_time();
	
	if(time >= servers_next_output)
	{
		pthread_mutex_lock(&servers_mutex);
		output_server_query();
		pthread_mutex_unlock(&servers_mutex);
	
		servers_next_output = (floor((time - servers_first_output) * 
			SERVER_QUERY_OUTPUT_RATE) + 1.0) / SERVER_QUERY_OUTPUT_RATE + servers_first_output;
	}
}


void server_up(int state)
{
	if(!state)
		return;
	
	pthread_mutex_lock(&servers_mutex);
	
	if(selected_server == -1)
	{
		selected_server = 0;
		goto end;
	}
	
	if(selected_server > 0)
		selected_server--;
	
	if(selected_server < top_server_visible)
		top_server_visible--;
	
	end:
	pthread_mutex_unlock(&servers_mutex);
}


void server_down_in_lock()
{
	if(selected_server == -1)
	{
		selected_server = 0;
		return;
	}
	
	if(selected_server < num_servers_found - 1)
		selected_server++;
	
	if(selected_server - top_server_visible >= 8)
		top_server_visible++;
}


void server_down(int state)
{
	if(!state)
		return;
	
	pthread_mutex_lock(&servers_mutex);
	server_down_in_lock();
	pthread_mutex_unlock(&servers_mutex);
}


void server_enter(int state)
{
	if(!state)
		return;
	
	pthread_mutex_lock(&servers_mutex);
	
	struct found_server_t *cserver = found_servers;
	int i = 0;
		
	while(cserver)
	{
		if(i == selected_server)
		{
			if(!r_DrawConsole)
				console_toggle();
			
			em_connect(cserver->ip, cserver->port);
			goto end;
		}
		
		LL_NEXT(cserver);
		i++;
	}
		
	end:

	pthread_mutex_unlock(&servers_mutex);
}


void add_new_found_server(struct found_server_t *found_server)
{
	// see if this server is already in the list
	
	struct found_server_t *cserver = found_servers, *next;
	int i;
	
	while(cserver)
	{
		if(cserver->ip == found_server->ip &&
			cserver->port == found_server->port)
			return;
		
		LL_NEXT(cserver);
	}
	
	
	num_servers_found++;

	// if rumoured_servers is NULL, then create new server here

	if(!found_servers)
	{
		found_servers = malloc(sizeof(struct found_server_t));
		memcpy(found_servers, found_server, sizeof(struct found_server_t));
		(found_servers)->next = NULL;
		return;
	}
	
	if((found_servers)->ping > found_server->ping)
	{
		next = found_servers;
		found_servers = malloc(sizeof(struct found_server_t));
		memcpy(found_servers, found_server, sizeof(struct found_server_t));
		(found_servers)->next = next;
		
		if(selected_server != -1)
			server_down_in_lock();
		
		return;
	}
	
	
	cserver = found_servers;
	i = 1;
	
	while(cserver->next)
	{
		if(cserver->next->ping > found_server->ping)
		{
			next = cserver->next;
			cserver->next = malloc(sizeof(struct found_server_t));
			cserver = cserver->next;
			memcpy(cserver, found_server, sizeof(struct found_server_t));
			cserver->next = next;
			
			if(selected_server != -1 && i <= selected_server)
				server_down_in_lock();
			
			return;
		}

		LL_NEXT(cserver);
		i++;
	}


	cserver->next = malloc(sizeof(struct found_server_t));
	cserver = cserver->next;
	memcpy(cserver, found_server, sizeof(struct found_server_t));
	cserver->next = NULL;
}


void process_server_info(struct sockaddr_in *sockaddr, struct buffer_t *buffer)
{
	struct queried_server_t *cserver = queried_servers;
	struct found_server_t new_server_info;
	uint16_t servers;
	struct sockaddr_in s;
	time_t t;
	int proto_ver;
		
	while(cserver)
	{
		if(cserver->ip == sockaddr->sin_addr.s_addr && 
			cserver->port == sockaddr->sin_port)
		{
			proto_ver = buffer_read_uint8(buffer);
			if(proto_ver != EM_PROTO_VER)
			{
				if(proto_ver > EM_PROTO_VER)
					num_servers_new_proto++;
				
				break;
			}
			
			new_server_info.ip = sockaddr->sin_addr.s_addr;
			new_server_info.port = sockaddr->sin_port;
			new_server_info.ping = get_wall_time() - cserver->stamp;

			new_server_info.host_name = buffer_read_string(buffer);
			new_server_info.map_name = buffer_read_string(buffer);
			new_server_info.num_players = buffer_read_uint8(buffer);
			new_server_info.max_players = buffer_read_uint8(buffer);
			new_server_info.authenticating = buffer_read_uint8(buffer);
		
			add_new_found_server(&new_server_info);
				
			
			add_new_server(&rumoured_servers, sockaddr, time(NULL));
				
			
			servers = buffer_read_uint16(buffer);
			
			while(servers--)
			{
				s.sin_addr.s_addr = buffer_read_uint32(buffer);
				s.sin_port = buffer_read_uint16(buffer);
				t = buffer_read_uint32(buffer);
				
				if(!find_queried_server(s.sin_addr.s_addr, s.sin_port))
				{
					if(add_new_server(&unqueried_servers, &s, t))
						num_servers_unqueried++;
				}
			}
			
			break;
		}
			
		LL_NEXT(cserver);
	}			
}


void *servers_thread(void *a)
{
	struct pollfd *fds;
	int fdcount;
	
	struct sockaddr_in sockaddr;
	struct buffer_t *buffer;
	
	fdcount = 2;
	
	fds = calloc(sizeof(struct pollfd), fdcount);
	
	fds[0].fd = servers_kill_pipe[0];	fds[0].events = POLLIN;
	fds[1].fd = servers_info_pipe[0];	fds[1].events = POLLIN;
	

	while(1)
	{
		if(TEMP_FAILURE_RETRY(poll(fds, fdcount, -1)) == -1)
		{
			free(fds);
			pthread_exit(NULL);
		}

		if(fds[0].revents & POLLIN)
		{
			free(fds);
			pthread_exit(NULL);
		}
		
		if(fds[1].revents & POLLIN)
		{
			TEMP_FAILURE_RETRY(read(servers_info_pipe[0], &sockaddr, sizeof(struct sockaddr_in)));
			TEMP_FAILURE_RETRY(read(servers_info_pipe[0], &buffer, 4));
			pthread_mutex_lock(&servers_mutex);
			process_server_info(&sockaddr, buffer);
			pthread_mutex_unlock(&servers_mutex);
			free_buffer(buffer);
		}
	}
}


void emit_servers(uint32_t conn)
{
	net_emit_uint8(conn, EMMSG_SERVERS);
	
	int servers = min(45, count_servers());
	
	net_emit_uint32(conn, servers);
	
	struct server_t *cserver = rumoured_servers;

	while(servers--)
	{
		net_emit_uint32(conn, cserver->ip);
		net_emit_uint16(conn, cserver->port);
		net_emit_uint32(conn, cserver->time);
		
		LL_NEXT(cserver);
	}
	
	net_emit_end_of_stream(conn);
}


void start_server_discovery()
{
	if(server_discovery_started)
		return;
	
	selected_server = -1;
	top_server_visible = 0;
	
	num_servers_unqueried = 0;
	num_servers_queried = 0;
	num_servers_found = 0;
	num_servers_new_proto = 0;
	
	struct server_t *cserver = rumoured_servers;
	
	while(cserver)
	{
		num_servers_unqueried++;
		LL_ADD_TAIL(struct server_t, &unqueried_servers, cserver);
		LL_NEXT(cserver);
	}
	
	pipe(servers_info_pipe);

	output_server_query();
	servers_first_output = get_wall_time();
	servers_next_output = 1.0 / SERVER_QUERY_OUTPUT_RATE + servers_first_output;

	pipe(servers_kill_pipe);
	pthread_create(&servers_thread_id, NULL, servers_thread, NULL);

	server_discovery_started = 1;
}


void stop_server_discovery()
{
	if(!server_discovery_started)
		return;
	
	uint8_t c;
	TEMP_FAILURE_RETRY(write(servers_kill_pipe[1], &c, 1));
	pthread_join(servers_thread_id, NULL);

	close(servers_kill_pipe[0]);
	servers_kill_pipe[0] = -1;
	close(servers_kill_pipe[1]);
	servers_kill_pipe[1] = -1;

	close(servers_info_pipe[0]);
	servers_info_pipe[0] = -1;
	close(servers_info_pipe[1]);
	servers_info_pipe[1] = -1;
	
	LL_REMOVE_ALL(struct server_t, &unqueried_servers);
	LL_REMOVE_ALL(struct queried_server_t, &queried_servers);
	LL_REMOVE_ALL(struct found_server_t, &found_servers);

	server_discovery_started = 0;
	
	save_rumoured_servers();
}


void render_servers()
{
	if(r_DrawConsole)
		return;
	
	if(server_discovery_started)
	{
		pthread_mutex_lock(&servers_mutex);
		
		blit_text(0, vid_height * 5 / 6, 0xff, 0xff, 0xff, 
			s_backbuffer, "Servers unqueried:");
	
		blit_text(150, vid_height * 5 / 6, 0xff, 0xff, 0xff, 
			s_backbuffer, "%u", num_servers_unqueried);

		blit_text(0, vid_height * 5 / 6 + 14, 0xff, 0xff, 0xff, 
			s_backbuffer, "Servers queried:");
		
		blit_text(150, vid_height * 5 / 6 + 14, 0xff, 0xff, 0xff, 
			s_backbuffer, "%u", num_servers_queried);
		
		blit_text(0, vid_height * 5 / 6 + 28, 0xff, 0xff, 0xff, 
			s_backbuffer, "Servers found:");
		
		blit_text(150, vid_height * 5 / 6 + 28, 0xff, 0xff, 0xff, 
			s_backbuffer, "%u", num_servers_found);

		if(num_servers_new_proto)
		{
			blit_text(0, vid_height * 5 / 6 + 42, 0xff, 0xff, 0xff, 
				s_backbuffer, "Servers found requiring new version:");
		
			blit_text(275, vid_height * 5 / 6 + 42, 0xff, 0xff, 0xff, 
				s_backbuffer, "%u", num_servers_new_proto);
		}

		
		blit_text(50, vid_height / 6, 0xff, 0xff, 0xff, 
			s_backbuffer, "Server");

		blit_text(50 + 264, vid_height / 6, 0xff, 0xff, 0xff, 
			s_backbuffer, "Port");

		blit_text(50 + 264 + 64, vid_height / 6, 0xff, 0xff, 0xff, 
			s_backbuffer, "Ping");

		blit_text(50 + 264 + 64 + 64, vid_height / 6, 0xff, 0xff, 0xff, 
			s_backbuffer, "Key");
			
		blit_text(50 + 264 + 64 + 64, vid_height / 6 + 16, 0xff, 0xff, 0xff, 
			s_backbuffer, "Required?");
			

		blit_text(50 + 32, vid_height / 6 + 14, 0xff, 0xff, 0xff, 
			s_backbuffer, "Map");
		
		blit_text(50 + 32 + 264, vid_height / 6 + 14, 0xff, 0xff, 0xff, 
			s_backbuffer, "Players");

		struct found_server_t *cserver = found_servers;
		int i = 1;
		int server = 0;
			
		while(cserver && i <= 8)
		{
			if(server >= top_server_visible)
			{
				blit_text(50, vid_height / 6 + i * 32, 0xef, 0x6f, 0xff, 
					s_backbuffer, "%s", cserver->host_name->text);
	
				blit_text(50 + 264, vid_height / 6 + i * 32, 0xef, 0x6f, 0xff, 
					s_backbuffer, "%u", ntohs(cserver->port));
	
				blit_text(50 + 264 + 64, vid_height / 6 + i * 32, 0xef, 0x6f, 0xff, 
					s_backbuffer, "%.2fms", cserver->ping * 1000.0);
	
				if(cserver->authenticating)
				{
					blit_text_centered(50 + 264 + 64 + 64 + 12, vid_height / 6 + i * 32 + 6, 
						0xef, 0x6f, 0xff, s_backbuffer, "Y");
				}
	
				blit_text(50 + 32, vid_height / 6 + i * 32 + 14, 0xef, 0x6f, 0xff, 
					s_backbuffer, "%s", cserver->map_name->text);
				
				blit_text(50 + 32 + 264, vid_height / 6 + i * 32 + 14, 0xef, 0x6f, 0xff, 
					s_backbuffer, "%u/%u", cserver->num_players, cserver->max_players);
				
				if(server == selected_server)
				{
					struct blit_params_t params;
						
					params.red = 0x54;
					params.green = 0xa6;
					params.blue = 0xf9;
					params.alpha = 0x7f;
					
					params.dest = s_backbuffer;
					params.dest_x = 48;
					params.dest_y = vid_height / 6 + i * 32 - 2;
					params.width = 475;
					params.height = 32;
				
					alpha_draw_rect(&params);
				}
				
				i++;
			}
			
			LL_NEXT(cserver);
			server++;
		}
		
		pthread_mutex_unlock(&servers_mutex);
	}
}


void init_servers()
{
	load_rumoured_servers();
	create_alarm_listener(servers_alarm);
}


void kill_servers()
{
	if(server_discovery_started)
		stop_server_discovery();
	
	save_rumoured_servers();
}
