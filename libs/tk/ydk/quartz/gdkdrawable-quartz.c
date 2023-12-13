/* gdkdrawable-quartz.c
 *
 * Copyright (C) 2005-2007 Imendio AB
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

#include "config.h"
#include <sys/time.h>
#include <cairo-quartz.h>
#include "gdkprivate-quartz.h"

static gpointer parent_class;

static cairo_user_data_key_t gdk_quartz_cairo_key;

typedef struct {
  GdkDrawable  *drawable;
  CGContextRef  cg_context;
} GdkQuartzCairoSurfaceData;

void
_gdk_windowing_set_cairo_surface_size (cairo_surface_t *surface,
				       int              width,
				       int              height)
{
  /* This is not supported with quartz surfaces. */
}

static void
gdk_quartz_cairo_surface_destroy (void *data)
{
  GdkQuartzCairoSurfaceData *surface_data = data;
  GdkDrawableImplQuartz *impl = GDK_DRAWABLE_IMPL_QUARTZ (surface_data->drawable);

  impl->cairo_surface = NULL;

  gdk_quartz_drawable_release_context (surface_data->drawable,
                                       surface_data->cg_context);

  g_free (surface_data);
}

cairo_surface_t *
_gdk_windowing_create_cairo_surface (GdkDrawable *drawable,
				     int          width,
				     int          height)
{
  CGContextRef cg_context;
  GdkQuartzCairoSurfaceData *surface_data;
  cairo_surface_t *surface;

  cg_context = gdk_quartz_drawable_get_context (drawable, TRUE);

  surface_data = g_new (GdkQuartzCairoSurfaceData, 1);
  surface_data->drawable = drawable;
  surface_data->cg_context = cg_context;

  if (cg_context)
    surface = cairo_quartz_surface_create_for_cg_context (cg_context,
                                                          width, height);
  else
    surface = cairo_quartz_surface_create(CAIRO_FORMAT_ARGB32, width, height);

  cairo_surface_set_user_data (surface, &gdk_quartz_cairo_key,
                               surface_data,
                               gdk_quartz_cairo_surface_destroy);

  return surface;
}

static cairo_surface_t *
gdk_quartz_ref_cairo_surface (GdkDrawable *drawable)
{
  GdkDrawableImplQuartz *impl = GDK_DRAWABLE_IMPL_QUARTZ (drawable);

  if (GDK_IS_WINDOW_IMPL_QUARTZ (drawable) &&
      GDK_WINDOW_DESTROYED (impl->wrapper))
    return NULL;

  if (!impl->cairo_surface)
    {
      int width, height;

      gdk_drawable_get_size (drawable, &width, &height);
      impl->cairo_surface = _gdk_windowing_create_cairo_surface (drawable,
                                                                 width, height);
    }
  else
    cairo_surface_reference (impl->cairo_surface);

  return impl->cairo_surface;
}

static void
gdk_quartz_set_colormap (GdkDrawable *drawable,
			 GdkColormap *colormap)
{
  GdkDrawableImplQuartz *impl = GDK_DRAWABLE_IMPL_QUARTZ (drawable);

  if (impl->colormap == colormap)
    return;
  
  if (impl->colormap)
    g_object_unref (impl->colormap);
  impl->colormap = colormap;
  if (impl->colormap)
    g_object_ref (impl->colormap);
}

static GdkColormap*
gdk_quartz_get_colormap (GdkDrawable *drawable)
{
  return GDK_DRAWABLE_IMPL_QUARTZ (drawable)->colormap;
}

static GdkScreen*
gdk_quartz_get_screen (GdkDrawable *drawable)
{
  return _gdk_screen;
}

static GdkVisual*
gdk_quartz_get_visual (GdkDrawable *drawable)
{
  return gdk_drawable_get_visual (GDK_DRAWABLE_IMPL_QUARTZ (drawable)->wrapper);
}

static int
gdk_quartz_get_depth (GdkDrawable *drawable)
{
  /* This is a bit bogus but I'm not sure the other way is better */

  return gdk_drawable_get_depth (GDK_DRAWABLE_IMPL_QUARTZ (drawable)->wrapper);
}

