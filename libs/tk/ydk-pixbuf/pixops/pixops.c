/*
 * Copyright (C) 2000 Red Hat, Inc
 * mediaLib integration Copyright (c) 2001-2007 Sun Microsystems, Inc.
 * All rights reserved.  (Brian Cameron, Dmitriy Demin, James Cheng,
 * Padraig O'Briain)
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, see <http://www.gnu.org/licenses/>.
 */
#include "config.h"
#include <math.h>
#include <glib.h>

#include "pixops.h"
#include "pixops-internal.h"

#define SUBSAMPLE_BITS 4
#define SUBSAMPLE (1 << SUBSAMPLE_BITS)
#define SUBSAMPLE_MASK ((1 << SUBSAMPLE_BITS)-1)
#define SCALE_SHIFT 16

static void
_pixops_scale_real (guchar        *dest_buf,
                    int            render_x0,
                    int            render_y0,
                    int            render_x1,
                    int            render_y1,
                    int            dest_rowstride,
                    int            dest_channels,
                    gboolean       dest_has_alpha,
                    const guchar  *src_buf,
                    int            src_width,
                    int            src_height,
                    int            src_rowstride,
                    int            src_channels,
                    gboolean       src_has_alpha,
                    double         scale_x,
                    double         scale_y,
                    PixopsInterpType  interp_type);

typedef struct _PixopsFilter PixopsFilter;
typedef struct _PixopsFilterDimension PixopsFilterDimension;

struct _PixopsFilterDimension
{
  int n;
  double offset;
  double *weights;
};

struct _PixopsFilter
{
  PixopsFilterDimension x;
  PixopsFilterDimension y;
  double overall_alpha;
}; 

typedef guchar *(*PixopsLineFunc) (int *weights, int n_x, int n_y,
				   guchar *dest, int dest_x, guchar *dest_end,
				   int dest_channels, int dest_has_alpha,
				   guchar **src, int src_channels,
				   gboolean src_has_alpha, int x_init,
				   int x_step, int src_width, int check_size,
				   guint32 color1, guint32 color2);
typedef void (*PixopsPixelFunc)   (guchar *dest, int dest_x, int dest_channels,
				   int dest_has_alpha, int src_has_alpha,
				   int check_size, guint32 color1,
				   guint32 color2,
				   guint r, guint g, guint b, guint a);

#ifdef USE_MEDIALIB
#include <stdlib.h>
#include <dlfcn.h>
#include <mlib_image.h>

#ifdef HAVE_STRINGS_H
#include <strings.h>
#endif

#ifdef HAVE_STRING_H
#include <string.h>
#endif

#if defined(HAVE_SYS_SYSTEMINFO_H)
#include <sys/systeminfo.h>
#elif defined(HAVE_SYS_SYSINFO_H)
#include <sys/sysinfo.h>
#endif

static void pixops_medialib_composite    (guchar          *dest_buf,
                                          int              dest_width,
                                          int              dest_height,
                                          int              dest_rowstride,
                                          int              dest_channels,
                                          int              dest_has_alpha,
                                          const guchar    *src_buf,
                                          int              src_width,
                                          int              src_height,
                                          int              src_rowstride,
                                          int              src_channels,
                                          int              src_has_alpha,
                                          int              dest_x,
                                          int              dest_y,
                                          int              dest_region_width,
                                          int              dest_region_height,
                                          double           offset_x,
                                          double           offset_y,
                                          double           scale_x,
                                          double           scale_y,
                                          PixopsInterpType interp_type,
                                          int              overall_alpha);

static void pixops_medialib_scale        (guchar          *dest_buf,
                                          int              dest_width,
                                          int              dest_height,
                                          int              dest_rowstride,
                                          int              dest_channels,
                                          int              dest_has_alpha,
                                          const guchar    *src_buf,
                                          int              src_width,
                                          int              src_height,
                                          int              src_rowstride,
                                          int              src_channels,
                                          int              src_has_alpha,
                                          int              dest_x,
                                          int              dest_y,
                                          int              dest_region_width,
                                          int              dest_region_height,
                                          double           offset_x,
                                          double           offset_y,
                                          double           scale_x,
                                          double           scale_y,
                                          PixopsInterpType interp_type);

typedef struct _mlInterp mlInterp;

struct _mlInterp
{
  double       tx;
  double       ty;
  PixopsFilter po_filter;
  void         *interp_table;
};

static gboolean medialib_initialized = FALSE;
static gboolean use_medialib         = TRUE;

/*
 * Sun mediaLib(tm) support.
 *
 *   http://www.sun.com/processors/vis/mlib.html
 *
 */
static void
_pixops_use_medialib ()
{
  char *mlib_version_string;
  char  sys_info[257];
  long  count;

  medialib_initialized = TRUE; 

  if (getenv ("GDK_DISABLE_MEDIALIB"))
    {
      use_medialib = FALSE;
      return;
    }

  /*
   * The imaging functions we want to use were added in mediaLib version 2.
   * So turn off mediaLib support if the user has an older version.
   * mlib_version returns a string in this format:
   *
   * mediaLib:0210:20011101:v8plusa
   * ^^^^^^^^ ^^^^ ^^^^^^^^ ^^^^^^^
   * libname  vers  build   ISALIST identifier
   *                date    (in this case sparcv8plus+vis)
   * 
   * The first 2 digits of the version are the major version.  The 3rd digit
   * is the minor version, and the 4th digit is the micro version.  So the
   * above string corresponds to version 2.1.0.  In the following test we only
   * care about the major version.
   */
  mlib_version_string = mlib_version ();

  count = sysinfo (SI_ARCHITECTURE, &sys_info[0], 257);

  if (count != -1)
    {
      if (strcmp (sys_info, "i386") == 0)
        {
          char *mlib_target_isa = &mlib_version_string[23];

          /*
           * For x86 processors mediaLib generic C implementation
           * does not give any performance advantage so disable it
           */
          if (strncmp (mlib_target_isa, "sse", 3) != 0)
            {
              use_medialib = FALSE;
              return;
            }

          /*
           * For x86 processors use of libumem conflicts with
           * mediaLib, so avoid using it.
           */
          if (dlsym (RTLD_PROBE,   "umem_alloc") != NULL)
            {
              use_medialib = FALSE;
              return;
            }
        }
    }
  else
    {
      /* Failed to get system architecture, disable mediaLib anyway */
      use_medialib = FALSE;
      return;
    }
}
#endif

static int
get_check_shift (int check_size)
{
  int check_shift = 0;
  g_return_val_if_fail (check_size >= 0, 4);

  while (!(check_size & 1))
    {
      check_shift++;
      check_size >>= 1;
    }

  return check_shift;
}

static void
pixops_scale_nearest (guchar        *dest_buf,
		      int            render_x0,
		      int            render_y0,
		      int            render_x1,
		      int            render_y1,
		      int            dest_rowstride,
		      int            dest_channels,
		      gboolean       dest_has_alpha,
		      const guchar  *src_buf,
		      int            src_width,
		      int            src_height,
		      int            src_rowstride,
		      int            src_channels,
		      gboolean       src_has_alpha,
		      double         scale_x,
		      double         scale_y)
{
  int i;
  int x;
  int x_step = (1 << SCALE_SHIFT) / scale_x;
  int y_step = (1 << SCALE_SHIFT) / scale_y;
  int xmax, xstart, xstop, x_pos, y_pos;
  const guchar *p;

#define INNER_LOOP(SRC_CHANNELS,DEST_CHANNELS,ASSIGN_PIXEL)     \
      xmax = x + (render_x1 - render_x0) * x_step;              \
      xstart = MIN (0, xmax);                                   \
      xstop = MIN (src_width << SCALE_SHIFT, xmax);             \
      p = src + (CLAMP (x, xstart, xstop) >> SCALE_SHIFT) * SRC_CHANNELS; \
      while (x < xstart)                                        \
        {                                                       \
          ASSIGN_PIXEL;                                         \
          dest += DEST_CHANNELS;                                \
          x += x_step;                                          \
        }                                                       \
      while (x < xstop)                                         \
        {                                                       \
          p = src + (x >> SCALE_SHIFT) * SRC_CHANNELS;          \
          ASSIGN_PIXEL;                                         \
          dest += DEST_CHANNELS;                                \
          x += x_step;                                          \
        }                                                       \
      x_pos = x >> SCALE_SHIFT;                                 \
      p = src + CLAMP (x_pos, 0, src_width - 1) * SRC_CHANNELS; \
      while (x < xmax)                                          \
        {                                                       \
          ASSIGN_PIXEL;                                         \
          dest += DEST_CHANNELS;                                \
          x += x_step;                                          \
        }

  for (i = 0; i < (render_y1 - render_y0); i++)
    {
      const guchar *src;
      guchar       *dest;
      y_pos = ((i + render_y0) * y_step + y_step / 2) >> SCALE_SHIFT;
      y_pos = CLAMP (y_pos, 0, src_height - 1);
      src  = src_buf + y_pos * src_rowstride;
      dest = dest_buf + i * dest_rowstride;

      x = render_x0 * x_step + x_step / 2;

      if (src_channels == 3)
	{
	  if (dest_channels == 3)
	    {
	      INNER_LOOP (3, 3, dest[0]=p[0];dest[1]=p[1];dest[2]=p[2]);
	    }
	  else
	    {
	      INNER_LOOP (3, 4, dest[0]=p[0];dest[1]=p[1];dest[2]=p[2];dest[3]=0xff);
	    }
	}
      else if (src_channels == 4)
	{
	  if (dest_channels == 3)
	    {
	      INNER_LOOP (4, 3, dest[0]=p[0];dest[1]=p[1];dest[2]=p[2]);
	    }
	  else
	    {
	      guint32 *p32;
	      INNER_LOOP(4, 4, p32=(guint32*)dest;*p32=*((guint32*)p));
	    }
	}
    }
}

