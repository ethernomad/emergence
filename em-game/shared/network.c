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
#include <unistd.h>
#include <netdb.h>
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <string.h>
#include <math.h>
#include <assert.h>
#include <pthread.h>

#include <sys/socket.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/epoll.h>
#include <sys/poll.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include "types.h"
#include "minmax.h"
#include "llist.h"
#include "stringbuf.h"
#include "buffer.h"
#include "cvar.h"
#include "timer.h"
#include "alarm.h"
#include "network.h"

#include "console.h"
#include "main.h"

#ifdef EMSERVER
#include "game.h"
#include "entry.h"
#endif

#ifdef EMCLIENT
#include "servers.h"
extern int servers_info_pipe[2];
#endif

struct in_packet_ll_t
{
	struct packet_t packet;
	int payload_size;
	int stream_delivered;		// set on the first packet of a stream once that 
								// stream has been delivered ooo
		
	struct in_packet_ll_t *next;
};


struct out_packet_ll_t
{
	struct packet_t packet;
	float next_resend_time;
	int size;
		
	struct out_packet_ll_t *next;
};


struct conn_cookie_t
{
	float expire_time;
	uint32_t cookie;
	uint32_t ip;
	uint16_t port;
	
	struct conn_cookie_t *next;
		
} *conn_cookie0 = NULL;


struct conn_t
{
	int state;
	
	#ifdef EMSERVER
	int begun;
	int type;
	#endif

	struct sockaddr_in sockaddr;	// address of the other end

	uint32_t next_read_index;		// index of the next packet to be read
	uint32_t next_process_index;	// index of the next packet to be processed
	uint32_t next_write_index;		// index of the next packet to be sent
	
	struct in_packet_ll_t *in_packet0;		// unprocessed received packets in index order
	struct out_packet_ll_t *out_packet0;	// unacknowledged sent packets in index order
	
	float last_time;	// the time that the last ack was received or 
						// first packet in out_packet was sent if it was empty or
						// the time the connection was disconnected
	
	struct packet_t cpacket;	// the packet currently being constructed
	int cpacket_payload_size;
	
	struct conn_t *next;

} *conn0 = NULL;


#define NETSTATE_CONNECTING		0	// we are trying to connect from this end (client only)
#define NETSTATE_CONNECTED		1
#define NETSTATE_DISCONNECTING	2	// the connection has been terminated this end
#define NETSTATE_DISCONNECTED	3	// we keep the conn info for a while so we can resend acks

#define OUT_OF_ORDER_RECV_AHEAD		500
#define CONNECTION_COOKIE_TIMEOUT	4.0
#define CONNECTION_TIMEOUT			5.0
#define CONNECTION_MAX_OUT_PACKETS	400
#define CONNECT_RESEND_DELAY		0.5
#define STREAM_RESEND_DELAY			0.1

pthread_t network_thread_id;
pthread_mutex_t net_mutex = PTHREAD_RECURSIVE_MUTEX_INITIALIZER_NP;

int udp_fd = -1;

int net_kill_pipe[2];
int net_out_pipe[2];

int net_shutting_down = 0;

#ifdef EMSERVER
uint16_t net_listen_port = EMNET_DEFAULTPORT;
#endif

char *hostname = NULL;


struct conn_t *new_conn()
{
	struct conn_t *next = conn0;
	
	conn0 = calloc(1, sizeof(struct conn_t));
	conn0->next = next;

	return conn0;
}


void delete_conn(struct conn_t *conn)
{
	LL_REMOVE_ALL(struct in_packet_ll_t, &conn->in_packet0);
	LL_REMOVE_ALL(struct out_packet_ll_t, &conn->out_packet0);
	LL_REMOVE(struct conn_t, &conn0, conn);
}


void delete_all_conns()
{
	while(conn0)
		delete_conn(conn0);
}


struct conn_t *get_conn(struct sockaddr_in *sockaddr)
{
	struct conn_t *conn = conn0;

	while(conn)
	{
		if(conn->sockaddr.sin_addr.s_addr == sockaddr->sin_addr.s_addr &&
			conn->sockaddr.sin_port == sockaddr->sin_port)
			return conn;
		
		conn = conn->next;
	}

	return NULL;
}


int conn_exist(struct conn_t *conn)
{
	struct conn_t *cconn = conn0;

	while(cconn)
	{
		if(cconn == conn)
			return 1;
		
		cconn = cconn->next;
	}
	
	return 0;
}


int insert_in_packet(struct conn_t *conn, struct packet_t *packet, int size)
{
	struct in_packet_ll_t *temp;
	struct in_packet_ll_t *cpacket;
	uint32_t index;
	
	
	// if packet0 is NULL, then create new packet here

	if(!conn->in_packet0)
	{
		conn->in_packet0 = calloc(1, sizeof(struct in_packet_ll_t));
		memcpy(conn->in_packet0, packet, size);
		conn->in_packet0->payload_size = size - EMNETHEADER_SIZE;
		return 1;
	}

	
	index = conn->in_packet0->packet.header & EMNETINDEX_MASK;
	
	if(index == (packet->header & EMNETINDEX_MASK))
		return 0;		// the packet is already present in this list
	
	if(index < conn->next_process_index)
		index += EMNETINDEX_MAX + 1;
	
	if(index > (packet->header & EMNETINDEX_MASK))
	{
		temp = conn->in_packet0;
		conn->in_packet0 = calloc(1, sizeof(struct in_packet_ll_t));
		memcpy(conn->in_packet0, packet, size);
		conn->in_packet0->payload_size = size - EMNETHEADER_SIZE;
		conn->in_packet0->next = temp;
		return 1;
	}

	
	// search through the rest of the list
	// to find the packet before the first
	// packet to have an index greater
	// than index


	cpacket = conn->in_packet0;
	
	while(cpacket->next)
	{
		uint32_t index = cpacket->next->packet.header & EMNETINDEX_MASK;
		
		if(index == (packet->header & EMNETINDEX_MASK))
			return 0;		// the packet is already present in this list

		if(index < conn->next_process_index)
			index += EMNETINDEX_MAX + 1;
		
		if(index > (packet->header & EMNETINDEX_MASK))
		{
			temp = cpacket->next;
			cpacket->next = calloc(1, sizeof(struct in_packet_ll_t));
			cpacket = cpacket->next;
			memcpy(cpacket, packet, size);
			cpacket->payload_size = size - EMNETHEADER_SIZE;
			cpacket->next = temp;
			return 1;
		}

		LL_NEXT(cpacket);
	}

	
	// we have reached the end of the list and not found
	// a packet that has an index greater than or equal to packet->index

	cpacket->next = calloc(1, sizeof(struct in_packet_ll_t));
	cpacket = cpacket->next;
	memcpy(cpacket, packet, size);
	cpacket->payload_size = size - EMNETHEADER_SIZE;
	return 1;
}


