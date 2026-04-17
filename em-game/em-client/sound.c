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

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <signal.h>
#include <math.h>
#include <pthread.h>
#include <string.h>

#include <sys/epoll.h>
#include <sys/poll.h>

#include <SDL/SDL.h>
#include <vorbis/codec.h>
#include <vorbis/vorbisfile.h>

#include "llist.h"
#include "minmax.h"
#include "timer.h"
#include "alarm.h"
#include "sgame.h"
#include "console.h"
#include "entry.h"
#include "tick.h"
#include "sound.h"
#include "game.h"


//snd_pcm_t *playback_handle = NULL;


struct queued_sample_t
{
	uint32_t index;
	int type;
	struct sample_t *sample;
	uint32_t ent_index;
	float x;
	float y;
	uint32_t start_tick;
	int begun;
	int next_frame;
	
	struct queued_sample_t *next;
		
} *queued_sample0 = NULL;


#define SAMPLE_TYPE_GLOBAL	0
#define SAMPLE_TYPE_STATIC	1
#define SAMPLE_TYPE_ENTITY	2


uint32_t next_queued_sample_index = 0;

//int sound_kill_pipe[2];
int sound_active = 0;
pthread_mutex_t sound_mutex = PTHREAD_MUTEX_INITIALIZER;
//pthread_t sound_thread_id;

int alsa_fd;


void add_frames(int32_t *dst, int16_t *src, int c)
{
	int i = 0;
	
	while(c)
	{
		dst[i] += src[i];
		i++;
		c--;
	}
}


void multiply_and_add_frames(int32_t *dst, int16_t *src, float m, int c)
{
	int i = 0;
	
	while(c)
	{
		dst[i] += lrint((double)src[i] * m);
		i++;
		c--;
	}
}


double get_attenuation(float x, float y)
{
	double xdist = (viewx - x) / 750.0;
	double ydist = (viewy - y) / 750.0;
	
	return min(1.0, 1.0 / (xdist * xdist + ydist * ydist));
}


double get_entity_attenuation(uint32_t ent_index)
{
	struct entity_t *entity = get_entity(centity0, ent_index);
		
	if(!entity)
		return 0.0;
	
	return get_attenuation(entity->xdis, entity->ydis);
}


void add_sample_frames(int32_t *dst, struct queued_sample_t *sample, int s, int c)
{
	int16_t *src = sample->sample->buf;
	double att;
	
	switch(sample->type)
	{
	case SAMPLE_TYPE_GLOBAL:
		add_frames(dst, &src[s], c);
		break;
	
	case SAMPLE_TYPE_STATIC:
		att = get_attenuation(sample->x, sample->y);
		multiply_and_add_frames(dst, &src[s], att, c);
		break;
	
	case SAMPLE_TYPE_ENTITY:
		att = get_entity_attenuation(sample->ent_index);
		multiply_and_add_frames(dst, &src[s], att, c);
		break;
	}
}


void saturate_frames(int32_t *dst, int c)
{
	int i = 0;
	
	while(c)
	{
		((int16_t*)dst)[i] = (int16_t)min(max(dst[i], -32768), 32767);
		i++;
		c--;
	}
}


void sound_mutex_lock()
{
	pthread_mutex_lock(&sound_mutex);
}
	

void sound_mutex_unlock()
{
	pthread_mutex_unlock(&sound_mutex);
}


