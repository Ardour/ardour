/* GDK - The GIMP Drawing Kit
 * Copyright (C) 1995-1997 Peter Mattis, Spencer Kimball and Josh MacDonald
 * Copyright (C) 1998-2004 Tor Lillqvist
 * Copyright (C) 2001-2005 Hans Breuer
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
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

/*
 * Modified by the GTK+ Team and others 1997-2000.  See the AUTHORS
 * file for a list of people on the GTK+ Team.  See the ChangeLog
 * files for a list of changes.  These files are distributed with
 * GTK+ at ftp://ftp.gtk.org/pub/gtk/. 
 */

#include "config.h"
#include <math.h>
#include <stdio.h>
#include <glib.h>

#include <pango/pangowin32.h>
#include <cairo-win32.h>

#include "gdkscreen.h" /* gdk_screen_get_default() */
#include "gdkregion-generic.h"
#include "gdkprivate-win32.h"

#define ROP3_D 0x00AA0029
#define ROP3_DSna 0x00220326
#define ROP3_DSPDxax 0x00E20746

#define LINE_ATTRIBUTES (GDK_GC_LINE_WIDTH|GDK_GC_LINE_STYLE| \
			 GDK_GC_CAP_STYLE|GDK_GC_JOIN_STYLE)

#define MUST_RENDER_DASHES_MANUALLY(gcwin32)			\
  (gcwin32->line_style == GDK_LINE_DOUBLE_DASH ||		\
   (gcwin32->line_style == GDK_LINE_ON_OFF_DASH && gcwin32->pen_dash_offset))

static void gdk_win32_draw_rectangle (GdkDrawable    *drawable,
				      GdkGC          *gc,
				      gboolean        filled,
				      gint            x,
				      gint            y,
				      gint            width,
				      gint            height);
static void gdk_win32_draw_arc       (GdkDrawable    *drawable,
				      GdkGC          *gc,
				      gboolean        filled,
				      gint            x,
				      gint            y,
				      gint            width,
				      gint            height,
				      gint            angle1,
				      gint            angle2);
static void gdk_win32_draw_polygon   (GdkDrawable    *drawable,
				      GdkGC          *gc,
				      gboolean        filled,
				      GdkPoint       *points,
				      gint            npoints);
static void gdk_win32_draw_text      (GdkDrawable    *drawable,
				      GdkFont        *font,
				      GdkGC          *gc,
				      gint            x,
				      gint            y,
				      const gchar    *text,
				      gint            text_length);
static void gdk_win32_draw_text_wc   (GdkDrawable    *drawable,
				      GdkFont        *font,
				      GdkGC          *gc,
				      gint            x,
				      gint            y,
				      const GdkWChar *text,
				      gint            text_length);
static void gdk_win32_draw_drawable  (GdkDrawable    *drawable,
				      GdkGC          *gc,
				      GdkPixmap      *src,
				      gint            xsrc,
				      gint            ysrc,
				      gint            xdest,
				      gint            ydest,
				      gint            width,
				      gint            height,
				      GdkDrawable    *original_src);
static void gdk_win32_draw_points    (GdkDrawable    *drawable,
				      GdkGC          *gc,
				      GdkPoint       *points,
				      gint            npoints);
static void gdk_win32_draw_segments  (GdkDrawable    *drawable,
				      GdkGC          *gc,
				      GdkSegment     *segs,
				      gint            nsegs);
static void gdk_win32_draw_lines     (GdkDrawable    *drawable,
				      GdkGC          *gc,
				      GdkPoint       *points,
				      gint            npoints);
static void gdk_win32_draw_image     (GdkDrawable     *drawable,
				      GdkGC           *gc,
				      GdkImage        *image,
				      gint             xsrc,
				      gint             ysrc,
				      gint             xdest,
				      gint             ydest,
				      gint             width,
				      gint             height);
static void gdk_win32_draw_pixbuf     (GdkDrawable     *drawable,
				      GdkGC           *gc,
				      GdkPixbuf       *pixbuf,
				      gint             src_x,
				      gint             src_y,
				      gint             dest_x,
				      gint             dest_y,
				      gint             width,
				      gint             height,
				      GdkRgbDither     dither,
				      gint             x_dither,
				      gint             y_dither);

static cairo_surface_t *gdk_win32_ref_cairo_surface (GdkDrawable *drawable);
     
static void gdk_win32_set_colormap   (GdkDrawable    *drawable,
				      GdkColormap    *colormap);

static GdkColormap* gdk_win32_get_colormap   (GdkDrawable    *drawable);

static gint         gdk_win32_get_depth      (GdkDrawable    *drawable);

static GdkScreen *  gdk_win32_get_screen     (GdkDrawable    *drawable);

static GdkVisual*   gdk_win32_get_visual     (GdkDrawable    *drawable);

static void gdk_drawable_impl_win32_finalize   (GObject *object);

static const cairo_user_data_key_t gdk_win32_cairo_key;
static const cairo_user_data_key_t gdk_win32_cairo_hdc_key;

G_DEFINE_TYPE (GdkDrawableImplWin32,  _gdk_drawable_impl_win32, GDK_TYPE_DRAWABLE)


static void
_gdk_drawable_impl_win32_class_init (GdkDrawableImplWin32Class *klass)
{
  GdkDrawableClass *drawable_class = GDK_DRAWABLE_CLASS (klass);
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = gdk_drawable_impl_win32_finalize;

  drawable_class->create_gc = _gdk_win32_gc_new;
  drawable_class->draw_rectangle = gdk_win32_draw_rectangle;
  drawable_class->draw_arc = gdk_win32_draw_arc;
  drawable_class->draw_polygon = gdk_win32_draw_polygon;
  drawable_class->draw_text = gdk_win32_draw_text;
  drawable_class->draw_text_wc = gdk_win32_draw_text_wc;
  drawable_class->draw_drawable_with_src = gdk_win32_draw_drawable;
  drawable_class->draw_points = gdk_win32_draw_points;
  drawable_class->draw_segments = gdk_win32_draw_segments;
  drawable_class->draw_lines = gdk_win32_draw_lines;
  drawable_class->draw_image = gdk_win32_draw_image;
  drawable_class->draw_pixbuf = gdk_win32_draw_pixbuf;
  
  drawable_class->ref_cairo_surface = gdk_win32_ref_cairo_surface;
  
  drawable_class->set_colormap = gdk_win32_set_colormap;
  drawable_class->get_colormap = gdk_win32_get_colormap;

  drawable_class->get_depth = gdk_win32_get_depth;
  drawable_class->get_screen = gdk_win32_get_screen;
  drawable_class->get_visual = gdk_win32_get_visual;

  drawable_class->_copy_to_image = _gdk_win32_copy_to_image;
}

static void
_gdk_drawable_impl_win32_init (GdkDrawableImplWin32 *impl)
{
}

static void
gdk_drawable_impl_win32_finalize (GObject *object)
{
  gdk_drawable_set_colormap (GDK_DRAWABLE (object), NULL);

  G_OBJECT_CLASS (_gdk_drawable_impl_win32_parent_class)->finalize (object);
}

/*****************************************************
 * Win32 specific implementations of generic functions *
 *****************************************************/

static GdkColormap*
gdk_win32_get_colormap (GdkDrawable *drawable)
{
  return GDK_DRAWABLE_IMPL_WIN32 (drawable)->colormap;
}

static void
gdk_win32_set_colormap (GdkDrawable *drawable,
			GdkColormap *colormap)
{
  GdkDrawableImplWin32 *impl = GDK_DRAWABLE_IMPL_WIN32 (drawable);

  if (impl->colormap == colormap)
    return;
  
  if (impl->colormap)
    g_object_unref (impl->colormap);
  impl->colormap = colormap;
  if (impl->colormap)
    g_object_ref (impl->colormap);
}

/* Drawing
 */

static int
rop2_to_rop3 (int rop2)
{
  switch (rop2)
    {
    /* Oh, Microsoft's silly names for binary and ternary rops. */
#define CASE(rop2,rop3) case R2_##rop2: return rop3
      CASE (BLACK, BLACKNESS);
      CASE (NOTMERGEPEN, NOTSRCERASE);
      CASE (MASKNOTPEN, 0x00220326);
      CASE (NOTCOPYPEN, NOTSRCCOPY);
      CASE (MASKPENNOT, SRCERASE);
      CASE (NOT, DSTINVERT);
      CASE (XORPEN, SRCINVERT);
      CASE (NOTMASKPEN, 0x007700E6);
      CASE (MASKPEN, SRCAND);
      CASE (NOTXORPEN, 0x00990066);
      CASE (NOP, 0x00AA0029);
      CASE (MERGENOTPEN, MERGEPAINT);
      CASE (COPYPEN, SRCCOPY);
      CASE (MERGEPENNOT, 0x00DD0228);
      CASE (MERGEPEN, SRCPAINT);
      CASE (WHITE, WHITENESS);
#undef CASE
    default: return SRCCOPY;
    }
}

