
/*
 *   zl - pbuffer packet implementation
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

/*
  this code uses glx. i don't know if it is worth to take into
  account portabiliy. since it will take a while until pdp runs
  on anything else than linux. but in any case, providing a windows/osx
  implementation here should not be too difficult..
*/

#include "zl/3Dcontext.h"
#include <GL/gl.h>
#include <GL/glu.h>

#define D if (0)

/* constructor */

/* pbuf operators */

uint32_t zl_3Dcontext_width(zl_3Dcontext *c) {
    if (c) return c->width;
    else return 0;
}

uint32_t zl_3Dcontext_height(zl_3Dcontext *c) {
    if (c) return c->height;
    else return 0;
}

uint32_t zl_3Dcontext_subwidth(zl_3Dcontext *c) {
    if (c) return c->sub_width;
    else return 0;
}


uint32_t zl_3Dcontext_subheight(zl_3Dcontext *c) {
    if (c) return c->sub_height;
    else return 0;
}


void  zl_3Dcontext_set_subwidth(zl_3Dcontext *c, uint32_t w) {
    if (c) c->sub_width = w;
}


void zl_3Dcontext_set_subheight(zl_3Dcontext *c, uint32_t h) {
    if (c)  c->sub_height = h;
}


float zl_3Dcontext_subaspect(zl_3Dcontext *c) {
    if (c) return (float)c->sub_width/c->sub_height;
    else return 0;
}


void zl_3Dcontext_snap_to_bitmap(zl_3Dcontext *c, int w, int h, void *_data) {
    int x, y;
    char *data = _data;

    x = zl_3Dcontext_subwidth(c);
    y = zl_3Dcontext_subheight(c);

    // ???
    x = (x - w) >> 1;
    y = (y - h) >> 1;
    x = (x < 0 ) ? 0 : x;
    y = (y < 0 ) ? 0 : y;

    zl_3Dcontext_set_rendering_context(c);

    glReadPixels(x,y, w ,h,GL_RGB,GL_UNSIGNED_BYTE, data);
}




/* setup for 2d operation from pbuf dimensions */
void zl_3Dcontext_setup_2d_context(zl_3Dcontext *c) {
    uint32_t w;
    uint32_t h;
    float asp;
    w = zl_3Dcontext_subwidth(c);
    h = zl_3Dcontext_subheight(c);
    asp = zl_3Dcontext_subaspect(c);


    /* set the viewport to the size of the sub frame */
    glViewport(0, 0, w, h);

    /* set orthogonal projection, with a relative frame size of (2asp x 2) */
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    gluOrtho2D(0, 2*asp, 0, 2);

    /* set the center of view */
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
    glTranslatef(asp, 1, 0);
    glScalef(1,-1,1);


}

/* setup for 3d operation from pbuf dimensions */
void zl_3Dcontext_setup_3d_context(zl_3Dcontext *c) {
    uint32_t w;
    uint32_t h;
    int i;
    float asp;
    float m_perspect[] = {-1.f, /* left */
			  1.f,  /* right */
			  -1.f, /* bottom */
			  1.f,  /* top */
			  1.f,  /* front */
			  20.f};/* back */

    w = zl_3Dcontext_subwidth(c);
    h = zl_3Dcontext_subheight(c);
    asp = zl_3Dcontext_subaspect(c);


    /* set the viewport to the size of the sub frame */
    glViewport(0, 0, w, h);

    /* set orthogonal projection, with a relative frame size of (2asp x 2) */
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glFrustum(m_perspect[0] * asp, m_perspect[1] * asp,       // left, right
	      m_perspect[2], m_perspect[3],                   // bottom, top
	      m_perspect[4], m_perspect[5]);                  // front, back
    
    /* reset texture matrix */
    glMatrixMode(GL_TEXTURE);
    glLoadIdentity();


    /* set the center of view */
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
    gluLookAt(0, 0, 4, 0, 0, 0, 0, 1, 0);
    //glTranslatef(asp, 1, 0);


    glEnable(GL_DEPTH_TEST);
    glEnable(GL_AUTO_NORMAL);
    glEnable(GL_NORMALIZE);
    glShadeModel(GL_SMOOTH);
    //glShadeModel(GL_FLAT);


    /* disable everything that is enabled in other modules
     this resets the ogl state to its initial conditions */
    glDisable(GL_LIGHTING);
    for (i=0; i<8; i++) glDisable(GL_LIGHT0 + i);
    glDisable(GL_COLOR_MATERIAL);


}

