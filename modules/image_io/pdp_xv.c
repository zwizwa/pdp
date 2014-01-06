/*
 *   Pure Data Packet module. Xvideo image packet output
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
#include "zl/xv.h"
#include "zl_pd/zl_pd.h"

#include <stdarg.h>

#define PDP_XV_AUTOCREATE_RETRY 10


typedef struct pdp_xv_struct
{
    t_object x_obj;

    zl_xdisplay_p x_xdpy;
    zl_xwindow_p x_xwin;
    zl_xv_p x_xvid;

    t_outlet *x_outlet;

    int x_packet0;
    t_symbol *x_display;

    //Display *x_dpy;

    int  x_initialized;
    int  x_autocreate;


} t_pdp_xv;


static void pdp_xv_cursor(t_pdp_xv *x, t_floatarg f)
{
    if (x->x_xwin) zl_xwindow_cursor(x->x_xwin, f);
}

/* delete all submodules */
static void _pdp_xv_cleanup(t_pdp_xv *x)
{
    if (x->x_xwin) zl_xwindow_free(x->x_xwin);
    if (x->x_xvid) zl_xv_free(x->x_xvid);
    if (x->x_xdpy) zl_xdisplay_free(x->x_xdpy);
    x->x_xwin = 0;
    x->x_xvid = 0;
    x->x_xdpy = 0;
    x->x_initialized = 0;
}

// this destroys the window and all the x connections
static void pdp_xv_destroy(t_pdp_xv* x)
{
    if (x->x_initialized) {
	
	_pdp_xv_cleanup(x);        // delete all objects

	pdp_packet_mark_unused(x->x_packet0); // delete packet
	x->x_packet0 = -1;
	x->x_initialized = 0;

    }
}


/* this creates a window (opens a dpy connection, creates xvideo and xwinow objects) */
static void pdp_xv_create(t_pdp_xv* x)
{
    int i;
    if(x->x_initialized) return;

    
    /* open a display */
    if (!(x->x_xdpy = zl_xdisplay_new(x->x_display->s_name))) goto exit;

    /* open an xv port on the display */
    x->x_xvid = zl_xv_new(FOURCC_YV12);
    if (!zl_xv_open_on_display(x->x_xvid, x->x_xdpy, 0)) goto exit;

    /* create a window on the display */
    x->x_xwin = zl_xwindow_new();
    if (!zl_xwindow_config(x->x_xwin, x->x_xdpy)) goto exit;

    /* done */
    x->x_initialized = 1;
    return;

    /* cleanup exits */
 exit:
    post("pdp_xv: cant open display %s\n",x->x_display->s_name);
    _pdp_xv_cleanup(x);

}

static int pdp_xv_try_autocreate(t_pdp_xv *x)
{

    if (x->x_autocreate){
	post("pdp_xv: autocreate window");
	pdp_xv_create(x);
	if (!(x->x_initialized)){
	    x->x_autocreate--;
	    if (!x->x_autocreate){
		post ("pdp_xv: autocreate failed %d times: disabled", PDP_XV_AUTOCREATE_RETRY);
		post ("pdp_xv: send [autocreate 1] message to re-enable");
		return 0;
	    }
	}
	else return 1;

    }
    return 0;
}

static void pdp_xv_bang(t_pdp_xv *x);

static void display_packet(zl_xv_p xvid, zl_xwindow_p xwin, int packet)
{
    t_pdp *header = pdp_packet_header(packet);
    void *data = pdp_packet_data(packet);
    t_bitmap * bm = pdp_packet_bitmap_info(packet);
    unsigned int width, height, encoding, size, nbpixels;

    /* some checks: only display when initialized and when pacet is bitmap YV12 */
    if (!header) return;
    if (!bm) return;

    width = bm->width;
    height = bm->height;
    encoding = bm->encoding;
    size = (width * height + (((width>>1)*(height>>1))<<1));
    nbpixels = width * height;

    if (PDP_BITMAP != header->type) {
        pdp_post("pdp_xv: not a bitmap");
        return;
    }
    if (PDP_BITMAP_YV12 != encoding) {
        pdp_post("pdp_xv: not a YV12 bitmap");
        return;
    }

    void *xv_data = zl_xv_image_data(xvid, xwin, width, height);
    if (xv_data) { // Always OK?
        /* Copy the data to the XvImage buffer */
        memcpy(xv_data, data, size);

        /* Send it to the adapter. */
        zl_xv_image_display(xvid, xwin);
    }

}



/* Poll for X11 events. */
static void outlet_event(void *context, int nb_double_args, zl_tag tag, va_list args) {
    t_pdp_xv *x = context;
    outlet_zl_float_list(x->x_outlet, nb_double_args, tag, args);
}
static void pdp_xv_poll(t_pdp_xv *x) {
    zl_xdisplay_route_events(x->x_xdpy);
    zl_xwindow_for_parsed_events(x->x_xwin, outlet_event, x);
}

static void pdp_xv_bang(t_pdp_xv *x)
{
    /* check if window is initialized */
    if (!(x->x_initialized)){
        if (!pdp_xv_try_autocreate(x)) return;
    }

    /* check if we can proceed */
    if (-1 == x->x_packet0) return;

    display_packet(x->x_xvid, x->x_xwin, x->x_packet0);

    /* Release packet */
    pdp_packet_mark_unused(x->x_packet0);
    x->x_packet0 = -1;

    /* Get events. */
    pdp_xv_poll(x);
}

static void pdp_xv_input_0(t_pdp_xv *x, t_symbol *s, t_floatarg f)
{
    if (s == gensym("register_ro")) {
        pdp_packet_convert_ro_or_drop(&x->x_packet0, (int)f, pdp_gensym("bitmap/yv12/*"));
    }
    if (s == gensym("process")) pdp_xv_bang(x);
}



