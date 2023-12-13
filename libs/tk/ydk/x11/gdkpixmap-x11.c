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
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
/* Needed for SEEK_END in SunOS */
#include <unistd.h>
#include <X11/Xlib.h>

#include "gdkx.h"

#include "gdkpixmap-x11.h"
#include "gdkprivate-x11.h"
#include "gdkscreen-x11.h"
#include "gdkdisplay-x11.h"

#include <gdk/gdkinternals.h>
#include "gdkalias.h"

typedef struct
{
  gchar *color_string;
  GdkColor color;
  gint transparent;
} _GdkPixmapColor;

typedef struct
{
  guint ncolors;
  GdkColormap *colormap;
  gulong pixels[1];
} _GdkPixmapInfo;

static void gdk_pixmap_impl_x11_get_size   (GdkDrawable        *drawable,
                                            gint               *width,
                                            gint               *height);

static void gdk_pixmap_impl_x11_dispose    (GObject            *object);
static void gdk_pixmap_impl_x11_finalize   (GObject            *object);

G_DEFINE_TYPE (GdkPixmapImplX11, gdk_pixmap_impl_x11, GDK_TYPE_DRAWABLE_IMPL_X11)

GType
_gdk_pixmap_impl_get_type (void)
{
  return gdk_pixmap_impl_x11_get_type ();
}

static void
gdk_pixmap_impl_x11_init (GdkPixmapImplX11 *impl)
{
  impl->width = 1;
  impl->height = 1;
}

static void
gdk_pixmap_impl_x11_class_init (GdkPixmapImplX11Class *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GdkDrawableClass *drawable_class = GDK_DRAWABLE_CLASS (klass);
  
  object_class->dispose  = gdk_pixmap_impl_x11_dispose;
  object_class->finalize = gdk_pixmap_impl_x11_finalize;

  drawable_class->get_size = gdk_pixmap_impl_x11_get_size;
}

static void
gdk_pixmap_impl_x11_dispose (GObject *object)
{
  GdkPixmapImplX11 *impl = GDK_PIXMAP_IMPL_X11 (object);
  GdkPixmap *wrapper = GDK_PIXMAP (GDK_DRAWABLE_IMPL_X11 (impl)->wrapper);
  GdkDisplay *display = GDK_PIXMAP_DISPLAY (wrapper);

  if (!display->closed)
    {
      if (!impl->is_foreign)
	XFreePixmap (GDK_DISPLAY_XDISPLAY (display), GDK_PIXMAP_XID (wrapper));
    }

  _gdk_xid_table_remove (display, GDK_PIXMAP_XID (wrapper));

  G_OBJECT_CLASS (gdk_pixmap_impl_x11_parent_class)->dispose (object);
}

static void
gdk_pixmap_impl_x11_finalize (GObject *object)
{
  GdkPixmapImplX11 *impl = GDK_PIXMAP_IMPL_X11 (object);
  GdkPixmap *wrapper = GDK_PIXMAP (GDK_DRAWABLE_IMPL_X11 (impl)->wrapper);
  GdkDisplay *display = GDK_PIXMAP_DISPLAY (wrapper);

  if (!display->closed)
    {
      GdkDrawableImplX11 *draw_impl = GDK_DRAWABLE_IMPL_X11 (impl);

      _gdk_x11_drawable_finish (GDK_DRAWABLE (draw_impl));
    }

  G_OBJECT_CLASS (gdk_pixmap_impl_x11_parent_class)->finalize (object);
}

static void
gdk_pixmap_impl_x11_get_size   (GdkDrawable *drawable,
                                gint        *width,
                                gint        *height)
{
  if (width)
    *width = GDK_PIXMAP_IMPL_X11 (drawable)->width;
  if (height)
    *height = GDK_PIXMAP_IMPL_X11 (drawable)->height;
}

