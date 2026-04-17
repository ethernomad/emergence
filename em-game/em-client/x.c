/**
 * @file
 * @copyright 1998-2004 Jonathan Brown <jbrown@bluedroplet.com>
 * @license https://www.gnu.org/licenses/gpl-3.0.html
 * @homepage https://github.com/bluedroplet/emergence
 */

#define _GNU_SOURCE
#define _REENTRANT


#include <stdint.h>
#include <signal.h>
#include <math.h>
#include <stdlib.h>
#include <pthread.h>
#include <errno.h>

#include <assert.h>

#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/epoll.h>
#include <sys/poll.h>


#define __USE_X_SHAREDMEMORY__

#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/Xos.h>
#include <X11/Xatom.h>
#include <X11/extensions/XInput.h>
#include <X11/extensions/XShm.h>
#include <X11/extensions/Xrandr.h>

#include "types.h"
#include "llist.h"
#include "stringbuf.h"
#include "buffer.h"
#include "gsub.h"
#include "timer.h"
#include "cvar.h"
#include "console.h"
#include "main.h"
#include "entry.h"
#include "control.h"
#include "render.h"
#include "map.h"
#include "game.h"


Display *xdisplay;
int xscreen;
Window xwindow;

XImage *image;

GC gc;

int x_fd;
int x_render_pipe[2];
int x_kill_pipe[2];

int use_x_mouse;

XRRScreenConfiguration *screen_config;
int original_mode;
int vid_modes;
struct vid_mode_t
{
	int width;
	int height;
	int index;
	
	struct vid_mode_t *next;
		
} *vid_mode0 = NULL;

int vid_mode = -1;
int depth;

pthread_mutex_t x_mutex = PTHREAD_RECURSIVE_MUTEX_INITIALIZER_NP;
pthread_t x_thread_id;

int accel_numerator, accel_denominator, threshold;


void update_frame_buffer()
{
	pthread_mutex_lock(&x_mutex);	
	
//	float time1 = get_double_time();
	
	XShmPutImage(xdisplay, xwindow, gc, image, 
		0, 0, 0, 0, vid_width, vid_height, True);
//	float time2 = get_double_time();
	XFlush(xdisplay);
//	float time3 = get_double_time();
	
//	printf("%f %f %f\n", time1, time2, time3);
	
	pthread_mutex_unlock(&x_mutex);	
}


static int aaaa(XEvent *event)
{
	XEvent peekevent;
	int repeated;

	repeated = 0;
	if ( XPending(xdisplay) ) {
		XPeekEvent(xdisplay, &peekevent);
		if ( (peekevent.type == KeyPress) &&
		     (peekevent.xkey.keycode == event->xkey.keycode) &&
		     ((peekevent.xkey.time-event->xkey.time) < 2) ) {
			repeated = 1;
			XNextEvent(xdisplay, &peekevent);
		}
	}
	return(repeated);
}

#define MOUSE_FUDGE_FACTOR	8


void process_x_motion(int x, int y)
{
	if(x)
		process_axis(0, (float)x);

	if(y)
		process_axis(1, (float)y);
}

struct
{
	int x, y;
	
} mouse_last;

int firsta = 1;

void bbbb(XEvent *xevent)
{
	if(firsta)
	{
		firsta = 0;
		mouse_last.x = xevent->xmotion.x;
		mouse_last.y = xevent->xmotion.y;
	}
	
	int w, h, i;
	int deltax, deltay;

	w = vid_width;
	h = vid_height;
	deltax = xevent->xmotion.x - mouse_last.x;
	deltay = xevent->xmotion.y - mouse_last.y;
	
	mouse_last.x = xevent->xmotion.x;
	mouse_last.y = xevent->xmotion.y;
	process_x_motion(deltax, deltay);

	if ( (xevent->xmotion.x < MOUSE_FUDGE_FACTOR) ||
	     (xevent->xmotion.x > (w-MOUSE_FUDGE_FACTOR)) ||
	     (xevent->xmotion.y < MOUSE_FUDGE_FACTOR) ||
	     (xevent->xmotion.y > (h-MOUSE_FUDGE_FACTOR)) ) 
	{
		/* Get the events that have accumulated */
		while ( XCheckTypedEvent(xdisplay, MotionNotify, xevent) ) 
		{
			deltax = xevent->xmotion.x - mouse_last.x;
			deltay = xevent->xmotion.y - mouse_last.y;

			mouse_last.x = xevent->xmotion.x;
			mouse_last.y = xevent->xmotion.y;
			process_x_motion(deltax, deltay);
		}
		
		mouse_last.x = w/2;
		mouse_last.y = h/2;
		XWarpPointer(xdisplay, None, xwindow, 0, 0, 0, 0,
					mouse_last.x, mouse_last.y);
		for ( i=0; i<10; ++i ) 
		{
        		XMaskEvent(xdisplay, PointerMotionMask, xevent);
			if ( (xevent->xmotion.x >
			          (mouse_last.x-MOUSE_FUDGE_FACTOR)) &&
			     (xevent->xmotion.x <
			          (mouse_last.x+MOUSE_FUDGE_FACTOR)) &&
			     (xevent->xmotion.y >
			          (mouse_last.y-MOUSE_FUDGE_FACTOR)) &&
			     (xevent->xmotion.y <
			          (mouse_last.y+MOUSE_FUDGE_FACTOR)) ) {
				break;
			}
			
		}
	}
}


