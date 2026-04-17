/**
 * @file
 * @copyright 1998-2004 Jonathan Brown <jbrown@bluedroplet.com>
 * @license https://www.gnu.org/licenses/gpl-3.0.html
 * @homepage https://github.com/bluedroplet/emergence
 */

#define _GNU_SOURCE
#define _REENTRANT

#define __USE_X_SHAREDMEMORY__

#include <stdlib.h>
#include <stdint.h>
#include <signal.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <fcntl.h>

#include <unistd.h>
#include <sys/types.h>

#include <GL/gl.h>
#include <GL/glu.h>

#include <X11/Xlib.h>
#include <GL/glx.h>

#include "types.h"
#include "stringbuf.h"
#include "buffer.h"
#include "gsub.h"
#include "console.h"
#include "main.h"
#include "entry.h"
#include "control.h"
#include "render.h"

int x_fd;


Display *dpy;
int screen;
XVisualInfo *vi;
Colormap cmap;
Window win;
GLXContext ctx;
XEvent event;
XSetWindowAttributes attr;



/* attribute list for glx-visual */
static int attrList[] = { GLX_RGBA, GLX_DOUBLEBUFFER,
GLX_RED_SIZE, 1,
GLX_GREEN_SIZE, 1,
GLX_BLUE_SIZE, 1,
None };


void update_frame_buffer()
{
	glColorMask(1,1,1,0);
	glDepthMask(0);
	glIndexMask(0);
	glStencilMask(0);
	glDisable(GL_SCISSOR_TEST);
	glDisable(GL_ALPHA_TEST);
	glDisable(GL_STENCIL_TEST);
	glDisable(GL_DEPTH_TEST);
	glDisable(GL_DITHER);
	glDisable(GL_COLOR_LOGIC_OP);
	glDrawBuffer(GL_BACK_LEFT);
//	glScalef(2,2,2);
	glRasterPos2f(-1.0, +1.0);
	glMatrixMode(GL_MODELVIEW);
	glLoadIdentity();
	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();
	glDrawPixels(800, 600, GL_RGB, GL_UNSIGNED_SHORT_5_6_5, vid_backbuffer);
	glFlush();
	glXSwapBuffers(dpy, win);
	
	glFinish();
}


void process_x()
{
	XEvent report;
	
	XNextEvent(dpy, &report);
	
//	int CompletionType = XShmGetEventBase (dpy) + ShmCompletion;

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
}









