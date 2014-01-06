/*
 *   Pure Data Packet system module. - x window glue code (fairly tied to pd and pf)
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


// this code is fairly tied to pd and pf. serves mainly as reusable glue code
// for pf_xv, pf_glx, pf_3d_windowcontext, ...

#include <string.h>
#include <stdlib.h>
#include <stdio.h>

// Note sure why this had to be defined..
#define PRIVATE 

//#include <pf/stream.h>
//#include <pf/packet.h>
//#include <pf/packet_imp.h>
//#include <pf/image.h>
#include "xwindow.h"
#include "xv.h"

#define D if(1)


/************************************* XV ************************************/

/* XvImage image. */
static void zl_xv_destroy_xvimage(zl_xv_p xvid);
static void zl_xv_create_xvimage(zl_xv_p xvid, int width, int height);
// static void zl_xv_get_dims(zl_xv_p xvid, unsigned int *w, unsigned int *h);


/* this was a local function. */
static int errors;
static int error_handler(Display *dpy, XErrorEvent *e){errors++; return 0;}

static void zl_xv_create_xvimage(zl_xv_p xvid, int width, int height)
{
    // int i;
    long size;
    errors = 0;

    xvid->width = width;
    xvid->height = height;
    size = (xvid->width * xvid->height + (((xvid->width>>1)*(xvid->height>>1))<<1));

    // don't use shared memory
    if (!xvid->shm){
      no_shm:        

        xvid->data = (unsigned char *)malloc(size + 4); // 4 bytes extra to get word size buffer for word bitflip
        //for (i=0; i<size; i++) xvid->data[i] = i;

        
        /* fprintf(stderr, "XvCreateImage(%p,%08x,%08x,%p,%d,%d)",
                xvid->xdpy->dpy, xvid->xv_port, xvid->xv_format,
                (char *)xvid->data,  xvid->width, xvid->height); */

        xvid->xvi = XvCreateImage(xvid->xdpy->dpy, xvid->xv_port, xvid->xv_format, 
                                  (char *)xvid->data, xvid->width, xvid->height);
        xvid->last_encoding = -1;

        if ((!xvid->xvi) || (!xvid->data)) fprintf (stderr, "zl_xv_create_xvimage: error creating xvimage");
        return;

    }
    // try shared memory
    else {
        void *old_handler = XSetErrorHandler(error_handler);

        // ASSERT!
        if (xvid->shminfo) {
            fprintf(stderr, "have shminfo?"); exit(1);
        }

        xvid->shminfo = malloc(sizeof(XShmSegmentInfo));
        memset(xvid->shminfo, 0, sizeof(XShmSegmentInfo));

        /* fprintf(stderr, "XvShmCreateImage(%p,%08x,%08x,%08x,%d,%d,%p)",
                xvid->xdpy->dpy, xvid->xv_port, xvid->xv_format,
                0,  xvid->width, xvid->height, xvid->shminfo); */
        
        xvid->xvi = XvShmCreateImage(xvid->xdpy->dpy, xvid->xv_port, xvid->xv_format,
                                     0,  xvid->width, xvid->height, xvid->shminfo);
        if (!xvid->xvi) goto shm_error;
        if (-1 == (xvid->shminfo->shmid = shmget(IPC_PRIVATE, xvid->xvi->data_size,
                                             IPC_CREAT | 0777))) goto shm_error;
        if (((void *)-1) == (xvid->shminfo->shmaddr = (char *) shmat(xvid->shminfo->shmid, 0, 0))) goto shm_error;
        
        xvid->data = xvid->xvi->data = xvid->shminfo->shmaddr;
        xvid->shminfo->readOnly = False;
        XShmAttach(xvid->xdpy->dpy, xvid->shminfo);
        XSync(xvid->xdpy->dpy, False);
        if (errors) goto shm_error;
        shmctl(xvid->shminfo->shmid, IPC_RMID, 0);
        XSetErrorHandler(old_handler);
        D fprintf(stderr, "xv: using shared memory\n");
        return; // ok

      shm_error:
        D fprintf(stderr, "xv: can't use shared memory\n");
        zl_xv_destroy_xvimage(xvid);
        XSetErrorHandler(old_handler);
        xvid->shm = 0;
        goto no_shm;
    }
}

static void zl_xv_destroy_xvimage(zl_xv_p xvid)
{
    if (xvid->xvi) XFree(xvid->xvi);
    if (xvid->shminfo){
        if ((void *)-1 != xvid->shminfo->shmaddr  &&  NULL != xvid->shminfo->shmaddr)
            shmdt(xvid->shminfo->shmaddr);
        free(xvid->shminfo);
    }
    else {
        if (xvid->data) free(xvid->data);
    }
    xvid->shminfo = 0;
    xvid->xvi = 0;
    xvid->data = 0;
}