void process_x()
{
	while(XPending(xdisplay))
	{
		
		XEvent report;
		char c;
		
		XNextEvent(xdisplay, &report);
		
		int CompletionType = XShmGetEventBase (xdisplay) + ShmCompletion;
	
		switch(report.type)
		{
		case MotionNotify:
			bbbb(&report);
			
			break;
			
	    case ButtonPress: {
			switch(report.xbutton.button)
			{
			case Button1:
				process_button(0, 1);
				break;
			
			case Button2:
				process_button(2, 1);
				break;
			
			case Button3:
				process_button(1, 1);
				break;
			
			case Button4:
				process_button(3, 1);
				break;
			
			case Button5:
				process_button(4, 1);
				break;
			}
			
	    }
	    break;

	    /* Mouse button release? */
	    case ButtonRelease: {
			switch(report.xbutton.button)
			{
			case Button1:
				process_button(0, 0);
				break;
			
			case Button2:
				process_button(2, 0);
				break;
			
			case Button3:
				process_button(1, 0);
				break;
			
			case Button4:
				process_button(3, 0);
				break;
			
			case Button5:
				process_button(4, 0);
				break;
			}
	    }
	    break;

		case Expose:
			// unless this is the last contiguous expose,
			// don't draw the window 
			if (report.xexpose.count != 0)
				break;
	
		//	write(x_render_pipe[1], &c, 1);
			
			break;
		case KeyPress:
			//	XKeysymToKeycode(xdisplay, xsym);
	
				process_keypress(report.xkey.keycode - 8, 1);
			
	//		process_keypress(((XKeyEvent*)&report)->keycode, 1);
			break;
		
		case KeyRelease:
			if(!aaaa(&report))
				process_keypress(report.xkey.keycode - 8, 0);
			break;
		
		default:
		
			if(report.type == CompletionType)
			{
				TEMP_FAILURE_RETRY(write(x_render_pipe[1], &c, 1));
			}
			
			break;
		}
	}
}

Cursor create_blank_cursor()
{
	Cursor cursor;
	XGCValues GCvalues;
	GC        GCcursor;
	XImage *data_image, *mask_image;
	Pixmap  data_pixmap, mask_pixmap;
	int       clen, i;
	char     *x_data, *x_mask;
	static XColor black = {  0,  0,  0,  0 };
	static XColor white = { 0xffff, 0xffff, 0xffff, 0xffff };
	
	uint8_t data[8] = { 0, 0, 0, 0, 0, 0, 0, 0 };
	uint8_t mask[8] = { 0, 0, 0, 0, 0, 0, 0, 0 };

	/* Mix the mask and the data */
	clen = 8;
	x_data = (char *)malloc(clen);
	if ( x_data == NULL ) {
	//	SDL_OutOfMemory();
		return 0;
	}
	x_mask = (char *)malloc(clen);
	if ( x_mask == NULL ) {
		free(x_data);
	//	SDL_OutOfMemory();
		return 0;
	}
	for ( i=0; i<clen; ++i ) {
		/* The mask is OR'd with the data to turn inverted color
		   pixels black since inverted color cursors aren't supported
		   under X11.
		 */
		x_mask[i] = data[i] | mask[i];
		x_data[i] = data[i];
	}


	/* Create the data image */
	data_image = XCreateImage(xdisplay, 
			DefaultVisual(xdisplay, xscreen),
					1, XYBitmap, 0, x_data, 8, 8, 8, 1);
	data_image->byte_order = MSBFirst;
	data_image->bitmap_bit_order = MSBFirst;
	data_pixmap = XCreatePixmap(xdisplay, xwindow, 8, 8, 1);

	/* Create the data mask */
	mask_image = XCreateImage(xdisplay, 
			DefaultVisual(xdisplay, xscreen),
					1, XYBitmap, 0, x_mask, 8, 8, 8, 1);
	mask_image->byte_order = MSBFirst;
	mask_image->bitmap_bit_order = MSBFirst;
	mask_pixmap = XCreatePixmap(xdisplay, xwindow, 8, 8, 1);

	/* Create the graphics context */
	GCvalues.function = GXcopy;
	GCvalues.foreground = ~0;
	GCvalues.background =  0;
	GCvalues.plane_mask = AllPlanes;
	GCcursor = XCreateGC(xdisplay, data_pixmap,
			(GCFunction|GCForeground|GCBackground|GCPlaneMask),
								&GCvalues);

	/* Blit the images to the pixmaps */
	XPutImage(xdisplay, data_pixmap, GCcursor, data_image,
							0, 0, 0, 0, 8, 8);
	XPutImage(xdisplay, mask_pixmap, GCcursor, mask_image,
							0, 0, 0, 0, 8, 8);
	XFreeGC(xdisplay, GCcursor);
	/* These free the x_data and x_mask memory pointers */
	XDestroyImage(data_image);
	XDestroyImage(mask_image);

	/* Create the cursor */
	cursor = XCreatePixmapCursor(xdisplay, data_pixmap,
				mask_pixmap, &black, &white, 0, 0);


	return(cursor);
}