static void
pixops_composite_nearest (guchar        *dest_buf,
			  int            render_x0,
			  int            render_y0,
			  int            render_x1,
			  int            render_y1,
			  int            dest_rowstride,
			  int            dest_channels,
			  gboolean       dest_has_alpha,
			  const guchar  *src_buf,
			  int            src_width,
			  int            src_height,
			  int            src_rowstride,
			  int            src_channels,
			  gboolean       src_has_alpha,
			  double         scale_x,
			  double         scale_y,
			  int            overall_alpha)
{
  int i;
  int x;
  int x_step = (1 << SCALE_SHIFT) / scale_x;
  int y_step = (1 << SCALE_SHIFT) / scale_y;
  int xmax, xstart, xstop, x_pos, y_pos;
  const guchar *p;
  unsigned int  a0;

  for (i = 0; i < (render_y1 - render_y0); i++)
    {
      const guchar *src;
      guchar       *dest;
      y_pos = ((i + render_y0) * y_step + y_step / 2) >> SCALE_SHIFT;
      y_pos = CLAMP (y_pos, 0, src_height - 1);
      src  = src_buf + y_pos * src_rowstride;
      dest = dest_buf + i * dest_rowstride;

      x = render_x0 * x_step + x_step / 2;
      
      INNER_LOOP(src_channels, dest_channels,
	  if (src_has_alpha)
	    a0 = (p[3] * overall_alpha) / 0xff;
	  else
	    a0 = overall_alpha;

          switch (a0)
            {
            case 0:
              break;
            case 255:
              dest[0] = p[0];
              dest[1] = p[1];
              dest[2] = p[2];
              if (dest_has_alpha)
                dest[3] = 0xff;
              break;
            default:
              if (dest_has_alpha)
                {
                  unsigned int w0 = 0xff * a0;
                  unsigned int w1 = (0xff - a0) * dest[3];
                  unsigned int w = w0 + w1;

		  dest[0] = (w0 * p[0] + w1 * dest[0]) / w;
		  dest[1] = (w0 * p[1] + w1 * dest[1]) / w;
		  dest[2] = (w0 * p[2] + w1 * dest[2]) / w;
		  dest[3] = w / 0xff;
                }
              else
                {
                  unsigned int a1 = 0xff - a0;
		  unsigned int tmp;

		  tmp = a0 * p[0] + a1 * dest[0] + 0x80;
                  dest[0] = (tmp + (tmp >> 8)) >> 8;
		  tmp = a0 * p[1] + a1 * dest[1] + 0x80;
                  dest[1] = (tmp + (tmp >> 8)) >> 8;
		  tmp = a0 * p[2] + a1 * dest[2] + 0x80;
                  dest[2] = (tmp + (tmp >> 8)) >> 8;
                }
              break;
            }
	);
    }
}

static void
pixops_composite_color_nearest (guchar        *dest_buf,
				int            render_x0,
				int            render_y0,
				int            render_x1,
				int            render_y1,
				int            dest_rowstride,
				int            dest_channels,
				gboolean       dest_has_alpha,
				const guchar  *src_buf,
				int            src_width,
				int            src_height,
				int            src_rowstride,
				int            src_channels,
				gboolean       src_has_alpha,
				double         scale_x,
				double         scale_y,
				int            overall_alpha,
				int            check_x,
				int            check_y,
				int            check_size,
				guint32        color1,
				guint32        color2)
{
  int i, j;
  int x;
  int x_step = (1 << SCALE_SHIFT) / scale_x;
  int y_step = (1 << SCALE_SHIFT) / scale_y;
  int r1, g1, b1, r2, g2, b2;
  int check_shift = get_check_shift (check_size);
  int xmax, xstart, xstop, x_pos, y_pos;
  const guchar *p;
  unsigned int  a0;

  for (i = 0; i < (render_y1 - render_y0); i++)
    {
      const guchar *src;
      guchar       *dest;
      y_pos = ((i + render_y0) * y_step + y_step / 2) >> SCALE_SHIFT;
      y_pos = CLAMP (y_pos, 0, src_height - 1);
      src  = src_buf + y_pos * src_rowstride;
      dest = dest_buf + i * dest_rowstride;

      x = render_x0 * x_step + x_step / 2;
      
      
      if (((i + check_y) >> check_shift) & 1)
	{
	  r1 = (color2 & 0xff0000) >> 16;
	  g1 = (color2 & 0xff00) >> 8;
	  b1 = color2 & 0xff;

	  r2 = (color1 & 0xff0000) >> 16;
	  g2 = (color1 & 0xff00) >> 8;
	  b2 = color1 & 0xff;
	}
      else
	{
	  r1 = (color1 & 0xff0000) >> 16;
	  g1 = (color1 & 0xff00) >> 8;
	  b1 = color1 & 0xff;

	  r2 = (color2 & 0xff0000) >> 16;
	  g2 = (color2 & 0xff00) >> 8;
	  b2 = color2 & 0xff;
	}

      j = 0;
      INNER_LOOP(src_channels, dest_channels,
	  if (src_has_alpha)
	    a0 = (p[3] * overall_alpha + 0xff) >> 8;
	  else
	    a0 = overall_alpha;

          switch (a0)
            {
            case 0:
              if (((j + check_x) >> check_shift) & 1)
                {
                  dest[0] = r2; 
                  dest[1] = g2; 
                  dest[2] = b2;
                }
              else
                {
                  dest[0] = r1; 
                  dest[1] = g1; 
                  dest[2] = b1;
                }
            break;
            case 255:
	      dest[0] = p[0];
	      dest[1] = p[1];
	      dest[2] = p[2];
              break;
            default:
		     {
		       unsigned int tmp;
              if (((j + check_x) >> check_shift) & 1)
                {
                  tmp = ((int) p[0] - r2) * a0;
                  dest[0] = r2 + ((tmp + (tmp >> 8) + 0x80) >> 8);
                  tmp = ((int) p[1] - g2) * a0;
                  dest[1] = g2 + ((tmp + (tmp >> 8) + 0x80) >> 8);
                  tmp = ((int) p[2] - b2) * a0;
                  dest[2] = b2 + ((tmp + (tmp >> 8) + 0x80) >> 8);
                }
              else
                {
                  tmp = ((int) p[0] - r1) * a0;
                  dest[0] = r1 + ((tmp + (tmp >> 8) + 0x80) >> 8);
                  tmp = ((int) p[1] - g1) * a0;
                  dest[1] = g1 + ((tmp + (tmp >> 8) + 0x80) >> 8);
                  tmp = ((int) p[2] - b1) * a0;
                  dest[2] = b1 + ((tmp + (tmp >> 8) + 0x80) >> 8);
                }
		     }
              break;
            }
	  
	  if (dest_channels == 4)
	    dest[3] = 0xff;

		 j++;
	);
    }
}
#undef INNER_LOOP

static void
composite_pixel (guchar *dest, int dest_x, int dest_channels, int dest_has_alpha,
		 int src_has_alpha, int check_size, guint32 color1, guint32 color2,
		 guint r, guint g, guint b, guint a)
{
  if (dest_has_alpha)
    {
      unsigned int w0 = a - (a >> 8);
      unsigned int w1 = ((0xff0000 - a) >> 8) * dest[3];
      unsigned int w = w0 + w1;
      
      if (w != 0)
	{
	  dest[0] = (r - (r >> 8) + w1 * dest[0]) / w;
	  dest[1] = (g - (g >> 8) + w1 * dest[1]) / w;
	  dest[2] = (b - (b >> 8) + w1 * dest[2]) / w;
	  dest[3] = w / 0xff00;
	}
      else
	{
	  dest[0] = 0;
	  dest[1] = 0;
	  dest[2] = 0;
	  dest[3] = 0;
	}
    }
  else
    {
      dest[0] = (r + (0xff0000 - a) * dest[0]) / 0xff0000;
      dest[1] = (g + (0xff0000 - a) * dest[1]) / 0xff0000;
      dest[2] = (b + (0xff0000 - a) * dest[2]) / 0xff0000;
    }
}

static guchar *
composite_line (int *weights, int n_x, int n_y,
		guchar *dest, int dest_x, guchar *dest_end, int dest_channels, int dest_has_alpha,
		guchar **src, int src_channels, gboolean src_has_alpha,
		int x_init, int x_step, int src_width,
		int check_size, guint32 color1, guint32 color2)
{
  int x = x_init;
  int i, j;

  while (dest < dest_end)
    {
      int x_scaled = x >> SCALE_SHIFT;
      unsigned int r = 0, g = 0, b = 0, a = 0;
      int *pixel_weights;
      
      pixel_weights = weights + ((x >> (SCALE_SHIFT - SUBSAMPLE_BITS)) & SUBSAMPLE_MASK) * n_x * n_y;
      
      for (i=0; i<n_y; i++)
	{
	  guchar *q = src[i] + x_scaled * src_channels;
	  int *line_weights = pixel_weights + n_x * i;
	  
	  for (j=0; j<n_x; j++)
	    {
	      unsigned int ta;

	      if (src_has_alpha)
		ta = q[3] * line_weights[j];
	      else
		ta = 0xff * line_weights[j];
	      
	      r += ta * q[0];
	      g += ta * q[1];
	      b += ta * q[2];
	      a += ta;

	      q += src_channels;
	    }
	}

      if (dest_has_alpha)
	{
	  unsigned int w0 = a - (a >> 8);
	  unsigned int w1 = ((0xff0000 - a) >> 8) * dest[3];
	  unsigned int w = w0 + w1;

	  if (w != 0)
	    {
	      dest[0] = (r - (r >> 8) + w1 * dest[0]) / w;
	      dest[1] = (g - (g >> 8) + w1 * dest[1]) / w;
	      dest[2] = (b - (b >> 8) + w1 * dest[2]) / w;
	      dest[3] = w / 0xff00;
	    }
	  else
	    {
	      dest[0] = 0;
	      dest[1] = 0;
	      dest[2] = 0;
	      dest[3] = 0;
	    }
	}
      else
	{
	  dest[0] = (r + (0xff0000 - a) * dest[0]) / 0xff0000;
	  dest[1] = (g + (0xff0000 - a) * dest[1]) / 0xff0000;
	  dest[2] = (b + (0xff0000 - a) * dest[2]) / 0xff0000;
	}
      
      dest += dest_channels;
      x += x_step;
    }

  return dest;
}

