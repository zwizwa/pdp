/*
 *   3d render context object
 *   Copyright (c) 2003-2013 by Tom Schouten <tom@zwizwa.be>
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


/* the 3d render context packet: platform independent data structure
   and method prototypes */


#ifndef __zl_3dcontext_h__
#define __zl_3dcontext_h__

#include <stdint.h>
#include "zl/xwindow.h"

typedef struct zl_3Dcontext
{
    uint32_t encoding;            /* the kind of render context */
    uint32_t width;               /* context width */
    uint32_t height;              /* context height */
    uint32_t sub_width;           /* portion that is currently used */
    uint32_t sub_height;
    union {
        zl_xwindow *xwindow;
    } drawable;              /* context's drawable (i.e. Window, GLXPbuffer, ...) */
    void *context;           /* context's context object */

} zl_3Dcontext;

#define ZL_3DCONTEXT_WINDOW  1  /* window context packet id */
#define ZL_3DCONTEXT_PBUFFER 2  /* pbuf context packet id */


/* all symbols are C-style */
#ifdef __cplusplus
extern "C"
{
#endif
    uint32_t zl_3Dcontext_width(zl_3Dcontext *c);
    uint32_t zl_3Dcontext_height(zl_3Dcontext *c);
    uint32_t zl_3Dcontext_subwidth(zl_3Dcontext *c);
    uint32_t zl_3Dcontext_subheight(zl_3Dcontext *c);
    float zl_3Dcontext_subaspect(zl_3Dcontext *c);

    /* setters */
    void zl_3Dcontext_set_subwidth(zl_3Dcontext *c, uint32_t w);
    void zl_3Dcontext_set_subheight(zl_3Dcontext *c, uint32_t h);

    /* render context activation and initialization */
    void zl_3Dcontext_set_rendering_context(zl_3Dcontext *c);
    void zl_3Dcontext_unset_rendering_context(zl_3Dcontext *c);
    void zl_3Dcontext_setup_3d_context(zl_3Dcontext *c);
    void zl_3Dcontext_setup_2d_context(zl_3Dcontext *c);

    /* constructors */
    zl_3Dcontext* zl_3Dcontext_pbuf_new(uint32_t width, uint32_t height, uint32_t depth);
    zl_3Dcontext* zl_3Dcontext_win_new(void);

    /* window specific methods, just return window instance? */
    void zl_3Dcontext_win_resize(zl_3Dcontext *c, int width, int height);
    void zl_3Dcontext_win_cursor(zl_3Dcontext *c, int toggle);
    void zl_3Dcontext_for_parsed_events(zl_3Dcontext *c, zl_receiver recever, void *context);
    void zl_3Dcontext_win_swapbuffers(zl_3Dcontext *c);

    /* converters */
    void zl_3Dcontext_snap_to_bitmap(zl_3Dcontext *c, int w, int h, void *data);

    void zl_3Dcontext_free(zl_3Dcontext *c);

    /* global setup */
    void zl_3Dcontext_glx_setup(void);


#ifdef __cplusplus
}
#endif

#endif
