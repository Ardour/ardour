/*
    Copyright (C) 2000 EMC Capital Management, Inc.

    Developed by Jon Trowbridge <trow@gnu.org> and
    Havoc Pennington <hp@pobox.com>.

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.

*/

#ifndef __gtkmm2ext_rgb_macros_h__
#define __gtkmm2ext_rgb_macros_h__

/*
  Some convenient macros for drawing into an RGB buffer.
  Beware of side effects, code-bloat, and all of the other classic
  cpp-perils...
*/

#define RGB_TO_UINT(r,g,b) ((((guint)(r))<<16)|(((guint)(g))<<8)|((guint)(b)))
#define RGB_TO_RGBA(x,a) (((x) << 8) | ((((guint)a) & 0xff)))
#define RGBA_TO_UINT(r,g,b,a) RGB_TO_RGBA(RGB_TO_UINT(r,g,b), a)
#define RGB_WHITE  RGB_TO_UINT(0xff, 0xff, 0xff)
#define RGB_BLACK  RGB_TO_UINT(0x00, 0x00, 0x00)
#define RGB_RED    RGB_TO_UINT(0xff, 0x00, 0x00)
#define RGB_GREEN  RGB_TO_UINT(0x00, 0xff, 0x00)
#define RGB_BLUE   RGB_TO_UINT(0x00, 0x00, 0xff)
#define RGB_YELLOW RGB_TO_UINT(0xff, 0xff, 0x00)
#define RGB_VIOLET RGB_TO_UINT(0xff, 0x00, 0xff)
#define RGB_CYAN   RGB_TO_UINT(0x00, 0xff, 0xff)
#define RGBA_WHITE  RGB_TO_RGBA(RGB_WHITE, 0xff)
#define RGBA_BLACK  RGB_TO_RGBA(RGB_BLACK, 0xff)
#define RGBA_RED    RGB_TO_RGBA(RGB_RED, 0xff)
#define RGBA_GREEN  RGB_TO_RGBA(RGB_GREEN, 0xff)
#define RGBA_BLUE   RGB_TO_RGBA(RGB_BLUE, 0xff)
#define RGBA_YELLOW RGB_TO_RGBA(RGB_YELLOW, 0xff)
#define RGBA_VIOLET RGB_TO_RGBA(RGB_VIOLET, 0xff)
#define RGBA_CYAN   RGB_TO_RGBA(RGB_CYAN, 0xff)
#define RGB_GREY(x) RGB_TO_UINT(x,x,x)
#define RGBA_GREY(x) RGB_TO_RGBA(RGB_GREY(x), 0xff)
#define UINT_RGBA_R(x) (((guint)(x))>>24)
#define UINT_RGBA_G(x) ((((guint)(x))>>16)&0xff)
#define UINT_RGBA_B(x) ((((guint)(x))>>8)&0xff)
#define UINT_RGBA_A(x) (((guint)(x))&0xff)
#define UINT_RGBA_R_FLT(x) ((((guint)(x))>>24)/255.0)
#define UINT_RGBA_G_FLT(x) (((((guint)(x))>>16)&0xff)/255.0)
#define UINT_RGBA_B_FLT(x) (((((guint)(x))>>8)&0xff)/255.0)
#define UINT_RGBA_A_FLT(x) ((((guint)(x))&0xff)/255.0)
#define UINT_RGBA_CHANGE_R(x, r) (((x)&(~(0xff<<24)))|(((r)&0xff)<<24))
#define UINT_RGBA_CHANGE_G(x, g) (((x)&(~(0xff<<16)))|(((g)&0xff)<<16))
#define UINT_RGBA_CHANGE_B(x, b) (((x)&(~(0xff<<8)))|(((b)&0xff)<<8))
#define UINT_RGBA_CHANGE_A(x, a) (((x)&(~0xff))|((a)&0xff))
#define UINT_TO_RGB(u,r,g,b) \
{ (*(r)) = ((u)>>16)&0xff; (*(g)) = ((u)>>8)&0xff; (*(b)) = (u)&0xff; }
#define UINT_TO_RGBA(u,r,g,b,a) \
{ UINT_TO_RGB(((u)>>8),r,g,b); (*(a)) = (u)&0xff; }
#define MONO_INTERPOLATE(v1, v2, t) ((gint)rint((v2)*(t)+(v1)*(1-(t))))
#define UINT_INTERPOLATE(c1, c2, t) \
  RGBA_TO_UINT( MONO_INTERPOLATE(UINT_RGBA_R(c1), UINT_RGBA_R(c2), t), \
		MONO_INTERPOLATE(UINT_RGBA_G(c1), UINT_RGBA_G(c2), t), \
		MONO_INTERPOLATE(UINT_RGBA_B(c1), UINT_RGBA_B(c2), t), \
		MONO_INTERPOLATE(UINT_RGBA_A(c1), UINT_RGBA_A(c2), t) )
