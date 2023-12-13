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

#ifndef __GDK_PIXMAP_WIN32_H__
#define __GDK_PIXMAP_WIN32_H__

#include <gdk/gdkdrawable-win32.h>
#include <gdk/gdkpixmap.h>

G_BEGIN_DECLS

/* Pixmap implementation for Win32
 */

typedef struct _GdkPixmapImplWin32 GdkPixmapImplWin32;
typedef struct _GdkPixmapImplWin32Class GdkPixmapImplWin32Class;

#define GDK_TYPE_PIXMAP_IMPL_WIN32              (_gdk_pixmap_impl_win32_get_type ())
#define GDK_PIXMAP_IMPL_WIN32(object)           (G_TYPE_CHECK_INSTANCE_CAST ((object), GDK_TYPE_PIXMAP_IMPL_WIN32, GdkPixmapImplWin32))
#define GDK_PIXMAP_IMPL_WIN32_CLASS(klass)      (G_TYPE_CHECK_CLASS_CAST ((klass), GDK_TYPE_PIXMAP_IMPL_WIN32, GdkPixmapImplWin32Class))
#define GDK_IS_PIXMAP_IMPL_WIN32(object)        (G_TYPE_CHECK_INSTANCE_TYPE ((object), GDK_TYPE_PIXMAP_IMPL_WIN32))
#define GDK_IS_PIXMAP_IMPL_WIN32_CLASS(klass)   (G_TYPE_CHECK_CLASS_TYPE ((klass), GDK_TYPE_PIXMAP_IMPL_WIN32))
#define GDK_PIXMAP_IMPL_WIN32_GET_CLASS(obj)    (G_TYPE_INSTANCE_GET_CLASS ((obj), GDK_TYPE_PIXMAP_IMPL_WIN32, GdkPixmapImplWin32Class))

struct _GdkPixmapImplWin32
{
  GdkDrawableImplWin32 parent_instance;

  gint width;
  gint height;
  guchar *bits;
  guint is_foreign : 1;
  guint is_allocated : 1;
};
 
struct _GdkPixmapImplWin32Class 
{
  GdkDrawableImplWin32Class parent_class;
};

GType _gdk_pixmap_impl_win32_get_type (void);

G_END_DECLS

#endif /* __GDK_PIXMAP_WIN32_H__ */