static int
rop2_to_patblt_rop (int rop2)
{
  switch (rop2)
    {
#define CASE(rop2,patblt_rop) case R2_##rop2: return patblt_rop
      CASE (COPYPEN, PATCOPY);
      CASE (XORPEN, PATINVERT);
      CASE (NOT, DSTINVERT);
      CASE (BLACK, BLACKNESS);
      CASE (WHITE, WHITENESS);
#undef CASE
    default:
      g_warning ("Unhandled rop2 in GC to be used in PatBlt: %#x", rop2);
      return PATCOPY;
    }
}

static inline int
align_with_dash_offset (int a, DWORD *dashes, int num_dashes, GdkGCWin32 *gcwin32)
{
  int	   n = 0;
  int    len_sum = 0;
  /* 
   * We can't simply add the dashoffset, it can be an arbitrary larger
   * or smaller value not even between x1 and x2. It just says use the
   * dash pattern aligned to the offset. So ensure x1 is smaller _x1
   * and we start with the appropriate dash.
   */
  for (n = 0; n < num_dashes; n++)
    len_sum += dashes[n];
  if (   len_sum > 0 /* pathological api usage? */
      && gcwin32->pen_dash_offset > a)
    a -= (((gcwin32->pen_dash_offset/len_sum - a/len_sum) + 1) * len_sum);
  else
    a = gcwin32->pen_dash_offset;

  return a;
}
 
/* Render a dashed line 'by hand'. Used for all dashes on Win9x (where
 * GDI is way too limited), and for double dashes on all Windowses.
 */
static inline gboolean
render_line_horizontal (GdkGCWin32 *gcwin32,
                        int         x1,
                        int         x2,
                        int         y)
{
  int n = 0;
  const int pen_width = MAX (gcwin32->pen_width, 1);
  const int _x1 = x1;

  g_assert (gcwin32->pen_dashes);

  x1 = align_with_dash_offset (x1, gcwin32->pen_dashes, gcwin32->pen_num_dashes, gcwin32);

  for (n = 0; x1 < x2; n++)
    {
      int len = gcwin32->pen_dashes[n % gcwin32->pen_num_dashes];
      if (x1 + len > x2)
        len = x2 - x1;

      if (n % 2 == 0 && x1 + len > _x1)
        if (!GDI_CALL (PatBlt, (gcwin32->hdc, 
				x1 < _x1 ? _x1 : x1, 
				y - pen_width / 2, 
				len, pen_width, 
				rop2_to_patblt_rop (gcwin32->rop2))))
	  return FALSE;

      x1 += gcwin32->pen_dashes[n % gcwin32->pen_num_dashes];
    }

  if (gcwin32->line_style == GDK_LINE_DOUBLE_DASH)
    {
      HBRUSH hbr;

      if ((hbr = SelectObject (gcwin32->hdc, gcwin32->pen_hbrbg)) == HGDI_ERROR)
	return FALSE;
      x1 = _x1;
      x1 += gcwin32->pen_dash_offset;
      for (n = 0; x1 < x2; n++)
	{
	  int len = gcwin32->pen_dashes[n % gcwin32->pen_num_dashes];
	  if (x1 + len > x2)
	    len = x2 - x1;

	  if (n % 2)
	    if (!GDI_CALL (PatBlt, (gcwin32->hdc, x1, y - pen_width / 2,
				    len, pen_width,
				    rop2_to_patblt_rop (gcwin32->rop2))))
	      return FALSE;

	  x1 += gcwin32->pen_dashes[n % gcwin32->pen_num_dashes];
	}
      if (SelectObject (gcwin32->hdc, hbr) == HGDI_ERROR)
	return FALSE;
    }

  return TRUE;
}

static inline gboolean
render_line_vertical (GdkGCWin32 *gcwin32,
		      int         x,
                      int         y1,
                      int         y2)
{
  int n;
  const int pen_width = MAX (gcwin32->pen_width, 1);
  const int _y1 = y1;

  g_assert (gcwin32->pen_dashes);

  y1 = align_with_dash_offset (y1, gcwin32->pen_dashes, gcwin32->pen_num_dashes, gcwin32);
  for (n = 0; y1 < y2; n++)
    {
      int len = gcwin32->pen_dashes[n % gcwin32->pen_num_dashes];
      if (y1 + len > y2)
        len = y2 - y1;
      if (n % 2 == 0 && y1 + len > _y1)
        if (!GDI_CALL (PatBlt, (gcwin32->hdc, x - pen_width / 2, 
				y1 < _y1 ? _y1 : y1, 
				pen_width, len, 
				rop2_to_patblt_rop (gcwin32->rop2))))
	  return FALSE;

      y1 += gcwin32->pen_dashes[n % gcwin32->pen_num_dashes];
    }

  if (gcwin32->line_style == GDK_LINE_DOUBLE_DASH)
    {
      HBRUSH hbr;

      if ((hbr = SelectObject (gcwin32->hdc, gcwin32->pen_hbrbg)) == HGDI_ERROR)
	return FALSE;
      y1 = _y1;
      y1 += gcwin32->pen_dash_offset;
      for (n = 0; y1 < y2; n++)
	{
	  int len = gcwin32->pen_dashes[n % gcwin32->pen_num_dashes];
	  if (y1 + len > y2)
	    len = y2 - y1;
	  if (n % 2)
	    if (!GDI_CALL (PatBlt, (gcwin32->hdc, x - pen_width / 2, y1,
				    pen_width, len,
				    rop2_to_patblt_rop (gcwin32->rop2))))
	      return FALSE;

	  y1 += gcwin32->pen_dashes[n % gcwin32->pen_num_dashes];
	}
      if (SelectObject (gcwin32->hdc, hbr) == HGDI_ERROR)
	return FALSE;
    }

  return TRUE;
}

static void
draw_tiles_lowlevel (HDC  dest,
		     HDC  tile,
		     int  rop3,
		     gint dest_x,
		     gint dest_y,
		     gint tile_x_origin,
		     gint tile_y_origin,
		     gint width,
		     gint height,
		     gint tile_width,
		     gint tile_height)
{
  gint x, y;

  GDK_NOTE (DRAW, g_print ("draw_tiles_lowlevel: %p %+d%+d tile=%p:%dx%d@%+d%+d %dx%d\n",
			   dest,
			   dest_x, dest_y,
			   tile, tile_width, tile_height,
			   tile_x_origin, tile_y_origin,
			   width, height));

  y = tile_y_origin % tile_height;
  if (y > 0)
    y -= tile_height;
  while (y < dest_y + height)
    {
      if (y + tile_height >= dest_y)
	{
	  x = tile_x_origin % tile_width;
	  if (x > 0)
	    x -= tile_width;
	  while (x < dest_x + width)
	    {
	      if (x + tile_width >= dest_x)
		{
		  gint src_x = MAX (0, dest_x - x);
		  gint src_y = MAX (0, dest_y - y);

		  if (!GDI_CALL (BitBlt, (dest, x + src_x, y + src_y,
					  MIN (tile_width, dest_x + width - (x + src_x)),
					  MIN (tile_height, dest_y + height - (y + src_y)),
					  tile,
					  src_x, src_y,
					  rop3)))
		    return;
		}
	      x += tile_width;
	    }
	}
      y += tile_height;
    }
}

static void
draw_tiles (GdkDrawable *drawable,
	    GdkGC       *gc,
	    int          rop3,
	    GdkPixmap   *tile,
	    gint         dest_x,
	    gint 	 dest_y,
	    gint 	 tile_x_origin,
	    gint 	 tile_y_origin,
	    gint 	 width,
	    gint 	 height)
{
  const GdkGCValuesMask mask = GDK_GC_FOREGROUND;
  gint tile_width, tile_height;
  GdkGC *gc_copy;
  HDC dest_hdc, tile_hdc;

  gc_copy = gdk_gc_new (tile);
  gdk_gc_copy (gc_copy, gc);
  dest_hdc = gdk_win32_hdc_get (drawable, gc, mask);
  tile_hdc = gdk_win32_hdc_get (tile, gc_copy, mask);

  gdk_drawable_get_size (tile, &tile_width, &tile_height);

  draw_tiles_lowlevel (dest_hdc, tile_hdc, rop3,
		       dest_x, dest_y, tile_x_origin, tile_y_origin,
		       width, height, tile_width, tile_height);

  gdk_win32_hdc_release (drawable, gc, mask);
  gdk_win32_hdc_release (tile, gc_copy, mask);
  g_object_unref (gc_copy);
}

