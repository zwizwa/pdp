/*
 *   Pure Data Packet. portable image processing routines.
 *   Copyright (c) 2003-2012 by Tom Schouten <tom@zwizwa.be>
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



#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <math.h>

/* Check this file for the relation between the more generic names and
   the direct wrappers for the MMX instructions.
   For GCC 4.6 on Debian this is in:
   /usr/lib/gcc/x86_64-linux-gnu/4.6/include/mmintrin.h */
#include <mmintrin.h>

/* General remarks on MMX code.

   PMUL
   ----

   There is only one kind of multiplication implemented by 2
   instructions, both operating on signed 16 bit values grouped in 4
   element vectors

   (4 x s16, 4 x s16) -> 4 x s16 (hi)  | 4 x u16 (lo)

   _mm_mullo_pi16() -> returns low 16 bits
   _mm_mulhi_pi16() -> returns high 16 bits

   Most code below uses _mm_mulhi_pi16()

   If you _interpret_ the data as fixed point s.15, meaing 1 sign bit,
   15 fractional bits, the _interpretation_ of _mm_mulhi_pi16(a,b) is
   (a x b) / 2


   PACK / PUNPCK
   -------------

   In general:

   PACK:   Reduce the word size by throwing away the upper bits
   PUNPCK: Increase the word size by appending upper bits.

   PACK comes in two variants: signed and unsigned saturation.  This
   is fairly straightforward.  If you want to throw away lower bits,
   do a shift first.

   PACK does not interleave vectors.

   PUNPCK is the only one that interleaves vectors.  It comes in two
   variants, taking data from the low or high words of the operand
   vectors.


   http://www.tommesani.com/MMXPrimer.html

*/



#include "pdp_imageproc.h"

#ifdef DEBUG
#define INLINE
#else
#define INLINE __attribute__((always_inline))
#endif




/* pdp memory alloc/dealloc prototype */
void *pdp_alloc(int size);
void pdp_dealloc(void *);

/* This should be the native signed machine word, i.e. 64 bit on 64
   bit arch, 32 bit on 32 bit arch. */
typedef int word_t;
typedef union {
    __m64 v;
    s16 s[4];  // short
    u16 us[4]; // short
    word_t w; // only low 32 bits are used
} v4_t;

typedef union {
    __m64 v;
    u32 l[2]; // long
} v2_t;


#define CLAMP16(x) (((x) > 0x7fff) ? 0x7fff : (((x) < -0x7fff) ? -0x7fff : (x)))

// utility stuff
inline static s16 float2fixed(float f)
{
    if (f > 1) f = 1;
    if (f < -1) f = -1;
    f *= 0x7fff;
    return (s16)f;
}

inline static void setvec(s16 *v, float f)
{
    s16 a = float2fixed(f);
    v[0] = a;
    v[1] = a;
    v[2] = a;
    v[3] = a;
}

// GCC what? 4.7 ??
#define __builtin_assume_aligned(x,a) x

// add two images
void pdp_imageproc_add_process(void *x, u32 width, u32 height,
                               s16 *image, s16 *image2) {
    __m64 *im  = __builtin_assume_aligned((void*)image, 16);
    __m64 *im2 = __builtin_assume_aligned((void*)image2, 16);
    int a, b;
    unsigned int v, v_max = width * height / 4;
    for (v = 0; v < v_max; v++) {
        im[v] = _mm_adds_pi16(im[v], im2[v]);
    }
    _mm_empty();
}

// mul two images
void pdp_imageproc_mul_process(void *x, u32 width, u32 height, s16 *image, s16 *image2)
{
    __m64 *im  = (void*)image;
    __m64 *im2 = (void*)image2;
    int a, b;
    unsigned int v, v_max = width * height / 4;
    for (v = 0; v < v_max; v++) {
        im[v] = _mm_mulhi_pi16(im[v], im2[v]);
    }
    _mm_empty();
}

// mix 2 images
#define MIX_SIZE 8*sizeof(s16)
void *pdp_imageproc_mix_new(void){return pdp_alloc(MIX_SIZE);}
int pdp_imageproc_mix_nb_stackwords(void){return PDP_IMAGEPROC_NB_STACKWORDS(MIX_SIZE);}
void pdp_imageproc_mix_delete(void *x) {pdp_dealloc (x);}
void pdp_imageproc_mix_setleftgain(void *x, float gain){setvec((s16 *)x, gain);}
void pdp_imageproc_mix_setrightgain(void *x, float gain){setvec((s16 *)x + 4, gain);}
void pdp_imageproc_mix_process(void *x, u32 width, u32 height, s16 *image, s16 *image2)
{
    __m64 *g = x;
    __m64 g_left  = g[0];
    __m64 g_right = g[1];

    /* Do not multiply; copy image to allow for perfect feedback.  Do
       not copy if pointers are the same (same packet). */
    if (0 == *((s16*)x)) {
        if (image != image2) {
            memcpy(image, image2, width * height * 2);
        }
        return;
    }

    __m64 *im_left  = (void*)image;
    __m64 *im_right = (void*)image2;
    int a, b;
    unsigned int v, v_max = width * height / 4;
    for (v = 0; v < v_max; v++) {
        __m64 left  = _mm_mulhi_pi16(g_left,  im_left[v]);
        __m64 right = _mm_mulhi_pi16(g_right, im_right[v]);
        __m64 sum   = _mm_adds_pi16(left, right);
        __m64 sum2  = _mm_adds_pi16(sum, sum);
        im_left[v] = sum2;
    }
    _mm_empty();
}


// random mix 2 images
void *pdp_imageproc_randmix_new(void){return pdp_alloc(2*sizeof(s32));;}
int  pdp_imageproc_randmix_nb_stackwords(void) {return PDP_IMAGEPROC_NB_STACKWORDS(2*sizeof(s32));}
void pdp_imageproc_randmix_delete(void *x) {pdp_dealloc(x);}
void pdp_imageproc_randmix_setthreshold(void *x, float threshold)
{
    s32 *d = (s32 *)x;
    if (threshold > 1.0f) threshold = 1.0f;
    if (threshold < 0.0f) threshold = 0.0f;
    d[0] = float2fixed(threshold);
}
void pdp_imageproc_randmix_setseed(void *x, float seed)
{
    s32 *d = (s32 *)x;
    d[1] = float2fixed(seed);
}
void pdp_imageproc_randmix_process(void *x, u32 width, u32 height, s16 *image, s16 *image2)
{
    s32 *d = (s32 *)x;
    u32 i;
    s16 r;
    srandom((u32)d[1]);

    for(i=0; i<width*height; i++){
	// get a random val between 0 and 0x7fff
	r = (s16)(random() & 0x7fff);
	if (r < d[0]) image[i] = image2[i];
    }
}