int output_cpacket(struct conn_t *conn)
{
	// count packets
	
	struct out_packet_ll_t **packet = &conn->out_packet0;
	int num_packets = 0;

	while(*packet)
	{
		++num_packets;
		packet = &(*packet)->next;
	}
	
	if(num_packets >= CONNECTION_MAX_OUT_PACKETS)
		return 0;


	// send packet
	
	int size = conn->cpacket_payload_size + EMNETHEADER_SIZE;
	
	TEMP_FAILURE_RETRY(sendto(udp_fd, (char*)&conn->cpacket, size, 0, 
		&conn->sockaddr, sizeof(struct sockaddr_in)));
			

	// get time
		
	float time = get_wall_time();
	
	if(num_packets == 0)
		conn->last_time = time;

	
	// create new packet at end of conn->out_packet0
	
	switch(conn->cpacket.header & EMNETCLASS_MASK)
	{
	case EMNETCLASS_CONTROL:
		switch(conn->cpacket.header & EMNETFLAG_MASK)
		{
		case EMNETFLAG_CONNECT:
			time += CONNECT_RESEND_DELAY;
			break;
		
		case EMNETFLAG_DISCONNECT:
			time += STREAM_RESEND_DELAY;
			break;
		}
		
		break;
			
	case EMNETCLASS_STREAM:
		time += STREAM_RESEND_DELAY;
		conn->cpacket.header |= EMNETFLAG_STREAMRESNT;
		break;
	}

	*packet = malloc(sizeof(struct out_packet_ll_t));
	memcpy(*packet, &conn->cpacket, size);
	(*packet)->next_resend_time = time;
	(*packet)->size = size;
	(*packet)->next = NULL;
	return 1;
}


#ifdef EMSERVER
void send_conn_cookie(struct sockaddr_in *sockaddr)
{
	struct conn_cookie_t *cookie = malloc(sizeof(struct conn_cookie_t));
	cookie->cookie = mrand48() & EMNETCOOKIE_MASK;
	
	uint32_t header = EMNETFLAG_COOKIE | cookie->cookie;
	TEMP_FAILURE_RETRY(sendto(udp_fd, (char*)&header, EMNETHEADER_SIZE, 0, 
		sockaddr, sizeof(struct sockaddr_in)));
	
	cookie->expire_time = get_wall_time() + CONNECTION_COOKIE_TIMEOUT;
	cookie->ip = sockaddr->sin_addr.s_addr;
	cookie->port = sockaddr->sin_port;
	cookie->next = conn_cookie0;
	conn_cookie0 = cookie;
}


int query_conn_cookie(struct sockaddr_in *sockaddr, uint32_t cookie)
{
	struct conn_cookie_t *ccookie = conn_cookie0;
		
	while(ccookie)
	{
		if(ccookie->ip == sockaddr->sin_addr.s_addr && 
			ccookie->port == sockaddr->sin_port && 
			ccookie->cookie == cookie)
		{
			LL_REMOVE(struct conn_cookie_t, &conn_cookie0, ccookie);	// optimize me away?
			return 1;
		}
		
		ccookie = ccookie->next;
	}
	
	return 0;
}
#endif // EMSERVER


void send_connect(struct sockaddr_in *addr, uint32_t index)
{
	uint32_t header = EMNETFLAG_CONNECT | index;
	TEMP_FAILURE_RETRY(sendto(udp_fd, (char*)&header, EMNETHEADER_SIZE, 0, 
		addr, sizeof(struct sockaddr_in)));
}


void send_ack(struct sockaddr_in *addr, uint32_t index)
{
	uint32_t header = EMNETFLAG_ACKWLDGEMNT | index;
	TEMP_FAILURE_RETRY(sendto(udp_fd, (char*)&header, EMNETHEADER_SIZE, 0, 
		addr, sizeof(struct sockaddr_in)));
}


void process_ack(struct conn_t *conn, uint32_t index)
{
	struct out_packet_ll_t *temp;

	if(!conn->out_packet0)
		return;

	if((conn->out_packet0->packet.header & EMNETINDEX_MASK) 
		== index)	// the packet is the first on the list
	{
		temp = conn->out_packet0->next;
		free(conn->out_packet0);
		conn->out_packet0 = temp;
	}
	else
	{
		// search the rest of the list

		struct out_packet_ll_t *packet = conn->out_packet0;

		while(packet->next)
		{
			if((packet->next->packet.header & EMNETINDEX_MASK) == index)
			{
				temp = packet->next->next;
				free(packet->next);
				packet->next = temp;
				break;
			}

			packet = packet->next;
		}
	}


	if(conn->state == NETSTATE_DISCONNECTING && !conn->out_packet0)
	{
		delete_conn(conn);
		return;
	}
	
	// record the time for timeout purposes
	
	conn->last_time = get_wall_time();
}


void emit_process_connecting()
{
	uint32_t msg = NETMSG_CONNECTING;
	TEMP_FAILURE_RETRY(write(net_out_pipe[1], &msg, 4));
}


void emit_process_cookie_echoed()
{
	uint32_t msg = NETMSG_COOKIE_ECHOED;
	TEMP_FAILURE_RETRY(write(net_out_pipe[1], &msg, 4));
}


void emit_process_connection(struct conn_t *conn)
{
	uint32_t msg = NETMSG_CONNECTION;
	TEMP_FAILURE_RETRY(write(net_out_pipe[1], &msg, 4));
	TEMP_FAILURE_RETRY(write(net_out_pipe[1], &conn, 4));
	
	#ifdef EMSERVER
	TEMP_FAILURE_RETRY(write(net_out_pipe[1], &conn->type, 4));
	#endif
	
	#ifdef EMCLIENT
	TEMP_FAILURE_RETRY(write(net_out_pipe[1], &conn->sockaddr, sizeof(struct sockaddr_in)));
	#endif
}


void emit_process_connection_failed(struct conn_t *conn)
{
	uint32_t msg = NETMSG_CONNECTION_FAILED;
	TEMP_FAILURE_RETRY(write(net_out_pipe[1], &msg, 4));
	TEMP_FAILURE_RETRY(write(net_out_pipe[1], &conn, 4));
}


