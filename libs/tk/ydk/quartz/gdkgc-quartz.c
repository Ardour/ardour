/* gdkgc-quartz.c
 *
 * Copyright (C) 2005 Imendio AB
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

#include "gdkgc.h"
#include "gdkprivate-quartz.h"

static gpointer parent_class = NULL;

static void
gdk_quartz_gc_get_values (GdkGC       *gc,
			  GdkGCValues *values)
{
  GdkGCQuartz *private;

  private = GDK_GC_QUARTZ (gc);

  values->foreground.pixel = _gdk_gc_get_fg_pixel (gc);
  values->background.pixel = _gdk_gc_get_bg_pixel (gc);

  values->font = private->font;

  values->function = private->function;

  values->fill = _gdk_gc_get_fill (gc);
  values->tile = _gdk_gc_get_tile (gc);
  values->stipple = _gdk_gc_get_stipple (gc);
  
  /* The X11 backend always returns a NULL clip_mask. */
  values->clip_mask = NULL;

  values->ts_x_origin = gc->ts_x_origin;
  values->ts_y_origin = gc->ts_y_origin;
  values->clip_x_origin = gc->clip_x_origin;
  values->clip_y_origin = gc->clip_y_origin;

  values->graphics_exposures = private->graphics_exposures;

  values->line_width = private->line_width;
  values->line_style = private->line_style;
  values->cap_style = private->cap_style;
  values->join_style = private->join_style;
}


static void
data_provider_release (void *info, const void *data, size_t size)
{
  g_free (info);
}

static CGImageRef
create_clip_mask (GdkPixmap *source_pixmap)
{
  int width, height, bytes_per_row, bits_per_pixel;
  void *data;
  CGImageRef source;
  CGImageRef clip_mask;
  CGContextRef cg_context;
  CGDataProviderRef data_provider;

  /* We need to flip the clip mask here, because this cannot be done during
   * the drawing process when this mask will be used to do clipping.  We
   * quickly create a new CGImage, set up a CGContext, draw the source
   * image while flipping, and done.  If this appears too slow in the
   * future, we would look into doing this by hand on the actual raw
   * data.
   */
  source = _gdk_pixmap_get_cgimage (source_pixmap);

  width = CGImageGetWidth (source);
  height = CGImageGetHeight (source);
  bytes_per_row = CGImageGetBytesPerRow (source);
  bits_per_pixel = CGImageGetBitsPerPixel (source);

  data = g_malloc (height * bytes_per_row);
  data_provider = CGDataProviderCreateWithData (data, data,
                                                height * bytes_per_row,
                                                data_provider_release);

  clip_mask = CGImageCreate (width, height, 8,
                             bits_per_pixel,
                             bytes_per_row,
                             CGImageGetColorSpace (source),
                             CGImageGetAlphaInfo (source),
                             data_provider, NULL, FALSE,
                             kCGRenderingIntentDefault);
  CGDataProviderRelease (data_provider);

  cg_context = CGBitmapContextCreate (data,
                                      width, height,
                                      CGImageGetBitsPerComponent (source),
                                      bytes_per_row,
                                      CGImageGetColorSpace (source),
                                      CGImageGetBitmapInfo (source));

  if (cg_context)
    {
      CGContextTranslateCTM (cg_context, 0, height);
      CGContextScaleCTM (cg_context, 1.0, -1.0);

      CGContextDrawImage (cg_context,
                          CGRectMake (0, 0, width, height), source);

      CGContextRelease (cg_context);
    }

  return clip_mask;
}

