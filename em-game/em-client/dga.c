/**
 * @file
 * @copyright 1998-2004 Jonathan Brown <jbrown@bluedroplet.com>
 * @license https://www.gnu.org/licenses/gpl-3.0.html
 * @homepage https://github.com/bluedroplet/emergence
 */

#define _GNU_SOURCE
#define _REENTRANT

#include <stdlib.h>
#include <stdint.h>
#include <unistd.h>
#include <signal.h>
#include <string.h>
#include <fcntl.h>


#include <X11/Xlib.h>
#include <xf86dga.h>

#include "types.h"
#include "stringbuf.h"
#include "buffer.h"
#include "gsub.h"
#include "main.h"
#include "control.h"
#include "entry.h"


Display *DGA_Display;
int DGA_Screen;

uint16_t *dga_buffer;

int bps;

int x_fd;

int gbuffer = 1;

int DGA_event_base;

void process_x()
{
//	printf("process_x\n");
	
	XEvent xevent;
	
	if(XPending(DGA_Display))
	{
		XNextEvent(DGA_Display, &xevent);
	
//	int CompletionType = XShmGetEventBase (xdisplay) + ShmCompletion;

		xevent.type -= DGA_event_base;
	switch (xevent.type) {


	    case KeyPress:
		{
		XKeyEvent xkey;
//	printf("KeyPress\n");

		XDGAKeyEventToXKeyEvent(&xevent.xkey, &xkey);
		
		process_keypress(xkey.keycode - 8, 1);
		}

	    break;

	}
}

	
/*	
	
	
	switch(report.type)
	{
	case Expose:
		// unless this is the last contiguous expose,
		// don't draw the window 
		if (report.xexpose.count != 0)
			break;

		break;
	case KeyPress:
		//	XKeysymToKeycode(xdisplay, xsym);

		process_keypress(report.xkey.keycode - 8, 1);
		
//		process_keypress(((XKeyEvent*)&report)->keycode, 1);
		break;
	
	case KeyRelease:
		break;
	
	default:
	
	//	if(report.type == CompletionType)
	//		buffer_cat_uint32(msg_buf, (uint32_t)MSG_RENDER);
		
		break;
	}
	*/
}


void process_dga_events()
{
	while(XPending(DGA_Display))
		process_x();
}


void update_frame_buffer()
{
	gbuffer = (gbuffer + 1) % 2;
		
//	while(XDGAGetViewportStatus(DGA_Display, DGA_Screen));
	// copy frame to video mem
	
	
	// flip
	
	uint8_t *dst = dga_buffer;// + gbuffer * bps * 300;
	uint8_t *src = vid_backbuffer;
	
	int y;
	for(y = 0; y < 600; y++)
	{
		memcpy(dst, src, 1600);
	//	memset(dst, 0, 1600);
		dst+= bps;
		src+= 1600;
	}
	
	
//	mask_sigs();
	// wait for previous flip to occur
//	XDGASetViewport(DGA_Display, DGA_Screen, 0, gbuffer * 600, XDGAFlipRetrace);
	
	
	
	
//	memcpy(dga_buffer, vid_backbuffer, 960000);
//	memset(dga_buffer, 0xff, 960000);
	
//	XFlush(DGA_Display);
	
//	unmask_sigs();
}