static void
generic_draw (GdkDrawable    *drawable,
	      GdkGC          *gc,
	      GdkGCValuesMask mask,
	      void (*function) (GdkGCWin32 *, HDC, gint, gint, va_list),
	      const GdkRegion *region,
	      ...)
{
  GdkDrawableImplWin32 *impl = GDK_DRAWABLE_IMPL_WIN32 (drawable);
  GdkGCWin32 *gcwin32 = GDK_GC_WIN32 (gc);
  HDC hdc;
  va_list args;
  GdkFill fill_style = _gdk_gc_get_fill (gc);

  va_start (args, region);

  /* If tiled or stippled, draw to a temp pixmap and do blitting magic.
   */

  if (gcwin32->values_mask & GDK_GC_FILL &&
      ((fill_style == GDK_TILED &&
	gcwin32->values_mask & GDK_GC_TILE &&
	_gdk_gc_get_tile (gc) != NULL)
       ||
       ((fill_style == GDK_OPAQUE_STIPPLED ||
	 fill_style == GDK_STIPPLED) &&
	gcwin32->values_mask & GDK_GC_STIPPLE &&
	_gdk_gc_get_stipple (gc) != NULL)))
    {
      const GdkGCValuesMask blitting_mask = 0;
      GdkGCValuesMask drawing_mask = GDK_GC_FOREGROUND;
      gint ts_x_origin = 0, ts_y_origin = 0;

      gint width = region->extents.x2 - region->extents.x1;
      gint height = region->extents.y2 - region->extents.y1;

      GdkPixmap *mask_pixmap =
	gdk_pixmap_new (drawable, width, height, 1);
      GdkPixmap *tile_pixmap =
	gdk_pixmap_new (drawable, width, height, -1);
      GdkPixmap *stipple_bitmap = NULL;
      GdkColor fg;
      
      GdkGC *mask_gc = gdk_gc_new (mask_pixmap);
      GdkGC *tile_gc = gdk_gc_new (tile_pixmap);

      HDC mask_hdc;
      HDC tile_hdc;

      GdkGCValues gcvalues;

      hdc = gdk_win32_hdc_get (drawable, gc, blitting_mask);
      tile_hdc = gdk_win32_hdc_get (tile_pixmap, tile_gc, blitting_mask);

      if (gcwin32->values_mask & GDK_GC_TS_X_ORIGIN)
	ts_x_origin = gc->ts_x_origin;
      if (gcwin32->values_mask & GDK_GC_TS_Y_ORIGIN)
	ts_y_origin = gc->ts_y_origin;

      ts_x_origin -= region->extents.x1;
      ts_y_origin -= region->extents.y1;

      /* Fill mask bitmap with zeros */
      gdk_gc_set_function (mask_gc, GDK_CLEAR);
      gdk_draw_rectangle (mask_pixmap, mask_gc, TRUE,
			  0, 0, width, height);

      /* Paint into mask bitmap, drawing ones */
      gdk_gc_set_function (mask_gc, GDK_COPY);
      fg.pixel = 1;
      gdk_gc_set_foreground (mask_gc, &fg);

      /* If the drawing function uses line attributes, set them as in
       * the real GC.
       */
      if (mask & LINE_ATTRIBUTES)
	{
	  gdk_gc_get_values (gc, &gcvalues);
	  if (gcvalues.line_width != 0 ||
	      gcvalues.line_style != GDK_LINE_SOLID ||
	      gcvalues.cap_style != GDK_CAP_BUTT ||
	      gcvalues.join_style != GDK_JOIN_MITER)
	    gdk_gc_set_line_attributes (mask_gc,
					gcvalues.line_width,
					gcvalues.line_style,
					gcvalues.cap_style,
					gcvalues.join_style);
	  drawing_mask |= LINE_ATTRIBUTES;
	}

      /* Ditto, if the drawing function draws text, set up for that. */
      if (mask & GDK_GC_FONT)
	drawing_mask |= GDK_GC_FONT;

      mask_hdc = gdk_win32_hdc_get (mask_pixmap, mask_gc, drawing_mask);
      (*function) (GDK_GC_WIN32 (mask_gc), mask_hdc,
		   region->extents.x1, region->extents.y1, args);
      gdk_win32_hdc_release (mask_pixmap, mask_gc, drawing_mask);

      if (fill_style == GDK_TILED)
	{
	  /* Tile pixmap with tile */
	  draw_tiles (tile_pixmap, tile_gc, SRCCOPY,
		      _gdk_gc_get_tile (gc),
		      0, 0, ts_x_origin, ts_y_origin,
		      width, height);
	}
      else
	{
	  /* Tile with stipple */
	  GdkGC *stipple_gc;

	  stipple_bitmap = gdk_pixmap_new (NULL, width, height, 1);
	  stipple_gc = gdk_gc_new (stipple_bitmap);

	  /* Tile stipple bitmap */
	  draw_tiles (stipple_bitmap, stipple_gc, SRCCOPY,
		      _gdk_gc_get_stipple (gc),
		      0, 0, ts_x_origin, ts_y_origin,
		      width, height);

	  if (fill_style == GDK_OPAQUE_STIPPLED)
	    {
	      /* Fill tile pixmap with background */
	      fg.pixel = _gdk_gc_get_bg_pixel (gc);
	      gdk_gc_set_foreground (tile_gc, &fg);
	      gdk_draw_rectangle (tile_pixmap, tile_gc, TRUE,
				  0, 0, width, height);
	    }
	  g_object_unref (stipple_gc);
	}

      mask_hdc = gdk_win32_hdc_get (mask_pixmap, mask_gc, blitting_mask);

      if (fill_style == GDK_STIPPLED ||
	  fill_style == GDK_OPAQUE_STIPPLED)
	{
	  HDC stipple_hdc;
	  HBRUSH fg_brush;
	  HGDIOBJ old_tile_brush;
	  GdkGC *stipple_gc;

	  stipple_gc = gdk_gc_new (stipple_bitmap);
	  stipple_hdc = gdk_win32_hdc_get (stipple_bitmap, stipple_gc, blitting_mask);

	  if ((fg_brush = CreateSolidBrush
	       (_gdk_win32_colormap_color (impl->colormap,
					   _gdk_gc_get_fg_pixel (gc)))) == NULL)
	    WIN32_GDI_FAILED ("CreateSolidBrush");

	  if ((old_tile_brush = SelectObject (tile_hdc, fg_brush)) == NULL)
	    WIN32_GDI_FAILED ("SelectObject");

	  /* Paint tile with foreround where stipple is one
	   *
	   *  Desired ternary ROP: (P=foreground, S=stipple, D=destination)
           *   P   S   D   ?
           *   0   0   0   0
           *   0   0   1   1
           *   0   1   0   0
           *   0   1   1   0
           *   1   0   0   0
           *   1   0   1   1
           *   1   1   0   1
           *   1   1   1   1
	   *
	   * Reading bottom-up: 11100010 = 0xE2. PSDK docs say this is
	   * known as DSPDxax, with hex value 0x00E20746.
	   */
	  GDI_CALL (BitBlt, (tile_hdc, 0, 0, width, height,
			     stipple_hdc, 0, 0, ROP3_DSPDxax));

	  if (fill_style == GDK_STIPPLED)
	    {
	      /* Punch holes in mask where stipple is zero */
	      GDI_CALL (BitBlt, (mask_hdc, 0, 0, width, height,
				 stipple_hdc, 0, 0, SRCAND));
	    }

	  GDI_CALL (SelectObject, (tile_hdc, old_tile_brush));
	  GDI_CALL (DeleteObject, (fg_brush));
	  gdk_win32_hdc_release (stipple_bitmap, stipple_gc, blitting_mask);
	  g_object_unref (stipple_gc);
	  g_object_unref (stipple_bitmap);
	}

      /* Tile pixmap now contains the pattern that we should paint in
       * the areas where mask is one. (It is filled with said pattern.)
       */

      GDI_CALL (MaskBlt, (hdc, region->extents.x1, region->extents.y1,
			  width, height,
			  tile_hdc, 0, 0,
			  GDK_PIXMAP_HBITMAP (mask_pixmap), 0, 0,
			  MAKEROP4 (rop2_to_rop3 (gcwin32->rop2), ROP3_D)));

      /* Cleanup */
      gdk_win32_hdc_release (mask_pixmap, mask_gc, blitting_mask);
      g_object_unref (mask_gc);
      g_object_unref (mask_pixmap);
      gdk_win32_hdc_release (tile_pixmap, tile_gc, blitting_mask);
      g_object_unref (tile_gc);
      g_object_unref (tile_pixmap);

      gdk_win32_hdc_release (drawable, gc, blitting_mask);
    }
  else
    {
      hdc = gdk_win32_hdc_get (drawable, gc, mask);
      (*function) (gcwin32, hdc, 0, 0, args);
      gdk_win32_hdc_release (drawable, gc, mask);
    }
  va_end (args);
}

static GdkRegion *
widen_bounds (GdkRectangle *bounds,
	      gint          pen_width)
{
  if (pen_width == 0)
    pen_width = 1;

  bounds->x -= pen_width;
  bounds->y -= pen_width;
  bounds->width += 2 * pen_width;
  bounds->height += 2 * pen_width;

  return gdk_region_rectangle (bounds);
}