static void
gdk_quartz_gc_set_values (GdkGC           *gc,
			  GdkGCValues     *values,
			  GdkGCValuesMask  mask)
{
  GdkGCQuartz *private = GDK_GC_QUARTZ (gc);

  if (mask & GDK_GC_FONT)
    {
      /* FIXME: implement font */
    }

  if (mask & GDK_GC_FUNCTION)
    private->function = values->function;

  if (mask & GDK_GC_SUBWINDOW)
    private->subwindow_mode = values->subwindow_mode;

  if (mask & GDK_GC_EXPOSURES)
    private->graphics_exposures = values->graphics_exposures;

  if (mask & GDK_GC_CLIP_MASK)
    {
      private->have_clip_region = FALSE;
      private->have_clip_mask = values->clip_mask != NULL;
      if (private->clip_mask)
	CGImageRelease (private->clip_mask);

      if (values->clip_mask)
        private->clip_mask = create_clip_mask (values->clip_mask);
      else
	private->clip_mask = NULL;
    }

  if (mask & GDK_GC_LINE_WIDTH)
    private->line_width = values->line_width;

  if (mask & GDK_GC_LINE_STYLE)
    private->line_style = values->line_style;

  if (mask & GDK_GC_CAP_STYLE)
    private->cap_style = values->cap_style;

  if (mask & GDK_GC_JOIN_STYLE)
    private->join_style = values->join_style;
}

static void
gdk_quartz_gc_set_dashes (GdkGC *gc,
			  gint   dash_offset,
			  gint8  dash_list[],
			  gint   n)
{
  GdkGCQuartz *private = GDK_GC_QUARTZ (gc);
  gint i;

  private->dash_count = n;
  g_free (private->dash_lengths);
  private->dash_lengths = g_new (CGFloat, n);
  for (i = 0; i < n; i++)
    private->dash_lengths[i] = (CGFloat) dash_list[i];
  private->dash_phase = (CGFloat) dash_offset;
}

static void
gdk_gc_quartz_finalize (GObject *object)
{
  GdkGCQuartz *private = GDK_GC_QUARTZ (object);

  if (private->clip_mask)
    CGImageRelease (private->clip_mask);

  if (private->ts_pattern)
    CGPatternRelease (private->ts_pattern);

  G_OBJECT_CLASS (parent_class)->finalize (object);
}

static void
gdk_gc_quartz_class_init (GdkGCQuartzClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GdkGCClass *gc_class = GDK_GC_CLASS (klass);
  
  parent_class = g_type_class_peek_parent (klass);

  object_class->finalize = gdk_gc_quartz_finalize;

  gc_class->get_values = gdk_quartz_gc_get_values;
  gc_class->set_values = gdk_quartz_gc_set_values;
  gc_class->set_dashes = gdk_quartz_gc_set_dashes;
}

static void
gdk_gc_quartz_init (GdkGCQuartz *gc_quartz)
{
  gc_quartz->function = GDK_COPY;
  gc_quartz->subwindow_mode = GDK_CLIP_BY_CHILDREN;
  gc_quartz->graphics_exposures = TRUE;
  gc_quartz->line_width = 0;
  gc_quartz->line_style = GDK_LINE_SOLID;
  gc_quartz->cap_style = GDK_CAP_BUTT;
  gc_quartz->join_style = GDK_JOIN_MITER;
}

GType
_gdk_gc_quartz_get_type (void)
{
  static GType object_type = 0;

  if (!object_type)
    {
      const GTypeInfo object_info =
      {
        sizeof (GdkGCQuartzClass),
        (GBaseInitFunc) NULL,
        (GBaseFinalizeFunc) NULL,
        (GClassInitFunc) gdk_gc_quartz_class_init,
        NULL,           /* class_finalize */
        NULL,           /* class_data */
        sizeof (GdkGCQuartz),
        0,              /* n_preallocs */
        (GInstanceInitFunc) gdk_gc_quartz_init,
      };
      
      object_type = g_type_register_static (GDK_TYPE_GC,
                                            "GdkGCQuartz",
                                            &object_info, 0);
    }
  
  return object_type;
}

GdkGC *
_gdk_quartz_gc_new (GdkDrawable      *drawable,
		    GdkGCValues      *values,
		    GdkGCValuesMask   values_mask)
{
  GdkGC *gc;

  gc = g_object_new (GDK_TYPE_GC_QUARTZ, NULL);

  _gdk_gc_init (gc, drawable, values, values_mask);

  gdk_quartz_gc_set_values (gc, values, values_mask);

  return gc;
}