// 3x1 or 1x3 in place convolution
// orientation
void *pdp_imageproc_conv_new(void){return(pdp_alloc(6*sizeof(s32)));}
void pdp_imageproc_conv_delete(void *x){pdp_dealloc(x);}
void pdp_imageproc_conv_setmin1(void *x, float val)
{
    s32 *d = (s32 *)x;
    d[0] = float2fixed(val);
}
void pdp_imageproc_conv_setzero(void *x, float val)
{
    s32 *d = (s32 *)x;
    d[1] = float2fixed(val);
}
void pdp_imageproc_conv_setplus1(void *x, float val)
{
    s32 *d = (s32 *)x;
    d[2] = float2fixed(val);
}
void pdp_imageproc_conv_setbordercolor(void *x, float val)
{
    s32 *d = (s32 *)x;
    d[3] = float2fixed(val);
}
void pdp_imageproc_conv_setorientation(void *x, u32 val){((u32 *)x)[4] = val;}
void pdp_imageproc_conv_setnbpasses(void *x, u32 val){((u32 *)x)[5] = val;}

static inline void pdp_imageproc_conv_scanline(void *x, s16 *data, u32 count, s32 stride)
{
    s32 *d = (s32 *)x;
    s32 a,b,c,r;
    u32 i;

    a = d[3]; //border
    b = data[0];
    c = data[stride];

    for(i = 0; i < count-2; i++){
	r = a*d[0] + b*d[1] + c*d[2];
	a = data[0];
	b = data[stride];
	c = data[stride<<1];
	data[0] = (s16)CLAMP16(r>>15);
	data += stride;
    }
    r = a*d[0] + b*d[1] + c*d[2];
    a = data[0];
    b = data[stride];
    c = d[3]; //border
    data[0] = (s16)CLAMP16(r>>15);
    r = a*d[0] + b*d[1] + c*d[2];
    data[stride] = (s16)CLAMP16(r>>15);

}

void pdp_imageproc_conv_process(void *x, u32 width, u32 height, s16 *image)
{
    s32 *d = (s32 *)x;
    u32 i, j;
    u32 orientation = d[4];
    u32 nbp = d[5];
    if (orientation == PDP_IMAGEPROC_CONV_HORIZONTAL){
	for(i=0; i<width*height; i+=width)
	    for(j=0; j<nbp; j++)
		pdp_imageproc_conv_scanline(x, image+i, width, 1);

    }

    if (orientation == PDP_IMAGEPROC_CONV_VERTICAL){
	for(i=0; i<width; i++)
	    for(j=0; j<nbp; j++)
		pdp_imageproc_conv_scanline(x, image+i, height, width);

    }



	
}
void *pdp_imageproc_gain_new(void){return(pdp_alloc(8*sizeof(s16)));}
void pdp_imageproc_gain_delete(void *x){pdp_dealloc(x);}
void pdp_imageproc_gain_setgain(void *x, float gain)
{
    /* convert float to s16 + shift */
    s16 *d = (s16 *)x;
    s16 g;
    int i;
    float sign;
    int shift = 0;

    sign = (gain < 0) ? -1 : 1;
    gain *= sign;

    /* max shift = 16 */
    for(i=0; i<=16; i++) {
        if (gain < 0x4000) {
            gain *= 2;
            shift++;
        }
        else break;
    }

    gain *= sign;
    g = (s16) gain;

    //g = 0x4000;
    //shift = 14;

    // fprintf(stderr, "0x%04x %2d (%f)\n", g, shift, gain);

    d[0]=g;
    d[1]=g;
    d[2]=g;
    d[3]=g;
    d[4]=(s16)shift;
    d[5]=0;
    d[6]=0;
    d[7]=0;
}

/* NOTE: In porting from pixel_gain_s16.s this produced a different
   result. See the *256 correction factor above. */

void pdp_imageproc_gain_process(void *x, u32 width, u32 height, s16 *image)
{
    __m64 *g = x;
    __m64 gain = g[0];
    __m64 shift = g[1];
    __m64 *im = (void*)image;
    unsigned int v, v_max = width * height / 4;
    for (v = 0; v < v_max; v++) {
        __m64 pix = im[v];
        /* Integer multiply */
        __m64 lo4 = _mm_mullo_pi16(gain, pix);
        __m64 hi4 = _mm_mulhi_pi16(gain, pix);
        /* Shuffle product into 2 registers containing 2x32 words. */
        __m64 w0 = _mm_unpacklo_pi16(lo4, hi4);
        __m64 w1 = _mm_unpackhi_pi16(lo4, hi4);
        /* Shift right */
        __m64 w0s = _mm_sra_pi32(w0, shift);
        __m64 w1s = _mm_sra_pi32(w1, shift);
        /* Saturated pack */
        __m64 w = _mm_packs_pi32(w0s, w1s);
        im[v] = w;
    }
    _mm_empty();
}


// colour rotation for 2 colour planes
void *pdp_imageproc_crot2d_new(void){return pdp_alloc(4*sizeof(s32));}
void pdp_imageproc_crot2d_delete(void *x){pdp_dealloc(x);}
void pdp_imageproc_crot2d_setmatrix(void *x, float *matrix)
{
    s32 *d = (s32 *)x;
    d[0] = float2fixed(matrix[0]);
    d[1] = float2fixed(matrix[1]);
    d[2] = float2fixed(matrix[2]);
    d[3] = float2fixed(matrix[3]);

}
void pdp_imageproc_crot2d_process(void *x, s16 *image, u32 width, u32 height)
{
    s32 *d = (s32 *)x;
    u32 i,j;
    s32 a1,a2,c1,c2;

    for(i=0, j=width*height; i<width*height; i++, j++){
	c1 = (s32)image[i];
	c2 = (s32)image[j];
	
	a1 = d[0] * c1;
	a2 = d[1] * c1;
	a1+= d[2] * c2;
	a2+= d[3] * c2;

	a1 >>= 15;
	a2 >>= 15;

	image[i] = (s16)CLAMP16(a1);
	image[j] = (s16)CLAMP16(a2);
    }
}