GdkPixmap*
_gdk_pixmap_new (GdkDrawable *drawable,
                 gint         width,
                 gint         height,
                 gint         depth)
{
  GdkPixmap *pixmap;
  GdkDrawableImplX11 *draw_impl;
  GdkPixmapImplX11 *pix_impl;
  GdkColormap *cmap;
  gint window_depth;
  
  g_return_val_if_fail (drawable == NULL || GDK_IS_DRAWABLE (drawable), NULL);
  g_return_val_if_fail ((drawable != NULL) || (depth != -1), NULL);
  g_return_val_if_fail ((width != 0) && (height != 0), NULL);
  
  if (!drawable)
    {
      GDK_NOTE (MULTIHEAD, g_message ("need to specify the screen parent window "
				      "for gdk_pixmap_new() to be multihead safe"));
      drawable = gdk_screen_get_root_window (gdk_screen_get_default ());
    }

  if (GDK_IS_WINDOW (drawable) && GDK_WINDOW_DESTROYED (drawable))
    return NULL;

  window_depth = gdk_drawable_get_depth (GDK_DRAWABLE (drawable));
  if (depth == -1)
    depth = window_depth;

  pixmap = g_object_new (gdk_pixmap_get_type (), NULL);
  draw_impl = GDK_DRAWABLE_IMPL_X11 (GDK_PIXMAP_OBJECT (pixmap)->impl);
  pix_impl = GDK_PIXMAP_IMPL_X11 (GDK_PIXMAP_OBJECT (pixmap)->impl);
  draw_impl->wrapper = GDK_DRAWABLE (pixmap);
  
  draw_impl->screen = GDK_WINDOW_SCREEN (drawable);
  draw_impl->xid = XCreatePixmap (GDK_PIXMAP_XDISPLAY (pixmap),
                                  GDK_WINDOW_XID (drawable),
                                  width, height, depth);
  
  pix_impl->is_foreign = FALSE;
  pix_impl->width = width;
  pix_impl->height = height;
  GDK_PIXMAP_OBJECT (pixmap)->depth = depth;

  if (depth == window_depth)
    {
      cmap = gdk_drawable_get_colormap (drawable);
      if (cmap)
        gdk_drawable_set_colormap (pixmap, cmap);
    }
  
  _gdk_xid_table_insert (GDK_WINDOW_DISPLAY (drawable), 
			 &GDK_PIXMAP_XID (pixmap), pixmap);
  return pixmap;
}

GdkPixmap *
_gdk_bitmap_create_from_data (GdkDrawable *drawable,
                              const gchar *data,
                              gint         width,
                              gint         height)
{
  GdkPixmap *pixmap;
  GdkDrawableImplX11 *draw_impl;
  GdkPixmapImplX11 *pix_impl;
  
  g_return_val_if_fail (data != NULL, NULL);
  g_return_val_if_fail ((width != 0) && (height != 0), NULL);
  g_return_val_if_fail (drawable == NULL || GDK_IS_DRAWABLE (drawable), NULL);

  if (!drawable)
    {
      GDK_NOTE (MULTIHEAD, g_message ("need to specify the screen parent window "
				     "for gdk_bitmap_create_from_data() to be multihead safe"));
      drawable = gdk_screen_get_root_window (gdk_screen_get_default ());
    }
  
  if (GDK_IS_WINDOW (drawable) && GDK_WINDOW_DESTROYED (drawable))
    return NULL;

  pixmap = g_object_new (gdk_pixmap_get_type (), NULL);
  draw_impl = GDK_DRAWABLE_IMPL_X11 (GDK_PIXMAP_OBJECT (pixmap)->impl);
  pix_impl = GDK_PIXMAP_IMPL_X11 (GDK_PIXMAP_OBJECT (pixmap)->impl);
  draw_impl->wrapper = GDK_DRAWABLE (pixmap);

  pix_impl->is_foreign = FALSE;
  pix_impl->width = width;
  pix_impl->height = height;
  GDK_PIXMAP_OBJECT (pixmap)->depth = 1;

  draw_impl->screen = GDK_WINDOW_SCREEN (drawable);
  draw_impl->xid = XCreateBitmapFromData (GDK_WINDOW_XDISPLAY (drawable),
                                          GDK_WINDOW_XID (drawable),
                                          (char *)data, width, height);

  _gdk_xid_table_insert (GDK_WINDOW_DISPLAY (drawable), 
			 &GDK_PIXMAP_XID (pixmap), pixmap);
  return pixmap;
}

