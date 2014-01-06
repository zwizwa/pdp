#ifndef _ZL_XWINDOW_H_
#define _ZL_XWINDOW_H_


/*
 *   Pure Data Packet header file: zl_xwindow glue code
 *   Copyright (c) by Tom Schouten <tom@zwizwa.be>
 *
 *   This program is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation; either version 2 of the License, or
 *   (at your option) any later version.
 *
 *   This program is distributed in the hope that it will be useful,x
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with this program; if not, write to the Free Software
 *   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 */

#ifndef ZL_XWINDOW_H
#define ZL_XWINDOW_H


#ifdef HAVE_LEAF
#include <leaf/leaf.h>
#else
// Stand-alone
#define LEAF_CLASS(member) int member[0]
#endif


// x stuff
#include <X11/Xlib.h>
#include <X11/Xatom.h> 

#include "zl.h"
#include "util.h" // small utility library


/* x display class */
typedef struct {
    LEAF_CLASS(super);
} zl_xdisplay_class;

/* x window class */
typedef struct {
    LEAF_CLASS(super);
} zl_xwindow_class;

struct zl_xdisplay;
typedef struct zl_xdisplay zl_xdisplay;
typedef struct zl_xdisplay *zl_xdisplay_p;

struct zl_xwindow;
typedef struct zl_xwindow zl_xwindow;
typedef struct zl_xwindow *zl_xwindow_p;

#ifdef PRIVATE
struct zl_xdisplay
{
    zl_xdisplay_class *type;

    Display *dpy;              // the display connection
    int screen;                // the screen
    buf_t *windowlist;         // all windows belonging to this connection
                               // this contains (id, eventlist)

    typeof(((XEvent*)0)->xbutton.button) dragbutton;
};

struct zl_xwindow
{
    zl_xwindow_class *type;
    zl_xdisplay_p xdisplay;   // the display object
    Window win;               // window reference
    GC gc;                    // graphics context
    Atom WM_DELETE_WINDOW;

    buf_t *events;

    int winwidth;             // dim states
    int winheight;
    int winxoffset;
    int winyoffset;

    int  initialized;
    int  autocreate;

    char lastbut; // last button pressed (for drag)

    //t_symbol *dragbutton;
    
    float cursor;

};
#endif


typedef void (*zl_zwindow_event_fn)(void *context, XEvent *ev);
typedef XEvent *XEvent_p;

#define ZL_FOR_BUTTON(M, ...)                   \
    M(1, __VA_ARGS__)                           \
    M(2, __VA_ARGS__)                           \
    M(3, __VA_ARGS__)                           \
    M(4, __VA_ARGS__)                           \
    M(5, __VA_ARGS__)

#define ZL_BUTTON_EV(n, EV, tag) EV(tag##n)
#define ZL_XDISPLAY_DEF_EV_BUT(EV, tag) EV(tag) ZL_FOR_BUTTON(ZL_BUTTON_EV, EV, tag)

#define ZL_XDISPLAY_EV_LIST(EV)           \
    ZL_XDISPLAY_DEF_EV_BUT(EV, drag)      \
    ZL_XDISPLAY_DEF_EV_BUT(EV, press)     \
    ZL_XDISPLAY_DEF_EV_BUT(EV, release)   \
    ZL_XDISPLAY_DEF_EV_BUT(EV, motion)    \
    EV(close)                             \
    EV(keypress)                          \
    EV(keyrelease)                        \

#define ZL_XDISPLAY_EV(name) zl_display_ev_##name

/* All events are const char, which allows them to be used as static
   (pointer) references, or to convert to a dynamic hashed string
   (interned symbol).  The macro ZL_DISPLAY_EV_LIST could also be used
   to define app-specific tag wrappers based on name only. */
#define _ZL_XDISPLAY_EV_DECL(name) extern zl_tag ZL_XDISPLAY_EV(name);
ZL_XDISPLAY_EV_LIST(_ZL_XDISPLAY_EV_DECL)

zl_xwindow_p zl_xwindow_new(void);
zl_xdisplay_p zl_xdisplay_new(const char *dpy);

#define FUN(t,f) t zl_xdisplay_##f(zl_xdisplay_p
#define ARG(t,a) , t a
#define END );

#include "xdisplay.api"

#undef FUN
#define FUN(t,f) t zl_xwindow_##f(zl_xwindow_p

#include "xwindow.api"

#undef FUN
#undef ARG
#undef END


zl_xwindow_class *zl_xwindow_type(void);
zl_xdisplay_class *zl_xdisplay_type(void);

#endif
#endif