// biquad and biquad time
typedef struct
{
    s16 ma1[4];
    s16 ma2[4];
    s16 b0[4];
    s16 b1[4];
    s16 b2[4];
    s16 u0[4];
    s16 u1[4];
    s16 u0_save[4];
    s16 u1_save[4];
    u32 nbpasses;
    u32 direction;
} t_bq;
void *pdp_imageproc_bq_new(void){return pdp_alloc(sizeof(t_bq));}
void pdp_imageproc_bq_delete(void *x){pdp_dealloc(x);}
void pdp_imageproc_bq_setcoef(void *x, float *coef) // a0,-a1,-a2,b0,b1,b2,u0,u1
{
    t_bq *d = (t_bq *)x;
    float ia0 = 1.0f / coef[0];

    /* all coefs are s1.14 fixed point */
    /* representing values -2 < x < 2  */
    /* so scale down before using the ordinary s0.15 float->fixed routine */

    ia0 *= 0.5f;

    // coef
    setvec(d->ma1, ia0*coef[1]);
    setvec(d->ma2, ia0*coef[2]);
    setvec(d->b0, ia0*coef[3]);
    setvec(d->b1, ia0*coef[4]);
    setvec(d->b2, ia0*coef[5]);

    // state to reset too
    setvec(d->u0_save, coef[6]);
    setvec(d->u1_save, coef[7]);

}
void pdp_imageproc_bq_setnbpasses(void *x, u32 nbpasses){((t_bq *)x)->nbpasses = nbpasses;}
void pdp_imageproc_bq_setdirection(void *x, u32 direction){((t_bq *)x)->direction = direction;}
void pdp_imageproc_bq_process(void *x, u32 width, u32 height, s16* image);

/*   DIRECT FORM II BIQUAD

	 y[k]  = b0 * x[k] + u1[k-1]
	 u1[k] = b1 * x[k] + u2[k-1] - a1 * y[k]
	 u2[k] = b2 * x[k]           - a2 * y[k]
	 MACRO:	df2 <reg>

	 computes a direct form 2 biquad
	 does not use {mm0-mm3}\<inreg>

	 input:	<reg>   == input
			%mm4    == state 1
			%mm5    == state 2
			(%esi)  == biquad coefs (-a1 -a2 b0 b1 b2) in s1.14
	 output:	<reg>   == output
			%mm4    == state 1
			%mm5    == state 2
*/

struct __attribute__((__packed__)) biquad_coef {
    __m64 ma1; // -a1
    __m64 ma2; // -a2
    __m64 b0;
    __m64 b1;
    __m64 b2;
};
static INLINE void
biquad_df2(__m64 *ry, /* output */
           __m64 x,   /* input */
           __m64 *ru1, /* biquad state */
           __m64 *ru2,
           struct biquad_coef *c)
{
    __m64 xb0 = _mm_mulhi_pi16(c->b0, x);
    __m64 xb1 = _mm_mulhi_pi16(c->b1, x);
    __m64 u1 = *ru1;
    __m64 u2 = *ru2;

    __m64 b0x_plus_u1 = _mm_add_pi16(xb0, u1); // (1)
    __m64 b1x_plus_u2 = _mm_add_pi16(xb1, u2); // (1)
    /* 2x saturated add to compensate for mul = x*y/4;
       coefs are s1.14 fixed point */
    __m64 y_div_2 = _mm_adds_pi16(b0x_plus_u1, b0x_plus_u1);
    __m64 y = _mm_adds_pi16(y_div_2, y_div_2);
    __m64 ma1y = _mm_mulhi_pi16(c->ma1, y);
    __m64 ma2y = _mm_mulhi_pi16(c->ma2, y);
    __m64 b2x = _mm_mulhi_pi16(c->b2, x);
    __m64 next_u1 = _mm_add_pi16(b1x_plus_u2, ma1y); // (1)
    __m64 next_u2 = _mm_add_pi16(b2x, ma2y); // (1)
    *ru1 = next_u1;
    *ru2 = next_u2;
    *ry = y;

    /* (1) This uses non-saturated add for the internal filter nodes.
       Leaving it in, but this makes sense only in direct form I, so
       is probably a bug from old MMX code switch from DFI->DFII */
}

/* From the old MMX code (edited for clarity).

   In order to use the 4 line parallel biquad routine on horizontal
   lines, we need to reorder (rotate or transpose) the matrix, since
   images are scanline encoded, and we want to work in parallell on 4
   lines.

   Since the 4 lines are independent, it doesnt matter in which order
   the the vector elements are present.  This allows to pick either
   rotation or transpose to go from horizontal->vertical.

   Rotations, transposes and flips are part of the 8-element
   non-abelean group of square isometries consisting of

	 (I) identity
	 (H) horizontal axis mirror
	 (V) vertical axis mirror
	 (T) transpose (diagonal axis mirror)
	 (A) antitranspose (antidiagonal axis mirror)
	 (R1) 90deg anticlockwize rotation
	 (R2) 180deg rotation
	 (R3) 90deg clockwize rotation

   We basicly have two options: (R1,R3) or (T,A)

   The choice seems arbitrary.. I opt for flips because they are
   self-inverting.

   Only one of T,A needs to be implemented with MMX unpack
   instructions.  The other one can be derived by flips and both input
   and output.

   A = V T V = H T H

*/

/* The routines below use the low=0 to high=3 naming scheme that is
   used in MMX, i.e. the low word corresponds to the lowest 16 bits in
   the 64 bit register.  Note however that wrt to memory, MMX is
   little endian, so what looks like a transpose for the above naming
   scheme, looks like an antitranspose for pixels in memory!

   Since this is so confusing, to actually implement this it's best to
   realize that the transform needs to be performed twice, and the
   L->R and R->L need the opposite transforms.  Then if it doesn't
   look right, flip the signs.
*/

static INLINE void
transpose(__m64 *v0, __m64 *v1, __m64 *v2, __m64 *v3) {

    __m64 a0_a1_a2_a3 = *v0;
    __m64 b0_b1_b2_b3 = *v1;
    __m64 c0_c1_c2_c3 = *v2;
    __m64 d0_d1_d2_d3 = *v3;

    __m64 a0_c0_a1_c1 = _mm_unpacklo_pi16(a0_a1_a2_a3, c0_c1_c2_c3);
    __m64 a2_c2_a3_c3 = _mm_unpackhi_pi16(a0_a1_a2_a3, c0_c1_c2_c3);
    __m64 b0_d0_b1_d1 = _mm_unpacklo_pi16(b0_b1_b2_b3, d0_d1_d2_d3);
    __m64 b2_d2_b3_d3 = _mm_unpackhi_pi16(b0_b1_b2_b3, d0_d1_d2_d3);

    __m64 a0_b0_c0_d0 = _mm_unpacklo_pi16(a0_c0_a1_c1, b0_d0_b1_d1);
    __m64 a1_b1_c1_d1 = _mm_unpackhi_pi16(a0_c0_a1_c1, b0_d0_b1_d1);
    __m64 a2_b2_c2_d2 = _mm_unpacklo_pi16(a2_c2_a3_c3, b2_d2_b3_d3);
    __m64 a3_b3_c3_d3 = _mm_unpackhi_pi16(a2_c2_a3_c3, b2_d2_b3_d3);

    *v0 = a0_b0_c0_d0;
    *v1 = a1_b1_c1_d1;
    *v2 = a2_b2_c2_d2;
    *v3 = a3_b3_c3_d3;
}

