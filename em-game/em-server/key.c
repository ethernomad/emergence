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
#include "buffer.h"
#include "user.h"
#include "console.h"
#include "key.h"
#include "main.h"

SSL_CTX *ctx;

int key_in_pipe[2];
int key_out_pipe[2];

pthread_t key_thread_id;

#define KEY_THREAD_IN_SHUTDOWN			0
#define KEY_THREAD_IN_VERIFY_SESSION	1

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
	char session_key[17];
	uint8_t msg;
	uint32_t conn;
	
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
			break;
		
		case KEY_THREAD_IN_VERIFY_SESSION:
		
			ssl = create_SSL(MASTER_SERVER);
			if(!ssl)
			{
				msg = KEY_THREAD_OUT_KEY_ERROR;
				TEMP_FAILURE_RETRY(write(key_out_pipe[1], &msg, 1));
				break;
			}
			
			TEMP_FAILURE_RETRY(read(key_in_pipe[0], &conn, 4));
			TEMP_FAILURE_RETRY(read(key_in_pipe[0], session_key, 16));
			session_key[16] = '\0';
			
			request = new_string_text("GET /verify-session.php?session_key=");
			string_cat_text(request, session_key);
//			string_cat_text(request, " HTTP/1.1\r\nHost: master.emergence.uk.net\r\n\r\n");
			string_cat_text(request, "\r\n");
			SSL_write(ssl, request->text, strlen(request->text));
			free_string(request);
			SSL_read(ssl, result, 8);
			
			if(strncmp(result, "success\n", 8) != 0)
			{
				msg = KEY_THREAD_OUT_KEY_DECLINED;
				TEMP_FAILURE_RETRY(write(key_out_pipe[1], &msg, 1));
				TEMP_FAILURE_RETRY(write(key_out_pipe[1], &conn, 4));
				SSL_clear(ssl);
				SSL_free(ssl);
				break;
			}
			
			msg = KEY_THREAD_OUT_KEY_ACCEPTED;
			TEMP_FAILURE_RETRY(write(key_out_pipe[1], &msg, 1));
			TEMP_FAILURE_RETRY(write(key_out_pipe[1], &conn, 4));
				
			SSL_clear(ssl);
			SSL_free(ssl);
			
			break;
		}
	}
}


void init_key()
{
	pipe(key_in_pipe);
	pipe(key_out_pipe);
	pthread_create(&key_thread_id, NULL, key_thread, NULL);
}


void kill_key()
{
	uint8_t msg = KEY_THREAD_IN_SHUTDOWN;
	TEMP_FAILURE_RETRY(write(key_in_pipe[1], &msg, 1));
	pthread_join(key_thread_id, NULL);
}


void key_verify_session(uint32_t conn, char session_key[16])
{
	uint8_t msg = KEY_THREAD_IN_VERIFY_SESSION;
	TEMP_FAILURE_RETRY(write(key_in_pipe[1], &msg, 1));
	TEMP_FAILURE_RETRY(write(key_in_pipe[1], &conn, 4));
	TEMP_FAILURE_RETRY(write(key_in_pipe[1], session_key, 16));
}


void process_key_out_pipe()
{
	uint8_t msg;
	uint32_t conn;
	
	TEMP_FAILURE_RETRY(read(key_out_pipe[0], &msg, 1));
	TEMP_FAILURE_RETRY(read(key_out_pipe[0], &conn, 4));

	switch(msg)
	{
	case KEY_THREAD_OUT_KEY_ACCEPTED:
		process_session_accepted(conn);
		break;
	
	case KEY_THREAD_OUT_KEY_DECLINED:
		process_session_declined(conn);
		break;
	
	case KEY_THREAD_OUT_KEY_ERROR:
		process_session_error(conn);
		break;
	}
}
