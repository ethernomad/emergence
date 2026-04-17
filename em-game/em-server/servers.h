/**
 * @file
 * @copyright 1998-2004 Jonathan Brown <jbrown@bluedroplet.com>
 * @license https://www.gnu.org/licenses/gpl-3.0.html
 * @homepage https://github.com/bluedroplet/emergence
 */

#include "shared/servers.h"
#include "buffer.h"

void process_servers(struct buffer_t *stream);
void servers_process_serverinfo(struct sockaddr_in *sockaddr);
void init_servers();
void kill_servers();