static void
gdk_quartz_draw_rectangle (GdkDrawable *drawable,
			   GdkGC       *gc,
			   gboolean     filled,
			   gint         x,
			   gint         y,
			   gint         width,
			   gint         height)
{
  CGContextRef context = gdk_quartz_drawable_get_context (drawable, FALSE);

  if (!context)
    return;

  if(!_gdk_quartz_gc_update_cg_context (gc,
					drawable,
					context,
					filled ?
					GDK_QUARTZ_CONTEXT_FILL :
					GDK_QUARTZ_CONTEXT_STROKE))
    {
      gdk_quartz_drawable_release_context (drawable, context);
      return;
    }
  if (filled)
    {
      CGRect rect = CGRectMake (x, y, width, height);

      CGContextFillRect (context, rect);
    }
  else
    {
      CGRect rect = CGRectMake (x + 0.5, y + 0.5, width, height);

      CGContextStrokeRect (context, rect);
    }

  gdk_quartz_drawable_release_context (drawable, context);
}

static void
gdk_quartz_draw_arc (GdkDrawable *drawable,
		     GdkGC       *gc,
		     gboolean     filled,
		     gint         x,
		     gint         y,
		     gint         width,
		     gint         height,
		     gint         angle1,
		     gint         angle2)
{
  CGContextRef context = gdk_quartz_drawable_get_context (drawable, FALSE);
  float start_angle, end_angle;
  gboolean clockwise = FALSE;

  if (!context)
    return;

  if (!_gdk_quartz_gc_update_cg_context (gc, drawable, context,
					 filled ?
					 GDK_QUARTZ_CONTEXT_FILL :
					 GDK_QUARTZ_CONTEXT_STROKE))
    {
      gdk_quartz_drawable_release_context (drawable, context);
      return;
    }
  start_angle = angle1 * 2.0 * G_PI / 360.0 / 64.0;
  end_angle = start_angle + angle2 * 2.0 * G_PI / 360.0 / 64.0;

  /*  angle2 is relative to angle1 and can be negative, which switches
   *  the drawing direction
   */
  if (angle2 < 0)
    clockwise = TRUE;

  /*  below, flip the coordinate system back to its original y-diretion
   *  so the angles passed to CGContextAddArc() are interpreted as
   *  expected
   *
   *  FIXME: the implementation below works only for perfect circles
   *  (width == height). Any other aspect ratio either scales the
   *  line width unevenly or scales away the path entirely for very
   *  small line widths (esp. for line_width == 0, which is a hair
   *  line on X11 but must be approximated with the thinnest possible
   *  line on quartz).
   */

  if (filled)
    {
      CGContextTranslateCTM (context,
                             x + width / 2.0,
                             y + height / 2.0);
      CGContextScaleCTM (context, 1.0, - (double)height / (double)width);

      CGContextMoveToPoint (context, 0, 0);
      CGContextAddArc (context, 0, 0, width / 2.0,
		       start_angle, end_angle,
		       clockwise);
      CGContextClosePath (context);
      CGContextFillPath (context);
    }
  else
    {
      CGContextTranslateCTM (context,
                             x + width / 2.0 + 0.5,
                             y + height / 2.0 + 0.5);
      CGContextScaleCTM (context, 1.0, - (double)height / (double)width);

      CGContextAddArc (context, 0, 0, width / 2.0,
		       start_angle, end_angle,
		       clockwise);
      CGContextStrokePath (context);
    }

  gdk_quartz_drawable_release_context (drawable, context);
}

