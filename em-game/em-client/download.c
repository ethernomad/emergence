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

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <sys/sendfile.h>
#include <sys/poll.h>
#include <netinet/in.h>

#include <stdlib.h>
#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <pthread.h>
#include <string.h>
#include <errno.h>

#include "stringbuf.h"
#include "user.h"
#include "gsub.h"
#include "network.h"
#include "game.h"
#include "map.h"
#include "render.h"

int download_in_pipe[2];
int download_out_pipe[2];
pthread_t download_thread_id;

#define DOWNLOAD_THREAD_IN_SHUTDOWN				0
#define DOWNLOAD_THREAD_IN_DOWNLOAD_FILE		1
#define DOWNLOAD_THREAD_IN_STOP_DOWNLOADING		2

#define DOWNLOAD_THREAD_OUT_DOWNLOAD_PROGRESS	0
#define DOWNLOAD_THREAD_OUT_DOWNLOAD_COMPLETED	1
#define DOWNLOAD_THREAD_OUT_DOWNLOAD_FAILED		2

int download_net_fd = -1, download_file_fd = -1;
off_t download_size, download_offset;

uint8_t *download_buf;

#define DOWNLOAD_BUF_SIZE 65536

int map_download_progress;


void _stop_downloading_map()
{
	close(download_net_fd);
	download_net_fd = -1;
	
	close(download_file_fd);
	download_file_fd = -1;
}