void init_dga()
{
	int event_base, error_base;
	int major_version, minor_version;
	Visual *visual;
	int i, num_modes;


	mask_sigs();
	
	
	DGA_Display = XOpenDisplay(NULL);
	if ( DGA_Display == NULL ) {
		client_error("Couldn't open X11 display");
	}


/*
  XGrabKeyboard (DGA_Display, DefaultRootWindow(DGA_Display), True, 
                 GrabModeAsync,GrabModeAsync, CurrentTime);

  XGrabPointer (DGA_Display, DefaultRootWindow(DGA_Display), True, 
                ButtonPressMask,GrabModeAsync, GrabModeAsync, 
                None, None, CurrentTime);
*/
	/* Check for the DGA extension */
	if ( ! XDGAQueryExtension(DGA_Display, &event_base, &error_base) ||
	     ! XDGAQueryVersion(DGA_Display, &major_version, &minor_version) ) {
		client_error("DGA extension not available");
	}
	if ( major_version < 2 ) {
		client_error("DGA driver requires DGA 2.0 or newer");
	}
	
	DGA_event_base = event_base;

	
	DGA_Screen = DefaultScreen(DGA_Display);

//	visual = DefaultVisual(DGA_Display, DGA_Screen);
	
	/* Open access to the framebuffer */
	if ( ! XDGAOpenFramebuffer(DGA_Display, DGA_Screen) ) {
		client_error("Unable to map the video memory");
	}

	
	
	XDGAMode *modes;
	XDGADevice *mode;
	int screen_len;
	uint8_t *surfaces_mem;
	int surfaces_len;


//	int qq = 9;
	
	/* Search for a matching video mode */
	modes = XDGAQueryModes(DGA_Display, DGA_Screen, &num_modes);
	for ( i=0; i<num_modes; ++i ) {

		
		if (( modes[i].depth == 16) &&
		     (modes[i].viewportWidth == 800) &&
		     (modes[i].viewportHeight == 600) &&
		     (modes[i].visualClass == DirectColor))
		{
		//	if(--qq == 0)
				break;
		}
	}
	if ( i == num_modes ) {
		client_error("No matching video mode found");
	}

	/* Set the video mode */
	mode = XDGASetMode(DGA_Display, DGA_Screen, modes[i].num);
	XFree(modes);
	if ( mode == NULL ) {
		client_error("Unable to switch to requested mode");
	}
	

  XDGASync(DGA_Display, DGA_Screen);


	/*
	Colormap DGA_colormap = XDGACreateColormap(DGA_Display, DGA_Screen,
							mode, AllocAll);


	XDGAInstallColormap(DGA_Display, DGA_Screen, DGA_colormap);
	*/

//	memory_base = (Uint8 *)mode->data;
//	memory_pitch = mode->mode.bytesPerScanline;
	
//	printf("r: %u\n r: %u\n r: %u\n", modes[i].redMask, modes[i].greenMask, modes[i].blueMask);

	
	printf("bps: %u\n", modes[i].bytesPerScanline);
	
	bps = modes[i].bytesPerScanline;
	
	printf("imageHeight: %u\n", modes[i].imageHeight);
	
	dga_buffer = mode->data;

	/* Update for double-buffering, if we can */
	XDGASetViewport(DGA_Display, DGA_Screen, 0, 0, XDGAFlipRetrace);

	 long input_mask;
	  input_mask = (KeyPressMask | KeyReleaseMask);
	 XDGASelectInput(DGA_Display, DGA_Screen, input_mask);



/*	x_fd = ConnectionNumber(DGA_Display);

	console_print("Getting x to generate signals: ");
	
	if(fcntl(x_fd, F_SETOWN, getpid()) == -1)
		client_error("fcntl");
	
	if(fcntl(x_fd, F_SETFL, O_ASYNC) == -1)
		client_error("fcntl");
*/	
	
	vid_width = 800;
	vid_height = 600;

	vid_pitch = vid_width;
	vid_halfwidth = vid_width / 2;
	vid_heightm1 = vid_height - 1;
	vid_halfheight = vid_height / 2;
	vid_halfheightm1 = vid_halfheight - 1;
	vid_byteswidth = vid_width * 2;
	vid_bbsize = vid_byteswidth * vid_height;

	vid_backbuffer = (word*)malloc(vid_bbsize);
	
	
	unmask_sigs();
	
//	sigio_process |= SIGIO_PROCESS_X;
//	client_error("kill anyway\n");
	
	
	}