void
_gdk_windowing_gc_set_clip_region (GdkGC           *gc,
				   const GdkRegion *region,
				   gboolean         reset_origin)
{
  GdkGCQuartz *private = GDK_GC_QUARTZ (gc);

  if ((private->have_clip_region && ! region) || private->have_clip_mask)
    {
      if (private->clip_mask)
	{
	  CGImageRelease (private->clip_mask);
	  private->clip_mask = NULL;
	}
      private->have_clip_mask = FALSE;
    }

  if (region == NULL || gdk_region_empty (region))
     private->have_clip_region = FALSE;
  else
     private->have_clip_region = TRUE;

  if (reset_origin)
    {
      gc->clip_x_origin = 0;
      gc->clip_y_origin = 0;
    }
}

void
_gdk_windowing_gc_copy (GdkGC *dst_gc,
			GdkGC *src_gc)
{
  GdkGCQuartz *dst_quartz_gc = GDK_GC_QUARTZ (dst_gc);
  GdkGCQuartz *src_quartz_gc = GDK_GC_QUARTZ (src_gc);

  if (dst_quartz_gc->font)
    gdk_font_unref (dst_quartz_gc->font);
  dst_quartz_gc->font = src_quartz_gc->font;
  if (dst_quartz_gc->font)
    gdk_font_ref (dst_quartz_gc->font);

  dst_quartz_gc->function = src_quartz_gc->function;
  dst_quartz_gc->subwindow_mode = src_quartz_gc->subwindow_mode;
  dst_quartz_gc->graphics_exposures = src_quartz_gc->graphics_exposures;

  dst_quartz_gc->have_clip_region = src_quartz_gc->have_clip_region;
  dst_quartz_gc->have_clip_mask = src_quartz_gc->have_clip_mask;

  if (dst_quartz_gc->clip_mask)
    {
      CGImageRelease (dst_quartz_gc->clip_mask);
      dst_quartz_gc->clip_mask = NULL;
    }
  
  if (src_quartz_gc->clip_mask)
    dst_quartz_gc->clip_mask = _gdk_pixmap_get_cgimage (GDK_PIXMAP (src_quartz_gc->clip_mask));

  dst_quartz_gc->line_width = src_quartz_gc->line_width;
  dst_quartz_gc->line_style = src_quartz_gc->line_style;
  dst_quartz_gc->cap_style = src_quartz_gc->cap_style;
  dst_quartz_gc->join_style = src_quartz_gc->join_style;

  g_free (dst_quartz_gc->dash_lengths);
  dst_quartz_gc->dash_lengths = g_memdup (src_quartz_gc->dash_lengths,
					  sizeof (CGFloat) * src_quartz_gc->dash_count);
  dst_quartz_gc->dash_count = src_quartz_gc->dash_count;
  dst_quartz_gc->dash_phase = src_quartz_gc->dash_phase;
}

GdkScreen *  
gdk_gc_get_screen (GdkGC *gc)
{
  return _gdk_screen;
}

struct PatternCallbackInfo
{
  GdkGCQuartz *private_gc;
  GdkDrawable *drawable;
};

static void
pattern_callback_info_release (void *info)
{
  g_free (info);
}

static void
gdk_quartz_draw_tiled_pattern (void         *info,
			       CGContextRef  context)
{
  struct PatternCallbackInfo *pinfo = info;
  GdkGC       *gc = GDK_GC (pinfo->private_gc);
  CGImageRef   pattern_image;
  size_t       width, height;

  if (!context)
    return;
  
  pattern_image = _gdk_pixmap_get_cgimage (GDK_PIXMAP (_gdk_gc_get_tile (gc)));

  width = CGImageGetWidth (pattern_image);
  height = CGImageGetHeight (pattern_image);

  CGContextDrawImage (context, 
		      CGRectMake (0, 0, width, height),
		      pattern_image);
  CGImageRelease (pattern_image);
}

