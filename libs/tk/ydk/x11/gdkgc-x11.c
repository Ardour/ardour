/* GDK - The GIMP Drawing Kit
 * Copyright (C) 1995-1997 Peter Mattis, Spencer Kimball and Josh MacDonald
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

#include "gdkgc.h"
#include "gdkprivate-x11.h"
#include "gdkregion-generic.h"
#include "gdkx.h"
#include "gdkalias.h"

#include <string.h>

typedef enum {
  GDK_GC_DIRTY_CLIP = 1 << 0,
  GDK_GC_DIRTY_TS = 1 << 1
} GdkGCDirtyValues;

static void gdk_x11_gc_values_to_xvalues (GdkGCValues    *values,
					  GdkGCValuesMask mask,
					  XGCValues      *xvalues,
					  unsigned long  *xvalues_mask);

static void gdk_x11_gc_get_values (GdkGC           *gc,
				   GdkGCValues     *values);
static void gdk_x11_gc_set_values (GdkGC           *gc,
				   GdkGCValues     *values,
				   GdkGCValuesMask  values_mask);
static void gdk_x11_gc_set_dashes (GdkGC           *gc,
				   gint             dash_offset,
				   gint8            dash_list[],
				   gint             n);

static void gdk_gc_x11_finalize   (GObject         *object);

G_DEFINE_TYPE (GdkGCX11, _gdk_gc_x11, GDK_TYPE_GC)

static void
_gdk_gc_x11_class_init (GdkGCX11Class *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GdkGCClass *gc_class = GDK_GC_CLASS (klass);
  
  object_class->finalize = gdk_gc_x11_finalize;

  gc_class->get_values = gdk_x11_gc_get_values;
  gc_class->set_values = gdk_x11_gc_set_values;
  gc_class->set_dashes = gdk_x11_gc_set_dashes;
}

static void
_gdk_gc_x11_init (GdkGCX11 *gc)
{
}

static void
gdk_gc_x11_finalize (GObject *object)
{
  GdkGCX11 *x11_gc = GDK_GC_X11 (object);
  
  XFreeGC (GDK_GC_XDISPLAY (x11_gc), GDK_GC_XGC (x11_gc));

  G_OBJECT_CLASS (_gdk_gc_x11_parent_class)->finalize (object);
}


GdkGC *
_gdk_x11_gc_new (GdkDrawable      *drawable,
		 GdkGCValues      *values,
		 GdkGCValuesMask   values_mask)
{
  GdkGC *gc;
  GdkGCX11 *private;
  
  XGCValues xvalues;
  unsigned long xvalues_mask;

  /* NOTICE that the drawable here has to be the impl drawable,
   * not the publically-visible drawables.
   */
  g_return_val_if_fail (GDK_IS_DRAWABLE_IMPL_X11 (drawable), NULL);

  gc = g_object_new (_gdk_gc_x11_get_type (), NULL);
  private = GDK_GC_X11 (gc);

  _gdk_gc_init (gc, drawable, values, values_mask);

  private->dirty_mask = 0;
  private->have_clip_mask = FALSE;
    
  private->screen = GDK_DRAWABLE_IMPL_X11 (drawable)->screen;

  private->depth = gdk_drawable_get_depth (drawable);

  if (values_mask & (GDK_GC_CLIP_X_ORIGIN | GDK_GC_CLIP_Y_ORIGIN))
    {
      values_mask &= ~(GDK_GC_CLIP_X_ORIGIN | GDK_GC_CLIP_Y_ORIGIN);
      private->dirty_mask |= GDK_GC_DIRTY_CLIP;
    }

  if (values_mask & (GDK_GC_TS_X_ORIGIN | GDK_GC_TS_Y_ORIGIN))
    {
      values_mask &= ~(GDK_GC_TS_X_ORIGIN | GDK_GC_TS_Y_ORIGIN);
      private->dirty_mask |= GDK_GC_DIRTY_TS;
    }

  if ((values_mask & GDK_GC_CLIP_MASK) && values->clip_mask)
    private->have_clip_mask = TRUE;

  xvalues.function = GXcopy;
  xvalues.fill_style = FillSolid;
  xvalues.arc_mode = ArcPieSlice;
  xvalues.subwindow_mode = ClipByChildren;
  xvalues.graphics_exposures = False;
  xvalues_mask = GCFunction | GCFillStyle | GCArcMode | GCSubwindowMode | GCGraphicsExposures;

  gdk_x11_gc_values_to_xvalues (values, values_mask, &xvalues, &xvalues_mask);
  
  private->xgc = XCreateGC (GDK_GC_XDISPLAY (gc),
                            GDK_DRAWABLE_IMPL_X11 (drawable)->xid,
                            xvalues_mask, &xvalues);

  return gc;
}