static INLINE void
antitranspose(__m64 *v0, __m64 *v1, __m64 *v2, __m64 *v3) {
    /* Vertical flip input and output. */
    transpose(v3,v2,v1,v0);
}
static INLINE void
biquad_4x4(__m64 *v0, __m64 *v1, __m64 *v2, __m64 *v3,
           __m64 *ru1, /* biquad state */
           __m64 *ru2,
           struct biquad_coef *c) {
    biquad_df2(v0, *v0, ru1, ru2, c);
    biquad_df2(v1, *v1, ru1, ru2, c);
    biquad_df2(v2, *v2, ru1, ru2, c);
    biquad_df2(v3, *v3, ru1, ru2, c);
}
static INLINE void
biquad_4x4_t(__m64 *v0, __m64 *v1, __m64 *v2, __m64 *v3,
             __m64 *ru1, /* biquad state */
             __m64 *ru2,
             struct biquad_coef *c) {
    transpose(v0, v1, v2, v3);
    biquad_4x4(v0, v1, v2, v3, ru1, ru2, c);
    transpose(v0, v1, v2, v3);
}
static INLINE void
biquad_4x4_a(__m64 *v0, __m64 *v1, __m64 *v2, __m64 *v3,
             __m64 *ru1, /* biquad state */
             __m64 *ru2,
             struct biquad_coef *c) {
    antitranspose(v0, v1, v2, v3);
    biquad_4x4(v0, v1, v2, v3, ru1, ru2, c);
    antitranspose(v0, v1, v2, v3);
}


static void pixel_biquad_time_s16(s16 *image,  // imput plane
                                  s16 *state0, // state planes
                                  s16 *state1,
                                  s16 *coef,
                                  int v_max) {
    __m64 *im = (void*)image;
    __m64 *pu1 = (void*)state0;
    __m64 *pu2 = (void*)state1;
    struct biquad_coef *c = (void*)coef;
    int v;
    for (v = 0; v < v_max; v++) {
        __m64 x = im[v];
        __m64 u1  = pu1[v];
        __m64 u2  = pu2[v];
        __m64 y;
        biquad_df2(&y, x, &u1, &u2, c);
        im[v] = y;
        pu1[v] = u1;
        pu2[v] = u2;
    }
    _mm_empty();

}

/* This code performs 4x4 strips in 4 directions: T->B, B->T, L->R,  R->L.

   The loop iteration can probably be simplified a bit; this has a lot
   of duplication. */
static void	pixel_biquad_vertb_s16(s16 *image,
                                   int nb_4x4_blocks /* height>>2 */,
                                   int stride,
                                   s16 *coefs,
                                   s16 *state) {
    stride /= 4; // vector stride
    __m64 *im = (void*)image;
    __m64 *s = (void*)state;
    __m64 u1 = s[0];
    __m64 u2 = s[1];
    struct biquad_coef *c = (void*)coefs;
    int block;
    int stride2 = 2 * stride;
    int stride3 = 3 * stride;
    int stride4 = 4 * stride;
    for (block = 0; block < nb_4x4_blocks; block++) {
        __m64 v0 = im[0];
        __m64 v1 = im[stride];
        __m64 v2 = im[stride2];
        __m64 v3 = im[stride3];
        biquad_4x4(&v0, &v1, &v2, &v3, &u1, &u2, c);
        im[0]       = v0;
        im[stride]  = v1;
        im[stride2] = v2;
        im[stride3] = v3;
        im += stride4;
    }
    s[0] = u1;
    s[1] = u2;
    _mm_empty();
}
static void	pixel_biquad_verbt_s16(s16 *image,
                                   int nb_4x4_blocks /* height>>2 */,
                                   int stride,
                                   s16 *coefs,
                                   s16 *state) {
    stride /= 4; // vector stride
    __m64 *im = (void*)image;
    __m64 *s = (void*)state;
    __m64 u1 = s[0];
    __m64 u2 = s[1];
    struct biquad_coef *c = (void*)coefs;
    int block;
    int stride2 = 2 * stride;
    int stride3 = 3 * stride;
    int stride4 = 4 * stride;
    im += (nb_4x4_blocks - 1) * stride4;
    for (block = 0; block < nb_4x4_blocks; block++) {
        __m64 v3 = im[0];
        __m64 v2 = im[stride];
        __m64 v1 = im[stride2];
        __m64 v0 = im[stride3];
        biquad_4x4(&v0, &v1, &v2, &v3, &u1, &u2, c);
        im[0]       = v3;
        im[stride]  = v2;
        im[stride2] = v1;
        im[stride3] = v0;
        im -= stride4;
    }
    s[0] = u1;
    s[1] = u2;
    _mm_empty();
}
static void	pixel_biquad_horlr_s16(s16 *image,
                                   int nb_4x4_blocks /* width>>2 */,
                                   int stride,
                                   s16 *coefs,
                                   s16 *state) {
    stride /= 4; // vector stride
    __m64 *im = (void*)image;
    __m64 *s = (void*)state;
    __m64 u1 = s[0];
    __m64 u2 = s[1];
    struct biquad_coef *c = (void*)coefs;
    int block;
    int stride2 = 2 * stride;
    int stride3 = 3 * stride;
    int stride4 = 4 * stride;
    for (block = 0; block < nb_4x4_blocks; block++) {
        __m64 v0 = im[0];
        __m64 v1 = im[stride];
        __m64 v2 = im[stride2];
        __m64 v3 = im[stride3];
        biquad_4x4_t(&v0, &v1, &v2, &v3, &u1, &u2, c);
        im[0]       = v0;
        im[stride]  = v1;
        im[stride2] = v2;
        im[stride3] = v3;
        im ++;
    }
    s[0] = u1;
    s[1] = u2;
    _mm_empty();
}
static void	pixel_biquad_horrl_s16(s16 *image,
                                   int nb_4x4_blocks /* width>>2 */,
                                   int stride,
                                   s16 *coefs,
                                   s16 *state) {
    stride /= 4; // vector stride
    __m64 *im = (void*)image;
    __m64 *s = (void*)state;
    __m64 u1 = s[0];
    __m64 u2 = s[1];
    struct biquad_coef *c = (void*)coefs;
    int block;
    int stride2 = 2 * stride;
    int stride3 = 3 * stride;
    int stride4 = 4 * stride;
    im += nb_4x4_blocks - 1;
    for (block = 0; block < nb_4x4_blocks; block++) {
        __m64 v3 = im[0];
        __m64 v2 = im[stride];
        __m64 v1 = im[stride2];
        __m64 v0 = im[stride3];
        biquad_4x4_a(&v0, &v1, &v2, &v3, &u1, &u2, c);
        im[0]       = v3;
        im[stride]  = v2;
        im[stride2] = v1;
        im[stride3] = v0;
        im--;
    }
    s[0] = u1;
    s[1] = u2;
    _mm_empty();
}
void pdp_imageproc_bqt_process(void *x, u32 width, u32 height, s16 *image, s16 *state0, s16 *state1)
{
    s16 *d = (s16 *)x;
    pixel_biquad_time_s16(image, state0, state1, d, (width*height)>>2);
}