#define PIXEL_RGB(p, r, g, b) \
{((guchar*)(p))[0]=(r); ((guchar*)(p))[1]=(g); ((guchar*)(p))[2]=(b);}
#define PIXEL_RGBA(p, r, g, b, a) \
{ if ((a)>=0xff) { PIXEL_RGB(p,r,g,b) } \
  else if ((a)>0) { \
    guint pixel_tmp; \
    pixel_tmp = ((guchar*)(p))[0]; \
    ((guchar*)(p))[0] = pixel_tmp + ((((r)-pixel_tmp)*(a)+0x80) >> 8); \
    pixel_tmp = ((guchar*)(p))[1]; \
    ((guchar*)(p))[1] = pixel_tmp + ((((g)-pixel_tmp)*(a)+0x80) >> 8); \
    pixel_tmp = ((guchar*)(p))[2]; \
    ((guchar*)(p))[2] = pixel_tmp + ((((b)-pixel_tmp)*(a)+0x80) >> 8); }}
#define PIXEL_RGB_UINT(p, i) \
UINT_TO_RGB((i), ((guchar*)p), ((guchar*)p)+1, ((guchar*)p)+2)
#define PIXEL_RGBA_UINT(p, i) \
  PIXEL_RGBA((p), ((i)>>24)&0xff, ((i)>>16)&0xff, ((i)>>8)&0xff, (i)&0xff)
#define PIXEL_BLACK(p) PIXEL_RGB(p,0,0,0)
#define PIXEL_WHITE(p) PIXEL_RGB(p,0xff,0xff,0xff)
#define PIXEL_GREY(p,g) PIXEL_RGB(p,g,g,g)
#define PIXEL_GREYA(p,g,a) PIXEL_RGBA(p,g,g,g,a)
#define BUF_PTR(inbuf, ptx, pty) \
 ((inbuf)->buf + 3*((ptx)-(inbuf)->rect.x0) + (inbuf)->buf_rowstride*((pty)-(inbuf)->rect.y0))