GdkPixmap*
_gdk_pixmap_create_from_data (GdkDrawable    *drawable,
			      const gchar    *data,
			      gint            width,
			      gint            height,
			      gint            depth,
			      const GdkColor *fg,
			      const GdkColor *bg)
{
  GdkPixmap *pixmap;
  GdkDrawableImplX11 *draw_impl;
  GdkPixmapImplX11 *pix_impl;

  g_return_val_if_fail (drawable == NULL || GDK_IS_DRAWABLE (drawable), NULL);
  g_return_val_if_fail (data != NULL, NULL);
  g_return_val_if_fail (fg != NULL, NULL);
  g_return_val_if_fail (bg != NULL, NULL);
  g_return_val_if_fail ((drawable != NULL) || (depth != -1), NULL);
  g_return_val_if_fail ((width != 0) && (height != 0), NULL);

  if (!drawable)
    {
      GDK_NOTE (MULTIHEAD, g_message ("need to specify the screen parent window"
				      "for gdk_pixmap_create_from_data() to be multihead safe"));
      drawable = gdk_screen_get_root_window (gdk_screen_get_default ());
    }

  if (GDK_IS_WINDOW (drawable) && GDK_WINDOW_DESTROYED (drawable))
    return NULL;

  if (depth == -1)
    depth = gdk_drawable_get_visual (drawable)->depth;

  pixmap = g_object_new (gdk_pixmap_get_type (), NULL);
  draw_impl = GDK_DRAWABLE_IMPL_X11 (GDK_PIXMAP_OBJECT (pixmap)->impl);
  pix_impl = GDK_PIXMAP_IMPL_X11 (GDK_PIXMAP_OBJECT (pixmap)->impl);
  draw_impl->wrapper = GDK_DRAWABLE (pixmap);
  
  pix_impl->is_foreign = FALSE;
  pix_impl->width = width;
  pix_impl->height = height;
  GDK_PIXMAP_OBJECT (pixmap)->depth = depth;

  draw_impl->screen = GDK_DRAWABLE_SCREEN (drawable);
  draw_impl->xid = XCreatePixmapFromBitmapData (GDK_WINDOW_XDISPLAY (drawable),
                                                GDK_WINDOW_XID (drawable),
                                                (char *)data, width, height,
                                                fg->pixel, bg->pixel, depth);

  _gdk_xid_table_insert (GDK_WINDOW_DISPLAY (drawable),
			 &GDK_PIXMAP_XID (pixmap), pixmap);
  return pixmap;
}

/**
 * gdk_pixmap_foreign_new_for_display:
 * @display: The #GdkDisplay where @anid is located.
 * @anid: a native pixmap handle.
 * 
 * Wraps a native pixmap in a #GdkPixmap.
 * This may fail if the pixmap has been destroyed.
 *
 * For example in the X backend, a native pixmap handle is an Xlib
 * <type>XID</type>.
 *
 * Return value: the newly-created #GdkPixmap wrapper for the 
 *    native pixmap or %NULL if the pixmap has been destroyed.
 *
 * Since: 2.2
 **/
GdkPixmap *
gdk_pixmap_foreign_new_for_display (GdkDisplay      *display,
				    GdkNativeWindow  anid)
{
  Pixmap xpixmap;
  Window root_return;
  GdkScreen *screen;
  int x_ret, y_ret;
  unsigned int w_ret, h_ret, bw_ret, depth_ret;

  g_return_val_if_fail (GDK_IS_DISPLAY (display), NULL);

  /* check to make sure we were passed something at
   * least a little sane */
  g_return_val_if_fail ((anid != 0), NULL);
  
  /* set the pixmap to the passed in value */
  xpixmap = anid;

  /* get information about the Pixmap to fill in the structure for
     the gdk window */
  if (!XGetGeometry (GDK_DISPLAY_XDISPLAY (display),
		     xpixmap, &root_return,
		     &x_ret, &y_ret, &w_ret, &h_ret, &bw_ret, &depth_ret))
    return NULL;
  
  screen = _gdk_x11_display_screen_for_xrootwin (display, root_return);
  return gdk_pixmap_foreign_new_for_screen (screen, anid, w_ret, h_ret, depth_ret);
}