static void
draw_rectangle (GdkGCWin32 *gcwin32,
		HDC         hdc,
		gint        x_offset,
		gint        y_offset,
		va_list     args)
{
  HGDIOBJ old_pen_or_brush;
  gboolean filled;
  gint x;
  gint y;
  gint width;
  gint height;

  filled = va_arg (args, gboolean);
  x = va_arg (args, gint);
  y = va_arg (args, gint);
  width = va_arg (args, gint);
  height = va_arg (args, gint);
  
  x -= x_offset;
  y -= y_offset;

  if (!filled && MUST_RENDER_DASHES_MANUALLY (gcwin32))
    {
      render_line_vertical (gcwin32, x, y, y+height+1) &&
      render_line_horizontal (gcwin32, x, x+width+1, y) &&
      render_line_vertical (gcwin32, x+width+1, y, y+height+1) &&
      render_line_horizontal (gcwin32, x, x+width+1, y+height+1);
    }
  else
    {
      if (filled)
	old_pen_or_brush = SelectObject (hdc, GetStockObject (NULL_PEN));
      else
	old_pen_or_brush = SelectObject (hdc, GetStockObject (HOLLOW_BRUSH));
      if (old_pen_or_brush == NULL)
	WIN32_GDI_FAILED ("SelectObject");
      else
	GDI_CALL (Rectangle, (hdc, x, y, x+width+1, y+height+1));

      if (old_pen_or_brush != NULL)
	GDI_CALL (SelectObject, (hdc, old_pen_or_brush));
    }
}

static void
gdk_win32_draw_rectangle (GdkDrawable *drawable,
			  GdkGC       *gc,
			  gboolean     filled,
			  gint         x,
			  gint         y,
			  gint         width,
			  gint         height)
{
  GdkRectangle bounds;
  GdkRegion *region;

  GDK_NOTE (DRAW, g_print ("gdk_win32_draw_rectangle: %s (%p) %s%dx%d@%+d%+d\n",
			   _gdk_win32_drawable_description (drawable),
			   gc,
			   (filled ? "fill " : ""),
			   width, height, x, y));
    
  bounds.x = x;
  bounds.y = y;
  bounds.width = width;
  bounds.height = height;
  region = widen_bounds (&bounds, GDK_GC_WIN32 (gc)->pen_width);

  generic_draw (drawable, gc,
		GDK_GC_FOREGROUND | GDK_GC_BACKGROUND |
		(filled ? 0 : LINE_ATTRIBUTES),
		draw_rectangle, region, filled, x, y, width, height);

  gdk_region_destroy (region);
}

static void
draw_arc (GdkGCWin32 *gcwin32,
	  HDC         hdc,
	  gint        x_offset,
	  gint        y_offset,
	  va_list     args)
{
  HGDIOBJ old_pen;
  gboolean filled;
  gint x, y;
  gint width, height;
  gint angle1, angle2;
  int nXStartArc, nYStartArc, nXEndArc, nYEndArc;

  filled = va_arg (args, gboolean);
  x = va_arg (args, gint);
  y = va_arg (args, gint);
  width = va_arg (args, gint);
  height = va_arg (args, gint);
  angle1 = va_arg (args, gint);
  angle2 = va_arg (args, gint);

  x -= x_offset;
  y -= y_offset;
  
  if (angle2 >= 360*64)
    {
      nXStartArc = nYStartArc = nXEndArc = nYEndArc = 0;
    }
  else if (angle2 > 0)
    {
      nXStartArc = x + width/2 + width * cos(angle1/64.*2.*G_PI/360.);
      nYStartArc = y + height/2 + -height * sin(angle1/64.*2.*G_PI/360.);
      nXEndArc = x + width/2 + width * cos((angle1+angle2)/64.*2.*G_PI/360.);
      nYEndArc = y + height/2 + -height * sin((angle1+angle2)/64.*2.*G_PI/360.);
    }
  else
    {
      nXEndArc = x + width/2 + width * cos(angle1/64.*2.*G_PI/360.);
      nYEndArc = y + height/2 + -height * sin(angle1/64.*2.*G_PI/360.);
      nXStartArc = x + width/2 + width * cos((angle1+angle2)/64.*2.*G_PI/360.);
      nYStartArc = y + height/2 + -height * sin((angle1+angle2)/64.*2.*G_PI/360.);
    }
  
  if (filled)
    {
      old_pen = SelectObject (hdc, GetStockObject (NULL_PEN));
      GDK_NOTE (DRAW, g_print ("... Pie(%p,%d,%d,%d,%d,%d,%d,%d,%d)\n",
			       hdc, x, y, x+width, y+height,
			       nXStartArc, nYStartArc, nXEndArc, nYEndArc));
      GDI_CALL (Pie, (hdc, x, y, x+width, y+height,
		      nXStartArc, nYStartArc, nXEndArc, nYEndArc));
      GDI_CALL (SelectObject, (hdc, old_pen));
    }
  else
    {
      GDK_NOTE (DRAW, g_print ("... Arc(%p,%d,%d,%d,%d,%d,%d,%d,%d)\n",
			       hdc, x, y, x+width, y+height,
			       nXStartArc, nYStartArc, nXEndArc, nYEndArc));
      GDI_CALL (Arc, (hdc, x, y, x+width, y+height,
		      nXStartArc, nYStartArc, nXEndArc, nYEndArc));
    }
}

static void
gdk_win32_draw_arc (GdkDrawable *drawable,
		    GdkGC       *gc,
		    gboolean     filled,
		    gint         x,
		    gint         y,
		    gint         width,
		    gint         height,
		    gint         angle1,
		    gint         angle2)
{
  GdkRectangle bounds;
  GdkRegion *region;

  GDK_NOTE (DRAW, g_print ("gdk_win32_draw_arc: %s  %d,%d,%d,%d  %d %d\n",
			   _gdk_win32_drawable_description (drawable),
			   x, y, width, height, angle1, angle2));

  if (width <= 2 || height <= 2 || angle2 == 0)
    return;

  bounds.x = x;
  bounds.y = y;
  bounds.width = width;
  bounds.height = height;
  region = widen_bounds (&bounds, GDK_GC_WIN32 (gc)->pen_width);

  generic_draw (drawable, gc,
		GDK_GC_FOREGROUND | (filled ? 0 : LINE_ATTRIBUTES),
		draw_arc, region, filled, x, y, width, height, angle1, angle2);

  gdk_region_destroy (region);
}

static void
draw_polygon (GdkGCWin32 *gcwin32,
	      HDC         hdc,
	      gint        x_offset,
	      gint        y_offset,
	      va_list     args)
{
  gboolean filled;
  POINT *pts;
  HGDIOBJ old_pen_or_brush;
  gint npoints;
  gint i;

  filled = va_arg (args, gboolean);
  pts = va_arg (args, POINT *);
  npoints = va_arg (args, gint);

  if (x_offset != 0 || y_offset != 0)
    for (i = 0; i < npoints; i++)
      {
	pts[i].x -= x_offset;
	pts[i].y -= y_offset;
      }

  if (filled)
    old_pen_or_brush = SelectObject (hdc, GetStockObject (NULL_PEN));
  else
    old_pen_or_brush = SelectObject (hdc, GetStockObject (HOLLOW_BRUSH));
  if (old_pen_or_brush == NULL)
    WIN32_GDI_FAILED ("SelectObject");
  GDI_CALL (Polygon, (hdc, pts, npoints));
  if (old_pen_or_brush != NULL)
    GDI_CALL (SelectObject, (hdc, old_pen_or_brush));
}

static void
gdk_win32_draw_polygon (GdkDrawable *drawable,
			GdkGC       *gc,
			gboolean     filled,
			GdkPoint    *points,
			gint         npoints)
{
  GdkRectangle bounds;
  GdkRegion *region;
  POINT *pts;
  int i;

  GDK_NOTE (DRAW, g_print ("gdk_win32_draw_polygon: %s %d points\n",
			   _gdk_win32_drawable_description (drawable),
			   npoints));

  if (npoints < 2)
    return;

  bounds.x = G_MAXINT;
  bounds.y = G_MAXINT;
  bounds.width = 0;
  bounds.height = 0;

  pts = g_new (POINT, npoints);

  for (i = 0; i < npoints; i++)
    {
      bounds.x = MIN (bounds.x, points[i].x);
      bounds.y = MIN (bounds.y, points[i].y);
      pts[i].x = points[i].x;
      pts[i].y = points[i].y;
    }

  for (i = 0; i < npoints; i++)
    {
      bounds.width = MAX (bounds.width, points[i].x - bounds.x);
      bounds.height = MAX (bounds.height, points[i].y - bounds.y);
    }

  region = widen_bounds (&bounds, GDK_GC_WIN32 (gc)->pen_width);

  generic_draw (drawable, gc,
		GDK_GC_FOREGROUND | (filled ? 0 : LINE_ATTRIBUTES),
		draw_polygon, region, filled, pts, npoints);

  gdk_region_destroy (region);
  g_free (pts);
}

typedef struct
{
  gint x, y;
  HDC hdc;
} gdk_draw_text_arg;

static void
gdk_draw_text_handler (GdkWin32SingleFont *singlefont,
		       const wchar_t      *wcstr,
		       int                 wclen,
		       void               *arg)
{
  HGDIOBJ oldfont;
  SIZE size;
  gdk_draw_text_arg *argp = (gdk_draw_text_arg *) arg;

  if (!singlefont)
    return;

  if ((oldfont = SelectObject (argp->hdc, singlefont->hfont)) == NULL)
    {
      WIN32_GDI_FAILED ("SelectObject");
      return;
    }
  
  if (!TextOutW (argp->hdc, argp->x, argp->y, wcstr, wclen))
    WIN32_GDI_FAILED ("TextOutW");
  GetTextExtentPoint32W (argp->hdc, wcstr, wclen, &size);
  argp->x += size.cx;

  SelectObject (argp->hdc, oldfont);
}

