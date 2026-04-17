/**
 * @file
 * @copyright 1998-2004 Jonathan Brown <jbrown@bluedroplet.com>
 * @license https://www.gnu.org/licenses/gpl-3.0.html
 * @homepage https://github.com/bluedroplet/emergence
 */

#define _GNU_SOURCE
#ifndef _REENTRANT
#define _REENTRANT
#endif

#include <stdint.h>
#include <pthread.h>


#include "gsub.h"
#include "worker_thread.h"


pthread_mutex_t mutex;


void enter_main_lock()
{
	pthread_mutex_lock(&mutex);
}


void worker_enter_main_lock()
{
	pthread_mutex_lock(&mutex);
}


int worker_try_enter_main_lock()
{
	if(!pthread_mutex_trylock(&mutex))
		return 1;
	else
		return 0;
}


void leave_main_lock()
{
	pthread_mutex_unlock(&mutex);
}


void worker_leave_main_lock()
{
	pthread_mutex_unlock(&mutex);
}


void create_main_lock()
{
	pthread_mutex_init(&mutex, NULL);
}


void destroy_main_lock()
{
	pthread_mutex_destroy(&mutex);
}


struct surface_t *leave_main_lock_and_rotate_surface(struct surface_t *in_surface, 
	int scale_width, int scale_height, double theta)
{
	struct surface_t *duplicate, *rotated;
	
	duplicate = duplicate_surface(in_surface);
	
	leave_main_lock();
	
	rotated = rotate_surface(duplicate, scale_width, scale_height, theta);
	free_surface(duplicate);
	return rotated;
}


struct surface_t *leave_main_lock_and_resize_surface(struct surface_t *in_surface, 
	int width, int height)
{
	struct surface_t *duplicate, *resized;
	
	duplicate = duplicate_surface(in_surface);
	
	leave_main_lock();
	
	resized = resize(duplicate, width, height);
	free_surface(duplicate);
	return resized;
}
