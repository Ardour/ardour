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
 * Modified by the GTK+ Team and others 1997-2005.  See the AUTHORS
 * file for a list of people on the GTK+ Team.  See the ChangeLog
 * files for a list of changes.  These files are distributed with
 * GTK+ at ftp://ftp.gtk.org/pub/gtk/.
 */

#include <config.h>
#include <math.h>
#include "gdk.h"
#include "gdkwindow.h"
#include "gdkinternals.h"
#include "gdkwindowimpl.h"
#include "gdkpixmap.h"
#include "gdkdrawable.h"
#include "gdktypes.h"
#include "gdkscreen.h"
#include "gdkgc.h"
#include "gdkcolor.h"
#include "gdkcursor.h"
#include "gdkalias.h"

/* LIMITATIONS:
 *
 * Offscreen windows can't be the child of a foreign window,
 *   nor contain foreign windows
 * GDK_POINTER_MOTION_HINT_MASK isn't effective
 */

typedef struct _GdkOffscreenWindowClass GdkOffscreenWindowClass;

struct _GdkOffscreenWindow
{
  GdkDrawable parent_instance;

  GdkWindow *wrapper;
  GdkCursor *cursor;
  GdkColormap *colormap;
  GdkScreen *screen;

  GdkPixmap *pixmap;
  GdkWindow *embedder;
};

struct _GdkOffscreenWindowClass
{
  GdkDrawableClass parent_class;
};

#define GDK_OFFSCREEN_WINDOW_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), GDK_TYPE_OFFSCREEN_WINDOW, GdkOffscreenWindowClass))
#define GDK_IS_OFFSCREEN_WINDOW_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), GDK_TYPE_OFFSCREEN_WINDOW))
#define GDK_OFFSCREEN_WINDOW_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), GDK_TYPE_OFFSCREEN_WINDOW, GdkOffscreenWindowClass))

static void       gdk_offscreen_window_impl_iface_init    (GdkWindowImplIface         *iface);
static void       gdk_offscreen_window_hide               (GdkWindow                  *window);

G_DEFINE_TYPE_WITH_CODE (GdkOffscreenWindow,
			 gdk_offscreen_window,
			 GDK_TYPE_DRAWABLE,
			 G_IMPLEMENT_INTERFACE (GDK_TYPE_WINDOW_IMPL,
						gdk_offscreen_window_impl_iface_init));


static void
gdk_offscreen_window_finalize (GObject *object)
{
  GdkOffscreenWindow *offscreen = GDK_OFFSCREEN_WINDOW (object);

  if (offscreen->cursor)
    gdk_cursor_unref (offscreen->cursor);

  offscreen->cursor = NULL;

  g_object_unref (offscreen->pixmap);

  G_OBJECT_CLASS (gdk_offscreen_window_parent_class)->finalize (object);
}

static void
gdk_offscreen_window_init (GdkOffscreenWindow *window)
{
}

static void
gdk_offscreen_window_destroy (GdkWindow *window,
			      gboolean   recursing,
			      gboolean   foreign_destroy)
{
  GdkWindowObject *private = GDK_WINDOW_OBJECT (window);
  GdkOffscreenWindow *offscreen;

  offscreen = GDK_OFFSCREEN_WINDOW (private->impl);

  gdk_offscreen_window_set_embedder (window, NULL);
  
  if (!recursing)
    gdk_offscreen_window_hide (window);

  g_object_unref (offscreen->colormap);
  offscreen->colormap = NULL;
}

static gboolean
is_parent_of (GdkWindow *parent,
	      GdkWindow *child)
{
  GdkWindow *w;

  w = child;
  while (w != NULL)
    {
      if (w == parent)
	return TRUE;

      w = gdk_window_get_parent (w);
    }

  return FALSE;
}

static GdkGC *
gdk_offscreen_window_create_gc (GdkDrawable     *drawable,
				GdkGCValues     *values,
				GdkGCValuesMask  values_mask)
{
  GdkOffscreenWindow *offscreen = GDK_OFFSCREEN_WINDOW (drawable);

  return gdk_gc_new_with_values (offscreen->pixmap, values, values_mask);
}

static GdkImage*
gdk_offscreen_window_copy_to_image (GdkDrawable    *drawable,
				    GdkImage       *image,
				    gint            src_x,
				    gint            src_y,
				    gint            dest_x,
				    gint            dest_y,
				    gint            width,
				    gint            height)
{
  GdkOffscreenWindow *offscreen = GDK_OFFSCREEN_WINDOW (drawable);

  return gdk_drawable_copy_to_image (offscreen->pixmap,
				     image,
				     src_x,
				     src_y,
				     dest_x, dest_y,
				     width, height);
}

static cairo_surface_t *
gdk_offscreen_window_ref_cairo_surface (GdkDrawable *drawable)
{
  GdkOffscreenWindow *offscreen = GDK_OFFSCREEN_WINDOW (drawable);

  return _gdk_drawable_ref_cairo_surface (offscreen->pixmap);
}