void pdp_imageproc_bq_process(void *x, u32 width, u32 height, s16 *image)
{
    t_bq *d = (t_bq *)x;
    s16 *c = d->ma1; /* coefs */
    s16 *s = d->u0;  /* state */
    u32 direction = d->direction;
    u32 nbp = d->nbpasses;
    unsigned int i,j;



    /* VERTICAL */

    if ((direction & PDP_IMAGEPROC_BIQUAD_TOP2BOTTOM)
	&& (direction &  PDP_IMAGEPROC_BIQUAD_BOTTOM2TOP)){

	for(i=0; i<width; i +=4){
	    for (j=0; j<nbp; j++){
		pixel_biquad_vertb_s16(image+i,    height>>2, width, c, s);
		pixel_biquad_verbt_s16(image+i,    height>>2, width, c, s);
	    }
	}
    }

    else if (direction & PDP_IMAGEPROC_BIQUAD_TOP2BOTTOM){
	for(i=0; i<width; i +=4){
	    for (j=0; j<nbp; j++){
		pixel_biquad_vertb_s16(image+i,    height>>2, width, c, s);
	    }
	}
    }

    else if (direction & PDP_IMAGEPROC_BIQUAD_BOTTOM2TOP){
	for(i=0; i<width; i +=4){
	    for (j=0; j<nbp; j++){
		pixel_biquad_verbt_s16(image+i,    height>>2, width, c, s);
	    }
	}
    }

    /* HORIZONTAL */

    if ((direction & PDP_IMAGEPROC_BIQUAD_LEFT2RIGHT)
	&& (direction & PDP_IMAGEPROC_BIQUAD_RIGHT2LEFT)){

	for(i=0; i<(width*height); i +=(width<<2)){
	    for (j=0; j<nbp; j++){
		pixel_biquad_horlr_s16(image+i,    width>>2, width, c, s);
		pixel_biquad_horrl_s16(image+i,    width>>2, width, c, s);
	    }
	}
    }

    else if (direction & PDP_IMAGEPROC_BIQUAD_LEFT2RIGHT){
	for(i=0; i<(width*height); i +=(width<<2)){
	    for (j=0; j<nbp; j++){
		pixel_biquad_horlr_s16(image+i,    width>>2, width, c, s);
	    }
	}
    }

    else if (direction & PDP_IMAGEPROC_BIQUAD_RIGHT2LEFT){
	for(i=0; i<(width*height); i +=(width<<2)){
	    for (j=0; j<nbp; j++){
		pixel_biquad_horrl_s16(image+i,    width>>2, width, c, s);
	    }
	}
    }

}

// produce a random image
// note: random number generator can be platform specific
// however, it should be seeded. (same seed produces the same result)
void *pdp_imageproc_random_new(void){return pdp_alloc(4*sizeof(s16));}
void pdp_imageproc_random_delete(void *x){pdp_dealloc(x);}
void pdp_imageproc_random_setseed(void *x, float seed)
{
    s16 *d = (s16 *)x;
    srandom((u32)seed);
    d[0] = (s16)random();
    d[1] = (s16)random();
    d[2] = (s16)random();
    d[3] = (s16)random();
    
}
void pdp_imageproc_random_process(void *x, u32 width, u32 height, s16 *image)
{
    v4_t one = {.s = {1,1,1,1}};
    __m64 lsb_mask = one.v;
    __m64 *im = (void*)image;
    __m64 *seed = (void*)x;
    __m64 state = *seed;
    unsigned int v, nb_vec = (width * height)>>2;
    for (v=0; v<nb_vec; v++) {
        /* Fibonacci LFSR for the polynomial
           p(x) = x^16 + x^15 + x^13 + x^4 + 1

           I did not know this at the time (2003?), but a Galois LFSR
           seems more appropriate as it uses a single conditional
           XOR.

           http://en.wikipedia.org/wiki/Linear_feedback_shift_register */
        __m64 bit =             _mm_srli_pi16(state, 15);
        bit = _mm_xor_si64(bit, _mm_srli_pi16(state, 14));
        bit = _mm_xor_si64(bit, _mm_srli_pi16(state, 12));
        bit = _mm_xor_si64(bit, _mm_srli_pi16(state, 3));
        /* Isolate LSB and shift it into the state. */
        bit = _mm_and_si64(bit, lsb_mask);
        state = _mm_or_si64(_mm_slli_pi16(state, 1), bit);

        im[v] = state;
    }
    /* keep random state to seed next iteration */
    *seed = state;

    _mm_empty();
}


/* Image resampling

   This uses a similar approach to the other MMX routines:
   1. Preparation of "cooked" parameters to be used in the vector code
   2. Vector code

   The major point is this: the resampler MMX code is shared for all
   resampling code.  It uses data specified in t_resample_cbrd (Cooked
   Bilinear Resampler Data)

   This is designed for multiple feeder algorithms, but only one is
   implemented: Linear remapper with data in t_resample_clmd, that
   supports zoom, rotate, ...

   Resampling works as follows:

   1. A feeder algorithm produces a stream of fractional coordinates:
      every pixel in the output frame is thus related to a coordinate.
      This consists of two nested loops: one row incrementor and one
      column incrementor.

   2. From the integer part of the coordinates a 2x2 pixel environment
      is fetched from the source image and stored in the TL,TR,BL,BR
      temp space.

   3. A bilinear resampler will use the fractional part of the
      coordinates to perform the interpolation of the 2x2 pixel
      environment.
*/