void emit_process_disconnection(struct conn_t *conn)
{
	uint32_t msg = NETMSG_DISCONNECTION;
	TEMP_FAILURE_RETRY(write(net_out_pipe[1], &msg, 4));
	TEMP_FAILURE_RETRY(write(net_out_pipe[1], &conn, 4));
}


void emit_process_conn_lost(struct conn_t *conn)
{
	uint32_t msg = NETMSG_CONNLOST;
	TEMP_FAILURE_RETRY(write(net_out_pipe[1], &msg, 4));
	TEMP_FAILURE_RETRY(write(net_out_pipe[1], &conn, 4));
}


void init_new_conn(struct conn_t *conn, uint32_t index)
{
	conn->next_read_index = conn->next_process_index = 
		conn->next_write_index = index;

	conn->cpacket.header = EMNETCLASS_STREAM | EMNETFLAG_STREAMFIRST | 
		conn->next_write_index++;
	conn->next_write_index %= EMNETINDEX_MAX + 1;
	
	
	#ifdef EMSERVER
	
	uint32_t addr = conn->sockaddr.sin_addr.s_addr;
	
	if((addr & 0xff) == 0x7f)			// 127.0.0.0 - 127.255.255.255  (127/8 prefix) (loopback)
		conn->type = CONN_TYPE_LOCAL;
	else if((addr & 0xff) == 0x0a ||	// 10.0.0.0 - 10.255.255.255  (10/8 prefix)
		(addr & 0xf0ff) == 0x10ac ||	// 172.16.0.0 - 172.31.255.255  (172.16/12 prefix)
		(addr & 0xffff) == 0xa8c0 ||	// 192.168.0.0 - 192.168.255.255 (192.168/16 prefix)
		(addr & 0xffff) == 0xfea9)		// 169.254.0.0 - 169.254.255.255 (169.254/16 prefix) (APIPA)
		conn->type = CONN_TYPE_PRIVATE;
	else
		conn->type = CONN_TYPE_PUBLIC;
	
	#endif
	
	
	emit_process_connection(conn);
}


void process_in_order_stream(struct conn_t *conn, struct in_packet_ll_t *end)
{
	struct buffer_t *buf = new_buffer();
	struct in_packet_ll_t *temp;
	int untimed = 0;
	
	if(conn->in_packet0->stream_delivered)
		untimed = 1;
	
	uint32_t index = conn->in_packet0->packet.header & EMNETINDEX_MASK;
	
	while(conn->in_packet0 != end)
	{
		if(conn->in_packet0->packet.header & EMNETFLAG_STREAMRESNT)
			untimed = 1;
			
		buffer_cat_buf(buf, (char*)&conn->in_packet0->packet.payload, 
			conn->in_packet0->payload_size);

		temp = conn->in_packet0;
		conn->in_packet0 = conn->in_packet0->next;
		free(temp);
	}

	if(conn->in_packet0->packet.header & EMNETFLAG_STREAMRESNT)
		untimed = 1;
	
	buffer_cat_buf(buf, (char*)&conn->in_packet0->packet.payload, conn->in_packet0->payload_size);

	temp = conn->in_packet0;
	conn->in_packet0 = conn->in_packet0->next;
	free(temp);

	uint32_t msg;
	uint64_t stamp;
	
	if(untimed)
	{
		msg = NETMSG_STREAM_UNTIMED;
		TEMP_FAILURE_RETRY(write(net_out_pipe[1], &msg, 4));
		TEMP_FAILURE_RETRY(write(net_out_pipe[1], &index, 4));
	}
	else
	{
		msg = NETMSG_STREAM_TIMED;
		stamp = timestamp();
		TEMP_FAILURE_RETRY(write(net_out_pipe[1], &msg, 4));
		TEMP_FAILURE_RETRY(write(net_out_pipe[1], &index, 4));
		TEMP_FAILURE_RETRY(write(net_out_pipe[1], &stamp, 8));
	}
	
	TEMP_FAILURE_RETRY(write(net_out_pipe[1], &conn, 4));
	TEMP_FAILURE_RETRY(write(net_out_pipe[1], &buf, 4));
}


void process_out_of_order_stream(struct conn_t *conn, struct in_packet_ll_t *start, 
	struct in_packet_ll_t *end)
{
	if(start->stream_delivered)
		return;
	
	struct in_packet_ll_t *cpacket = start;
	struct buffer_t *buf = new_buffer();
	int untimed = 0;
	
	while(cpacket != end)
	{
		if(cpacket->packet.header & EMNETFLAG_STREAMRESNT)
			untimed = 1;
			
		buffer_cat_buf(buf, (char*)&cpacket->packet.payload, cpacket->payload_size);
		
		cpacket = cpacket->next;
	}

	if(cpacket->packet.header & EMNETFLAG_STREAMRESNT)
		untimed = 1;
	
	buffer_cat_buf(buf, (char*)&cpacket->packet.payload, cpacket->payload_size);

	uint32_t msg;
	uint32_t index = start->packet.header & EMNETINDEX_MASK;
	uint64_t stamp;
	
	if(untimed)
	{
		msg = NETMSG_STREAM_UNTIMED_OOO;
		TEMP_FAILURE_RETRY(write(net_out_pipe[1], &msg, 4));
		TEMP_FAILURE_RETRY(write(net_out_pipe[1], &index, 4));
	}
	else
	{
		msg = NETMSG_STREAM_TIMED_OOO;
		stamp = timestamp();
		TEMP_FAILURE_RETRY(write(net_out_pipe[1], &msg, 4));
		TEMP_FAILURE_RETRY(write(net_out_pipe[1], &index, 4));
		TEMP_FAILURE_RETRY(write(net_out_pipe[1], &stamp, 8));
	}
	
	TEMP_FAILURE_RETRY(write(net_out_pipe[1], &conn, 4));
	TEMP_FAILURE_RETRY(write(net_out_pipe[1], &buf, 4));
	
	start->stream_delivered = 1;
}