static guchar *
composite_line_22_4a4 (int *weights, int n_x, int n_y,
		       guchar *dest, int dest_x, guchar *dest_end, int dest_channels, int dest_has_alpha,
		       guchar **src, int src_channels, gboolean src_has_alpha,
		       int x_init, int x_step, int src_width,
		       int check_size, guint32 color1, guint32 color2)
{
  int x = x_init;
  guchar *src0 = src[0];
  guchar *src1 = src[1];

  g_return_val_if_fail (src_channels != 3, dest);
  g_return_val_if_fail (src_has_alpha, dest);
  
  while (dest < dest_end)
    {
      int x_scaled = x >> SCALE_SHIFT;
      unsigned int r, g, b, a, ta;
      int *pixel_weights;
      guchar *q0, *q1;
      int w1, w2, w3, w4;
      
      q0 = src0 + x_scaled * 4;
      q1 = src1 + x_scaled * 4;
      
      pixel_weights = (int *)((char *)weights +
	((x >> (SCALE_SHIFT - SUBSAMPLE_BITS - 4)) & (SUBSAMPLE_MASK << 4)));
      
      w1 = pixel_weights[0];
      w2 = pixel_weights[1];
      w3 = pixel_weights[2];
      w4 = pixel_weights[3];

      a = w1 * q0[3];
      r = a * q0[0];
      g = a * q0[1];
      b = a * q0[2];

      ta = w2 * q0[7];
      r += ta * q0[4];
      g += ta * q0[5];
      b += ta * q0[6];
      a += ta;

      ta = w3 * q1[3];
      r += ta * q1[0];
      g += ta * q1[1];
      b += ta * q1[2];
      a += ta;

      ta = w4 * q1[7];
      r += ta * q1[4];
      g += ta * q1[5];
      b += ta * q1[6];
      a += ta;

      dest[0] = ((0xff0000 - a) * dest[0] + r) >> 24;
      dest[1] = ((0xff0000 - a) * dest[1] + g) >> 24;
      dest[2] = ((0xff0000 - a) * dest[2] + b) >> 24;
      dest[3] = a >> 16;
      
      dest += 4;
      x += x_step;
    }

  return dest;
}

#ifdef USE_MMX
static guchar *
composite_line_22_4a4_mmx_stub (int *weights, int n_x, int n_y, guchar *dest,
				int dest_x, guchar *dest_end,
				int dest_channels, int dest_has_alpha,
				guchar **src, int src_channels,
				gboolean src_has_alpha, int x_init,
				int x_step, int src_width, int check_size,
				guint32 color1, guint32 color2)
{
  guint32 mmx_weights[16][8];
  int j;

  for (j=0; j<16; j++)
    {
      mmx_weights[j][0] = 0x00010001 * (weights[4*j] >> 8);
      mmx_weights[j][1] = 0x00010001 * (weights[4*j] >> 8);
      mmx_weights[j][2] = 0x00010001 * (weights[4*j + 1] >> 8);
      mmx_weights[j][3] = 0x00010001 * (weights[4*j + 1] >> 8);
      mmx_weights[j][4] = 0x00010001 * (weights[4*j + 2] >> 8);
      mmx_weights[j][5] = 0x00010001 * (weights[4*j + 2] >> 8);
      mmx_weights[j][6] = 0x00010001 * (weights[4*j + 3] >> 8);
      mmx_weights[j][7] = 0x00010001 * (weights[4*j + 3] >> 8);
    }

  return _pixops_composite_line_22_4a4_mmx (mmx_weights, dest, src[0], src[1],
					    x_step, dest_end, x_init);
}
#endif /* USE_MMX */

static void
composite_pixel_color (guchar *dest, int dest_x, int dest_channels,
		       int dest_has_alpha, int src_has_alpha, int check_size,
		       guint32 color1, guint32 color2, guint r, guint g,
		       guint b, guint a)
{
  int dest_r, dest_g, dest_b;
  int check_shift = get_check_shift (check_size);

  if ((dest_x >> check_shift) & 1)
    {
      dest_r = (color2 & 0xff0000) >> 16;
      dest_g = (color2 & 0xff00) >> 8;
      dest_b = color2 & 0xff;
    }
  else
    {
      dest_r = (color1 & 0xff0000) >> 16;
      dest_g = (color1 & 0xff00) >> 8;
      dest_b = color1 & 0xff;
    }

  dest[0] = ((0xff0000 - a) * dest_r + r) >> 24;
  dest[1] = ((0xff0000 - a) * dest_g + g) >> 24;
  dest[2] = ((0xff0000 - a) * dest_b + b) >> 24;

  if (dest_has_alpha)
    dest[3] = 0xff;
  else if (dest_channels == 4)
    dest[3] = a >> 16;
}

static guchar *
composite_line_color (int *weights, int n_x, int n_y, guchar *dest,
		      int dest_x, guchar *dest_end, int dest_channels,
		      int dest_has_alpha, guchar **src, int src_channels,
		      gboolean src_has_alpha, int x_init, int x_step,
		      int src_width, int check_size, guint32 color1,
		      guint32 color2)
{
  int x = x_init;
  int i, j;
  int check_shift = get_check_shift (check_size);
  int dest_r1, dest_g1, dest_b1;
  int dest_r2, dest_g2, dest_b2;

  g_return_val_if_fail (check_size != 0, dest);

  dest_r1 = (color1 & 0xff0000) >> 16;
  dest_g1 = (color1 & 0xff00) >> 8;
  dest_b1 = color1 & 0xff;

  dest_r2 = (color2 & 0xff0000) >> 16;
  dest_g2 = (color2 & 0xff00) >> 8;
  dest_b2 = color2 & 0xff;

  while (dest < dest_end)
    {
      int x_scaled = x >> SCALE_SHIFT;
      unsigned int r = 0, g = 0, b = 0, a = 0;
      int *pixel_weights;
      
      pixel_weights = weights + ((x >> (SCALE_SHIFT - SUBSAMPLE_BITS)) & SUBSAMPLE_MASK) * n_x * n_y;

      for (i=0; i<n_y; i++)
	{
	  guchar *q = src[i] + x_scaled * src_channels;
	  int *line_weights = pixel_weights + n_x * i;
	  
	  for (j=0; j<n_x; j++)
	    {
	      unsigned int ta;
	      
	      if (src_has_alpha)
		ta = q[3] * line_weights[j];
	      else
		ta = 0xff * line_weights[j];
		  
	      r += ta * q[0];
	      g += ta * q[1];
	      b += ta * q[2];
	      a += ta;

	      q += src_channels;
	    }
	}

      if ((dest_x >> check_shift) & 1)
	{
	  dest[0] = ((0xff0000 - a) * dest_r2 + r) >> 24;
	  dest[1] = ((0xff0000 - a) * dest_g2 + g) >> 24;
	  dest[2] = ((0xff0000 - a) * dest_b2 + b) >> 24;
	}
      else
	{
	  dest[0] = ((0xff0000 - a) * dest_r1 + r) >> 24;
	  dest[1] = ((0xff0000 - a) * dest_g1 + g) >> 24;
	  dest[2] = ((0xff0000 - a) * dest_b1 + b) >> 24;
	}

      if (dest_has_alpha)
	dest[3] = 0xff;
      else if (dest_channels == 4)
	dest[3] = a >> 16;
	
      dest += dest_channels;
      x += x_step;
      dest_x++;
    }

  return dest;
}

#ifdef USE_MMX
static guchar *
composite_line_color_22_4a4_mmx_stub (int *weights, int n_x, int n_y,
				      guchar *dest, int dest_x,
				      guchar *dest_end, int dest_channels,
				      int dest_has_alpha, guchar **src,
				      int src_channels, gboolean src_has_alpha,
				      int x_init, int x_step, int src_width,
				      int check_size, guint32 color1,
				      guint32 color2)
{
  guint32 mmx_weights[16][8];
  int check_shift = get_check_shift (check_size);
  int colors[4];
  int j;

  for (j=0; j<16; j++)
    {
      mmx_weights[j][0] = 0x00010001 * (weights[4*j] >> 8);
      mmx_weights[j][1] = 0x00010001 * (weights[4*j] >> 8);
      mmx_weights[j][2] = 0x00010001 * (weights[4*j + 1] >> 8);
      mmx_weights[j][3] = 0x00010001 * (weights[4*j + 1] >> 8);
      mmx_weights[j][4] = 0x00010001 * (weights[4*j + 2] >> 8);
      mmx_weights[j][5] = 0x00010001 * (weights[4*j + 2] >> 8);
      mmx_weights[j][6] = 0x00010001 * (weights[4*j + 3] >> 8);
      mmx_weights[j][7] = 0x00010001 * (weights[4*j + 3] >> 8);
    }

  colors[0] = (color1 & 0xff00) << 8 | (color1 & 0xff);
  colors[1] = (color1 & 0xff0000) >> 16;
  colors[2] = (color2 & 0xff00) << 8 | (color2 & 0xff);
  colors[3] = (color2 & 0xff0000) >> 16;

  return _pixops_composite_line_color_22_4a4_mmx (mmx_weights, dest, src[0],
    src[1], x_step, dest_end, x_init, dest_x, check_shift, colors);
}
#endif /* USE_MMX */

static void
scale_pixel (guchar *dest, int dest_x, int dest_channels, int dest_has_alpha,
	     int src_has_alpha, int check_size, guint32 color1, guint32 color2,
	     guint r, guint g, guint b, guint a)
{
  if (src_has_alpha)
    {
      if (a)
	{
	  dest[0] = r / a;
	  dest[1] = g / a;
	  dest[2] = b / a;
	  dest[3] = a >> 16;
	}
      else
	{
	  dest[0] = 0;
	  dest[1] = 0;
	  dest[2] = 0;
	  dest[3] = 0;
	}
    }
  else
    {
      dest[0] = (r + 0xffffff) >> 24;
      dest[1] = (g + 0xffffff) >> 24;
      dest[2] = (b + 0xffffff) >> 24;
      
      if (dest_has_alpha)
	dest[3] = 0xff;
    }
}