void query_vid_modes()
{
	console_print("Querying available video modes\n");
	
	screen_config = XRRGetScreenInfo(xdisplay, RootWindow(xdisplay, xscreen));
	
	if(!screen_config)
		client_error("XRRScreenConfig failed");
	
	int nsizes;
	XRRScreenSize *screens = XRRConfigSizes(screen_config, &nsizes);
	
	
	int n = 0;
	vid_modes = 0;
	while(nsizes)
	{
		int width = screens[n].width;
		int height = screens[n].height;

		n++;
		nsizes--;
		
		if(width * 3 != height * 4)
			continue;

		double d;
		if(modf((double)width / 8, &d) != 0.0)	// make sure 200x200 blocks scale to integer
									// (change this to integer)
			continue;
		
		struct vid_mode_t *cvid_mode = vid_mode0;
		int dup = 0;
			
		while(cvid_mode)
		{
			if(cvid_mode->width == width &&
				cvid_mode->height == height)
			{
				dup = 1;
				break;
			}
			
			cvid_mode = cvid_mode->next;
		}
		
		if(dup)
			continue;
			
			
		vid_modes++;
		
		struct vid_mode_t vid_mode = {width, height, n-1};
		LL_ADD(struct vid_mode_t, &vid_mode0, &vid_mode);
	}
	
	console_print("Found %u modes:\n", vid_modes);
	
	// sort modes
	
	struct vid_mode_t *new_vid_mode0 = NULL;
	
	while(vid_mode0)
	{
		struct vid_mode_t *max_vid_mode = vid_mode0;
		
		struct vid_mode_t *nvid_mode = max_vid_mode->next;
			
		while(nvid_mode)
		{
			if(nvid_mode->width > max_vid_mode->width)
				max_vid_mode = nvid_mode;
			
			nvid_mode = nvid_mode->next;
		}
		
		console_print("%ux%u\n", max_vid_mode->width, max_vid_mode->height);
		
		LL_ADD_TAIL(struct vid_mode_t, &new_vid_mode0, max_vid_mode);
		LL_REMOVE(struct vid_mode_t, &vid_mode0, max_vid_mode);
	}
	
	vid_mode0 = new_vid_mode0;
	
	if(vid_mode == -1 || vid_mode >= vid_modes)
	{
		// find a nice mode
	
		struct vid_mode_t *cvid_mode;
		int nearest = 0;
		int dist;
		
		assert(vid_mode0);
		
		dist = abs(vid_mode0->width - 640);
			
		cvid_mode = vid_mode0->next;
		
		int m = 1;
		
		while(cvid_mode)
		{
			int this_dist = abs(cvid_mode->width - 640);
			
			if(this_dist < dist)
			{
				dist = this_dist;
				nearest = m;
			}
			
			cvid_mode = cvid_mode->next;
			m++;
		}
		
		vid_mode = nearest;
	}
	
	
	Rotation r;
	original_mode = XRRConfigCurrentConfiguration(screen_config, &r);
	XRRFreeScreenConfigInfo(screen_config);
}


