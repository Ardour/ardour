/* gdkdrawable-quartz.h
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

#ifndef __GDK_PIXMAP_QUARTZ_H__
#define __GDK_PIXMAP_QUARTZ_H__

#include <ApplicationServices/ApplicationServices.h>
#include <gdk/quartz/gdkdrawable-quartz.h>
#include <gdk/gdkpixmap.h>

G_BEGIN_DECLS

/* Pixmap implementation for Quartz
 */

typedef struct _GdkPixmapImplQuartz GdkPixmapImplQuartz;
typedef struct _GdkPixmapImplQuartzClass GdkPixmapImplQuartzClass;

#define GDK_TYPE_PIXMAP_IMPL_QUARTZ              (_gdk_pixmap_impl_quartz_get_type ())
#define GDK_PIXMAP_IMPL_QUARTZ(object)           (G_TYPE_CHECK_INSTANCE_CAST ((object), GDK_TYPE_PIXMAP_IMPL_QUARTZ, GdkPixmapImplQuartz))
#define GDK_PIXMAP_IMPL_QUARTZ_CLASS(klass)      (G_TYPE_CHECK_CLASS_CAST ((klass), GDK_TYPE_PIXMAP_IMPL_QUARTZ, GdkPixmapImplQuartzClass))
#define GDK_IS_PIXMAP_IMPL_QUARTZ(object)        (G_TYPE_CHECK_INSTANCE_TYPE ((object), GDK_TYPE_PIXMAP_IMPL_QUARTZ))
#define GDK_IS_PIXMAP_IMPL_QUARTZ_CLASS(klass)   (G_TYPE_CHECK_CLASS_TYPE ((klass), GDK_TYPE_PIXMAP_IMPL_QUARTZ))
#define GDK_PIXMAP_IMPL_QUARTZ_GET_CLASS(obj)    (G_TYPE_INSTANCE_GET_CLASS ((obj), GDK_TYPE_PIXMAP_IMPL_QUARTZ, GdkPixmapImplQuartzClass))

struct _GdkPixmapImplQuartz
{
  GdkDrawableImplQuartz parent_instance;

  gint width;
  gint height;

  void *data;
  CGDataProviderRef data_provider;
};
 
struct _GdkPixmapImplQuartzClass 
{
  GdkDrawableImplQuartzClass parent_class;
};

GType _gdk_pixmap_impl_quartz_get_type (void);

G_END_DECLS

#endif /* __GDK_PIXMAP_QUARTZ_H__ */