static guchar *
scale_line (int *weights, int n_x, int n_y, guchar *dest, int dest_x,
	    guchar *dest_end, int dest_channels, int dest_has_alpha,
	    guchar **src, int src_channels, gboolean src_has_alpha, int x_init,
	    int x_step, int src_width, int check_size, guint32 color1,
	    guint32 color2)
{
  int x = x_init;
  int i, j;

  while (dest < dest_end)
    {
      int x_scaled = x >> SCALE_SHIFT;
      int *pixel_weights;

      pixel_weights = weights +
        ((x >> (SCALE_SHIFT - SUBSAMPLE_BITS)) & SUBSAMPLE_MASK) * n_x * n_y;

      if (src_has_alpha)
	{
	  unsigned int r = 0, g = 0, b = 0, a = 0;
	  for (i=0; i<n_y; i++)
	    {
	      guchar *q = src[i] + x_scaled * src_channels;
	      int *line_weights  = pixel_weights + n_x * i;
	      
	      for (j=0; j<n_x; j++)
		{
		  unsigned int ta;
		  
		  ta = q[3] * line_weights[j];
		  r += ta * q[0];
		  g += ta * q[1];
		  b += ta * q[2];
		  a += ta;
		  
		  q += src_channels;
		}
	    }

	  if (a)
	    {
	      dest[0] = r / a;
	      dest[1] = g / a;
	      dest[2] = b / a;
	      dest[3] = a >> 16;
	    }
	  else
	    {
	      dest[0] = 0;
	      dest[1] = 0;
	      dest[2] = 0;
	      dest[3] = 0;
	    }
	}
      else
	{
	  unsigned int r = 0, g = 0, b = 0;
	  for (i=0; i<n_y; i++)
	    {
	      guchar *q = src[i] + x_scaled * src_channels;
	      int *line_weights  = pixel_weights + n_x * i;
	      
	      for (j=0; j<n_x; j++)
		{
		  unsigned int ta = line_weights[j];
		  
		  r += ta * q[0];
		  g += ta * q[1];
		  b += ta * q[2];

		  q += src_channels;
		}
	    }

	  dest[0] = (r + 0xffff) >> 16;
	  dest[1] = (g + 0xffff) >> 16;
	  dest[2] = (b + 0xffff) >> 16;
	  
	  if (dest_has_alpha)
	    dest[3] = 0xff;
	}

      dest += dest_channels;
      
      x += x_step;
    }

  return dest;
}

#ifdef USE_MMX 
static guchar *
scale_line_22_33_mmx_stub (int *weights, int n_x, int n_y, guchar *dest,
			   int dest_x, guchar *dest_end, int dest_channels,
			   int dest_has_alpha, guchar **src, int src_channels,
			   gboolean src_has_alpha, int x_init, int x_step,
			   int src_width, int check_size, guint32 color1,
			   guint32 color2)
{
  guint32 mmx_weights[16][8];
  int j;

  for (j=0; j<16; j++)
    {
      mmx_weights[j][0] = 0x00010001 * (weights[4*j] >> 8);
      mmx_weights[j][1] = 0x00010001 * (weights[4*j] >> 8);
      mmx_weights[j][2] = 0x00010001 * (weights[4*j + 1] >> 8);
      mmx_weights[j][3] = 0x00010001 * (weights[4*j + 1] >> 8);
      mmx_weights[j][4] = 0x00010001 * (weights[4*j + 2] >> 8);
      mmx_weights[j][5] = 0x00010001 * (weights[4*j + 2] >> 8);
      mmx_weights[j][6] = 0x00010001 * (weights[4*j + 3] >> 8);
      mmx_weights[j][7] = 0x00010001 * (weights[4*j + 3] >> 8);
    }

  return _pixops_scale_line_22_33_mmx (mmx_weights, dest, src[0], src[1],
				       x_step, dest_end, x_init);
}
#endif /* USE_MMX */

static guchar *
scale_line_22_33 (int *weights, int n_x, int n_y, guchar *dest, int dest_x,
		  guchar *dest_end, int dest_channels, int dest_has_alpha,
		  guchar **src, int src_channels, gboolean src_has_alpha,
		  int x_init, int x_step, int src_width,
		  int check_size, guint32 color1, guint32 color2)
{
  int x = x_init;
  guchar *src0 = src[0];
  guchar *src1 = src[1];
  
  while (dest < dest_end)
    {
      unsigned int r, g, b;
      int x_scaled = x >> SCALE_SHIFT;
      int *pixel_weights;
      guchar *q0, *q1;
      int w1, w2, w3, w4;

      q0 = src0 + x_scaled * 3;
      q1 = src1 + x_scaled * 3;
      
      pixel_weights = weights +
        ((x >> (SCALE_SHIFT - SUBSAMPLE_BITS)) & SUBSAMPLE_MASK) * 4;

      w1 = pixel_weights[0];
      w2 = pixel_weights[1];
      w3 = pixel_weights[2];
      w4 = pixel_weights[3];

      r = w1 * q0[0];
      g = w1 * q0[1];
      b = w1 * q0[2];

      r += w2 * q0[3];
      g += w2 * q0[4];
      b += w2 * q0[5];

      r += w3 * q1[0];
      g += w3 * q1[1];
      b += w3 * q1[2];

      r += w4 * q1[3];
      g += w4 * q1[4];
      b += w4 * q1[5];

      dest[0] = (r + 0x8000) >> 16;
      dest[1] = (g + 0x8000) >> 16;
      dest[2] = (b + 0x8000) >> 16;
      
      dest += 3;
      x += x_step;
    }
  
  return dest;
}

static void
process_pixel (int *weights, int n_x, int n_y, guchar *dest, int dest_x,
	       int dest_channels, int dest_has_alpha, guchar **src,
	       int src_channels, gboolean src_has_alpha, int x_start,
	       int src_width, int check_size, guint32 color1, guint32 color2,
	       PixopsPixelFunc pixel_func)
{
  unsigned int r = 0, g = 0, b = 0, a = 0;
  int i, j;
  
  for (i=0; i<n_y; i++)
    {
      int *line_weights  = weights + n_x * i;

      for (j=0; j<n_x; j++)
	{
	  unsigned int ta;
	  guchar *q;

	  if (x_start + j < 0)
	    q = src[i];
	  else if (x_start + j < src_width)
	    q = src[i] + (x_start + j) * src_channels;
	  else
	    q = src[i] + (src_width - 1) * src_channels;

	  if (src_has_alpha)
	    ta = q[3] * line_weights[j];
	  else
	    ta = 0xff * line_weights[j];

	  r += ta * q[0];
	  g += ta * q[1];
	  b += ta * q[2];
	  a += ta;
	}
    }

  (*pixel_func) (dest, dest_x, dest_channels, dest_has_alpha, src_has_alpha,
    check_size, color1, color2, r, g, b, a);
}

static void 
correct_total (int    *weights, 
               int    n_x, 
               int    n_y,
               int    total, 
               double overall_alpha)
{
  int correction = (int)(0.5 + 65536 * overall_alpha) - total;
  int remaining, c, d, i;
  
  if (correction != 0)
    {
      remaining = correction;
      for (d = 1, c = correction; c != 0 && remaining != 0; d++, c = correction / d) 
	for (i = n_x * n_y - 1; i >= 0 && c != 0 && remaining != 0; i--) 
	  if (*(weights + i) + c >= 0) 
	    {
	      *(weights + i) += c;
	      remaining -= c;
	      if ((0 < remaining && remaining < c) ||
		  (0 > remaining && remaining > c))
		c = remaining;
	    }
    }
}

static int *
make_filter_table (PixopsFilter *filter)
{
  int i_offset, j_offset;
  int n_x = filter->x.n;
  int n_y = filter->y.n;
  int *weights = g_new (int, SUBSAMPLE * SUBSAMPLE * n_x * n_y);

  for (i_offset=0; i_offset < SUBSAMPLE; i_offset++)
    for (j_offset=0; j_offset < SUBSAMPLE; j_offset++)
      {
        double weight;
        int *pixel_weights = weights + ((i_offset*SUBSAMPLE) + j_offset) * n_x * n_y;
        int total = 0;
        int i, j;

        for (i=0; i < n_y; i++)
          for (j=0; j < n_x; j++)
            {
              weight = filter->x.weights[(j_offset * n_x) + j] *
                       filter->y.weights[(i_offset * n_y) + i] *
                       filter->overall_alpha * 65536 + 0.5;

              total += (int)weight;

              *(pixel_weights + n_x * i + j) = weight;
            }

        correct_total (pixel_weights, n_x, n_y, total, filter->overall_alpha);
      }

  return weights;
}