GC
_gdk_x11_gc_flush (GdkGC *gc)
{
  Display *xdisplay = GDK_GC_XDISPLAY (gc);
  GdkGCX11 *private = GDK_GC_X11 (gc);
  GC xgc = private->xgc;

  if (private->dirty_mask & GDK_GC_DIRTY_CLIP)
    {
      GdkRegion *clip_region = _gdk_gc_get_clip_region (gc);
      
      if (!clip_region)
	XSetClipOrigin (xdisplay, xgc,
			gc->clip_x_origin, gc->clip_y_origin);
      else
	{
	  XRectangle *rectangles;
          gint n_rects;

          _gdk_region_get_xrectangles (clip_region,
                                       gc->clip_x_origin,
                                       gc->clip_y_origin,
                                       &rectangles,
                                       &n_rects);
	  
	  XSetClipRectangles (xdisplay, xgc, 0, 0,
                              rectangles,
                              n_rects, YXBanded);
          
	  g_free (rectangles);
	}
    }

  if (private->dirty_mask & GDK_GC_DIRTY_TS)
    {
      XSetTSOrigin (xdisplay, xgc,
		    gc->ts_x_origin, gc->ts_y_origin);
    }

  private->dirty_mask = 0;
  return xgc;
}

static void
gdk_x11_gc_get_values (GdkGC       *gc,
		       GdkGCValues *values)
{
  XGCValues xvalues;
  
  if (XGetGCValues (GDK_GC_XDISPLAY (gc), GDK_GC_XGC (gc),
		    GCForeground | GCBackground | GCFont |
		    GCFunction | GCTile | GCStipple | /* GCClipMask | */
		    GCSubwindowMode | GCGraphicsExposures |
		    GCTileStipXOrigin | GCTileStipYOrigin |
		    GCClipXOrigin | GCClipYOrigin |
		    GCLineWidth | GCLineStyle | GCCapStyle |
		    GCFillStyle | GCJoinStyle, &xvalues))
    {
      values->foreground.pixel = xvalues.foreground;
      values->background.pixel = xvalues.background;
      values->font = gdk_font_lookup_for_display (GDK_GC_DISPLAY (gc),
						  xvalues.font);

      switch (xvalues.function)
	{
	case GXcopy:
	  values->function = GDK_COPY;
	  break;
	case GXinvert:
	  values->function = GDK_INVERT;
	  break;
	case GXxor:
	  values->function = GDK_XOR;
	  break;
	case GXclear:
	  values->function = GDK_CLEAR;
	  break;
	case GXand:
	  values->function = GDK_AND;
	  break;
	case GXandReverse:
	  values->function = GDK_AND_REVERSE;
	  break;
	case GXandInverted:
	  values->function = GDK_AND_INVERT;
	  break;
	case GXnoop:
	  values->function = GDK_NOOP;
	  break;
	case GXor:
	  values->function = GDK_OR;
	  break;
	case GXequiv:
	  values->function = GDK_EQUIV;
	  break;
	case GXorReverse:
	  values->function = GDK_OR_REVERSE;
	  break;
	case GXcopyInverted:
	  values->function =GDK_COPY_INVERT;
	  break;
	case GXorInverted:
	  values->function = GDK_OR_INVERT;
	  break;
	case GXnand:
	  values->function = GDK_NAND;
	  break;
	case GXset:
	  values->function = GDK_SET;
	  break;
	case GXnor:
	  values->function = GDK_NOR;
	  break;
	}

      switch (xvalues.fill_style)
	{
	case FillSolid:
	  values->fill = GDK_SOLID;
	  break;
	case FillTiled:
	  values->fill = GDK_TILED;
	  break;
	case FillStippled:
	  values->fill = GDK_STIPPLED;
	  break;
	case FillOpaqueStippled:
	  values->fill = GDK_OPAQUE_STIPPLED;
	  break;
	}

      values->tile = gdk_pixmap_lookup_for_display (GDK_GC_DISPLAY (gc),
						    xvalues.tile);
      values->stipple = gdk_pixmap_lookup_for_display (GDK_GC_DISPLAY (gc),
						       xvalues.stipple);
      values->clip_mask = NULL;
      values->subwindow_mode = xvalues.subwindow_mode;
      values->ts_x_origin = xvalues.ts_x_origin;
      values->ts_y_origin = xvalues.ts_y_origin;
      values->clip_x_origin = xvalues.clip_x_origin;
      values->clip_y_origin = xvalues.clip_y_origin;
      values->graphics_exposures = xvalues.graphics_exposures;
      values->line_width = xvalues.line_width;

      switch (xvalues.line_style)
	{
	case LineSolid:
	  values->line_style = GDK_LINE_SOLID;
	  break;
	case LineOnOffDash:
	  values->line_style = GDK_LINE_ON_OFF_DASH;
	  break;
	case LineDoubleDash:
	  values->line_style = GDK_LINE_DOUBLE_DASH;
	  break;
	}

      switch (xvalues.cap_style)
	{
	case CapNotLast:
	  values->cap_style = GDK_CAP_NOT_LAST;
	  break;
	case CapButt:
	  values->cap_style = GDK_CAP_BUTT;
	  break;
	case CapRound:
	  values->cap_style = GDK_CAP_ROUND;
	  break;
	case CapProjecting:
	  values->cap_style = GDK_CAP_PROJECTING;
	  break;
	}

      switch (xvalues.join_style)
	{
	case JoinMiter:
	  values->join_style = GDK_JOIN_MITER;
	  break;
	case JoinRound:
	  values->join_style = GDK_JOIN_ROUND;
	  break;
	case JoinBevel:
	  values->join_style = GDK_JOIN_BEVEL;
	  break;
	}
    }
  else
    {
      memset (values, 0, sizeof (GdkGCValues));
    }
}