void init_gl()
{

/* get a connection */
dpy = XOpenDisplay(0);
		if(!dpy)
		goto error;

	
	
	x_fd = ConnectionNumber(dpy);

	console_print("Getting x to generate signals: ");
	
	if(fcntl(x_fd, F_SETOWN, getpid()) == -1)
		goto error;
	
	if(fcntl(x_fd, F_SETFL, O_ASYNC) == -1)
		goto error;
	
	
	console_print("ok\n");
	
	
	
	
	
	screen = DefaultScreen(dpy);
	/* get the screen resolution */
	//dpy_width = DisplayWidth (dpy, screen);
	//dpy_height = DisplayHeight (dpy, screen);
	
	
	int numReturned;
	
	int doubleBufferAttributes[] = {
		GLX_DRAWABLE_TYPE,	GLX_WINDOW_BIT,
		GLX_RENDER_TYPE,	GLX_RGBA_BIT,
		GLX_DOUBLEBUFFER,	1, 
		GLX_RED_SIZE, 		1, 
		GLX_GREEN_SIZE,		1, 
		GLX_BLUE_SIZE, 		1, 
		None
	};
	
/*	console_print("glXChooseFBConfig: ");
	
	GLXFBConfig *fbConfigs = glXChooseFBConfig(dpy, screen, NULL, &numReturned);
	if(!fbConfigs)
		goto error;
*/	
//	console_print("glXGetFBConfigs: ");
	
//	GLXFBConfig *fbConfigs = glXGetFBConfigs(dpy, screen, &numReturned);
//	if(!fbConfigs)
//		goto error;
	
	
		console_print("glXGetVisualFromFBConfig: ");

	
	/* get a visual */
	vi = glXChooseVisual (dpy, screen, attrList);
	if(!vi)
		goto error;
	
		console_print("ok\nglXCreateContext: ");
/* create a GLX context */
	ctx = glXCreateContext(dpy, vi, 0, GL_TRUE);
	if(!ctx)
		goto error;
	
	console_print("ok\nXCreateColormap: ");
	/* create a color map */
	cmap = XCreateColormap(dpy, RootWindow(dpy, vi->screen),
	vi->visual, AllocNone);
	
	if(!cmap)
		goto error;
	
	
	/* create a window */
	attr.colormap = cmap;
	attr.border_pixel = 0;
	/* prevent the window-manager from decorating our window */
	attr.override_redirect = True;
	attr.event_mask = ExposureMask | KeyPressMask | ButtonPressMask |
	StructureNotifyMask;
	win = XCreateWindow(dpy, RootWindow(dpy, vi->screen), 0, 0, 800,
	600, 0, vi->depth, InputOutput, vi->visual, CWBorderPixel|CWColormap|CWEventMask|CWOverrideRedirect, &attr);
	/* display our window in front of everything else */
	
	
	
//	console_print("ok\glXCreateWindow: ");
//	GLXWindow glxWin = glXCreateWindow(dpy, attrList, win, NULL);
	
//	if(!glxWin)
//		goto error;
	
//	XMapWindow(dpy, win);
	
//	glXMakeContextCurrent(dpy, glxWin, glxWin, ctx);



	XSelectInput(dpy, win, KeyPressMask | KeyReleaseMask | ExposureMask);
   
	XMapRaised(dpy, win);
	
	XSetInputFocus(dpy, win, RevertToNone, CurrentTime);
	
	
	if(!glXMakeCurrent(dpy, win, ctx))
		goto error;
	
//	console_print("ok\glXIsDirect: ");
	if(!glXIsDirect(dpy, ctx))
		goto error;
	
		
	
	
	glClearColor(0.0, 0.0, 0.0, 0.0);
	glPixelZoom(1.0, -1.0);
	glShadeModel(GL_FLAT);
	glDisable(GL_TEXTURE_1D);
	glDisable(GL_TEXTURE_2D);
	glDisable(GL_TEXTURE_3D);
	glDisable(GL_LIGHTING);
	glDisable(GL_LIGHT0);
	glDisable(GL_TEXTURE_3D);
	glPixelStorei(GL_PACK_ALIGNMENT, 8);
	glPixelStorei(GL_UNPACK_ALIGNMENT, 8);
//	glPixelTransfer

       /*
         * Disable stuff that's likely to slow down glDrawPixels.
         * (Omit as much of this as possible, when you know in advance
         * that the OpenGL state will already be set correctly.)
         */
        glDisable(GL_ALPHA_TEST);
        glDisable(GL_BLEND);
        glDisable(GL_DEPTH_TEST);
        glDisable(GL_DITHER);
        glDisable(GL_FOG);
        glDisable(GL_LIGHTING);
        glDisable(GL_LOGIC_OP);
        glDisable(GL_STENCIL_TEST);
        glDisable(GL_TEXTURE_1D);
        glDisable(GL_TEXTURE_2D);
        glPixelTransferi(GL_MAP_COLOR, GL_FALSE);
        glPixelTransferi(GL_RED_SCALE, 1);
        glPixelTransferi(GL_RED_BIAS, 0);
        glPixelTransferi(GL_GREEN_SCALE, 1);
        glPixelTransferi(GL_GREEN_BIAS, 0);
        glPixelTransferi(GL_BLUE_SCALE, 1);
        glPixelTransferi(GL_BLUE_BIAS, 0);
        glPixelTransferi(GL_ALPHA_SCALE, 1);
        glPixelTransferi(GL_ALPHA_BIAS, 0);

        /*
         * Disable extensions that could slow down glDrawPixels.
         * (Actually, you should check for the presence of the proper
         * extension before making these calls.  I've omitted that
         * code for simplicity.)
         */

/*	#ifdef GL_EXT_convolution
		glDisable(GL_CONVOLUTION_1D_EXT);
		glDisable(GL_CONVOLUTION_2D_EXT);
		glDisable(GL_SEPARABLE_2D_EXT);
	#endif

	#ifdef GL_EXT_histogram
		glDisable(GL_HISTOGRAM_EXT);
	        glDisable(GL_MINMAX_EXT);
	#endif

	#ifdef GL_EXT_texture3D
	        glDisable(GL_TEXTURE_3D_EXT);
	#endif	
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
	
	sigio_process |= SIGIO_PROCESS_X;
	
	return;

error:
	
	client_error("fail\n");

}
