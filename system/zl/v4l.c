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
 *   2012-10-29 : V4L2 support from Pidip's pdp_v4l2 (= fork of pdp_v4l)
 *                Thanks to Yves Degoyon, Lluis Gomez i Bigorda, Gerd Knorr
 *
 */


/* NOTE: This code isn't particularly well-written, but it'll have to
   do for now.  I kept most of it as is during porting from pdp_v4l /
   pdp_v4l2

   This code is probably broken on V4L1.  If you need it, drop me an
   email at <pdp@zwizwa.be> 

   Overall I'm not sure if it makes sense to keep V4L1 alive..
*/

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <unistd.h>
#include <string.h>
#include <ctype.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/time.h>
#include <linux/types.h>
#include <stdbool.h>

#include "v4l.h"


#include <sys/mman.h>
#include <sched.h>
#include <pthread.h>


//include it anyway
//#ifdef HAVE_PWCV4L
#include "pwc-ioctl.h"
//#endif


#define DO_SANITY_CHECKS 0



/* Formats supported in the _bang method. */

/* FIXME: V4L2: add raw PWC MJPEG decompressor for format [0x32435750]
   'PWC2' (Raw Philips Webcam) */

static void v4l_notify_frame(struct zl_v4l *x);
static void *v4l_thread(void *_x);


/* V4L1 */
#ifdef HAVE_V4L1
static void v4l1_unmap(struct zl_v4l *x) {
    /* On V4L1 there is only a single buffer. */
    if (x->x_buf[0]) {
        if (x->x_debug) ZL_LOG("v4l: unmapping %p", x->x_buf[0]);
        if (-1 == munmap(x->x_buf[0],  x->x_api.v4l1.mbuf.size)) {
            ZL_PERROR("munmap");
        }
    }
    x->x_buf[0] = NULL;
}
static void v4l1_capture_frame(struct zl_v4l *x) {
    if (!(ioctl(x->x_tvfd, VIDIOCMCAPTURE,
                &x->x_api.v4l1.mmap[x->x_frame_wr]) < 0)) return; // OK
    /* Handle error */
    if (errno == EAGAIN)
        ZL_LOG("v4l: can't sync (no video source?)\n");
    else
        ZL_PERROR("v4l: VIDIOCMCAPTURE");

    /* Try again */
    if (ioctl(x->x_tvfd, VIDIOCMCAPTURE,
              &x->x_api.v4l1.mmap[x->x_frame_wr]) < 0)
        ZL_PERROR("v4l: VIDIOCMCAPTURE2");
    ZL_LOG("v4l: frame %d %d, format %d, width %d, height %d",
           x->x_frame_wr,
           x->x_api.v4l1.mmap[x->x_frame_wr].frame,
           x->x_api.v4l1.mmap[x->x_frame_wr].format,
           x->x_api.v4l1.mmap[x->x_frame_wr].width,
           x->x_api.v4l1.mmap[x->x_frame_wr].height);
    // zl_v4l_close(x);  // FIXME: don't close from thread
}
unsigned char *v4l1_image_dequeue(struct zl_v4l *x) {
    unsigned char *data =
        (unsigned char *)(x->x_buf[0] + x->x_api.v4l1.mbuf.offsets[x->x_frame_rd]);
    return data;
}
static void v4l_notify_frame(struct zl_v4l *x);

void v4l1_thread_loop(struct zl_v4l *x) {

    /* capture with a double buffering scheme */
    while (x->x_continue_thread) {

        /* schedule capture command for next frame */
        v4l1_capture_frame(x);

        /* wait until previous capture is ready */
        if (ioctl(x->x_tvfd, VIDIOCSYNC,
                  &x->x_api.v4l1.mmap[x->x_frame_wr].frame) < 0) {
            ZL_PERROR("v4l: VIDIOCSYNC");
            // zl_v4l_close(x); // FIXME: Don't call close in thread
            break;
        }

        /* setup pointers for main thread */
        v4l_notify_frame(x);
    }
    ZL_LOG("v4l: capture thread quitting" );
}

static void v4l1_pwc_agc(struct zl_v4l *x, float gain) {
    if (x->x_v4l_api != V4L1) {
        ZL_LOG("v4l: agc message needs V4L1");
        return;
    }

    gain *= (float)(1<<16);
    int g = (int)gain;
    if (g < 0) g = -1;            // automatic
    if (g > (1<<16)) g = (1<<16) - 1; // fixed

    //ZL_LOG("v4l: setting agc to %d", g);
    if (ioctl(x->x_tvfd, VIDIOCPWCSAGC, &g)){
        ZL_LOG("v4l: pwc: VIDIOCPWCSAGC");
        //goto closit;
    }
}
static void v4l1_pwc_init(struct zl_v4l *x) {
    struct pwc_probe probe;
    int isPhilips = 0;

#ifdef HAVE_PWCV4L
    /* skip test */
    isPhilips = 1;
#else
    /* test for pwc */
    if (ioctl(x->x_tvfd, VIDIOCPWCPROBE, &probe) == 0)
       if (!strcmp(x->x_api.v4l1.cap.name, probe.name))
         isPhilips = 1;

#endif

    /* don't do pwc specific stuff */
    if (!isPhilips) return;

    ZL_LOG("v4l: detected pwc");


    if(ioctl(x->x_tvfd, VIDIOCPWCRUSER)){
        ZL_PERROR("v4l: pwc: VIDIOCPWCRUSER");
        goto closit;
    }

    /* this is a workaround:
       we disable AGC after restoring user prefs
       something is wrong with newer cams (like Qickcam 4000 pro)
    */

    if (1) {
        zl_v4l_pwc_agc(x, 1.0);
    }


    if (ioctl(x->x_tvfd, VIDIOCGWIN, &x->x_api.v4l1.win)){
        ZL_PERROR("v4l: pwc: VIDIOCGWIN");
        goto closit;
    }



    if (x->x_api.v4l1.win.flags & PWC_FPS_MASK){
        //ZL_LOG("v4l: pwc: camera framerate: %d", (x->x_api.v4l1.win.flags & PWC_FPS_MASK) >> PWC_FPS_SHIFT);
        //ZL_LOG("v4l: pwc: setting camera framerate to %d", x->x_framerate);
        x->x_api.v4l1.win.flags &= PWC_FPS_MASK;
        x->x_api.v4l1.win.flags |= ((x->x_fps_num / x->x_fps_den) << PWC_FPS_SHIFT);
        if (ioctl(x->x_tvfd, VIDIOCSWIN, &x->x_api.v4l1.win)){
            ZL_PERROR("v4l: pwc: VIDIOCSWIN");
            goto closit;
        }
        if (ioctl(x->x_tvfd, VIDIOCGWIN, &x->x_api.v4l1.win)){
            ZL_PERROR("v4l: pwc: VIDIOCGWIN");
            goto closit;
        }
        ZL_LOG("v4l: camera framerate set to %d fps", (x->x_api.v4l1.win.flags & PWC_FPS_MASK) >> PWC_FPS_SHIFT);

    }


    return;



 closit:
    zl_v4l_close_error(x);
    return;

}
static void *v4l_thread(void *_x);