static GdkColormap*
gdk_offscreen_window_get_colormap (GdkDrawable *drawable)
{
  GdkOffscreenWindow *offscreen = GDK_OFFSCREEN_WINDOW (drawable);

  return offscreen->colormap;
}

static void
gdk_offscreen_window_set_colormap (GdkDrawable *drawable,
				   GdkColormap*colormap)
{
  GdkOffscreenWindow *offscreen = GDK_OFFSCREEN_WINDOW (drawable);

  if (colormap && GDK_WINDOW_DESTROYED (offscreen->wrapper))
    return;

  if (offscreen->colormap == colormap)
    return;

  if (offscreen->colormap)
    g_object_unref (offscreen->colormap);

  offscreen->colormap = colormap;
  if (offscreen->colormap)
    g_object_ref (offscreen->colormap);
}


static gint
gdk_offscreen_window_get_depth (GdkDrawable *drawable)
{
  GdkOffscreenWindow *offscreen = GDK_OFFSCREEN_WINDOW (drawable);

  return gdk_drawable_get_depth (offscreen->wrapper);
}

static GdkDrawable *
gdk_offscreen_window_get_source_drawable (GdkDrawable  *drawable)
{
  GdkOffscreenWindow *offscreen = GDK_OFFSCREEN_WINDOW (drawable);

  return _gdk_drawable_get_source_drawable (offscreen->pixmap);
}

static GdkDrawable *
gdk_offscreen_window_get_composite_drawable (GdkDrawable *drawable,
					     gint         x,
					     gint         y,
					     gint         width,
					     gint         height,
					     gint        *composite_x_offset,
					     gint        *composite_y_offset)
{
  GdkOffscreenWindow *offscreen = GDK_OFFSCREEN_WINDOW (drawable);

  return g_object_ref (offscreen->pixmap);
}

static GdkScreen*
gdk_offscreen_window_get_screen (GdkDrawable *drawable)
{
  GdkOffscreenWindow *offscreen = GDK_OFFSCREEN_WINDOW (drawable);

  return offscreen->screen;
}

static GdkVisual*
gdk_offscreen_window_get_visual (GdkDrawable    *drawable)
{
  GdkOffscreenWindow *offscreen = GDK_OFFSCREEN_WINDOW (drawable);

  return gdk_drawable_get_visual (offscreen->wrapper);
}

static void
add_damage (GdkOffscreenWindow *offscreen,
	    int x, int y,
	    int w, int h,
	    gboolean is_line)
{
  GdkRectangle rect;
  GdkRegion *damage;

  rect.x = x;
  rect.y = y;
  rect.width = w;
  rect.height = h;

  if (is_line)
    {
      /* This should really take into account line width, line
       * joins (and miter) and line caps. But these are hard
       * to compute, rarely used and generally a pain. And in
       * the end a snug damage rectangle is not that important
       * as multiple damages are generally created anyway.
       *
       * So, we just add some padding around the rect.
       * We use a padding of 3 pixels, plus an extra row
       * below and on the right for the normal line size. I.E.
       * line from (0,0) to (2,0) gets h=0 but is really
       * at least one pixel tall.
       */
      rect.x -= 3;
      rect.y -= 3;
      rect.width += 7;
      rect.height += 7;
    }

  damage = gdk_region_rectangle (&rect);
  _gdk_window_add_damage (offscreen->wrapper, damage);
  gdk_region_destroy (damage);
}

static GdkDrawable *
get_real_drawable (GdkOffscreenWindow *offscreen)
{
  GdkPixmapObject *pixmap;
  pixmap = (GdkPixmapObject *) offscreen->pixmap;
  return GDK_DRAWABLE (pixmap->impl);
}

static void
gdk_offscreen_window_draw_drawable (GdkDrawable *drawable,
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
  GdkOffscreenWindow *offscreen = GDK_OFFSCREEN_WINDOW (drawable);
  GdkDrawable *real_drawable = get_real_drawable (offscreen);

  gdk_draw_drawable (real_drawable, gc,
		     src, xsrc, ysrc,
		     xdest, ydest,
		     width, height);

  add_damage (offscreen, xdest, ydest, width, height, FALSE);
}

static void
gdk_offscreen_window_draw_rectangle (GdkDrawable  *drawable,
				     GdkGC	  *gc,
				     gboolean	   filled,
				     gint	   x,
				     gint	   y,
				     gint	   width,
				     gint	   height)
{
  GdkOffscreenWindow *offscreen = GDK_OFFSCREEN_WINDOW (drawable);
  GdkDrawable *real_drawable = get_real_drawable (offscreen);

  gdk_draw_rectangle (real_drawable,
		      gc, filled, x, y, width, height);

  add_damage (offscreen, x, y, width, height, !filled);

}