void set_vid_mode(int mode)	// use goto error crap
{
	if(mode >= vid_modes)
		return;
	
	pthread_mutex_lock(&x_mutex);
	
	
	if(image)
	{
		XDestroyImage(image);
		image = NULL;
	}
	
	Window oldwindow = xwindow;

	struct vid_mode_t *cvid_mode = vid_mode0;
		
	while(mode)
	{
		mode--;
		
		cvid_mode = cvid_mode->next;
		if(!cvid_mode)
			return;
	}
	

    XSetWindowAttributes xattr;

    xattr.override_redirect = True;
    xattr.background_pixel = BlackPixel(xdisplay, xscreen);
    xattr.border_pixel = 0;
    xattr.colormap = DefaultColormap(xdisplay, xscreen);

	vid_width = cvid_mode->width;
	vid_height = cvid_mode->height;

    xwindow = XCreateWindow(xdisplay, RootWindow(xdisplay, xscreen), 0, 0, vid_width, vid_height, 0,
			     depth, InputOutput, DefaultVisual(xdisplay, xscreen),
			     CWOverrideRedirect | CWBackPixel | CWBorderPixel
			     | CWColormap,
			     &xattr);

	XMapRaised(xdisplay, xwindow);

		
	if(use_x_mouse)
		XSelectInput(xdisplay, xwindow, KeyPressMask | KeyReleaseMask | PointerMotionMask
			| ButtonPressMask | ButtonReleaseMask);
	else
		XSelectInput(xdisplay, xwindow, KeyPressMask | KeyReleaseMask);
   
	XSetInputFocus(xdisplay, xwindow, RevertToNone, CurrentTime);

	
	Cursor xcursor = create_blank_cursor();
	
//	XDefineCursor(xdisplay, xwindow, xcursor);
	
  gc=XCreateGC(xdisplay, xwindow, 0, NULL);	
	

	if(use_x_mouse)
		XGrabPointer(xdisplay, xwindow, 0, PointerMotionMask | ButtonPressMask | ButtonReleaseMask, 
			GrabModeAsync, GrabModeAsync, xwindow, xcursor, CurrentTime);
	else
		XGrabPointer(xdisplay, xwindow, 0, 0, GrabModeAsync, GrabModeAsync,
			xwindow, xcursor, CurrentTime);
	

      int ShmMajor,ShmMinor;
      Bool ShmPixmaps;

     XShmQueryVersion(xdisplay,&ShmMajor,&ShmMinor,&ShmPixmaps);

	XShmSegmentInfo *shmseginfo = (XShmSegmentInfo *)malloc(sizeof(XShmSegmentInfo));

    memset(shmseginfo,0, sizeof(XShmSegmentInfo));

	image=XShmCreateImage(xdisplay, DefaultVisual(xdisplay, xscreen), depth, ZPixmap,
                                  NULL, shmseginfo, vid_width, vid_height);	
   
   	if(!image)
		client_shutdown();
   
   
 	shmseginfo->shmid=shmget(IPC_PRIVATE, image->bytes_per_line*
			         image->height, IPC_CREAT|0777);
	
   	if(shmseginfo->shmid < 0)
		client_shutdown();
	
  	shmseginfo->shmaddr=shmat(shmseginfo->shmid,NULL,0);
   	if(!shmseginfo->shmaddr)
		client_shutdown();
	shmseginfo->readOnly=False;
	
	switch(depth)
	{
	case 16:
		s_backbuffer = new_surface_no_buf(SURFACE_16BIT, vid_width, vid_height);
		break;
	
	case 24:
		s_backbuffer = new_surface_no_buf(SURFACE_24BITPADDING8BIT, vid_width, vid_height);
		break;
	}
	
	s_backbuffer->buf = image->data = shmseginfo->shmaddr;
	s_backbuffer->pitch = image->bytes_per_line;
	
	XShmAttach(xdisplay, shmseginfo);

	screen_config = XRRGetScreenInfo(xdisplay, RootWindow(xdisplay, xscreen));
	XRRSetScreenConfig (xdisplay, screen_config,
			   RootWindow(xdisplay, xscreen),
			   cvid_mode->index,
			   RR_Rotate_0,
			   CurrentTime);
	XRRFreeScreenConfigInfo(screen_config);
	
	if(oldwindow)
	{
		XDestroyWindow(xdisplay, oldwindow);
	}
	
	pthread_mutex_unlock(&x_mutex);
}