static void
pixops_process (guchar         *dest_buf,
		int             render_x0,
		int             render_y0,
		int             render_x1,
		int             render_y1,
		int             dest_rowstride,
		int             dest_channels,
		gboolean        dest_has_alpha,
		const guchar   *src_buf,
		int             src_width,
		int             src_height,
		int             src_rowstride,
		int             src_channels,
		gboolean        src_has_alpha,
		double          scale_x,
		double          scale_y,
		int             check_x,
		int             check_y,
		int             check_size,
		guint32         color1,
		guint32         color2,
		PixopsFilter   *filter,
		PixopsLineFunc  line_func,
		PixopsPixelFunc pixel_func)
{
  int i, j;
  int x, y;			/* X and Y position in source (fixed_point) */

  guchar **line_bufs;
  int *filter_weights;

  int x_step;
  int y_step;

  int check_shift;
  int scaled_x_offset;

  int run_end_x;
  int run_end_index;

  x_step = (1 << SCALE_SHIFT) / scale_x; /* X step in source (fixed point) */
  y_step = (1 << SCALE_SHIFT) / scale_y; /* Y step in source (fixed point) */

  if (x_step == 0 || y_step == 0)
    return; /* overflow, bail out */

  line_bufs = g_new (guchar *, filter->y.n);
  filter_weights = make_filter_table (filter);

  check_shift = check_size ? get_check_shift (check_size) : 0;

  scaled_x_offset = floor (filter->x.offset * (1 << SCALE_SHIFT));

  /* Compute the index where we run off the end of the source buffer. The
   * furthest source pixel we access at index i is:
   *
   *  ((render_x0 + i) * x_step + scaled_x_offset) >> SCALE_SHIFT + filter->x.n - 1
   *
   * So, run_end_index is the smallest i for which this pixel is src_width,
   * i.e, for which:
   *
   *  (i + render_x0) * x_step >= ((src_width - filter->x.n + 1) << SCALE_SHIFT) - scaled_x_offset
   *
   */
#define MYDIV(a,b) ((a) > 0 ? (a) / (b) : ((a) - (b) + 1) / (b))    /* Division so that -1/5 = -1 */

  run_end_x = (((src_width - filter->x.n + 1) << SCALE_SHIFT) - scaled_x_offset);
  run_end_index = MYDIV (run_end_x + x_step - 1, x_step) - render_x0;
  run_end_index = MIN (run_end_index, render_x1 - render_x0);

  y = render_y0 * y_step + floor (filter->y.offset * (1 << SCALE_SHIFT));
  for (i = 0; i < (render_y1 - render_y0); i++)
    {
      int dest_x;
      int y_start = y >> SCALE_SHIFT;
      int x_start;
      int *run_weights = filter_weights +
                         ((y >> (SCALE_SHIFT - SUBSAMPLE_BITS)) & SUBSAMPLE_MASK) *
                         filter->x.n * filter->y.n * SUBSAMPLE;
      guchar *new_outbuf;
      guint32 tcolor1, tcolor2;

      guchar *outbuf = dest_buf + dest_rowstride * i;
      guchar *outbuf_end = outbuf + dest_channels * (render_x1 - render_x0);

      if (((i + check_y) >> check_shift) & 1)
	{
	  tcolor1 = color2;
	  tcolor2 = color1;
	}
      else
	{
	  tcolor1 = color1;
	  tcolor2 = color2;
	}

      for (j=0; j<filter->y.n; j++)
	{
	  if (y_start <  0)
	    line_bufs[j] = (guchar *)src_buf;
	  else if (y_start < src_height)
	    line_bufs[j] = (guchar *)src_buf + src_rowstride * y_start;
	  else
	    line_bufs[j] = (guchar *)src_buf + src_rowstride * (src_height - 1);

	  y_start++;
	}

      dest_x = check_x;
      x = render_x0 * x_step + scaled_x_offset;
      x_start = x >> SCALE_SHIFT;

      while (x_start < 0 && outbuf < outbuf_end)
	{
	  process_pixel (run_weights + ((x >> (SCALE_SHIFT - SUBSAMPLE_BITS)) & SUBSAMPLE_MASK) * (filter->x.n * filter->y.n), filter->x.n, filter->y.n,
			 outbuf, dest_x, dest_channels, dest_has_alpha,
			 line_bufs, src_channels, src_has_alpha,
			 x >> SCALE_SHIFT, src_width,
			 check_size, tcolor1, tcolor2, pixel_func);

	  x += x_step;
	  x_start = x >> SCALE_SHIFT;
	  dest_x++;
	  outbuf += dest_channels;
	}

      new_outbuf = (*line_func) (run_weights, filter->x.n, filter->y.n,
				 outbuf, dest_x, dest_buf + dest_rowstride *
				 i + run_end_index * dest_channels,
				 dest_channels, dest_has_alpha,
				 line_bufs, src_channels, src_has_alpha,
				 x, x_step, src_width, check_size, tcolor1,
				 tcolor2);

      dest_x += (new_outbuf - outbuf) / dest_channels;

      x = (dest_x - check_x + render_x0) * x_step + scaled_x_offset;
      outbuf = new_outbuf;

      while (outbuf < outbuf_end)
	{
	  process_pixel (run_weights + ((x >> (SCALE_SHIFT - SUBSAMPLE_BITS)) & SUBSAMPLE_MASK) * (filter->x.n * filter->y.n), filter->x.n, filter->y.n,
			 outbuf, dest_x, dest_channels, dest_has_alpha,
			 line_bufs, src_channels, src_has_alpha,
			 x >> SCALE_SHIFT, src_width,
			 check_size, tcolor1, tcolor2, pixel_func);

	  x += x_step;
	  dest_x++;
	  outbuf += dest_channels;
	}

      y += y_step;
    }

  g_free (line_bufs);
  g_free (filter_weights);
}

/* Compute weights for reconstruction by replication followed by
 * sampling with a box filter
 */
static void
tile_make_weights (PixopsFilterDimension *dim,
		   double                 scale)
{
  int n = ceil (1 / scale + 1);
  double *pixel_weights = g_new (double, SUBSAMPLE * n);
  int offset;
  int i;

  dim->n = n;
  dim->offset = 0;
  dim->weights = pixel_weights;

  for (offset = 0; offset < SUBSAMPLE; offset++)
    {
      double x = (double)offset / SUBSAMPLE;
      double a = x + 1 / scale;

      for (i = 0; i < n; i++)
        {
          if (i < x)
            {
              if (i + 1 > x)
                *(pixel_weights++)  = (MIN (i + 1, a) - x) * scale;
              else
                *(pixel_weights++) = 0;
            }
          else
            {
              if (a > i)
                *(pixel_weights++)  = (MIN (i + 1, a) - i) * scale;
              else
                *(pixel_weights++) = 0;
            }
       }
    }
}

/* Compute weights for a filter that, for minification
 * is the same as 'tiles', and for magnification, is bilinear
 * reconstruction followed by a sampling with a delta function.
 */
static void
bilinear_magnify_make_weights (PixopsFilterDimension *dim,
			       double                 scale)
{
  double *pixel_weights;
  int n;
  int offset;
  int i;

  if (scale > 1.0)            /* Linear */
    {
      n = 2;
      dim->offset = 0.5 * (1 / scale - 1);
    }
  else                          /* Tile */
    {
      n = ceil (1.0 + 1.0 / scale);
      dim->offset = 0.0;
    }

  dim->n = n;
  dim->weights = g_new (double, SUBSAMPLE * n);

  pixel_weights = dim->weights;

  for (offset=0; offset < SUBSAMPLE; offset++)
    {
      double x = (double)offset / SUBSAMPLE;

      if (scale > 1.0)      /* Linear */
        {
          for (i = 0; i < n; i++)
            *(pixel_weights++) = (((i == 0) ? (1 - x) : x) / scale) * scale;
        }
      else                  /* Tile */
        {
          double a = x + 1 / scale;

          /*           x
           * ---------|--.-|----|--.-|-------  SRC
           * ------------|---------|---------  DEST
           */
          for (i = 0; i < n; i++)
            {
              if (i < x)
                {
                  if (i + 1 > x)
                    *(pixel_weights++) = (MIN (i + 1, a) - x) * scale;
                  else
                    *(pixel_weights++) = 0;
                }
              else
                {
                  if (a > i)
                    *(pixel_weights++) = (MIN (i + 1, a) - i) * scale;
                  else
                    *(pixel_weights++) = 0;
                }
            }
        }
    }
}

/* Computes the integral from b0 to b1 of
 *
 * f(x) = x; 0 <= x < 1
 * f(x) = 0; otherwise
 *
 * We combine two of these to compute the convolution of
 * a box filter with a triangular spike.
 */
static double
linear_box_half (double b0, double b1)
{
  double a0, a1;
  double x0, x1;

  a0 = 0.;
  a1 = 1.;

  if (a0 < b0)
    {
      if (a1 > b0)
        {
          x0 = b0;
          x1 = MIN (a1, b1);
        }
      else
        return 0;
    }
  else
    {
      if (b1 > a0)
        {
          x0 = a0;
          x1 = MIN (a1, b1);
        }
      else
        return 0;
    }

  return 0.5 * (x1*x1 - x0*x0);
}

/* Compute weights for reconstructing with bilinear
 * interpolation, then sampling with a box filter
 */
static void
bilinear_box_make_weights (PixopsFilterDimension *dim,
			   double                 scale)
{
  int n = ceil (1/scale + 3.0);
  double *pixel_weights = g_new (double, SUBSAMPLE * n);
  double w;
  int offset, i;

  dim->offset = -1.0;
  dim->n = n;
  dim->weights = pixel_weights;

  for (offset = 0; offset < SUBSAMPLE; offset++)
    {
      double x = (double)offset / SUBSAMPLE;
      double a = x + 1 / scale;

      for (i = 0; i < n; i++)
        {
          w  = linear_box_half (0.5 + i - a, 0.5 + i - x);
          w += linear_box_half (1.5 + x - i, 1.5 + a - i);
      
          *(pixel_weights++) = w * scale;
        }
    }
}

static void
make_weights (PixopsFilter     *filter,
	      PixopsInterpType  interp_type,	      
	      double            scale_x,
	      double            scale_y)
{
  switch (interp_type)
    {
    case PIXOPS_INTERP_NEAREST:
      g_assert_not_reached ();
      break;

    case PIXOPS_INTERP_TILES:
      tile_make_weights (&filter->x, scale_x);
      tile_make_weights (&filter->y, scale_y);
      break;
      
    case PIXOPS_INTERP_BILINEAR:
      bilinear_magnify_make_weights (&filter->x, scale_x);
      bilinear_magnify_make_weights (&filter->y, scale_y);
      break;
      
    case PIXOPS_INTERP_HYPER:
      bilinear_box_make_weights (&filter->x, scale_x);
      bilinear_box_make_weights (&filter->y, scale_y);
      break;
    }
}

static void
_pixops_composite_color_real (guchar          *dest_buf,
			      int              render_x0,
			      int              render_y0,
			      int              render_x1,
			      int              render_y1,
			      int              dest_rowstride,
			      int              dest_channels,
			      gboolean         dest_has_alpha,
			      const guchar    *src_buf,
			      int              src_width,
			      int              src_height,
			      int              src_rowstride,
			      int              src_channels,
			      gboolean         src_has_alpha,
			      double           scale_x,
			      double           scale_y,
			      PixopsInterpType interp_type,
			      int              overall_alpha,
			      int              check_x,
			      int              check_y,
			      int              check_size,
			      guint32          color1,
			      guint32          color2)
{
  PixopsFilter filter;
  PixopsLineFunc line_func;
  
#ifdef USE_MMX
  gboolean found_mmx = _pixops_have_mmx ();
#endif

  g_return_if_fail (!(dest_channels == 3 && dest_has_alpha));
  g_return_if_fail (!(src_channels == 3 && src_has_alpha));

  if (scale_x == 0 || scale_y == 0)
    return;

  if (interp_type == PIXOPS_INTERP_NEAREST)
    {
      pixops_composite_color_nearest (dest_buf, render_x0, render_y0,
				      render_x1, render_y1, dest_rowstride,
				      dest_channels, dest_has_alpha, src_buf,
				      src_width, src_height, src_rowstride,
				      src_channels, src_has_alpha, scale_x,
				      scale_y, overall_alpha, check_x, check_y,
				      check_size, color1, color2);
      return;
    }
  
  filter.overall_alpha = overall_alpha / 255.;
  make_weights (&filter, interp_type, scale_x, scale_y);

#ifdef USE_MMX
  if (filter.x.n == 2 && filter.y.n == 2 &&
      dest_channels == 4 && src_channels == 4 &&
      src_has_alpha && !dest_has_alpha && found_mmx)
    line_func = composite_line_color_22_4a4_mmx_stub;
  else
#endif
    line_func = composite_line_color;
  
  pixops_process (dest_buf, render_x0, render_y0, render_x1, render_y1,
		  dest_rowstride, dest_channels, dest_has_alpha,
		  src_buf, src_width, src_height, src_rowstride, src_channels,
		  src_has_alpha, scale_x, scale_y, check_x, check_y, check_size, color1, color2,
		  &filter, line_func, composite_pixel_color);

  g_free (filter.x.weights);
  g_free (filter.y.weights);
}

