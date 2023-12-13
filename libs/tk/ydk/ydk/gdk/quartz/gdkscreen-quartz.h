/* gdkscreen-quartz.h
 *
 * Copyright (C) 2009  Kristian Rietveld  <kris@gtk.org>
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

#ifndef __GDK_SCREEN_QUARTZ_H__
#define __GDK_SCREEN_QUARTZ_H__

G_BEGIN_DECLS

typedef struct _GdkScreenQuartz GdkScreenQuartz;
typedef struct _GdkScreenQuartzClass GdkScreenQuartzClass;

#define GDK_TYPE_SCREEN_QUARTZ              (_gdk_screen_quartz_get_type ())
#define GDK_SCREEN_QUARTZ(object)           (G_TYPE_CHECK_INSTANCE_CAST ((object), GDK_TYPE_SCREEN_QUARTZ, GdkScreenQuartz))
#define GDK_SCREEN_QUARTZ_CLASS(klass)      (G_TYPE_CHECK_CLASS_CAST ((klass), GDK_TYPE_SCREEN_QUARTZ, GdkScreenQuartzClass))
#define GDK_IS_SCREEN_QUARTZ(object)        (G_TYPE_CHECK_INSTANCE_TYPE ((object), GDK_TYPE_SCREEN_QUARTZ))
#define GDK_IS_SCREEN_QUARTZ_CLASS(klass)   (G_TYPE_CHECK_CLASS_TYPE ((klass), GDK_TYPE_SCREEN_QUARTZ))
#define GDK_SCREEN_QUARTZ_GET_CLASS(obj)    (G_TYPE_INSTANCE_GET_CLASS ((obj), GDK_TYPE_SCREEN_QUARTZ, GdkScreenQuartzClass))

struct _GdkScreenQuartz
{
  GdkScreen parent_instance;

  GdkDisplay *display;
  GdkColormap *default_colormap;

  /* Origin of "root window" in Cocoa coordinates */
  gint min_x;
  gint min_y;

  gint width;
  gint height;

  int n_screens;
  GdkRectangle *screen_rects;

  guint screen_changed_id;

  guint emit_monitors_changed : 1;
};

struct _GdkScreenQuartzClass
{
  GdkScreenClass parent_class;
};

GType      _gdk_screen_quartz_get_type (void);
GdkScreen *_gdk_screen_quartz_new      (void);

G_END_DECLS

#endif /* _GDK_SCREEN_QUARTZ_H_ */
