/* GDK - The GIMP Drawing Kit
 * Copyright (C) 2005 Red Hat, Inc. 
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.	 See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#include "gdkcairo.h"
#include "gdkdrawable.h"
#include "gdkinternals.h"
#include "gdkregion-generic.h"
#include "gdkalias.h"

static void
gdk_ensure_surface_flush (gpointer surface)
{
  cairo_surface_flush (surface);
  cairo_surface_destroy (surface);
}

/**
 * gdk_cairo_create:
 * @drawable: a #GdkDrawable
 * 
 * Creates a Cairo context for drawing to @drawable.
 *
 * <note><para>
 * Note that due to double-buffering, Cairo contexts created 
 * in a GTK+ expose event handler cannot be cached and reused 
 * between different expose events. 
 * </para></note>
 *
 * Return value: A newly created Cairo context. Free with
 *  cairo_destroy() when you are done drawing.
 * 
 * Since: 2.8
 **/
cairo_t *
gdk_cairo_create (GdkDrawable *drawable)
{
  static const cairo_user_data_key_t key;
  cairo_surface_t *surface;
  cairo_t *cr;
    
  g_return_val_if_fail (GDK_IS_DRAWABLE (drawable), NULL);

  surface = _gdk_drawable_ref_cairo_surface (drawable);
  cr = cairo_create (surface);

  if (GDK_DRAWABLE_GET_CLASS (drawable)->set_cairo_clip)
    GDK_DRAWABLE_GET_CLASS (drawable)->set_cairo_clip (drawable, cr);
    
  /* Ugly workaround for GTK not ensuring to flush surfaces before
   * directly accessing the drawable backed by the surface. Not visible
   * on X11 (where flushing is a no-op). For details, see
   * https://bugzilla.gnome.org/show_bug.cgi?id=628291
   */
  cairo_set_user_data (cr, &key, surface, gdk_ensure_surface_flush);

  return cr;
}

/**
 * gdk_cairo_reset_clip:
 * @cr: a #cairo_t
 * @drawable: a #GdkDrawable
 *
 * Resets the clip region for a Cairo context created by gdk_cairo_create().
 *
 * This resets the clip region to the "empty" state for the given drawable.
 * This is required for non-native windows since a direct call to
 * cairo_reset_clip() would unset the clip region inherited from the
 * drawable (i.e. the window clip region), and thus let you e.g.
 * draw outside your window.
 *
 * This is rarely needed though, since most code just create a new cairo_t
 * using gdk_cairo_create() each time they want to draw something.
 *
 * Since: 2.18
 **/
void
gdk_cairo_reset_clip (cairo_t            *cr,
		      GdkDrawable        *drawable)
{
  cairo_reset_clip (cr);

  if (GDK_DRAWABLE_GET_CLASS (drawable)->set_cairo_clip)
    GDK_DRAWABLE_GET_CLASS (drawable)->set_cairo_clip (drawable, cr);
}

/**
 * gdk_cairo_set_source_color:
 * @cr: a #cairo_t
 * @color: a #GdkColor
 * 
 * Sets the specified #GdkColor as the source color of @cr.
 *
 * Since: 2.8
 **/
void
gdk_cairo_set_source_color (cairo_t        *cr,
			    const GdkColor *color)
{
  g_return_if_fail (cr != NULL);
  g_return_if_fail (color != NULL);
    
  cairo_set_source_rgb (cr,
			color->red / 65535.,
			color->green / 65535.,
			color->blue / 65535.);
}

/**
 * gdk_cairo_rectangle:
 * @cr: a #cairo_t
 * @rectangle: a #GdkRectangle
 * 
 * Adds the given rectangle to the current path of @cr.
 *
 * Since: 2.8
 **/
void
gdk_cairo_rectangle (cairo_t            *cr,
		     const GdkRectangle *rectangle)
{
  g_return_if_fail (cr != NULL);
  g_return_if_fail (rectangle != NULL);

  cairo_rectangle (cr,
		   rectangle->x,     rectangle->y,
		   rectangle->width, rectangle->height);
}

/**
 * gdk_cairo_region:
 * @cr: a #cairo_t
 * @region: a #GdkRegion
 * 
 * Adds the given region to the current path of @cr.
 *
 * Since: 2.8
 **/
void
gdk_cairo_region (cairo_t         *cr,
		  const GdkRegion *region)
{
  GdkRegionBox *boxes;
  gint n_boxes, i;

  g_return_if_fail (cr != NULL);
  g_return_if_fail (region != NULL);

  boxes = region->rects;
  n_boxes = region->numRects;

  for (i = 0; i < n_boxes; i++)
    cairo_rectangle (cr,
		     boxes[i].x1,
		     boxes[i].y1,
		     boxes[i].x2 - boxes[i].x1,
		     boxes[i].y2 - boxes[i].y1);
}

/**
 * gdk_cairo_set_source_pixbuf:
 * @cr: a #Cairo context
 * @pixbuf: a #GdkPixbuf
 * @pixbuf_x: X coordinate of location to place upper left corner of @pixbuf
 * @pixbuf_y: Y coordinate of location to place upper left corner of @pixbuf
 * 
 * Sets the given pixbuf as the source pattern for the Cairo context.
 * The pattern has an extend mode of %CAIRO_EXTEND_NONE and is aligned
 * so that the origin of @pixbuf is @pixbuf_x, @pixbuf_y
 *
 * Since: 2.8
 **/
