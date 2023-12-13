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

#ifndef __GDK_DRAWABLE_X11_H__
#define __GDK_DRAWABLE_X11_H__

#include <gdk/gdkdrawable.h>

#include <X11/Xlib.h>
#include <X11/extensions/Xrender.h>

G_BEGIN_DECLS

/* Drawable implementation for X11
 */

typedef enum
{
  GDK_X11_FORMAT_NONE,
  GDK_X11_FORMAT_EXACT_MASK,
  GDK_X11_FORMAT_ARGB_MASK,
  GDK_X11_FORMAT_ARGB
} GdkX11FormatType;

typedef struct _GdkDrawableImplX11 GdkDrawableImplX11;
typedef struct _GdkDrawableImplX11Class GdkDrawableImplX11Class;

#define GDK_TYPE_DRAWABLE_IMPL_X11              (_gdk_drawable_impl_x11_get_type ())
#define GDK_DRAWABLE_IMPL_X11(object)           (G_TYPE_CHECK_INSTANCE_CAST ((object), GDK_TYPE_DRAWABLE_IMPL_X11, GdkDrawableImplX11))
#define GDK_DRAWABLE_IMPL_X11_CLASS(klass)      (G_TYPE_CHECK_CLASS_CAST ((klass), GDK_TYPE_DRAWABLE_IMPL_X11, GdkDrawableImplX11Class))
#define GDK_IS_DRAWABLE_IMPL_X11(object)        (G_TYPE_CHECK_INSTANCE_TYPE ((object), GDK_TYPE_DRAWABLE_IMPL_X11))
#define GDK_IS_DRAWABLE_IMPL_X11_CLASS(klass)   (G_TYPE_CHECK_CLASS_TYPE ((klass), GDK_TYPE_DRAWABLE_IMPL_X11))
#define GDK_DRAWABLE_IMPL_X11_GET_CLASS(obj)    (G_TYPE_INSTANCE_GET_CLASS ((obj), GDK_TYPE_DRAWABLE_IMPL_X11, GdkDrawableImplX11Class))

struct _GdkDrawableImplX11
{
  GdkDrawable parent_instance;

  GdkDrawable *wrapper;
  
  GdkColormap *colormap;
  
  Window xid;
  GdkScreen *screen;

  Picture picture;
  cairo_surface_t *cairo_surface;
};
 
struct _GdkDrawableImplX11Class 
{
  GdkDrawableClass parent_class;

};

GType _gdk_drawable_impl_x11_get_type (void);

void  _gdk_x11_convert_to_format      (guchar           *src_buf,
                                       gint              src_rowstride,
                                       guchar           *dest_buf,
                                       gint              dest_rowstride,
                                       GdkX11FormatType  dest_format,
                                       GdkByteOrder      dest_byteorder,
                                       gint              width,
                                       gint              height);

/* Note that the following take GdkDrawableImplX11, not the wrapper drawable */
void _gdk_x11_drawable_finish           (GdkDrawable  *drawable);
void _gdk_x11_drawable_update_size      (GdkDrawable  *drawable);
GdkDrawable *gdk_x11_window_get_drawable_impl (GdkWindow *window);
GdkDrawable *gdk_x11_pixmap_get_drawable_impl (GdkPixmap *pixmap);

G_END_DECLS

#endif /* __GDK_DRAWABLE_X11_H__ */