#define BUF_INBOUNDS_X(inbuf, ptx) \
((inbuf)->rect.x0 <= (ptx) && (ptx) < (inbuf)->rect.x1)
#define BUF_INBOUNDS_Y(inbuf, pty) \
((inbuf)->rect.y0 <= (pty) && (pty) < (inbuf)->rect.y1)
#define PAINT_DOT(inbuf, colr, colg, colb,ptx, pty) \
{ \
  guchar* pd_p; \
  if (BUF_INBOUNDS_X(inbuf, ptx) && BUF_INBOUNDS_Y(inbuf, pty)) { \
    pd_p = BUF_PTR(inbuf, ptx, pty); \
    PIXEL_RGB(pd_p, (colr), (colg), (colb)); \
  } \
}
#define FAST_PAINT_DOT(inbuf, colr, colg, colb,ptx, pty) \
{ \
  guchar* pd_p; \
  pd_p = BUF_PTR(inbuf, ptx, pty); \
  PIXEL_RGB(pd_p, (colr), (colg), (colb)); \
}
#define PAINT_DOTA(inbuf, colr, colg, colb, cola, ptx, pty) \
{ \
  guchar* pd_p; \
  if (BUF_INBOUNDS_X(inbuf, ptx) && BUF_INBOUNDS_Y(inbuf, pty)) { \
    pd_p = BUF_PTR(inbuf, ptx, pty); \
    PIXEL_RGBA(pd_p, (colr), (colg), (colb), (cola)); \
  } \
}
#define FAST_PAINT_DOTA(inbuf, colr, colg, colb, cola, ptx, pty) \
{ \
  guchar* pd_p; \
  pd_p = BUF_PTR(inbuf, ptx, pty); \
  PIXEL_RGBA(pd_p, (colr), (colg), (colb), (cola)); \
}
#define PAINT_HORIZ(inbuf, colr, colg, colb, ptx0, ptx1, pty) \
{ \
  GnomeCanvasBuf* ph_buf = (inbuf); \
  guchar* ph_p; \
  gint ph_a0, ph_a1; \
  gint ph_colr=(colr), ph_colg=(colg), ph_colb=(colb); \
\
  ph_a0 = MAX(ph_buf->rect.x0, (gint)(ptx0)); \
  ph_a1 = MIN(ph_buf->rect.x1, (gint)(ptx1)); \
\
  if (ph_a0 < ph_a1 && BUF_INBOUNDS_Y(ph_buf, (gint)(pty))) { \
    ph_p = BUF_PTR(ph_buf, ph_a0, pty); \
    while (ph_a0 < ph_a1) { \
      PIXEL_RGB(ph_p, ph_colr, ph_colg, ph_colb); \
      ++ph_a0; \
      ph_p += 3; \
    } \
  } \
}
#define FAST_PAINT_HORIZ(inbuf, colr, colg, colb, ptx0, ptx1, pty) \
{ \
  GnomeCanvasBuf* ph_buf = (inbuf); \
  guchar* ph_p; \
  gint ph_a0, ph_a1; \
  gint ph_colr=(colr), ph_colg=(colg), ph_colb=(colb); \
\
  ph_a0 = MAX(ph_buf->rect.x0, (gint)(ptx0)); \
  ph_a1 = MIN(ph_buf->rect.x1, (gint)(ptx1)); \
\
  if (ph_a0 < ph_a1 && BUF_INBOUNDS_Y(ph_buf, (gint)(pty))) { \
    ph_p = BUF_PTR(ph_buf, ph_a0, pty); \
    while (ph_a0 < ph_a1) { \
      PIXEL_RGB(ph_p, ph_colr, ph_colg, ph_colb); \
      ++ph_a0; \
      ph_p += 3; \
    } \
  } \
}
#define PAINT_HORIZA(inbuf, colr, colg, colb, cola, ptx0, ptx1, pty) \
{ \
  GnomeCanvasBuf* ph_buf = (inbuf); \
  guchar* ph_p; \
  gint ph_a0, ph_a1; \
  gint ph_colr=(colr), ph_colg=(colg), ph_colb=(colb), ph_cola=(cola); \
\
  ph_a0 = MAX(ph_buf->rect.x0, (gint)(ptx0)); \
  ph_a1 = MIN(ph_buf->rect.x1, (gint)(ptx1)); \
\
  if (ph_a0 < ph_a1 && BUF_INBOUNDS_Y(ph_buf, (gint)(pty))) { \
    ph_p = BUF_PTR(ph_buf, ph_a0, pty); \
    while (ph_a0 < ph_a1) { \
      PIXEL_RGBA(ph_p, ph_colr, ph_colg, ph_colb, ph_cola); \
      ++ph_a0; \
      ph_p += 3; \
    } \
  } \
}
#define PAINT_VERT(inbuf, colr, colg, colb, ptx, pty0, pty1) \
{ \
  GnomeCanvasBuf* pv_buf = (inbuf); \
  guchar* pv_p; \
  gint pv_b0, pv_b1; \
  gint pv_colr=(colr), pv_colg=(colg), pv_colb=(colb);\
\
  pv_b0 = MAX(pv_buf->rect.y0, (gint)(pty0)); \
  pv_b1 = MIN(pv_buf->rect.y1, (gint)(pty1)); \
\
 if (pv_b0 < pv_b1 && BUF_INBOUNDS_X(pv_buf, (gint)(ptx))) { \
    pv_p = BUF_PTR(pv_buf, ptx, pv_b0); \
    while (pv_b0 < pv_b1) { \
      PIXEL_RGB(pv_p, pv_colr, pv_colg, pv_colb); \
      ++pv_b0; \
      pv_p += pv_buf->buf_rowstride; \
    } \
  } \
}
#define FAST_PAINT_VERT(inbuf, colr, colg, colb, ptx, pty0, pty1) \
{ \
  GnomeCanvasBuf* fpv_buf = (inbuf); \
  guchar* fpv_p; \
  gint fpv_b0, fpv_b1; \
\
  fpv_b0 = MAX(fpv_buf->rect.y0, (gint)(pty0)); \
  fpv_b1 = MIN(fpv_buf->rect.y1, (gint)(pty1)); \
\
  fpv_p = BUF_PTR(fpv_buf, ptx, fpv_b0); \
\
  while (fpv_b0 < fpv_b1) { \
      PIXEL_RGB(fpv_p, colr, colg, colb); \
      ++fpv_b0; \
      fpv_p += fpv_buf->buf_rowstride; \
  } \
}
#define PAINT_VERTA(inbuf, colr, colg, colb, cola, ptx, pty0, pty1) \
{ \
  GnomeCanvasBuf* pv_buf = (inbuf); \
  guchar* pv_p; \
  gint pv_b0, pv_b1; \
  gint pv_colr=(colr), pv_colg=(colg), pv_colb=(colb), pv_cola=(cola);\
\
  pv_b0 = MAX(pv_buf->rect.y0, (pty0)); \
  pv_b1 = MIN(pv_buf->rect.y1, (pty1)); \
\
 if (pv_b0 < pv_b1 && BUF_INBOUNDS_X(pv_buf, ptx)) { \
    pv_p = BUF_PTR(pv_buf, ptx, pv_b0); \
    while (pv_b0 < pv_b1) { \
      PIXEL_RGBA(pv_p, pv_colr, pv_colg, pv_colb, pv_cola); \
      ++pv_b0; \
      pv_p += pv_buf->buf_rowstride; \
    } \
  } \
}