static void
gdk_offscreen_window_draw_arc (GdkDrawable  *drawable,
			       GdkGC	       *gc,
			       gboolean	filled,
			       gint		x,
			       gint		y,
			       gint		width,
			       gint		height,
			       gint		angle1,
			       gint		angle2)
{
  GdkOffscreenWindow *offscreen = GDK_OFFSCREEN_WINDOW (drawable);
  GdkDrawable *real_drawable = get_real_drawable (offscreen);

  gdk_draw_arc (real_drawable,
		gc,
		filled,
		x,
		y,
		width,
		height,
		angle1,
		angle2);
  add_damage (offscreen, x, y, width, height, !filled);
}

static void
gdk_offscreen_window_draw_polygon (GdkDrawable  *drawable,
				   GdkGC	       *gc,
				   gboolean	filled,
				   GdkPoint     *points,
				   gint		npoints)
{
  GdkOffscreenWindow *offscreen = GDK_OFFSCREEN_WINDOW (drawable);
  GdkDrawable *real_drawable = get_real_drawable (offscreen);

  gdk_draw_polygon (real_drawable,
		    gc,
		    filled,
		    points,
		    npoints);

  if (npoints > 0)
    {
      int min_x, min_y, max_x, max_y, i;

      min_x = max_x = points[0].x;
      min_y = max_y = points[0].y;

	for (i = 1; i < npoints; i++)
	  {
	    min_x = MIN (min_x, points[i].x);
	    max_x = MAX (max_x, points[i].x);
	    min_y = MIN (min_y, points[i].y);
	    max_y = MAX (max_y, points[i].y);
	  }

	add_damage (offscreen, min_x, min_y,
		    max_x - min_x,
		    max_y - min_y, !filled);
    }
}

static void
gdk_offscreen_window_draw_text (GdkDrawable  *drawable,
				GdkFont      *font,
				GdkGC	       *gc,
				gint		x,
				gint		y,
				const gchar  *text,
				gint		text_length)
{
  GdkOffscreenWindow *offscreen = GDK_OFFSCREEN_WINDOW (drawable);
  GdkDrawable *real_drawable = get_real_drawable (offscreen);
  GdkWindowObject *private = GDK_WINDOW_OBJECT (offscreen->wrapper);

  gdk_draw_text (real_drawable,
		 font,
		 gc,
		 x,
		 y,
		 text,
		 text_length);

  /* Hard to compute the minimal size, not that often used anyway. */
  add_damage (offscreen, 0, 0, private->width, private->height, FALSE);
}

static void
gdk_offscreen_window_draw_text_wc (GdkDrawable	 *drawable,
				   GdkFont	 *font,
				   GdkGC		 *gc,
				   gint		  x,
				   gint		  y,
				   const GdkWChar *text,
				   gint		  text_length)
{
  GdkOffscreenWindow *offscreen = GDK_OFFSCREEN_WINDOW (drawable);
  GdkDrawable *real_drawable = get_real_drawable (offscreen);
  GdkWindowObject *private = GDK_WINDOW_OBJECT (offscreen->wrapper);

  gdk_draw_text_wc (real_drawable,
		    font,
		    gc,
		    x,
		    y,
		    text,
		    text_length);

  /* Hard to compute the minimal size, not that often used anyway. */
  add_damage (offscreen, 0, 0, private->width, private->height, FALSE);
}

static void
gdk_offscreen_window_draw_points (GdkDrawable  *drawable,
				  GdkGC	       *gc,
				  GdkPoint     *points,
				  gint		npoints)
{
  GdkOffscreenWindow *offscreen = GDK_OFFSCREEN_WINDOW (drawable);
  GdkDrawable *real_drawable = get_real_drawable (offscreen);

  gdk_draw_points (real_drawable,
		   gc,
		   points,
		   npoints);


  if (npoints > 0)
    {
      int min_x, min_y, max_x, max_y, i;

      min_x = max_x = points[0].x;
      min_y = max_y = points[0].y;

	for (i = 1; i < npoints; i++)
	  {
	    min_x = MIN (min_x, points[i].x);
	    max_x = MAX (max_x, points[i].x);
	    min_y = MIN (min_y, points[i].y);
	    max_y = MAX (max_y, points[i].y);
	  }

	add_damage (offscreen, min_x, min_y,
		    max_x - min_x + 1,
		    max_y - min_y + 1,
		    FALSE);
    }
}