static void v4l1_open(struct zl_v4l *x, const char *name)
{    /* if already opened -> close */
    if (x->x_initialized) zl_v4l_close(x);


    ZL_LOG("v4l: opening %s (V4L1)", name);
    x->x_v4l_api = V4L1;
    bzero(&x->x_api.v4l1, sizeof(x->x_api.v4l1));


    if (x->x_initialized) return; // idempotent

    /* open a v4l device and allocate a buffer */

    int i;
    unsigned int width, height;


    if ((x->x_tvfd = open(name, O_RDWR)) < 0)
    {
        ZL_LOG("v4l: error:");
        perror(name);
        goto closit;
    }


    if (ioctl(x->x_tvfd, VIDIOCGCAP, &x->x_api.v4l1.cap) < 0)
    {
        // http://stackoverflow.com/questions/898474/v4l-problem-with-vidiocgcap-ioctl-call
        perror("get capabilities");
        goto closit;
    }

    ZL_LOG("v4l: cap: name %s type %d channels %d maxw %d maxh %d minw %d minh %d",
        x->x_api.v4l1.cap.name, x->x_api.v4l1.cap.type,  x->x_api.v4l1.cap.channels,  x->x_api.v4l1.cap.maxwidth,  x->x_api.v4l1.cap.maxheight,
            x->x_api.v4l1.cap.minwidth,  x->x_api.v4l1.cap.minheight);

#if 0 // FIXME: not used
    x->x_minwidth = pdp_imageproc_legalwidth(x->x_api.v4l1.cap.minwidth);
    x->x_maxwidth = pdp_imageproc_legalwidth_round_down(x->x_api.v4l1.cap.maxwidth);
    x->x_minheight = pdp_imageproc_legalheight(x->x_api.v4l1.cap.minheight);
    x->x_maxheight = pdp_imageproc_legalheight_round_down(x->x_api.v4l1.cap.maxheight);
#endif


    if (ioctl(x->x_tvfd, VIDIOCGPICT, &x->x_api.v4l1.picture) < 0)
    {
        perror("VIDIOCGCAP");
        goto closit;
    }

    ZL_LOG("v4l: picture: brightness %d depth %d palette %d",
            x->x_api.v4l1.picture.brightness, x->x_api.v4l1.picture.depth, x->x_api.v4l1.picture.palette);

    /* get channel info */
    for (i = 0; i < x->x_api.v4l1.cap.channels; i++)
    {
        x->x_api.v4l1.channel.channel = i;
        if (ioctl(x->x_tvfd, VIDIOCGCHAN, &x->x_api.v4l1.channel) < 0)
        {
            perror("VDIOCGCHAN");
            goto closit;
        }
        ZL_LOG("v4l: channel %d name %s type %d flags %d",
            x->x_api.v4l1.channel.channel, x->x_api.v4l1.channel.name,
            x->x_api.v4l1.channel.type, x->x_api.v4l1.channel.flags);
    }

    /* switch to the desired channel */
    if (x->x_channel >= (unsigned long)x->x_api.v4l1.cap.channels) x->x_channel = x->x_api.v4l1.cap.channels - 1;

    x->x_api.v4l1.channel.channel = x->x_channel;
    if (ioctl(x->x_tvfd, VIDIOCGCHAN, &x->x_api.v4l1.channel) < 0)
    {
        ZL_PERROR("v4l: warning: VDIOCGCHAN");
        ZL_LOG("v4l: cant change to channel %d",x->x_channel);

        // ignore error
        // goto closit;
    }
    else{
        ZL_LOG("v4l: switched to channel %d", x->x_channel);
    }


    /* set norm */
    x->x_api.v4l1.channel.norm = x->x_norm;
    if (ioctl(x->x_tvfd, VIDIOCSCHAN, &x->x_api.v4l1.channel) < 0)
    {
        ZL_PERROR("v4l: warning: VDIOCSCHAN");
        ZL_LOG("v4l: cant change to norm %d",x->x_norm);

        // ignore error
        // goto closit;
    }
    else {
        ZL_LOG("v4l: set norm to %u", x->x_norm);
    }

    if (x->x_freq > 0){
        if (ioctl(x->x_tvfd, VIDIOCSFREQ, &x->x_freq) < 0)
            perror ("couldn't set frequency :");
    }




    /* get mmap numbers */
    if (ioctl(x->x_tvfd, VIDIOCGMBUF, &x->x_api.v4l1.mbuf) < 0)
    {
        ZL_PERROR("v4l: VIDIOCGMBUF");
        goto closit;
    }
    ZL_LOG("v4l: buffer size %d, frames %d, offset %d %d", x->x_api.v4l1.mbuf.size,
        x->x_api.v4l1.mbuf.frames, x->x_api.v4l1.mbuf.offsets[0], x->x_api.v4l1.mbuf.offsets[1]);

    bzero(x->x_buf, sizeof(x->x_buf));
    if (!(x->x_buf[0] = (unsigned char *)
        mmap(0, x->x_api.v4l1.mbuf.size, PROT_READ|PROT_WRITE, MAP_SHARED, x->x_tvfd, 0)))
    {
        ZL_PERROR("v4l: mmap");
        goto closit;
    }
    x->x_nbbuffers = 2; // V4L has only 1 mmapped buffer with 2 frames

    // FIXME:
    // pdp_v4l_setlegaldim(x, x->x_width, x->x_height);

    width = x->x_width;
    height = x->x_height;

    for (i = 0; i < ZL_V4L_NBUF; i++)
    {
      //x->x_api.v4l1.mmap[i].format = VIDEO_PALETTE_YUV420P;
      //x->x_api.v4l1.mmap[i].format = VIDEO_PALETTE_UYVY;
      x->x_api.v4l1.mmap[i].width = width;
      x->x_api.v4l1.mmap[i].height = height;
      x->x_api.v4l1.mmap[i].frame  = i;
    }


/* fallthrough macro for case statement */
#define TRYPALETTE(_palette)                                                        \
        x->x_api.v4l1.palette = _palette;                                                \
        for (i = 0; i < ZL_V4L_NBUF; i++) x->x_api.v4l1.mmap[i].format = x->x_api.v4l1.palette;        \
        if (ioctl(x->x_tvfd, VIDIOCMCAPTURE, &x->x_api.v4l1.mmap[x->x_frame_wr]) < 0)        \
            {                                                                        \
                if (errno == EAGAIN)                                                \
                    ZL_LOG("v4l: can't sync (no video source?)");                \
                if (x->x_format) break; /* only break if not autodetecting */        \
            }                                                                        \
        else{                                                                        \
            ZL_LOG("v4l: using " #_palette);                                        \
            goto capture_ok;                                                        \
        }

    switch(x->x_format){
    default:
    case 0:
    case 1: TRYPALETTE(VIDEO_PALETTE_YUV420P);
    case 2: TRYPALETTE(VIDEO_PALETTE_YUV422);
    case 3: TRYPALETTE(VIDEO_PALETTE_RGB24);
    case 4: TRYPALETTE(VIDEO_PALETTE_RGB32);
    }

    // none of the formats are supported
    ZL_PERROR("v4l: VIDIOCMCAPTURE: format not supported");
    goto closit;


 capture_ok:

    ZL_LOG("v4l: frame %d %d, format %d, width %d, height %d",
           x->x_frame_wr,
           x->x_api.v4l1.mmap[x->x_frame_wr].frame,
           x->x_api.v4l1.mmap[x->x_frame_wr].format,
           x->x_api.v4l1.mmap[x->x_frame_wr].width,
           x->x_api.v4l1.mmap[x->x_frame_wr].height);

    x->x_width = width;
    x->x_height = height;

    ZL_LOG("v4l: Opened video connection (%dx%d)",x->x_width,x->x_height);


    /* do some pwc specific inits */
    v4l1_pwc_init(x);

    x->x_initialized = true;

    /* create thread */
    x->x_continue_thread = 1;
    sem_init(&x->x_frame_ready_sem, 0, 0);
    pthread_create(&x->x_thread_id, 0, v4l_thread, x);

    goto done;

  closit:
    zl_v4l_close_error(x);

  done:
    return;
}
#if 0
static void v4l1_audio (struct zl_v4l *x) {

    if (x->x_v4l_api != V4L1) {
        ZL_LOG("v4l: audio message needs V4L1");
        return;
    }
    int i = 0;
    if (x->x_initialized) {
        fprintf(stderr,"  audios  : %d\n",x->x_api.v4l1.cap.audios);
        x->x_api.v4l1.audio.audio = 0;
        ioctl(x->x_tvfd,VIDIOCGAUDIO, &x->x_api.v4l1.audio);

        fprintf(stderr,"    %d (%s): ",i,x->x_api.v4l1.audio.name);
        if (x->x_api.v4l1.audio.flags & VIDEO_AUDIO_MUTABLE)
            fprintf(stderr,"muted=%s ",
                    (x->x_api.v4l1.audio.flags & VIDEO_AUDIO_MUTE) ? "yes":"no");
        if (x->x_api.v4l1.audio.flags & VIDEO_AUDIO_VOLUME)
            fprintf(stderr,"volume=%d ",x->x_api.v4l1.audio.volume);
        if (x->x_api.v4l1.audio.flags & VIDEO_AUDIO_BASS)
            fprintf(stderr,"bass=%d ",x->x_api.v4l1.audio.bass);
        if (x->x_api.v4l1.audio.flags & VIDEO_AUDIO_TREBLE)
            fprintf(stderr,"treble=%d ",x->x_api.v4l1.audio.treble);
        fprintf(stderr,"\n");

    }
}
#endif



#else // !HAVE_V4L1
static inline void v4l1_unmap(struct zl_v4l *x __attribute__((__unused__))) {}
static inline void v4l1_capture_frame(struct zl_v4l *x __attribute__((__unused__))) {}
#endif

/* V4L2 */
#ifdef HAVE_V4L2

unsigned char *v4l2_image_dequeue(struct zl_v4l *x) {
    return (unsigned char *)( x->x_buf[x->x_frame_rd]);
}

static void v4l2_unmap(struct zl_v4l *x) {
    int i;
    for(i=0; i<x->x_nbbuffers; i++ ) {
        // if (x->x_debug) ZL_LOG("v4l: unmapping %p (%d)", x->x_buf[i], i);
        if (x->x_buf[i]) {
            if (-1 == munmap(x->x_buf[i], x->x_api.v4l2.buffer[i].length)) {
                ZL_PERROR("munmap");
            }
        }
        x->x_buf[i] = NULL;
    }
}
static void v4l2_capture_frame(struct zl_v4l *x) {
    x->x_api.v4l2.buffer[x->x_frame_wr].index  = x->x_frame_wr;
    x->x_api.v4l2.buffer[x->x_frame_wr].type   = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    x->x_api.v4l2.buffer[x->x_frame_wr].memory = V4L2_MEMORY_MMAP;

    if (-1 == ioctl (x->x_tvfd, VIDIOC_DQBUF, &x->x_api.v4l2.buffer[x->x_frame_wr])) {
        switch (errno) {
        case EAGAIN: return;
        case EIO: // could ignore EIO, see spec
        default:
            ZL_PERROR("v4l: error reading buffer");
            goto error;
        }
    }

    // reenqueing buffer
    if (-1 == ioctl (x->x_tvfd, VIDIOC_QBUF, &x->x_api.v4l2.buffer[x->x_frame_wr])) {
        ZL_PERROR("v4l: error queing buffers");
        goto error;
    }
    return;
  error:
    // FIXME: handle error;
    return;
}
static int v4l2_format_supported(unsigned long format) {
    switch(format) {
    case  V4L2_PIX_FMT_YUV420:
    case  V4L2_PIX_FMT_RGB24:
    case  V4L2_PIX_FMT_RGB32:
    case  V4L2_PIX_FMT_YUYV:
    case  V4L2_PIX_FMT_UYVY:
    case  v4l2_fourcc('S', '9', '2', '0'):
        return 1;
    default:
        return 0;
    }
}

/* Add a timeout to avoid waiting forever. */
static bool v42l_sync_timeout(struct zl_v4l *x) {

    fd_set fds;
    struct timeval tv;
    int ret;
    FD_ZERO (&fds);
    FD_SET (x->x_tvfd, &fds);

    // Timeout.
    tv.tv_sec = 5;
    tv.tv_usec = 0;

    ret = select (x->x_tvfd + 1, &fds, NULL, NULL, &tv);

    if (-1 == ret) {
        if (EINTR == errno) goto error;
        ZL_LOG("v4l: select error");
        goto error;
    }
    if (0 == ret) {
        ZL_LOG("v4l: select timeout");
        goto error;
    }

    /* Check how long we've waited. */
    if (0 && x->x_debug) {
        float dt = tv.tv_usec;
        dt /= 1000000;
        dt += tv.tv_sec;
        dt = 5 - dt;
        ZL_LOG("v4l: wait %f ms", dt);
    }
    return false;
  error:
    return true;
}

void v4l2_thread_loop(struct zl_v4l *x) {
    enum v4l2_buf_type type;

    /* Start capturing. */
    type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    if (-1 == ioctl (x->x_tvfd, VIDIOC_STREAMON, &type)) {
        ZL_PERROR("v4l: error starting streaming");
        goto error;
    }
    ZL_LOG("v4l: capture started");

    while (x->x_continue_thread) {

        /* Don't wait forever for something to happen. */
        if (v42l_sync_timeout(x)) goto error;

        /* Get next frame.  Without the sync timeout this will also
           work, but might wait forever. */
        v4l2_capture_frame(x);

        /* Notify reader */
        v4l_notify_frame(x);

    }

    /* Stop capturing. */
    type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    if (-1 == ioctl (x->x_tvfd, VIDIOC_STREAMOFF, &type)) {
        ZL_PERROR("v4l: error stopping streaming");
    }

  error:
    ZL_LOG("v4l: capture thread quitting" );
}
// http://v4l.videotechnology.com/dwg/v4l2.html#CONTROL
void zl_v4l_control(struct zl_v4l *x, int id, int value) {
    const int fd = x->x_tvfd;
    const struct v4l2_queryctrl queryctrl = {
        .id = id,
    };
    /* Check if it's supported. */
    if (-1 == ioctl (fd, VIDIOC_QUERYCTRL, &queryctrl)) {
        if (errno != EINVAL) {
            perror ("VIDIOC_QUERYCTRL");
            goto error;
        } else {
            // see /usr/include/linux/videodev2.h
            // V4L2_CID_CAMERA_CLASS_BASE == 0x00980000
            ZL_LOG("v4l: control %08x is not supported", id);
            perror ("VIDIOC_QUERYCTRL");
        }
    } else if (queryctrl.flags & V4L2_CTRL_FLAG_DISABLED) {
        ZL_LOG("v4l: control %d is not supported", id);
    } else {
        /* Set the control */
        const struct v4l2_control control = {
            .id = id,
            .value = value,
        };
        if (-1 == ioctl (fd, VIDIOC_S_CTRL, &control)) {
            perror ("VIDIOC_S_CTRL");
            goto error;
        }
    }
    return;
  error:
    return;
}

static void v4l2_set_format(struct zl_v4l *x, int index) {
    if (x->x_v4l_api != V4L2) return;

    typeof(x->x_api.v4l2.format) *format = &x->x_api.v4l2.format;
    typeof(format->fmt.pix) *pix = &format->fmt.pix;

    format->type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    pix->pixelformat = x->x_api.v4l2.fmtdesc[index].pixelformat;
    pix->field = V4L2_FIELD_ANY;

    pix->width  = x->x_width;
    pix->height = x->x_height;

    ZL_LOG("v4l: setting format: index %d, pixel format %c%c%c%c",
            index,
            pix->pixelformat & 0xff,
            (pix->pixelformat >>  8) & 0xff,
            (pix->pixelformat >> 16) & 0xff,
            (pix->pixelformat >> 24) & 0xff );

    if (-1 == ioctl(x->x_tvfd, VIDIOC_S_FMT, &x->x_api.v4l2.format)) {
       ZL_LOG("v4l: defaulting to format: pixel format %c%c%c%c",
            pix->pixelformat & 0xff,
            (pix->pixelformat >>  8) & 0xff,
            (pix->pixelformat >> 16) & 0xff,
            (pix->pixelformat >> 24) & 0xff );
    }

    ZL_LOG("v4l: capture format: width %d, height %d, bytesperline %d, image size %d",
          pix->width , pix->height,
          pix->bytesperline, pix->sizeimage );

}

static void v4l2_set_timeperframe(struct zl_v4l *x, int tpf_num, int tpf_den) {
    struct v4l2_streamparm streamparm;
    streamparm.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    streamparm.parm.capture.timeperframe.numerator   = tpf_num;
    streamparm.parm.capture.timeperframe.denominator = tpf_den;

    ZL_LOG("v4l: timeperframe = %d/%d (requested)",
           streamparm.parm.capture.timeperframe.numerator,
           streamparm.parm.capture.timeperframe.denominator);

    if (-1 == ioctl(x->x_tvfd, VIDIOC_S_PARM, &streamparm)) {
        ZL_PERROR("v4l: VIDIOC_S_PARM");
    }
    else {
        /* Actual might differ from requested. */
        ZL_LOG("v4l: timeperframe = %d/%d (actual)",
               streamparm.parm.capture.timeperframe.numerator,
               streamparm.parm.capture.timeperframe.denominator);
    }
    return;
}

static int v4l2_init_mmap(struct zl_v4l *x) {
    if (x->x_v4l_api != V4L2) return -1;

    int i;

    // get mmap numbers 
    x->x_api.v4l2.requestbuffers.count  = x->x_nbbuffers_wanted;
    x->x_api.v4l2.requestbuffers.type   = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    x->x_api.v4l2.requestbuffers.memory = V4L2_MEMORY_MMAP;
    if (-1 == ioctl(x->x_tvfd, VIDIOC_REQBUFS, &x->x_api.v4l2.requestbuffers, 0))
    {
        ZL_LOG("v4l: error : couldn't init driver buffers" ); 
        return -1;
    }
    ZL_LOG("v4l: got %d buffers type %d memory %d", 
        x->x_api.v4l2.requestbuffers.count, x->x_api.v4l2.requestbuffers.type, x->x_api.v4l2.requestbuffers.memory );

    if ( x->x_api.v4l2.requestbuffers.count > ZL_V4L_MAX_BUFFERS )
    {
        ZL_LOG("v4l: this device requires more buffer space" ); 
        return -1;
    }

    x->x_nbbuffers = x->x_api.v4l2.requestbuffers.count;
    ZL_LOG("v4l: using %d buffers", x->x_nbbuffers);

    for (i = 0; i < x->x_nbbuffers; i++) 
    {
        x->x_api.v4l2.buffer[i].index  = i;
        x->x_api.v4l2.buffer[i].type   = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        x->x_api.v4l2.buffer[i].memory = V4L2_MEMORY_MMAP;
        if (-1 == ioctl(x->x_tvfd, VIDIOC_QUERYBUF, &x->x_api.v4l2.buffer[i], 0))
        {
            ZL_LOG("v4l: error : couldn't query buffer %d", i ); 
            return -1;
        }
        x->x_buf[i] = (unsigned char *)
            mmap(NULL, x->x_api.v4l2.buffer[i].length,
                 PROT_READ | PROT_WRITE, MAP_SHARED,
                 x->x_tvfd, x->x_api.v4l2.buffer[i].m.offset);
        if (MAP_FAILED == x->x_buf[i]) {
            ZL_PERROR("v4l: mmap");
            return -1;
        }
    }
    ZL_LOG("v4l: mapped %d buffers", x->x_api.v4l2.requestbuffers.count ); 

    for (i = 0; i < x->x_nbbuffers; i++) 
    {
        x->x_api.v4l2.buffer[i].type        = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        x->x_api.v4l2.buffer[i].memory      = V4L2_MEMORY_MMAP;
        x->x_api.v4l2.buffer[i].index       = i;

        if (-1 == ioctl (x->x_tvfd, VIDIOC_QBUF, &x->x_api.v4l2.buffer[i]))
        {
            ZL_PERROR("v4l: error queing buffers");
            return -1;
        }
    }
    ZL_LOG("v4l: queued %d buffers", x->x_api.v4l2.requestbuffers.count ); 

    return 0;
}

/* This is just for info, since we're not exposing any ranges.  For
   PDP it doesn't matter: if a control doesn't work, it's just ignored
   except for a log message. */
static bool v4l2_check_control(struct zl_v4l *x, int id) {
    struct v4l2_queryctrl ctrl = { .id = id };
    if (-1 == ioctl(x->x_tvfd, VIDIOC_QUERYCTRL, &ctrl) ||
        (ctrl.flags & V4L2_CTRL_FLAG_DISABLED)) {
        return false;
    }
    else if (x->x_debug) {
        const char *name = zl_v4l_control_name(ctrl.id);
        ZL_LOG("v4l: control %08x %s", ctrl.id, name);
    }
    return true;
}
static void v4l2_check_controls(struct zl_v4l *x) {
#define ZL_V4L_CTRL(id) v4l2_check_control(x, V4L2_CID_##id);
    ZL_V4L_CTRL_LIST
#undef ZL_V4L_CTRL
}

static void v4l2_open(struct zl_v4l *x, const char *name, bool start_thread) {

    int i;

    /* if already opened -> close */
    if (x->x_initialized) zl_v4l_close(x);

    ZL_LOG("v4l: opening %s (V4L2)", name);
    x->x_v4l_api = V4L2;
    bzero(&x->x_api.v4l2, sizeof(x->x_api.v4l2));

    x->x_device = name;

    if ((x->x_tvfd = open(name, O_RDWR)) < 0) {
        ZL_LOG("v4l: error: open %s: %s",name,strerror(errno));
        perror(name);
        goto error;
    }

    /* Get capabilities */
    typeof (x->x_api.v4l2.capability) *capability = &x->x_api.v4l2.capability;
    if (ioctl(x->x_tvfd, VIDIOC_QUERYCAP, capability) < 0) {
        perror("get capabilities");
        goto error;
    }
    ZL_LOG("v4l: driver info: %s %d.%d.%d / %s @ %s",
           capability->driver,
           (capability->version >> 16) & 0xff,
           (capability->version >>  8) & 0xff,
           capability->version & 0xff,
           capability->card,
           capability->bus_info);

    /* Get inputs. */
    for (i = 0; i < ZL_V4L_MAX_INPUT; i++) {
        x->x_api.v4l2.input[i].index = i;
        if (-1 == ioctl(x->x_tvfd, VIDIOC_ENUMINPUT, &x->x_api.v4l2.input[i])) {
            // perror("get inputs");
            break;
        }
        else {
            ZL_LOG("v4l: input %d: %s", i, x->x_api.v4l2.input[i].name);
        }
    }
    if (x->x_debug) ZL_LOG("v4l: device has %d inputs", i);
    x->x_api.v4l2.ninputs = i;

    /* Set current input */
    if ( x->x_api.v4l2.ninputs > 0 ) {
         if (ioctl(x->x_tvfd, VIDIOC_S_INPUT, &x->x_curinput) < 0) {
            ZL_PERROR("v4l: error: VIDIOC_S_INPUT");
            ZL_LOG("v4l: cant switch to input %d",x->x_curinput);
        }
        else {
            ZL_LOG("v4l: switched to input %d", x->x_curinput);
        }
    }

    /* Get standards */
    for (i = 0; i < ZL_V4L_MAX_NORM; i++) {
        x->x_api.v4l2.standard[i].index = i;
        if (-1 == ioctl(x->x_tvfd, VIDIOC_ENUMSTD, &x->x_api.v4l2.standard[i])) {
            // perror("get standards");
            break;
        }
        else {
            ZL_LOG("v4l: standard %d : %s",  i, x->x_api.v4l2.standard[i].name );
        }
    }
    if (x->x_debug) ZL_LOG("v4l: device supports %d standards", i );
    x->x_api.v4l2.nstandards = i;

    /* Set norm if available */
    if ( x->x_api.v4l2.nstandards > 0 ) {
        if (ioctl(x->x_tvfd, VIDIOC_S_STD, &x->x_curstandard) < 0) {
            ZL_PERROR("v4l: error: VIDIOC_S_STD");
            ZL_LOG("v4l: cant switch to standard %d",x->x_curstandard);
        }
        else {
            ZL_LOG("v4l: switched to standard %d", x->x_curstandard);
        }
    }

    /* Set frequency. */
    if (x->x_freq > 0) {
        if (ioctl(x->x_tvfd, VIDIOC_S_FREQUENCY, &x->x_freq) < 0)
            perror ("couldn't set frequency :");
    }

    /* formats */
    int last_supported = -1;
    for (i = 0; i < ZL_V4L_MAX_FORMAT; i++) {
        x->x_api.v4l2.fmtdesc[i].index = i;
        x->x_api.v4l2.fmtdesc[i].type  = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        if (-1 == ioctl(x->x_tvfd, VIDIOC_ENUM_FMT, &x->x_api.v4l2.fmtdesc[i])) {
            break;
        }
        else {
            if (v4l2_format_supported(x->x_api.v4l2.fmtdesc[i].pixelformat)) {
                last_supported = i;
            }
            ZL_LOG("v4l: format %d: %s",
                   i, x->x_api.v4l2.fmtdesc[i].description );
        }
    }
    if (x->x_debug) ZL_LOG("v4l: device supports %d formats", i );
    x->x_api.v4l2.nfmtdescs = i;

    /* Use the current format as default and change some parameters. */
    if ( x->x_api.v4l2.nfmtdescs > 0 ) {
        x->x_api.v4l2.format.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        if ((i = ioctl(x->x_tvfd, VIDIOC_G_FMT, &x->x_api.v4l2.format)) < 0) {
            ZL_PERROR("v4l: error: VIDIOC_G_FMT");
            zl_v4l_close_error(x);
            x->x_initialized = false;
        }
        if (last_supported >= 0) {
            x->x_curformat = last_supported;
        }
        ZL_LOG("v4l: setting FOURCC format %08x", x->x_curformat);
        v4l2_set_format(x, x->x_curformat);
    }
    else {
        ZL_LOG("v4l: error : no available formats : closing..." );
        zl_v4l_close_error(x);
        x->x_initialized = false;
        return;
    }

    /* Get controls */
    v4l2_check_controls(x);

    /* Configure buffers */
    if ( v4l2_init_mmap(x) < 0 ) {
        ZL_LOG("v4l: error : couldn't initialize memory mapping : closing..." );
        zl_v4l_close_error(x);
        x->x_initialized = false;
        return;
    }

    x->x_initialized=true;
    ZL_LOG("v4l: device initialized" );

    /* Set framerate: tpf = 1 / fps */
    v4l2_set_timeperframe(x, x->x_fps_den, x->x_fps_num);

    /* Reset pointers */
    x->x_frame_wr = 0;
    x->x_frame_rd = 0;

    /* Create thread if requested. */
    if (start_thread) {
        x->x_continue_thread = 1;
        sem_init(&x->x_frame_ready_sem, 0, 0);
        pthread_create(&x->x_thread_id, 0, v4l_thread, x);
        ZL_LOG("v4l: created thread: %p", x->x_thread_id );
    }

    return;
  error:
    zl_v4l_close_error(x);
    return;
}
const char *v4l2_card(struct zl_v4l *x) {
    if (!x->x_initialized) return "none";
    return (const char*)x->x_api.v4l2.capability.card;
}

static void v4l2_input(struct zl_v4l *x, int inpt) {
    if (!x->x_initialized) {
        ZL_LOG("v4l: cannot set input: no device opened ");
        return;
    }
    if ((inpt < 0) || (inpt >= x->x_api.v4l2.ninputs)) {
        ZL_LOG("v4l:input number %d out of range", inpt);
        return;
    }
    if (x->x_initialized) {
        zl_v4l_close(x);
        x->x_curinput = inpt;
        zl_v4l_reopen(x);
    }
}

static void v4l2_standard(struct zl_v4l *x, int standrd) {
    if (!x->x_initialized){
       ZL_LOG("v4l: cannot set standard: no device opened ");
       return;
    }
    if ( ( standrd < 0 ) || ( standrd >= x->x_api.v4l2.nstandards ) ) {
        ZL_LOG("v4l: standard number %d out of range", standrd );
        return;
    }
    if (x->x_initialized){
        zl_v4l_close(x);
        x->x_curstandard = standrd;
        zl_v4l_reopen(x);
    }
}


#else // !HAVE_V4L2
static void v4l2_unmap(struct zl_v4l *x) {}
static void v4l2_capture_frame(struct zl_v4l *x) {}
#endif







/* Shared */

static void v4l_notify_frame(struct zl_v4l *x) {

    /* Check if reader is keeping up. */
    int nb_frames = x->x_nbbuffers;
    sem_getvalue(&x->x_frame_ready_sem, &nb_frames);
    if (nb_frames >= x->x_nbbuffers) {
        /* Don't let semaphore grow larger than buffer size.
           For PDP: this happens when Pd thread doesn't bang fast enough. */
        // ZL_LOG("v4l: dropped frame");
    }
    else {
        /* Notify there's a new frame. */
        sem_post(&x->x_frame_ready_sem);
        // ZL_LOG("sem_post: %d", x->x_frame_wr);

        /* Increment write pointer */
        x->x_frame_wr = (x->x_frame_wr + 1) % x->x_nbbuffers;
    }
}


int zl_v4l_close(struct zl_v4l *x) {

    /* Terminate thread if there is one */
    if (x->x_continue_thread) {
        if (x->x_debug) ZL_LOG("v4l: stopping thread");
        void *dummy;
        x->x_continue_thread = 0;
        pthread_join (x->x_thread_id, &dummy);
        sem_destroy(&x->x_frame_ready_sem);
    }

    /* Unmap memory */
    switch (x->x_v4l_api) {
#ifdef HAVE_V4L1
    case V4L1: v4l1_unmap(x); break;
#endif
    case V4L2: v4l2_unmap(x); break;
    }

    /* Close device. */
    if (x->x_tvfd >= 0) {
        ZL_LOG("v4l: closing fd %d", x->x_tvfd);
        close(x->x_tvfd);
        x->x_tvfd = -1;
    }
    /* Make sure there's no lingering V4L info. */
    x->x_initialized = false;
    bzero(&x->x_api, sizeof(x->x_api));
    return 0;
}

void zl_v4l_close_error(struct zl_v4l *x) {
    zl_v4l_close(x);
}


void zl_v4l_capture_frame(struct zl_v4l *x) {
    switch(x->x_v4l_api) {
#ifdef HAVE_V4L1
    case V4L1: v4l1_capture_frame(x); break;
#endif
    case V4L2: v4l2_capture_frame(x); break;
    }
}

static void *v4l_thread(void *_x) {
    struct zl_v4l *x = _x;
    switch(x->x_v4l_api) {
#ifdef HAVE_V4L1
    case V4L1: v4l1_thread_loop(x); break;
#endif
    case V4L2: v4l2_thread_loop(x); break;
    }
    x->x_continue_thread = 0; // make sure we won't wait for thread
    return NULL;
}




#define NOT_SUPPORTED() if(0){}

void zl_v4l_pwc_agc(struct zl_v4l *x, float gain __attribute__((__unused__))) {
    switch(x->x_v4l_api) {
#ifdef HAVE_V4L1
    case V4L1: return v4l1_pwc_agc(x, gain);
#endif
    default: NOT_SUPPORTED();
    }
}



void zl_v4l_reopen(struct zl_v4l *x) {
    zl_v4l_open(x, x->x_device, x->x_start_thread);
}

/* The auto-open is not implemented in this file.  See pdp_v4l.c */
void zl_v4l_open_if_necessary(struct zl_v4l *x) {
    if (!x->x_initialized) zl_v4l_reopen(x);
}



const char *zl_v4l_card(struct zl_v4l *x) {
    switch(x->x_v4l_api) {
    case V4L2: return v4l2_card(x);
    default:
        NOT_SUPPORTED();
        return "unknown";
    }
}

const char *zl_v4l_control_name(int id) {
    const char *name;
    switch(id) {
#define ZL_V4L_CTRL(id) case V4L2_CID_##id: name = #id; break;
        ZL_V4L_CTRL_LIST
#undef ZL_V4L_CTRL
    default:
        name = "unknown";
    }
    return name;
}
void zl_v4l_set_fps(struct zl_v4l *x, int fps_num, int fps_den) {
    /* Needs re-open.  Can't set when capture is on. */
    zl_v4l_close(x);
    x->x_fps_num = fps_num;
    x->x_fps_den = fps_den;
    zl_v4l_reopen(x);
}


void zl_v4l_open(struct zl_v4l *x, const char *name, bool start_thread) {
    x->x_device = name;

    v4l2_open(x, name, start_thread);
    if (!x->x_initialized) {
#ifdef HAVE_V4L1
        v4l1_open(x, name);
#endif
    }
}
void zl_v4l_input(struct zl_v4l *x, int inpt) {
    switch(x->x_v4l_api) {
    case V4L2: v4l2_input(x, inpt); break;
    default: NOT_SUPPORTED();
    }
}

void zl_v4l_standard(struct zl_v4l *x, int standrd) {
    switch(x->x_v4l_api) {
    case V4L2: v4l2_standard(x, standrd); break;
    default: NOT_SUPPORTED();
    }
}


void zl_v4l_freq(struct zl_v4l *x, int freq /* 1/16th of MHz */) {
    if (!x->x_initialized) {
        ZL_LOG("v4l: cannot set frequency: no device opened ");
        return;
    }
    x->x_freq = freq;
    if (x->x_freq <= 0) {
        ZL_LOG("v4l: freq needs to be positive");
        return;
    }

    switch(x->x_v4l_api) {
#ifdef HAVE_V4l1
    case V4L1:
        if (ioctl(x->x_tvfd, VIDIOCSFREQ, &x->x_freq) < 0) {
            perror("couldn't set frequency:");
            return;
        }
        break;
#endif
#ifdef HAVE_V4L2
    case V4L2:
        if (ioctl(x->x_tvfd, VIDIOC_S_FREQUENCY, &x->x_freq) < 0) {
            perror("couldn't set frequency:");
            return;
        }
        break;
#endif
    }
    ZL_LOG("v4l: tuner frequency set to: %f MHz", ((float)freq) / 16.0f);
}





void zl_v4l_next(struct zl_v4l *x, unsigned char **newimage) {
    *newimage = NULL;

    /* If you want auto-open, call zl_v4l_open_if_necessary() before _next () */
    if (!(x->x_initialized)) return;

    /* do nothing if there is no frame ready */
    int val = 0;
    sem_getvalue(&x->x_frame_ready_sem, &val);
    if (val == 0) return;

    /* Because of the previous check, this does not block. */
    sem_wait(&x->x_frame_ready_sem);
    // ZL_LOG("sem_wait: %d", x->x_frame_rd);

    /* Get the raw data + set the fourcc tag. */
    switch(x->x_v4l_api) {
#ifdef HAVE_V4L1
    case V4L1: *newimage = v4l1_image_dequeue(x); break;
#endif
    case V4L2: *newimage = v4l2_image_dequeue(x); break;
    }

    /* Get ready for next frame */
    x->x_frame_rd = (x->x_frame_rd + 1) % x->x_nbbuffers;
}

void zl_v4l_get_format(struct zl_v4l *x, unsigned int *fourcc,
                       unsigned int *width, unsigned int *height) {
    *width  = x->x_width;
    *height = x->x_height;
    *fourcc = 0;
    switch(x->x_v4l_api) {
#ifdef HAVE_V4L1
    case V4L1:
        switch (x->x_api.v4l1.palette) {
        case VIDEO_PALETTE_YUV420P: *fourcc = V4L2_PIX_FMT_YUV420; break;
        case VIDEO_PALETTE_RGB24:   *fourcc = V4L2_PIX_FMT_RGB24;  break;
        case VIDEO_PALETTE_RGB32:   *fourcc = V4L2_PIX_FMT_RGB32;  break;
        case VIDEO_PALETTE_YUV422:  *fourcc = V4L2_PIX_FMT_YUYV;   break;
        default: break;
        }
        break;
#endif
#ifdef HAVE_V4L2
    case V4L2:
        *fourcc = x->x_api.v4l2.format.fmt.pix.pixelformat;
        break;
#endif
    }
}

/* In PDP, setting the attribute re-opens the connection if it's
   already open.  As a quick hack to avoid duplication these macros go
   around the setter to do the conditional open/close. */

#define MEMBER_TYPE(member) typeof(((struct zl_v4l *)NULL)->x_##member)

#define DEFINE_SETTER_1(member)                                         \
    void zl_v4l_set_##member(struct zl_v4l *x, MEMBER_TYPE(member) value) { \
        if (value == x->x_##member) return;                             \
        bool do_re = x->x_initialized;                                  \
        if (do_re) zl_v4l_close(x);                                     \
        x->x_##member = value;                                          \
        if (do_re) zl_v4l_reopen(x);                                    \
    }

DEFINE_SETTER_1(norm)
DEFINE_SETTER_1(format)
DEFINE_SETTER_1(channel)
static DEFINE_SETTER_1(nbbuffers_wanted)


void zl_v4l_set_dim(struct zl_v4l *x, unsigned int w, unsigned int h) {
    if ((w == x->x_width) && (h == x->x_height)) return;
    bool do_re = x->x_initialized;
    if (do_re) zl_v4l_close(x);
    x->x_width = w;
    x->x_height = h;
    if (do_re) zl_v4l_reopen(x);
}

void zl_v4l_set_nb_buf(struct zl_v4l *x, unsigned int nb_buf) {
    if (nb_buf > ZL_V4L_MAX_BUFFERS) nb_buf = ZL_V4L_MAX_BUFFERS;
    if (nb_buf < 2) nb_buf = 2; // doesn't make much sense to take less
    zl_v4l_set_nbbuffers_wanted(x, nb_buf);
}

void zl_v4l_init(struct zl_v4l *x, bool start_thread) {
    x->x_initialized = false;
    x->x_tvfd = -1;
    x->x_format = 0; // V4L1: unify this
    x->x_curinput = 0;
    x->x_curstandard = 0;
    x->x_curformat = 0;
    x->x_freq = -1;
    x->x_frame_wr = 0;
    x->x_frame_rd = 0;
    x->x_continue_thread = 0;
    x->x_width = 320;
    x->x_height = 240;
    x->x_device = "/dev/video0";
    x->x_channel = 0;
    x->x_norm = 0; // PAL
    x->x_freq = -1; //don't set freq by default
    x->x_fps_num = 30;
    x->x_fps_den = 1;
#if 0 // FIXME: not used
    x->x_minwidth = pdp_imageproc_legalwidth(0);
    x->x_maxwidth = pdp_imageproc_legalwidth_round_down(0x7fffffff);
    x->x_minheight = pdp_imageproc_legalheight(0);
    x->x_maxheight = pdp_imageproc_legalheight_round_down(0x7fffffff);
#endif
    x->x_start_thread = start_thread;
    x->x_debug = 1;

    /* Not sure if this is still necessary. */
    x->x_nbbuffers_wanted = 2;
}