static void
gdk_win32_draw_text (GdkDrawable *drawable,
		     GdkFont     *font,
		     GdkGC       *gc,
		     gint         x,
		     gint         y,
		     const gchar *text,
		     gint         text_length)
{
  const GdkGCValuesMask mask = GDK_GC_FOREGROUND|GDK_GC_FONT;
  wchar_t *wcstr, wc;
  glong wlen;
  gdk_draw_text_arg arg;

  if (text_length == 0)
    return;

  g_assert (font->type == GDK_FONT_FONT || font->type == GDK_FONT_FONTSET);

  arg.x = x;
  arg.y = y;
  arg.hdc = gdk_win32_hdc_get (drawable, gc, mask);

  GDK_NOTE (DRAW, g_print ("gdk_win32_draw_text: %s (%d,%d) \"%.*s\" (len %d)\n",
			   _gdk_win32_drawable_description (drawable),
			   x, y,
			   (text_length > 10 ? 10 : text_length),
			   text, text_length));
  
  if (text_length == 1)
    {
      /* For single characters, don't try to interpret as UTF-8. */
      wc = (guchar) text[0];
      _gdk_wchar_text_handle (font, &wc, 1, gdk_draw_text_handler, &arg);
    }
  else
    {
      wcstr = g_utf8_to_utf16 (text, text_length, NULL, &wlen, NULL);
      _gdk_wchar_text_handle (font, wcstr, wlen, gdk_draw_text_handler, &arg);
      g_free (wcstr);
    }

  gdk_win32_hdc_release (drawable, gc, mask);
}

static void
gdk_win32_draw_text_wc (GdkDrawable	 *drawable,
			GdkFont          *font,
			GdkGC		 *gc,
			gint		  x,
			gint		  y,
			const GdkWChar *text,
			gint		  text_length)
{
  const GdkGCValuesMask mask = GDK_GC_FOREGROUND|GDK_GC_FONT;
  gint i;
  wchar_t *wcstr;
  gdk_draw_text_arg arg;

  if (text_length == 0)
    return;

  g_assert (font->type == GDK_FONT_FONT || font->type == GDK_FONT_FONTSET);

  arg.x = x;
  arg.y = y;
  arg.hdc = gdk_win32_hdc_get (drawable, gc, mask);

  GDK_NOTE (DRAW, g_print ("gdk_win32_draw_text_wc: %s (%d,%d) len: %d\n",
			   _gdk_win32_drawable_description (drawable),
			   x, y, text_length));
      
  if (sizeof (wchar_t) != sizeof (GdkWChar))
    {
      wcstr = g_new (wchar_t, text_length);
      for (i = 0; i < text_length; i++)
	wcstr[i] = text[i];
    }
  else
    wcstr = (wchar_t *) text;

  _gdk_wchar_text_handle (font, wcstr, text_length,
			 gdk_draw_text_handler, &arg);

  if (sizeof (wchar_t) != sizeof (GdkWChar))
    g_free (wcstr);

  gdk_win32_hdc_release (drawable, gc, mask);
}

static void
gdk_win32_draw_drawable (GdkDrawable *drawable,
			 GdkGC       *gc,
			 GdkPixmap   *src,
			 gint         xsrc,
			 gint         ysrc,
			 gint         xdest,
			 gint         ydest,
			 gint         width,
			 gint         height,
			 GdkDrawable *original_src)
{
  g_assert (GDK_IS_DRAWABLE_IMPL_WIN32 (drawable));

  _gdk_win32_blit (FALSE, (GdkDrawableImplWin32 *) drawable,
		   gc, src, xsrc, ysrc,
		   xdest, ydest, width, height);
}

static void
gdk_win32_draw_points (GdkDrawable *drawable,
		       GdkGC       *gc,
		       GdkPoint    *points,
		       gint         npoints)
{
  HDC hdc;
  HGDIOBJ old_pen;
  int i;

  hdc = gdk_win32_hdc_get (drawable, gc, GDK_GC_FOREGROUND);
  
  GDK_NOTE (DRAW, g_print ("gdk_win32_draw_points: %s %d points\n",
			   _gdk_win32_drawable_description (drawable),
			   npoints));

  /* The X11 version uses XDrawPoint(), which doesn't use the fill
   * mode, so don't use generic_draw. But we should use the current
   * function, so we can't use SetPixel(). Draw single-pixel
   * rectangles (sigh).
   */

  old_pen = SelectObject (hdc, GetStockObject (NULL_PEN));
  for (i = 0; i < npoints; i++)
    Rectangle (hdc, points[i].x, points[i].y,
	       points[i].x + 2, points[i].y + 2);

  SelectObject (hdc, old_pen);
  gdk_win32_hdc_release (drawable, gc, GDK_GC_FOREGROUND);
}

static void
draw_segments (GdkGCWin32 *gcwin32,
	       HDC         hdc,
	       gint        x_offset,
	       gint        y_offset,
	       va_list     args)
{
  GdkSegment *segs;
  gint nsegs;
  gint i;

  segs = va_arg (args, GdkSegment *);
  nsegs = va_arg (args, gint);

  if (x_offset != 0 || y_offset != 0)
    {
      /* must not modify in place, but could splice in the offset all below */
      segs = g_memdup (segs, nsegs * sizeof (GdkSegment));
      for (i = 0; i < nsegs; i++)
        {
          segs[i].x1 -= x_offset;
          segs[i].y1 -= y_offset;
          segs[i].x2 -= x_offset;
          segs[i].y2 -= y_offset;
        }
    }

  if (MUST_RENDER_DASHES_MANUALLY (gcwin32))
    {
      for (i = 0; i < nsegs; i++)
	{
	  if (segs[i].x1 == segs[i].x2)
	    {
	      int y1, y2;
	      
	      if (segs[i].y1 <= segs[i].y2)
		y1 = segs[i].y1, y2 = segs[i].y2;
	      else
		y1 = segs[i].y2, y2 = segs[i].y1;
	      
	      render_line_vertical (gcwin32, segs[i].x1, y1, y2);
	    }
	  else if (segs[i].y1 == segs[i].y2)
	    {
	      int x1, x2;
	      
	      if (segs[i].x1 <= segs[i].x2)
		x1 = segs[i].x1, x2 = segs[i].x2;
	      else
		x1 = segs[i].x2, x2 = segs[i].x1;
	      
	      render_line_horizontal (gcwin32, x1, x2, segs[i].y1);
	    }
	  else
	    GDI_CALL (MoveToEx, (hdc, segs[i].x1, segs[i].y1, NULL)) &&
	      GDI_CALL (LineTo, (hdc, segs[i].x2, segs[i].y2));
	}
    }
  else
    {
      for (i = 0; i < nsegs; i++)
	{
	  const GdkSegment *ps = &segs[i];
	  const int x1 = ps->x1, y1 = ps->y1;
	  int x2 = ps->x2, y2 = ps->y2;

	  GDK_NOTE (DRAW, g_print (" +%d+%d..+%d+%d", x1, y1, x2, y2));
	  GDI_CALL (MoveToEx, (hdc, x1, y1, NULL)) &&
	    GDI_CALL (LineTo, (hdc, x2, y2));
	}

      GDK_NOTE (DRAW, g_print ("\n"));
    }
  if (x_offset != 0 || y_offset != 0)
    g_free (segs);
}

static void
gdk_win32_draw_segments (GdkDrawable *drawable,
			 GdkGC       *gc,
			 GdkSegment  *segs,
			 gint         nsegs)
{
  GdkRectangle bounds;
  GdkRegion *region;
  gint i;

  GDK_NOTE (DRAW, g_print ("gdk_win32_draw_segments: %s %d segs\n",
			   _gdk_win32_drawable_description (drawable),
			   nsegs));

  bounds.x = G_MAXINT;
  bounds.y = G_MAXINT;
  bounds.width = 0;
  bounds.height = 0;

  for (i = 0; i < nsegs; i++)
    {
      bounds.x = MIN (bounds.x, segs[i].x1);
      bounds.x = MIN (bounds.x, segs[i].x2);
      bounds.y = MIN (bounds.y, segs[i].y1);
      bounds.y = MIN (bounds.y, segs[i].y2);
    }

  for (i = 0; i < nsegs; i++)
    {
      bounds.width = MAX (bounds.width, segs[i].x1 - bounds.x);
      bounds.width = MAX (bounds.width, segs[i].x2 - bounds.x);
      bounds.height = MAX (bounds.height, segs[i].y1 - bounds.y);
      bounds.height = MAX (bounds.height, segs[i].y2 - bounds.y);
    }

  region = widen_bounds (&bounds, GDK_GC_WIN32 (gc)->pen_width);

  generic_draw (drawable, gc, GDK_GC_FOREGROUND | LINE_ATTRIBUTES,
		draw_segments, region, segs, nsegs);

  gdk_region_destroy (region);
}