#define PAINT_VERTA_GR(inbuf, colr, colg, colb, cola, ptx, pty0, pty1, origin_y, obj_top) \
{ \
  GnomeCanvasBuf* pv_buf = (inbuf); \
  guchar* pv_p; \
  gint pv_b0, pv_b1; \
  gint pv_colr=(colr), pv_colg=(colg), pv_colb=(colb), pv_cola=(cola); \
  gint y_fract; \
  gint y_span = (origin_y - obj_top); \
  gint sat; \
\
  pv_b0 = MAX(pv_buf->rect.y0, (pty0)); \
  pv_b1 = MIN(pv_buf->rect.y1, (pty1)); \
\
 if (pv_b0 < pv_b1 && BUF_INBOUNDS_X(pv_buf, ptx)) { \
    pv_p = BUF_PTR(pv_buf, ptx, pv_b0); \
    while (pv_b0 < pv_b1) { \
      y_fract = (abs(origin_y - pv_b0)) * 0xFF; \
	  y_fract = y_fract / y_span; \
	  sat = 0xFF - (y_fract); \
	  PIXEL_RGBA(pv_p, (((pv_colr << 8) * sat) >> 16), (((pv_colg << 8) * sat) >> 16), (((pv_colb << 8) * sat) >> 16), pv_cola); \
      ++pv_b0; \
      pv_p += pv_buf->buf_rowstride; \
    } \
  } \
}

/* Paint a solid-colored box into a GnomeCanvasBuf (clipping as necessary).
   The box contains (ptx0,pty0), but not (ptx1, pty1).
   Each macro arg should appear exactly once in the body of the code. */