typedef struct {
    u32 lineoffset;
    s16 *image;
    u32 width;
    u32 height;
} t_resample_id; // Image Data


/* MMX resampling source image resampling data + coefs */
typedef struct {

    /* [ 0 | 0 | 2*(height-1) | 2(width-1) ]
       Used to convert image-relative s.15 coordinate o
       pixel coordinates [ pixel_index (16) | pixel_frac (16) ]
       The factor 2 is to compensate for the s.15 format */
    v4_t coord_scale;

    /* PMUL coefficient to convert [ 0 | 0 | y | x ] 16 vector
       coordinates to vector offsets [ y_offset | x_offset ] 32 
       (x == x_offset) */
    v4_t row_stride_scale;

    /* Image boundaries for PCMP:
       [ y_offset_max | x_offset_max ]32 */
    v2_t offset_max;

    /* Vector increments for x and y */
    v2_t inc_x; // [ 0 | 1 ]32;
    v2_t inc_y; // [ v_stride | 0 ]32

    s16 *image;

} t_resample_cid; // Cooked Image Data

/* Linear mapping data struct (zoom, scale, rotate, shear, ...)

   Each output pixel (H,V) is related to an input pixel with
   coordinates:

   origin + H * hor + V * ver

   Coordinates and increments are in 0.32 fractional format relative
   to the image width/height.  We're processing 2 coordinate sets in
   parallel. */
typedef struct {
    v2_t origin_x;  // initial x coord
    v2_t origin_y;  // initial y coord
    v2_t hor_dx;    // x row inc
    v2_t hor_dy;    // y row inc
    v2_t ver_dx;    // x column inc
    v2_t ver_dy;    // y column inc
} t_resample_clmd;  // Cooked Linear Mapping Data

/* this struct contains all the data necessary for
   bilin interpolation from src -> dst image
   (src can be == dst) */
typedef struct {
    t_resample_cid csrc;     //cooked src image meta data for bilinear interpolator
    t_resample_id src;       //src image meta
    t_resample_id dst;       //dst image meta
} t_resample_cbrd;            //Bilinear Resampler Data

/* this struct contains high level zoom parameters,
   all image relative */
typedef struct {
    float centerx;
    float centery;
    float zoomx;
    float zoomy;
    float angle;
} t_resample_zrd;

/* This struct contains all data for the zoom object.  Note that this
   is a flat struct to allow a single pointer register to be used for
   getting __m64 data. */
typedef struct {
    t_resample_cbrd cbrd;      // Bilinear Resampler Data
    t_resample_clmd clmd;      // Cooked Linear Mapping data
    t_resample_zrd   zrd;      // Zoom / Rotate Data
} t_resample_zoom_rotate;



/* initialize image meta data (dimensions + location) */
static void pdp_imageproc_resample_init_id(t_resample_id *x, u32 offset, s16* image, u32 w, u32 h)
{
    x->lineoffset = offset;
    x->image = image;
    x->width = w;
    x->height = h;
}


static INLINE void
bilin_pixel(t_resample_zoom_rotate *p, word_t *out, __m64 xy_32) {
    /* Input is in 0.32 image-relative coordinates:
       xy_32 == [ y(32) : x(32) ] */

    /* This routine uses MMX for as long as it is practical, and
       switches to scalar integer math once data shuffling would get
       too complicated (Yes, I tried, it's horrible..) */

    /* MMX only does signed 16 x 16 -> 16hi | 16lo so we convert here.
       This means we use a bit of precision which is practice is quite
       acceptable.  I.e. even for 1920x1080, the x sub-pixel
       resolution is still 32 levels. */
    __m64 xy_32_s = _mm_srli_pi32(xy_32, 16 + 1); // reduce precision + convert to signed
    __m64 xy_16 = _mm_packs_pi32(xy_32_s, xy_32_s); // 2nd arg is dummy: upper 32 bits ignored below

    /* Split coordinate into integer and factional part.
       Integer: used for indexing
       Fractional: used for interpolation.
        xy_16_index == [ 0 | 0 | y_index | x_index ]
        xy_16_frac  == [ 0 | 0 | y_frac  | x_frac ] */
    __m64 xy_16_index = _mm_mulhi_pi16(xy_16, p->cbrd.csrc.coord_scale.v);
    v4_t xy_16_frac = {.v = _mm_mullo_pi16(xy_16, p->cbrd.csrc.coord_scale.v)};
    word_t frac_x = xy_16_frac.us[0];
    word_t frac_y = xy_16_frac.us[1];

    /* The x and y integer pixel coordinates in xy_16_index need to be
       converted into pixel offsets for image array indexing.  Uses
       32bit result because 16bit is not enough. */
    __m64 offset_16_lo = _mm_mullo_pi16(xy_16_index, p->cbrd.csrc.row_stride_scale.v);
    __m64 offset_16_hi = _mm_mulhi_pi16(xy_16_index, p->cbrd.csrc.row_stride_scale.v);
    /* offset_x0_y0 == [ offset_y | x-offset ] */
    __m64 offset_x0_y0 = _mm_unpacklo_pi16(offset_16_lo, offset_16_hi);

    /* Next is to find the offsets of the neighbouring pixels in both
       x and y directions.  First just add the appropriate offsets
       from config.  This leads to offsets that might be past image
       borders. */
    __m64 offset_x1_y0 = _mm_add_pi32(offset_x0_y0, p->cbrd.csrc.inc_x.v);
    __m64 offset_x0_y1 = _mm_add_pi32(offset_x0_y0, p->cbrd.csrc.inc_y.v);
    __m64 offset_x1_y1 = _mm_add_pi32(offset_x1_y0, p->cbrd.csrc.inc_y.v);

    /* Before fetch, set coords to 0 if they are out-of-bounds,
       effecitively implementing wrap-around addressing. */
    s16 *im = p->cbrd.csrc.image;
    INLINE __m64 wrap_0(__m64 val, __m64 max) {
        /* Set components to zero when >= max */
        __m64 mask = _mm_cmpgt_pi32(max, val); // 0xFFFFFFFF when true
        __m64 val_out = _mm_and_si64(val, mask);
        return val_out;
    }
    INLINE word_t fetch_pixel(__m64 xy_offset, bool do_wrap) {
        __m64 wrapped = do_wrap ?
            wrap_0(xy_offset, p->cbrd.csrc.offset_max.v) :
            xy_offset;
        v2_t flat = {.v = _mm_add_pi32(wrapped, _mm_srli_si64(wrapped, 32))};
        word_t pixel = im[flat.l[0]];
        return pixel;
    }

    /* Fetch 2x2 pixel grid to interpolate. */
    word_t w_x0_y0 = fetch_pixel(offset_x0_y0, false);
    word_t w_x1_y0 = fetch_pixel(offset_x1_y0, true);
    word_t w_x0_y1 = fetch_pixel(offset_x0_y1, true);
    word_t w_x1_y1 = fetch_pixel(offset_x1_y1, true);

    /* Perform bilinear interpolation using fractional coordinates. */
    word_t w_y0 = w_x0_y0 + (((w_x1_y0 - w_x0_y0) * frac_x) >> 16);
    word_t w_y1 = w_x0_y1 + (((w_x1_y1 - w_x0_y1) * frac_x) >> 16);
    word_t w = w_y0 + (((w_y1 - w_y0) * frac_y) >> 16);

    *out = w;
}

