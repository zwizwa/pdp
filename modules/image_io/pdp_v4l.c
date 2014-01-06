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
 *   2012-10-29 : V4L2 support from Pidip's pdp_v4l2
 *                Thanks to Yves Degoyon, Lluis Gomez i Bigorda, Gerd Knorr)
 *
 */



#include "pdp_config.h"
#include "pdp.h"
#include "pdp_llconv.h"
#include "pdp_imageproc.h"

#include "zl/v4l.h"

#include <ctype.h>

static const int UVTranslate[32] = {0, 1, 2, 3, 
                        8, 9, 10, 11, 
                        16, 17, 18, 19, 
                        24, 25, 26, 27, 
                        4, 5, 6, 7,
                        12, 13, 14, 15,
                        20, 21, 22, 23,
                        28, 29, 30, 31};

static const int Y_coords_624x[128][2] = {
{ 0,  0}, { 1,  0}, { 2,  0}, { 3,  0}, { 4,  0}, { 5,  0}, { 6,  0}, { 7,  0},
{ 0,  1}, { 1,  1}, { 2,  1}, { 3,  1}, { 4,  1}, { 5,  1}, { 6,  1}, { 7,  1},
{ 0,  2}, { 1,  2}, { 2,  2}, { 3,  2}, { 4,  2}, { 5,  2}, { 6,  2}, { 7,  2},
{ 0,  3}, { 1,  3}, { 2,  3}, { 3,  3}, { 4,  3}, { 5,  3}, { 6,  3}, { 7,  3},

{ 0,  4}, { 1,  4}, { 2,  4}, { 3,  4}, { 4,  4}, { 5,  4}, { 6,  4}, { 7,  4},
{ 0,  5}, { 1,  5}, { 2,  5}, { 3,  5}, { 4,  5}, { 5,  5}, { 6,  5}, { 7,  5},
{ 0,  6}, { 1,  6}, { 2,  6}, { 3,  6}, { 4,  6}, { 5,  6}, { 6,  6}, { 7,  6},
{ 0,  7}, { 1,  7}, { 2,  7}, { 3,  7}, { 4,  7}, { 5,  7}, { 6,  7}, { 7,  7},

{ 8,  0}, { 9,  0}, {10,  0}, {11,  0}, {12,  0}, {13,  0}, {14,  0}, {15,  0},
{ 8,  1}, { 9,  1}, {10,  1}, {11,  1}, {12,  1}, {13,  1}, {14,  1}, {15,  1},
{ 8,  2}, { 9,  2}, {10,  2}, {11,  2}, {12,  2}, {13,  2}, {14,  2}, {15,  2},
{ 8,  3}, { 9,  3}, {10,  3}, {11,  3}, {12,  3}, {13,  3}, {14,  3}, {15,  3},

{ 8,  4}, { 9,  4}, {10,  4}, {11,  4}, {12,  4}, {13,  4}, {14,  4}, {15,  4},
{ 8,  5}, { 9,  5}, {10,  5}, {11,  5}, {12,  5}, {13,  5}, {14,  5}, {15,  5},
{ 8,  6}, { 9,  6}, {10,  6}, {11,  6}, {12,  6}, {13,  6}, {14,  6}, {15,  6},
{ 8,  7}, { 9,  7}, {10,  7}, {11,  7}, {12,  7}, {13,  7}, {14,  7}, {15,  7}
};

static void do_write_u(const unsigned char *buf, unsigned char *ptr,
        int i, int j)
{
        *ptr = buf[i + 128 + j];
}

static void do_write_v(const unsigned char *buf, unsigned char *ptr,
        int i, int j)
{
        *ptr = buf[i + 160 + j];
}