void
_pixops_composite_color (guchar          *dest_buf,
			 int              dest_width,
			 int              dest_height,
			 int              dest_rowstride,
			 int              dest_channels,
			 gboolean         dest_has_alpha,
			 const guchar    *src_buf,
			 int              src_width,
			 int              src_height,
			 int              src_rowstride,
			 int              src_channels,
			 gboolean         src_has_alpha,
			 int              dest_x,
			 int              dest_y,
			 int              dest_region_width,
			 int              dest_region_height,
			 double           offset_x,
			 double           offset_y,
			 double           scale_x,
			 double           scale_y,
			 PixopsInterpType interp_type,
			 int              overall_alpha,
			 int              check_x,
			 int              check_y,
			 int              check_size,
			 guint32          color1,
			 guint32          color2)
{
  guchar *new_dest_buf;
  int render_x0;
  int render_y0;
  int render_x1;
  int render_y1;

  if (!src_has_alpha && overall_alpha == 255)
    {
      _pixops_scale (dest_buf, dest_width, dest_height, dest_rowstride,
		     dest_channels, dest_has_alpha, src_buf, src_width,
		     src_height, src_rowstride, src_channels, src_has_alpha,
		     dest_x, dest_y, dest_region_width, dest_region_height,
		     offset_x, offset_y, scale_x, scale_y, interp_type);
      return;
    }

  new_dest_buf = dest_buf + dest_y * dest_rowstride + dest_x *
                 dest_channels;
  render_x0 = dest_x - offset_x;
  render_y0 = dest_y - offset_y;
  render_x1 = dest_x + dest_region_width  - offset_x;
  render_y1 = dest_y + dest_region_height - offset_y;

  _pixops_composite_color_real (new_dest_buf, render_x0, render_y0, render_x1,
				render_y1, dest_rowstride, dest_channels,
				dest_has_alpha, src_buf, src_width,
				src_height, src_rowstride, src_channels,
				src_has_alpha, scale_x, scale_y,
				(PixopsInterpType)interp_type, overall_alpha,
				check_x, check_y, check_size, color1, color2);
}

/**
 * _pixops_composite_real:
 * @dest_buf: pointer to location to store result
 * @render_x0: x0 of region of scaled source to store into @dest_buf
 * @render_y0: y0 of region of scaled source to store into @dest_buf
 * @render_x1: x1 of region of scaled source to store into @dest_buf
 * @render_y1: y1 of region of scaled source to store into @dest_buf
 * @dest_rowstride: rowstride of @dest_buf
 * @dest_channels: number of channels in @dest_buf
 * @dest_has_alpha: whether @dest_buf has alpha
 * @src_buf: pointer to source pixels
 * @src_width: width of source (used for clipping)
 * @src_height: height of source (used for clipping)
 * @src_rowstride: rowstride of source
 * @src_channels: number of channels in @src_buf
 * @src_has_alpha: whether @src_buf has alpha
 * @scale_x: amount to scale source by in X direction
 * @scale_y: amount to scale source by in Y direction
 * @interp_type: type of enumeration
 * @overall_alpha: overall alpha factor to multiply source by
 * 
 * Scale source buffer by scale_x / scale_y, then composite a given rectangle
 * of the result into the destination buffer.
 **/
static void
_pixops_composite_real (guchar          *dest_buf,
			int              render_x0,
			int              render_y0,
			int              render_x1,
			int              render_y1,
			int              dest_rowstride,
			int              dest_channels,
			gboolean         dest_has_alpha,
			const guchar    *src_buf,
			int              src_width,
			int              src_height,
			int              src_rowstride,
			int              src_channels,
			gboolean         src_has_alpha,
			double           scale_x,
			double           scale_y,
			PixopsInterpType interp_type,
			int              overall_alpha)
{
  PixopsFilter filter;
  PixopsLineFunc line_func;
  
#ifdef USE_MMX
  gboolean found_mmx = _pixops_have_mmx ();
#endif

  g_return_if_fail (!(dest_channels == 3 && dest_has_alpha));
  g_return_if_fail (!(src_channels == 3 && src_has_alpha));

  if (scale_x == 0 || scale_y == 0)
    return;

  if (interp_type == PIXOPS_INTERP_NEAREST)
    {
      pixops_composite_nearest (dest_buf, render_x0, render_y0, render_x1,
				render_y1, dest_rowstride, dest_channels,
				dest_has_alpha, src_buf, src_width, src_height,
				src_rowstride, src_channels, src_has_alpha,
				scale_x, scale_y, overall_alpha);
      return;
    }
  
  filter.overall_alpha = overall_alpha / 255.;
  make_weights (&filter, interp_type, scale_x, scale_y);

  if (filter.x.n == 2 && filter.y.n == 2 && dest_channels == 4 &&
      src_channels == 4 && src_has_alpha && !dest_has_alpha)
    {
#ifdef USE_MMX
      if (found_mmx)
	line_func = composite_line_22_4a4_mmx_stub;
      else
#endif	
	line_func = composite_line_22_4a4;
    }
  else
    line_func = composite_line;
  
  pixops_process (dest_buf, render_x0, render_y0, render_x1, render_y1,
		  dest_rowstride, dest_channels, dest_has_alpha,
		  src_buf, src_width, src_height, src_rowstride, src_channels,
		  src_has_alpha, scale_x, scale_y, 0, 0, 0, 0, 0, 
		  &filter, line_func, composite_pixel);

  g_free (filter.x.weights);
  g_free (filter.y.weights);
}

void
_pixops_composite (guchar          *dest_buf,
                   int              dest_width,
                   int              dest_height,
                   int              dest_rowstride,
                   int              dest_channels,
                   int              dest_has_alpha,
                   const guchar    *src_buf,
                   int              src_width,
                   int              src_height,
                   int              src_rowstride,
                   int              src_channels,
                   int              src_has_alpha,
                   int              dest_x,
                   int              dest_y,
                   int              dest_region_width,
                   int              dest_region_height,
                   double           offset_x,
                   double           offset_y,
                   double           scale_x,
                   double           scale_y,
                   PixopsInterpType interp_type,
                   int              overall_alpha)
{
  guchar *new_dest_buf;
  int render_x0;
  int render_y0;
  int render_x1;
  int render_y1;

  if (!src_has_alpha && overall_alpha == 255)
    {
      _pixops_scale (dest_buf, dest_width, dest_height, dest_rowstride,
		     dest_channels, dest_has_alpha, src_buf, src_width,
		     src_height, src_rowstride, src_channels, src_has_alpha,
		     dest_x, dest_y, dest_region_width, dest_region_height,
		     offset_x, offset_y, scale_x, scale_y, interp_type);
      return;
    }

#ifdef USE_MEDIALIB
  pixops_medialib_composite (dest_buf, dest_width, dest_height, dest_rowstride,
                             dest_channels, dest_has_alpha, src_buf,
                             src_width, src_height, src_rowstride,
                             src_channels, src_has_alpha, dest_x, dest_y,
                             dest_region_width, dest_region_height, offset_x,
			     offset_y, scale_x, scale_y,
                             (PixopsInterpType)interp_type, overall_alpha);
  return;
#endif

  new_dest_buf = dest_buf + dest_y * dest_rowstride + dest_x * dest_channels;
  render_x0 = dest_x - offset_x;
  render_y0 = dest_y - offset_y;
  render_x1 = dest_x + dest_region_width  - offset_x;
  render_y1 = dest_y + dest_region_height - offset_y;

  _pixops_composite_real (new_dest_buf, render_x0, render_y0, render_x1,
			  render_y1, dest_rowstride, dest_channels,
			  dest_has_alpha, src_buf, src_width, src_height,
			  src_rowstride, src_channels, src_has_alpha, scale_x,
			  scale_y, (PixopsInterpType)interp_type,
			  overall_alpha);
}

#ifdef USE_MEDIALIB
static void
medialib_get_interpolation (mlInterp * ml_interp,
                            PixopsInterpType interp_type,
                            double scale_x,
                            double scale_y,
                            double overall_alpha)
{
  mlib_s32 leftPadding, topPadding;
  ml_interp->interp_table = NULL;

 /*
  * medialib 2.1 and later supports scaling with user-defined interpolation
  * tables, so this logic is used.  
  *
  * bilinear_magnify_make_weights builds an interpolation table of size 2x2 if
  * the scale factor >= 1.0 and "ceil (1.0 + 1.0/scale)" otherwise.  These map
  * most closely to MLIB_BILINEAR, which uses an interpolation table of size
  * 2x2.
  *
  * tile_make_weights builds an interpolation table of size 2x2 if the scale
  * factor >= 1.0 and "ceil (1.0 + 1.0/scale)" otherwise.  These map most
  * closely to MLIB_BILINEAR, which uses an interpolation table of size 2x2.
  *
  * bilinear_box_make_weights builds an interpolation table of size 4x4 if the
  * scale factor >= 1.0 and "ceil (1.0 + 1.0/scale)" otherwise.  These map most
  * closely to MLIB_BICUBIC, which uses an interpolation table of size 4x4.
  *
  * PIXOPS_INTERP_NEAREST calls pixops_scale_nearest which does not use an
  * interpolation table.  This maps to MLIB_NEAREST.
  */
  switch (interp_type)
    {
    case PIXOPS_INTERP_BILINEAR:
      bilinear_magnify_make_weights (&(ml_interp->po_filter.x), scale_x);
      bilinear_magnify_make_weights (&(ml_interp->po_filter.y), scale_y);
      leftPadding = 0;
      topPadding  = 0;

      if (scale_x <= 1.0)
          ml_interp->tx = 0.5 * (1 - scale_x);
      else
          ml_interp->tx = 0.0;

      if (scale_y <= 1.0)
          ml_interp->ty = 0.5 * (1 - scale_y);
      else
          ml_interp->ty = 0.0;

      break;

    case PIXOPS_INTERP_TILES:
      tile_make_weights (&(ml_interp->po_filter.x), scale_x);
      tile_make_weights (&(ml_interp->po_filter.y), scale_y);
      leftPadding   = 0;
      topPadding    = 0;
      ml_interp->tx = 0.5 * (1 - scale_x);
      ml_interp->ty = 0.5 * (1 - scale_y);
      break;

    case PIXOPS_INTERP_HYPER:
      bilinear_box_make_weights (&(ml_interp->po_filter.x), scale_x);
      bilinear_box_make_weights (&(ml_interp->po_filter.y), scale_y);
      leftPadding   = 1;
      topPadding    = 1;
      ml_interp->tx = 0.5 * (1 - scale_x);
      ml_interp->ty = 0.5 * (1 - scale_y);
      break;

    case PIXOPS_INTERP_NEAREST:
    default:
      /*
       * Note that this function should not be called in the
       * PIXOPS_INTERP_NEAREST case since it does not use an interpolation
       * table.
       */
      g_assert_not_reached ();
      break;
    }

 /* 
  * If overall_alpha is not 1.0, then multiply the vectors built by the
  * sqrt (overall_alpha).  This will cause overall_alpha to get evenly
  * blended across both axis.
  *
  * Note there is no need to multiply the vectors built by the various
  * make-weight functions by sqrt (overall_alpha) since the make-weight
  * functions are called with overall_alpha hardcoded to 1.0.
  */
  if (overall_alpha != 1.0)
    {
      double sqrt_alpha = sqrt (overall_alpha);
      int i;

      for (i=0; i < SUBSAMPLE * ml_interp->po_filter.x.n; i++)
         ml_interp->po_filter.x.weights[i] *= sqrt_alpha;
      for (i=0; i < SUBSAMPLE * ml_interp->po_filter.y.n; i++)
         ml_interp->po_filter.y.weights[i] *= sqrt_alpha;
    }
    
  ml_interp->interp_table = (void *) mlib_ImageInterpTableCreate (MLIB_DOUBLE,
    ml_interp->po_filter.x.n, ml_interp->po_filter.y.n, leftPadding,
    topPadding, SUBSAMPLE_BITS, SUBSAMPLE_BITS, 8,
    ml_interp->po_filter.x.weights, ml_interp->po_filter.y.weights);

  g_free (ml_interp->po_filter.x.weights);
  g_free (ml_interp->po_filter.y.weights);  
}