void pixel_resample_linmap_s16(t_resample_zoom_rotate *z) {

    int hor,ver;
    int vector = 0;
    int hor_max = z->cbrd.dst.width / 4; // vector count
    int ver_max = z->cbrd.dst.height;    // row count

    /* Vertical increment loop. */
    __m64 ver_co_x = z->clmd.origin_x.v;
    __m64 ver_co_y = z->clmd.origin_y.v;
    int v = 0;
    for (ver = 0; ver < ver_max; ver++) {

        /* Horizontal increment loop */
        __m64 co_x = ver_co_x;
        __m64 co_y = ver_co_y;
        for (hor = 0; hor < hor_max; hor++) {

            /* Get coords of 2nd pixel pair. */
            __m64 co1_x = _mm_add_pi32(co_x, z->clmd.hor_dx.v);
            __m64 co1_y = _mm_add_pi32(co_y, z->clmd.hor_dy.v);

            /* Now we have 4 sets of input coordinates for the 4
               pixels in the output data.  The format is 0.32
               representing a number between 0 and 1.  (0 == top/left,
               1 = right/bottom)

               The interpolator however does only one pixel at a time,
               so these need to be packed to x/y format. */

            __m64 xy_0 = _mm_unpacklo_pi32(co_x, co_y);
            __m64 xy_1 = _mm_unpackhi_pi32(co_x, co_y);
            __m64 xy_2 = _mm_unpacklo_pi32(co1_x, co1_y);
            __m64 xy_3 = _mm_unpackhi_pi32(co1_x, co1_y);

            /* Resample */
            v4_t p0; bilin_pixel(z, &p0.w, xy_0);
            v4_t p1; bilin_pixel(z, &p1.w, xy_1);
            v4_t p2; bilin_pixel(z, &p2.w, xy_2);
            v4_t p3; bilin_pixel(z, &p3.w, xy_3);

            /* Pack & store */
            __m64 p02 = _mm_unpacklo_pi16(p0.v, p2.v);
            __m64 p13 = _mm_unpacklo_pi16(p1.v, p3.v);
            __m64 p0123 = _mm_unpacklo_pi16(p02, p13);
            __m64 *im = (void*)z->cbrd.dst.image;
            im[v++] = p0123;

            /* Next coord pair. */
            co_x = _mm_add_pi32(co1_x, z->clmd.hor_dx.v);
            co_y = _mm_add_pi32(co1_y, z->clmd.hor_dy.v);
        }

        /* Next coord */
        ver_co_x =  _mm_add_pi32(ver_co_x, z->clmd.ver_dx.v);
        ver_co_y =  _mm_add_pi32(ver_co_y, z->clmd.ver_dy.v);
    }
    _mm_empty();
}



/* convert image meta data into a cooked format used by the resampler routine */
static void pdp_imageproc_resample_init_cid(t_resample_cid *r, t_resample_id *i) {


    /* Factor 2 compensates for sign bit shift in s.15 fixed point. */
    v4_t coord_scale = {.s = {
            2 * (i->width-1),
            2 * (i->height-1),
            0, 0 }};

    /* No scaling factor here since this is proper integer math. */
    v4_t row_stride_scale = {.s = {
            1,           // x coord to offset
            i->width,    // y coord to offset
            0, 0}};

    /* 32bit maximum offsets. */
    v2_t offset_max = {.l = {
            i->width,
            i->width * i->height }};

    /* 32 bit +1 offsets in x and y directions. */
    v2_t inc_x = {.l = {
            1,
            0}};
    v2_t inc_y = {.l = {
            0,
            i->width }};

    r->image = i->image;
    r->coord_scale = coord_scale;
    r->row_stride_scale = row_stride_scale;
    r->offset_max = offset_max;
    r->inc_x = inc_x;
    r->inc_y = inc_y;

}


/* convert incremental linear remapping vectors to internal cooked
   format.  Coordinates and increments are 16.16 fixed point.  We'll
   process 2 pixel coords in parallel. */

static void pdp_imageproc_resample_cookedlinmap_init(t_resample_clmd *l,
                                                     s32 origin_x, s32 origin_y,
                                                     s32 hor_dx, s32 hor_dy,
                                                     s32 ver_dx, s32 ver_dy) {
    typeof(*l) config = {
        /* 2 Coordinate sets are computed in parallel. */
        .origin_x = {.l = {origin_x, origin_x + hor_dx}},
        .origin_y = {.l = {origin_y, origin_y + hor_dy}},
        .hor_dx = {.l = {hor_dx * 2, hor_dx * 2}},
        .hor_dy = {.l = {hor_dy * 2, hor_dy * 2}},
        .ver_dx = {.l = {ver_dx, ver_dx}},
        .ver_dy = {.l = {ver_dy, ver_dy}},
    };
    *l = config;
}




/* convert floating point center and zoom data to incremental linear remapping vectors */
static void pdp_imageproc_resample_clmd_init_from_id_zrd(t_resample_clmd *l, t_resample_id *i, t_resample_zrd *z)
{
    double izx = 1.0f / (z->zoomx);
    double izy = 1.0f / (z->zoomy);
    double scale = (double)0xffffffff;
    double scalew = scale / ((double)(i->width - 1));
    double scaleh = scale / ((double)(i->height - 1));
    double cx = ((double)z->centerx) * ((double)(i->width - 1));
    double cy = ((double)z->centery) * ((double)(i->height - 1));
    double angle = z->angle * (-M_PI / 180.0);
    double c = cos(angle);
    double s = sin(angle);

    /* affine x, y mappings in screen coordinates */
    double mapx(double x, double y){return cx + izx * ( c * (x-cx) + s * (y-cy));}
    double mapy(double x, double y){return cy + izy * (-s * (x-cx) + c * (y-cy));}

    u32 tl_x = (u32)(scalew * mapx(0,0));
    u32 tl_y = (u32)(scaleh * mapy(0,0));


    u32 row_inc_x = (u32)(scalew * (mapx(1,0)-mapx(0,0)));
    u32 row_inc_y = (u32)(scaleh * (mapy(1,0)-mapy(0,0)));
    u32 col_inc_x = (u32)(scalew * (mapx(0,1)-mapx(0,0)));
    u32 col_inc_y = (u32)(scaleh * (mapy(0,1)-mapy(0,0)));


    pdp_imageproc_resample_cookedlinmap_init(l, tl_x, tl_y, row_inc_x, row_inc_y, col_inc_x, col_inc_y);
}