void v4lconvert_sn9c20x_to_yuv420(const unsigned char *raw, unsigned char *i420,
  int width, int height, int yvu)
{
        int i = 0, x = 0, y = 0, j, relX, relY, x_div2, y_div2;
        const unsigned char *buf = raw;
        unsigned char *ptr;
        int frame_size = width * height;
        int frame_size_div2 = frame_size >> 1;
        int frame_size_div4 = frame_size >> 2;
        int width_div2 = width >> 1;
        int height_div2 = height >> 1;
        void (*do_write_uv1)(const unsigned char *buf, unsigned char *ptr, int i,
        int j) = NULL;
        void (*do_write_uv2)(const unsigned char *buf, unsigned char *ptr, int i,
        int j) = NULL;

        if (yvu) {
                do_write_uv1 = do_write_v;
                do_write_uv2 = do_write_u;
        }
        else {
                do_write_uv1 = do_write_u;
                do_write_uv2 = do_write_v;
        }

        while (i < (frame_size + frame_size_div2)) {
                for (j = 0; j < 128; j++) {
                        relX = x + Y_coords_624x[j][0];
                        relY = y + Y_coords_624x[j][1];

#if (DO_SANITY_CHECKS==1)
                        if ((relX < width) && (relY < height)) {
#endif
                                ptr = i420 + relY * width + relX;
                                *ptr = buf[i + j];
#if (DO_SANITY_CHECKS==1)
                        }
#endif

                }
                x_div2 = x >> 1;
                y_div2 = y >> 1;
                for (j = 0; j < 32; j++) {
                        relX = (x_div2) + (j & 0x07);
                        relY = (y_div2) + (j >> 3);

#if (DO_SANITY_CHECKS==1)
                        if ((relX < width_div2) && (relY < height_div2)) {
#endif
                                ptr = i420 + frame_size +
                                        relY * width_div2 + relX;
                                do_write_uv1(buf, ptr, i, j);
                                ptr += frame_size_div4;
                                do_write_uv2(buf, ptr, i, j);
#if (DO_SANITY_CHECKS==1)
                        }
#endif
                }

                i += 192;
                x += 16;
                if (x >= width) {
                        x = 0;
                        y += 8;
                }
        }
}



/* Pd / PDP stuff */

typedef struct pdp_v4l_struct
{
    /* Pd stuff */
    t_object x_obj;
    t_float x_f;
    t_outlet *x_outlet0;
    t_outlet *x_outlet1;
    t_symbol *x_device;
    t_symbol *x_image_type;


#if 0 // FIXME: not used
    u32 x_minwidth;
    u32 x_maxwidth;
    u32 x_minheight;
    u32 x_maxheight;
#endif

    /* Most of the work is done in the atapter object. */
    struct zl_v4l zl;

    /* Generic */

} t_pdp_v4l;




/* Shared code (independent of V4L1/V4L2) */
static void pdp_v4l_close(t_pdp_v4l *x) {
    zl_v4l_close(&x->zl);
}
static void pdp_v4l_open(t_pdp_v4l *x, t_symbol *name) {
    zl_v4l_open(&x->zl, name->s_name, true);
}
static void pdp_v4l_close_error(t_pdp_v4l *x) {
    zl_v4l_close_error(&x->zl);
}


static void control(t_pdp_v4l *x, int id, int value) {
    const char *name = zl_v4l_control_name(id);
    post("pdp_v4l: control (%08x) %s %d", id, name, value);
    zl_v4l_control(&x->zl, id, value);
}

static void pdp_v4l_control(t_pdp_v4l *x, t_symbol *s, t_float f1, t_float f2) {
    /* | "id" id value < */
    if (s == gensym("id")) {
        control(x, f1, f2);
    }
    /* Named controls.  These come straight from /usr/include/linux/0videodev2.h */
#define ZL_V4L_CTRL(id,...) else if (s == gensym(#id)) control(x, V4L2_CID_ ##id, f1);
    ZL_V4L_CTRL_LIST
#undef ZL_V4L_CTRL
    else {
        post("pdp_v4l: unknown control %s", s->s_name);
    }
}
static void pdp_v4l_input(t_pdp_v4l *x, t_float f) {
    zl_v4l_input(&x->zl, f);
}

static void pdp_v4l_standard(t_pdp_v4l *x, t_float f) {
    zl_v4l_standard(&x->zl, f);
}


static void pdp_v4l_freq(t_pdp_v4l *x, t_float f) {
    zl_v4l_freq(&x->zl, f);
}

static void pdp_v4l_freqMHz(t_pdp_v4l *x, t_float f) {
    zl_v4l_freq(&x->zl, f*16.0f);
}



