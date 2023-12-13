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

#ifndef __GDK_DRAWABLE_QUARTZ_H__
#define __GDK_DRAWABLE_QUARTZ_H__

#include <gdk/gdkdrawable.h>

G_BEGIN_DECLS

/* Drawable implementation for Quartz
 */

typedef struct _GdkDrawableImplQuartz GdkDrawableImplQuartz;
typedef struct _GdkDrawableImplQuartzClass GdkDrawableImplQuartzClass;

#define GDK_TYPE_DRAWABLE_IMPL_QUARTZ              (gdk_drawable_impl_quartz_get_type ())
#define GDK_DRAWABLE_IMPL_QUARTZ(object)           (G_TYPE_CHECK_INSTANCE_CAST ((object), GDK_TYPE_DRAWABLE_IMPL_QUARTZ, GdkDrawableImplQuartz))
#define GDK_DRAWABLE_IMPL_QUARTZ_CLASS(klass)      (G_TYPE_CHECK_CLASS_CAST ((klass), GDK_TYPE_DRAWABLE_IMPL_QUARTZ, GdkDrawableImplQuartzClass))
#define GDK_IS_DRAWABLE_IMPL_QUARTZ(object)        (G_TYPE_CHECK_INSTANCE_TYPE ((object), GDK_TYPE_DRAWABLE_IMPL_QUARTZ))
#define GDK_IS_DRAWABLE_IMPL_QUARTZ_CLASS(klass)   (G_TYPE_CHECK_CLASS_TYPE ((klass), GDK_TYPE_DRAWABLE_IMPL_QUARTZ))
#define GDK_DRAWABLE_IMPL_QUARTZ_GET_CLASS(obj)    (G_TYPE_INSTANCE_GET_CLASS ((obj), GDK_TYPE_DRAWABLE_IMPL_QUARTZ, GdkDrawableImplQuartzClass))

struct _GdkDrawableImplQuartz
{
  GdkDrawable      parent_instance;

  GdkDrawable     *wrapper;

  GdkColormap     *colormap;

  cairo_surface_t *cairo_surface;
};
 
struct _GdkDrawableImplQuartzClass
{
  GdkDrawableClass parent_class;

  /* vtable */
  CGContextRef (*get_context) (GdkDrawable* drawable,
			       gboolean     antialias);
};

GType        gdk_drawable_impl_quartz_get_type   (void);
CGContextRef gdk_quartz_drawable_get_context     (GdkDrawable  *drawable, 
						  gboolean      antialias);
void         gdk_quartz_drawable_release_context (GdkDrawable  *drawable, 
						  CGContextRef  context);

G_END_DECLS

#endif /* __GDK_DRAWABLE_QUARTZ_H__ */
