
/*
 *   OpenGL Extension Module for pdp - opengl system stuff
 *   Copyright (c) by Tom Schouten <tom@zwizwa.be>
 *
 *   This program is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation; either version 2 of the License, or
 *   (at your option) any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with this program; if not, write to the Free Software
 *   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 */

/* this file contains the platform dependent opengl setup routines (glx)
   and pdp_packet_3Dcontext methods */

#include "zl/config.h"
#include "zl/xwindow.h"
#include "zl/glx.h"

//#include "pdp_packet.h"
#include "zl/3Dcontext.h"
#include <GL/gl.h>
#include <GL/glx.h>
#include <GL/glu.h>

/* all symbols are C-style */
#ifdef __cplusplus
//extern "C"
//{
#endif

// this is buggy: disabled
#define PRIVATE_CONTEXT 0


/* structure to hold the (platform dependent) gl environment setup */
typedef struct _gl_env {
    zl_glx *glx;
    zl_xdisplay *xdisplay;
    zl_3Dcontext *last_context; /* the packet that is currently rendered too (for caching) */
} gl_env;

static gl_env glx_env;

/* PDP_3DCONTEXT packet methods */

/* set/unset ogl rendering context to pbuf */
void zl_3Dcontext_set_rendering_context(zl_3Dcontext *c) {

    /* don't do a glx call if the context is still the same */
    if (glx_env.last_context == c) return;

    /* pbuffer */
    switch(c->encoding){
    case ZL_3DCONTEXT_WINDOW:
        zl_glx_makecurrent(glx_env.glx, c->drawable.xwindow);
	glx_env.last_context = c;
	break;
    case ZL_3DCONTEXT_PBUFFER:
	glx_env.last_context = NULL;
	break;
    default:
	glx_env.last_context = NULL;
	break;
    }
}

void zl_3Dcontext_unset_rendering_context(zl_3Dcontext *c) {
    if (!c) c = glx_env.last_context;
    if (!c) return;

    /* pbuffer */
    switch(c->encoding){
    case ZL_3DCONTEXT_WINDOW:
        zl_glx_makecurrent(glx_env.glx, NULL);
	break;
    case ZL_3DCONTEXT_PBUFFER:
	break;
    default:
	break;
    }
    glx_env.last_context = NULL;
}





/* window specific methods */


static void zl_3Dcontext_set_window_size(zl_3Dcontext *c, zl_xwindow *xwin) {
    c->width     =
    c->sub_width = zl_xwindow_winwidth(xwin);
    c->height    =
    c->sub_height= zl_xwindow_winheight(xwin);
}

/* resize the window */
void zl_3Dcontext_win_resize(zl_3Dcontext *c, int width, int height) {
    zl_xwindow *xwin;
    if (!c) return;
    if (ZL_3DCONTEXT_WINDOW != c->encoding) return;
    xwin = c->drawable.xwindow;
    zl_xwindow_resize(xwin, width, height);
    zl_3Dcontext_set_window_size(c, xwin);
}

void zl_3Dcontext_for_parsed_events(zl_3Dcontext *c, 
				    zl_receiver receiver,
				    void *context) {
    if (!c) return;
    if (ZL_3DCONTEXT_WINDOW != c->encoding) return;
    zl_xwindow *xwin = c->drawable.xwindow;
    zl_xwindow_route_events(xwin);
    zl_xwindow_for_parsed_events(xwin, receiver, context);
    zl_3Dcontext_set_window_size(c, xwin); // Update from window size
}

void zl_3Dcontext_win_cursor(zl_3Dcontext *c, int toggle) {
    if (!c) return;
    if (ZL_3DCONTEXT_WINDOW != c->encoding) return;
    zl_xwindow *xwin = c->drawable.xwindow;
    zl_xwindow_cursor(xwin, toggle);

}

void zl_3Dcontext_win_swapbuffers(zl_3Dcontext *c) {
    if (!c) return;
    if (ZL_3DCONTEXT_WINDOW != c->encoding) return;
    zl_xwindow *xwin = c->drawable.xwindow;
    // ZL_LOG("%p %p %p\n", c, glx_env.glx, xwin);
    zl_glx_swapbuffers(glx_env.glx, xwin);
}


/* constructors */

/* construct (or reuse) a window packet */
zl_3Dcontext *zl_3Dcontext_win_new(void) {
    zl_3Dcontext *c = calloc(1,sizeof(*c));
    zl_xwindow *xwin = zl_xwindow_new();
    c->drawable.xwindow = xwin;
    c->encoding = ZL_3DCONTEXT_WINDOW;
    zl_glx_open_on_display(glx_env.glx, xwin, glx_env.xdisplay);
    zl_xwindow_config(xwin, glx_env.xdisplay);
    zl_3Dcontext_set_window_size(c, xwin);
    return c;
}

/* pbuf constructor */
// FIXME: why is this not implemented?  Check PF code.
zl_3Dcontext *zl_3Dcontext_pbuf_new(uint32_t width  __attribute__((__unused__)),
                                    uint32_t height __attribute__((__unused__)),
                                    uint32_t depth __attribute__((__unused__))) {
    return NULL;
}



void zl_3Dcontext_free(zl_3Dcontext *c) {
    // ZL_LOG("zl_3Dcontext_free(%p,%p)\n", c, c->drawable.xwindow);
    zl_3Dcontext_unset_rendering_context(c);

    glx_env.last_context = NULL;
    switch(c->encoding){
    case ZL_3DCONTEXT_WINDOW:
        zl_xdisplay_unregister_window(glx_env.xdisplay, c->drawable.xwindow);
        zl_xwindow_free(c->drawable.xwindow);
        c->drawable.xwindow = 0;
        // FIXME: where does glXDestroyContext get called?
        // glXDestroyContext (pdp_glx_env.dpy, (GLXContext)c->context);
        break;
    case ZL_3DCONTEXT_PBUFFER:
	break;
	//glXDestroyContext(c->dpy, c->context);
	//glXDestroyPbuffer(c->dpy, c->drawable.pbuf);
    default:
	break;
    }
}


/* setup routine */
void zl_3Dcontext_glx_setup(void) {

    /* open display:
       the first display on the local machine is opened, not DISPLAY.
       since pdp_opengl is all about direct rendering, and there
       is no way to specify another display, or even close it and
       open it again, this seems to be the "least surprise" solution.
       it enables the pd interface to be displayed on another display,
       using the DISPLAY environment variable. */

    const char *display = ":0";
    if (NULL == (glx_env.xdisplay = zl_xdisplay_new(display))){
        ZL_LOG("3DContext: Can't open display %s\n", display);
	goto init_failed;
    }

    if (NULL == (glx_env.glx = zl_glx_new())) {
        ZL_LOG("3DContext: Can't create glx object\n");
	goto init_failed;
    }

    return;
  init_failed:
    // This is a bit drastic..
    exit(1);
}



#ifdef __cplusplus
//}
#endif