/* display */
void zl_xv_image_display(zl_xv_p xvid, zl_xwindow_p xwin){

    if (!xvid->initialized) return;

    /* set aspect ratio here if necessary 
       currently we don't : client responsability */
    int a_x = 0;
    int a_y = 0;
    int b_x = xvid->width;
    int b_y = xvid->height;
    // int offset = 0;

    int w_w = xwin->winwidth;
    int w_h = xwin->winheight;

    D fprintf(stderr, "%d %d %d %d %d %d\n", a_x, a_y, b_x, b_y, w_w, w_h);

    if (xvid->shminfo){
        XvShmPutImage(xvid->xdpy->dpy,xvid->xv_port,
                      xwin->win,xwin->gc,xvid->xvi,
                      a_x, a_y, b_x, b_y, 0,0, w_w, w_h, True);
    }
    else {
        XvPutImage(xvid->xdpy->dpy,xvid->xv_port,
                   xwin->win, xwin->gc, xvid->xvi,
                   a_x, a_y, b_x, b_y, 0,0, w_w, w_h);
    }

    XFlush(xvid->xdpy->dpy);
}


void *zl_xv_image_data(zl_xv_p xvid, zl_xwindow_p xwin, 
                        unsigned int width, unsigned int height){

    if (!xvid->initialized) return 0;

    /* check if xvimage needs to be recreated */
    if ((width != xvid->width) || (height != xvid->height)){
        zl_xv_destroy_xvimage(xvid);
        zl_xv_create_xvimage(xvid, width, height);
    }

    return xvid->data;

}




void zl_xv_close(zl_xv_p xvid)
{
    if (xvid->initialized){
        if (xvid->xvi) zl_xv_destroy_xvimage(xvid);
        XvUngrabPort(xvid->xdpy->dpy, xvid->xv_port, CurrentTime);
        xvid->xv_port = 0;
        xvid->xdpy = 0;
        xvid->last_encoding = -1;
        xvid->initialized = 0;
    }
}

void zl_xv_cleanup(zl_xv_p xvid)
{
    // close xv port (and delete XvImage)
    zl_xv_close(xvid);

    // no more dynamic data to free

}

void zl_xv_free(zl_xv_p xvid){
    zl_xv_cleanup(xvid);
    free(xvid);
}

void zl_xv_init(zl_xv_p xvid, int format)
{

    xvid->xdpy = 0;

    // xvid->xv_format = FOURCC_I420; // used to be FOURCC_YV12.  veejay compat.
    // xvid->xv_format = FOURCC_YV12; // PDP uses this
    // xvid->xv_format = 0x32595559; // Microsoft webcam hack
    xvid->xv_format = format;

    xvid->xv_port = 0;

    xvid->shm = 1; // try to use shm
    //xvid->shm = 0; // don't
    xvid->shminfo = 0;

    xvid->width = 320;
    xvid->height = 240;

    xvid->data = 0;
    xvid->xvi = 0;

    xvid->initialized = 0;
    xvid->last_encoding = -1;

}
zl_xv_p zl_xv_new(int format)
{
    zl_xv_p xvid = malloc(sizeof(*xvid));
    zl_xv_init(xvid, format);
    return xvid;
}

int zl_xv_open_on_display(zl_xv_p xvid, zl_xdisplay_p d, int adaptor_start)
{
    unsigned int ver, rel, req, ev, err, i, j;
    unsigned int adaptors;
    // int formats;
    XvAdaptorInfo        *ai;

    if (xvid->initialized) return 1;
    if (!d) return 0;
    xvid->xdpy = d;

    if (Success != XvQueryExtension(xvid->xdpy->dpy,
                                    &ver,&rel,&req,&ev,&err))
        return 0;

    /* find + lock port */
    if (Success != XvQueryAdaptors(xvid->xdpy->dpy,
                                   DefaultRootWindow(xvid->xdpy->dpy),
                                   &adaptors,&ai))
        return 0;
    for (i = adaptor_start; i < adaptors; i++) {
        if ((ai[i].type & XvInputMask) && (ai[i].type & XvImageMask)) {
            for (j=0; j < ai[i].num_ports; j++){
                if (Success != XvGrabPort(xvid->xdpy->dpy,
                                          ai[i].base_id+j,CurrentTime)) {
                    D fprintf(stderr,"xv: Xv port %ld on adapter %d: is busy, skipping\n",
                              ai[i].base_id+j, i);
                }
                else {
                    xvid->xv_port = ai[i].base_id + j;
                    goto breakout;
                }
            }
        }
    }


 breakout:

    XFree(ai);
    if (0 == xvid->xv_port) return 0;
    D fprintf(stderr, "xv: grabbed port %d on adaptor %d\n", xvid->xv_port, i);
    xvid->initialized = 1;
    zl_xv_create_xvimage(xvid, xvid->width, xvid->height);
    return 1;
}


//static void zl_xv_get_dims(zl_xv_p xvid, unsigned int *w, unsigned int *h) {
//    if (w) *w = xvid->width;
//    if (w) *h = xvid->height;
//}