static void pdp_v4l_bang(t_pdp_v4l *x) {

    /* Make some effort to have an open device. */
    int tries = 3;
    while (tries--) zl_v4l_open_if_necessary(&x->zl);

    /* Get raw image data */
    unsigned char *newimage = NULL;
    zl_v4l_next(&x->zl, &newimage);
    if (NULL == newimage) return;

    unsigned int fourcc;
    unsigned int w,h;
    zl_v4l_get_format(&x->zl, &fourcc, &w, &h);

    int pdp_packt = pdp_packet_new_image(PDP_IMAGE_YV12, w, h);
    t_pdp* header = pdp_packet_header(pdp_packt);
    if (!header){
        post("pdp_v4l: ERROR: can't allocate packet");
        return;
    }

    t_image *image = pdp_packet_image_info(pdp_packt);
    short int *data = (short int *) pdp_packet_data(pdp_packt);

    /* convert data to pdp packet */
    switch(fourcc) {
    case  V4L2_PIX_FMT_YUV420: pdp_llconv(newimage, RIF_YUV__P411_U8, data, RIF_YVU__P411_S16, w, h); break;
    case  V4L2_PIX_FMT_RGB24:  pdp_llconv(newimage, RIF_BGR__P____U8, data, RIF_YVU__P411_S16, w, h); break;
    case  V4L2_PIX_FMT_RGB32:  pdp_llconv(newimage, RIF_BGRA_P____U8, data, RIF_YVU__P411_S16, w, h); break;
    case  V4L2_PIX_FMT_YUYV:   pdp_llconv(newimage, RIF_YUYV_P____U8, data, RIF_YVU__P411_S16, w, h); break;
    case  V4L2_PIX_FMT_UYVY:   pdp_llconv(newimage, RIF_UYVY_P____U8, data, RIF_YVU__P411_S16, w, h); break;
    case  v4l2_fourcc('S', '9', '2', '0'):
        v4lconvert_sn9c20x_to_yuv420( newimage, (unsigned char*)data, w, h, 1);
        pdp_llconv(data, RIF_YUV__P411_U8, newimage, RIF_YVU__P411_S16, w, h);
        memcpy(data, newimage, w * h *2);
        break;
    case V4L2_PIX_FMT_PAC207:
        /* GSPCA compressed ompressed RGGB bayer */
    default:
        /* For more inspiration see:
           http://www.lavrsen.dk/svn/motion/trunk/video2.c
           http://www.lavrsen.dk/svn/motion/trunk/video_common.c
           http://www.siliconimaging.com/RGB%20Bayer.htm
        */
        post("pdp_v4l: unsupported color model: %08x", fourcc);
        break;
    }

    pdp_packet_pass_if_valid(x->x_outlet0, &pdp_packt);

}


static void pdp_v4l_dim(t_pdp_v4l *x, t_floatarg xx, t_floatarg yy) {


    unsigned int w,h;

    w  = pdp_imageproc_legalwidth((int)xx);
    h  = pdp_imageproc_legalheight((int)yy);

    post("dim %d x %d (%f x %f)\n", w,h,xx,yy);

#if 0 // FIXME: not used
    w = (w < x->x_maxwidth) ? w : x->x_maxwidth;
    w = (w > x->x_minwidth) ? w : x->x_minwidth;

    h = (h < x->x_maxheight) ? h : x->x_maxheight;
    h = (h > x->x_minheight) ? h : x->x_minheight;
#endif

    zl_v4l_set_dim(&x->zl, w, h);
}


static void pdp_v4l_free(t_pdp_v4l *x) {
    pdp_v4l_close(x);
}

static void pdp_v4l_norm(t_pdp_v4l *x, t_symbol *s) {
#ifdef HAVE_V4L1
    unsigned int norm;
    if (gensym("PAL") == s) norm = VIDEO_MODE_PAL;
    else if (gensym("NTSC") == s) norm = VIDEO_MODE_NTSC;
    else if (gensym("SECAM") == s) norm = VIDEO_MODE_SECAM;
    else norm = VIDEO_MODE_AUTO;
    zl_v4l_set_norm(&x->zl, norm);
#endif
}


static void pdp_v4l_format(t_pdp_v4l *x, t_symbol *s) {
    unsigned int format = 0;
    if      (s == gensym("YUV420P")) format = 1;
    else if (s == gensym("YUV422"))  format = 2;
    else if (s == gensym("RGB24"))   format = 3;
    else if (s == gensym("RGB32"))   format = 4;
    else if (s == gensym("auto"))    format = 0;
    else {
        post("pdp_v4l: format %s unknown, using autodetect", s->s_name);
        format = 0;
    }
    zl_v4l_set_format(&x->zl, format);
}
static void pdp_v4l_channel(t_pdp_v4l *x, t_float channel) {
    zl_v4l_set_channel(&x->zl, channel);
}

