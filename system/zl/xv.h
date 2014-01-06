
/*
 *   Pure Data Packet header file: xwindow glue code
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

#ifndef _zl_xv_h_
#define _zl_xv_h_

struct zl_xv;
typedef struct zl_xv  zl_xv;
typedef struct zl_xv *zl_xv_p;

#ifndef PRIVATE
#include "xwindow.h"
#else

// x stuff
#include <X11/Xlib.h>
#include <X11/Xatom.h>
#include <sys/shm.h>
#include <X11/extensions/XShm.h>
#include <X11/extensions/Xv.h>
#include <X11/extensions/Xvlib.h>
#include "config.h"
#include "xwindow.h"

/* xv class */
typedef void zl_xv_class;
struct zl_xv
{
    zl_xv_class *type;    

    zl_xdisplay_p xdpy;

    int xv_format;
    int xv_port;

    XvImage *xvi;
    XShmSegmentInfo *shminfo; // if null, not using shm
    int shm; // try shm next time
    void *data;
    unsigned int width;
    unsigned int height;
    int last_encoding;

    int  initialized;

};
#endif

zl_xv_p zl_xv_new(int fourcc);

// image formats for communication with the X Server
#define FOURCC_YV12 0x32315659  /* YV12   YUV420P */
#define FOURCC_YUV2 0x32595559  /* YUV2   YUV422 */
#define FOURCC_I420 0x30323449  /* I420   Intel Indeo 4 */


#define FUN(t,f) t zl_xv_##f(zl_xv_p x
#define ARG(t,a) , t a
#define END );
#include "zl/xv.api"
#undef FUN
#undef ARG
#undef END


#endif