void check_stream(struct conn_t *conn)
{
	struct in_packet_ll_t *cpacket = conn->in_packet0;
	uint32_t nindex = conn->next_process_index;

	check_first:
	
	if((cpacket->packet.header & EMNETINDEX_MASK) == nindex)
	{
		if((cpacket->packet.header & (EMNETCLASS_MASK | EMNETFLAG_MASK)) == EMNETFLAG_DISCONNECT)
		{
			conn->state = NETSTATE_DISCONNECTED;
			emit_process_disconnection(conn);
			return;
		}
		
		
		// packet is definately stream first
		
		while(1)
		{
			if((cpacket->packet.header & EMNETINDEX_MASK) == conn->next_read_index)
				conn->next_read_index = (conn->next_read_index + 1) % (EMNETINDEX_MAX + 1);
	
			if(cpacket->packet.header & EMNETFLAG_STREAMLAST)
			{
				process_in_order_stream(conn, cpacket);
				
				cpacket = conn->in_packet0;
				conn->next_process_index = ++nindex;
				
				if(!cpacket)
					return;
				
				goto check_first;		
			}

			cpacket = cpacket->next;

			if(!cpacket)
				return;
			
			nindex++;
			
			if((cpacket->packet.header & EMNETINDEX_MASK) != nindex)
				break;
		}
	}
	
	
	// deliver out-of-order streams
	
	struct in_packet_ll_t *start_packet = NULL;
	
	while(cpacket)
	{
		if((cpacket->packet.header & (EMNETCLASS_MASK | EMNETFLAG_MASK)) == EMNETFLAG_DISCONNECT)
			return;
		
		// packet is definately stream
		
		if(!start_packet)
		{
			check_first_ooo:

			if(cpacket->packet.header & EMNETFLAG_STREAMFIRST)
			{
				if(cpacket->packet.header & EMNETFLAG_STREAMLAST)
				{
					process_out_of_order_stream(conn, cpacket, cpacket);
				}
				else
				{
					start_packet = cpacket;
					nindex = (start_packet->packet.header & EMNETINDEX_MASK) + 1;
				}
			}
		}
		else
		{
			if((cpacket->packet.header & EMNETINDEX_MASK) != nindex)
			{
				start_packet = NULL;
				goto check_first_ooo;
			}
			
			if(cpacket->packet.header & EMNETFLAG_STREAMLAST)
			{
				process_out_of_order_stream(conn, start_packet, cpacket);
				start_packet = NULL;
			}
			else
			{
				nindex++;
			}
		}
		
		cpacket = cpacket->next;
	}
}


void emit_serverinfo(struct sockaddr_in *addr, uint8_t *info, int len)
{
	#ifdef EMSERVER
	uint32_t msg = NETMSG_SERVERINFO_REQUEST;
	TEMP_FAILURE_RETRY(write(net_out_pipe[1], &msg, 4));
	TEMP_FAILURE_RETRY(write(net_out_pipe[1], addr, sizeof(struct sockaddr_in)));
	#endif
	
	#ifdef EMCLIENT
	if(servers_info_pipe[1] != -1)
	{
		struct buffer_t *buffer = new_buffer();
		buffer_cat_buf(buffer, info, len);
		TEMP_FAILURE_RETRY(write(servers_info_pipe[1], addr, sizeof(struct sockaddr_in)));
		TEMP_FAILURE_RETRY(write(servers_info_pipe[1], &buffer, 4));
	}
	#endif
}


int all_connections_disconnected()
{
	struct conn_t *conn = conn0;

	while(conn)
	{
		if(conn->state != NETSTATE_DISCONNECTED)
			return 0;
		
		conn = conn->next;
	}

	return 1;
}