static void
draw_lines (GdkGCWin32 *gcwin32,
	    HDC         hdc,
	    gint        x_offset,
	    gint        y_offset,
	    va_list     args)
{
  POINT *pts;
  gint npoints;
  gint i;

  pts = va_arg (args, POINT *);
  npoints = va_arg (args, gint);

  if (x_offset != 0 || y_offset != 0)
    for (i = 0; i < npoints; i++)
      {
	pts[i].x -= x_offset;
	pts[i].y -= y_offset;
      }
  
  if (MUST_RENDER_DASHES_MANUALLY (gcwin32))
    {
      for (i = 0; i < npoints - 1; i++)
        {
	  if (pts[i].x == pts[i+1].x)
	    {
	      int y1, y2;
	      if (pts[i].y > pts[i+1].y)
	        y1 = pts[i+1].y, y2 = pts[i].y;
	      else
	        y1 = pts[i].y, y2 = pts[i+1].y;
	      
	      render_line_vertical (gcwin32, pts[i].x, y1, y2);
	    }
	  else if (pts[i].y == pts[i+1].y)
	    {
	      int x1, x2;
	      if (pts[i].x > pts[i+1].x)
	        x1 = pts[i+1].x, x2 = pts[i].x;
	      else
	        x1 = pts[i].x, x2 = pts[i+1].x;

	      render_line_horizontal (gcwin32, x1, x2, pts[i].y);
	    }
	  else
	    GDI_CALL (MoveToEx, (hdc, pts[i].x, pts[i].y, NULL)) &&
	      GDI_CALL (LineTo, (hdc, pts[i+1].x, pts[i+1].y));
	}
    }
  else
    GDI_CALL (Polyline, (hdc, pts, npoints));
}

static void
gdk_win32_draw_lines (GdkDrawable *drawable,
		      GdkGC       *gc,
		      GdkPoint    *points,
		      gint         npoints)
{
  GdkRectangle bounds;
  GdkRegion *region;
  POINT *pts;
  int i;

  GDK_NOTE (DRAW, g_print ("gdk_win32_draw_lines: %s %d points\n",
			   _gdk_win32_drawable_description (drawable),
			   npoints));

  if (npoints < 2)
    return;

  bounds.x = G_MAXINT;
  bounds.y = G_MAXINT;
  bounds.width = 0;
  bounds.height = 0;

  pts = g_new (POINT, npoints);

  for (i = 0; i < npoints; i++)
    {
      bounds.x = MIN (bounds.x, points[i].x);
      bounds.y = MIN (bounds.y, points[i].y);
      pts[i].x = points[i].x;
      pts[i].y = points[i].y;
    }

  for (i = 0; i < npoints; i++)
    {
      bounds.width = MAX (bounds.width, points[i].x - bounds.x);
      bounds.height = MAX (bounds.height, points[i].y - bounds.y);
    }

  region = widen_bounds (&bounds, GDK_GC_WIN32 (gc)->pen_width);

  generic_draw (drawable, gc, GDK_GC_FOREGROUND | GDK_GC_BACKGROUND |
			      LINE_ATTRIBUTES,
		draw_lines, region, pts, npoints);

  gdk_region_destroy (region);
  g_free (pts);
}

static void
blit_from_pixmap (gboolean              use_fg_bg,
		  GdkDrawableImplWin32 *dest,
		  HDC                   hdc,
		  GdkPixmapImplWin32   *src,
		  GdkGC                *gc,
		  gint         	      	xsrc,
		  gint         	      	ysrc,
		  gint         	      	xdest,
		  gint         	      	ydest,
		  gint         	      	width,
		  gint         	      	height)
{
  GdkGCWin32 *gcwin32 = GDK_GC_WIN32 (gc);
  HDC srcdc;
  HBITMAP holdbitmap;
  RGBQUAD oldtable[256], newtable[256];
  COLORREF bg, fg;

  gint newtable_size = 0, oldtable_size = 0;
  gboolean ok = TRUE;
  
  GDK_NOTE (DRAW, g_print ("blit_from_pixmap\n"));

  srcdc = _gdk_win32_drawable_acquire_dc (GDK_DRAWABLE (src));
  if (!srcdc)
    return;
  
  if (!(holdbitmap = SelectObject (srcdc, ((GdkDrawableImplWin32 *) src)->handle)))
    WIN32_GDI_FAILED ("SelectObject");
  else
    {
      if (GDK_PIXMAP_OBJECT (src->parent_instance.wrapper)->depth <= 8)
	{
	  /* Blitting from a 1, 4 or 8-bit pixmap */

	  if ((oldtable_size = GetDIBColorTable (srcdc, 0, 256, oldtable)) == 0)
	    WIN32_GDI_FAILED ("GetDIBColorTable");
	  else if (GDK_PIXMAP_OBJECT (src->parent_instance.wrapper)->depth == 1)
	    {
	      /* Blitting from an 1-bit pixmap */

	      gint bgix, fgix;
	      
	      if (use_fg_bg)
		{
		  bgix = _gdk_gc_get_bg_pixel (gc);
		  fgix = _gdk_gc_get_fg_pixel (gc);
		}
	      else
		{
		  bgix = 0;
		  fgix = 1;
		}
	      
	      if (GDK_IS_PIXMAP_IMPL_WIN32 (dest) &&
		  GDK_PIXMAP_OBJECT (dest->wrapper)->depth <= 8)
		{
		  /* Destination is also pixmap, get fg and bg from
		   * its palette. Either use the foreground and
		   * background pixel values in the GC (only in the
		   * case of gdk_image_put(), cf. XPutImage()), or 0
		   * and 1 to index the palette.
		   */
		  if (!GDI_CALL (GetDIBColorTable, (hdc, bgix, 1, newtable)) ||
		      !GDI_CALL (GetDIBColorTable, (hdc, fgix, 1, newtable+1)))
		    ok = FALSE;
		}
	      else
		{
		  /* Destination is a window, get fg and bg from its
		   * colormap
		   */

		  bg = _gdk_win32_colormap_color (dest->colormap, bgix);
		  fg = _gdk_win32_colormap_color (dest->colormap, fgix);
		  newtable[0].rgbBlue = GetBValue (bg);
		  newtable[0].rgbGreen = GetGValue (bg);
		  newtable[0].rgbRed = GetRValue (bg);
		  newtable[0].rgbReserved = 0;
		  newtable[1].rgbBlue = GetBValue (fg);
		  newtable[1].rgbGreen = GetGValue (fg);
		  newtable[1].rgbRed = GetRValue (fg);
		  newtable[1].rgbReserved = 0;
		}
	      if (ok)
		GDK_NOTE (DRAW, g_print ("bg: %02x %02x %02x "
					 "fg: %02x %02x %02x\n",
					 newtable[0].rgbRed,
					 newtable[0].rgbGreen,
					 newtable[0].rgbBlue,
					 newtable[1].rgbRed,
					 newtable[1].rgbGreen,
					 newtable[1].rgbBlue));
	      newtable_size = 2;
	    }
	  else if (GDK_IS_PIXMAP_IMPL_WIN32 (dest))
	    {
	      /* Destination is pixmap, get its color table */
	      
	      if ((newtable_size = GetDIBColorTable (hdc, 0, 256, newtable)) == 0)
		WIN32_GDI_FAILED ("GetDIBColorTable"), ok = FALSE;
	    }
	  
	  /* If blitting between pixmaps, set source's color table */
	  if (ok && newtable_size > 0)
	    {
	      GDK_NOTE (MISC_OR_COLORMAP,
			g_print ("blit_from_pixmap: set color table"
				 " hdc=%p count=%d\n",
				 srcdc, newtable_size));
	      if (!GDI_CALL (SetDIBColorTable, (srcdc, 0, newtable_size, newtable)))
		ok = FALSE;
	    }
	}
      
      if (ok)
	if (!BitBlt (hdc, xdest, ydest, width, height,
		     srcdc, xsrc, ysrc, rop2_to_rop3 (gcwin32->rop2)) &&
	    GetLastError () != ERROR_INVALID_HANDLE)
	  WIN32_GDI_FAILED ("BitBlt");
      
      /* Restore source's color table if necessary */
      if (ok && newtable_size > 0 && oldtable_size > 0)
	{
	  GDK_NOTE (MISC_OR_COLORMAP,
		    g_print ("blit_from_pixmap: reset color table"
			     " hdc=%p count=%d\n",
			     srcdc, oldtable_size));
	  GDI_CALL (SetDIBColorTable, (srcdc, 0, oldtable_size, oldtable));
	}
      
      GDI_CALL (SelectObject, (srcdc, holdbitmap));
    }
  
  _gdk_win32_drawable_release_dc (GDK_DRAWABLE (src));
}

static void
blit_inside_drawable (HDC                   hdc,
                      GdkGCWin32           *gcwin32,
                      GdkDrawableImplWin32 *src,
                      gint                  xsrc,
                      gint                  ysrc,
                      gint                  xdest,
                      gint                  ydest,
                      gint                  width,
                      gint                  height)