// zoom + rotate
void *pdp_imageproc_resample_affinemap_new(void)
{
    t_resample_zoom_rotate *z = (t_resample_zoom_rotate *)pdp_alloc(sizeof(t_resample_zoom_rotate));
    z->zrd.centerx = 0.5;
    z->zrd.centery = 0.5;
    z->zrd.zoomx = 1.0;
    z->zrd.zoomy = 1.0;
    z->zrd.angle = 0.0f;
    return (void *)z;
}
void pdp_imageproc_resample_affinemap_delete(void *x){pdp_dealloc(x);}
void pdp_imageproc_resample_affinemap_setcenterx(void *x, float f){((t_resample_zoom_rotate *)x)->zrd.centerx = f;}
void pdp_imageproc_resample_affinemap_setcentery(void *x, float f){((t_resample_zoom_rotate *)x)->zrd.centery = f;}
void pdp_imageproc_resample_affinemap_setzoomx(void *x, float f){((t_resample_zoom_rotate *)x)->zrd.zoomx = f;}
void pdp_imageproc_resample_affinemap_setzoomy(void *x, float f){((t_resample_zoom_rotate *)x)->zrd.zoomy = f;}
void pdp_imageproc_resample_affinemap_setangle(void *x, float f){((t_resample_zoom_rotate *)x)->zrd.angle = f;}
void pdp_imageproc_resample_affinemap_process(void *x, u32 width, u32 height, s16 *srcimage, s16 *dstimage)
{
    t_resample_zoom_rotate *z = (t_resample_zoom_rotate *)x;

    /* setup resampler image meta data */
    pdp_imageproc_resample_init_id(&(z->cbrd.src), width, srcimage, width, height);
    pdp_imageproc_resample_init_id(&(z->cbrd.dst), width, dstimage, width, height);
    pdp_imageproc_resample_init_cid(&(z->cbrd.csrc),&(z->cbrd.src)); 

    /* setup linmap data from zoom_rotate parameters */
    pdp_imageproc_resample_clmd_init_from_id_zrd(&(z->clmd), &(z->cbrd.src), &(z->zrd));


    /* call assembler routine */
    pixel_resample_linmap_s16(z);
}






// polynomials


typedef struct
{
    u32 order;
    u32 nbpasses;
    s16 coefs[0];
} t_cheby;

void *pdp_imageproc_cheby_new(int order)
{
    t_cheby *z;
    int i;
    if (order < 2) order = 2;
    z = (t_cheby *)pdp_alloc(sizeof(t_cheby) + (order + 1) * sizeof(s16[4]));
    z->order = order;
    setvec(z->coefs + 0*4, 0);
    setvec(z->coefs + 1*4, 0.25);
    for (i=2; i<=order; i++)  setvec(z->coefs + i*4, 0);

    return z;
}
void pdp_imageproc_cheby_delete(void *x){pdp_dealloc(x);}
void pdp_imageproc_cheby_setcoef(void *x, u32 n, float f)
{
    t_cheby *z = (t_cheby *)x;
    if (n <= z->order){
	setvec(z->coefs + n*4, f * 0.25); // coefs are in s2.13 format
    }
}
void pdp_imageproc_cheby_setnbpasses(void *x, u32 n){((t_cheby *)x)->nbpasses = n;}


static INLINE void
pixel_cheby_s16_3plus(s16 *image, int nb_vec, int order_plus_one, s16 *coefs) {
    /* Coefs are s2.13 fixed point [-4,4] */
    __m64 *a = (void*)coefs;
    __m64 *im = (void*)image;
    __m64 a0 = a[0];
    __m64 FFFF = _mm_cmpeq_pi16(a0, a0);  // independent of a0
    __m64 one = _mm_srli_pi16(FFFF, 1); // logic shift -> 7FFF
    __m64 a0_div_2 = _mm_srai_pi16(a0, 1);
    int n,v;
    for (v = 0; v < nb_vec; v++) {
        __m64 x = im[v];
        __m64 T1 = x;
        __m64 T2 = one;
        __m64 a1x_div_2 = _mm_mulhi_pi16(a[1], x);
        __m64 acc = _mm_adds_pi16(a1x_div_2, a0_div_2);
        for (n = 2; n < order_plus_one; n++) {
            /* Chebyshev recursion: T = 2x T1 - T2 */
            __m64 xT1_div_2 = _mm_mulhi_pi16(x, T1);
            __m64 T2_div_4 = _mm_srai_pi16(T2, 2);
            __m64 T_div_4 = _mm_subs_pi16(xT1_div_2, T2_div_4);
            __m64 T_div_2 = _mm_adds_pi16(T_div_4, T_div_4);
            __m64 T = _mm_adds_pi16(T_div_2, T_div_2);
            /* Polynomial evaluation */
            __m64 aT_div_2 = _mm_mulhi_pi16(a[n], T);
            acc = _mm_adds_pi16(acc, aT_div_2);
            /* Recursion state update */
            T2 = T1;
            T1 = T;
        }
        /* Compensate for overall 1/8 factor. */
        __m64 acc_2 = _mm_adds_pi16(acc, acc);
        __m64 acc_4 = _mm_adds_pi16(acc_2, acc_2);
        __m64 acc_8 = _mm_adds_pi16(acc_4, acc_4);
        im[v] = acc_8;
    }
    _mm_empty();
}

void pdp_imageproc_cheby_process(void *x, u32 width, u32 height, s16 *image)
{
    t_cheby *z = (t_cheby *)x;
    u32 iterations = z->nbpasses;
    u32 i,j;
    for (j=0; j < (height*width); j += width) {
        for (i=0; i<iterations; i++) {
            pixel_cheby_s16_3plus(image+j, width>>2, z->order+1, z->coefs);
        }
    }

    //pixel_cheby_s16_3plus(image, (width*height)>>2, z->order+1, z->coefs);
}



