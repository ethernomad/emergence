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

#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <unistd.h>
#include <fcntl.h>
#include <pthread.h>
#include <string.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/poll.h>
#include <sys/socket.h>
#include <sys/sendfile.h>
#include <netinet/in.h>
#include <netinet/tcp.h>

#include "stringbuf.h"
#include "buffer.h"
#include "user.h"
#include "resource.h"
#include "llist.h"
#include "network.h"
#include "main.h"

struct download_t
{
	struct string_t *filename;
	int net_fd, file_fd;
	off_t size, offset;
	
	struct download_t *next;
		
} *download0 = NULL;

int download_kill_pipe[2];
pthread_t download_thread_id;
int fdcount;


int check_filename(char *cc)
{
	while(*cc)
	{
		if(*cc == '/')
			return 0;
		
		cc++;
	}
	
	return 1;
}


int start_download(struct download_t *download)
{
	struct stat buf;
		
	if(!check_filename(download->filename->text))
		return 0;
	
	struct string_t *filename = new_string_string(emergence_home_dir);
	string_cat_text(filename, "/maps/");
	string_cat_string(filename, download->filename);
	string_cat_text(filename, ".cmap");
	
	if(stat(filename->text, &buf) == -1)
	{
		free_string(filename);
		struct string_t *search = new_string_text("stock-maps/");
		string_cat_string(search, download->filename);
		string_cat_text(search, ".cmap");
		
		filename = new_string_text(find_resource(search->text));
		free_string(search);
	
		if(stat(filename->text, &buf) == -1)
		{
			free_string(filename);
			return 0;
		}
	}

	download->file_fd = open(filename->text, O_RDONLY | O_NONBLOCK | O_NOFOLLOW);
	free_string(filename);
	
	if(download->file_fd == -1)
		return 0;
	
	download->size = buf.st_size;
	download->offset = 0;
	
	if(TEMP_FAILURE_RETRY(send(download->net_fd, &download->size, 4, 0)) <= 0)
		return 0;
	
	return 1;
}


void remove_download(struct download_t *download)
{
	free_string(download->filename);
	close(download->net_fd);
	close(download->file_fd);
	LL_REMOVE(struct download_t, &download0, download);
	fdcount--;
}


void *download_thread(void *a)
{
	int listen_fd = socket(PF_INET, SOCK_STREAM, 0);
	if(listen_fd < 0)
		pthread_exit(NULL);

	struct sockaddr_in name;
	name.sin_family = AF_INET;
	name.sin_port = htons(net_listen_port);
	name.sin_addr.s_addr = htonl(INADDR_ANY);

	if(bind(listen_fd, (struct sockaddr *) &name, sizeof (name)) < 0)
	{
		close(listen_fd);
		pthread_exit(NULL);
	}

	if(listen(listen_fd, SOMAXCONN) < 0)
	{
		close(listen_fd);
		pthread_exit(NULL);
	}
	
	if(fcntl(listen_fd, F_SETFL, O_NONBLOCK) < 0)
	{
		close(listen_fd);
		pthread_exit(NULL);
	}
	
	fdcount = 2;
	struct pollfd *fds = NULL;
		
	while(1)
	{
		int cfd;
		struct download_t *cdownload, *temp;
			
		free(fds);
		fds = calloc(sizeof(struct pollfd), fdcount);
			
		fds[0].fd = download_kill_pipe[0];
		fds[0].events = POLLIN;
		
		fds[1].fd = listen_fd;
		fds[1].events = POLLIN;
			
		cdownload = download0;
		cfd = 2;

		while(cdownload)		
		{
			fds[cfd].fd = cdownload->net_fd;
			
			if(cdownload->file_fd == -1)
				fds[cfd].events = POLLIN;
			else
				fds[cfd].events = POLLOUT;
			
			LL_NEXT(cdownload);
			cfd++;
		}
		
		if(TEMP_FAILURE_RETRY(poll(fds, fdcount, -1)) == -1)
		{
			while(download0)
				remove_download(download0);
			
			free(fds);
			close(listen_fd);
			pthread_exit(NULL);
		}
		
		if(fds[0].revents & POLLIN)
		{
			while(download0)
				remove_download(download0);
			
			free(fds);
			close(listen_fd);
			pthread_exit(NULL);
		}
		
		if(fds[1].revents & POLLIN)
		{
			struct download_t download;
				
			download.net_fd = accept(listen_fd, NULL, NULL);
			
			if(download.net_fd != -1)
			{
				if(fcntl(download.net_fd, F_SETFL, O_NONBLOCK) < 0)
				{
					close(download.net_fd);
					continue;
				}
				
				int i = 1;
				if(setsockopt(download.net_fd, SOL_TCP, TCP_CORK, &i, 4) < 0)
				{
					close(download.net_fd);
					continue;
				}
				
				download.file_fd = -1;
				download.filename = new_string();
				
				LL_ADD(struct download_t, &download0, &download);
				fdcount++;
				continue;
			}
		}
		
		cdownload = download0;
		cfd = 2;
		while(cdownload)		
		{
			if(fds[cfd].revents & (POLLERR | POLLHUP))
			{
				temp = cdownload->next;
				remove_download(cdownload);
				cdownload = temp;
				cfd++;
				continue;
			}
			
			if(cdownload->file_fd == -1)
			{
				if(fds[cfd].revents & POLLIN)
				{
					char c;
					int r = TEMP_FAILURE_RETRY(recv(cdownload->net_fd, &c, 1, 0));
					
					if(r == -1 && errno == EAGAIN)
					{
						LL_NEXT(cdownload);
						cfd++;
						continue;
					}
					
					if(r <= 0)
					{
						temp = cdownload->next;
						remove_download(cdownload);
						cdownload = temp;
						cfd++;
						continue;
					}
					
					if(c != 0)
					{
						string_cat_char(cdownload->filename, c);
					}
					else
					{
						if(!start_download(cdownload))
						{
							temp = cdownload->next;
							remove_download(cdownload);
							cdownload = temp;
							cfd++;
							continue;
						}
					}
				}
			}
			else
			{
				if(fds[cfd].revents & POLLOUT)
				{
					sendfile(cdownload->net_fd, cdownload->file_fd, 
						&cdownload->offset, cdownload->size - cdownload->offset);
					
					if(cdownload->offset == cdownload->size)
					{
						temp = cdownload->next;
						remove_download(cdownload);
						cdownload = temp;
						cfd++;
						continue;
					}
				}
			}
			
			LL_NEXT(cdownload);
			cfd++;
		}
	}
	
	return NULL;
}


void init_download()
{
	pipe(download_kill_pipe);
	pthread_create(&download_thread_id, NULL, download_thread, NULL);
}


void kill_download()
{
	char c;
	TEMP_FAILURE_RETRY(write(download_kill_pipe[1], &c, 1));
	pthread_join(download_thread_id, NULL);
	close(download_kill_pipe[0]);
	close(download_kill_pipe[1]);
}