void process_udp_data()
{
	struct packet_t packet;
	socklen_t addr_size;
	struct sockaddr_in recv_addr;
	int size, stop;
	uint32_t index, rollover_index;
	struct conn_t *conn;
	
	while(1)
	{
		// receive packet and address
	
		addr_size = sizeof(struct sockaddr_in);
		size = TEMP_FAILURE_RETRY(recvfrom(udp_fd, (char*)&packet, EMNETPACKET_MAXSIZE, 0, 
			(struct sockaddr*)&recv_addr, &addr_size));
			
		if(size == -1)
			break;
		
		
	//	if(drand48() > 0.95)
	//		continue;

		
		// check packet has a header
		
		if(size < EMNETHEADER_SIZE)
			continue;
		
		index = packet.header & EMNETINDEX_MASK;
		
	
		// see if the packet is from an address where a connection is established
		
		conn = get_conn(&recv_addr);
		
		
		// process packet
		
		switch(packet.header & EMNETCLASS_MASK)
		{
		case EMNETCLASS_CONTROL:
			
			switch(packet.header & EMNETFLAG_MASK)
			{
			case EMNETFLAG_CONNECT:
				
				if(size != EMNETHEADER_SIZE)
					break;
				
				
				#ifdef EMCLIENT
				
				if(!conn)
					break;

				if(conn->state != NETSTATE_CONNECTING)
					break;
				
				
				// we know that the server knows we have successfully performed
				// a cookie exchange, so we start the connection

				conn->state = NETSTATE_CONNECTED;
				LL_REMOVE_ALL(struct out_packet_ll_t, &conn->out_packet0);
				init_new_conn(conn, index);
				
				#endif // EMCLIENT
				
				
				#ifdef EMSERVER
				
				if(index != EMNETCONNECT_MAGIC)
					break;
				
				if(conn)
				{
					// we have received a connect packet from the client, 
					// even though we understand the connection to have 
					// started
					
					if(!conn->begun)
					{
						// a) the client never received the connection ack, 
						//    so send it again
						
						// b) the cookie ack got duplicated en-route
						
						send_connect(&conn->sockaddr, conn->next_read_index);
						
						conn->last_time = get_wall_time();	// give the client a little helping
															// hand in troubled times
					}
					else
					{
						// c) the client did not disconnect gracefully or the disconnected state
						//    has not timed out yet and the client is now trying to reconnect 
						//    from the same ip:port
						
						// d) the initial connect packet from the client was
						//    duplicated on the net and arrived again after conn->begun
						//    was set (unlikely). the cookie will be ignored by the client
					
						send_conn_cookie(&recv_addr);
					}
				}
				else
				{
					send_conn_cookie(&recv_addr);
				}
				
				#endif
				
				
				break;
			
				
			case EMNETFLAG_COOKIE:
				
				if(size != EMNETHEADER_SIZE)
					break;
				
				
				#ifdef EMCLIENT
				
				if(!conn)
					break;
				
				if(conn->state != NETSTATE_CONNECTING)
					break;
				
				TEMP_FAILURE_RETRY(sendto(udp_fd, (char*)&packet, EMNETHEADER_SIZE, 0, 
					&recv_addr, sizeof(struct sockaddr_in)));
				
				emit_process_cookie_echoed();
					
				#endif // EMCLIENT
					
				
				#ifdef EMSERVER
			
				if(!query_conn_cookie(&recv_addr, index))
					break;
					
				send_connect(&recv_addr, index);
				
				if(conn)	// this occurs if condition c (above) has occured previously
				{
					if(conn->state == NETSTATE_CONNECTED)
						emit_process_disconnection(conn);
					
					delete_conn(conn);
				}

				conn = new_conn();
				conn->state = NETSTATE_CONNECTED;
				
				memcpy(&conn->sockaddr, &recv_addr, sizeof(struct sockaddr_in));
				init_new_conn(conn, index);
				
				#endif // EMSERVER
				
				break;
			
				
			case EMNETFLAG_DISCONNECT:
				
				if(size != EMNETHEADER_SIZE)
					break;
			
				if(!conn)
					break;
				
				#ifdef EMSERVER
				conn->begun = 1;
				#endif
				
				stop = 0;
				
				switch(conn->state)
				{
				#ifdef EMCLIENT
				case NETSTATE_CONNECTING:
					stop = 1;
					break;
				#endif
				
				case NETSTATE_CONNECTED:
					break;
	
				case NETSTATE_DISCONNECTING:
					send_ack(&recv_addr, index);
					conn->last_time = get_wall_time();
					conn->state = NETSTATE_DISCONNECTED;
					stop = 1;
					break;
				
				case NETSTATE_DISCONNECTED:
					send_ack(&recv_addr, index);
					stop = 1;
					break;
				}
				
				if(stop)
					break;
			
				if(index < conn->next_read_index)
					rollover_index = index + EMNETINDEX_MAX + 1;
				else
					rollover_index = index;
				
				if(rollover_index > conn->next_read_index + OUT_OF_ORDER_RECV_AHEAD)
					break;
				
				send_ack(&recv_addr, index);
				
				if(insert_in_packet(conn, &packet, size))
					check_stream(conn);		// see if this packet has completed a stream
		
				break;
			
				
			case EMNETFLAG_ACKWLDGEMNT:
				
				if(size != EMNETHEADER_SIZE)
					break;
			
				if(!conn)
					break;
				
				#ifdef EMSERVER
				conn->begun = 1;
				#endif
				
				process_ack(conn, index);
				
				break;
			}
			
			break;
	
			
		case EMNETCLASS_STREAM:
			
			if(size == EMNETHEADER_SIZE)
				break;	// the packet is invalid
			
			if(!conn)
				break;
			
			#ifdef EMSERVER
			conn->begun = 1;
			#endif
				
			stop = 0;
			
			switch(conn->state)
			{
			#ifdef EMCLIENT
			case NETSTATE_CONNECTING:
				stop = 1;
				break;
			#endif
			
			case NETSTATE_CONNECTED:
				break;

			case NETSTATE_DISCONNECTING:
			case NETSTATE_DISCONNECTED:
				send_ack(&recv_addr, index);
				stop = 1;
				break;
			}
			
			if(stop)
				break;
			
			if(index < conn->next_read_index)
				rollover_index = index + EMNETINDEX_MAX + 1;
			else
				rollover_index = index;
			
			if(rollover_index > conn->next_read_index + OUT_OF_ORDER_RECV_AHEAD)
				break;
			
			send_ack(&recv_addr, index);

			if(insert_in_packet(conn, &packet, size))
				check_stream(conn);		// see if this packet has completed a stream
				
			break;
			
			
		case EMNETCLASS_MISC:
			
			switch(packet.header & EMNETFLAG_MASK)
			{
			case EMNETFLAG_PING:
				
				if(size != EMNETHEADER_SIZE)
					break;
			
				packet.header = EMNETCLASS_MISC | EMNETFLAG_PONG | 
					(packet.header & EMNETCOOKIE_MASK);
				TEMP_FAILURE_RETRY(sendto(udp_fd, (char*)&packet, EMNETHEADER_SIZE, 0, 
					&recv_addr, sizeof(struct sockaddr_in)));
						
				break;
			
					
			case EMNETFLAG_PONG:
				
				if(size != EMNETHEADER_SIZE)
					break;
				
				break;
			
				
			case EMNETFLAG_SERVERINFO:
				emit_serverinfo(&recv_addr, packet.payload, size - EMNETHEADER_SIZE);
				break;
			}
			
			break;
		}
	}
	
	if(net_shutting_down)
	{
		if(all_connections_disconnected())
		{
			pthread_mutex_unlock(&net_mutex);
			pthread_exit(NULL);
		}
	}
}


struct string_t *get_text_addr(uint32_t conn)
{
	pthread_mutex_lock(&net_mutex);
	
	assert(conn);
	
	return new_string_text("%s:%u", inet_ntoa(((struct conn_t*)conn)->sockaddr.sin_addr), 
		((struct conn_t*)conn)->sockaddr.sin_port);
	
	pthread_mutex_unlock(&net_mutex);
}


void get_sockaddr_in_from_conn(uint32_t conn, struct sockaddr_in *sockaddr)
{
	pthread_mutex_lock(&net_mutex);
	
	assert(conn);

	memcpy(sockaddr, &((struct conn_t*)conn)->sockaddr, sizeof(struct sockaddr_in));
	
	pthread_mutex_unlock(&net_mutex);
}


