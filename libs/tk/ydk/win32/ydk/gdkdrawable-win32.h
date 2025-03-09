/* GDK - The GIMP Drawing Kit
 * Copyright (C) 1995-1997 Peter Mattis, Spencer Kimball and Josh MacDonald
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

/*
 * Modified by the GTK+ Team and others 1997-1999.  See the AUTHORS
 * file for a list of people on the GTK+ Team.  See the ChangeLog
 * files for a list of changes.  These files are distributed with
 * GTK+ at ftp://ftp.gtk.org/pub/gtk/. 
 */

#ifndef __GDK_DRAWABLE_WIN32_H__
#define __GDK_DRAWABLE_WIN32_H__

#include <ydk/gdkdrawable.h>
#include <ydk/gdkwin32.h>

G_BEGIN_DECLS

/* Drawable implementation for Win32
 */

typedef struct _GdkDrawableImplWin32 GdkDrawableImplWin32;
typedef struct _GdkDrawableImplWin32Class GdkDrawableImplWin32Class;

#define GDK_TYPE_DRAWABLE_IMPL_WIN32              (_gdk_drawable_impl_win32_get_type ())
#define GDK_DRAWABLE_IMPL_WIN32(object)           (G_TYPE_CHECK_INSTANCE_CAST ((object), GDK_TYPE_DRAWABLE_IMPL_WIN32, GdkDrawableImplWin32))
#define GDK_DRAWABLE_IMPL_WIN32_CLASS(klass)      (G_TYPE_CHECK_CLASS_CAST ((klass), GDK_TYPE_DRAWABLE_IMPL_WIN32, GdkDrawableImplWin32Class))
#define GDK_IS_DRAWABLE_IMPL_WIN32(object)        (G_TYPE_CHECK_INSTANCE_TYPE ((object), GDK_TYPE_DRAWABLE_IMPL_WIN32))
#define GDK_IS_DRAWABLE_IMPL_WIN32_CLASS(klass)   (G_TYPE_CHECK_CLASS_TYPE ((klass), GDK_TYPE_DRAWABLE_IMPL_WIN32))
#define GDK_DRAWABLE_IMPL_WIN32_GET_CLASS(obj)    (G_TYPE_INSTANCE_GET_CLASS ((obj), GDK_TYPE_DRAWABLE_IMPL_WIN32, GdkDrawableImplWin32Class))

struct _GdkDrawableImplWin32
{
  GdkDrawable parent_instance;
  GdkDrawable *wrapper;
  GdkColormap *colormap;
  HANDLE handle;

  guint hdc_count;
  HDC hdc;
  HBITMAP saved_dc_bitmap;	/* Original bitmap for dc */
  cairo_surface_t *cairo_surface;
};
 
struct _GdkDrawableImplWin32Class 
{
  GdkDrawableClass parent_class;
};

GType _gdk_drawable_impl_win32_get_type (void);

HDC  _gdk_win32_drawable_acquire_dc (GdkDrawable *drawable);
void _gdk_win32_drawable_release_dc (GdkDrawable *drawable);
void _gdk_win32_drawable_finish     (GdkDrawable *drawable);

G_END_DECLS

#endif /* __GDK_DRAWABLE_WIN32_H__ */