static void
gdk_offscreen_window_draw_segments (GdkDrawable  *drawable,
				    GdkGC	 *gc,
				    GdkSegment   *segs,
				    gint	  nsegs)
{
  GdkOffscreenWindow *offscreen = GDK_OFFSCREEN_WINDOW (drawable);
  GdkDrawable *real_drawable = get_real_drawable (offscreen);

  gdk_draw_segments (real_drawable,
		     gc,
		     segs,
		     nsegs);

  if (nsegs > 0)
    {
      int min_x, min_y, max_x, max_y, i;

      min_x = max_x = segs[0].x1;
      min_y = max_y = segs[0].y1;

	for (i = 0; i < nsegs; i++)
	  {
	    min_x = MIN (min_x, segs[i].x1);
	    max_x = MAX (max_x, segs[i].x1);
	    min_x = MIN (min_x, segs[i].x2);
	    max_x = MAX (max_x, segs[i].x2);
	    min_y = MIN (min_y, segs[i].y1);
	    max_y = MAX (max_y, segs[i].y1);
	    min_y = MIN (min_y, segs[i].y2);
	    max_y = MAX (max_y, segs[i].y2);
	  }

	add_damage (offscreen, min_x, min_y,
		    max_x - min_x,
		    max_y - min_y, TRUE);
    }

}

static void
gdk_offscreen_window_draw_lines (GdkDrawable  *drawable,
				 GdkGC        *gc,
				 GdkPoint     *points,
				 gint          npoints)
{
  GdkOffscreenWindow *offscreen = GDK_OFFSCREEN_WINDOW (drawable);
  GdkDrawable *real_drawable = get_real_drawable (offscreen);
  GdkWindowObject *private = GDK_WINDOW_OBJECT (offscreen->wrapper);

  gdk_draw_lines (real_drawable,
		  gc,
		  points,
		  npoints);

  /* Hard to compute the minimal size, as we don't know the line
     width, and since joins are hard to calculate.
     Its not that often used anyway, damage it all */
  add_damage (offscreen, 0, 0, private->width, private->height, TRUE);
}

static void
gdk_offscreen_window_draw_image (GdkDrawable *drawable,
				 GdkGC	      *gc,
				 GdkImage    *image,
				 gint	       xsrc,
				 gint	       ysrc,
				 gint	       xdest,
				 gint	       ydest,
				 gint	       width,
				 gint	       height)
{
  GdkOffscreenWindow *offscreen = GDK_OFFSCREEN_WINDOW (drawable);
  GdkDrawable *real_drawable = get_real_drawable (offscreen);

  gdk_draw_image (real_drawable,
		  gc,
		  image,
		  xsrc,
		  ysrc,
		  xdest,
		  ydest,
		  width,
		  height);

  add_damage (offscreen, xdest, ydest, width, height, FALSE);
}


static void
gdk_offscreen_window_draw_pixbuf (GdkDrawable *drawable,
				  GdkGC       *gc,
				  GdkPixbuf   *pixbuf,
				  gint         src_x,
				  gint         src_y,
				  gint         dest_x,
				  gint         dest_y,
				  gint         width,
				  gint         height,
				  GdkRgbDither dither,
				  gint         x_dither,
				  gint         y_dither)
{
  GdkOffscreenWindow *offscreen = GDK_OFFSCREEN_WINDOW (drawable);
  GdkDrawable *real_drawable = get_real_drawable (offscreen);

  gdk_draw_pixbuf (real_drawable,
		   gc,
		   pixbuf,
		   src_x,
		   src_y,
		   dest_x,
		   dest_y,
		   width,
		   height,
		   dither,
		   x_dither,
		   y_dither);

  add_damage (offscreen, dest_x, dest_y, width, height, FALSE);

}

void
_gdk_offscreen_window_new (GdkWindow     *window,
			   GdkScreen     *screen,
			   GdkVisual     *visual,
			   GdkWindowAttr *attributes,
			   gint           attributes_mask)
{
  GdkWindowObject *private;
  GdkOffscreenWindow *offscreen;

  g_return_if_fail (attributes != NULL);

  if (attributes->wclass != GDK_INPUT_OUTPUT)
    return; /* Can't support input only offscreens */

  private = (GdkWindowObject *)window;

  if (private->parent != NULL && GDK_WINDOW_DESTROYED (private->parent))
    return;

  private->impl = g_object_new (GDK_TYPE_OFFSCREEN_WINDOW, NULL);
  offscreen = GDK_OFFSCREEN_WINDOW (private->impl);
  offscreen->wrapper = window;

  offscreen->screen = screen;

  if (attributes_mask & GDK_WA_COLORMAP)
    offscreen->colormap = g_object_ref (attributes->colormap);
  else
    {
      if (gdk_screen_get_system_visual (screen) == visual)
	{
	  offscreen->colormap = gdk_screen_get_system_colormap (screen);
	  g_object_ref (offscreen->colormap);
	}
      else
	offscreen->colormap = gdk_colormap_new (visual, FALSE);
    }

  offscreen->pixmap = gdk_pixmap_new ((GdkDrawable *)private->parent,
				      private->width,
				      private->height,
				      private->depth);
  gdk_drawable_set_colormap (offscreen->pixmap, offscreen->colormap);
}