#define PAINT_BOX(inbuf, colr, colg, colb, cola, ptx0, pty0, ptx1, pty1) \
{ \
  GnomeCanvasBuf* pb_buf = (inbuf); \
  guchar* pb_p; \
  guchar* pb_pp; \
  gint pb_a0, pb_a1, pb_b0, pb_b1, pb_i, pb_j; \
  gint pb_colr=(colr), pb_colg=(colg), pb_colb=(colb), pb_cola=(cola); \
\
  pb_a0 = MAX(pb_buf->rect.x0, (ptx0)); \
  pb_a1 = MIN(pb_buf->rect.x1, (ptx1)); \
  pb_b0 = MAX(pb_buf->rect.y0, (pty0)); \
  pb_b1 = MIN(pb_buf->rect.y1, (pty1)); \
\
  if (pb_a0 < pb_a1 && pb_b0 < pb_b1) { \
    pb_p = BUF_PTR(pb_buf, pb_a0, pb_b0); \
    for (pb_j=pb_b0; pb_j<pb_b1; ++pb_j) { \
      pb_pp = pb_p; \
      for (pb_i=pb_a0; pb_i<pb_a1; ++pb_i) { \
        PIXEL_RGBA(pb_pp, pb_colr, pb_colg, pb_colb, pb_cola); \
        pb_pp += 3; \
      } \
      pb_p += pb_buf->buf_rowstride; \
    } \
  } \
}

/* Paint a gradient-colored box into a GnomeCanvasBuf (clipping as necessary).
   The box contains (ptx0,pty0), but not (ptx1, pty1).
   Each macro arg should appear exactly once in the body of the code. */
#define PAINT_BOX_GR(inbuf, colr, colg, colb, cola, ptx0, pty0, ptx1, pty1, v_span) \
{ \
  GnomeCanvasBuf* pb_buf = (inbuf); \
  guchar* pb_p; \
  guchar* pb_pp; \
  gint pb_a0, pb_a1, pb_b0, pb_b1, pb_i, pb_j; \
  gint pb_colr=(colr), pb_colg=(colg), pb_colb=(colb), pb_cola=(cola); \
  gint sat; \
  gint y_fract; \
  gint y_span = MAX(abs(v_span), 1); \
\
  pb_a0 = MAX(pb_buf->rect.x0, (ptx0)); \
  pb_a1 = MIN(pb_buf->rect.x1, (ptx1)); \
  pb_b0 = MAX(pb_buf->rect.y0, (pty0)); \
  pb_b1 = MIN(pb_buf->rect.y1, (pty1)); \
\
  if (pb_a0 < pb_a1 && pb_b0 < pb_b1) { \
    pb_p = BUF_PTR(pb_buf, pb_a0, pb_b0); \
    for (pb_j=pb_b0; pb_j<pb_b1; ++pb_j) { \
	  y_fract = 0xFF * (abs(pb_j - pty0));  \
	  y_fract = y_fract / y_span; \
	  sat = 0xFF - (y_fract >> 1); \
      pb_pp = pb_p; \
      for (pb_i=pb_a0; pb_i<pb_a1; ++pb_i) { \
        PIXEL_RGBA(pb_pp, (((pb_colr << 8) * sat) >> 16), (((pb_colg << 8) * sat) >> 16), (((pb_colb << 8) * sat) >> 16), pb_cola); \
        pb_pp += 3; \
      } \
      pb_p += pb_buf->buf_rowstride; \
    } \
  } \
}


/* No bounds checking in this version */

#define FAST_PAINT_BOX(inbuf, colr, colg, colb, cola, ptx0, pty0, ptx1, pty1) \
{ \
  GnomeCanvasBuf* pb_buf = (inbuf); \
  guchar* pb_p; \
  guchar* pb_pp; \
  gint pb_i, pb_j; \
\
  pb_p = BUF_PTR(pb_buf, ptx0, pty0); \
  for (pb_j=pty0; pb_j<pty1; ++pb_j) { \
      pb_pp = pb_p; \
      for (pb_i=ptx0; pb_i<ptx1; ++pb_i) { \
        PIXEL_RGBA(pb_pp, colr, colg, colb, cola); \
        pb_pp += 3; \
      } \
      pb_p += pb_buf->buf_rowstride; \
    } \
}

#endif /* __gtkmm2ext_rgb_macros_h__ */
