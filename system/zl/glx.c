/*
 *   Pure Data Packet support file: glx glue code
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

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <stdbool.h>
#include <zl/config.h>

//#include <pf/typedefs.h>
#define PRIVATE
#include "glx.h"
//#include <pf/image.h>
//#include <pf/packet.h>
//#include <pf/packet_imp.h>
//#include <pf/stream.h>

#define D if (0)

/* ------------------------- GLX_MESA_swap_control ------------------------- */
#ifndef GLX_MESA_swap_control
#define GLX_MESA_swap_control 1
typedef int ( * PFNGLXGETSWAPINTERVALMESAPROC) (void);
typedef int ( * PFNGLXSWAPINTERVALMESAPROC) (unsigned int interval);
#define glXGetSwapIntervalMESA GLXEW_GET_FUN(__glewXGetSwapIntervalMESA)
#define glXSwapIntervalMESA GLXEW_GET_FUN(__glewXSwapIntervalMESA)
#define GLXEW_MESA_swap_control GLXEW_GET_VAR(__GLXEW_MESA_swap_control)
#endif /* GLX_MESA_swap_control */

static zl_glx *current_context = 0;

/* cons */
void zl_glx_init(zl_glx *x)
{
    memset(x, 0, sizeof(*x));
    x->tex_width = 1; // smallest nonzero (for mul 2) dummy size
    x->tex_height = 1;
    x->format = GL_RGB; // FIXME: This is never set
}

zl_glx *zl_glx_new(void){
    zl_glx *x = malloc(sizeof(*x));
    zl_glx_init(x);
    return x;
}

/* des */
void zl_glx_cleanup(zl_glx* x)
{
    // XEvent e;

    if (x->initialized){
        ZL_LOG("glXDestroyContext()\n");
	glXDestroyContext(x->xdpy->dpy, x->glx_context);
	x->xdpy = 0;
	x->initialized = 0;
    }
}
void zl_glx_free(zl_glx* x){
    if (glIsTexture(x->texture)){
        glDeleteTextures(1, &(x->texture));
        x->texture = 0;
    }
    zl_glx_cleanup(x);
    free(x);
}


void zl_glx_swapbuffers(zl_glx *x, zl_xwindow_p xwin)
{
    // ZL_LOG("swapbuffers %x %x", x, xwin);
    // ZL_LOG("glXSwapBuffers(%p,%p)\n",x->xdpy->dpy, xwin->win);
    glXSwapBuffers(x->xdpy->dpy, xwin->win);
}

// FIXME: pbufs?
void zl_glx_makecurrent(zl_glx *x, zl_xwindow_p xwin)
{
    if (NULL == xwin) {
	glXMakeCurrent(x->xdpy->dpy, None, x->glx_context);
    }
    else if (x != current_context){

	D fprintf(stderr, "glXMakeCurrent(%p,%p,%p)\n", 
                  (void*)x->xdpy->dpy, 
                  (void*)xwin->win, 
                  (void*)x->glx_context);
	glXMakeCurrent(x->xdpy->dpy, xwin->win, x->glx_context);

	current_context = x;

    }
}


void *zl_glx_image_data(zl_glx* x, zl_xwindow_p xwin,
			unsigned int w, unsigned int h){

    if (!x->initialized) return 0;

    x->image_width = w;
    x->image_height = h;
    
    int tex_width = x->tex_width;
    int tex_height = x->tex_height;

    /* set window context current
       just to make sure we don't conflict with other contexts */
    zl_glx_makecurrent(x, xwin);

    /* setup texture if necesary */
    if ((x->image_width > tex_width) || (x->image_height > tex_height)) {
	while (x->image_width > tex_width) tex_width *= 2;
	while (x->image_height > tex_height) tex_height *= 2;
	if (!glIsTexture(x->texture)){glGenTextures(1, &(x->texture));}
	glBindTexture(GL_TEXTURE_2D, x->texture);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, tex_width, tex_height, 0, GL_RGBA, GL_UNSIGNED_BYTE, 0);
	x->tex_width = tex_width;
	x->tex_height = tex_height;
	fprintf(stderr, "zl_glx_image_data: creating texture %dx%d", tex_width, tex_height);
	if (x->data) free (x->data);
	x->data = malloc(4 * x->image_width * x->image_height);
    }
    else {
	glBindTexture(GL_TEXTURE_2D, x->texture);
    }
    return x->data;
}



void display_texture(void *ctx, int w, int h) {
    zl_glx *x = ctx;
    /* upload subtexture */
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, x->image_width, x->image_height, 
                    x->format, GL_UNSIGNED_BYTE, x->data);

    /* enable default texture */
    glEnable(GL_TEXTURE_2D);
    glBindTexture(GL_TEXTURE_2D, x->texture);
    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);
    glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);

    /* get fractional dimensions */
    float fx = (float)x->image_width / (float)x->tex_width;
    float fy = (float)x->image_height / (float)x->tex_height;

    /* display texture */
    glBegin(GL_QUADS);
    {
        glTexCoord2f(fx, fy);
        glVertex2i(w,0);
        glTexCoord2f(fx, 0);
        glVertex2i(w, h);
        glTexCoord2f(0.0, 0.0);
        glVertex2i(0, h);
        glTexCoord2f(0, fy);
        glVertex2i(0,0);
    }
    glEnd();
}


void zl_glx_2d_display(zl_glx *x, zl_xwindow_p xwin,
                       void (*draw)(void*,int,int), void *ctx) {

    /* set window context current
       just to make sure we don't conflict with other contexts */
    zl_glx_makecurrent(x, xwin);

    /* setup viewport, projection and modelview */
    glViewport(0, 0, xwin->winwidth, xwin->winheight);
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    gluOrtho2D(0.0, xwin->winwidth, 0.0, xwin->winheight);
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();

    /* user drawing routine */
    draw(ctx, xwin->winwidth, xwin->winheight);

    glFlush();
    glXSwapBuffers(x->xdpy->dpy, xwin->win);

}