static gboolean
gdk_offscreen_window_reparent (GdkWindow *window,
			       GdkWindow *new_parent,
			       gint       x,
			       gint       y)
{
  GdkWindowObject *private = (GdkWindowObject *)window;
  GdkWindowObject *new_parent_private = (GdkWindowObject *)new_parent;
  GdkWindowObject *old_parent;
  gboolean was_mapped;

  if (new_parent)
    {
      /* No input-output children of input-only windows */
      if (new_parent_private->input_only && !private->input_only)
	return FALSE;

      /* Don't create loops in hierarchy */
      if (is_parent_of (window, new_parent))
	return FALSE;
    }

  was_mapped = GDK_WINDOW_IS_MAPPED (window);

  gdk_window_hide (window);

  if (private->parent)
    private->parent->children = g_list_remove (private->parent->children, window);

  old_parent = private->parent;
  private->parent = new_parent_private;
  private->x = x;
  private->y = y;

  if (new_parent_private)
    private->parent->children = g_list_prepend (private->parent->children, window);

  _gdk_synthesize_crossing_events_for_geometry_change (window);
  if (old_parent)
    _gdk_synthesize_crossing_events_for_geometry_change (GDK_WINDOW (old_parent));

  return was_mapped;
}

static void
from_embedder (GdkWindow *window,
	       double embedder_x, double embedder_y,
	       double *offscreen_x, double *offscreen_y)
{
  GdkWindowObject *private;

  private = (GdkWindowObject *)window;

  g_signal_emit_by_name (private->impl_window,
			 "from-embedder",
			 embedder_x, embedder_y,
			 offscreen_x, offscreen_y,
			 NULL);
}

static void
to_embedder (GdkWindow *window,
	     double offscreen_x, double offscreen_y,
	     double *embedder_x, double *embedder_y)
{
  GdkWindowObject *private;

  private = (GdkWindowObject *)window;

  g_signal_emit_by_name (private->impl_window,
			 "to-embedder",
			 offscreen_x, offscreen_y,
			 embedder_x, embedder_y,
			 NULL);
}

static gint
gdk_offscreen_window_get_root_coords (GdkWindow *window,
				      gint       x,
				      gint       y,
				      gint      *root_x,
				      gint      *root_y)
{
  GdkWindowObject *private = GDK_WINDOW_OBJECT (window);
  GdkOffscreenWindow *offscreen;
  int tmpx, tmpy;

  tmpx = x;
  tmpy = y;

  offscreen = GDK_OFFSCREEN_WINDOW (private->impl);
  if (offscreen->embedder)
    {
      double dx, dy;
      to_embedder (window,
		   x, y,
		   &dx, &dy);
      tmpx = floor (dx + 0.5);
      tmpy = floor (dy + 0.5);
      gdk_window_get_root_coords (offscreen->embedder,
				  tmpx, tmpy,
				  &tmpx, &tmpy);

    }

  if (root_x)
    *root_x = tmpx;
  if (root_y)
    *root_y = tmpy;

  return TRUE;
}

static gint
gdk_offscreen_window_get_deskrelative_origin (GdkWindow *window,
					      gint      *x,
					      gint      *y)
{
  GdkWindowObject *private = GDK_WINDOW_OBJECT (window);
  GdkOffscreenWindow *offscreen;
  int tmpx, tmpy;

  tmpx = 0;
  tmpy = 0;

  offscreen = GDK_OFFSCREEN_WINDOW (private->impl);
  if (offscreen->embedder)
    {
      double dx, dy;
      gdk_window_get_deskrelative_origin (offscreen->embedder,
					  &tmpx, &tmpy);

      to_embedder (window,
		   0, 0,
		   &dx, &dy);
      tmpx = floor (tmpx + dx + 0.5);
      tmpy = floor (tmpy + dy + 0.5);
    }


  if (x)
    *x = tmpx;
  if (y)
    *y = tmpy;

  return TRUE;
}

static gboolean
gdk_offscreen_window_get_pointer (GdkWindow       *window,
				  gint            *x,
				  gint            *y,
				  GdkModifierType *mask)
{
  GdkWindowObject *private = GDK_WINDOW_OBJECT (window);
  GdkOffscreenWindow *offscreen;
  int tmpx, tmpy;
  double dtmpx, dtmpy;
  GdkModifierType tmpmask;

  tmpx = 0;
  tmpy = 0;
  tmpmask = 0;

  offscreen = GDK_OFFSCREEN_WINDOW (private->impl);
  if (offscreen->embedder != NULL)
    {
      gdk_window_get_pointer (offscreen->embedder, &tmpx, &tmpy, &tmpmask);
      from_embedder (window,
		     tmpx, tmpy,
		     &dtmpx, &dtmpy);
      tmpx = floor (dtmpx + 0.5);
      tmpy = floor (dtmpy + 0.5);
    }

  if (x)
    *x = tmpx;
  if (y)
    *y = tmpy;
  if (mask)
    *mask = tmpmask;
  return TRUE;
}

GdkDrawable *
_gdk_offscreen_window_get_real_drawable (GdkOffscreenWindow *offscreen)
{
  return get_real_drawable (offscreen);
}

