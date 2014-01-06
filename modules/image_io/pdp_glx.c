/*
 *   Pure Data Packet module.
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

#include "pdp.h"
#include "pdp_base.h"
#include "zl/xwindow.h"
#include "zl/glx.h"
#include "zl_pd/zl_pd.h"
#include "pdp.h"
#include "pdp_llconv.h"

/* initial image dimensions */
#define PDP_OGL_W 320
#define PDP_OGL_H 240

#define PDP_GLX_AUTOCREATE_RETRY 10


typedef struct pdp_glx_struct
{
    t_object x_obj;

    zl_xwindow_p x_xwin;
    zl_xdisplay_p x_xdpy;
    zl_glx *x_glx;

    t_outlet *x_outlet;

    int x_packet0;
    t_symbol *x_display;

    int  x_initialized;
    int  x_autocreate;

} t_pdp_glx;



static void pdp_glx_cursor(t_pdp_glx *x, t_floatarg f) {
    if (x->x_initialized) zl_xwindow_cursor(x->x_xwin, f);
}

static void pdp_glx_fullscreen(t_pdp_glx *x) {
    if (x->x_initialized) zl_xwindow_fullscreen(x->x_xwin);
}

static void pdp_glx_resize(t_pdp_glx* x, t_floatarg width, t_floatarg height) {
    if (x->x_initialized) zl_xwindow_resize(x->x_xwin, width, height);
}

static void pdp_glx_move(t_pdp_glx* x, t_floatarg width, t_floatarg height) {
    if (x->x_initialized) zl_xwindow_move(x->x_xwin, width, height);
}

static void pdp_glx_moveresize(t_pdp_glx* x, t_floatarg xoff, t_floatarg yoff,
                               t_floatarg width, t_floatarg height) {
    if (x->x_initialized) zl_xwindow_moveresize(x->x_xwin, xoff, yoff, width, height);
}

static void pdp_glx_tile(t_pdp_glx* x, t_floatarg xtiles, t_floatarg ytiles,
                         t_floatarg i, t_floatarg j) {
    if (x->x_initialized) zl_xwindow_tile(x->x_xwin, xtiles, ytiles, i, j);
}


static void fill_texture(t_pdp_glx *x)
{
    t_pdp *header = pdp_packet_header(x->x_packet0);
    void  *data   = pdp_packet_data  (x->x_packet0);

    int i = header->info.image.width;

    int w = header->info.image.width;
    int h = header->info.image.height;

    void *tex_data = zl_glx_image_data(x->x_glx, x->x_xwin, w, h);

    switch (header->info.image.encoding){
    case PDP_IMAGE_GREY: pdp_llconv(data,RIF_GREY______S16, tex_data, RIF_GREY______U8, w, h); break;
    case PDP_IMAGE_YV12: pdp_llconv(data,RIF_YVU__P411_S16, tex_data, RIF_RGB__P____U8, w, h); break;
    default:
        pdp_post("pdp_glx: unknown encoding %08x", header->info.image.encoding);
        return;
    }
}

static void display_packet(t_pdp_glx *x)
{
    t_pdp *header = pdp_packet_header(x->x_packet0);
    void  *data   = pdp_packet_data  (x->x_packet0);

    /* check data packet */
    if (!(header)) {
	post("pdp_glx: invalid packet header");
	return;
    }
    if (PDP_IMAGE != header->type) {
	post("pdp_glx: packet is not a PDP_IMAGE");
	return;
    }
    if ((PDP_IMAGE_YV12 != header->info.image.encoding)
	&& (PDP_IMAGE_GREY != header->info.image.encoding)) {
	post("pdp_glx: packet is not a PDP_IMAGE_YV12/GREY");
	return;
    }

    /* fill the texture with the data in the packet */
    fill_texture(x);

    /* display the new image */
    zl_glx_image_display(x->x_glx, x->x_xwin);
}





/* Poll for X11 events. */
static void outlet_event(void *context, int nb_double_args, zl_tag tag, va_list args) {
    t_pdp_glx *x = context;
    outlet_zl_float_list(x->x_outlet, nb_double_args, tag, args);
}
static void pdp_glx_poll(t_pdp_glx *x) {
    zl_xdisplay_route_events(x->x_xdpy);
    zl_xwindow_for_parsed_events(x->x_xwin, outlet_event, x);
}
/* delete all submodules */
static void _pdp_glx_cleanup(t_pdp_glx *x)
{
    if (x->x_xwin) zl_xwindow_free(x->x_xwin);
    if (x->x_glx)  zl_glx_free(x->x_glx);
    if (x->x_xdpy) zl_xdisplay_free(x->x_xdpy);
    x->x_xwin = 0;
    x->x_glx = 0;
    x->x_xdpy = 0;
    x->x_initialized = 0;
}
// this destroys the window and all the x connections
static void pdp_glx_destroy(t_pdp_glx* x)
{
    if (x->x_initialized) {
	_pdp_glx_cleanup(x);        // delete all objects
	x->x_initialized = 0;
    }
    pdp_packet_mark_unused(x->x_packet0); // delete packet
    x->x_packet0 = -1;
}


/* this creates a window (opens a dpy connection, creates glxideo and xwinow objects) */
static void pdp_glx_create(t_pdp_glx* x)
{
    int i;
    if(x->x_initialized) return;

    /* open a display */
    if (!(x->x_xdpy = zl_xdisplay_new(x->x_display->s_name))) goto exit;

    /* create a window on the display */
    x->x_xwin = zl_xwindow_new();

    /* open an glx context on the display + window */
    x->x_glx = zl_glx_new();
    if (!zl_glx_open_on_display(x->x_glx, x->x_xwin, x->x_xdpy)) goto exit;

    /* display the window */
    if (!zl_xwindow_config(x->x_xwin, x->x_xdpy)) goto exit;

    /* done */
    x->x_initialized = 1;
    return;

    /* cleanup exits */
 exit:
    post("pdp_glx: cant open display %s\n",x->x_display->s_name);
    _pdp_glx_cleanup(x);

}