void vid_mode_qc(int mode)
{
	if(mode == vid_mode)
		return;
	
	if(mode >= vid_modes)
		return;
	
	vid_mode = mode;
	set_vid_mode(mode);
	
	game_resolution_change();
	
	char c;
	TEMP_FAILURE_RETRY(write(x_render_pipe[1], &c, 1));
}


void create_x_cvars()
{
	create_cvar_int("vid_mode", &vid_mode, 0);
}


void *x_thread(void *a)
{
	struct pollfd *fds;
	int fdcount;
	
	fdcount = 2;
	
	fds = calloc(sizeof(struct pollfd), fdcount);
	
	fds[0].fd = x_fd;			fds[0].events = POLLIN;
	fds[1].fd = x_kill_pipe[0];	fds[1].events = POLLIN;
	

	while(1)
	{
		if(TEMP_FAILURE_RETRY(poll(fds, fdcount, -1)) == -1)
		{
			free(fds);
			pthread_exit(NULL);
		}

		if(fds[0].revents & POLLIN)
		{			
			pthread_mutex_lock(&x_mutex);
			process_x();
			pthread_mutex_unlock(&x_mutex);
		}
		
		if(fds[1].revents & POLLIN)
		{
			free(fds);
			pthread_exit(NULL);
		}
	}
}


/*
void *x_thread(void *a)
{
	int epoll_fd = epoll_create(2);
	
	struct epoll_event ev;

	ev.events = EPOLLIN;
	ev.data.u32 = 0;
	epoll_ctl(epoll_fd, EPOLL_CTL_ADD, x_fd, &ev);

	ev.events = EPOLLIN | EPOLLET;
	ev.data.u32 = 1;
	epoll_ctl(epoll_fd, EPOLL_CTL_ADD, x_kill_pipe[0], &ev);

	while(1)
	{
		epoll_wait(epoll_fd, &ev, 1, -1);
		
		switch(ev.data.u32)
		{
		case 0:
			pthread_mutex_lock(&x_mutex);
			process_x();
			pthread_mutex_unlock(&x_mutex);
			break;
		
		case 1:
			pthread_exit(NULL);
			break;
		}
	}
}
*/


void init_x()
{
	// connect to X server 
	if(!(xdisplay = XOpenDisplay(NULL)))
		client_error("cannot connect to X server %s\n", XDisplayName(NULL));
	
	if(use_x_mouse)
	{
		XGetPointerControl(xdisplay, &accel_numerator,
		  &accel_denominator, &threshold);

		XChangePointerControl(xdisplay, 1, 1, 1, 1, 1);
	}
	
	
	xscreen = DefaultScreen(xdisplay);
	
	depth = DefaultDepth(xdisplay, xscreen);
	

	x_fd = ConnectionNumber(xdisplay);

	
//	fcntl(x_fd, F_SETFL, O_NONBLOCK);
	
	query_vid_modes();
	set_vid_mode(vid_mode);
	set_int_cvar_qc_function("vid_mode", vid_mode_qc);

	pipe(x_render_pipe);
	fcntl(x_render_pipe[0], F_SETFL, O_NONBLOCK);
	
	pipe(x_kill_pipe);
	
	pthread_create(&x_thread_id, NULL, x_thread, NULL);

	
	return;

//error:
	
//	client_error("fail\n");
}


void kill_x()
{
	char c;
	TEMP_FAILURE_RETRY(write(x_kill_pipe[1], &c, 1));
	pthread_join(x_thread_id, NULL);
	close(x_kill_pipe[0]);
	close(x_kill_pipe[1]);
	
	if(image)
	{
		XDestroyImage(image);
		image = NULL;
	}
	
	if(xwindow)
	{
		XDestroyWindow(xdisplay, xwindow);
		xwindow = 0;
	}
	
	screen_config = XRRGetScreenInfo(xdisplay, RootWindow(xdisplay, xscreen));
	
	Rotation r;
	if(original_mode != XRRConfigCurrentConfiguration(screen_config, &r))
	{
		XRRSetScreenConfig (xdisplay, screen_config,
				   RootWindow(xdisplay, xscreen),
				   original_mode,
				   RR_Rotate_0,
				   CurrentTime);
	}
	
	XRRFreeScreenConfigInfo(screen_config);
	
	if(use_x_mouse)
	{
		XChangePointerControl(xdisplay, 1, 1, accel_numerator, accel_denominator, threshold);
	}
	
	XCloseDisplay(xdisplay);
}