static void
gdk_quartz_draw_stippled_pattern (void         *info,
				  CGContextRef  context)
{
  struct PatternCallbackInfo *pinfo = info;
  GdkGC      *gc = GDK_GC (pinfo->private_gc);
  CGImageRef  pattern_image;
  CGRect      rect;
  CGColorRef  color;

  if (!context)
    return;

  pattern_image = _gdk_pixmap_get_cgimage (GDK_PIXMAP (_gdk_gc_get_stipple (gc)));
  rect = CGRectMake (0, 0,
		     CGImageGetWidth (pattern_image),
		     CGImageGetHeight (pattern_image));

  CGContextClipToMask (context, rect, pattern_image);
  color = _gdk_quartz_colormap_get_cgcolor_from_pixel (pinfo->drawable,
                                                       _gdk_gc_get_fg_pixel (gc));
  CGContextSetFillColorWithColor (context, color);
  CGColorRelease (color);

  CGContextFillRect (context, rect);

  CGImageRelease (pattern_image);
}

static void
gdk_quartz_draw_opaque_stippled_pattern (void         *info,
					 CGContextRef  context)
{
  struct PatternCallbackInfo *pinfo = info;
  GdkGC      *gc = GDK_GC (pinfo->private_gc);
  CGImageRef  pattern_image;
  CGRect      rect;
  CGColorRef  color;

  if (!context)
    return;

  pattern_image = _gdk_pixmap_get_cgimage (GDK_PIXMAP (_gdk_gc_get_stipple (gc)));
  rect = CGRectMake (0, 0,
		     CGImageGetWidth (pattern_image),
		     CGImageGetHeight (pattern_image));

  color = _gdk_quartz_colormap_get_cgcolor_from_pixel (pinfo->drawable,
                                                       _gdk_gc_get_bg_pixel (gc));
  CGContextSetFillColorWithColor (context, color);
  CGColorRelease (color);

  CGContextFillRect (context, rect);

  CGContextClipToMask (context, rect, pattern_image);
  color = _gdk_quartz_colormap_get_cgcolor_from_pixel (pinfo->drawable,
                                                       _gdk_gc_get_fg_pixel (gc));
  CGContextSetFillColorWithColor (context, color);
  CGColorRelease (color);

  CGContextFillRect (context, rect);

  CGImageRelease (pattern_image);
}