void process_sound(void *userdata, Uint8 *stream, int len)	// check for off-by-ones
{
	int avail = len / 2;
	int32_t *buf = malloc(avail * 4);	// get rid of me
	
	memset(buf, 0, avail * 4);
	

	pthread_mutex_lock(&gamestate_mutex);
	sound_mutex_lock();
	
	uint32_t start_tick = get_game_tick();
	uint32_t end_tick = start_tick + (avail * counts_per_second) / 44100;
	
	struct queued_sample_t *c_queued_sample = queued_sample0;
	
	while(c_queued_sample)
	{
		// is the sample yet to be started
		
		if(!c_queued_sample->begun)
		{
			uint32_t sample_end_tick = c_queued_sample->start_tick + 
				(c_queued_sample->sample->len * counts_per_second) / 44100;
			
			
			// has the time for the sample to be played already passed?
			
			if(sample_end_tick < start_tick)
			{
				struct queued_sample_t *temp = c_queued_sample->next;
				LL_REMOVE(struct queued_sample_t, &queued_sample0, c_queued_sample);
				c_queued_sample = temp;
				
				continue;
			}
			
			
			// should the sample be started from the beginning?
			
			if(c_queued_sample->start_tick <= start_tick)
			{
				add_sample_frames(buf, c_queued_sample, 0, 
					min(c_queued_sample->sample->len, avail));
				
				
				// have we played the whole sample?
				
				if(avail >= c_queued_sample->sample->len)
				{
					struct queued_sample_t *temp = c_queued_sample->next;
					LL_REMOVE(struct queued_sample_t, &queued_sample0, c_queued_sample);
					c_queued_sample = temp;
					
					continue;
				}
				else
				{
					c_queued_sample->begun = 1;
					c_queued_sample->next_frame = avail;
				}
			}
			
			else
			

			// should the sample be started later in this buffer?
			
			if(c_queued_sample->start_tick <= end_tick)
			{
				int start_frame = ((c_queued_sample->start_tick - start_tick) * avail) / 
					((avail * counts_per_second) / 44100);
				
				add_sample_frames(&buf[start_frame], c_queued_sample, 0, 
					min(c_queued_sample->sample->len, avail - start_frame));
				
				
				// have we played the whole sample?
				
				if(avail - start_frame >= c_queued_sample->sample->len)
				{
					struct queued_sample_t *temp = c_queued_sample->next;
					LL_REMOVE(struct queued_sample_t, &queued_sample0, c_queued_sample);
					c_queued_sample = temp;
					
					continue;
				}
				else
				{
					c_queued_sample->begun = 1;
					c_queued_sample->next_frame = avail - start_frame;
				}
			}
		}
		else
		{
			add_sample_frames(buf, c_queued_sample, c_queued_sample->next_frame, 
				min(c_queued_sample->sample->len - c_queued_sample->next_frame, avail));
			
			
			// have we played the whole sample?
			
			if(c_queued_sample->next_frame + avail >= c_queued_sample->sample->len)
			{
				struct queued_sample_t *temp = c_queued_sample->next;
				LL_REMOVE(struct queued_sample_t, &queued_sample0, c_queued_sample);
				c_queued_sample = temp;
				
				continue;
			}
			else
			{
				c_queued_sample->next_frame += avail;
			}
		}
		
		c_queued_sample = c_queued_sample->next;
	}
	

	sound_mutex_unlock();
	pthread_mutex_unlock(&gamestate_mutex);
	
	saturate_frames(buf, avail);
//	snd_pcm_writei(playback_handle, buf, avail);
	
	memcpy(stream, buf, len);
	
	free(buf);
}


uint32_t start_global_sample(struct sample_t *sample, uint32_t start_tick)
{
	if(!sound_active)
		return -1;
	
	struct queued_sample_t queued_sample = {
		next_queued_sample_index++, 
		SAMPLE_TYPE_GLOBAL,
		sample, 0, 0.0, 0.0,
		start_tick, 0};
		
	sound_mutex_lock();
	LL_ADD(struct queued_sample_t, &queued_sample0, &queued_sample);
	sound_mutex_unlock();
	
	return queued_sample.index;
}


uint32_t start_static_sample(struct sample_t *sample, float x, float y, uint32_t start_tick)
{
	if(!sound_active)
		return -1;
	
	struct queued_sample_t queued_sample = {
		next_queued_sample_index++, 
		SAMPLE_TYPE_STATIC,
		sample, 0, x, y,
		start_tick, 0};
		
	sound_mutex_lock();
	LL_ADD(struct queued_sample_t, &queued_sample0, &queued_sample);
	sound_mutex_unlock();
	
	return queued_sample.index;
}


uint32_t start_entity_sample(struct sample_t *sample, uint32_t ent_index, uint32_t start_tick)
{
	if(!sound_active)
		return -1;
	
	struct queued_sample_t queued_sample = {
		next_queued_sample_index++, 
		SAMPLE_TYPE_ENTITY,
		sample, ent_index, 0.0, 0.0,
		start_tick, 0};
		
	sound_mutex_lock();
	LL_ADD(struct queued_sample_t, &queued_sample0, &queued_sample);
	sound_mutex_unlock();
	
	return queued_sample.index;
}


void stop_sample(uint32_t index)
{
	sound_mutex_lock();
	
	struct queued_sample_t *cqueued_sample = queued_sample0;
		
	while(cqueued_sample)
	{
		if(cqueued_sample->index == index)
		{
			LL_REMOVE(struct queued_sample_t, &queued_sample0, cqueued_sample);		// inefficient
			break;
		}
		
		cqueued_sample = cqueued_sample->next;
	}
	
	sound_mutex_unlock();
}

