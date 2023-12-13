/* GIMP Drawing Kit
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

#include "gdkx.h"
#include "gdkregion-generic.h"

#include <cairo-xlib.h>

#include <stdlib.h>
#include <string.h>		/* for memcpy() */

#if defined (HAVE_IPC_H) && defined (HAVE_SHM_H) && defined (HAVE_XSHM_H)
#define USE_SHM
#endif

#ifdef USE_SHM
#include <X11/extensions/XShm.h>
#endif /* USE_SHM */

#include "gdkprivate-x11.h"
#include "gdkdrawable-x11.h"
#include "gdkpixmap-x11.h"
#include "gdkscreen-x11.h"
#include "gdkdisplay-x11.h"

#include "gdkalias.h"

static void gdk_x11_draw_rectangle (GdkDrawable    *drawable,
				    GdkGC          *gc,
				    gboolean        filled,
				    gint            x,
				    gint            y,
				    gint            width,
				    gint            height);
static void gdk_x11_draw_arc       (GdkDrawable    *drawable,
				    GdkGC          *gc,
				    gboolean        filled,
				    gint            x,
				    gint            y,
				    gint            width,
				    gint            height,
				    gint            angle1,
				    gint            angle2);
static void gdk_x11_draw_polygon   (GdkDrawable    *drawable,
				    GdkGC          *gc,
				    gboolean        filled,
				    GdkPoint       *points,
				    gint            npoints);
static void gdk_x11_draw_text      (GdkDrawable    *drawable,
				    GdkFont        *font,
				    GdkGC          *gc,
				    gint            x,
				    gint            y,
				    const gchar    *text,
				    gint            text_length);
static void gdk_x11_draw_text_wc   (GdkDrawable    *drawable,
				    GdkFont        *font,
				    GdkGC          *gc,
				    gint            x,
				    gint            y,
				    const GdkWChar *text,
				    gint            text_length);
static void gdk_x11_draw_drawable  (GdkDrawable    *drawable,
				    GdkGC          *gc,
				    GdkPixmap      *src,
				    gint            xsrc,
				    gint            ysrc,
				    gint            xdest,
				    gint            ydest,
				    gint            width,
				    gint            height,
				    GdkDrawable    *original_src);
static void gdk_x11_draw_points    (GdkDrawable    *drawable,
				    GdkGC          *gc,
				    GdkPoint       *points,
				    gint            npoints);
static void gdk_x11_draw_segments  (GdkDrawable    *drawable,
				    GdkGC          *gc,
				    GdkSegment     *segs,
				    gint            nsegs);
static void gdk_x11_draw_lines     (GdkDrawable    *drawable,
				    GdkGC          *gc,
				    GdkPoint       *points,
				    gint            npoints);

static void gdk_x11_draw_image     (GdkDrawable     *drawable,
                                    GdkGC           *gc,
                                    GdkImage        *image,
                                    gint             xsrc,
                                    gint             ysrc,
                                    gint             xdest,
                                    gint             ydest,
                                    gint             width,
                                    gint             height);