static void pdp_v4l_info(t_pdp_v4l *x) {
    const char *card = zl_v4l_card(&x->zl);
    char c, nospace_card[strlen(card)+1];
    int i = 0;
    while (0 != (c = card[i])) {
        nospace_card[i++] = isalnum(c) ? c : '_';
    }
    nospace_card[i] = 0;
    t_atom atom[2];
    SETSYMBOL(atom+0, gensym("card"));
    SETSYMBOL(atom+1, gensym(nospace_card));
#if 0
    // "list" may not be the proper way to do this...
    outlet_anything(x->x_outlet1, gensym("list"), 2, atom);
#else
    outlet_anything(x->x_outlet1, atom[0].a_w.w_symbol, 1, atom+1);
#endif
}



static void pdp_v4l_pwc_agc(t_pdp_v4l *x, t_float gain) {
    return zl_v4l_pwc_agc(&x->zl, gain);
}

static void pdp_v4l_fps(t_pdp_v4l *x, t_floatarg num, t_floatarg den) {
    int iden = den;
    if (iden == 0) iden = 1;
    return zl_v4l_set_fps(&x->zl, num, iden);
}



t_class *pdp_v4l_class;



void *pdp_v4l_new(t_symbol *vdef, t_symbol *format)
{
    t_pdp_v4l *x = (t_pdp_v4l *)pd_new(pdp_v4l_class);

    x->x_outlet0 = outlet_new(&x->x_obj, &s_anything);
    x->x_outlet1 = outlet_new(&x->x_obj, &s_anything);

    zl_v4l_init(&x->zl, true);

    if (format != gensym("")){
        pdp_v4l_format(x, format);
    }

    return (void *)x;
}

#ifdef __cplusplus
extern "C"
{
#endif

void pdp_v4l_setup(void) {

    pdp_v4l_class = class_new(gensym("pdp_v4l"), (t_newmethod)pdp_v4l_new,
            (t_method)pdp_v4l_free, sizeof(t_pdp_v4l), 0, A_DEFSYMBOL, A_DEFSYMBOL, A_NULL);

    class_addmethod(pdp_v4l_class, (t_method)pdp_v4l_bang, gensym("bang"), A_NULL);
    // class_addmethod(pdp_v4l_class, (t_method)pdp_v4l_audio, gensym("audio"), A_NULL);
    class_addmethod(pdp_v4l_class, (t_method)pdp_v4l_close, gensym("close"), A_NULL);
    class_addmethod(pdp_v4l_class, (t_method)pdp_v4l_open, gensym("open"), A_SYMBOL, A_NULL);
    class_addmethod(pdp_v4l_class, (t_method)pdp_v4l_channel, gensym("channel"), A_FLOAT, A_NULL);
    class_addmethod(pdp_v4l_class, (t_method)pdp_v4l_norm, gensym("norm"), A_SYMBOL, A_NULL);
    class_addmethod(pdp_v4l_class, (t_method)pdp_v4l_standard, gensym("standard"), A_FLOAT, A_NULL);
    class_addmethod(pdp_v4l_class, (t_method)pdp_v4l_input, gensym("input"), A_FLOAT, A_NULL);
    class_addmethod(pdp_v4l_class, (t_method)pdp_v4l_dim, gensym("dim"), A_FLOAT, A_FLOAT, A_NULL);
    class_addmethod(pdp_v4l_class, (t_method)pdp_v4l_freq, gensym("freq"), A_FLOAT, A_NULL);
    class_addmethod(pdp_v4l_class, (t_method)pdp_v4l_freqMHz, gensym("freqMHz"), A_FLOAT, A_NULL);
    class_addmethod(pdp_v4l_class, (t_method)pdp_v4l_pwc_agc, gensym("gain"), A_FLOAT, A_NULL);
    class_addmethod(pdp_v4l_class, (t_method)pdp_v4l_fps, gensym("fps"), A_FLOAT, A_DEFFLOAT, A_NULL);
    class_addmethod(pdp_v4l_class, (t_method)pdp_v4l_format, gensym("captureformat"), A_SYMBOL, A_NULL);
    class_addmethod(pdp_v4l_class, (t_method)pdp_v4l_control, gensym("control"), A_SYMBOL, A_FLOAT, A_DEFFLOAT, A_NULL);
    class_addmethod(pdp_v4l_class, (t_method)pdp_v4l_info, gensym("info"), A_NULL);

}

#ifdef __cplusplus
}
#endif
