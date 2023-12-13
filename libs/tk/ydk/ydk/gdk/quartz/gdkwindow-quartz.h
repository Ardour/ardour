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

#ifndef __GDK_WINDOW_QUARTZ_H__
#define __GDK_WINDOW_QUARTZ_H__

#include <gdk/quartz/gdkdrawable-quartz.h>
#import <gdk/quartz/GdkQuartzView.h>
#import <gdk/quartz/GdkQuartzWindow.h>

G_BEGIN_DECLS

/* Window implementation for Quartz
 */

typedef struct _GdkWindowImplQuartz GdkWindowImplQuartz;
typedef struct _GdkWindowImplQuartzClass GdkWindowImplQuartzClass;

#define GDK_TYPE_WINDOW_IMPL_QUARTZ              (_gdk_window_impl_quartz_get_type ())
#define GDK_WINDOW_IMPL_QUARTZ(object)           (G_TYPE_CHECK_INSTANCE_CAST ((object), GDK_TYPE_WINDOW_IMPL_QUARTZ, GdkWindowImplQuartz))
#define GDK_WINDOW_IMPL_QUARTZ_CLASS(klass)      (G_TYPE_CHECK_CLASS_CAST ((klass), GDK_TYPE_WINDOW_IMPL_QUARTZ, GdkWindowImplQuartzClass))
#define GDK_IS_WINDOW_IMPL_QUARTZ(object)        (G_TYPE_CHECK_INSTANCE_TYPE ((object), GDK_TYPE_WINDOW_IMPL_QUARTZ))
#define GDK_IS_WINDOW_IMPL_QUARTZ_CLASS(klass)   (G_TYPE_CHECK_CLASS_TYPE ((klass), GDK_TYPE_WINDOW_IMPL_QUARTZ))
#define GDK_WINDOW_IMPL_QUARTZ_GET_CLASS(obj)    (G_TYPE_INSTANCE_GET_CLASS ((obj), GDK_TYPE_WINDOW_IMPL_QUARTZ, GdkWindowImplQuartzClass))

struct _GdkWindowImplQuartz
{
  GdkDrawableImplQuartz parent_instance;

  NSWindow *toplevel;
  NSTrackingRectTag tracking_rect;
  GdkQuartzView *view;

  GdkWindowTypeHint type_hint;

  GdkRegion *paint_clip_region;
  gint begin_paint_count;
  gint in_paint_rect_count;

  GdkWindow *transient_for;

  /* Sorted by z-order */
  GList *sorted_children;

  GdkRegion *needs_display_region;

  GdkColor background_color;

  guint background_color_set : 1;
};
 
struct _GdkWindowImplQuartzClass 
{
  GdkDrawableImplQuartzClass parent_class;
};

GType _gdk_window_impl_quartz_get_type (void);

G_END_DECLS

#endif /* __GDK_WINDOW_QUARTZ_H__ */