static void
pixops_medialib_composite (guchar          *dest_buf,
                           int              dest_width,
                           int              dest_height,
                           int              dest_rowstride,
                           int              dest_channels,
                           int              dest_has_alpha,
                           const guchar    *src_buf,
                           int              src_width,
                           int              src_height,
                           int              src_rowstride,
                           int              src_channels,
                           int              src_has_alpha,
                           int              dest_x,
                           int              dest_y,
                           int              dest_region_width,
                           int              dest_region_height,
                           double           offset_x,
                           double           offset_y,
                           double           scale_x,
                           double           scale_y,
                           PixopsInterpType interp_type,
                           int              overall_alpha)
{
  mlib_blend blend;
  g_return_if_fail (!(dest_channels == 3 && dest_has_alpha));
  g_return_if_fail (!(src_channels == 3 && src_has_alpha));

  if (scale_x == 0 || scale_y == 0)
    return;

  if (!medialib_initialized)
    _pixops_use_medialib ();

  if (!use_medialib)
    {
      /* Use non-mediaLib version */
      _pixops_composite_real (dest_buf + dest_y * dest_rowstride + dest_x *
			      dest_channels, dest_x - offset_x, dest_y -
			      offset_y, dest_x + dest_region_width - offset_x,
			      dest_y + dest_region_height - offset_y,
			      dest_rowstride, dest_channels, dest_has_alpha,
			      src_buf, src_width, src_height, src_rowstride,
			      src_channels, src_has_alpha, scale_x, scale_y,
			      interp_type, overall_alpha);
    }
  else
    {
      mlInterp ml_interp;
      mlib_image img_src, img_dest;
      double ml_offset_x, ml_offset_y;

      if (!src_has_alpha && overall_alpha == 255 &&
	  dest_channels <= src_channels) 
        {
          pixops_medialib_scale (dest_buf, dest_region_width,
				 dest_region_height, dest_rowstride,
				 dest_channels, dest_has_alpha, src_buf,
				 src_width, src_height, src_rowstride,
				 src_channels, src_has_alpha, dest_x, dest_y,
				 dest_region_width, dest_region_height,
				 offset_x, offset_y, scale_x, scale_y,
				 interp_type);
          return;
        }

      mlib_ImageSetStruct (&img_src, MLIB_BYTE, src_channels,
			   src_width, src_height, src_rowstride, src_buf);

      if (dest_x == 0 && dest_y == 0 &&
          dest_width  == dest_region_width &&
          dest_height == dest_region_height)
        {
          mlib_ImageSetStruct (&img_dest, MLIB_BYTE, dest_channels,
			       dest_width, dest_height, dest_rowstride,
			       dest_buf);
        }
      else
        {
	  mlib_u8 *data = dest_buf + (dest_y * dest_rowstride) + 
				     (dest_x * dest_channels);

          mlib_ImageSetStruct (&img_dest, MLIB_BYTE, dest_channels,
			       dest_region_width, dest_region_height,
			       dest_rowstride, data);
        }

      ml_offset_x = floor (offset_x) - dest_x;
      ml_offset_y = floor (offset_y) - dest_y;

      if (interp_type == PIXOPS_INTERP_NEAREST)
        {
          blend = src_has_alpha ? MLIB_BLEND_GTK_SRC_OVER2 : MLIB_BLEND_GTK_SRC;

          mlib_ImageZoomTranslateBlend (&img_dest,
                                        &img_src,
                                        scale_x,
                                        scale_y,
                                        ml_offset_x,
                                        ml_offset_y,
                                        MLIB_NEAREST,
                                        MLIB_EDGE_SRC_EXTEND_INDEF,
                                        blend,
                                        overall_alpha,
                                        1);
        }
      else
        {
          blend = src_has_alpha ? MLIB_BLEND_GTK_SRC_OVER : MLIB_BLEND_GTK_SRC;

          if (interp_type == PIXOPS_INTERP_BILINEAR &&
	      scale_x > 1.0 && scale_y > 1.0)
            {
              mlib_ImageZoomTranslateBlend (&img_dest,
                                            &img_src,
                                            scale_x,
                                            scale_y,
                                            ml_offset_x,
                                            ml_offset_y,
                                            MLIB_BILINEAR,
                                            MLIB_EDGE_SRC_EXTEND_INDEF,
                                            blend,
                                            overall_alpha,
                                            1);
            }
          else
            {
              medialib_get_interpolation (&ml_interp, interp_type, scale_x,
					  scale_y, overall_alpha/255.0);

              if (ml_interp.interp_table != NULL)
                {
                  mlib_ImageZoomTranslateTableBlend (&img_dest,
                                                     &img_src,
                                                     scale_x,
                                                     scale_y,
                                                     ml_offset_x + ml_interp.tx,
                                                     ml_offset_y + ml_interp.ty,
                                                     ml_interp.interp_table,
                                                     MLIB_EDGE_SRC_EXTEND_INDEF,
                                                     blend,
                                                     1);
                  mlib_ImageInterpTableDelete (ml_interp.interp_table);
                }
              else
                {
                  /* Should not happen - Use non-mediaLib version */
                  _pixops_composite_real (dest_buf + dest_y * dest_rowstride +
                                          dest_x * dest_channels,
                                          dest_x - offset_x, dest_y - offset_y,
                                          dest_x + dest_region_width - offset_x,
                                          dest_y + dest_region_height - offset_y,
                                          dest_rowstride, dest_channels,
                                          dest_has_alpha, src_buf, src_width,
                                          src_height, src_rowstride,
                                          src_channels, src_has_alpha, scale_x,
                                          scale_y, interp_type, overall_alpha);
                }
            }
        }
    }
}
#endif

static void
_pixops_scale_real (guchar        *dest_buf,
		    int            render_x0,
		    int            render_y0,
		    int            render_x1,
		    int            render_y1,
		    int            dest_rowstride,
		    int            dest_channels,
		    gboolean       dest_has_alpha,
		    const guchar  *src_buf,
		    int            src_width,
		    int            src_height,
		    int            src_rowstride,
		    int            src_channels,
		    gboolean       src_has_alpha,
		    double         scale_x,
		    double         scale_y,
		    PixopsInterpType  interp_type)
{
  PixopsFilter filter;
  PixopsLineFunc line_func;

#ifdef USE_MMX
  gboolean found_mmx = _pixops_have_mmx ();
#endif

  g_return_if_fail (!(dest_channels == 3 && dest_has_alpha));
  g_return_if_fail (!(src_channels == 3 && src_has_alpha));
  g_return_if_fail (!(src_has_alpha && !dest_has_alpha));

  if (scale_x == 0 || scale_y == 0)
    return;

  if (interp_type == PIXOPS_INTERP_NEAREST)
    {
      pixops_scale_nearest (dest_buf, render_x0, render_y0, render_x1,
			    render_y1, dest_rowstride, dest_channels,
			    dest_has_alpha, src_buf, src_width, src_height,
			    src_rowstride, src_channels, src_has_alpha,
			    scale_x, scale_y);
      return;
    }
  
  filter.overall_alpha = 1.0;
  make_weights (&filter, interp_type, scale_x, scale_y);

  if (filter.x.n == 2 && filter.y.n == 2 && dest_channels == 3 && src_channels == 3)
    {
#ifdef USE_MMX
      if (found_mmx)
	line_func = scale_line_22_33_mmx_stub;
      else
#endif
	line_func = scale_line_22_33;
    }
  else
    line_func = scale_line;
  
  pixops_process (dest_buf, render_x0, render_y0, render_x1, render_y1,
		  dest_rowstride, dest_channels, dest_has_alpha,
		  src_buf, src_width, src_height, src_rowstride, src_channels,
		  src_has_alpha, scale_x, scale_y, 0, 0, 0, 0, 0,
		  &filter, line_func, scale_pixel);

  g_free (filter.x.weights);
  g_free (filter.y.weights);
}