gboolean
_gdk_quartz_gc_update_cg_context (GdkGC                      *gc,
				  GdkDrawable                *drawable,
				  CGContextRef                context,
				  GdkQuartzContextValuesMask  mask)
{
  GdkGCQuartz *private;
  guint32      fg_pixel;
  guint32      bg_pixel;

  g_return_val_if_fail (gc == NULL || GDK_IS_GC (gc), FALSE);

  if (!gc || !context)
    return FALSE;

  private = GDK_GC_QUARTZ (gc);

  if (private->have_clip_region)
    {
      CGRect rect;
      CGRect *cg_rects;
      GdkRectangle *rects;
      gint n_rects, i;

      gdk_region_get_rectangles (_gdk_gc_get_clip_region (gc),
				 &rects, &n_rects);

      if (!n_rects)
	  return FALSE;

      if (n_rects == 1)
	cg_rects = &rect;
      else
	cg_rects = g_new (CGRect, n_rects);

      for (i = 0; i < n_rects; i++)
	{
	  cg_rects[i].origin.x = rects[i].x + gc->clip_x_origin;
	  cg_rects[i].origin.y = rects[i].y + gc->clip_y_origin;
	  cg_rects[i].size.width = rects[i].width;
	  cg_rects[i].size.height = rects[i].height;
	}

      CGContextClipToRects (context, cg_rects, n_rects);

      g_free (rects);
      if (cg_rects != &rect)
	g_free (cg_rects);
    }
  else if (private->have_clip_mask && private->clip_mask)
    {
      /* Note: This is 10.4 only. For lower versions, we need to transform the
       * mask into a region.
       */
      CGContextClipToMask (context,
			   CGRectMake (gc->clip_x_origin, gc->clip_y_origin,
				       CGImageGetWidth (private->clip_mask),
				       CGImageGetHeight (private->clip_mask)),
			   private->clip_mask);
    }

  fg_pixel = _gdk_gc_get_fg_pixel (gc);
  bg_pixel = _gdk_gc_get_bg_pixel (gc);

  {
    CGBlendMode blend_mode = kCGBlendModeNormal;

    switch (private->function)
      {
      case GDK_COPY:
	blend_mode = kCGBlendModeNormal;
	break;

      case GDK_INVERT:
      case GDK_XOR:
	blend_mode = kCGBlendModeExclusion;
	fg_pixel = 0xffffffff;
	bg_pixel = 0xffffffff;
	break;

      case GDK_CLEAR:
      case GDK_AND:
      case GDK_AND_REVERSE:
      case GDK_AND_INVERT:
      case GDK_NOOP:
      case GDK_OR:
      case GDK_EQUIV:
      case GDK_OR_REVERSE:
      case GDK_COPY_INVERT:
      case GDK_OR_INVERT:
      case GDK_NAND:
      case GDK_NOR:
      case GDK_SET:
	blend_mode = kCGBlendModeNormal; /* FIXME */
	break;
      }

    CGContextSetBlendMode (context, blend_mode);
  }

  /* FIXME: implement subwindow mode */

  /* FIXME: implement graphics exposures */

  if (mask & GDK_QUARTZ_CONTEXT_STROKE)
    {
      CGLineCap  line_cap  = kCGLineCapButt;
      CGLineJoin line_join = kCGLineJoinMiter;
      CGColorRef color;

      color = _gdk_quartz_colormap_get_cgcolor_from_pixel (drawable,
                                                           fg_pixel);
      CGContextSetStrokeColorWithColor (context, color);
      CGColorRelease (color);

      CGContextSetLineWidth (context, MAX (1.0, private->line_width));

      switch (private->line_style)
	{
	case GDK_LINE_SOLID:
	  CGContextSetLineDash (context, 0.0, NULL, 0);
	  break;

	case GDK_LINE_DOUBLE_DASH:
	  /* FIXME: Implement; for now, fall back to GDK_LINE_ON_OFF_DASH */

	case GDK_LINE_ON_OFF_DASH:
	  CGContextSetLineDash (context, private->dash_phase,
				private->dash_lengths, private->dash_count);
	  break;
	}

      switch (private->cap_style)
        {
	case GDK_CAP_NOT_LAST:
	  /* FIXME: Implement; for now, fall back to GDK_CAP_BUTT */
	case GDK_CAP_BUTT:
	  line_cap = kCGLineCapButt;
	  break;
	case GDK_CAP_ROUND:
	  line_cap = kCGLineCapRound;
	  break;
	case GDK_CAP_PROJECTING:
	  line_cap = kCGLineCapSquare;
	  break;
	}

      CGContextSetLineCap (context, line_cap);

      switch (private->join_style)
        {
	case GDK_JOIN_MITER:
	  line_join = kCGLineJoinMiter;
	  break;
	case GDK_JOIN_ROUND:
	  line_join = kCGLineJoinRound;
	  break;
	case GDK_JOIN_BEVEL:
	  line_join = kCGLineJoinBevel;
	  break;
	}

      CGContextSetLineJoin (context, line_join);
    }

  if (mask & GDK_QUARTZ_CONTEXT_FILL)
    {
      GdkFill         fill = _gdk_gc_get_fill (gc);
      CGColorSpaceRef baseSpace;
      CGColorSpaceRef patternSpace;
      CGFloat         alpha     = 1.0;

      if (fill == GDK_SOLID)
	{
          CGColorRef color;

	  color = _gdk_quartz_colormap_get_cgcolor_from_pixel (drawable,
                                                               fg_pixel);
	  CGContextSetFillColorWithColor (context, color);
          CGColorRelease (color);
	}
      else
	{
          struct PatternCallbackInfo *info;

	  if (!private->ts_pattern)
	    {
	      gfloat     width, height;
	      gboolean   is_colored = FALSE;
	      CGPatternCallbacks callbacks =  { 0, NULL, NULL };
	      CGPoint    phase;
              GdkPixmapImplQuartz *pix_impl = NULL;

              info = g_new (struct PatternCallbackInfo, 1);
              private->ts_pattern_info = info;

              /* Won't ref to avoid circular dependencies */
              info->drawable = drawable;
              info->private_gc = private;

              callbacks.releaseInfo = pattern_callback_info_release;

	      switch (fill)
		{
		case GDK_TILED:
		  pix_impl = GDK_PIXMAP_IMPL_QUARTZ (GDK_PIXMAP_OBJECT (_gdk_gc_get_tile (gc))->impl);
		  width = pix_impl->width;
		  height = pix_impl->height;
		  is_colored = TRUE;
		  callbacks.drawPattern = gdk_quartz_draw_tiled_pattern;
		  break;
		case GDK_STIPPLED:
		  pix_impl = GDK_PIXMAP_IMPL_QUARTZ (GDK_PIXMAP_OBJECT (_gdk_gc_get_stipple (gc))->impl);
		  width = pix_impl->width;
		  height = pix_impl->height;
		  is_colored = FALSE;
		  callbacks.drawPattern = gdk_quartz_draw_stippled_pattern;
		  break;
		case GDK_OPAQUE_STIPPLED:
		  pix_impl = GDK_PIXMAP_IMPL_QUARTZ (GDK_PIXMAP_OBJECT (_gdk_gc_get_stipple (gc))->impl);
		  width = pix_impl->width;
		  height = pix_impl->height;
		  is_colored = TRUE;
		  callbacks.drawPattern = gdk_quartz_draw_opaque_stippled_pattern;
		  break;
		default:
		  break;
		}

	      phase = CGPointApplyAffineTransform (CGPointMake (gc->ts_x_origin, gc->ts_y_origin), CGContextGetCTM (context));
	      CGContextSetPatternPhase (context, CGSizeMake (phase.x, phase.y));

	      private->ts_pattern = CGPatternCreate (info,
						     CGRectMake (0, 0, width, height),
						     CGAffineTransformIdentity,
						     width, height,
						     kCGPatternTilingConstantSpacing,
						     is_colored,
						     &callbacks);
	    }
          else
            info = (struct PatternCallbackInfo *)private->ts_pattern_info;

          /* Update drawable in the pattern callback info.  Again, we
           * won't ref to avoid circular dependencies.
           */
          info->drawable = drawable;

	  baseSpace = (fill == GDK_STIPPLED) ? CGColorSpaceCreateDeviceRGB () : NULL;
	  patternSpace = CGColorSpaceCreatePattern (baseSpace);

	  CGContextSetFillColorSpace (context, patternSpace);
	  CGColorSpaceRelease (patternSpace);
	  CGColorSpaceRelease (baseSpace);

	  if (fill == GDK_STIPPLED)
            {
              CGColorRef color;
              const CGFloat *components;

              color = _gdk_quartz_colormap_get_cgcolor_from_pixel (drawable,
                                                                   fg_pixel);
              components = CGColorGetComponents (color);

              CGContextSetFillPattern (context, private->ts_pattern,
                                       components);
              CGColorRelease (color);
            }
          else
            CGContextSetFillPattern (context, private->ts_pattern, &alpha);
       }
    }

  if (mask & GDK_QUARTZ_CONTEXT_TEXT)
    {
      /* FIXME: implement text */
    }

  if (GDK_IS_WINDOW_IMPL_QUARTZ (drawable))
      private->is_window = TRUE;
  else
      private->is_window = FALSE;

  return TRUE;
}