static void
gdk_x11_gc_set_values (GdkGC           *gc,
		       GdkGCValues     *values,
		       GdkGCValuesMask  values_mask)
{
  GdkGCX11 *x11_gc;
  XGCValues xvalues;
  unsigned long xvalues_mask = 0;

  x11_gc = GDK_GC_X11 (gc);

  if (values_mask & (GDK_GC_CLIP_X_ORIGIN | GDK_GC_CLIP_Y_ORIGIN))
    {
      values_mask &= ~(GDK_GC_CLIP_X_ORIGIN | GDK_GC_CLIP_Y_ORIGIN);
      x11_gc->dirty_mask |= GDK_GC_DIRTY_CLIP;
    }

  if (values_mask & (GDK_GC_TS_X_ORIGIN | GDK_GC_TS_Y_ORIGIN))
    {
      values_mask &= ~(GDK_GC_TS_X_ORIGIN | GDK_GC_TS_Y_ORIGIN);
      x11_gc->dirty_mask |= GDK_GC_DIRTY_TS;
    }

  if (values_mask & GDK_GC_CLIP_MASK)
    {
      x11_gc->have_clip_region = FALSE;
      x11_gc->have_clip_mask = values->clip_mask != NULL;
    }

  gdk_x11_gc_values_to_xvalues (values, values_mask, &xvalues, &xvalues_mask);

  XChangeGC (GDK_GC_XDISPLAY (gc),
	     GDK_GC_XGC (gc),
	     xvalues_mask,
	     &xvalues);
}

static void
gdk_x11_gc_set_dashes (GdkGC *gc,
		       gint   dash_offset,
		       gint8  dash_list[],
		       gint   n)
{
  g_return_if_fail (GDK_IS_GC (gc));
  g_return_if_fail (dash_list != NULL);

  XSetDashes (GDK_GC_XDISPLAY (gc), GDK_GC_XGC (gc),
	      dash_offset, (char *)dash_list, n);
}