void
_pixops_scale (guchar          *dest_buf,
               int              dest_width,
               int              dest_height,
               int              dest_rowstride,
               int              dest_channels,
               int              dest_has_alpha,
               const guchar    *src_buf,
               int              src_width,
               int              src_height,
               int              src_rowstride,
               int              src_channels,
               int              src_has_alpha,
               int              dest_x,
               int              dest_y,
               int              dest_region_width,
               int              dest_region_height,
               double           offset_x,
               double           offset_y,
               double           scale_x,
               double           scale_y,
               PixopsInterpType interp_type)
{
  guchar *new_dest_buf;
  int render_x0;
  int render_y0;
  int render_x1;
  int render_y1;

#ifdef USE_MEDIALIB
  pixops_medialib_scale (dest_buf, dest_width, dest_height, dest_rowstride,
                         dest_channels, dest_has_alpha, src_buf, src_width,
                         src_height, src_rowstride, src_channels,
                         src_has_alpha, dest_x, dest_y, dest_region_width,
			 dest_region_height, offset_x, offset_y, scale_x,
			 scale_y, (PixopsInterpType)interp_type);
  return;
#endif

  new_dest_buf = dest_buf + dest_y * dest_rowstride + dest_x * dest_channels;
  render_x0    = dest_x - offset_x;
  render_y0    = dest_y - offset_y;
  render_x1    = dest_x + dest_region_width  - offset_x;
  render_y1    = dest_y + dest_region_height - offset_y;

  _pixops_scale_real (new_dest_buf, render_x0, render_y0, render_x1,
                      render_y1, dest_rowstride, dest_channels,
                      dest_has_alpha, src_buf, src_width, src_height,
                      src_rowstride, src_channels, src_has_alpha,
                      scale_x, scale_y, (PixopsInterpType)interp_type);
}

#ifdef USE_MEDIALIB
static void
pixops_medialib_scale     (guchar          *dest_buf,
                           int              dest_width,
                           int              dest_height,
                           int              dest_rowstride,
                           int              dest_channels,
                           int              dest_has_alpha,
                           const guchar    *src_buf,
                           int              src_width,
                           int              src_height,
                           int              src_rowstride,
                           int              src_channels,
                           int              src_has_alpha,
                           int              dest_x,
                           int              dest_y,
                           int              dest_region_width,
                           int              dest_region_height,
                           double           offset_x,
                           double           offset_y,
                           double           scale_x,
                           double           scale_y,
                           PixopsInterpType interp_type)
{
  if (scale_x == 0 || scale_y == 0)
    return;

  if (!medialib_initialized)
    _pixops_use_medialib ();
 
  /*
   * We no longer support mediaLib 2.1 because it has a core dumping problem
   * in the mlib_ImageZoomTranslateTable function that has been corrected in
   * 2.2.  Although the mediaLib_zoom function could be used, it does not
   * work properly if the source and destination images have different 
   * values for "has_alpha" or "num_channels".  The complicated if-logic
   * required to support both versions is not worth supporting
   * mediaLib 2.1 moving forward.
   */
  if (!use_medialib)
    {
      _pixops_scale_real (dest_buf + dest_y * dest_rowstride + dest_x *
			  dest_channels, dest_x - offset_x, dest_y - offset_y, 
			  dest_x + dest_region_width - offset_x,
			  dest_y + dest_region_height - offset_y,
			  dest_rowstride, dest_channels, dest_has_alpha,
			  src_buf, src_width, src_height, src_rowstride,
			  src_channels, src_has_alpha, scale_x, scale_y,
			  interp_type);
    }
  else 
    {
      mlInterp ml_interp;
      mlib_image img_orig_src, img_src, img_dest;
      double ml_offset_x, ml_offset_y;
      guchar *tmp_buf = NULL;

      mlib_ImageSetStruct (&img_orig_src, MLIB_BYTE, src_channels, src_width, 
			   src_height, src_rowstride, src_buf);

      if (dest_x == 0 && dest_y == 0 &&
          dest_width == dest_region_width &&
          dest_height == dest_region_height)
        {
          mlib_ImageSetStruct (&img_dest, MLIB_BYTE, dest_channels,
			       dest_width, dest_height, dest_rowstride,
			       dest_buf);
        }
      else
        {
	  mlib_u8 *data = dest_buf + (dest_y * dest_rowstride) + 
				     (dest_x * dest_channels);

          mlib_ImageSetStruct (&img_dest, MLIB_BYTE, dest_channels,
			       dest_region_width, dest_region_height,
			       dest_rowstride, data);
        }

      ml_offset_x = floor (offset_x) - dest_x;
      ml_offset_y = floor (offset_y) - dest_y;

     /*
      * Note that zoomTranslate and zoomTranslateTable are faster
      * than zoomTranslateBlend and zoomTranslateTableBlend.  However
      * the faster functions only work in the following case:
      *
      *   if (src_channels == dest_channels &&
      *       (!src_alpha && interp_table != PIXOPS_INTERP_NEAREST))
      *
      * We use the faster versions if we can.
      *
      * Note when the interp_type is BILINEAR and the interpolation
      * table will be size 2x2 (when both x/y scale factors > 1.0),
      * then we do not bother building the interpolation table.   In
      * this case we can just use MLIB_BILINEAR, which is faster than
      * using a specified interpolation table.
      */
      img_src = img_orig_src;

      if (!src_has_alpha)
        {
          if (src_channels > dest_channels)
            {
              int channels  = 3;
              int rowstride = (channels * src_width + 3) & ~3;
        
              tmp_buf = g_malloc (src_rowstride * src_height);

              if (src_buf != NULL)
                {
                  src_channels  = channels;
                  src_rowstride = rowstride;
          
                  mlib_ImageSetStruct (&img_src, MLIB_BYTE, src_channels,
				       src_width, src_height, src_rowstride,
				       tmp_buf);
                  mlib_ImageChannelExtract (&img_src, &img_orig_src, 0xE);  
                }
            }
        }
    
      if (interp_type == PIXOPS_INTERP_NEAREST)
        {
          if (src_channels == dest_channels)
            {
              mlib_ImageZoomTranslate (&img_dest,
                                       &img_src,
                                       scale_x,
                                       scale_y,
                                       ml_offset_x,
                                       ml_offset_y,
                                       MLIB_NEAREST,
                                       MLIB_EDGE_SRC_EXTEND_INDEF);
            }
          else
            {
              mlib_ImageZoomTranslateBlend (&img_dest,
                                            &img_src,
                                            scale_x,
                                            scale_y,
                                            ml_offset_x,
                                            ml_offset_y,
                                            MLIB_NEAREST,
                                            MLIB_EDGE_SRC_EXTEND_INDEF,
                                            MLIB_BLEND_GTK_SRC,
                                            1.0,
                                            1);
            }
        }
      else if (src_channels == dest_channels && !src_has_alpha)
        {
          if (interp_type == PIXOPS_INTERP_BILINEAR &&
              scale_x > 1.0 && scale_y > 1.0)
            {
               mlib_ImageZoomTranslate (&img_dest,
                                        &img_src,
                                        scale_x,
                                        scale_y,
                                        ml_offset_x,
                                        ml_offset_y,
                                        MLIB_BILINEAR,
                                        MLIB_EDGE_SRC_EXTEND_INDEF);
            }
          else
            {
              medialib_get_interpolation (&ml_interp, interp_type,
                                          scale_x, scale_y, 1.0);

              if (ml_interp.interp_table != NULL)
                {
                  mlib_ImageZoomTranslateTable (&img_dest, 
                                                &img_src,
                                                scale_x,
                                                scale_y,
                                                ml_offset_x + ml_interp.tx,
                                                ml_offset_y + ml_interp.ty,
                                                ml_interp.interp_table,
                                                MLIB_EDGE_SRC_EXTEND_INDEF);

	          mlib_ImageInterpTableDelete (ml_interp.interp_table);
                }
              else
                {
                  /* Should not happen. */
                  mlib_filter  ml_filter;

                  switch (interp_type)
                    {
                    case PIXOPS_INTERP_BILINEAR:
                      ml_filter = MLIB_BILINEAR;
                      break;

                    case PIXOPS_INTERP_TILES:
                      ml_filter = MLIB_BILINEAR;
                      break;

                    case PIXOPS_INTERP_HYPER:
                      ml_filter = MLIB_BICUBIC;
                      break;
                    }

                  mlib_ImageZoomTranslate (&img_dest,
                                           &img_src,
                                           scale_x,
                                           scale_y,
                                           ml_offset_x,
                                           ml_offset_y,
                                           ml_filter,
                                           MLIB_EDGE_SRC_EXTEND_INDEF);
                }
            }
        }

      /* Deal with case where src_channels != dest_channels || src_has_alpha */
      else if (interp_type == PIXOPS_INTERP_BILINEAR &&
               scale_x > 1.0 && scale_y > 1.0)
        {
          mlib_ImageZoomTranslateBlend (&img_dest,
                                        &img_src,
                                        scale_x,
                                        scale_y,
                                        ml_offset_x,
                                        ml_offset_y,
                                        MLIB_BILINEAR,
                                        MLIB_EDGE_SRC_EXTEND_INDEF,
                                        MLIB_BLEND_GTK_SRC,
                                        1.0,
                                        1);
        }
      else
        {
          medialib_get_interpolation (&ml_interp, interp_type,
                                      scale_x, scale_y, 1.0);

          if (ml_interp.interp_table != NULL)
            {
              mlib_ImageZoomTranslateTableBlend (&img_dest,
                                                 &img_src,
                                                 scale_x,
                                                 scale_y,
                                                 ml_offset_x + ml_interp.tx,
                                                 ml_offset_y + ml_interp.ty,
                                                 ml_interp.interp_table,
                                                 MLIB_EDGE_SRC_EXTEND_INDEF,
                                                 MLIB_BLEND_GTK_SRC,
                                                 1);
              mlib_ImageInterpTableDelete (ml_interp.interp_table);
            }
          else
            {
              mlib_filter  ml_filter;

              switch (interp_type)
                {
                case PIXOPS_INTERP_BILINEAR:
                  ml_filter = MLIB_BILINEAR;
                  break;
            
                case PIXOPS_INTERP_TILES:
                  ml_filter = MLIB_BILINEAR;
                  break;

                case PIXOPS_INTERP_HYPER:
                  ml_filter = MLIB_BICUBIC;
                  break;
                }

              mlib_ImageZoomTranslate (&img_dest,
                                       &img_src,
                                       scale_x,
                                       scale_y,
                                       ml_offset_x,
                                       ml_offset_y,
                                       ml_filter,
                                       MLIB_EDGE_SRC_EXTEND_INDEF);
            }
        }

      if (tmp_buf != NULL)
        g_free (tmp_buf);
    }
}
#endif
