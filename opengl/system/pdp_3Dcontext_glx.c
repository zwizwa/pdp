
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

#include "zl_pd/zl_pd.h"  // outlet_zl_float_list()

//#include "pdp_packet.h"
#include "pdp_3Dcontext.h"
#include "pdp_internals.h"
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



static t_pdp_class *context_class;

/* PDP_3DCONTEXT packet methods */

/* set/unset ogl rendering context to pbuf */
void pdp_packet_3Dcontext_set_rendering_context(int packet)
{
    zl_3Dcontext *c = pdp_packet_3Dcontext_info(packet);
    zl_3Dcontext_set_rendering_context(c);
}

void pdp_packet_3Dcontext_unset_rendering_context(int packet)
{
    zl_3Dcontext *c = pdp_packet_3Dcontext_info(packet);
    zl_3Dcontext_unset_rendering_context(c);
}


/* cons/des */
static void _3Dcontext_clone(t_pdp *dst __attribute__((__unused__)),
                             t_pdp *src __attribute__((__unused__)))
{
    post("ERROR: clone not supported for 3Dcontext packets");
}

static void _3Dcontext_copy(t_pdp *dst __attribute__((__unused__)),
                            t_pdp *src __attribute__((__unused__)))
{
    post("ERROR: copy not supported for 3Dcontext packets");
}

static void _3Dcontext_reinit(t_pdp *dst __attribute__((__unused__)))
{
    /* leave the packet as is */
}
static void _3Dcontext_cleanup(t_pdp *dst) {
    zl_3Dcontext *c = (void *)(&dst->info.raw);
    zl_3Dcontext_free(c);
}


/* setup packet methods */
static void _3Dcontext_init_methods(t_pdp *header)
{
    header->theclass = context_class;
    header->flags = PDP_FLAG_DONOTCOPY;
}



/* window specific methods */

/* resize the window */
void pdp_packet_3Dcontext_win_resize(int packet, int width, int height)
{
    zl_3Dcontext *c = pdp_packet_3Dcontext_info(packet);
    zl_3Dcontext_win_resize(c, width, height);
}

static void outlet_event(void *context, int nb_double_args, zl_tag tag, va_list args) {
    t_outlet *outlet = context;
    outlet_zl_float_list(outlet, nb_double_args, tag, args);
}

void pdp_packet_3Dcontext_event_out(int packet, t_outlet *outlet) {
    zl_3Dcontext *c = pdp_packet_3Dcontext_info(packet);
    if (!c) return;
    zl_3Dcontext_for_parsed_events(c, outlet_event, outlet);
}

void pdp_packet_3Dcontext_win_cursor(int packet, bool toggle)
{
    zl_3Dcontext *c = pdp_packet_3Dcontext_info(packet);
    zl_3Dcontext_win_cursor(c, toggle);
}

void pdp_packet_3Dcontext_win_swapbuffers(int packet)
{
    zl_3Dcontext *c = pdp_packet_3Dcontext_info(packet);
    zl_3Dcontext_win_swapbuffers(c);
}




/* constructors */

/* construct (or reuse) a window packet */
int pdp_packet_new_3Dcontext_win(void)
{
    /* $$$FIXME: this assumes packet can't be reused */
    int p = pdp_packet_new(PDP_3DCONTEXT, 0);
    t_pdp *header = pdp_packet_header(p);
    if (!header) return -1; /* pool full ? */
    zl_3Dcontext *c = zl_3Dcontext_win_new();
    zl_3Dcontext_wrapper *cw = (void*)&header->info.raw;
    cw->c = c;

    /* init packet methods */
    _3Dcontext_init_methods(header);

    /* init header */
    header->desc = pdp_gensym("3Dcontext/window");
    header->flags = PDP_FLAG_DONOTCOPY;
    return p;
}

/* pbuf constructor */
int pdp_packet_new_3Dcontext_pbuf(u32 width  __attribute__((__unused__)),
                                  u32 height __attribute__((__unused__)),
                                  u32 depth  __attribute__((__unused__)))
{
    post("ERROR: 3Dcontext/pbuffer packets not implemented");
    return -1;
}

/* this is a notifier sent when the processing thread which
   executes gl commands is changed. we need to release the current context
   before another thread can take it. */
void pdp_3Dcontext_prepare_for_thread_switch(void)
{
    zl_3Dcontext_unset_rendering_context(NULL); // unset current
}


static void pdp_3Dcontext_glx_setup_in_thread(void) {
    /* init xlib for thread usage */
    if (!XInitThreads()){
    	post("pdp_3Dcontext_glx_setup_in_thread: can't init Xlib for thread usage.");
        return;
    }
    else {
        // post("pdp_3Dcontext_glx_setup_in_thread: OK");
    }
    zl_3Dcontext_glx_setup();
}


/* setup routine */
void pdp_3Dcontext_glx_setup(void)
{
    /* run the setup routine in the procqueue thread, and wait for it to finish */
    t_pdp_procqueue *q = pdp_opengl_get_queue();
    pdp_procqueue_add(q, 0, pdp_3Dcontext_glx_setup_in_thread, 0, 0);
    pdp_procqueue_flush(q);
    // post("pdp_3Dcontext_glx_setup: pdp_procqueue_flush() OK");

    /* setup class object */
    context_class = pdp_class_new(pdp_gensym("3Dcontext/*"), 0);
    context_class->cleanup = _3Dcontext_cleanup;
    context_class->wakeup = _3Dcontext_reinit;
    //context_class->clone = _3Dcontext_clone;
    context_class->copy = _3Dcontext_copy;

    /* setup conversion programs: NOT IMPLEMENTED */
    return;

}



#ifdef __cplusplus
//}
#endif
