#ifdef SWIG
%module v4l
%{
#include "v4l.h"
%}
#endif

#ifndef _ZL_V4L_
#define _ZL_V4L_

#include "zl/config.h"
#include <pthread.h>
#include <stdbool.h>
#include <semaphore.h>

// FIXME
// #define HAVE_V4L1
#define HAVE_V4L2

#ifdef HAVE_V4L2
#include <linux/videodev2.h>
#else
/* Doesn't matter, as long as they are unique. */
#define  V4L2_PIX_FMT_YUV420 1
#define  V4L2_PIX_FMT_RGB24  2
#define  V4L2_PIX_FMT_RGB32  3
#define  V4L2_PIX_FMT_YUYV   4
#define  V4L2_PIX_FMT_UYVY   5
#define  V4L2_PIX_FMT_PAC207 6
#define v4l2_fourcc(...) 7
#endif

#ifdef HAVE_V4L1
#ifdef HAVE_LIBV4L1_VIDEODEV_H
# include <libv4l1-videodev.h>
#else
//#warning trying to use deprecated V4L-1 API
# include <linux/videodev.h>
#endif
#endif


/* This was on 8 in Yves' code.  Not sure why.  2 is much better, so I
   leave it configurable, but default set to 2. */
#define ZL_V4L_MAX_BUFFERS 8

#define ZL_V4L_DEVICENO 0
#define ZL_V4L_NBUF 2
#define ZL_V4L_COMPOSITEIN 1
#define ZL_V4L_MAX_INPUT   16
#define ZL_V4L_MAX_NORM    16
#define ZL_V4L_MAX_FORMAT  32
#define ZL_V4L_MAX_CTRL    32

struct zl_v4l {

    unsigned long x_format; // 0 means autodetect
    unsigned long x_channel;
    unsigned long x_norm;

    bool x_initialized;
    bool x_open_retry;
    bool x_start_thread; // start thread on re-open

    unsigned int x_width;
    unsigned int x_height;
    int x_freq;

    int x_tvfd;
    int x_frame_wr; // capture thread writes this frame
    int x_frame_rd; // main thread reads this frame
    int x_skipnext;
    int x_mytopmargin, x_mybottommargin;
    int x_myleftmargin, x_myrightmargin;

    int x_debug;

    /* V4L only uses a single mapped region which has 2 halves.
       V4L2 uses separately mapped buffers. */
    int x_nbbuffers_wanted;
    int x_nbbuffers;
    unsigned char *x_buf[ZL_V4L_MAX_BUFFERS];

    //int x_pdp_image_type;

    pthread_t x_thread_id;
    int x_continue_thread;

    sem_t x_frame_ready_sem;

    int x_curinput;
    int x_curstandard;
    int x_curformat;

    int x_fps_num;
    int x_fps_den;

    /* FIXME: Against ZL principles to keep this external pointer
       here, so make sure it is an object with infinite extent. */
    const char *x_device;

    /* This uses V4L2 format descs, also for the V4L1 */
    unsigned long fourcc;

    enum {
#ifdef HAVE_V4L1
        V4L1,
#endif
#ifdef HAVE_V4L2
        V4L2,
#endif
    } x_v4l_api;
    union {
#ifdef HAVE_V4L1
        struct {
            int palette;
            struct video_tuner      tuner;
            struct video_picture    picture;
            struct video_buffer     buffer;
            struct video_capability cap;
            struct video_channel    channel;
            struct video_audio      audio;
            struct video_mbuf       mbuf;
            struct video_mmap       mmap[ZL_V4L_NBUF];
            struct video_window     win;
        } v4l1;
#endif
#ifdef HAVE_V4L2
        struct {
            int ninputs;
            int nstandards;
            int nfmtdescs;
            struct v4l2_capability     capability;
            struct v4l2_input          input[ZL_V4L_MAX_INPUT];
            struct v4l2_standard       standard[ZL_V4L_MAX_NORM];
            struct v4l2_fmtdesc        fmtdesc[ZL_V4L_MAX_FORMAT];
            struct v4l2_streamparm     streamparam;
            // struct v4l2_queryctrl      queryctrl[ZL_V4L_MAX_CTRL];
            // struct v4l2_queryctrl      queryctrl_priv[ZL_V4L_MAX_CTRL];
            struct v4l2_buffer         buffer[ZL_V4L_MAX_BUFFERS];
            struct v4l2_format         format;
            struct v4l2_requestbuffers requestbuffers;
        } v4l2;
#endif
    } x_api;
};

