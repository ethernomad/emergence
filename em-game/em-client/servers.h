/**
 * @file
 * @copyright 1998-2004 Jonathan Brown <jbrown@bluedroplet.com>
 * @license https://www.gnu.org/licenses/gpl-3.0.html
 * @homepage https://github.com/bluedroplet/emergence
 */

#include "shared/servers.h"

void init_servers();
void kill_servers();
extern int servers_info_pipe[2];

#ifdef	_NETINET_IN_H
void add_new_last_seen_server(struct sockaddr_in *sockaddr);
#endif

void emit_servers(uint32_t conn);
void start_server_discovery();
void stop_server_discovery();
void render_servers();

void server_up(int state);
void server_down(int state);
void server_enter(int state);

extern int server_discovery_started;