static void
gdk_quartz_draw_polygon (GdkDrawable *drawable,
			 GdkGC       *gc,
			 gboolean     filled,
			 GdkPoint    *points,
			 gint         npoints)
{
  CGContextRef context = gdk_quartz_drawable_get_context (drawable, FALSE);
  int i;

  if (!context)
    return;

  if (!_gdk_quartz_gc_update_cg_context (gc, drawable, context,
					 filled ?
					 GDK_QUARTZ_CONTEXT_FILL :
					 GDK_QUARTZ_CONTEXT_STROKE))
    {
      gdk_quartz_drawable_release_context (drawable, context);
      return;
    }
  if (filled)
    {
      CGContextMoveToPoint (context, points[0].x, points[0].y);
      for (i = 1; i < npoints; i++)
	CGContextAddLineToPoint (context, points[i].x, points[i].y);

      CGContextClosePath (context);
      CGContextFillPath (context);
    }
  else
    {
      CGContextMoveToPoint (context, points[0].x + 0.5, points[0].y + 0.5);
      for (i = 1; i < npoints; i++)
	CGContextAddLineToPoint (context, points[i].x + 0.5, points[i].y + 0.5);

      CGContextClosePath (context);
      CGContextStrokePath (context);
    }

  gdk_quartz_drawable_release_context (drawable, context);
}

static void
gdk_quartz_draw_text (GdkDrawable *drawable,
		      GdkFont     *font,
		      GdkGC       *gc,
		      gint         x,
		      gint         y,
		      const gchar *text,
		      gint         text_length)
{
  /* FIXME: Implement */
}

static void
gdk_quartz_draw_text_wc (GdkDrawable    *drawable,
			 GdkFont	*font,
			 GdkGC	        *gc,
			 gint	         x,
			 gint	         y,
			 const GdkWChar *text,
			 gint	         text_length)
{
  /* FIXME: Implement */
}

