/*
 *   Pure Data Packet system module. - x window glue code (fairly tied to pd and pf)
 *   Copyright (c) by Tom Schouten <tom@zwizwa.be>
 *
 *   This program is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as publishd by
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


/* TODO:
   - rename to zl_ prefix
   - merge with libprim
   - merge with pdp_xv
*/



// this code is fairly tied to pd and pf. serves mainly as reusable glue code
// for pf_xv, pf_glx, pf_3d_windowcontext, ...


#include <string.h>

//#include <pf/stream.h> // no longer depending on pf
//#include <pf/debug.h>
//#include <pf/symbol.h>
//#include <pf/list.h>

#include "config.h"
#include <stdlib.h>
#include <stdio.h>
#include <X11/Xutil.h>

#define PRIVATE
#include "xwindow.h"


#define D if(0)

// TODO: clean this up
#define ASSERT(x) \
    if (!(x)) {                                    \
        fprintf(stderr, "%s:%d: ASSERT(%s)\n",     \
                __FILE__, __LINE__, #x);           \
        exit(1);                                   \
    }                                              \
    

/************************************* ZL_XDISPLAY ************************************/


/* For use in libprim */
#ifdef HAVE_LEAF
#include <leaf/port.h>
static int zl_xdisplay_write(zl_xdisplay *x, port *p) {
    return port_printf(p, "#<zl_xdisplay:%p>", x);
}
zl_xdisplay_class *zl_xdisplay_type(void) {
    static zl_xdisplay_class *type = NULL;
    if (!type) {
        type = calloc(1, sizeof(*type));
        leaf_class_init((leaf_class*)type,
                        (leaf_free_m)zl_xdisplay_free,
                        (leaf_write_m)zl_xdisplay_write);
    }
    return type;
}
static int zl_xwindow_write(zl_xdisplay *x, port *p) {
    return port_printf(p, "#<zl_xwindow:%p>", x);
}
zl_xwindow_class *zl_xwindow_type(void) {
    static zl_xwindow_class *type = NULL;
    if (!type) {
        type = calloc(1, sizeof(*type));
        leaf_class_init((leaf_class*)type,
                        (leaf_free_m)zl_xwindow_free,
                        (leaf_write_m)zl_xwindow_write);
    }
    return type;
}
#else
zl_xdisplay_class *zl_xdisplay_type(void) { return NULL; }
zl_xwindow_class *zl_xwindow_type(void) { return NULL; }
#endif

int zl_xdisplay_errorhandler(Display *dpy, XErrorEvent *ev){
    fprintf(stderr, "zl_xdisplay_errorhandler\n");
    return 0xdeadf00d;
}

zl_xdisplay_p zl_xdisplay_new(const char *dpy_string) {
    zl_xdisplay_p d = malloc(sizeof(*d));

    /* open display */
    d->type = zl_xdisplay_type();
    if (!(d->dpy = XOpenDisplay(dpy_string))) {
        ZL_LOG("x11: can't open display %s\n", dpy_string);
        free(d);
        return NULL;
    }
    else {
        //ZL_LOG("x11: open %s OK\n", dpy_string);
    }

    /* install error handler */
    //XSetErrorHandler(&zl_xdisplay_errorhandler);

    d->windowlist = buf_new(16);
    d->screen = DefaultScreen(d->dpy);
    d->dragbutton = -1;
    D fprintf (stderr, "x11: using display %s\n", dpy_string);
    return d;
}



void zl_xdisplay_free(zl_xdisplay_p d) {
    XCloseDisplay(d->dpy);
    ASSERT(0 == d->windowlist->elements); // make sure there are no dangling zl_xwindow objects
    buf_free(d->windowlist);
    free(d);
}

/* some private members */
// static int _windowset_contains(zl_xdisplay_p d, zl_xwindow *w){
//    return (buf_lookup(d->windowlist, w) >= 0);
//}


void zl_xdisplay_register_window(zl_xdisplay_p d, zl_xwindow_p w)
{
    //pf_post("zl_xdisplay: registering window %p", w);
    buf_add_to_set(d->windowlist, w);
    
    //if (!_windowset_contains(d, w)) _windowset_add(d, w);
}
void zl_xdisplay_unregister_window(zl_xdisplay_p d, zl_xwindow_p w)
{
    //pf_post("zl_xdisplay: unregistering window %p", w);
    buf_remove(d->windowlist, w);
    //if (_windowset_contains(d, w)) _windowset_remove(d, w);
}


// this reads all events from display and passes them on to client
// just raw x events. client needs to parse events.. since this is
// shared code, we keep it as simple as possible

/* These are const strings to enable:
   - Identification by pointer using tag == ZL_DISPLAY_EV_XXX
   - Conversion to hashed symbol if target language has a symbol type */




void zl_xdisplay_route_events(zl_xdisplay_p d) {

    zl_xwindow_p w;
    int i;
    XEvent *e = 0;

    while (XPending(d->dpy)){
        if (!e) e = malloc(sizeof(XEvent));
        XNextEvent(d->dpy, e);        
	// ZL_LOG("event %x", e);
        // find zl_xwindow instance

        for (i = 0; i<d->windowlist->elements; i++){
            w = (zl_xwindow_p )d->windowlist->data[i];
            if (w->win == e->xany.window){
                if (e->type == ConfigureNotify){
                    w->winwidth  = e->xconfigure.width;
                    w->winheight = e->xconfigure.height;

		    // ZL_LOG("ConfigureNotify %d %d\n", w->winwidth, w->winheight);

                    /* Workaround for weird window sizes 42592x42379
                       that cause BadValue for XV. */
                    int max = 4000;
                    if (w->winwidth > max) w->winwidth  = max;
                    if (w->winheight> max) w->winheight = max;
                }
                zl_xwindow_queue_event(w, e);
		// ZL_LOG("queue w:%x e:%x", w, e);
                e = 0; // moved
            }
        }
    }

    if (e) free(e);
}



/************************************* ZL_XWINDOW ************************************/

void zl_xwindow_queue_event(zl_xwindow_p x, XEvent *e){
    buf_add(x->events, e);
}

void zl_xwindow_for_events(zl_xwindow_p xwin, zl_zwindow_event_fn handle, void *context) {
    void *e;
    while ((e = stack_pop(xwin->events))) {
        handle(context, e);
        free(e);
    }
}

/* Define the const tag strings. */
#define _ZL_XDISPLAY_EV_IMPL(name) zl_tag ZL_XDISPLAY_EV(name) = #name;
ZL_XDISPLAY_EV_LIST(_ZL_XDISPLAY_EV_IMPL)

/* There are probably better ways to do this, but we don't have access
   to the integer, only the tag, so it has to be a switch stmt. */
#define BUT_TAG_N(n, tagn, name)                \
    case Button##n:                             \
    tagn = ZL_XDISPLAY_EV(name##n);             \
    break;
#define BUT_TAG(ev, tagn, name)  do {          \
switch(ev) {                                   \
default:                                       \
    ZL_FOR_BUTTON(BUT_TAG_N, tagn, name)       \
}} while(0)



/* Parse the XEvent to something more useful.  The "useful" here means
   in the style used in PDP: tagged lists + button/drag tracking. */
void zl_xwindow_for_parsed_events(zl_xwindow_p xwin,
                                  zl_receiver r,
                                  void *c) {
    XEvent *e;
    /* set dim scalers */
    const double inv_x = 1.0f / (double)(xwin->winwidth);
    const double inv_y = 1.0f / (double)(xwin->winheight);



    const int bmask = Button1Mask
	| Button2Mask
	| Button3Mask
	| Button4Mask
	| Button5Mask;

    while ((e = stack_pop(xwin->events))) {

	// ZL_LOG("pop w:%x e:%x", xwin, e);

        /* handle event */
        double x = inv_x * (double)e->xbutton.x;
        double y = inv_y * (double)e->xbutton.y;

	switch(e->type) {
	case ClientMessage:
	    if ((Atom)e->xclient.data.l[0] == xwin->WM_DELETE_WINDOW) {
                zl_send(r, c, 0, ZL_XDISPLAY_EV(close));
	    }
	    break;
	case KeyPress:
	case KeyRelease:
            zl_send(r, c, 1, e->type == KeyPress ?
                ZL_XDISPLAY_EV(keypress) :
                ZL_XDISPLAY_EV(keyrelease),
                (double)e->xkey.keycode);
            break;
	case ButtonPress:
	case ButtonRelease:
        {
            zl_tag tag;
            zl_tag tagn;
            if (e->type == ButtonPress) {
                tag = ZL_XDISPLAY_EV(press);
                BUT_TAG(e->xbutton.button, tagn, press);
            }
            else {
                tag = ZL_XDISPLAY_EV(release);
                BUT_TAG(e->xbutton.button, tagn, press);
            }

	    /* send generic event */
            zl_send(r, c, 2, tag,  x, y);
            zl_send(r, c, 2, tagn, x, y);

            /* Keep track of button for MotionNotify. */
            xwin->lastbut = e->xbutton.button;

	    break;
        }
	case MotionNotify: {
	    if (e->xbutton.state & bmask){
		/* button is down: it is a drag event */
                zl_tag tag = ZL_XDISPLAY_EV(drag);
                zl_tag tagn;
                BUT_TAG(xwin->lastbut, tagn, drag);
                zl_send(r, c, 2, tag,  x, y);
                zl_send(r, c, 2, tagn, x, y);
	    }
	    else {
                zl_tag tag = ZL_XDISPLAY_EV(motion);
                zl_send(r, c, 2, tag,  x, y);
	    }
            break;
        }
        // FIXME: should these even get here?
        case UnmapNotify: {
            break;
        }
        case ConfigureNotify: {
            break;
        }
        default:
            ZL_LOG("unknown event type %d\n", e->type);
            break;
        }
        free(e);
    }
}

void zl_xwindow_drop_events(zl_xwindow_p x) {
    void handle(void *x, XEvent *e) {}
    zl_xwindow_for_events(x, handle, NULL);
}

void zl_xwindow_warppointer(zl_xwindow_p xwin, int x, int y)
{
    if (xwin->initialized) {
        XWarpPointer(xwin->xdisplay->dpy, None, xwin->win, 0, 0, 0, 0, x, y);
    }
}
void zl_xwindow_warppointer_rel(zl_xwindow_p xwin, float x, float y) {
    if (xwin->initialized) {
        x *= xwin->winwidth;
        y *= xwin->winheight;
	zl_xwindow_warppointer(xwin, x, y);
    }
}





void zl_xwindow_overrideredirect(zl_xwindow_p xwin, int b)
{
    XSetWindowAttributes new_attr;
    new_attr.override_redirect = b ? True : False;
    XChangeWindowAttributes(xwin->xdisplay->dpy, xwin->win, CWOverrideRedirect, &new_attr);
    //XFlush(xwin->xdisplay->dpy);

}


void zl_xwindow_moveresize(zl_xwindow_p xwin, int xoffset, int yoffset, int width, int height)
{

    if ((width > 0) && (height > 0)){
        xwin->winwidth = width;
        xwin->winheight = height;
        xwin->winxoffset = xoffset;
        xwin->winyoffset = yoffset;

        if (xwin->initialized){
            XMoveResizeWindow(xwin->xdisplay->dpy, xwin->win, xoffset, yoffset, width,  height);
            XFlush(xwin->xdisplay->dpy);
        }
    }
}


void zl_xwindow_fullscreen(zl_xwindow_p xwin)
{
    XWindowAttributes rootwin_attr;


    /* hmm.. fullscreen and xlib the big puzzle..
       if it looks like a hack it is a hack. */

    if (xwin->initialized){
        
        XGetWindowAttributes(xwin->xdisplay->dpy, RootWindow(xwin->xdisplay->dpy, xwin->xdisplay->screen), &rootwin_attr );

        //zl_xwindow_overrideredirect(xwin, 0);
        zl_xwindow_moveresize(xwin, 0, 0, rootwin_attr.width,  rootwin_attr.height);
        //zl_xwindow_overrideredirect(xwin, 1);
        //XRaiseWindow(xwin->xdisplay->dpy, xwin->win);
        //zl_xwindow_moveresize(xwin, 0, 0, rootwin_attr.width,   rootwin_attr.height);
        //zl_xwindow_overrideredirect(xwin, 0);

 


    }
}


void zl_xwindow_tile(zl_xwindow_p xwin, int x_tiles, int y_tiles, int i, int j)
{
    XWindowAttributes rootwin_attr;
    // XSetWindowAttributes new_attr;

    if (xwin->initialized){
        int tile_w;
        int tile_h;
        XGetWindowAttributes(xwin->xdisplay->dpy, RootWindow(xwin->xdisplay->dpy, xwin->xdisplay->screen), &rootwin_attr );

        tile_w = rootwin_attr.width / x_tiles;
        tile_h = rootwin_attr.height / y_tiles;

        xwin->winwidth = (x_tiles-1) ? rootwin_attr.width - (x_tiles-1)*tile_w : tile_w;
        xwin->winheight = (y_tiles-1) ? rootwin_attr.height - (y_tiles-1)*tile_h : tile_h;
        xwin->winxoffset = i * tile_w;
        xwin->winyoffset = j * tile_h;

        //new_attr.override_redirect = True;
        //XChangeWindowAttributes(xwin->xdisplay->dpy, xwin->win, CWOverrideRedirect, &new_attr );
        XMoveResizeWindow(xwin->xdisplay->dpy, xwin->win, xwin->winxoffset, xwin->winyoffset, xwin->winwidth,  xwin->winheight);

    }
}

/* resize window */
void zl_xwindow_resize(zl_xwindow_p xwin, int width, int height)
{
    if ((width > 0) && (height > 0)){
        xwin->winwidth = width;
        xwin->winheight = height;
        if (xwin->initialized){
            XResizeWindow(xwin->xdisplay->dpy, xwin->win,  width,  height);
            XFlush(xwin->xdisplay->dpy);
        }
    }
    //_zl_xwindow_moveresize(xwin, xwin->winxoffset, xwin->winyoffset, width, height);
}

/* move window */
void zl_xwindow_move(zl_xwindow_p xwin, int xoffset, int yoffset)
{
    zl_xwindow_moveresize(xwin, xoffset, yoffset, xwin->winwidth, xwin->winheight);
}




/* set an arbitrary cursor image */
void zl_xwindow_cursor_image(zl_xwindow_p xwin, char *data, int width, int height)
{
    if (!xwin->initialized) return;

    Cursor cursor;
    Pixmap pm;
    XColor fg;
    XColor bg;

    fg.red = fg.green = fg.blue = 0xffff;
    bg.red = bg.green = bg.blue = 0x0000;

    pm = XCreateBitmapFromData(xwin->xdisplay->dpy, xwin->win, data, width, height);
    cursor = XCreatePixmapCursor(xwin->xdisplay->dpy, pm, pm, &fg,
                                 &bg, width/2, height/2);
    XFreePixmap(xwin->xdisplay->dpy, pm);
    XDefineCursor(xwin->xdisplay->dpy, xwin->win,cursor);
}

/* enable / disable cursor */
void zl_xwindow_cursor(zl_xwindow_p xwin, int i){
    xwin->cursor = i;
    if (!xwin->initialized) return;
    if (i == 0) {
        char data[] = {0};
        zl_xwindow_cursor_image(xwin, data, 1, 1);
    }
    else
        XUndefineCursor(xwin->xdisplay->dpy, xwin->win);
}


void zl_xwindowitle(zl_xwindow_p xwin, char *title)
{
    if (xwin->initialized)
        XStoreName(xwin->xdisplay->dpy, xwin->win, title);
}


/* create zl_xwindow */
int zl_xwindow_config(zl_xwindow_p xwin, zl_xdisplay_p d) 
{
    XEvent e;
    // unsigned int i;

    /* check if already opened */
    if(  xwin->initialized ){
        fprintf(stderr, "x11: window already created\n");
        goto exit;
    }

    xwin->xdisplay = d;
    ASSERT(xwin->xdisplay);

    /* create a window if xwin doesn't contain one yet:
       glx init will create its own window */
    if (!xwin->win){
        xwin->win = XCreateSimpleWindow(
            xwin->xdisplay->dpy, 
            RootWindow(xwin->xdisplay->dpy, xwin->xdisplay->screen), 
            xwin->winxoffset, xwin->winyoffset, 
            xwin->winwidth, xwin->winheight, 0, 
            BlackPixel(xwin->xdisplay->dpy, xwin->xdisplay->screen),
            BlackPixel(xwin->xdisplay->dpy, xwin->xdisplay->screen));

        D fprintf(stderr, "created window %x on screen %d\n", 
                  (unsigned int)xwin->win, xwin->xdisplay->screen);
    }
    else {
        D fprintf(stderr, "reused window %x on screen %d\n", 
                  (unsigned int)xwin->win, xwin->xdisplay->screen);
    }


    /* enable handling of close window event */
    xwin->WM_DELETE_WINDOW = XInternAtom(xwin->xdisplay->dpy,
                                         "WM_DELETE_WINDOW", True);
    (void)XSetWMProtocols(xwin->xdisplay->dpy, xwin->win,
                          &xwin->WM_DELETE_WINDOW, 1);

    if(!(xwin->win)){
        /* clean up mess */
        fprintf(stderr, "x11: could not create window.\n");
        //XCloseDisplay(xwin->xdisplay->dpy); NOT OWNER
        xwin->xdisplay = 0;
        xwin->initialized = 0;
        goto exit;
    }

    /* select input events */
    XSelectInput(xwin->xdisplay->dpy, xwin->win, 
                 StructureNotifyMask  
                 | KeyPressMask 
                 | KeyReleaseMask 
                 | ButtonPressMask 
                 | ButtonReleaseMask 
                 | MotionNotify 
                 | PointerMotionMask);
    //         | ButtonMotionMask);
    //XSelectInput(xwin->xdisplay->dpy, xwin->win, StructureNotifyMask);



    /* set WM_CLASS */
    XClassHint chint = {"pf","Pdp"};
    XSetClassHint(xwin->xdisplay->dpy, xwin->win, &chint);
    XSetCommand(xwin->xdisplay->dpy, xwin->win, 0, 0);

#if 0 // FIXME: Not sure what to do here...
    int w = xwin->winwidth;
    int h = xwin->winheight;
    ZL_LOG("XSizeHints %dx%d", w, h);
    XSizeHints shints = {
        .flags = PBaseSize | PMinSize | PMaxSize,
        .base_width = w, .base_height = h,
        .min_width  = w, .min_height  = h,
        .max_width  = w, .max_height  = h,
    };
    XSetWMNormalHints(xwin->xdisplay->dpy, xwin->win, &shints);
#endif

    /* map */
    XMapWindow(xwin->xdisplay->dpy, xwin->win);

    /* create graphics context */
    xwin->gc = XCreateGC(xwin->xdisplay->dpy, xwin->win, 0, 0);
    
    /* catch events until MapNotify */
    for(;;){
        XNextEvent(xwin->xdisplay->dpy, &e);
        if (e.type == MapNotify) break;
        if (e.type == ConfigureNotify) {
            xwin->winwidth  = e.xconfigure.width;
            xwin->winheight = e.xconfigure.height;
            // fprintf(stderr, "_win: %d x %d\n", e.xconfigure.width, e.xconfigure.height);
            continue;
        }
        ZL_LOG("drop event type %d\n", e.type);
    }

    D fprintf(stderr, "mapped window %x\n", (unsigned int)xwin->win);


    /* we're done initializing */
    xwin->initialized = 1;

    /* disable/enable cursor */
    zl_xwindow_cursor(xwin, xwin->cursor);

    /* set window title */
    zl_xwindowitle(xwin, "pf");

    zl_xdisplay_register_window(xwin->xdisplay, xwin);

 exit:
    return xwin->initialized;

}

#if 0
static int errors = 0;
int xwindow_error_handler(Display *dpy, XErrorEvent *e){
    errors++;
    return 0;
}
#endif

void zl_xwindow_init(zl_xwindow_p xwin)
{
    xwin->xdisplay = 0;
    xwin->win = 0;

    xwin->winwidth = 320;
    xwin->winheight = 240;
    xwin->winxoffset = 0;
    xwin->winyoffset = 0;

    xwin->initialized = 0;

    xwin->cursor = 0;
    //xwin->dragbutton = gensym("drag1");

    xwin->events = buf_new(16);

    //XSetErrorHandler(xwindow_error_handler);

}

zl_xwindow_p zl_xwindow_new(void)
{
    zl_xwindow_p xwin = malloc(sizeof(*xwin));
    xwin->type = zl_xwindow_type();
    zl_xwindow_init(xwin);
    return xwin;
}
    
void zl_xwindow_close(zl_xwindow_p xwin)
{
    // void *v;
    zl_xwindow_drop_events(xwin); 
    buf_free(xwin->events);
    xwin->events = 0;

    XEvent e;

    if (xwin->initialized){
        XFreeGC(xwin->xdisplay->dpy, xwin->gc);
        XDestroyWindow(xwin->xdisplay->dpy, xwin->win);
        while(XPending(xwin->xdisplay->dpy)) XNextEvent(xwin->xdisplay->dpy, &e);
        zl_xdisplay_unregister_window(xwin->xdisplay, xwin);
        xwin->xdisplay = 0;
        xwin->initialized = 0;
    }
}

void zl_xwindow_cleanup(zl_xwindow_p x)
{
    // close win
    zl_xwindow_close(x);

    // no more dynamic data to free
}

void zl_xwindow_free(zl_xwindow_p xwin)
{
    zl_xwindow_cleanup(xwin);
    free(xwin);
}

int zl_xwindow_winwidth(zl_xwindow *x)   { return x->winwidth; }
int zl_xwindow_winheight(zl_xwindow *x)  { return x->winheight; }
int zl_xwindow_winxoffset(zl_xwindow *x) { return x->winxoffset; }
int zl_xwindow_winyoffset(zl_xwindow *x) { return x->winyoffset; }


// Delegate to display
void zl_xwindow_route_events(zl_xwindow *x) {
    zl_xdisplay_route_events(x->xdisplay);
}
