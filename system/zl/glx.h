#ifndef _ZL_GLX_H_
#define _ZL_GLX_H_


/*
 *   Pure Data Packet header file: glx glue code
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


// x stuff
#define GLX_GLXEXT_PROTOTYPES

#include "zl/xwindow.h"
#include <string.h>
#include <stdbool.h>
#include <GL/gl.h>
#include <GL/glx.h>
#include <GL/glu.h>



// does not work on Darwin
//#include <GL/glxext.h>

/* glx class */
typedef struct zl_glx {

    zl_xdisplay_p xdpy; //mother display object
    int  initialized;

    /* texture */
    XVisualInfo *vis_info;
    GLXContext glx_context;
    GLuint texture;
    GLsizei tex_width;
    GLsizei tex_height;
    GLuint format;

    /* image */
    void *data;
    int image_width;
    int image_height;


} zl_glx, *zl_glx_p;


/* cons */
void zl_glx_init(zl_glx *x);
zl_glx *zl_glx_new(void);

/* des */
void zl_glx_cleanup(zl_glx* x);
void zl_glx_free(zl_glx* x);


/* Open an opengl context.
   Call this before zl_xwindow_config() */
int zl_glx_open_on_display(zl_glx *x, zl_xwindow_p w, zl_xdisplay_p d);

/* close an opengl context */
void zl_glx_close(zl_glx* x);

/* display a packet */
//void zl_glx_display_packet(zl_glx *x, xwindow *xwin, pf_packet_t packet);

/* get texture data buffer */
void *zl_glx_image_data(zl_glx *x, zl_xwindow_p xwin, 
                        unsigned int width, unsigned int height);

/* display the texture */
void zl_glx_image_display(zl_glx *x, zl_xwindow_p xwin);

/* call an OpenGL rendering function */
void zl_glx_2d_display(zl_glx *x, zl_xwindow_p xwin,
                       void (*draw)(void*,int,int), void *ctx);

/* opengl specific stuff*/
void zl_glx_swapbuffers(zl_glx *x, zl_xwindow_p xwin);
void zl_glx_makecurrent(zl_glx *x, zl_xwindow_p xwin);

void zl_glx_vsync(zl_glx *x, bool sync);

/* opengl sync module 
   this uses GLX_SGI_video_sync to sync to retrace
*/

//typedef struct {
//    xdisplay *xdpy;
//    Window dummy_window;
//   GLXContext context;
//} glx_sync_t;

// glx_sync_t *glx_sync_new(void);
// oid glx_sync_free(glx_sync_t *x);
// void glx_sync_wait(glx_sync_t *x);
// int glx_sync_open_on_display(glx_sync_t *x, xdisplay *xdpy);

#endif