static void
gdk_quartz_draw_drawable (GdkDrawable *drawable,
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
  int src_depth = gdk_drawable_get_depth (src);
  int dest_depth = gdk_drawable_get_depth (drawable);
  GdkDrawableImplQuartz *src_impl;

  if (GDK_IS_WINDOW_IMPL_QUARTZ (src))
    {
      GdkWindowImplQuartz *window_impl;

      window_impl = GDK_WINDOW_IMPL_QUARTZ (src);

      /* We do support moving areas on the same drawable, if it can be done
       * by using a scroll. FIXME: We need to check that the params support
       * this hack, and make sure it's done properly with any offsets etc?
       */
      if (drawable == (GdkDrawable *)window_impl)
        {
          NSRect rect = NSMakeRect (xsrc, ysrc, width, height);
          NSSize offset = NSMakeSize (xdest - xsrc, ydest - ysrc);
          GdkRectangle tmp_rect;
          GdkRegion *orig_region, *offset_region, *need_display_region;
          GdkWindow *window = GDK_DRAWABLE_IMPL_QUARTZ (drawable)->wrapper;

          /* Origin region */
          tmp_rect.x = xsrc;
          tmp_rect.y = ysrc;
          tmp_rect.width = width;
          tmp_rect.height = height;
          orig_region = gdk_region_rectangle (&tmp_rect);

          /* Destination region (or the offset region) */
          offset_region = gdk_region_copy (orig_region);
          gdk_region_offset (offset_region, offset.width, offset.height);

          need_display_region = gdk_region_copy (orig_region);

          if (window_impl->in_paint_rect_count == 0)
            {
              GdkRegion *bottom_border_region;

              /* If we are not in drawRect:, we can use scrollRect:.
               * We apply scrollRect on the rectangle to be moved and
               * subtract this area from the rectangle that needs display.
               *
               * Note: any area in this moved region that already needed
               * display will be handled by GDK (queue translation).
               *
               * Queuing the redraw below is important, otherwise the
               * results from scrollRect will not take effect!
               */
              [window_impl->view scrollRect:rect by:offset];

              gdk_region_subtract (need_display_region, offset_region);

              /* Here we take special care with the bottom window border,
               * which extents 4 pixels and typically draws rounded corners.
               */
              tmp_rect.x = 0;
              tmp_rect.y = gdk_window_get_height (window) - 4;
              tmp_rect.width = gdk_window_get_width (window);
              tmp_rect.height = 4;

              if (gdk_region_rect_in (offset_region, &tmp_rect) !=
                  GDK_OVERLAP_RECTANGLE_OUT)
                {
                  /* We are copying pixels to the bottom border, we need
                   * to submit this area for redisplay to get the rounded
                   * corners drawn.
                   */
                  gdk_region_union_with_rect (need_display_region,
                                              &tmp_rect);
                }

              /* Compute whether the bottom border is moved elsewhere.
               * Because this part will have rounded corners, we have
               * to fill the contents of where the rounded corners used
               * to be. We post this area for redisplay.
               */
              bottom_border_region = gdk_region_rectangle (&tmp_rect);
              gdk_region_intersect (bottom_border_region,
                                    orig_region);

              gdk_region_offset (bottom_border_region,
                                 offset.width, offset.height);

              gdk_region_union (need_display_region, bottom_border_region);

              gdk_region_destroy (bottom_border_region);
            }
          else
            {
              /* If we cannot handle things with a scroll, we must redisplay
               * the union of the source area and the destination area.
               */
              gdk_region_union (need_display_region, offset_region);
            }

          _gdk_quartz_window_set_needs_display_in_region (window,
                                                          need_display_region);

          gdk_region_destroy (orig_region);
          gdk_region_destroy (offset_region);
          gdk_region_destroy (need_display_region);
        }
      else
        g_warning ("Drawing with window source != dest is not supported");

      return;
    }
  else if (GDK_IS_DRAWABLE_IMPL_QUARTZ (src))
    src_impl = GDK_DRAWABLE_IMPL_QUARTZ (src);
  else if (GDK_IS_PIXMAP (src))
    src_impl = GDK_DRAWABLE_IMPL_QUARTZ (GDK_PIXMAP_OBJECT (src)->impl);
  else
    {
      g_warning ("Unsupported source %s", G_OBJECT_TYPE_NAME (src));
      return;
    }

  /* Handle drawable and pixmap sources. */
  if (src_depth == 1)
    {
      /* FIXME: src depth 1 is not supported yet */
      g_warning ("Source with depth 1 unsupported");
    }
  else if (dest_depth != 0 && src_depth == dest_depth)
    {
      GdkPixmapImplQuartz *pixmap_impl = GDK_PIXMAP_IMPL_QUARTZ (src_impl);
      CGContextRef context = gdk_quartz_drawable_get_context (drawable, FALSE);
      CGImageRef image;

      if (!context)
        return;

      if (!_gdk_quartz_gc_update_cg_context (gc, drawable, context,
					     GDK_QUARTZ_CONTEXT_STROKE))
	{
	  gdk_quartz_drawable_release_context (drawable, context);
	  return;
	}
      CGContextClipToRect (context, CGRectMake (xdest, ydest, width, height));
      CGContextTranslateCTM (context, xdest - xsrc, ydest - ysrc +
                             pixmap_impl->height);
      CGContextScaleCTM (context, 1.0, -1.0);

      image = _gdk_pixmap_get_cgimage (src);
      CGContextDrawImage (context,
                          CGRectMake (0, 0, pixmap_impl->width, pixmap_impl->height),
                          image);
      CGImageRelease (image);

      gdk_quartz_drawable_release_context (drawable, context);
    }
  else
    g_warning ("Attempt to draw a drawable with depth %d to a drawable with depth %d",
               src_depth, dest_depth);
}

static void
gdk_quartz_draw_points (GdkDrawable *drawable,
			GdkGC       *gc,
			GdkPoint    *points,
			gint         npoints)
{
  CGContextRef context = gdk_quartz_drawable_get_context (drawable, FALSE);
  int i;

  if (!context)
    return;

  if (!_gdk_quartz_gc_update_cg_context (gc, drawable, context,
					 GDK_QUARTZ_CONTEXT_STROKE |
					 GDK_QUARTZ_CONTEXT_FILL))
    {
      gdk_quartz_drawable_release_context (drawable, context);
      return;
    }
  /* Just draw 1x1 rectangles */
  for (i = 0; i < npoints; i++) 
    {
      CGRect rect = CGRectMake (points[i].x, points[i].y, 1, 1);
      CGContextFillRect (context, rect);
    }

  gdk_quartz_drawable_release_context (drawable, context);
}

