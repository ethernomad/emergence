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
#include <stdio.h>

#include <pthread.h>

#include <openssl/bio.h>
#include <openssl/ssl.h>
#include <openssl/rand.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>


#include "stringbuf.h"
#include "user.h"
#include "console.h"
#include "key.h"
#include "game.h"

SSL_CTX *ctx;

int key_in_pipe[2];
int key_out_pipe[2];

pthread_t key_thread_id;

int key_loaded = 0;
char key[17];

#define KEY_THREAD_IN_SHUTDOWN			0
#define KEY_THREAD_IN_NEW_SESSION		1
#define KEY_THREAD_IN_QUIT_SESSION		2

#define KEY_THREAD_OUT_KEY_ACCEPTED		0
#define KEY_THREAD_OUT_KEY_DECLINED		1
#define KEY_THREAD_OUT_KEY_ERROR		2

#define MASTER_SERVER "master.emergence.uk.net:442"
//#define MASTER_SERVER "localhost:443"


SSL *create_SSL(char *addr)
{
	SSL *ssl;
	BIO *conn;

	conn = BIO_new_connect(addr);
	if(!conn)
		return NULL;

	if(BIO_do_connect(conn) <= 0)
		return NULL;
	
	ssl = SSL_new(ctx);
	if(!ssl)
		return NULL;
	
	SSL_set_bio(ssl, conn, conn);
	
	if(SSL_connect(ssl) <= 0)
		return NULL;
	
	return ssl;
}


void *key_thread(void *a)
{
	SSL *ssl;
	struct string_t *request = NULL;
	char result[8];
	char session_key[16];
	uint8_t msg;
	
	SSL_library_init();
	
	RAND_load_file("/dev/urandom", 1024);
	
	ctx = SSL_CTX_new(SSLv23_method());
	
	while(1)
	{
		TEMP_FAILURE_RETRY(read(key_in_pipe[0], &msg, 1));
		
		switch(msg)
		{
		case KEY_THREAD_IN_SHUTDOWN:
			SSL_CTX_free(ctx);
			pthread_exit(NULL);
		
		case KEY_THREAD_IN_NEW_SESSION:
		
			ssl = create_SSL(MASTER_SERVER);
			if(!ssl)
			{
				msg = KEY_THREAD_OUT_KEY_ERROR;
				TEMP_FAILURE_RETRY(write(key_out_pipe[1], &msg, 1));
				break;
			}
			
			request = new_string_text("GET /new-session.php?key=");
			string_cat_text(request, key);
//			string_cat_text(request, " HTTP/1.1\r\nHost: master.emergence.uk.net\r\n\r\n");
			string_cat_text(request, "\r\n");
			SSL_write(ssl, request->text, strlen(request->text));
			free_string(request);
			SSL_read(ssl, result, 8);
			
			if(strncmp(result, "success\n", 8) != 0)
			{
				msg = KEY_THREAD_OUT_KEY_DECLINED;
				TEMP_FAILURE_RETRY(write(key_out_pipe[1], &msg, 1));
				SSL_clear(ssl);
				SSL_free(ssl);
				break;
			}
			
			SSL_read(ssl, session_key, 16);

			msg = KEY_THREAD_OUT_KEY_ACCEPTED;
			TEMP_FAILURE_RETRY(write(key_out_pipe[1], &msg, 1));
			TEMP_FAILURE_RETRY(write(key_out_pipe[1], session_key, 16));
				
			SSL_clear(ssl);
			SSL_free(ssl);
			
			break;

			
		case KEY_THREAD_IN_QUIT_SESSION:
			
			ssl = create_SSL(MASTER_SERVER);
			if(!ssl)
				break;
			
			request = new_string_text("GET /quit-session.php?key=");
			string_cat_text(request, key);
//			string_cat_text(request, " HTTP/1.1\r\nHost: master.emergence.uk.net\r\n\r\n");
			string_cat_text(request, "\r\n");
			SSL_write(ssl, request->text, strlen(request->text));
			free_string(request);
				
			SSL_clear(ssl);
			SSL_free(ssl);
			
			break;
		}
	}
}


void init_key()
{
	struct string_t *key_path = new_string_string(home_dir);
	string_cat_text(key_path, "/.emergence-key");
	
	FILE *key_file = fopen(key_path->text, "r");
	free_string(key_path);

	if(!key_file)
	{
		console_print("Authentication key not found.\n");
		return;
	}
	
	fread(key, 1, 16, key_file);
	
	fclose(key_file);
	
	key[16] = '\0';
	
	console_print("Authentication key found.\n");
	key_loaded = 1;

	pipe(key_in_pipe);
	pipe(key_out_pipe);
	pthread_create(&key_thread_id, NULL, key_thread, NULL);
}


void kill_key()
{
	if(!key_loaded)
		return;
	
	uint8_t msg = KEY_THREAD_IN_SHUTDOWN;
	TEMP_FAILURE_RETRY(write(key_in_pipe[1], &msg, 1));
	pthread_join(key_thread_id, NULL);
	close(key_in_pipe[0]);
	close(key_in_pipe[1]);
	close(key_out_pipe[0]);
	close(key_out_pipe[1]);
}


int key_create_session()
{
	if(!key_loaded)
		return 0;
	
	uint8_t msg = KEY_THREAD_IN_NEW_SESSION;
	TEMP_FAILURE_RETRY(write(key_in_pipe[1], &msg, 1));
	
	return 1;
}


void process_key_out_pipe()
{
	uint8_t msg;
	char session_key[16];
	
	TEMP_FAILURE_RETRY(read(key_out_pipe[0], &msg, 1));

	switch(msg)
	{
	case KEY_THREAD_OUT_KEY_ACCEPTED:
		TEMP_FAILURE_RETRY(read(key_out_pipe[0], session_key, 16));
		game_process_key_accepted(session_key);
		break;
	
	case KEY_THREAD_OUT_KEY_DECLINED:
		game_process_key_declined();
		break;
	
	case KEY_THREAD_OUT_KEY_ERROR:
		game_process_key_error();
		break;
	}
}