static void
gdk_x11_gc_values_to_xvalues (GdkGCValues    *values,
			      GdkGCValuesMask mask,
			      XGCValues      *xvalues,
			      unsigned long  *xvalues_mask)
{
  /* Optimization for the common case (gdk_gc_new()) */
  if (values == NULL || mask == 0)
    return;
  
  if (mask & GDK_GC_FOREGROUND)
    {
      xvalues->foreground = values->foreground.pixel;
      *xvalues_mask |= GCForeground;
    }
  if (mask & GDK_GC_BACKGROUND)
    {
      xvalues->background = values->background.pixel;
      *xvalues_mask |= GCBackground;
    }
  if ((mask & GDK_GC_FONT) && (values->font->type == GDK_FONT_FONT))
    {
      xvalues->font = ((XFontStruct *) (GDK_FONT_XFONT (values->font)))->fid;
      *xvalues_mask |= GCFont;
    }
  if (mask & GDK_GC_FUNCTION)
    {
      switch (values->function)
	{
	case GDK_COPY:
	  xvalues->function = GXcopy;
	  break;
	case GDK_INVERT:
	  xvalues->function = GXinvert;
	  break;
	case GDK_XOR:
	  xvalues->function = GXxor;
	  break;
	case GDK_CLEAR:
	  xvalues->function = GXclear;
	  break;
	case GDK_AND:
	  xvalues->function = GXand;
	  break;
	case GDK_AND_REVERSE:
	  xvalues->function = GXandReverse;
	  break;
	case GDK_AND_INVERT:
	  xvalues->function = GXandInverted;
	  break;
	case GDK_NOOP:
	  xvalues->function = GXnoop;
	  break;
	case GDK_OR:
	  xvalues->function = GXor;
	  break;
	case GDK_EQUIV:
	  xvalues->function = GXequiv;
	  break;
	case GDK_OR_REVERSE:
	  xvalues->function = GXorReverse;
	  break;
	case GDK_COPY_INVERT:
	  xvalues->function = GXcopyInverted;
	  break;
	case GDK_OR_INVERT:
	  xvalues->function = GXorInverted;
	  break;
	case GDK_NAND:
	  xvalues->function = GXnand;
	  break;
	case GDK_SET:
	  xvalues->function = GXset;
	  break;
	case GDK_NOR:
	  xvalues->function = GXnor;
	  break;
	}
      *xvalues_mask |= GCFunction;
    }
  if (mask & GDK_GC_FILL)
    {
      switch (values->fill)
	{
	case GDK_SOLID:
	  xvalues->fill_style = FillSolid;
	  break;
	case GDK_TILED:
	  xvalues->fill_style = FillTiled;
	  break;
	case GDK_STIPPLED:
	  xvalues->fill_style = FillStippled;
	  break;
	case GDK_OPAQUE_STIPPLED:
	  xvalues->fill_style = FillOpaqueStippled;
	  break;
	}
      *xvalues_mask |= GCFillStyle;
    }
  if (mask & GDK_GC_TILE)
    {
      if (values->tile)
	xvalues->tile = GDK_DRAWABLE_XID (values->tile);
      else
	xvalues->tile = None;
      
      *xvalues_mask |= GCTile;
    }
  if (mask & GDK_GC_STIPPLE)
    {
      if (values->stipple)
	xvalues->stipple = GDK_DRAWABLE_XID (values->stipple);
      else
	xvalues->stipple = None;
      
      *xvalues_mask |= GCStipple;
    }
  if (mask & GDK_GC_CLIP_MASK)
    {
      if (values->clip_mask)
	xvalues->clip_mask = GDK_DRAWABLE_XID (values->clip_mask);
      else
	xvalues->clip_mask = None;

      *xvalues_mask |= GCClipMask;
      
    }
  if (mask & GDK_GC_SUBWINDOW)
    {
      xvalues->subwindow_mode = values->subwindow_mode;
      *xvalues_mask |= GCSubwindowMode;
    }
  if (mask & GDK_GC_TS_X_ORIGIN)
    {
      xvalues->ts_x_origin = values->ts_x_origin;
      *xvalues_mask |= GCTileStipXOrigin;
    }
  if (mask & GDK_GC_TS_Y_ORIGIN)
    {
      xvalues->ts_y_origin = values->ts_y_origin;
      *xvalues_mask |= GCTileStipYOrigin;
    }
  if (mask & GDK_GC_CLIP_X_ORIGIN)
    {
      xvalues->clip_x_origin = values->clip_x_origin;
      *xvalues_mask |= GCClipXOrigin;
    }
  if (mask & GDK_GC_CLIP_Y_ORIGIN)
    {
      xvalues->clip_y_origin = values->clip_y_origin;
      *xvalues_mask |= GCClipYOrigin;
    }

  if (mask & GDK_GC_EXPOSURES)
    {
      xvalues->graphics_exposures = values->graphics_exposures;
      *xvalues_mask |= GCGraphicsExposures;
    }

  if (mask & GDK_GC_LINE_WIDTH)
    {
      xvalues->line_width = values->line_width;
      *xvalues_mask |= GCLineWidth;
    }
  if (mask & GDK_GC_LINE_STYLE)
    {
      switch (values->line_style)
	{
	case GDK_LINE_SOLID:
	  xvalues->line_style = LineSolid;
	  break;
	case GDK_LINE_ON_OFF_DASH:
	  xvalues->line_style = LineOnOffDash;
	  break;
	case GDK_LINE_DOUBLE_DASH:
	  xvalues->line_style = LineDoubleDash;
	  break;
	}
      *xvalues_mask |= GCLineStyle;
    }
  if (mask & GDK_GC_CAP_STYLE)
    {
      switch (values->cap_style)
	{
	case GDK_CAP_NOT_LAST:
	  xvalues->cap_style = CapNotLast;
	  break;
	case GDK_CAP_BUTT:
	  xvalues->cap_style = CapButt;
	  break;
	case GDK_CAP_ROUND:
	  xvalues->cap_style = CapRound;
	  break;
	case GDK_CAP_PROJECTING:
	  xvalues->cap_style = CapProjecting;
	  break;
	}
      *xvalues_mask |= GCCapStyle;
    }
  if (mask & GDK_GC_JOIN_STYLE)
    {
      switch (values->join_style)
	{
	case GDK_JOIN_MITER:
	  xvalues->join_style = JoinMiter;
	  break;
	case GDK_JOIN_ROUND:
	  xvalues->join_style = JoinRound;
	  break;
	case GDK_JOIN_BEVEL:
	  xvalues->join_style = JoinBevel;
	  break;
	}
      *xvalues_mask |= GCJoinStyle;
    }

}