/**
 * gdk_offscreen_window_get_pixmap:
 * @window: a #GdkWindow
 *
 * Gets the offscreen pixmap that an offscreen window renders into.
 * If you need to keep this around over window resizes, you need to
 * add a reference to it.
 *
 * Returns: The offscreen pixmap, or %NULL if not offscreen
 *
 * Since: 2.18
 */
GdkPixmap *
gdk_offscreen_window_get_pixmap (GdkWindow *window)
{
  GdkWindowObject *private = (GdkWindowObject *)window;
  GdkOffscreenWindow *offscreen;

  g_return_val_if_fail (GDK_IS_WINDOW (window), FALSE);

  if (!GDK_IS_OFFSCREEN_WINDOW (private->impl))
    return NULL;

  offscreen = GDK_OFFSCREEN_WINDOW (private->impl);
  return offscreen->pixmap;
}

static void
gdk_offscreen_window_raise (GdkWindow *window)
{
  /* gdk_window_raise already changed the stacking order */
  _gdk_synthesize_crossing_events_for_geometry_change (window);
}

static void
gdk_offscreen_window_lower (GdkWindow *window)
{
  /* gdk_window_lower already changed the stacking order */
  _gdk_synthesize_crossing_events_for_geometry_change (window);
}

static void
gdk_offscreen_window_move_resize_internal (GdkWindow *window,
					   gint       x,
					   gint       y,
					   gint       width,
					   gint       height,
					   gboolean   send_expose_events)
{
  GdkWindowObject *private = (GdkWindowObject *)window;
  GdkOffscreenWindow *offscreen;
  gint dx, dy, dw, dh;
  GdkGC *gc;
  GdkPixmap *old_pixmap;

  offscreen = GDK_OFFSCREEN_WINDOW (private->impl);

  if (width < 1)
    width = 1;
  if (height < 1)
    height = 1;

  if (private->destroyed)
    return;

  dx = x - private->x;
  dy = y - private->y;
  dw = width - private->width;
  dh = height - private->height;

  private->x = x;
  private->y = y;

  if (private->width != width ||
      private->height != height)
    {
      private->width = width;
      private->height = height;

      old_pixmap = offscreen->pixmap;
      offscreen->pixmap = gdk_pixmap_new (GDK_DRAWABLE (old_pixmap),
					  width,
					  height,
					  private->depth);

      gc = _gdk_drawable_get_scratch_gc (offscreen->pixmap, FALSE);
      gdk_draw_drawable (offscreen->pixmap,
			 gc,
			 old_pixmap,
			 0,0, 0, 0,
			 -1, -1);
      g_object_unref (old_pixmap);
    }

  if (GDK_WINDOW_IS_MAPPED (private))
    {
      // TODO: Only invalidate new area, i.e. for larger windows
      gdk_window_invalidate_rect (window, NULL, TRUE);
      _gdk_synthesize_crossing_events_for_geometry_change (window);
    }
}

static void
gdk_offscreen_window_move_resize (GdkWindow *window,
				  gboolean   with_move,
				  gint       x,
				  gint       y,
				  gint       width,
				  gint       height)
{
  GdkWindowObject *private = (GdkWindowObject *)window;
  GdkOffscreenWindow *offscreen;

  offscreen = GDK_OFFSCREEN_WINDOW (private->impl);

  if (!with_move)
    {
      x = private->x;
      y = private->y;
    }

  if (width < 0)
    width = private->width;

  if (height < 0)
    height = private->height;

  gdk_offscreen_window_move_resize_internal (window, x, y,
					     width, height,
					     TRUE);
}

static void
gdk_offscreen_window_show (GdkWindow *window,
			   gboolean already_mapped)
{
  GdkWindowObject *private = (GdkWindowObject *)window;

  gdk_window_clear_area_e (window, 0, 0,
			   private->width, private->height);
}


static void
gdk_offscreen_window_hide (GdkWindow *window)
{
  GdkWindowObject *private;
  GdkOffscreenWindow *offscreen;
  GdkDisplay *display;

  g_return_if_fail (window != NULL);

  private = (GdkWindowObject*) window;
  offscreen = GDK_OFFSCREEN_WINDOW (private->impl);

  /* May need to break grabs on children */
  display = gdk_drawable_get_display (window);

  /* TODO: This needs updating to the new grab world */
#if 0
  if (display->pointer_grab.window != NULL)
    {
      if (is_parent_of (window, display->pointer_grab.window))
	{
	  /* Call this ourselves, even though gdk_display_pointer_ungrab
	     does so too, since we want to pass implicit == TRUE so the
	     broken grab event is generated */
	  _gdk_display_unset_has_pointer_grab (display,
					       TRUE,
					       FALSE,
					       GDK_CURRENT_TIME);
	  gdk_display_pointer_ungrab (display, GDK_CURRENT_TIME);
	}
    }
#endif
}