void
gdk_cairo_set_source_pixbuf (cairo_t         *cr,
			     const GdkPixbuf *pixbuf,
			     double           pixbuf_x,
			     double           pixbuf_y)
{
  gint width = gdk_pixbuf_get_width (pixbuf);
  gint height = gdk_pixbuf_get_height (pixbuf);
  guchar *gdk_pixels = gdk_pixbuf_get_pixels (pixbuf);
  int gdk_rowstride = gdk_pixbuf_get_rowstride (pixbuf);
  int n_channels = gdk_pixbuf_get_n_channels (pixbuf);
  int cairo_stride;
  guchar *cairo_pixels;
  cairo_format_t format;
  cairo_surface_t *surface;
  static const cairo_user_data_key_t key;
  cairo_status_t status;
  int j;

  if (n_channels == 3)
    format = CAIRO_FORMAT_RGB24;
  else
    format = CAIRO_FORMAT_ARGB32;

  cairo_stride = cairo_format_stride_for_width (format, width);
  cairo_pixels = g_malloc_n (height, cairo_stride);
  surface = cairo_image_surface_create_for_data ((unsigned char *)cairo_pixels,
                                                 format,
                                                 width, height, cairo_stride);

  status = cairo_surface_set_user_data (surface, &key,
                                        cairo_pixels, (cairo_destroy_func_t)g_free);
  if (status != CAIRO_STATUS_SUCCESS)
    {
      g_free (cairo_pixels);
      goto out;
    }

  for (j = height; j; j--)
    {
      guchar *p = gdk_pixels;
      guchar *q = cairo_pixels;

      if (n_channels == 3)
	{
	  guchar *end = p + 3 * width;
	  
	  while (p < end)
	    {
#if G_BYTE_ORDER == G_LITTLE_ENDIAN
	      q[0] = p[2];
	      q[1] = p[1];
	      q[2] = p[0];
#else	  
	      q[1] = p[0];
	      q[2] = p[1];
	      q[3] = p[2];
#endif
	      p += 3;
	      q += 4;
	    }
	}
      else
	{
	  guchar *end = p + 4 * width;
	  guint t1,t2,t3;
	    
#define MULT(d,c,a,t) G_STMT_START { t = c * a + 0x80; d = ((t >> 8) + t) >> 8; } G_STMT_END

	  while (p < end)
	    {
#if G_BYTE_ORDER == G_LITTLE_ENDIAN
	      MULT(q[0], p[2], p[3], t1);
	      MULT(q[1], p[1], p[3], t2);
	      MULT(q[2], p[0], p[3], t3);
	      q[3] = p[3];
#else	  
	      q[0] = p[3];
	      MULT(q[1], p[0], p[3], t1);
	      MULT(q[2], p[1], p[3], t2);
	      MULT(q[3], p[2], p[3], t3);
#endif
	      
	      p += 4;
	      q += 4;
	    }
	  
#undef MULT
	}

      gdk_pixels += gdk_rowstride;
      cairo_pixels += cairo_stride;
    }

out:
  cairo_set_source_surface (cr, surface, pixbuf_x, pixbuf_y);
  cairo_surface_destroy (surface);
}

/**
 * gdk_cairo_set_source_pixmap:
 * @cr: a #Cairo context
 * @pixmap: a #GdkPixmap
 * @pixmap_x: X coordinate of location to place upper left corner of @pixmap
 * @pixmap_y: Y coordinate of location to place upper left corner of @pixmap
 * 
 * Sets the given pixmap as the source pattern for the Cairo context.
 * The pattern has an extend mode of %CAIRO_EXTEND_NONE and is aligned
 * so that the origin of @pixmap is @pixmap_x, @pixmap_y
 *
 * Since: 2.10
 *
 * Deprecated: 2.24: This function is being removed in GTK+ 3 (together
 *     with #GdkPixmap). Instead, use gdk_cairo_set_source_window() where
 *     appropriate.
 **/
void
gdk_cairo_set_source_pixmap (cairo_t   *cr,
			     GdkPixmap *pixmap,
			     double     pixmap_x,
			     double     pixmap_y)
{
  cairo_surface_t *surface;
  
  surface = _gdk_drawable_ref_cairo_surface (GDK_DRAWABLE (pixmap));
  cairo_set_source_surface (cr, surface, pixmap_x, pixmap_y);
  cairo_surface_destroy (surface);
}

/**
 * gdk_cairo_set_source_window:
 * @cr: a #Cairo context
 * @window: a #GdkWindow
 * @x: X coordinate of location to place upper left corner of @window
 * @y: Y coordinate of location to place upper left corner of @window
 *
 * Sets the given window as the source pattern for the Cairo context.
 * The pattern has an extend mode of %CAIRO_EXTEND_NONE and is aligned
 * so that the origin of @window is @x, @y. The window contains all its
 * subwindows when rendering.
 *
 * Note that the contents of @window are undefined outside of the
 * visible part of @window, so use this function with care.
 *
 * Since: 2.24
 **/
void
gdk_cairo_set_source_window (cairo_t   *cr,
                             GdkWindow *window,
                             double     x,
                             double     y)
{
  cairo_surface_t *surface;

  g_return_if_fail (cr != NULL);
  g_return_if_fail (GDK_IS_WINDOW (window));

  surface = _gdk_drawable_ref_cairo_surface (GDK_DRAWABLE (window));
  cairo_set_source_surface (cr, surface, x, y);
  cairo_surface_destroy (surface);
}


#define __GDK_CAIRO_C__
#include "gdkaliasdef.c"
