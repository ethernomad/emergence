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
#include <sys/types.h>
#include <sys/stat.h>

#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <unistd.h>
#include <pthread.h>
#include <errno.h>
#include <string.h>

#include "alarm.h"
#include "servers.h"
#include "network.h"
#include "user.h"
#include "stringbuf.h"
#include "llist.h"
#include "resource.h"


struct server_t *rumoured_servers = NULL;


void load_rumoured_servers()
{
	struct string_t *filename = new_string_string(emergence_home_dir);
	
	#ifdef EMCLIENT
	string_cat_text(filename, "/rumoured.client");
	#endif
	
	#ifdef EMSERVER
	string_cat_text(filename, "/rumoured.server.");
	string_cat_int(filename, net_listen_port);
	#endif
	
	FILE *file = fopen(filename->text, "r");
	free_string(filename);
	
	if(!file)
		return;

//	flockfile(file);
	
	char *name = NULL;
	uint16_t port;
	struct server_t server;
	
	while(fscanf(file, "%a[^:]:%hu %u\n", &name, &port, (uint32_t*)&server.time) == 3)
	{
		int r = inet_pton(AF_INET, name, &server.ip);
		
		free(name);
		name = NULL;
		
		if(r <= 0)
			continue;
		
		server.port = htons(port);
		
		LL_ADD_TAIL(struct server_t, &rumoured_servers, &server);
	}
	
	if(name)
	{
		free(name);
		name = NULL;
	}
	
	fclose(file);
}


void save_rumoured_servers()
{
	struct string_t *filename = new_string_string(emergence_home_dir);

	#ifdef EMCLIENT
	string_cat_text(filename, "/rumoured.client");
	#endif
	
	#ifdef EMSERVER
	string_cat_text(filename, "/rumoured.server.");
	string_cat_int(filename, net_listen_port);
	#endif
	
	FILE *file = fopen(filename->text, "w");
	free_string(filename);
	
	if(!file)
		return;
	
	struct server_t *server = rumoured_servers;
	
	while(server)
	{
		char addr[17];
		if(inet_ntop(AF_INET, &server->ip, addr, 17))
			fprintf(file, "%s:%hu %u\n", addr, ntohs(server->port), (uint32_t)server->time);
		
		LL_NEXT(server);
	}

	fclose(file);
}


int add_new_server(struct server_t **server0, struct sockaddr_in *sockaddr, time_t t)
{
	int found = 0;
	
	// see if this server is already in the list
	
	struct server_t *cserver = *server0, *temp, *next;
		
	while(cserver)
	{
		if(cserver->ip == sockaddr->sin_addr.s_addr &&
			cserver->port == sockaddr->sin_port)
		{
			next = cserver->next;
			LL_REMOVE(struct server_t, server0, cserver);
			cserver = next;
			found = 1;
			continue;
		}
		
		LL_NEXT(cserver);
	}
	
	struct server_t server = {
		sockaddr->sin_addr.s_addr, 
		sockaddr->sin_port, 
		t
	};
	


	// if rumoured_servers is NULL, then create new server here

	if(!*server0)
	{
		*server0 = malloc(sizeof(struct server_t));
		memcpy(*server0, &server, sizeof(struct server_t));
		(*server0)->next = NULL;
		goto end;
	}
	
	if((*server0)->time < t)
	{
		temp = *server0;
		*server0 = malloc(sizeof(struct server_t));
		memcpy(*server0, &server, sizeof(struct server_t));
		(*server0)->next = temp;
		goto end;
	}
	
	
	cserver = *server0;
	
	while(cserver->next)
	{
		if(cserver->next->time < t)
		{
			temp = cserver->next;
			cserver->next = malloc(sizeof(struct server_t));
			cserver = cserver->next;
			memcpy(cserver, &server, sizeof(struct server_t));
			cserver->next = temp;
			goto end;
		}

		LL_NEXT(cserver);
	}


	cserver->next = malloc(sizeof(struct server_t));
	cserver = cserver->next;
	memcpy(cserver, &server, sizeof(struct server_t));
	cserver->next = NULL;

	
	end:
	;
	
	return !found;
}


int count_servers()
{
	int c = 0;
	
	struct server_t *cserver = rumoured_servers;
		
	while(cserver)
	{
		c++;
		
		LL_NEXT(cserver);
	}
	
	return c;
}