int zl_v4l_close(struct zl_v4l *x);
void zl_v4l_close_error(struct zl_v4l *x);
void zl_v4l_control(struct zl_v4l *x, int id, int value);
void zl_v4l_pwc_agc(struct zl_v4l *x, float gain);
void zl_v4l_open(struct zl_v4l *x, const char *name, bool start_thread);
void zl_v4l_reopen(struct zl_v4l *x);
void zl_v4l_open_if_necessary(struct zl_v4l *x);
void zl_v4l_input(struct zl_v4l *x, int inpt);
void zl_v4l_standard(struct zl_v4l *x, int standrd);
void zl_v4l_freq(struct zl_v4l *x, int freq /* 1/16th of MHz */);
void zl_v4l_next(struct zl_v4l *x, unsigned char **newimage);
void zl_v4l_get_format(struct zl_v4l *x, unsigned int *fourcc,
                       unsigned int *width, unsigned int *height);

void zl_v4l_set_dim(struct zl_v4l *x, unsigned int w, unsigned int h);
void zl_v4l_set_norm(struct zl_v4l *x, unsigned long norm);
void zl_v4l_set_format(struct zl_v4l *x, unsigned long format);
void zl_v4l_set_channel(struct zl_v4l *x, unsigned long channel);
void zl_v4l_set_fps(struct zl_v4l *x, int num, int den);

void zl_v4l_init(struct zl_v4l *x, bool start_thread);

const char *zl_v4l_card(struct zl_v4l *x);
const char *zl_v4l_control_name(int id);

/* Useful for abstracting over symbolic names. */
#define ZL_V4L_CTRL_LIST                \
    ZL_V4L_CTRL(BRIGHTNESS) \
    ZL_V4L_CTRL(CONTRAST) \
    ZL_V4L_CTRL(SATURATION) \
    ZL_V4L_CTRL(HUE) \
    ZL_V4L_CTRL(AUDIO_VOLUME) \
    ZL_V4L_CTRL(AUDIO_BALANCE) \
    ZL_V4L_CTRL(AUDIO_BASS) \
    ZL_V4L_CTRL(AUDIO_TREBLE) \
    ZL_V4L_CTRL(AUDIO_MUTE) \
    ZL_V4L_CTRL(AUDIO_LOUDNESS) \
    ZL_V4L_CTRL(BLACK_LEVEL) \
    ZL_V4L_CTRL(AUTO_WHITE_BALANCE) \
    ZL_V4L_CTRL(DO_WHITE_BALANCE) \
    ZL_V4L_CTRL(RED_BALANCE) \
    ZL_V4L_CTRL(BLUE_BALANCE) \
    ZL_V4L_CTRL(GAMMA) \
    ZL_V4L_CTRL(EXPOSURE) \
    ZL_V4L_CTRL(AUTOGAIN) \
    ZL_V4L_CTRL(GAIN) \
    ZL_V4L_CTRL(HFLIP) \
    ZL_V4L_CTRL(VFLIP) \
    ZL_V4L_CTRL(HCENTER) \
    ZL_V4L_CTRL(VCENTER) \
    ZL_V4L_CTRL(POWER_LINE_FREQUENCY) \
    ZL_V4L_CTRL(HUE_AUTO) \
    ZL_V4L_CTRL(WHITE_BALANCE_TEMPERATURE) \
    ZL_V4L_CTRL(SHARPNESS) \
    ZL_V4L_CTRL(BACKLIGHT_COMPENSATION) \
    ZL_V4L_CTRL(CHROMA_AGC) \
    ZL_V4L_CTRL(COLOR_KILLER) \
    ZL_V4L_CTRL(COLORFX) \
    ZL_V4L_CTRL(AUTOBRIGHTNESS) \
    ZL_V4L_CTRL(BAND_STOP_FILTER) \
    ZL_V4L_CTRL(EXPOSURE_AUTO) \
    ZL_V4L_CTRL(EXPOSURE_ABSOLUTE) \
    ZL_V4L_CTRL(EXPOSURE_AUTO_PRIORITY) \
    ZL_V4L_CTRL(PAN_RELATIVE) \
    ZL_V4L_CTRL(TILT_RELATIVE) \
    ZL_V4L_CTRL(PAN_RESET) \
    ZL_V4L_CTRL(TILT_RESET) \
    ZL_V4L_CTRL(PAN_ABSOLUTE) \
    ZL_V4L_CTRL(TILT_ABSOLUTE) \
    ZL_V4L_CTRL(FOCUS_ABSOLUTE) \
    ZL_V4L_CTRL(FOCUS_RELATIVE) \
    ZL_V4L_CTRL(FOCUS_AUTO) \
    ZL_V4L_CTRL(ZOOM_ABSOLUTE) \
    ZL_V4L_CTRL(ZOOM_RELATIVE) \
    ZL_V4L_CTRL(ZOOM_CONTINUOUS) \
    ZL_V4L_CTRL(PRIVACY)

#endif