static void pdp_xv_autocreate(t_pdp_xv *x, t_floatarg f)
{
  if (f != 0.0f) x->x_autocreate = PDP_XV_AUTOCREATE_RETRY;
  else x->x_autocreate = 0;
}

static void pdp_xv_display(t_pdp_xv *x, t_symbol *s)
{
    x->x_display = s;

    /* only create if already active */
    if (x->x_initialized){
	pdp_xv_destroy(x);
	pdp_xv_create(x);
    }
}

static void pdp_xv_movecursor(t_pdp_xv *x, float cx, float cy)
{
    if (x->x_initialized) {
        zl_xwindow_warppointer_rel(x->x_xwin, cx, cy);
    }
}

static void pdp_xv_fullscreen(t_pdp_xv *x)
{
    if (x->x_initialized)
	zl_xwindow_fullscreen(x->x_xwin);
}

static void pdp_xv_resize(t_pdp_xv* x, t_floatarg width, t_floatarg height)
{
    if (x->x_initialized)
	zl_xwindow_resize(x->x_xwin, width, height);
}

static void pdp_xv_move(t_pdp_xv* x, t_floatarg width, t_floatarg height)
{
    if (x->x_initialized)
	zl_xwindow_move(x->x_xwin, width, height);
}

static void pdp_xv_moveresize(t_pdp_xv* x, t_floatarg xoff, t_floatarg yoff, t_floatarg width, t_floatarg height)
{
    if (x->x_initialized)
	zl_xwindow_moveresize(x->x_xwin, xoff, yoff, width, height);
}

static void pdp_xv_tile(t_pdp_xv* x, t_floatarg xtiles, t_floatarg ytiles, t_floatarg i, t_floatarg j)
{
    if (x->x_initialized)
	zl_xwindow_tile(x->x_xwin, xtiles, ytiles, i, j);
}

static void pdp_xv_vga(t_pdp_xv *x)
{
    pdp_xv_resize(x, 640, 480);
}

static void pdp_xv_free(t_pdp_xv *x)
{
    pdp_xv_destroy(x);
}

t_class *pdp_xv_class;



void *pdp_xv_new(void)
{
    t_pdp_xv *x = (t_pdp_xv *)pd_new(pdp_xv_class);
    x->x_outlet = outlet_new(&x->x_obj, &s_anything);
    x->x_xwin = 0;
    x->x_xvid = 0;
    x->x_xdpy = 0;
    x->x_packet0 = -1;
    x->x_display = gensym(":0");
    x->x_xdpy = 0;
    pdp_xv_autocreate(x,1);

    return (void *)x;
}





#ifdef __cplusplus
extern "C"
{
#endif


void pdp_xv_setup(void)
{
    pdp_xv_class = class_new(gensym("pdp_xv"), (t_newmethod)pdp_xv_new,
			     (t_method)pdp_xv_free, sizeof(t_pdp_xv), 0, A_NULL);


    class_addmethod(pdp_xv_class, (t_method)pdp_xv_bang, gensym("bang"), A_NULL);
    class_addmethod(pdp_xv_class, (t_method)pdp_xv_create, gensym("open"), A_NULL);
    class_addmethod(pdp_xv_class, (t_method)pdp_xv_create, gensym("create"), A_NULL);
    class_addmethod(pdp_xv_class, (t_method)pdp_xv_autocreate, gensym("autocreate"), A_FLOAT, A_NULL);
    class_addmethod(pdp_xv_class, (t_method)pdp_xv_destroy, gensym("destroy"), A_NULL);
    class_addmethod(pdp_xv_class, (t_method)pdp_xv_destroy, gensym("close"), A_NULL);
    class_addmethod(pdp_xv_class, (t_method)pdp_xv_resize, gensym("dim"), A_FLOAT, A_FLOAT, A_NULL);
    class_addmethod(pdp_xv_class, (t_method)pdp_xv_move, gensym("move"), A_FLOAT, A_FLOAT, A_NULL);
    class_addmethod(pdp_xv_class, (t_method)pdp_xv_move, gensym("pos"), A_FLOAT, A_FLOAT, A_NULL);
    class_addmethod(pdp_xv_class, (t_method)pdp_xv_resize, gensym("size"), A_FLOAT, A_FLOAT, A_NULL);
    class_addmethod(pdp_xv_class, (t_method)pdp_xv_display, gensym("display"), A_SYMBOL, A_NULL);
    class_addmethod(pdp_xv_class, (t_method)pdp_xv_input_0, gensym("pdp"),  A_SYMBOL, A_DEFFLOAT, A_NULL);
    class_addmethod(pdp_xv_class, (t_method)pdp_xv_cursor, gensym("cursor"), A_FLOAT, A_NULL);
    class_addmethod(pdp_xv_class, (t_method)pdp_xv_movecursor, gensym("movecursor"), A_FLOAT, A_FLOAT, A_NULL);
    class_addmethod(pdp_xv_class, (t_method)pdp_xv_fullscreen, gensym("fullscreen"), A_NULL);
    class_addmethod(pdp_xv_class, (t_method)pdp_xv_poll, gensym("poll"), A_NULL);
    class_addmethod(pdp_xv_class, (t_method)pdp_xv_moveresize, gensym("posdim"), A_FLOAT, A_FLOAT, A_FLOAT, A_FLOAT, A_NULL);
    class_addmethod(pdp_xv_class, (t_method)pdp_xv_tile, gensym("tile"), A_FLOAT, A_FLOAT, A_FLOAT, A_FLOAT, A_NULL);

    /* some shortcuts for the lazy */
    class_addmethod(pdp_xv_class, (t_method)pdp_xv_vga, gensym("vga"), A_NULL);

}

#ifdef __cplusplus
}
#endif