/*
void *sound_thread(void *a)
{
	struct pollfd *fds;
	int fdcount;
	
	fdcount = 2;
	
	fds = calloc(sizeof(struct pollfd), fdcount);
	
	fds[0].fd = alsa_fd;			fds[0].events = POLLIN;
	fds[1].fd = sound_kill_pipe[0];	fds[1].events = POLLIN;
	

	while(1)
	{
		if(poll(fds, fdcount, -1) == -1)
		{
			if(errno == EINTR)	// why is this necessary
				continue;
			
			return NULL;
		}

		if(fds[0].revents & POLLIN)
		{			
			sound_mutex_lock();
			process_alsa();
			sound_mutex_unlock();
		}
		
		if(fds[1].revents & POLLIN)
		{
			free(fds);
			pthread_exit(NULL);
		}
	}
}
*/

/*
void *sound_thread(void *a)
{
	int epoll_fd = epoll_create(2);
	
	struct epoll_event ev = 
	{
		.events = EPOLLIN | EPOLLET
	};

	ev.data.u32 = 0;
	epoll_ctl(epoll_fd, EPOLL_CTL_ADD, alsa_fd, &ev);

	ev.data.u32 = 1;
	epoll_ctl(epoll_fd, EPOLL_CTL_ADD, sound_kill_pipe[0], &ev);

	while(1)
	{
		epoll_wait(epoll_fd, &ev, 1, -1);
		
		switch(ev.data.u32)
		{
		case 0:
			sound_mutex_lock();
			process_alsa();
			sound_mutex_unlock();
			break;
		
		case 1:
			pthread_exit(NULL);
			break;
		}
	}
}
*/


struct sample_t *load_sample(char *filename)
{
	OggVorbis_File vf;

	FILE *file = fopen(filename, "r");
	
	if(ov_open(file, &vf, NULL, 0) < 0) {
	console_print("Input does not appear to be an Ogg bitstream.\n");
	}
	
//	vorbis_info *vi=ov_info(&vf,-1);
	
	struct sample_t *sample = malloc(sizeof(struct sample_t));
	
	sample->len = ov_pcm_total(&vf,-1);
	
	console_print("%i\n", sample->len);
	
	sample->buf = malloc(sample->len * 2);
	
	int current_section = 0;
	
	int j = 0;
	while(ov_read(&vf, (char*)&sample->buf[j], 2, 0, 2, 1, &current_section) > 0)
	{
		j++;
			
		if(j >= sample->len)
			break;
	}

	ov_clear(&vf);
		
	return sample;
}
	



void init_sound()
{
	/* Open the audio device */
	SDL_AudioSpec *desired, *obtained;
	SDL_AudioSpec *hardware_spec;
	
	/* Allocate a desired SDL_AudioSpec */
	desired = malloc(sizeof(SDL_AudioSpec));
	
	/* Allocate space for the obtained SDL_AudioSpec */
	obtained = malloc(sizeof(SDL_AudioSpec));
	
	/* 22050Hz - FM Radio quality */
	desired->freq=44100;
	
	/* 16-bit signed audio */
	desired->format=AUDIO_S16LSB;
	
	/* Mono */
	desired->channels=1;
	
	/* Large audio buffer reduces risk of dropouts but increases response time */
	desired->samples=256;
	
	/* Our callback function */
	desired->callback=process_sound;
	
	desired->userdata=NULL;
	
	/* Open the audio device */
	if ( SDL_OpenAudio(desired, obtained) < 0 ){
	  fprintf(stderr, "Couldn't open audio: %s\n", SDL_GetError());
	  exit(-1);
	}
	/* desired spec is no longer needed */
	free(desired);
	hardware_spec=obtained;
	
	SDL_PauseAudio(0);	
	char name[32];
	printf("Using audio driver: %s\n", SDL_AudioDriverName(name, 32));
	
	sound_active = 1;
}


void kill_sound()
{
	SDL_CloseAudio();
}