int start_downloading_map(char *map_name)
{
	download_net_fd = socket(PF_INET, SOCK_STREAM, 0);
	if(download_net_fd < 0)
	{
		perror(NULL);
		return 0;
	//	client_libc_error("socket failure");
	}
	
	struct sockaddr_in sockaddr;
	get_sockaddr_in_from_conn(game_conn, &sockaddr);
	
	if(connect(download_net_fd, &sockaddr, sizeof(struct sockaddr_in)) < 0)
	{
		perror(NULL);
		_stop_downloading_map();
		return 0;
	}
	
	TEMP_FAILURE_RETRY(
		send(download_net_fd, map_name, strlen(map_name) + 1, 0));		// check for EAGAIN
	
	struct string_t *filename = new_string_string(emergence_home_dir);
	string_cat_text(filename, "/maps/");
	string_cat_text(filename, map_name);
	string_cat_text(filename, ".cmap");
	
	download_file_fd = open(filename->text, O_CREAT | O_WRONLY | O_TRUNC | O_NOFOLLOW, 
		S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
	free_string(filename);
	
	if(download_file_fd < 0)
	{
		_stop_downloading_map();
		return 0;
	}
	
	
	int i = 0;
	
	while(i < 4)
	{
		int r = TEMP_FAILURE_RETRY(recv(download_net_fd, &((uint8_t*)&download_size)[i], 4 - i, 0));
		
		if(r <= 0)
		{
			if(r < 0)
				perror(NULL);
			
			_stop_downloading_map();
			return 0;
		}
		
		i += r;
	}

	fcntl(download_net_fd, F_SETFL, O_NONBLOCK);

	download_offset = 0;

	return 1;
}


void *download_thread(void *a)
{
	struct pollfd *fds;
	uint8_t msg;
	char c;
	struct string_t *map_name = new_string();
	download_buf = malloc(DOWNLOAD_BUF_SIZE);
	
	fds = calloc(sizeof(struct pollfd), 2);
	
	while(1)
	{
		int fdcount;
		
		if(download_net_fd != -1)
			fdcount = 2;
		else
			fdcount = 1;
		
		fds[0].fd = download_in_pipe[0];
		fds[0].events = POLLIN;
		
		if(download_net_fd != -1)
		{
			fds[1].fd = download_net_fd;
			fds[1].events = POLLIN;
		}
		
		if(TEMP_FAILURE_RETRY(poll(fds, fdcount, -1)) == -1)
		{
			_stop_downloading_map();
			free_string(map_name);
			free(download_buf);
			free(fds);
			pthread_exit(NULL);
		}
		
		if(fds[0].revents & POLLIN)
		{
			TEMP_FAILURE_RETRY(read(download_in_pipe[0], &msg, 1));
			
			switch(msg)
			{
			case DOWNLOAD_THREAD_IN_SHUTDOWN:
				_stop_downloading_map();
				free_string(map_name);
				free(download_buf);
				free(fds);
				pthread_exit(NULL);
			
			case DOWNLOAD_THREAD_IN_DOWNLOAD_FILE:
				
				string_clear(map_name);
				TEMP_FAILURE_RETRY(read(download_in_pipe[0], &c, 1));
					
				while(c)
				{
					string_cat_char(map_name, c);
					TEMP_FAILURE_RETRY(read(download_in_pipe[0], &c, 1));
				}
				
				if(download_net_fd != -1)
					_stop_downloading_map();
				
				if(!start_downloading_map(map_name->text))
				{
					msg = DOWNLOAD_THREAD_OUT_DOWNLOAD_FAILED;
					TEMP_FAILURE_RETRY(write(download_out_pipe[1], &msg, 1));
				}
				
				break;
				
			case DOWNLOAD_THREAD_IN_STOP_DOWNLOADING:
				_stop_downloading_map();
				break;
			}
		}
		
		if(download_net_fd != -1)
		{
			if(fds[1].revents & (POLLERR | POLLHUP))
			{
				_stop_downloading_map();
				continue;
			}
			
			if(fds[1].revents & POLLIN)
			{
				int r = TEMP_FAILURE_RETRY(
					recv(download_net_fd, download_buf, DOWNLOAD_BUF_SIZE, 0));
				
				if(r < 0 && errno == EAGAIN)
					continue;
				
				if(r <= 0)
				{
					_stop_downloading_map();
					msg = DOWNLOAD_THREAD_OUT_DOWNLOAD_FAILED;
					TEMP_FAILURE_RETRY(write(download_out_pipe[1], &msg, 1));
					continue;
				}
				
				TEMP_FAILURE_RETRY(write(download_file_fd, download_buf, r));
				download_offset += r;
				
				if(download_offset == download_size)
				{
					_stop_downloading_map();
					msg = DOWNLOAD_THREAD_OUT_DOWNLOAD_COMPLETED;
					TEMP_FAILURE_RETRY(write(download_out_pipe[1], &msg, 1));
				}
				else
				{
					msg = DOWNLOAD_THREAD_OUT_DOWNLOAD_PROGRESS;
					TEMP_FAILURE_RETRY(write(download_out_pipe[1], &msg, 1));
					msg = (download_offset * 100) / download_size;
					TEMP_FAILURE_RETRY(write(download_out_pipe[1], &msg, 1));
				}
			}
		}
	}
}


void download_map(char *map_name)
{
	map_download_progress = 0;
	
	uint8_t m = DOWNLOAD_THREAD_IN_DOWNLOAD_FILE;
	TEMP_FAILURE_RETRY(write(download_in_pipe[1], &m, 1));
	
	char *cc = map_name;
	
	while(*cc)
	{
		TEMP_FAILURE_RETRY(write(download_in_pipe[1], cc, 1));
		cc++;
	}
	
	TEMP_FAILURE_RETRY(write(download_in_pipe[1], cc, 1));
}


void stop_downloading_map()
{
	uint8_t m = DOWNLOAD_THREAD_IN_STOP_DOWNLOADING;
	TEMP_FAILURE_RETRY(write(download_in_pipe[1], &m, 1));
}


void init_download()
{
	pipe(download_in_pipe);
	pipe(download_out_pipe);
	pthread_create(&download_thread_id, NULL, download_thread, NULL);
}


void kill_download()
{
	uint8_t m = DOWNLOAD_THREAD_IN_SHUTDOWN;
	TEMP_FAILURE_RETRY(write(download_in_pipe[1], &m, 1));
	pthread_join(download_thread_id, NULL);
	close(download_in_pipe[0]);
	close(download_in_pipe[1]);
	close(download_out_pipe[0]);
	close(download_out_pipe[1]);
}


void process_download_out_pipe()
{
	uint8_t msg;
	
	TEMP_FAILURE_RETRY(read(download_out_pipe[0], &msg, 1));

	switch(msg)
	{
	case DOWNLOAD_THREAD_OUT_DOWNLOAD_PROGRESS:
		TEMP_FAILURE_RETRY(read(download_out_pipe[0], &msg, 1));
		map_download_progress = msg;
		break;
	
	case DOWNLOAD_THREAD_OUT_DOWNLOAD_COMPLETED:
		game_process_map_downloaded();
		break;
	
	case DOWNLOAD_THREAD_OUT_DOWNLOAD_FAILED:
		game_process_map_download_failed();
		break;
	}
}


void render_map_downloading()
{
	blit_text_centered(vid_width / 2, vid_height / 6, 0xef, 0x6f, 0xff, 
		s_backbuffer, "Downloading map...");

	blit_text_centered(vid_width / 2, vid_height / 6 + 14, 0xef, 0x6f, 0xff, 
		s_backbuffer, "[%i%%]", map_download_progress);
}