static inline void
gdk_quartz_fix_cap_not_last_line (GdkGCQuartz *private,
				  gint         x1,
				  gint         y1,
				  gint         x2,
				  gint         y2,
				  gint        *xfix,
				  gint        *yfix)
{
  *xfix = 0;
  *yfix = 0;

  if (private->cap_style == GDK_CAP_NOT_LAST && private->line_width == 0)
    {
      /* fix only vertical and horizontal lines for now */

      if (y1 == y2 && x1 != x2)
	{
	  *xfix = (x1 < x2) ? -1 : 1;
	}
      else if (x1 == x2 && y1 != y2)
	{
	  *yfix = (y1 < y2) ? -1 : 1;
	}
    }
}

static void
gdk_quartz_draw_segments (GdkDrawable    *drawable,
			  GdkGC          *gc,
			  GdkSegment     *segs,
			  gint            nsegs)
{
  CGContextRef context = gdk_quartz_drawable_get_context (drawable, FALSE);
  GdkGCQuartz *private;
  int i;

  if (!context)
    return;

  private = GDK_GC_QUARTZ (gc);

  if (!_gdk_quartz_gc_update_cg_context (gc, drawable, context,
					 GDK_QUARTZ_CONTEXT_STROKE))
    {
      gdk_quartz_drawable_release_context (drawable, context);
      return;
    }
  for (i = 0; i < nsegs; i++)
    {
      gint xfix, yfix;

      gdk_quartz_fix_cap_not_last_line (private,
					segs[i].x1, segs[i].y1,
					segs[i].x2, segs[i].y2,
					&xfix, &yfix);

      CGContextMoveToPoint (context, segs[i].x1 + 0.5, segs[i].y1 + 0.5);
      CGContextAddLineToPoint (context, segs[i].x2 + 0.5 + xfix, segs[i].y2 + 0.5 + yfix);
    }
  
  CGContextStrokePath (context);

  gdk_quartz_drawable_release_context (drawable, context);
}

static void
gdk_quartz_draw_lines (GdkDrawable *drawable,
		       GdkGC       *gc,
		       GdkPoint    *points,
		       gint         npoints)
{
  CGContextRef context = gdk_quartz_drawable_get_context (drawable, FALSE);
  GdkGCQuartz *private;
  gint xfix, yfix;
  gint i;

  if (!context)
    return;

  private = GDK_GC_QUARTZ (gc);

  if (!_gdk_quartz_gc_update_cg_context (gc, drawable, context,
					 GDK_QUARTZ_CONTEXT_STROKE))
    {
      gdk_quartz_drawable_release_context (drawable, context);
      return;
    }
  CGContextMoveToPoint (context, points[0].x + 0.5, points[0].y + 0.5);

  for (i = 1; i < npoints - 1; i++)
    CGContextAddLineToPoint (context, points[i].x + 0.5, points[i].y + 0.5);

  gdk_quartz_fix_cap_not_last_line (private,
				    points[npoints - 2].x, points[npoints - 2].y,
				    points[npoints - 1].x, points[npoints - 1].y,
				    &xfix, &yfix);

  CGContextAddLineToPoint (context,
			   points[npoints - 1].x + 0.5 + xfix,
			   points[npoints - 1].y + 0.5 + yfix);

  CGContextStrokePath (context);

  gdk_quartz_drawable_release_context (drawable, context);
}