/**
 * gdk_pixmap_foreign_new_for_screen:
 * @screen: a #GdkScreen
 * @anid: a native pixmap handle
 * @width: the width of the pixmap identified by @anid
 * @height: the height of the pixmap identified by @anid
 * @depth: the depth of the pixmap identified by @anid
 *
 * Wraps a native pixmap in a #GdkPixmap.
 * This may fail if the pixmap has been destroyed.
 *
 * For example in the X backend, a native pixmap handle is an Xlib
 * <type>XID</type>.
 *
 * This function is an alternative to gdk_pixmap_foreign_new_for_display()
 * for cases where the dimensions of the pixmap are known. For the X
 * backend, this avoids a roundtrip to the server.
 *
 * Return value: the newly-created #GdkPixmap wrapper for the 
 *    native pixmap or %NULL if the pixmap has been destroyed.
 * 
 * Since: 2.10
 */
GdkPixmap *
gdk_pixmap_foreign_new_for_screen (GdkScreen       *screen,
				   GdkNativeWindow  anid,
				   gint             width,
				   gint             height,
				   gint             depth)
{
  Pixmap xpixmap;
  GdkPixmap *pixmap;
  GdkDrawableImplX11 *draw_impl;
  GdkPixmapImplX11 *pix_impl;

  g_return_val_if_fail (GDK_IS_SCREEN (screen), NULL);
  g_return_val_if_fail (anid != 0, NULL);
  g_return_val_if_fail (width > 0, NULL);
  g_return_val_if_fail (height > 0, NULL);
  g_return_val_if_fail (depth > 0, NULL);

  pixmap = g_object_new (gdk_pixmap_get_type (), NULL);
  draw_impl = GDK_DRAWABLE_IMPL_X11 (GDK_PIXMAP_OBJECT (pixmap)->impl);
  pix_impl = GDK_PIXMAP_IMPL_X11 (GDK_PIXMAP_OBJECT (pixmap)->impl);
  draw_impl->wrapper = GDK_DRAWABLE (pixmap);

  xpixmap = anid;
  
  draw_impl->screen = screen;
  draw_impl->xid = xpixmap;

  pix_impl->is_foreign = TRUE;
  pix_impl->width = width;
  pix_impl->height = height;
  GDK_PIXMAP_OBJECT (pixmap)->depth = depth;
  
  _gdk_xid_table_insert (gdk_screen_get_display (screen), 
			 &GDK_PIXMAP_XID (pixmap), pixmap);

  return pixmap;
}

/**
 * gdk_pixmap_foreign_new:
 * @anid: a native pixmap handle.
 * 
 * Wraps a native window for the default display in a #GdkPixmap.
 * This may fail if the pixmap has been destroyed.
 *
 * For example in the X backend, a native pixmap handle is an Xlib
 * <type>XID</type>.
 *
 * Return value: the newly-created #GdkPixmap wrapper for the 
 *    native pixmap or %NULL if the pixmap has been destroyed.
 **/
GdkPixmap*
gdk_pixmap_foreign_new (GdkNativeWindow anid)
{
   return gdk_pixmap_foreign_new_for_display (gdk_display_get_default (), anid);
}

/**
 * gdk_pixmap_lookup:
 * @anid: a native pixmap handle.
 * 
 * Looks up the #GdkPixmap that wraps the given native pixmap handle.
 *
 * For example in the X backend, a native pixmap handle is an Xlib
 * <type>XID</type>.
 *
 * Return value: the #GdkPixmap wrapper for the native pixmap,
 *    or %NULL if there is none.
 **/
GdkPixmap*
gdk_pixmap_lookup (GdkNativeWindow anid)
{
  return (GdkPixmap*) gdk_xid_table_lookup_for_display (gdk_display_get_default (), anid);
}

/**
 * gdk_pixmap_lookup_for_display:
 * @display: the #GdkDisplay associated with @anid
 * @anid: a native pixmap handle.
 * 
 * Looks up the #GdkPixmap that wraps the given native pixmap handle.
 *
 * For example in the X backend, a native pixmap handle is an Xlib
 * <type>XID</type>.
 *
 * Return value: the #GdkPixmap wrapper for the native pixmap,
 *    or %NULL if there is none.
 *
 * Since: 2.2
 **/
GdkPixmap*
gdk_pixmap_lookup_for_display (GdkDisplay *display, GdkNativeWindow anid)
{
  g_return_val_if_fail (GDK_IS_DISPLAY (display), NULL);
  return (GdkPixmap*) gdk_xid_table_lookup_for_display (display, anid);
}

#define __GDK_PIXMAP_X11_C__
#include  "gdkaliasdef.c"