static void
gdk_offscreen_window_withdraw (GdkWindow *window)
{
}

static GdkEventMask
gdk_offscreen_window_get_events (GdkWindow *window)
{
  return 0;
}

static void
gdk_offscreen_window_set_events (GdkWindow       *window,
				 GdkEventMask     event_mask)
{
}

static void
gdk_offscreen_window_set_background (GdkWindow      *window,
				     const GdkColor *color)
{
  GdkWindowObject *private = (GdkWindowObject *)window;
  GdkColormap *colormap = gdk_drawable_get_colormap (window);

  private->bg_color = *color;
  gdk_colormap_query_color (colormap, private->bg_color.pixel, &private->bg_color);

  if (private->bg_pixmap &&
      private->bg_pixmap != GDK_PARENT_RELATIVE_BG &&
      private->bg_pixmap != GDK_NO_BG)
    g_object_unref (private->bg_pixmap);

  private->bg_pixmap = NULL;
}

static void
gdk_offscreen_window_set_back_pixmap (GdkWindow *window,
				      GdkPixmap *pixmap)
{
  GdkWindowObject *private = (GdkWindowObject *)window;

  if (pixmap &&
      private->bg_pixmap != GDK_PARENT_RELATIVE_BG &&
      private->bg_pixmap != GDK_NO_BG &&
      !gdk_drawable_get_colormap (pixmap))
    {
      g_warning ("gdk_window_set_back_pixmap(): pixmap must have a colormap");
      return;
    }

  if (private->bg_pixmap &&
      private->bg_pixmap != GDK_PARENT_RELATIVE_BG &&
      private->bg_pixmap != GDK_NO_BG)
    g_object_unref (private->bg_pixmap);

  private->bg_pixmap = pixmap;

  if (pixmap &&
      private->bg_pixmap != GDK_PARENT_RELATIVE_BG &&
      private->bg_pixmap != GDK_NO_BG)
    g_object_ref (pixmap);
}

static void
gdk_offscreen_window_shape_combine_region (GdkWindow       *window,
					   const GdkRegion *shape_region,
					   gint             offset_x,
					   gint             offset_y)
{
}

static void
gdk_offscreen_window_input_shape_combine_region (GdkWindow       *window,
						 const GdkRegion *shape_region,
						 gint             offset_x,
						 gint             offset_y)
{
}

static gboolean
gdk_offscreen_window_set_static_gravities (GdkWindow *window,
					   gboolean   use_static)
{
  return TRUE;
}

static void
gdk_offscreen_window_set_cursor (GdkWindow *window,
				 GdkCursor *cursor)
{
  GdkWindowObject *private = (GdkWindowObject *)window;
  GdkOffscreenWindow *offscreen;

  offscreen = GDK_OFFSCREEN_WINDOW (private->impl);

  if (offscreen->cursor)
    {
      gdk_cursor_unref (offscreen->cursor);
      offscreen->cursor = NULL;
    }

  if (cursor)
    offscreen->cursor = gdk_cursor_ref (cursor);

  /* TODO: The cursor is never actually used... */
}

static void
gdk_offscreen_window_get_geometry (GdkWindow *window,
				   gint      *x,
				   gint      *y,
				   gint      *width,
				   gint      *height,
				   gint      *depth)
{
  GdkWindowObject *private = (GdkWindowObject *)window;

  g_return_if_fail (window == NULL || GDK_IS_WINDOW (window));

  if (!GDK_WINDOW_DESTROYED (window))
    {
      if (x)
	*x = private->x;
      if (y)
	*y = private->y;
      if (width)
	*width = private->width;
      if (height)
	*height = private->height;
      if (depth)
	*depth = private->depth;
    }
}

static gboolean
gdk_offscreen_window_queue_antiexpose (GdkWindow *window,
				       GdkRegion *area)
{
  return FALSE;
}

static void
gdk_offscreen_window_queue_translation (GdkWindow *window,
					GdkGC     *gc,
					GdkRegion *area,
					gint       dx,
					gint       dy)
{
}

/**
 * gdk_offscreen_window_set_embedder:
 * @window: a #GdkWindow
 * @embedder: the #GdkWindow that @window gets embedded in
 *
 * Sets @window to be embedded in @embedder.
 *
 * To fully embed an offscreen window, in addition to calling this
 * function, it is also necessary to handle the #GdkWindow::pick-embedded-child
 * signal on the @embedder and the #GdkWindow::to-embedder and
 * #GdkWindow::from-embedder signals on @window.
 *
 * Since: 2.18
 */