static void
gdk_quartz_draw_pixbuf (GdkDrawable     *drawable,
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
  CGContextRef context = gdk_quartz_drawable_get_context (drawable, FALSE);
  CGColorSpaceRef colorspace;
  CGDataProviderRef data_provider;
  CGImageRef image;
  void *data;
  int rowstride, pixbuf_width, pixbuf_height;
  gboolean has_alpha;

  if (!context)
    return;

  pixbuf_width = gdk_pixbuf_get_width (pixbuf);
  pixbuf_height = gdk_pixbuf_get_height (pixbuf);
  rowstride = gdk_pixbuf_get_rowstride (pixbuf);
  has_alpha = gdk_pixbuf_get_has_alpha (pixbuf);

  data = gdk_pixbuf_get_pixels (pixbuf);

  colorspace = CGColorSpaceCreateDeviceRGB ();
  data_provider = CGDataProviderCreateWithData (NULL, data, pixbuf_height * rowstride, NULL);

  image = CGImageCreate (pixbuf_width, pixbuf_height, 8,
			 has_alpha ? 32 : 24, rowstride, 
			 colorspace, 
			 has_alpha ? kCGImageAlphaLast : 0,
			 data_provider, NULL, FALSE, 
			 kCGRenderingIntentDefault);

  CGDataProviderRelease (data_provider);
  CGColorSpaceRelease (colorspace);

  if (!_gdk_quartz_gc_update_cg_context (gc, drawable, context,
					 GDK_QUARTZ_CONTEXT_STROKE))
    {
      gdk_quartz_drawable_release_context (drawable, context);
      return;
    }
  CGContextClipToRect (context, CGRectMake (dest_x, dest_y, width, height));
  CGContextTranslateCTM (context, dest_x - src_x, dest_y - src_y + pixbuf_height);
  CGContextScaleCTM (context, 1, -1);

  CGContextDrawImage (context, CGRectMake (0, 0, pixbuf_width, pixbuf_height), image);
  CGImageRelease (image);

  gdk_quartz_drawable_release_context (drawable, context);
}

static void
gdk_quartz_draw_image (GdkDrawable     *drawable,
		       GdkGC           *gc,
		       GdkImage        *image,
		       gint             xsrc,
		       gint             ysrc,
		       gint             xdest,
		       gint             ydest,
		       gint             width,
		       gint             height)
{
  CGContextRef context = gdk_quartz_drawable_get_context (drawable, FALSE);
  CGColorSpaceRef colorspace;
  CGDataProviderRef data_provider;
  CGImageRef cgimage;

  if (!context)
    return;

  colorspace = CGColorSpaceCreateDeviceRGB ();
  data_provider = CGDataProviderCreateWithData (NULL, image->mem, image->height * image->bpl, NULL);

  /* FIXME: Make sure that this function draws 32-bit images correctly,
  * also check endianness wrt kCGImageAlphaNoneSkipFirst */
  cgimage = CGImageCreate (image->width, image->height, 8,
			   32, image->bpl,
			   colorspace,
			   kCGImageAlphaNoneSkipFirst, 
			   data_provider, NULL, FALSE, kCGRenderingIntentDefault);

  CGDataProviderRelease (data_provider);
  CGColorSpaceRelease (colorspace);

  if (!_gdk_quartz_gc_update_cg_context (gc, drawable, context,
					 GDK_QUARTZ_CONTEXT_STROKE))
    {
      gdk_quartz_drawable_release_context (drawable, context);
      return;
    }

  CGContextClipToRect (context, CGRectMake (xdest, ydest, width, height));
  CGContextTranslateCTM (context, xdest - xsrc, ydest - ysrc + image->height);
  CGContextScaleCTM (context, 1, -1);

  CGContextDrawImage (context, CGRectMake (0, 0, image->width, image->height), cgimage);
  CGImageRelease (cgimage);

  gdk_quartz_drawable_release_context (drawable, context);
}

static void
gdk_drawable_impl_quartz_finalize (GObject *object)
{
  GdkDrawableImplQuartz *impl = GDK_DRAWABLE_IMPL_QUARTZ (object);

  if (impl->colormap)
    g_object_unref (impl->colormap);

  G_OBJECT_CLASS (parent_class)->finalize (object);
}