void network_alarm()
{
	pthread_mutex_lock(&net_mutex);

	float time = get_wall_time();
	
	
	#ifdef EMSERVER
	
	// remove old cookies
	
	struct conn_cookie_t *cookie = conn_cookie0;
		
	while(cookie)
	{
		if(time > cookie->expire_time)
		{
			struct conn_cookie_t *temp = cookie->next;
			LL_REMOVE(struct conn_cookie_t, &conn_cookie0, cookie);		// optimize me away
			cookie = temp;
			continue;
		}
		
		cookie = cookie->next;
	}
	
	#endif // EMSERVER
	
	
	struct conn_t *conn = conn0;

	while(conn)
	{
		struct out_packet_ll_t *packet = conn->out_packet0;
		
		switch(conn->state)
		{
			
		#ifdef EMCLIENT
			
		case NETSTATE_CONNECTING:
			
			if(time - conn->last_time > CONNECTION_TIMEOUT)
			{
				emit_process_connection_failed(conn);
				
				struct conn_t *temp = conn->next;
				delete_conn(conn);
				conn = temp;
				continue;
			}
			
			if(time > packet->next_resend_time)
			{
				TEMP_FAILURE_RETRY(sendto(udp_fd, (char*)(packet), packet->size, 0, 
					(struct sockaddr*)&conn->sockaddr, sizeof(struct sockaddr_in)));
					
				packet->next_resend_time += (floor((time - packet->next_resend_time) / 
					CONNECT_RESEND_DELAY) + 1.0) * CONNECT_RESEND_DELAY;
					
				emit_process_connecting();
			}
			
			break;
			
		#endif // EMCLIENT
			
		
		case NETSTATE_CONNECTED:
			
			// check if conn has timed out
			
			if(packet && time - conn->last_time > CONNECTION_TIMEOUT)
			{
				emit_process_conn_lost(conn);
				
				struct conn_t *temp = conn->next;
				delete_conn(conn);
				conn = temp;
				continue;
			}
			
			
			// resend unacknowledged packets
	
			while(packet)
			{
				if(time > packet->next_resend_time)
				{
					TEMP_FAILURE_RETRY(sendto(udp_fd, (char*)(packet), packet->size, 0, 
						(struct sockaddr*)&conn->sockaddr, sizeof(struct sockaddr_in)));
						
					packet->next_resend_time += (floor((time - packet->next_resend_time) / 
						STREAM_RESEND_DELAY) + 1.0) * STREAM_RESEND_DELAY;
				}
	
				packet = packet->next;
			}
			
			break;
		
			
		case NETSTATE_DISCONNECTING:
			
			if(time - conn->last_time > CONNECTION_TIMEOUT)
			{
				struct conn_t *temp = conn->next;
				delete_conn(conn);
				conn = temp;
				continue;
			}
			
			// resend unacknowledged packets
	
			while(packet)
			{
				if(time > packet->next_resend_time)
				{
					TEMP_FAILURE_RETRY(sendto(udp_fd, (char*)(packet), packet->size, 0, 
						(struct sockaddr*)&conn->sockaddr, sizeof(struct sockaddr_in)));
						
					packet->next_resend_time += (floor((time - packet->next_resend_time) / 
						STREAM_RESEND_DELAY) + 1.0) * STREAM_RESEND_DELAY;
				}
	
				packet = packet->next;
			}
			
			break;
		
			
		case NETSTATE_DISCONNECTED:
			
			// check if conn has timed out
			
			if(time - conn->last_time > CONNECTION_TIMEOUT)
			{
				struct conn_t *temp = conn->next;
				delete_conn(conn);
				conn = temp;
				continue;
			}
		
			break;
		}

		conn = conn->next;
	}
	
	if(net_shutting_down)
	{
		if(all_connections_disconnected())
		{
			pthread_mutex_unlock(&net_mutex);
			pthread_exit(NULL);
		}
	}

	pthread_mutex_unlock(&net_mutex);
}


void *network_thread(void *a)
{
	struct pollfd *fds;
	int fdcount;
	
	fdcount = 2;
	
	fds = calloc(sizeof(struct pollfd), fdcount);
	
	fds[0].fd = udp_fd;				fds[0].events = POLLIN;
	fds[1].fd = net_kill_pipe[0];	fds[1].events = POLLIN;
	
	
	while(1)
	{
		if(TEMP_FAILURE_RETRY(poll(fds, fdcount, -1)) == -1)
		{
			free(fds);
			pthread_exit(NULL);
		}

		if(fds[0].revents & POLLIN)
		{			
			pthread_mutex_lock(&net_mutex);
			process_udp_data();
			pthread_mutex_unlock(&net_mutex);
		}
		
		if(fds[1].revents & POLLIN)
		{
			pthread_mutex_lock(&net_mutex);
			
			if(all_connections_disconnected())
			{
				free(fds);
				pthread_mutex_unlock(&net_mutex);
				pthread_exit(NULL);
			}
			
			pthread_mutex_unlock(&net_mutex);
		}
	}
}


/*
void *network_thread(void *a)
{
	int epoll_fd = epoll_create(3);
	
	struct epoll_event ev;
		
	ev.events = EPOLLIN | EPOLLET;
	ev.data.u32 = 0;
	epoll_ctl(epoll_fd, EPOLL_CTL_ADD, udp_fd, &ev);

	ev.events = EPOLLIN | EPOLLET;
	ev.data.u32 = 1;
	epoll_ctl(epoll_fd, EPOLL_CTL_ADD, net_timer_fd, &ev);

	ev.events = EPOLLIN | EPOLLET;
	ev.data.u32 = 2;
	epoll_ctl(epoll_fd, EPOLL_CTL_ADD, net_kill_pipe[0], &ev);

	while(1)
	{
		epoll_wait(epoll_fd, &ev, 1, -1);
		
		switch(ev.data.u32)
		{
		case 0:
			pthread_mutex_lock(&net_mutex);
			process_udp_data();
			pthread_mutex_unlock(&net_mutex);
			break;
		
		case 1:
			pthread_mutex_lock(&net_mutex);
			network_alarm();
			pthread_mutex_unlock(&net_mutex);
			break;
		
		case 2:
			if(all_connections_disconnected())
				pthread_exit(NULL);
		}
	}
}
*/


void send_cpacket(struct conn_t *conn)
{
	if(!output_cpacket(conn))
	{
		delete_conn(conn);
		emit_process_conn_lost(conn);
		return;
	}

	conn->cpacket.header = EMNETCLASS_STREAM | conn->next_write_index++;
	conn->next_write_index %= EMNETINDEX_MAX + 1;
}