{
  GDK_NOTE (DRAW, g_print ("blit_inside_drawable\n"));

  if GDK_IS_WINDOW_IMPL_WIN32 (src)
    {
      /* Simply calling BitBlt() instead of these ScrollDC() gymnastics might
       * seem tempting, but we need to do this to prevent blitting garbage when
       * scrolling a window that is partially obscured by another window. For
       * example, GIMP's toolbox being over the editor window. */

      RECT emptyRect, clipRect;
      HRGN updateRgn;
      GdkRegion *update_region;

      clipRect.left = xdest;
      clipRect.top = ydest;
      clipRect.right = xdest + width;
      clipRect.bottom = ydest + height;

      SetRectEmpty (&emptyRect);
      updateRgn = CreateRectRgnIndirect (&emptyRect);

      if (!ScrollDC (hdc, xdest - xsrc, ydest - ysrc, NULL, &clipRect, updateRgn, NULL))
        WIN32_GDI_FAILED ("ScrollDC");
      else
	{
	  GdkDrawable *wrapper = src->wrapper;
	  update_region = _gdk_win32_hrgn_to_region (updateRgn);
	  if (!gdk_region_empty (update_region))
	    _gdk_window_invalidate_for_expose (GDK_WINDOW (wrapper), update_region);
	  gdk_region_destroy (update_region);
	}

      if (!DeleteObject (updateRgn))
        WIN32_GDI_FAILED ("DeleteObject");
    }
  else
    {
      GDI_CALL (BitBlt, (hdc, xdest, ydest, width, height,
                         hdc, xsrc, ysrc, rop2_to_rop3 (gcwin32->rop2)));
    }
}

static void
blit_from_window (HDC                   hdc,
		  GdkGCWin32           *gcwin32,
		  GdkDrawableImplWin32 *src,
		  gint         	      	xsrc,
		  gint         	      	ysrc,
		  gint         	      	xdest,
		  gint         	      	ydest,
		  gint         	      	width,
		  gint         	      	height)
{
  HDC srcdc;
  HPALETTE holdpal = NULL;
  GdkColormap *cmap = gdk_colormap_get_system ();

  GDK_NOTE (DRAW, g_print ("blit_from_window\n"));

  if ((srcdc = GetDC (src->handle)) == NULL)
    {
      WIN32_GDI_FAILED ("GetDC");
      return;
    }

  if (cmap->visual->type == GDK_VISUAL_PSEUDO_COLOR ||
      cmap->visual->type == GDK_VISUAL_STATIC_COLOR)
    {
      gint k;
      
      if (!(holdpal = SelectPalette (srcdc, GDK_WIN32_COLORMAP_DATA (cmap)->hpal, FALSE)))
	WIN32_GDI_FAILED ("SelectPalette");
      else if ((k = RealizePalette (srcdc)) == GDI_ERROR)
	WIN32_GDI_FAILED ("RealizePalette");
      else if (k > 0)
	GDK_NOTE (MISC_OR_COLORMAP,
		  g_print ("blit_from_window: realized %d\n", k));
    }
  
  GDI_CALL (BitBlt, (hdc, xdest, ydest, width, height,
		     srcdc, xsrc, ysrc, rop2_to_rop3 (gcwin32->rop2)));
  
  if (holdpal != NULL)
    GDI_CALL (SelectPalette, (srcdc, holdpal, FALSE));
  
  GDI_CALL (ReleaseDC, (src->handle, srcdc));
}

void
_gdk_win32_blit (gboolean              use_fg_bg,
		 GdkDrawableImplWin32 *draw_impl,
		 GdkGC       	      *gc,
		 GdkDrawable 	      *src,
		 gint        	       xsrc,
		 gint        	       ysrc,
		 gint        	       xdest,
		 gint        	       ydest,
		 gint        	       width,
		 gint        	       height)
{
  HDC hdc;
  HRGN src_rgn, draw_rgn, outside_rgn;
  RECT r;
  GdkDrawableImplWin32 *src_impl = NULL;
  gint src_width, src_height;
  
  GDK_NOTE (DRAW, g_print ("_gdk_win32_blit: src:%s %dx%d@%+d%+d\n"
			   "                 dst:%s @%+d%+d use_fg_bg=%d\n",
			   _gdk_win32_drawable_description (src),
			   width, height, xsrc, ysrc,
			   _gdk_win32_drawable_description (&draw_impl->parent_instance),
			   xdest, ydest,
			   use_fg_bg));

  /* If blitting from the root window, take the multi-monitor offset
   * into account.
   */
  if (src == ((GdkWindowObject *)_gdk_root)->impl)
    {
      GDK_NOTE (DRAW, g_print ("... offsetting src coords\n"));
      xsrc -= _gdk_offset_x;
      ysrc -= _gdk_offset_y;
    }

  if (GDK_IS_DRAWABLE_IMPL_WIN32 (src))
    src_impl = (GdkDrawableImplWin32 *) src;
  else if (GDK_IS_WINDOW (src))
    src_impl = (GdkDrawableImplWin32 *) GDK_WINDOW_OBJECT (src)->impl;
  else if (GDK_IS_PIXMAP (src))
    src_impl = (GdkDrawableImplWin32 *) GDK_PIXMAP_OBJECT (src)->impl;
  else
    g_assert_not_reached ();

  if (GDK_IS_WINDOW_IMPL_WIN32 (draw_impl) &&
      GDK_IS_PIXMAP_IMPL_WIN32 (src_impl))
    {
      GdkPixmapImplWin32 *src_pixmap = GDK_PIXMAP_IMPL_WIN32 (src_impl);

      if (xsrc < 0)
	{
	  width += xsrc;
	  xdest -= xsrc;
	  xsrc = 0;
	}

      if (ysrc < 0)
	{
	  height += ysrc;
	  ydest -= ysrc;
	  ysrc = 0;
	}

      if (xsrc + width > src_pixmap->width)
	width = src_pixmap->width - xsrc;
      if (ysrc + height > src_pixmap->height)
	height = src_pixmap->height - ysrc;
    }

  hdc = gdk_win32_hdc_get (&draw_impl->parent_instance, gc, GDK_GC_FOREGROUND);

  gdk_drawable_get_size (src_impl->wrapper, &src_width, &src_height);

  if ((src_rgn = CreateRectRgn (0, 0, src_width + 1, src_height + 1)) == NULL)
    {
      WIN32_GDI_FAILED ("CreateRectRgn");
    }
  else if ((draw_rgn = CreateRectRgn (xsrc, ysrc,
				      xsrc + width + 1,
				      ysrc + height + 1)) == NULL)
    {
      WIN32_GDI_FAILED ("CreateRectRgn");
    }
  else
    {
      if (GDK_IS_WINDOW_IMPL_WIN32 (draw_impl))
	{
	  int comb;
	  
	  /* If we are drawing on a window, calculate the region that is
	   * outside the source pixmap, and invalidate that, causing it to
	   * be cleared. Not completely sure whether this is always needed. XXX
	   */
	  SetRectEmpty (&r);
	  outside_rgn = CreateRectRgnIndirect (&r);
	  
	  if ((comb = CombineRgn (outside_rgn,
				  draw_rgn, src_rgn,
				  RGN_DIFF)) == ERROR)
	    WIN32_GDI_FAILED ("CombineRgn");
	  else if (comb != NULLREGION)
	    {
	      OffsetRgn (outside_rgn, xdest, ydest);
	      GDK_NOTE (DRAW, (GetRgnBox (outside_rgn, &r),
			       g_print ("... InvalidateRgn "
					"bbox: %ldx%ld@%+ld%+ld\n",
					r.right - r.left - 1, r.bottom - r.top - 1,
					r.left, r.top)));
	      InvalidateRgn (draw_impl->handle, outside_rgn, TRUE);
	    }
	  GDI_CALL (DeleteObject, (outside_rgn));
	}

#if 1 /* Don't know if this is necessary XXX */
      if (CombineRgn (draw_rgn, draw_rgn, src_rgn, RGN_AND) == COMPLEXREGION)
	g_warning ("gdk_win32_blit: CombineRgn returned a COMPLEXREGION");
      
      GetRgnBox (draw_rgn, &r);
      if (r.left != xsrc || r.top != ysrc ||
	  r.right != xsrc + width + 1 || r.bottom != ysrc + height + 1)
	{
	  xdest += r.left - xsrc;
	  xsrc = r.left;
	  ydest += r.top - ysrc;
	  ysrc = r.top;
	  width = r.right - xsrc - 1;
	  height = r.bottom - ysrc - 1;
	  
	  GDK_NOTE (DRAW, g_print ("... restricted to src: %dx%d@%+d%+d, "
				   "dest: @%+d%+d\n",
				   width, height, xsrc, ysrc,
				   xdest, ydest));
	}
#endif

      GDI_CALL (DeleteObject, (src_rgn));
      GDI_CALL (DeleteObject, (draw_rgn));
    }

  if (draw_impl->handle == src_impl->handle)
    blit_inside_drawable (hdc, GDK_GC_WIN32 (gc), src_impl,
                          xsrc, ysrc, xdest, ydest, width, height);
  else if (GDK_IS_PIXMAP_IMPL_WIN32 (src_impl))
    blit_from_pixmap (use_fg_bg, draw_impl, hdc,
		      (GdkPixmapImplWin32 *) src_impl, gc,
		      xsrc, ysrc, xdest, ydest, width, height);
  else
    blit_from_window (hdc, GDK_GC_WIN32 (gc), src_impl,
                      xsrc, ysrc, xdest, ydest, width, height);

  gdk_win32_hdc_release (&draw_impl->parent_instance, gc, GDK_GC_FOREGROUND);
}