void
_gdk_windowing_gc_set_clip_region (GdkGC           *gc,
				   const GdkRegion *region,
				   gboolean reset_origin)
{
  GdkGCX11 *x11_gc = GDK_GC_X11 (gc);

  /* Unset immediately, to make sure Xlib doesn't keep the
   * XID of an old clip mask cached
   */
  if ((x11_gc->have_clip_region && !region) || x11_gc->have_clip_mask)
    {
      XSetClipMask (GDK_GC_XDISPLAY (gc), GDK_GC_XGC (gc), None);
      x11_gc->have_clip_mask = FALSE;
    }

  x11_gc->have_clip_region = region != NULL;

  if (reset_origin)
    {
      gc->clip_x_origin = 0;
      gc->clip_y_origin = 0;
    }

  x11_gc->dirty_mask |= GDK_GC_DIRTY_CLIP;
}

void
_gdk_windowing_gc_copy (GdkGC *dst_gc,
			GdkGC *src_gc)
{
  GdkGCX11 *x11_src_gc = GDK_GC_X11 (src_gc);
  GdkGCX11 *x11_dst_gc = GDK_GC_X11 (dst_gc);

  XCopyGC (GDK_GC_XDISPLAY (src_gc), GDK_GC_XGC (src_gc), ~((~1) << GCLastBit),
	   GDK_GC_XGC (dst_gc));

  x11_dst_gc->dirty_mask = x11_src_gc->dirty_mask;
  x11_dst_gc->have_clip_region = x11_src_gc->have_clip_region;
  x11_dst_gc->have_clip_mask = x11_src_gc->have_clip_mask;
}

/**
 * gdk_gc_get_screen:
 * @gc: a #GdkGC.
 *
 * Gets the #GdkScreen for which @gc was created
 *
 * Returns: the #GdkScreen for @gc.
 *
 * Since: 2.2
 */
GdkScreen *  
gdk_gc_get_screen (GdkGC *gc)
{
  g_return_val_if_fail (GDK_IS_GC_X11 (gc), NULL);
  
  return GDK_GC_X11 (gc)->screen;
}

/**
 * gdk_x11_gc_get_xdisplay:
 * @gc: a #GdkGC.
 * 
 * Returns the display of a #GdkGC.
 * 
 * Return value: an Xlib <type>Display*</type>.
 *
 * Deprecated: 2.22: #GdkGC has been replaced by #cairo_t.
 **/
Display *
gdk_x11_gc_get_xdisplay (GdkGC *gc)
{
  g_return_val_if_fail (GDK_IS_GC_X11 (gc), NULL);

  return GDK_SCREEN_XDISPLAY (gdk_gc_get_screen (gc));
}

/**
 * gdk_x11_gc_get_xgc:
 * @gc: a #GdkGC.
 * 
 * Returns the X GC of a #GdkGC.
 * 
 * Return value: an Xlib <type>GC</type>.
 *
 * Deprecated: 2.22: #GdkGC has been replaced by #cairo_t.
 **/
GC
gdk_x11_gc_get_xgc (GdkGC *gc)
{
  GdkGCX11 *gc_x11;
  
  g_return_val_if_fail (GDK_IS_GC_X11 (gc), NULL);

  gc_x11 = GDK_GC_X11 (gc);

  if (gc_x11->dirty_mask)
    _gdk_x11_gc_flush (gc);

  return gc_x11->xgc;
}

#define __GDK_GC_X11_C__
#include "gdkaliasdef.c"