static void gdk_x11_draw_pixbuf    (GdkDrawable     *drawable,
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

static cairo_surface_t *gdk_x11_ref_cairo_surface (GdkDrawable *drawable);
     
static void gdk_x11_set_colormap   (GdkDrawable    *drawable,
                                    GdkColormap    *colormap);

static GdkColormap* gdk_x11_get_colormap   (GdkDrawable    *drawable);
static gint         gdk_x11_get_depth      (GdkDrawable    *drawable);
static GdkScreen *  gdk_x11_get_screen	   (GdkDrawable    *drawable);
static GdkVisual*   gdk_x11_get_visual     (GdkDrawable    *drawable);

static void gdk_drawable_impl_x11_finalize   (GObject *object);

static const cairo_user_data_key_t gdk_x11_cairo_key;

G_DEFINE_TYPE (GdkDrawableImplX11, _gdk_drawable_impl_x11, GDK_TYPE_DRAWABLE)

static void
_gdk_drawable_impl_x11_class_init (GdkDrawableImplX11Class *klass)
{
  GdkDrawableClass *drawable_class = GDK_DRAWABLE_CLASS (klass);
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  
  object_class->finalize = gdk_drawable_impl_x11_finalize;
  
  drawable_class->create_gc = _gdk_x11_gc_new;
  drawable_class->draw_rectangle = gdk_x11_draw_rectangle;
  drawable_class->draw_arc = gdk_x11_draw_arc;
  drawable_class->draw_polygon = gdk_x11_draw_polygon;
  drawable_class->draw_text = gdk_x11_draw_text;
  drawable_class->draw_text_wc = gdk_x11_draw_text_wc;
  drawable_class->draw_drawable_with_src = gdk_x11_draw_drawable;
  drawable_class->draw_points = gdk_x11_draw_points;
  drawable_class->draw_segments = gdk_x11_draw_segments;
  drawable_class->draw_lines = gdk_x11_draw_lines;
  drawable_class->draw_image = gdk_x11_draw_image;
  drawable_class->draw_pixbuf = gdk_x11_draw_pixbuf;
  
  drawable_class->ref_cairo_surface = gdk_x11_ref_cairo_surface;

  drawable_class->set_colormap = gdk_x11_set_colormap;
  drawable_class->get_colormap = gdk_x11_get_colormap;

  drawable_class->get_depth = gdk_x11_get_depth;
  drawable_class->get_screen = gdk_x11_get_screen;
  drawable_class->get_visual = gdk_x11_get_visual;
  
  drawable_class->_copy_to_image = _gdk_x11_copy_to_image;
}

static void
_gdk_drawable_impl_x11_init (GdkDrawableImplX11 *impl)
{
}

static void
gdk_drawable_impl_x11_finalize (GObject *object)
{
  gdk_drawable_set_colormap (GDK_DRAWABLE (object), NULL);

  G_OBJECT_CLASS (_gdk_drawable_impl_x11_parent_class)->finalize (object);
}

/**
 * _gdk_x11_drawable_finish:
 * @drawable: a #GdkDrawableImplX11.
 * 
 * Performs necessary cleanup prior to freeing a pixmap or
 * destroying a window.
 **/
void
_gdk_x11_drawable_finish (GdkDrawable *drawable)
{
  GdkDrawableImplX11 *impl = GDK_DRAWABLE_IMPL_X11 (drawable);
  
  if (impl->picture)
    {
      XRenderFreePicture (GDK_SCREEN_XDISPLAY (impl->screen),
			  impl->picture);
      impl->picture = None;
    }
  
  if (impl->cairo_surface)
    {
      cairo_surface_finish (impl->cairo_surface);
      cairo_surface_set_user_data (impl->cairo_surface, &gdk_x11_cairo_key,
				   NULL, NULL);
    }
}

/**
 * _gdk_x11_drawable_update_size:
 * @drawable: a #GdkDrawableImplX11.
 * 
 * Updates the state of the drawable (in particular the drawable's
 * cairo surface) when its size has changed.
 **/
void
_gdk_x11_drawable_update_size (GdkDrawable *drawable)
{
  GdkDrawableImplX11 *impl = GDK_DRAWABLE_IMPL_X11 (drawable);
  
  if (impl->cairo_surface)
    {
      int width, height;
      
      gdk_drawable_get_size (drawable, &width, &height);
      cairo_xlib_surface_set_size (impl->cairo_surface, width, height);
    }
}

static void
try_pixmap (Display *xdisplay,
	    int      screen,
	    int      depth)
{
  Pixmap pixmap = XCreatePixmap (xdisplay,
				 RootWindow (xdisplay, screen),
				 1, 1, depth);
  XFreePixmap (xdisplay, pixmap);
}

gboolean
_gdk_x11_have_render (GdkDisplay *display)
{
  Display *xdisplay = GDK_DISPLAY_XDISPLAY (display);
  GdkDisplayX11 *x11display = GDK_DISPLAY_X11 (display);

  if (x11display->have_render == GDK_UNKNOWN)
    {
      int event_base, error_base;
      x11display->have_render =
	XRenderQueryExtension (xdisplay, &event_base, &error_base)
	? GDK_YES : GDK_NO;

      if (x11display->have_render == GDK_YES)
	{
	  /*
	   * Sun advertises RENDER, but fails to support 32-bit pixmaps.
	   * That is just no good.  Therefore, we check all screens
	   * for proper support.
	   */

	  int screen;
	  for (screen = 0; screen < ScreenCount (xdisplay); screen++)
	    {
	      int count;
	      int *depths = XListDepths (xdisplay, screen, &count);
	      gboolean has_8 = FALSE, has_32 = FALSE;

	      if (depths)
		{
		  int i;

		  for (i = 0; i < count; i++)
		    {
		      if (depths[i] == 8)
			has_8 = TRUE;
		      else if (depths[i] == 32)
			has_32 = TRUE;
		    }
		  XFree (depths);
		}

	      /* At this point, we might have a false positive;
	       * buggy versions of Xinerama only report depths for
	       * which there is an associated visual; so we actually
	       * go ahead and try create pixmaps.
	       */
	      if (!(has_8 && has_32))
		{
		  gdk_error_trap_push ();
		  if (!has_8)
		    try_pixmap (xdisplay, screen, 8);
		  if (!has_32)
		    try_pixmap (xdisplay, screen, 32);
		  XSync (xdisplay, False);
		  if (gdk_error_trap_pop () == 0)
		    {
		      has_8 = TRUE;
		      has_32 = TRUE;
		    }
		}
	      
	      if (!(has_8 && has_32))
		{
		  g_warning ("The X server advertises that RENDER support is present,\n"
			     "but fails to supply the necessary pixmap support.  In\n"
			     "other words, it is buggy.");
		  x11display->have_render = GDK_NO;
		  break;
		}
	    }
	}
    }

  return x11display->have_render == GDK_YES;
}

static Picture
gdk_x11_drawable_get_picture (GdkDrawable *drawable)
{
  GdkDrawableImplX11 *impl = GDK_DRAWABLE_IMPL_X11 (drawable);
  
  if (!impl->picture)
    {
      Display *xdisplay = GDK_SCREEN_XDISPLAY (impl->screen);
      XRenderPictFormat *format;
      
      GdkVisual *visual = gdk_drawable_get_visual (GDK_DRAWABLE_IMPL_X11 (drawable)->wrapper);
      if (!visual)
	return None;

      format = XRenderFindVisualFormat (xdisplay, GDK_VISUAL_XVISUAL (visual));
      if (format)
	{
	  XRenderPictureAttributes attributes;
	  attributes.graphics_exposures = False;
	  
	  impl->picture = XRenderCreatePicture (xdisplay, impl->xid, format,
						CPGraphicsExposure, &attributes);
	}
    }
  
  return impl->picture;
}

static void
gdk_x11_drawable_update_picture_clip (GdkDrawable *drawable,
				      GdkGC       *gc)
{
  GdkDrawableImplX11 *impl = GDK_DRAWABLE_IMPL_X11 (drawable);
  Display *xdisplay = GDK_SCREEN_XDISPLAY (impl->screen);
  Picture picture = gdk_x11_drawable_get_picture (drawable);
  GdkRegion *clip_region = gc ? _gdk_gc_get_clip_region (gc) : NULL;

  if (clip_region)
    {
      GdkRegionBox *boxes = clip_region->rects;
      gint n_boxes = clip_region->numRects;
      XRectangle *rects = g_new (XRectangle, n_boxes);
      int i;

      for (i=0; i < n_boxes; i++)
	{
	  rects[i].x = CLAMP (boxes[i].x1 + gc->clip_x_origin, G_MINSHORT, G_MAXSHORT);
	  rects[i].y = CLAMP (boxes[i].y1 + gc->clip_y_origin, G_MINSHORT, G_MAXSHORT);
	  rects[i].width = CLAMP (boxes[i].x2 + gc->clip_x_origin, G_MINSHORT, G_MAXSHORT) - rects[i].x;
	  rects[i].height = CLAMP (boxes[i].y2 + gc->clip_y_origin, G_MINSHORT, G_MAXSHORT) - rects[i].y;
	}
      
      XRenderSetPictureClipRectangles (xdisplay, picture,
				       0, 0, rects, n_boxes);
      
      g_free (rects);
    }
  else
    {
      XRenderPictureAttributes pa;
      GdkBitmap *mask;
      gulong pa_mask;

      pa_mask = CPClipMask;
      if (gc && (mask = _gdk_gc_get_clip_mask (gc)))
	{
	  pa.clip_mask = GDK_PIXMAP_XID (mask);
	  pa.clip_x_origin = gc->clip_x_origin;
	  pa.clip_y_origin = gc->clip_y_origin;
	  pa_mask |= CPClipXOrigin | CPClipYOrigin;
	}
      else
	pa.clip_mask = None;

      XRenderChangePicture (xdisplay, picture,
			    pa_mask, &pa);
    }
}

/*****************************************************
 * X11 specific implementations of generic functions *
 *****************************************************/

static GdkColormap*
gdk_x11_get_colormap (GdkDrawable *drawable)
{
  GdkDrawableImplX11 *impl;

  impl = GDK_DRAWABLE_IMPL_X11 (drawable);

  return impl->colormap;
}

static void
gdk_x11_set_colormap (GdkDrawable *drawable,
                      GdkColormap *colormap)
{
  GdkDrawableImplX11 *impl;

  impl = GDK_DRAWABLE_IMPL_X11 (drawable);

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

static void
gdk_x11_draw_rectangle (GdkDrawable *drawable,
			GdkGC       *gc,
			gboolean     filled,
			gint         x,
			gint         y,
			gint         width,
			gint         height)
{
  GdkDrawableImplX11 *impl;

  impl = GDK_DRAWABLE_IMPL_X11 (drawable);
  
  if (filled)
    XFillRectangle (GDK_SCREEN_XDISPLAY (impl->screen), impl->xid,
		    GDK_GC_GET_XGC (gc), x, y, width, height);
  else
    XDrawRectangle (GDK_SCREEN_XDISPLAY (impl->screen), impl->xid,
		    GDK_GC_GET_XGC (gc), x, y, width, height);
}

static void
gdk_x11_draw_arc (GdkDrawable *drawable,
		  GdkGC       *gc,
		  gboolean     filled,
		  gint         x,
		  gint         y,
		  gint         width,
		  gint         height,
		  gint         angle1,
		  gint         angle2)
{
  GdkDrawableImplX11 *impl;

  impl = GDK_DRAWABLE_IMPL_X11 (drawable);

  
  if (filled)
    XFillArc (GDK_SCREEN_XDISPLAY (impl->screen), impl->xid,
	      GDK_GC_GET_XGC (gc), x, y, width, height, angle1, angle2);
  else
    XDrawArc (GDK_SCREEN_XDISPLAY (impl->screen), impl->xid,
	      GDK_GC_GET_XGC (gc), x, y, width, height, angle1, angle2);
}

static void
gdk_x11_draw_polygon (GdkDrawable *drawable,
		      GdkGC       *gc,
		      gboolean     filled,
		      GdkPoint    *points,
		      gint         npoints)
{
  XPoint *tmp_points;
  gint tmp_npoints, i;
  GdkDrawableImplX11 *impl;

  impl = GDK_DRAWABLE_IMPL_X11 (drawable);

  
  if (!filled &&
      (points[0].x != points[npoints-1].x || points[0].y != points[npoints-1].y))
    {
      tmp_npoints = npoints + 1;
      tmp_points = g_new (XPoint, tmp_npoints);
      tmp_points[npoints].x = points[0].x;
      tmp_points[npoints].y = points[0].y;
    }
  else
    {
      tmp_npoints = npoints;
      tmp_points = g_new (XPoint, tmp_npoints);
    }

  for (i=0; i<npoints; i++)
    {
      tmp_points[i].x = points[i].x;
      tmp_points[i].y = points[i].y;
    }
  
  if (filled)
    XFillPolygon (GDK_SCREEN_XDISPLAY (impl->screen), impl->xid,
		  GDK_GC_GET_XGC (gc), tmp_points, tmp_npoints, Complex, CoordModeOrigin);
  else
    XDrawLines (GDK_SCREEN_XDISPLAY (impl->screen), impl->xid,
		GDK_GC_GET_XGC (gc), tmp_points, tmp_npoints, CoordModeOrigin);

  g_free (tmp_points);
}

/* gdk_x11_draw_text
 *
 * Modified by Li-Da Lho to draw 16 bits and Multibyte strings
 *
 * Interface changed: add "GdkFont *font" to specify font or fontset explicitely
 */
static void
gdk_x11_draw_text (GdkDrawable *drawable,
		   GdkFont     *font,
		   GdkGC       *gc,
		   gint         x,
		   gint         y,
		   const gchar *text,
		   gint         text_length)
{
  GdkDrawableImplX11 *impl;
  Display *xdisplay;

  impl = GDK_DRAWABLE_IMPL_X11 (drawable);
  xdisplay = GDK_SCREEN_XDISPLAY (impl->screen);
  
  if (font->type == GDK_FONT_FONT)
    {
      XFontStruct *xfont = (XFontStruct *) GDK_FONT_XFONT (font);
      XSetFont(xdisplay, GDK_GC_GET_XGC (gc), xfont->fid);
      if ((xfont->min_byte1 == 0) && (xfont->max_byte1 == 0))
	{
	  XDrawString (xdisplay, impl->xid,
		       GDK_GC_GET_XGC (gc), x, y, text, text_length);
	}
      else
	{
	  XDrawString16 (xdisplay, impl->xid,
			 GDK_GC_GET_XGC (gc), x, y, (XChar2b *) text, text_length / 2);
	}
    }
  else if (font->type == GDK_FONT_FONTSET)
    {
      XFontSet fontset = (XFontSet) GDK_FONT_XFONT (font);
      XmbDrawString (xdisplay, impl->xid,
		     fontset, GDK_GC_GET_XGC (gc), x, y, text, text_length);
    }
  else
    g_error("undefined font type\n");
}

static void
gdk_x11_draw_text_wc (GdkDrawable    *drawable,
		      GdkFont	     *font,
		      GdkGC	     *gc,
		      gint	      x,
		      gint	      y,
		      const GdkWChar *text,
		      gint	      text_length)
{
  GdkDrawableImplX11 *impl;
  Display *xdisplay;

  impl = GDK_DRAWABLE_IMPL_X11 (drawable);
  xdisplay = GDK_SCREEN_XDISPLAY (impl->screen);
  
  if (font->type == GDK_FONT_FONT)
    {
      XFontStruct *xfont = (XFontStruct *) GDK_FONT_XFONT (font);
      gchar *text_8bit;
      gint i;
      XSetFont(xdisplay, GDK_GC_GET_XGC (gc), xfont->fid);
      text_8bit = g_new (gchar, text_length);
      for (i=0; i<text_length; i++) text_8bit[i] = text[i];
      XDrawString (xdisplay, impl->xid,
                   GDK_GC_GET_XGC (gc), x, y, text_8bit, text_length);
      g_free (text_8bit);
    }
  else if (font->type == GDK_FONT_FONTSET)
    {
      if (sizeof(GdkWChar) == sizeof(wchar_t))
	{
	  XwcDrawString (xdisplay, impl->xid,
			 (XFontSet) GDK_FONT_XFONT (font),
			 GDK_GC_GET_XGC (gc), x, y, (wchar_t *)text, text_length);
	}
      else
	{
	  wchar_t *text_wchar;
	  gint i;
	  text_wchar = g_new (wchar_t, text_length);
	  for (i=0; i<text_length; i++) text_wchar[i] = text[i];
	  XwcDrawString (xdisplay, impl->xid,
			 (XFontSet) GDK_FONT_XFONT (font),
			 GDK_GC_GET_XGC (gc), x, y, text_wchar, text_length);
	  g_free (text_wchar);
	}
    }
  else
    g_error("undefined font type\n");
}

static void
gdk_x11_draw_drawable (GdkDrawable *drawable,
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
  GdkDrawableImplX11 *impl;
  GdkDrawableImplX11 *src_impl;
  
  impl = GDK_DRAWABLE_IMPL_X11 (drawable);

  if (GDK_IS_DRAWABLE_IMPL_X11 (src))
    src_impl = GDK_DRAWABLE_IMPL_X11 (src);
  else if (GDK_IS_WINDOW (src))
    src_impl = GDK_DRAWABLE_IMPL_X11(((GdkWindowObject *)src)->impl);
  else
    src_impl = GDK_DRAWABLE_IMPL_X11(((GdkPixmapObject *)src)->impl);

  if (GDK_IS_WINDOW_IMPL_X11 (impl) &&
      GDK_IS_PIXMAP_IMPL_X11 (src_impl))
    {
      GdkPixmapImplX11 *src_pixmap = GDK_PIXMAP_IMPL_X11 (src_impl);
      /* Work around an Xserver bug where non-visible areas from
       * a pixmap to a window will clear the window background
       * in destination areas that are supposed to be clipped out.
       * This is a problem with client side windows as this means
       * things may draw outside the virtual windows. This could
       * also happen for window to window copies, but I don't
       * think we generate any calls like that.
       *
       * See: 
       * http://lists.freedesktop.org/archives/xorg/2009-February/043318.html
       */
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
  
  if (src_depth == 1)
    {
      XCopyArea (GDK_SCREEN_XDISPLAY (impl->screen),
                 src_impl->xid,
		 impl->xid,
		 GDK_GC_GET_XGC (gc),
		 xsrc, ysrc,
		 width, height,
		 xdest, ydest);
    }
  else if (dest_depth != 0 && src_depth == dest_depth)
    {
      XCopyArea (GDK_SCREEN_XDISPLAY (impl->screen),
                 src_impl->xid,
		 impl->xid,
		 GDK_GC_GET_XGC (gc),
		 xsrc, ysrc,
		 width, height,
		 xdest, ydest);
    }
  else
    g_warning ("Attempt to draw a drawable with depth %d to a drawable with depth %d",
               src_depth, dest_depth);
}

static void
gdk_x11_draw_points (GdkDrawable *drawable,
		     GdkGC       *gc,
		     GdkPoint    *points,
		     gint         npoints)
{
  GdkDrawableImplX11 *impl;

  impl = GDK_DRAWABLE_IMPL_X11 (drawable);

  
  /* We special-case npoints == 1, because X will merge multiple
   * consecutive XDrawPoint requests into a PolyPoint request
   */
  if (npoints == 1)
    {
      XDrawPoint (GDK_SCREEN_XDISPLAY (impl->screen),
		  impl->xid,
		  GDK_GC_GET_XGC (gc),
		  points[0].x, points[0].y);
    }
  else
    {
      gint i;
      XPoint *tmp_points = g_new (XPoint, npoints);

      for (i=0; i<npoints; i++)
	{
	  tmp_points[i].x = points[i].x;
	  tmp_points[i].y = points[i].y;
	}
      
      XDrawPoints (GDK_SCREEN_XDISPLAY (impl->screen),
		   impl->xid,
		   GDK_GC_GET_XGC (gc),
		   tmp_points,
		   npoints,
		   CoordModeOrigin);

      g_free (tmp_points);
    }
}

static void
gdk_x11_draw_segments (GdkDrawable *drawable,
		       GdkGC       *gc,
		       GdkSegment  *segs,
		       gint         nsegs)
{
  GdkDrawableImplX11 *impl;

  impl = GDK_DRAWABLE_IMPL_X11 (drawable);

  
  /* We special-case nsegs == 1, because X will merge multiple
   * consecutive XDrawLine requests into a PolySegment request
   */
  if (nsegs == 1)
    {
      XDrawLine (GDK_SCREEN_XDISPLAY (impl->screen), impl->xid,
		 GDK_GC_GET_XGC (gc), segs[0].x1, segs[0].y1,
		 segs[0].x2, segs[0].y2);
    }
  else
    {
      gint i;
      XSegment *tmp_segs = g_new (XSegment, nsegs);

      for (i=0; i<nsegs; i++)
	{
	  tmp_segs[i].x1 = segs[i].x1;
	  tmp_segs[i].x2 = segs[i].x2;
	  tmp_segs[i].y1 = segs[i].y1;
	  tmp_segs[i].y2 = segs[i].y2;
	}
      
      XDrawSegments (GDK_SCREEN_XDISPLAY (impl->screen),
		     impl->xid,
		     GDK_GC_GET_XGC (gc),
		     tmp_segs, nsegs);

      g_free (tmp_segs);
    }
}

static void
gdk_x11_draw_lines (GdkDrawable *drawable,
		    GdkGC       *gc,
		    GdkPoint    *points,
		    gint         npoints)
{
  gint i;
  XPoint *tmp_points = g_new (XPoint, npoints);
  GdkDrawableImplX11 *impl;

  impl = GDK_DRAWABLE_IMPL_X11 (drawable);

  
  for (i=0; i<npoints; i++)
    {
      tmp_points[i].x = points[i].x;
      tmp_points[i].y = points[i].y;
    }
      
  XDrawLines (GDK_SCREEN_XDISPLAY (impl->screen),
	      impl->xid,
	      GDK_GC_GET_XGC (gc),
	      tmp_points, npoints,
	      CoordModeOrigin);

  g_free (tmp_points);
}

static void
gdk_x11_draw_image     (GdkDrawable     *drawable,
                        GdkGC           *gc,
                        GdkImage        *image,
                        gint             xsrc,
                        gint             ysrc,
                        gint             xdest,
                        gint             ydest,
                        gint             width,
                        gint             height)
{
  GdkDrawableImplX11 *impl;

  impl = GDK_DRAWABLE_IMPL_X11 (drawable);

#ifdef USE_SHM  
  if (image->type == GDK_IMAGE_SHARED)
    XShmPutImage (GDK_SCREEN_XDISPLAY (impl->screen), impl->xid,
                  GDK_GC_GET_XGC (gc), GDK_IMAGE_XIMAGE (image),
                  xsrc, ysrc, xdest, ydest, width, height, False);
  else
#endif
    XPutImage (GDK_SCREEN_XDISPLAY (impl->screen), impl->xid,
               GDK_GC_GET_XGC (gc), GDK_IMAGE_XIMAGE (image),
               xsrc, ysrc, xdest, ydest, width, height);
}

static gint
gdk_x11_get_depth (GdkDrawable *drawable)
{
  /* This is a bit bogus but I'm not sure the other way is better */

  return gdk_drawable_get_depth (GDK_DRAWABLE_IMPL_X11 (drawable)->wrapper);
}

static GdkDrawable *
get_impl_drawable (GdkDrawable *drawable)
{
  if (GDK_IS_WINDOW (drawable))
    return ((GdkWindowObject *)drawable)->impl;
  else if (GDK_IS_PIXMAP (drawable))
    return ((GdkPixmapObject *)drawable)->impl;
  else
    {
      g_warning (G_STRLOC " drawable is not a pixmap or window");
      return NULL;
    }
}

static GdkScreen*
gdk_x11_get_screen (GdkDrawable *drawable)
{
  if (GDK_IS_DRAWABLE_IMPL_X11 (drawable))
    return GDK_DRAWABLE_IMPL_X11 (drawable)->screen;
  else
    return GDK_DRAWABLE_IMPL_X11 (get_impl_drawable (drawable))->screen;
}

static GdkVisual*
gdk_x11_get_visual (GdkDrawable    *drawable)
{
  return gdk_drawable_get_visual (GDK_DRAWABLE_IMPL_X11 (drawable)->wrapper);
}

/**
 * gdk_x11_drawable_get_xdisplay:
 * @drawable: a #GdkDrawable.
 * 
 * Returns the display of a #GdkDrawable.
 * 
 * Return value: an Xlib <type>Display*</type>.
 **/
Display *
gdk_x11_drawable_get_xdisplay (GdkDrawable *drawable)
{
  if (GDK_IS_DRAWABLE_IMPL_X11 (drawable))
    return GDK_SCREEN_XDISPLAY (GDK_DRAWABLE_IMPL_X11 (drawable)->screen);
  else
    return GDK_SCREEN_XDISPLAY (GDK_DRAWABLE_IMPL_X11 (get_impl_drawable (drawable))->screen);
}

/**
 * gdk_x11_drawable_get_xid:
 * @drawable: a #GdkDrawable.
 * 
 * Returns the X resource (window or pixmap) belonging to a #GdkDrawable.
 * 
 * Return value: the ID of @drawable's X resource.
 **/
XID
gdk_x11_drawable_get_xid (GdkDrawable *drawable)
{
  GdkDrawable *impl;
  
  if (GDK_IS_WINDOW (drawable))
    {
      GdkWindow *window = (GdkWindow *)drawable;
      
      /* Try to ensure the window has a native window */
      if (!_gdk_window_has_impl (window))
	{
	  gdk_window_ensure_native (window);

	  /* We sync here to ensure the window is created in the Xserver when
	   * this function returns. This is required because the returned XID
	   * for this window must be valid immediately, even with another
	   * connection to the Xserver */
	  gdk_display_sync (gdk_drawable_get_display (window));
	}
      
      if (!GDK_WINDOW_IS_X11 (window))
        {
          g_warning (G_STRLOC " drawable is not a native X11 window");
          return None;
        }
      
      impl = ((GdkWindowObject *)drawable)->impl;
    }
  else if (GDK_IS_PIXMAP (drawable))
    impl = ((GdkPixmapObject *)drawable)->impl;
  else
    {
      g_warning (G_STRLOC " drawable is not a pixmap or window");
      return None;
    }

  return ((GdkDrawableImplX11 *)impl)->xid;
}

GdkDrawable *
gdk_x11_window_get_drawable_impl (GdkWindow *window)
{
  return ((GdkWindowObject *)window)->impl;
}
GdkDrawable *
gdk_x11_pixmap_get_drawable_impl (GdkPixmap *pixmap)
{
  return ((GdkPixmapObject *)pixmap)->impl;
}

/* Code for accelerated alpha compositing using the RENDER extension.
 * It's a bit long because there are lots of possibilities for
 * what's the fastest depending on the available picture formats,
 * whether we can used shared pixmaps, etc.
 */

static GdkX11FormatType
select_format (GdkDisplay         *display,
	       XRenderPictFormat **format,
	       XRenderPictFormat **mask)
{
  Display *xdisplay = GDK_DISPLAY_XDISPLAY (display);
  XRenderPictFormat pf;

  if (!_gdk_x11_have_render (display))
    return GDK_X11_FORMAT_NONE;
  
  /* Look for a 32-bit xRGB and Axxx formats that exactly match the
   * in memory data format. We can use them as pixmap and mask
   * to deal with non-premultiplied data.
   */

  pf.type = PictTypeDirect;
  pf.depth = 32;
  pf.direct.redMask = 0xff;
  pf.direct.greenMask = 0xff;
  pf.direct.blueMask = 0xff;
  
  pf.direct.alphaMask = 0;
  if (ImageByteOrder (xdisplay) == LSBFirst)
    {
      /* ABGR */
      pf.direct.red = 0;
      pf.direct.green = 8;
      pf.direct.blue = 16;
    }
  else
    {
      /* RGBA */
      pf.direct.red = 24;
      pf.direct.green = 16;
      pf.direct.blue = 8;
    }
  
  *format = XRenderFindFormat (xdisplay,
			       (PictFormatType | PictFormatDepth |
				PictFormatRedMask | PictFormatRed |
				PictFormatGreenMask | PictFormatGreen |
				PictFormatBlueMask | PictFormatBlue |
				PictFormatAlphaMask),
			       &pf,
			       0);

  pf.direct.alphaMask = 0xff;
  if (ImageByteOrder (xdisplay) == LSBFirst)
    {
      /* ABGR */
      pf.direct.alpha = 24;
    }
  else
    {
      pf.direct.alpha = 0;
    }
  
  *mask = XRenderFindFormat (xdisplay,
			     (PictFormatType | PictFormatDepth |
			      PictFormatAlphaMask | PictFormatAlpha),
			     &pf,
			     0);

  if (*format && *mask)
    return GDK_X11_FORMAT_EXACT_MASK;

  /* OK, that failed, now look for xRGB and Axxx formats in
   * RENDER's preferred order
   */
  pf.direct.alphaMask = 0;
  /* ARGB */
  pf.direct.red = 16;
  pf.direct.green = 8;
  pf.direct.blue = 0;
  
  *format = XRenderFindFormat (xdisplay,
			       (PictFormatType | PictFormatDepth |
				PictFormatRedMask | PictFormatRed |
				PictFormatGreenMask | PictFormatGreen |
				PictFormatBlueMask | PictFormatBlue |
				PictFormatAlphaMask),
			       &pf,
			       0);

  pf.direct.alphaMask = 0xff;
  pf.direct.alpha = 24;
  
  *mask = XRenderFindFormat (xdisplay,
			     (PictFormatType | PictFormatDepth |
			      PictFormatAlphaMask | PictFormatAlpha),
			     &pf,
			     0);

  if (*format && *mask)
    return GDK_X11_FORMAT_ARGB_MASK;

  /* Finally, if neither of the above worked, fall back to
   * looking for combined ARGB -- we'll premultiply ourselves.
   */

  pf.type = PictTypeDirect;
  pf.depth = 32;
  pf.direct.red = 16;
  pf.direct.green = 8;
  pf.direct.blue = 0;
  pf.direct.alphaMask = 0xff;
  pf.direct.alpha = 24;

  *format = XRenderFindFormat (xdisplay,
			       (PictFormatType | PictFormatDepth |
				PictFormatRedMask | PictFormatRed |
				PictFormatGreenMask | PictFormatGreen |
				PictFormatBlueMask | PictFormatBlue |
				PictFormatAlphaMask | PictFormatAlpha),
			       &pf,
			       0);
  *mask = NULL;

  if (*format)
    return GDK_X11_FORMAT_ARGB;

  return GDK_X11_FORMAT_NONE;
}

#if 0
static void
list_formats (XRenderPictFormat *pf)
{
  gint i;
  
  for (i=0 ;; i++)
    {
      XRenderPictFormat *pf = XRenderFindFormat (impl->xdisplay, 0, NULL, i);
      if (pf)
	{
	  g_print ("%2d R-%#06x/%#06x G-%#06x/%#06x B-%#06x/%#06x A-%#06x/%#06x\n",
		   pf->depth,
		   pf->direct.red,
		   pf->direct.redMask,
		   pf->direct.green,
		   pf->direct.greenMask,
		   pf->direct.blue,
		   pf->direct.blueMask,
		   pf->direct.alpha,
		   pf->direct.alphaMask);
	}
      else
	break;
    }
}
#endif  

void
_gdk_x11_convert_to_format (guchar           *src_buf,
                            gint              src_rowstride,
                            guchar           *dest_buf,
                            gint              dest_rowstride,
                            GdkX11FormatType  dest_format,
                            GdkByteOrder      dest_byteorder,
                            gint              width,
                            gint              height)
{
  gint i;

  for (i=0; i < height; i++)
    {
      switch (dest_format)
	{
	case GDK_X11_FORMAT_EXACT_MASK:
	  {
	    memcpy (dest_buf + i * dest_rowstride,
		    src_buf + i * src_rowstride,
		    width * 4);
	    break;
	  }
	case GDK_X11_FORMAT_ARGB_MASK:
	  {
	    guchar *row = src_buf + i * src_rowstride;
	    if (((gsize)row & 3) != 0)
	      {
		guchar *p = row;
		guint32 *q = (guint32 *)(dest_buf + i * dest_rowstride);
		guchar *end = p + 4 * width;

		while (p < end)
		  {
		    *q = (p[3] << 24) | (p[0] << 16) | (p[1] << 8) | p[2];
		    p += 4;
		    q++;
		  }
	      }
	    else
	      {
		guint32 *p = (guint32 *)row;
		guint32 *q = (guint32 *)(dest_buf + i * dest_rowstride);
		guint32 *end = p + width;

#if G_BYTE_ORDER == G_LITTLE_ENDIAN	    
		if (dest_byteorder == GDK_LSB_FIRST)
		  {
		    /* ABGR => ARGB */
		
		    while (p < end)
		      {
			*q = ( (*p & 0xff00ff00) |
			       ((*p & 0x000000ff) << 16) |
			       ((*p & 0x00ff0000) >> 16));
			q++;
			p++;
		      }
		  }
		else
		  {
		    /* ABGR => BGRA */
		
		    while (p < end)
		      {
			*q = (((*p & 0xff000000) >> 24) |
			      ((*p & 0x00ffffff) << 8));
			q++;
			p++;
		      }
		  }
#else /* G_BYTE_ORDER == G_BIG_ENDIAN */
		if (dest_byteorder == GDK_LSB_FIRST)
		  {
		    /* RGBA => BGRA */
		
		    while (p < end)
		      {
			*q = ( (*p & 0x00ff00ff) |
			       ((*p & 0x0000ff00) << 16) |
			       ((*p & 0xff000000) >> 16));
			q++;
			p++;
		      }
		  }
		else
		  {
		    /* RGBA => ARGB */
		
		    while (p < end)
		      {
			*q = (((*p & 0xffffff00) >> 8) |
			      ((*p & 0x000000ff) << 24));
			q++;
			p++;
		      }
		  }
#endif /* G_BYTE_ORDER*/	    
	      }
	    break;
	  }
	case GDK_X11_FORMAT_ARGB:
	  {
	    guchar *p = (src_buf + i * src_rowstride);
	    guchar *q = (dest_buf + i * dest_rowstride);
	    guchar *end = p + 4 * width;
	    guint t1,t2,t3;
	    
#define MULT(d,c,a,t) G_STMT_START { t = c * a; d = ((t >> 8) + t) >> 8; } G_STMT_END
	    
	    if (dest_byteorder == GDK_LSB_FIRST)
	      {
		while (p < end)
		  {
		    MULT(q[0], p[2], p[3], t1);
		    MULT(q[1], p[1], p[3], t2);
		    MULT(q[2], p[0], p[3], t3);
		    q[3] = p[3];
		    p += 4;
		    q += 4;
		  }
	      }
	    else
	      {
		while (p < end)
		  {
		    q[0] = p[3];
		    MULT(q[1], p[0], p[3], t1);
		    MULT(q[2], p[1], p[3], t2);
		    MULT(q[3], p[2], p[3], t3);
		    p += 4;
		    q += 4;
		  }
	      }
#undef MULT
	    break;
	  }
	case GDK_X11_FORMAT_NONE:
	  g_assert_not_reached ();
	  break;
	}
    }
}

static void
draw_with_images (GdkDrawable       *drawable,
		  GdkGC             *gc,
		  GdkX11FormatType   format_type,
		  XRenderPictFormat *format,
		  XRenderPictFormat *mask_format,
		  guchar            *src_rgb,
		  gint               src_rowstride,
		  gint               dest_x,
		  gint               dest_y,
		  gint               width,
		  gint               height)
{
  GdkScreen *screen = GDK_DRAWABLE_IMPL_X11 (drawable)->screen;
  Display *xdisplay = GDK_SCREEN_XDISPLAY (screen);
  GdkImage *image;
  GdkPixmap *pix;
  GdkGC *pix_gc;
  Picture pict;
  Picture dest_pict;
  Picture mask = None;
  gint x0, y0;

  pix = gdk_pixmap_new (gdk_screen_get_root_window (screen), width, height, 32);
						  
  pict = XRenderCreatePicture (xdisplay, 
			       GDK_PIXMAP_XID (pix),
			       format, 0, NULL);
  if (mask_format)
    mask = XRenderCreatePicture (xdisplay, 
				 GDK_PIXMAP_XID (pix),
				 mask_format, 0, NULL);

  dest_pict = gdk_x11_drawable_get_picture (drawable);  
  
  pix_gc = _gdk_drawable_get_scratch_gc (pix, FALSE);

  for (y0 = 0; y0 < height; y0 += GDK_SCRATCH_IMAGE_HEIGHT)
    {
      gint height1 = MIN (height - y0, GDK_SCRATCH_IMAGE_HEIGHT);
      for (x0 = 0; x0 < width; x0 += GDK_SCRATCH_IMAGE_WIDTH)
	{
	  gint xs0, ys0;
	  
	  gint width1 = MIN (width - x0, GDK_SCRATCH_IMAGE_WIDTH);
	  
	  image = _gdk_image_get_scratch (screen, width1, height1, 32, &xs0, &ys0);
	  
	  _gdk_x11_convert_to_format (src_rgb + y0 * src_rowstride + 4 * x0, src_rowstride,
                                      (guchar *)image->mem + ys0 * image->bpl + xs0 * image->bpp, image->bpl,
                                      format_type, image->byte_order, 
                                      width1, height1);

	  gdk_draw_image (pix, pix_gc,
			  image, xs0, ys0, x0, y0, width1, height1);
	}
    }
  
  XRenderComposite (xdisplay, PictOpOver, pict, mask, dest_pict, 
		    0, 0, 0, 0, dest_x, dest_y, width, height);

  XRenderFreePicture (xdisplay, pict);
  if (mask)
    XRenderFreePicture (xdisplay, mask);
  
  g_object_unref (pix);
}

typedef struct _ShmPixmapInfo ShmPixmapInfo;

struct _ShmPixmapInfo
{
  Display  *display;
  Pixmap    pix;
  Picture   pict;
  Picture   mask;
};

static void
shm_pixmap_info_destroy (gpointer data)
{
  ShmPixmapInfo *info = data;

  if (info->pict != None)
    XRenderFreePicture (info->display, info->pict);
  if (info->mask != None)
    XRenderFreePicture (info->display, info->mask);

  g_free (data);
}


#ifdef USE_SHM
/* Returns FALSE if we can't get a shm pixmap */
static gboolean
get_shm_pixmap_for_image (Display           *xdisplay,
			  GdkImage          *image,
			  XRenderPictFormat *format,
			  XRenderPictFormat *mask_format,
			  Pixmap            *pix,
			  Picture           *pict,
			  Picture           *mask)
{
  ShmPixmapInfo *info;
  
  if (image->type != GDK_IMAGE_SHARED)
    return FALSE;
  
  info = g_object_get_data (G_OBJECT (image), "gdk-x11-shm-pixmap");
  if (!info)
    {
      *pix = _gdk_x11_image_get_shm_pixmap (image);
      
      if (!*pix)
	return FALSE;
      
      info = g_new (ShmPixmapInfo, 1);
      info->display = xdisplay;
      info->pix = *pix;
      
      info->pict = XRenderCreatePicture (xdisplay, info->pix,
					 format, 0, NULL);
      if (mask_format)
	info->mask = XRenderCreatePicture (xdisplay, info->pix,
					   mask_format, 0, NULL);
      else
	info->mask = None;

      g_object_set_data_full (G_OBJECT (image), "gdk-x11-shm-pixmap", info,
	  shm_pixmap_info_destroy);
    }

  *pix = info->pix;
  *pict = info->pict;
  *mask = info->mask;

  return TRUE;
}

/* Returns FALSE if drawing with ShmPixmaps is not possible */
static gboolean
draw_with_pixmaps (GdkDrawable       *drawable,
		   GdkGC             *gc,
		   GdkX11FormatType   format_type,
		   XRenderPictFormat *format,
		   XRenderPictFormat *mask_format,
		   guchar            *src_rgb,
		   gint               src_rowstride,
		   gint               dest_x,
		   gint               dest_y,
		   gint               width,
		   gint               height)
{
  Display *xdisplay = GDK_SCREEN_XDISPLAY (GDK_DRAWABLE_IMPL_X11 (drawable)->screen);
  GdkImage *image;
  Pixmap pix;
  Picture pict;
  Picture dest_pict;
  Picture mask = None;
  gint x0, y0;

  dest_pict = gdk_x11_drawable_get_picture (drawable);
  
  for (y0 = 0; y0 < height; y0 += GDK_SCRATCH_IMAGE_HEIGHT)
    {
      gint height1 = MIN (height - y0, GDK_SCRATCH_IMAGE_HEIGHT);
      for (x0 = 0; x0 < width; x0 += GDK_SCRATCH_IMAGE_WIDTH)
	{
	  gint xs0, ys0;
	  
	  gint width1 = MIN (width - x0, GDK_SCRATCH_IMAGE_WIDTH);
	  
	  image = _gdk_image_get_scratch (GDK_DRAWABLE_IMPL_X11 (drawable)->screen,
					  width1, height1, 32, &xs0, &ys0);
	  if (!get_shm_pixmap_for_image (xdisplay, image, format, mask_format, &pix, &pict, &mask))
	    return FALSE;

	  _gdk_x11_convert_to_format (src_rgb + y0 * src_rowstride + 4 * x0, src_rowstride,
                                      (guchar *)image->mem + ys0 * image->bpl + xs0 * image->bpp, image->bpl,
                                      format_type, image->byte_order, 
                                      width1, height1);

	  XRenderComposite (xdisplay, PictOpOver, pict, mask, dest_pict, 
			    xs0, ys0, xs0, ys0, x0 + dest_x, y0 + dest_y,
			    width1, height1);
	}
    }

  return TRUE;
}
#endif

static void
gdk_x11_draw_pixbuf (GdkDrawable     *drawable,
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
  GdkX11FormatType format_type;
  XRenderPictFormat *format, *mask_format;
  gint rowstride;
#ifdef USE_SHM  
  gboolean use_pixmaps = TRUE;
#endif /* USE_SHM */
    
  format_type = select_format (gdk_drawable_get_display (drawable),
			       &format, &mask_format);

  if (format_type == GDK_X11_FORMAT_NONE ||
      !gdk_pixbuf_get_has_alpha (pixbuf) ||
      gdk_drawable_get_depth (drawable) == 1 ||
      (dither == GDK_RGB_DITHER_MAX && gdk_drawable_get_depth (drawable) != 24) ||
      gdk_x11_drawable_get_picture (drawable) == None)
    {
      GdkDrawable *wrapper = GDK_DRAWABLE_IMPL_X11 (drawable)->wrapper;
      GDK_DRAWABLE_CLASS (_gdk_drawable_impl_x11_parent_class)->draw_pixbuf (wrapper, gc, pixbuf,
									     src_x, src_y, dest_x, dest_y,
									     width, height,
									     dither, x_dither, y_dither);
      return;
    }

  gdk_x11_drawable_update_picture_clip (drawable, gc);

  rowstride = gdk_pixbuf_get_rowstride (pixbuf);

#ifdef USE_SHM
  if (use_pixmaps)
    {
      if (!draw_with_pixmaps (drawable, gc,
			      format_type, format, mask_format,
			      gdk_pixbuf_get_pixels (pixbuf) + src_y * rowstride + src_x * 4,
			      rowstride,
			      dest_x, dest_y, width, height))
	use_pixmaps = FALSE;
    }

  if (!use_pixmaps)
#endif /* USE_SHM */
    draw_with_images (drawable, gc,
		      format_type, format, mask_format,
		      gdk_pixbuf_get_pixels (pixbuf) + src_y * rowstride + src_x * 4,
		      rowstride,
		      dest_x, dest_y, width, height);
}

static void
gdk_x11_cairo_surface_destroy (void *data)
{
  GdkDrawableImplX11 *impl = data;

  impl->cairo_surface = NULL;
}

void
_gdk_windowing_set_cairo_surface_size (cairo_surface_t *surface,
				       int width,
				       int height)
{
  cairo_xlib_surface_set_size (surface, width, height);
}

cairo_surface_t *
_gdk_windowing_create_cairo_surface (GdkDrawable *drawable,
				     int width,
				     int height)
{
  GdkDrawableImplX11 *impl = GDK_DRAWABLE_IMPL_X11 (drawable);
  GdkVisual *visual;
    
  visual = gdk_drawable_get_visual (drawable);
  if (visual) 
    return cairo_xlib_surface_create (GDK_SCREEN_XDISPLAY (impl->screen),
				      impl->xid,
				      GDK_VISUAL_XVISUAL (visual),
				      width, height);
  else if (gdk_drawable_get_depth (drawable) == 1)
    return cairo_xlib_surface_create_for_bitmap (GDK_SCREEN_XDISPLAY (impl->screen),
						    impl->xid,
						    GDK_SCREEN_XSCREEN (impl->screen),
						    width, height);
  else
    {
      g_warning ("Using Cairo rendering requires the drawable argument to\n"
		 "have a specified colormap. All windows have a colormap,\n"
		 "however, pixmaps only have colormap by default if they\n"
		 "were created with a non-NULL window argument. Otherwise\n"
		 "a colormap must be set on them with gdk_drawable_set_colormap");
      return NULL;
    }
  
}

static cairo_surface_t *
gdk_x11_ref_cairo_surface (GdkDrawable *drawable)
{
  GdkDrawableImplX11 *impl = GDK_DRAWABLE_IMPL_X11 (drawable);

  if (GDK_IS_WINDOW_IMPL_X11 (drawable) &&
      GDK_WINDOW_DESTROYED (impl->wrapper))
    return NULL;

  if (!impl->cairo_surface)
    {
      int width, height;
  
      gdk_drawable_get_size (impl->wrapper, &width, &height);

      impl->cairo_surface = _gdk_windowing_create_cairo_surface (drawable, width, height);
      
      if (impl->cairo_surface)
	cairo_surface_set_user_data (impl->cairo_surface, &gdk_x11_cairo_key,
				     drawable, gdk_x11_cairo_surface_destroy);
    }
  else
    cairo_surface_reference (impl->cairo_surface);

  return impl->cairo_surface;
}

#define __GDK_DRAWABLE_X11_C__
#include "gdkaliasdef.c"