static void
gdk_drawable_impl_quartz_class_init (GdkDrawableImplQuartzClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GdkDrawableClass *drawable_class = GDK_DRAWABLE_CLASS (klass);

  parent_class = g_type_class_peek_parent (klass);

  object_class->finalize = gdk_drawable_impl_quartz_finalize;

  drawable_class->create_gc = _gdk_quartz_gc_new;
  drawable_class->draw_rectangle = gdk_quartz_draw_rectangle;
  drawable_class->draw_arc = gdk_quartz_draw_arc;
  drawable_class->draw_polygon = gdk_quartz_draw_polygon;
  drawable_class->draw_text = gdk_quartz_draw_text;
  drawable_class->draw_text_wc = gdk_quartz_draw_text_wc;
  drawable_class->draw_drawable_with_src = gdk_quartz_draw_drawable;
  drawable_class->draw_points = gdk_quartz_draw_points;
  drawable_class->draw_segments = gdk_quartz_draw_segments;
  drawable_class->draw_lines = gdk_quartz_draw_lines;
  drawable_class->draw_image = gdk_quartz_draw_image;
  drawable_class->draw_pixbuf = gdk_quartz_draw_pixbuf;

  drawable_class->ref_cairo_surface = gdk_quartz_ref_cairo_surface;

  drawable_class->set_colormap = gdk_quartz_set_colormap;
  drawable_class->get_colormap = gdk_quartz_get_colormap;

  drawable_class->get_depth = gdk_quartz_get_depth;
  drawable_class->get_screen = gdk_quartz_get_screen;
  drawable_class->get_visual = gdk_quartz_get_visual;

  drawable_class->_copy_to_image = _gdk_quartz_image_copy_to_image;
}

GType
gdk_drawable_impl_quartz_get_type (void)
{
  static GType object_type = 0;

  if (!object_type)
    {
      const GTypeInfo object_info =
      {
        sizeof (GdkDrawableImplQuartzClass),
        (GBaseInitFunc) NULL,
        (GBaseFinalizeFunc) NULL,
        (GClassInitFunc) gdk_drawable_impl_quartz_class_init,
        NULL,           /* class_finalize */
        NULL,           /* class_data */
        sizeof (GdkDrawableImplQuartz),
        0,              /* n_preallocs */
        (GInstanceInitFunc) NULL,
      };
      
      object_type = g_type_register_static (GDK_TYPE_DRAWABLE,
                                            "GdkDrawableImplQuartz",
                                            &object_info, 0);
    }
  
  return object_type;
}

CGContextRef
gdk_quartz_drawable_get_context (GdkDrawable *drawable,
				 gboolean     antialias)
{
  if (!GDK_DRAWABLE_IMPL_QUARTZ_GET_CLASS (drawable)->get_context)
    {
      g_warning ("%s doesn't implement GdkDrawableImplQuartzClass::get_context()",
                 G_OBJECT_TYPE_NAME (drawable));
      return NULL;
    }

  return GDK_DRAWABLE_IMPL_QUARTZ_GET_CLASS (drawable)->get_context (drawable, antialias);
}

void
gdk_quartz_drawable_release_context (GdkDrawable  *drawable, 
				     CGContextRef  cg_context)
{
  if (!cg_context)
    return;
  
  if (GDK_IS_WINDOW_IMPL_QUARTZ (drawable))
    {
      GdkWindowImplQuartz *window_impl = GDK_WINDOW_IMPL_QUARTZ (drawable);

      CGContextRestoreGState (cg_context);
      CGContextSetAllowsAntialiasing (cg_context, TRUE);

      /* See comment in gdk_quartz_drawable_get_context(). */
      if (window_impl->in_paint_rect_count == 0)
        {
          [window_impl->view unlockFocus];
        }
    }
  else if (GDK_IS_PIXMAP_IMPL_QUARTZ (drawable))
    CGContextRelease (cg_context);
}

void
_gdk_quartz_drawable_finish (GdkDrawable *drawable)
{
  GdkDrawableImplQuartz *impl = GDK_DRAWABLE_IMPL_QUARTZ (drawable);

  if (impl->cairo_surface)
    {
      cairo_surface_finish (impl->cairo_surface);
      cairo_surface_set_user_data (impl->cairo_surface, &gdk_quartz_cairo_key,
				   NULL, NULL);
      impl->cairo_surface = NULL;
    }
}  