void
gdk_offscreen_window_set_embedder (GdkWindow     *window,
				   GdkWindow     *embedder)
{
  GdkWindowObject *private = (GdkWindowObject *)window;
  GdkOffscreenWindow *offscreen;

  g_return_if_fail (GDK_IS_WINDOW (window));

  if (!GDK_IS_OFFSCREEN_WINDOW (private->impl))
    return;

  offscreen = GDK_OFFSCREEN_WINDOW (private->impl);

  if (embedder)
    {
      g_object_ref (embedder);
      GDK_WINDOW_OBJECT (embedder)->num_offscreen_children++;
    }

  if (offscreen->embedder)
    {
      g_object_unref (offscreen->embedder);
      GDK_WINDOW_OBJECT (offscreen->embedder)->num_offscreen_children--;
    }

  offscreen->embedder = embedder;
}

/**
 * gdk_offscreen_window_get_embedder:
 * @window: a #GdkWindow
 *
 * Gets the window that @window is embedded in.
 *
 * Returns: the embedding #GdkWindow, or %NULL if @window is not an
 *     embedded offscreen window
 *
 * Since: 2.18
 */
GdkWindow *
gdk_offscreen_window_get_embedder (GdkWindow *window)
{
  GdkWindowObject *private = (GdkWindowObject *)window;
  GdkOffscreenWindow *offscreen;

  g_return_val_if_fail (GDK_IS_WINDOW (window), NULL);

  if (!GDK_IS_OFFSCREEN_WINDOW (private->impl))
    return NULL;

  offscreen = GDK_OFFSCREEN_WINDOW (private->impl);

  return offscreen->embedder;
}

static void
gdk_offscreen_window_class_init (GdkOffscreenWindowClass *klass)
{
  GdkDrawableClass *drawable_class = GDK_DRAWABLE_CLASS (klass);
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = gdk_offscreen_window_finalize;

  drawable_class->create_gc = gdk_offscreen_window_create_gc;
  drawable_class->_copy_to_image = gdk_offscreen_window_copy_to_image;
  drawable_class->ref_cairo_surface = gdk_offscreen_window_ref_cairo_surface;
  drawable_class->set_colormap = gdk_offscreen_window_set_colormap;
  drawable_class->get_colormap = gdk_offscreen_window_get_colormap;
  drawable_class->get_depth = gdk_offscreen_window_get_depth;
  drawable_class->get_screen = gdk_offscreen_window_get_screen;
  drawable_class->get_visual = gdk_offscreen_window_get_visual;
  drawable_class->get_source_drawable = gdk_offscreen_window_get_source_drawable;
  drawable_class->get_composite_drawable = gdk_offscreen_window_get_composite_drawable;

  drawable_class->draw_rectangle = gdk_offscreen_window_draw_rectangle;
  drawable_class->draw_arc = gdk_offscreen_window_draw_arc;
  drawable_class->draw_polygon = gdk_offscreen_window_draw_polygon;
  drawable_class->draw_text = gdk_offscreen_window_draw_text;
  drawable_class->draw_text_wc = gdk_offscreen_window_draw_text_wc;
  drawable_class->draw_drawable_with_src = gdk_offscreen_window_draw_drawable;
  drawable_class->draw_points = gdk_offscreen_window_draw_points;
  drawable_class->draw_segments = gdk_offscreen_window_draw_segments;
  drawable_class->draw_lines = gdk_offscreen_window_draw_lines;
  drawable_class->draw_image = gdk_offscreen_window_draw_image;
  drawable_class->draw_pixbuf = gdk_offscreen_window_draw_pixbuf;
}

static void
gdk_offscreen_window_impl_iface_init (GdkWindowImplIface *iface)
{
  iface->show = gdk_offscreen_window_show;
  iface->hide = gdk_offscreen_window_hide;
  iface->withdraw = gdk_offscreen_window_withdraw;
  iface->raise = gdk_offscreen_window_raise;
  iface->lower = gdk_offscreen_window_lower;
  iface->move_resize = gdk_offscreen_window_move_resize;
  iface->set_background = gdk_offscreen_window_set_background;
  iface->set_back_pixmap = gdk_offscreen_window_set_back_pixmap;
  iface->get_events = gdk_offscreen_window_get_events;
  iface->set_events = gdk_offscreen_window_set_events;
  iface->reparent = gdk_offscreen_window_reparent;
  iface->set_cursor = gdk_offscreen_window_set_cursor;
  iface->get_geometry = gdk_offscreen_window_get_geometry;
  iface->shape_combine_region = gdk_offscreen_window_shape_combine_region;
  iface->input_shape_combine_region = gdk_offscreen_window_input_shape_combine_region;
  iface->set_static_gravities = gdk_offscreen_window_set_static_gravities;
  iface->queue_antiexpose = gdk_offscreen_window_queue_antiexpose;
  iface->queue_translation = gdk_offscreen_window_queue_translation;
  iface->get_root_coords = gdk_offscreen_window_get_root_coords;
  iface->get_deskrelative_origin = gdk_offscreen_window_get_deskrelative_origin;
  iface->get_pointer = gdk_offscreen_window_get_pointer;
  iface->destroy = gdk_offscreen_window_destroy;
}

#define __GDK_OFFSCREEN_WINDOW_C__
#include "gdkaliasdef.c"