void net_emit_uint32(uint32_t temp_conn, uint32_t val)
{
	struct conn_t *conn = (struct conn_t*)temp_conn;
	assert(conn);
	
	
	pthread_mutex_lock(&net_mutex);
	
	// make sure conn was not deleted before mutex acquired
	
	if(!conn_exist(conn))
		goto end;
	
	if(conn->state != NETSTATE_CONNECTED)
		goto end;
	
	switch(conn->cpacket_payload_size)
	{
	case EMNETPAYLOAD_MAXSIZE:

		send_cpacket(conn);

		*(uint32_t*)&conn->cpacket.payload[0] = val;
		conn->cpacket_payload_size = 4;

		break;


	case EMNETPAYLOAD_MAXSIZE - 1:

		conn->cpacket.payload[conn->cpacket_payload_size] = ((uint8_t*)&val)[0];
		conn->cpacket_payload_size = EMNETPAYLOAD_MAXSIZE;
		send_cpacket(conn);

		*(uint16_t*)&conn->cpacket.payload[0] = *(uint16_t*)&((uint8_t*)&val)[1];
		conn->cpacket.payload[2] = ((uint8_t*)&val)[3];
		conn->cpacket_payload_size = 3;

		break;


	case EMNETPAYLOAD_MAXSIZE - 2:

		*(uint16_t*)&conn->cpacket.payload[conn->cpacket_payload_size] = 
			((uint16_t*)(void*)&val)[0];
		conn->cpacket_payload_size = EMNETPAYLOAD_MAXSIZE;
		send_cpacket(conn);

		*(uint16_t*)&conn->cpacket.payload[0] = ((uint16_t*)(void*)&val)[1];
		conn->cpacket_payload_size = 2;

		break;


	case EMNETPAYLOAD_MAXSIZE - 3:

		*(uint16_t*)&conn->cpacket.payload[conn->cpacket_payload_size] = 
			((uint16_t*)(void*)&val)[0];
		conn->cpacket.payload[conn->cpacket_payload_size + 2] = ((uint8_t*)&val)[2];
		conn->cpacket_payload_size = EMNETPAYLOAD_MAXSIZE;
		send_cpacket(conn);

		conn->cpacket.payload[0] = ((uint8_t*)&val)[3];
		conn->cpacket_payload_size = 1;

		break;


	default:

		*(uint32_t*)&conn->cpacket.payload[conn->cpacket_payload_size] = val;
		conn->cpacket_payload_size += 4;

		break;
	}
	
	end:
	pthread_mutex_unlock(&net_mutex);
}


void net_emit_int(uint32_t temp_conn, int val)
{
	net_emit_uint32(temp_conn, *(uint32_t*)&val);
}


void net_emit_float(uint32_t temp_conn, float val)
{
	net_emit_uint32(temp_conn, *(uint32_t*)(void*)&val);
}


void net_emit_uint16(uint32_t temp_conn, uint16_t val)
{
	struct conn_t *conn = (struct conn_t*)temp_conn;
	assert(conn);
	
	
	pthread_mutex_lock(&net_mutex);
	
	// make sure conn was not deleted before mutex acquired
	
	if(!conn_exist(conn))
		goto end;
	
	if(conn->state != NETSTATE_CONNECTED)
		goto end;
	
	switch(conn->cpacket_payload_size)
	{
	case EMNETPAYLOAD_MAXSIZE:

		send_cpacket(conn);

		*(uint16_t*)&conn->cpacket.payload[0] = val;
		conn->cpacket_payload_size = 2;

		break;


	case EMNETPAYLOAD_MAXSIZE - 1:

		conn->cpacket.payload[conn->cpacket_payload_size] = ((uint8_t*)&val)[0];
		conn->cpacket_payload_size = EMNETPAYLOAD_MAXSIZE;
		send_cpacket(conn);

		conn->cpacket.payload[0] = ((uint8_t*)&val)[1];
		conn->cpacket_payload_size = 1;

		break;


	default:

		*(uint16_t*)&conn->cpacket.payload[conn->cpacket_payload_size] = val;
		conn->cpacket_payload_size += 2;

		break;
	}
	
	end:
	pthread_mutex_unlock(&net_mutex);
}


void net_emit_uint8(uint32_t temp_conn, uint8_t val)
{
	struct conn_t *conn = (struct conn_t*)temp_conn;
	assert(conn);
	
	
	pthread_mutex_lock(&net_mutex);
	
	// make sure conn was not deleted before mutex acquired
	
	if(!conn_exist(conn))
		goto end;
	
	if(conn->state != NETSTATE_CONNECTED)
		goto end;
	
	switch(conn->cpacket_payload_size)
	{
	case EMNETPAYLOAD_MAXSIZE:
		send_cpacket(conn);
		conn->cpacket.payload[0] = val;
		conn->cpacket_payload_size = 1;
		break;

	default:
		conn->cpacket.payload[conn->cpacket_payload_size] = val;
		conn->cpacket_payload_size++;
		break;
	}
	
	end:
	pthread_mutex_unlock(&net_mutex);
}


void net_emit_char(uint32_t temp_conn, char c)
{
	net_emit_uint8(temp_conn, *(uint8_t*)&c);
}


void net_emit_string(uint32_t temp_conn, char *cc)		// optimize me
{
	if(!cc)
		return;

	while(*cc)
		net_emit_char(temp_conn, *cc++);	// because this is really shit

	net_emit_char(temp_conn, '\0');
}


void net_emit_buf(uint32_t temp_conn, void *buf, int size)
{
	int p = 0;
	
	if(!buf)
		return;
	
	if(!size)
		return;
	
	while(size--)
		net_emit_uint8(temp_conn, ((uint8_t*)buf)[p++]);
}


void net_emit_end_of_stream(uint32_t temp_conn)
{
	struct conn_t *conn = (struct conn_t*)temp_conn;
	assert(conn);

	
	pthread_mutex_lock(&net_mutex);
	
	// make sure conn was not deleted before mutex acquired
	
	if(!conn_exist(conn))
		goto end;
	
	if(conn->state != NETSTATE_CONNECTED)
		goto end;
	
	if(conn->cpacket_payload_size == 0)
		goto end;
	
	conn->cpacket.header |= EMNETFLAG_STREAMLAST;
	send_cpacket(conn);
	conn->cpacket.header |= EMNETFLAG_STREAMFIRST;
	conn->cpacket_payload_size = 0;
	
	end:
	pthread_mutex_unlock(&net_mutex);
}


void net_emit_server_info(struct sockaddr_in *addr, uint8_t *buf, int size)
{
	struct packet_t packet = {
		EMNETCLASS_MISC | EMNETFLAG_SERVERINFO
	};
	
	if(buf)
	{
		size = min(EMNETPAYLOAD_MAXSIZE, size);
		
		memcpy(&packet.payload, buf, size);
	
		size += EMNETHEADER_SIZE;
	}
	else
	{
		size = EMNETHEADER_SIZE;
	}
	
	TEMP_FAILURE_RETRY(sendto(udp_fd, (char*)&packet, size, 0, 
		addr, sizeof(struct sockaddr_in)));
}