static void
gdk_win32_draw_image (GdkDrawable     *drawable,
		      GdkGC           *gc,
		      GdkImage        *image,
		      gint             xsrc,
		      gint             ysrc,
		      gint             xdest,
		      gint             ydest,
		      gint             width,
		      gint             height)
{
  g_assert (GDK_IS_DRAWABLE_IMPL_WIN32 (drawable));

  _gdk_win32_blit (TRUE, (GdkDrawableImplWin32 *) drawable,
		   gc, (GdkPixmap *) image->windowing_data,
		   xsrc, ysrc, xdest, ydest, width, height);
}

static void
gdk_win32_draw_pixbuf (GdkDrawable     *drawable,
			GdkGC           *gc,
			GdkPixbuf       *pixbuf,
			gint             src_x,
			gint             src_y,
			gint             dest_x,
			gint             dest_y,
			gint             width,
			gint             height,
			GdkRgbDither     dither,
			gint             x_dither,
			gint             y_dither)
{
  GdkDrawable *wrapper = GDK_DRAWABLE_IMPL_WIN32 (drawable)->wrapper;
  GDK_DRAWABLE_CLASS (_gdk_drawable_impl_win32_parent_class)->draw_pixbuf (wrapper, gc, pixbuf,
									     src_x, src_y, dest_x, dest_y,
									     width, height,
									     dither, x_dither, y_dither);
}

/**
 * _gdk_win32_drawable_acquire_dc
 * @drawable: a Win32 #GdkDrawable implementation
 * 
 * Gets a DC with the given drawable selected into
 * it.
 *
 * Return value: The DC, on success. Otherwise
 *  %NULL. If this function succeeded
 *  _gdk_win32_drawable_release_dc()  must be called
 *  release the DC when you are done using it.
 **/
HDC 
_gdk_win32_drawable_acquire_dc (GdkDrawable *drawable)
{
  GdkDrawableImplWin32 *impl = GDK_DRAWABLE_IMPL_WIN32 (drawable);
  
  if (GDK_IS_WINDOW_IMPL_WIN32 (drawable) &&
      GDK_WINDOW_DESTROYED (impl->wrapper))
    return NULL;

  if (!impl->hdc)
    {
      if (GDK_IS_PIXMAP_IMPL_WIN32 (impl))
	{
	  impl->hdc = CreateCompatibleDC (NULL);
	  if (!impl->hdc)
	    WIN32_GDI_FAILED ("CreateCompatibleDC");
	  
	  if (impl->hdc)
	    {
	      impl->saved_dc_bitmap = SelectObject (impl->hdc,
						    impl->handle);
	      if (!impl->saved_dc_bitmap)
		{
		  WIN32_GDI_FAILED ("SelectObject");
		  DeleteDC (impl->hdc);
		  impl->hdc = NULL;
		}
	    }
	}
      else
	{
	  impl->hdc = GetDC (impl->handle);
	  if (!impl->hdc)
	    WIN32_GDI_FAILED ("GetDC");
	}
    }

  if (impl->hdc)
    {
      impl->hdc_count++;
      return impl->hdc;
    }
  else
    {
      return NULL;
    }
}

/**
 * _gdk_win32_drawable_release_dc
 * @drawable: a Win32 #GdkDrawable implementation
 * 
 * Releases the reference count for the DC
 * from _gdk_win32_drawable_acquire_dc()
 **/
void
_gdk_win32_drawable_release_dc (GdkDrawable *drawable)
{
  GdkDrawableImplWin32 *impl = GDK_DRAWABLE_IMPL_WIN32 (drawable);
  
  g_return_if_fail (impl->hdc_count > 0);

  impl->hdc_count--;
  if (impl->hdc_count == 0)
    {
      if (impl->saved_dc_bitmap)
	{
	  GDI_CALL (SelectObject, (impl->hdc, impl->saved_dc_bitmap));
	  impl->saved_dc_bitmap = NULL;
	}
      
      if (impl->hdc)
	{
	  if (GDK_IS_PIXMAP_IMPL_WIN32 (impl))
	    GDI_CALL (DeleteDC, (impl->hdc));
	  else
	    GDI_CALL (ReleaseDC, (impl->handle, impl->hdc));
	  impl->hdc = NULL;
	}
    }
}

static void
gdk_win32_cairo_surface_release_hdc (void *data)
{
  _gdk_win32_drawable_release_dc (GDK_DRAWABLE (data));
}

cairo_surface_t *
_gdk_windowing_create_cairo_surface (GdkDrawable *drawable,
				     gint width,
				     gint height)
{
  cairo_surface_t *surface;
  HDC hdc;

  hdc = _gdk_win32_drawable_acquire_dc (drawable);
  if (!hdc)
    return NULL;

  surface = cairo_win32_surface_create (hdc);

  /* Whenever the cairo surface is destroyed, we need to release the
   * HDC that was acquired */
  cairo_surface_set_user_data (surface, &gdk_win32_cairo_hdc_key,
			       drawable,
			       gdk_win32_cairo_surface_release_hdc);

  return surface;
}

static void
gdk_win32_cairo_surface_destroy (void *data)
{
  GdkDrawableImplWin32 *impl = data;

  impl->cairo_surface = NULL;
}

static cairo_surface_t *
gdk_win32_ref_cairo_surface (GdkDrawable *drawable)
{
  GdkDrawableImplWin32 *impl = GDK_DRAWABLE_IMPL_WIN32 (drawable);

  if (GDK_IS_WINDOW_IMPL_WIN32 (drawable) &&
      GDK_WINDOW_DESTROYED (impl->wrapper))
    return NULL;

  if (!impl->cairo_surface)
    {
      /* width and height are determined from the DC */
      impl->cairo_surface = _gdk_windowing_create_cairo_surface (drawable, 0, 0);

      /* Whenever the cairo surface is destroyed, we need to clear the
       * pointer that we had stored here */
      cairo_surface_set_user_data (impl->cairo_surface, &gdk_win32_cairo_key,
				   drawable,
				   gdk_win32_cairo_surface_destroy);
    }
  else
    cairo_surface_reference (impl->cairo_surface);

  return impl->cairo_surface;
}

void
_gdk_windowing_set_cairo_surface_size (cairo_surface_t *surface,
				       gint width,
				       gint height)
{
  // Do nothing.  The surface size is determined by the DC
}

static gint
gdk_win32_get_depth (GdkDrawable *drawable)
{
  /* This is a bit bogus but I'm not sure the other way is better */

  return gdk_drawable_get_depth (GDK_DRAWABLE_IMPL_WIN32 (drawable)->wrapper);
}

static GdkScreen*
gdk_win32_get_screen (GdkDrawable *drawable)
{
  return gdk_screen_get_default ();
}
 
static GdkVisual*
gdk_win32_get_visual (GdkDrawable *drawable)
{
  return gdk_drawable_get_visual (GDK_DRAWABLE_IMPL_WIN32 (drawable)->wrapper);
}

HGDIOBJ
gdk_win32_drawable_get_handle (GdkDrawable *drawable)
{
  if (GDK_IS_WINDOW (drawable))
    {
      GdkWindow *window = (GdkWindow *)drawable;

      /* Try to ensure the window has a native window */
      if (!_gdk_window_has_impl (window))
	gdk_window_ensure_native (window);

      if (!GDK_WINDOW_IS_WIN32 (window))
	{
	  g_warning (G_STRLOC " drawable is not a native Win32 window");
	  return NULL;
	}
    }
  else if (!GDK_IS_PIXMAP (drawable))
    {
      g_warning (G_STRLOC " drawable is not a pixmap or window");
      return NULL;
    }

  return GDK_DRAWABLE_HANDLE (drawable);
}

/**
 * _gdk_win32_drawable_finish
 * @drawable: a Win32 #GdkDrawable implementation
 * 
 * Releases any resources allocated internally for the drawable.
 * This is called when the drawable becomes unusable
 * (gdk_window_destroy() for a window, or the refcount going to
 * zero for a pixmap.)
 **/
void
_gdk_win32_drawable_finish (GdkDrawable *drawable)
{
  GdkDrawableImplWin32 *impl = GDK_DRAWABLE_IMPL_WIN32 (drawable);

  if (impl->cairo_surface)
    {
      cairo_surface_finish (impl->cairo_surface);
      cairo_surface_set_user_data (impl->cairo_surface, &gdk_win32_cairo_hdc_key, NULL, NULL);
      cairo_surface_set_user_data (impl->cairo_surface, &gdk_win32_cairo_key, NULL, NULL);
    }

  /* impl->hdc_count doesn't have to be 0 here; as there may still be surfaces
   * created with gdk_windowing_create_cairo_surface() out there, which are not
   * managed internally by the drawable */
}