/*

void init_sound()
{
	int err;
	snd_pcm_hw_params_t *hw_params;
	
	if ((err = snd_pcm_open (&playback_handle, "default", SND_PCM_STREAM_PLAYBACK, 
		SND_PCM_NONBLOCK)) < 0) {
		console_print("cannot open audio device default (%s)\n", 
			 snd_strerror (err));
		return;
	}
	
	
	if ((err = snd_pcm_hw_params_malloc (&hw_params)) < 0) {
		console_print ("cannot allocate hardware parameter structure (%s)\n",
			 snd_strerror (err));
		return;
	}
			 
	if ((err = snd_pcm_hw_params_any (playback_handle, hw_params)) < 0) {
		console_print ("cannot initialize hardware parameter structure (%s)\n",
			 snd_strerror (err));
		return;
	}

	if ((err = snd_pcm_hw_params_set_access (playback_handle, hw_params, SND_PCM_ACCESS_RW_INTERLEAVED)) < 0) {
		console_print ("cannot set access type (%s)\n",
			 snd_strerror (err));
		return;
	}

	if ((err = snd_pcm_hw_params_set_format (playback_handle, hw_params, SND_PCM_FORMAT_S16_LE)) < 0) {
		console_print ("cannot set sample format (%s)\n",
			 snd_strerror (err));
		return;
	}

	if ((err = snd_pcm_hw_params_set_rate (playback_handle, hw_params, 44100, 0)) < 0) {
		console_print ("cannot set sample rate (%s)\n",
			 snd_strerror (err));
		return;
	}

	if ((err = snd_pcm_hw_params_set_channels (playback_handle, hw_params, 1)) < 0) {
		console_print ("cannot set channel count (%s)\n",
			 snd_strerror (err));
		return;
	}

    if (snd_pcm_hw_params_set_periods(playback_handle, hw_params, 2, 0) < 0) {
      fprintf(stderr, "Error setting periods.\n");
		return;
    }
	
 //  Set buffer size (in frames). The resulting latency is given by 
 //    latency = periodsize * periods / (rate * bytes_per_frame)     
    if (snd_pcm_hw_params_set_buffer_size(playback_handle, hw_params, 2048) < 0) {
      fprintf(stderr, "Error setting buffer size.\n");
 		return;
   }
	
	
/*	int l;
    if (snd_pcm_hw_params_get_buffer_size(hw_params, &l)) {
    }	
*/	

/*	if ((err = snd_pcm_hw_params (playback_handle, hw_params)) < 0) {
		console_print ("cannot set parameters (%s)\n",
			 snd_strerror (err));
		return;
	}

	snd_pcm_hw_params_free (hw_params);

//	snd_pcm_sw_params_t *sw_params;
	
	/* tell ALSA to wake us up whenever 4096 or more frames
		   of playback data can be delivered. Also, tell
		   ALSA that we'll start the device ourselves.
		*/
	
/*		if ((err = snd_pcm_sw_params_malloc (&sw_params)) < 0) {
			fprintf (stderr, "cannot allocate software parameters structure (%s)\n",
				 snd_strerror (err));
			exit (1);
		}
		if ((err = snd_pcm_sw_params_current (playback_handle, sw_params)) < 0) {
			fprintf (stderr, "cannot initialize software parameters structure (%s)\n",
				 snd_strerror (err));
			exit (1);
		}
		if ((err = snd_pcm_sw_params_set_avail_min (playback_handle, sw_params, 1)) < 0) {
			fprintf (stderr, "cannot set minimum available count (%s)n",
				 snd_strerror (err));
			exit (1);
		}
		if ((err = snd_pcm_sw_params_set_sleep_min (playback_handle, sw_params, 30)) < 0) {
			fprintf (stderr, "cannot set minimum available sleep (%s)\n",
				 snd_strerror (err));
			exit (1);
		}
		if ((err = snd_pcm_sw_params_set_start_threshold (playback_handle, sw_params, 0)) < 0) {
			fprintf (stderr, "cannot set start mode (%s)\n",
				 snd_strerror (err));
			exit (1);
		}
		if ((err = snd_pcm_sw_params (playback_handle, sw_params)) < 0) {
			fprintf (stderr, "cannot set software parameters (%s)\n",
				 snd_strerror (err));
			exit (1);
		}
	
		 the interface will interrupt the kernel every 4096 frames, and ALSA
		   will wake up this program very soon after that.
		*/
	
/*
	if ((err = snd_pcm_prepare (playback_handle)) < 0) {
		console_print ("cannot prepare audio interface for use (%s)\n",
			 snd_strerror (err));
		return;
	}
	
	sound_active = 1;
	
	pthread_mutex_init(&sound_mutex, NULL);
	pipe(sound_kill_pipe);
	alsa_fd = create_alarm_listener();
	pthread_create(&sound_thread_id, NULL, sound_thread, NULL);
}


void kill_sound()
{
	if(!sound_active)
		return;
	
	char c;
	write(sound_kill_pipe[1], &c, 1);
	pthread_join(sound_thread_id, NULL);
	close(sound_kill_pipe[0]);
	close(sound_kill_pipe[1]);

	if(playback_handle)
	{
		snd_pcm_close(playback_handle);
		playback_handle = NULL;
	}
}
*/