static int pdp_glx_try_autocreate(t_pdp_glx *x) {
    if (x->x_autocreate){
	post("pdp_glx: autocreate window");
	pdp_glx_create(x);
	if (!(x->x_initialized)){
	    x->x_autocreate--;
	    if (!x->x_autocreate){
		post ("pdp_glx: autocreate failed %d times: disabled", PDP_GLX_AUTOCREATE_RETRY);
		post ("pdp_glx: send [autocreate 1] message to re-enable");
		return 0;
	    }
	}
	else return 1;
    }
    return 0;
}
static void pdp_glx_bang(t_pdp_glx *x)
{

    /* check if window is initialized */
    if (!(x->x_initialized)){
        if (!pdp_glx_try_autocreate(x)) return;
    }

    /* check if we can proceed */
    if (-1 == x->x_packet0) return;

    /* Display */
    display_packet(x);

    /* release the packet if there is one */
    pdp_packet_mark_unused(x->x_packet0);
    x->x_packet0 = -1;

    pdp_glx_poll(x);

}

static void pdp_glx_input_0(t_pdp_glx *x, t_symbol *s, t_floatarg f)
{

    if (s == gensym("register_ro")) pdp_packet_copy_ro_or_drop(&x->x_packet0, (int)f);
    if (s == gensym("process")) pdp_glx_bang(x);
}



static void pdp_glx_vga(t_pdp_glx *x)
{
    pdp_glx_resize(x, 640, 480);
}

static void pdp_glx_autocreate(t_pdp_glx *x, t_floatarg f)
{
  if (f != 0.0f) x->x_autocreate = PDP_GLX_AUTOCREATE_RETRY;
  else x->x_autocreate = 0;
}

static void pdp_glx_display(t_pdp_glx *x, t_symbol *s)
{
    x->x_display = s;
    if (x->x_initialized) {
	pdp_glx_destroy(x);
        pdp_glx_create(x);
    }
}

static void pdp_glx_free(t_pdp_glx *x) {
    pdp_glx_destroy(x);
}

t_class *pdp_glx_class;



void *pdp_glx_new(void)
{
    t_pdp_glx *x = (t_pdp_glx *)pd_new(pdp_glx_class);

    x->x_xwin = 0;
    x->x_xdpy = 0;

    x->x_outlet = outlet_new(&x->x_obj, &s_anything);

    x->x_packet0 = -1;
    x->x_display = gensym(":0");

    x->x_initialized = 0;
    pdp_glx_autocreate(x,1);
    return (void *)x;
}





#ifdef __cplusplus
extern "C"
{
#endif


void pdp_glx_setup(void)
{


    pdp_glx_class = class_new(gensym("pdp_glx"), (t_newmethod)pdp_glx_new,
    	(t_method)pdp_glx_free, sizeof(t_pdp_glx), 0, A_NULL);

    /* add creator for pdp_tex_win */

    class_addmethod(pdp_glx_class, (t_method)pdp_glx_bang, gensym("bang"), A_NULL);
    class_addmethod(pdp_glx_class, (t_method)pdp_glx_create, gensym("open"), A_NULL);
    class_addmethod(pdp_glx_class, (t_method)pdp_glx_create, gensym("create"), A_NULL);
    class_addmethod(pdp_glx_class, (t_method)pdp_glx_autocreate, gensym("autocreate"), A_FLOAT, A_NULL);
    class_addmethod(pdp_glx_class, (t_method)pdp_glx_destroy, gensym("destroy"), A_NULL);
    class_addmethod(pdp_glx_class, (t_method)pdp_glx_destroy, gensym("close"), A_NULL);
    class_addmethod(pdp_glx_class, (t_method)pdp_glx_move, gensym("move"), A_FLOAT, A_FLOAT, A_NULL);
    class_addmethod(pdp_glx_class, (t_method)pdp_glx_move, gensym("pos"), A_FLOAT, A_FLOAT, A_NULL);
    class_addmethod(pdp_glx_class, (t_method)pdp_glx_resize, gensym("dim"), A_FLOAT, A_FLOAT, A_NULL);
    class_addmethod(pdp_glx_class, (t_method)pdp_glx_resize, gensym("size"), A_FLOAT, A_FLOAT, A_NULL);
    class_addmethod(pdp_glx_class, (t_method)pdp_glx_display, gensym("display"), A_SYMBOL, A_NULL);
    class_addmethod(pdp_glx_class, (t_method)pdp_glx_cursor, gensym("cursor"), A_FLOAT, A_NULL);
    class_addmethod(pdp_glx_class, (t_method)pdp_glx_fullscreen, gensym("fullscreen"), A_NULL);
    class_addmethod(pdp_glx_class, (t_method)pdp_glx_moveresize, gensym("posdim"), A_FLOAT, A_FLOAT, A_FLOAT, A_FLOAT, A_NULL);
    class_addmethod(pdp_glx_class, (t_method)pdp_glx_tile, gensym("tile"), A_FLOAT, A_FLOAT, A_FLOAT, A_FLOAT, A_NULL);


    /* accept both pdp and pdp_tex packets */
    class_addmethod(pdp_glx_class, (t_method)pdp_glx_input_0, gensym("pdp"),  A_SYMBOL, A_DEFFLOAT, A_NULL);


    /* some shortcuts for the lazy */
    class_addmethod(pdp_glx_class, (t_method)pdp_glx_vga, gensym("vga"), A_NULL);

}

#ifdef __cplusplus
}
#endif