#ifdef EMCLIENT
void em_connect(uint32_t ip, uint16_t port)
{
	pthread_mutex_lock(&net_mutex);
	
	// TODO : check we dont already have a connection to this ip:port, if we do then 
	// enter a new state to disconnect gracefully from the server and then reconnect
	
	struct conn_t *conn = new_conn();
	conn->state = NETSTATE_CONNECTING;

	conn->sockaddr.sin_family = AF_INET;
	conn->sockaddr.sin_port = port;
	conn->sockaddr.sin_addr.s_addr = ip;

	console_print("Connecting to ");
	console_print(inet_ntoa(conn->sockaddr.sin_addr));
	console_print("...\n");

	
	// send connect packet to server and keep sending it until it acknowledges

	conn->cpacket.header = EMNETCLASS_CONTROL | EMNETFLAG_CONNECT | 
		EMNETCONNECT_MAGIC;
	conn->cpacket_payload_size = 0;
	output_cpacket(conn);
	
	emit_process_connecting();
	
	pthread_mutex_unlock(&net_mutex);
}


void em_connect_text(char *addrport)
{
	char *addr = NULL;
	uint16_t port = EMNET_DEFAULTPORT;
	
	if(addrport)
		sscanf(addrport, "%a[^:]:%hu", &addr, &port);

	
	uint32_t ip;
	
	if(!addr)
	{	
		ip = htonl(INADDR_LOOPBACK);
	}
	else
	{
		struct hostent *hostent;
		
		hostent = gethostbyname(addr);
		if(!hostent)
		{
			console_print("Couldn't get any host info; %s\n", strerror(h_errno));
			return;
		}
	
		ip = *(uint32_t*)hostent->h_addr;
	}
	
	free(addr);
	
	em_connect(ip, htons(port));
}
#endif


void em_disconnect(uint32_t temp_conn)
{
	struct conn_t *conn = (struct conn_t*)temp_conn;
	assert(conn);

	
	pthread_mutex_lock(&net_mutex);
	
	// make sure conn was not deleted before mutex acquired
	
	if(!conn_exist(conn))
		goto end;
	
	#ifdef EMSERVER
	conn->begun = 1;
	#endif

	switch(conn->state)
	{
	#ifdef EMCLIENT
	case NETSTATE_CONNECTING:	// should really enter a new state that will disconnect from 
		delete_conn(conn);		// the server if it connects, but just timeout otherwise
		emit_process_connection_failed(conn);
		goto end;
	#endif
	
	case NETSTATE_CONNECTED:
		break;
	
	case NETSTATE_DISCONNECTING:
	case NETSTATE_DISCONNECTED:
		goto end;
	}
	
	conn->cpacket.header = EMNETCLASS_CONTROL | EMNETFLAG_DISCONNECT | 
		(conn->cpacket.header & EMNETINDEX_MASK);
	
	if(!output_cpacket(conn))
	{
		delete_conn(conn);
		emit_process_conn_lost(conn);
		goto end;
	}
	
	conn->state = NETSTATE_DISCONNECTING;
	
	emit_process_disconnection(conn);

	end:
	pthread_mutex_unlock(&net_mutex);
}


#ifdef EMSERVER
void net_set_listen_port(uint16_t port)
{
	net_listen_port = port;
}
#endif


void init_network()
{
	size_t size = 20;
	hostname = malloc(size);

	while(gethostname(hostname, size) == -1)
	{
		if(errno != ENAMETOOLONG)
		{
			free(hostname);
			#ifdef EMSERVER
			server_libc_error("gethostname failure");
			#endif
			#ifdef EMCLIENT
			client_libc_error("gethostname failure");
			#endif
		}

		size *= 2;

		if(size > 65536)
		{
			free(hostname);
			#ifdef EMSERVER
			server_error("Host name far too long!");
			#endif
			#ifdef EMCLIENT
			client_libc_error("Host name far too long!");
			#endif
		}
			
		hostname = realloc(hostname, size);
	}

	console_print("Host: %s\n", hostname);

	struct hostent *hostent;
	hostent = gethostbyname2(hostname, AF_INET);
	if(!hostent)
	{
		console_print("Error: gethostbyname2 failure; %s\n", strerror(h_errno));
	}
	else
	{
		struct in_addr **addr = (struct in_addr**)hostent->h_addr_list;
	
		struct string_t *addr_list = new_string_text(inet_ntoa(**addr));
	
		addr++;
	
		int multiple = 0;
	
		while(*addr)
		{
			string_cat_text(addr_list, "; ");
			string_cat_text(addr_list, inet_ntoa(**addr));
	
			addr++;
			multiple = 1;
		}
	
		create_cvar_string("ipaddr", addr_list->text, CVAR_PROTECTED);
	
		if(multiple)
			console_print("IP addresses: %s\n", addr_list->text);
		else
			console_print("IP address: %s\n", addr_list->text);
		
		free_string(addr_list);
	}

	udp_fd = socket(PF_INET, SOCK_DGRAM, 0);
	if(udp_fd < 0)
		#ifdef EMSERVER
		server_libc_error("socket failure");
		#endif
		#ifdef EMCLIENT
		client_libc_error("socket failure");
		#endif

	#ifdef EMSERVER	
	struct sockaddr_in name;
	name.sin_family = AF_INET;
	name.sin_port = htons(net_listen_port);
	name.sin_addr.s_addr = htonl(INADDR_ANY);

	while(bind(udp_fd, (struct sockaddr *) &name, sizeof (name)) < 0)
	{
		if(errno == EADDRINUSE)
			name.sin_port = htons(++net_listen_port);
		else
			server_libc_error("bind failure");
	}

	console_print("Listening on port %i\n", net_listen_port);
	#endif
	
	
	// TODO : use connect()??
	

	fcntl(udp_fd, F_SETFL, O_NONBLOCK);
	
	// seed to rng
	
	uint64_t stamp = timestamp();
	
	seed48((unsigned short int*)(void*)&stamp);		// always check this for endianness
	
	pipe(net_kill_pipe);
	pipe(net_out_pipe);
	
	fcntl(net_out_pipe[0], F_SETFL, O_NONBLOCK);
	
	create_alarm_listener(network_alarm);
	
	pthread_create(&network_thread_id, NULL, network_thread, NULL);
}


void disconnect_all_connections()
{
	pthread_mutex_lock(&net_mutex);
	
	struct conn_t *conn = conn0;

	while(conn)
	{
		em_disconnect((uint32_t)conn);
		conn = conn->next;
	}

	pthread_mutex_unlock(&net_mutex);
}


void kill_network()		// FIXME
{
	disconnect_all_connections();
	net_shutting_down = 1;
	
	char c;
	TEMP_FAILURE_RETRY(write(net_kill_pipe[1], &c, 1));
	pthread_join(network_thread_id, NULL);
	close(net_kill_pipe[0]);
	close(net_kill_pipe[1]);
	
	delete_all_conns();
	close(udp_fd);
}