void zl_glx_image_display(zl_glx *x, zl_xwindow_p xwin) {
    zl_glx_2d_display(x, xwin, display_texture, x);
}



int GLXExtensionSupported(Display *dpy, const char *extension) {
    const char *extensionsString;
    char *pos;
    extensionsString = glXQueryExtensionsString(dpy, DefaultScreen(dpy));
    pos = strstr(extensionsString, extension);
    return pos!=NULL && (pos==extensionsString || pos[-1]==' ') &&
        (pos[strlen(extension)]==' ' || pos[strlen(extension)]=='\0');
}


// VBLANK config, template from http://forum.openframeworks.cc/index.php?topic=561.0
typedef GLvoid (*glXSwapIntervalSGIFunc) (GLint);
typedef GLvoid (*glXSwapIntervalMESAFunc) (GLint);


#define TARGET_LINUX
void zl_glx_vsync(zl_glx *x, bool sync) {
#ifdef TARGET_WIN32
    if (sync) {
        if (GLEE_WGL_EXT_swap_control) wglSwapIntervalEXT (1);
    } else {
        if (GLEE_WGL_EXT_swap_control) wglSwapIntervalEXT (0);
    }
#endif
#ifdef TARGET_OSX
    long sync = sync ? 1 : 0;
    CGLSetParameter (CGLGetCurrentContext(), kCGLCPSwapInterval, &sync);
#endif
#ifdef TARGET_LINUX
    int interval = sync ? 2 : 0;
    glXSwapIntervalSGIFunc glXSwapIntervalSGI = 0;
    glXSwapIntervalMESAFunc glXSwapIntervalMESA = 0;

    if (GLXExtensionSupported(x->xdpy->dpy, "GLX_MESA_swap_control")) {
        glXSwapIntervalMESA = (glXSwapIntervalMESAFunc)
            glXGetProcAddressARB((const GLubyte *)"glXSwapIntervalMESA");
        if (glXSwapIntervalMESA) {
            ZL_LOG("glXSwapIntervalMESA(%d)", interval);
            glXSwapIntervalMESA (interval);
        }
        else {
            ZL_LOG("Could not get glXSwapIntervalMESA()\n");
        }
    }
    else if (GLXExtensionSupported(x->xdpy->dpy, "GLX_SGI_swap_control")) {
        glXSwapIntervalSGI = (glXSwapIntervalSGIFunc)
            glXGetProcAddressARB((const GLubyte *)"glXSwapIntervalSGI");
        if (glXSwapIntervalSGI) {
            ZL_LOG("glXSwapIntervalSGI(%d)", interval, glXSwapIntervalSGI);
            glXSwapIntervalSGI (interval);
        }
        else {
            ZL_LOG("Could not get glXSwapIntervalSGI()\n");
        }
    }
    else {
        ZL_LOG("can't change vblank settings");
    }
#endif
}

int zl_glx_open_on_display(zl_glx *x, zl_xwindow_p w, zl_xdisplay_p d)
{
    static int vis_attr[] = {GLX_RGBA, 
			     GLX_RED_SIZE, 4, 
			     GLX_GREEN_SIZE, 4,
			     GLX_BLUE_SIZE, 4, 
			     GLX_DEPTH_SIZE, 16, 
			     GLX_DOUBLEBUFFER, None};


    /* store mother */
    x->xdpy = d;

    /* create a glx visual */
    if (!(x->vis_info = glXChooseVisual(x->xdpy->dpy,
					x->xdpy->screen,
					vis_attr))){
	fprintf(stderr, "zl_glx_open_on_display: can't find appropriate visual\n");
	goto error;
    }
    else {
	D fprintf(stderr, "zl_glx_open_on_display: using visual 0x%x\n", 
		(unsigned int)x->vis_info->visualid);
    }

    /* create the rendering context */
    if (!(x->glx_context = glXCreateContext(x->xdpy->dpy,
					    x->vis_info,
					    0 /*share list*/,
					    GL_TRUE))){
	fprintf(stderr, "zl_glx_open_on_display: can't create render context\n");
	goto error;
    }
    else {
	D fprintf(stderr, 
                  "zl_glx_open_on_display: created render "
                  "context %p on screen %d\n", 
                  (void*)x->glx_context, x->xdpy->screen);
    }

    
    /* following is from glxgears.c */

    XSetWindowAttributes swa;
    Colormap cmap;
    /* create an X colormap since probably not using default visual */
    cmap = XCreateColormap(d->dpy, RootWindow(d->dpy, x->vis_info->screen),
			   x->vis_info->visual, AllocNone);
    swa.colormap = cmap;
    swa.border_pixel = 0;
    swa.event_mask = ExposureMask | ButtonPressMask | StructureNotifyMask;

    /* Create a window using this visual.  This will play nice with
       zl_xwindow: it will not create a window if there is already one
       there. */
    w->win = XCreateWindow(
	d->dpy,
	RootWindow(d->dpy, x->vis_info->screen), 
	w->winxoffset, w->winyoffset, 
	w->winwidth, w->winheight, 0,
	x->vis_info->depth,
	InputOutput, x->vis_info->visual,
	CWBorderPixel | CWColormap | CWEventMask, &swa);



    x->initialized = 1;

    return 1;

  error:
    
    return 0;
}





void handle_error(char *msg){
    fprintf(stderr, "%s\n", msg);
    exit(1);
}





