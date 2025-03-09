/* GTK - The GIMP Toolkit
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

#ifndef __GTK_VIEWPORT_H__
#define __GTK_VIEWPORT_H__


#if defined(GTK_DISABLE_SINGLE_INCLUDES) && !defined (__GTK_H_INSIDE__) && !defined (GTK_COMPILATION)
#error "Only <ytk/ytk.h> can be included directly."
#endif

#include <ytk/gtkadjustment.h>
#include <ytk/gtkbin.h>


G_BEGIN_DECLS


#define GTK_TYPE_VIEWPORT            (gtk_viewport_get_type ())
#define GTK_VIEWPORT(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), GTK_TYPE_VIEWPORT, GtkViewport))
#define GTK_VIEWPORT_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), GTK_TYPE_VIEWPORT, GtkViewportClass))
#define GTK_IS_VIEWPORT(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GTK_TYPE_VIEWPORT))
#define GTK_IS_VIEWPORT_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), GTK_TYPE_VIEWPORT))
#define GTK_VIEWPORT_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), GTK_TYPE_VIEWPORT, GtkViewportClass))


typedef struct _GtkViewport       GtkViewport;
typedef struct _GtkViewportClass  GtkViewportClass;

struct _GtkViewport
{
  GtkBin bin;

  GtkShadowType GSEAL (shadow_type);
  GdkWindow *GSEAL (view_window);
  GdkWindow *GSEAL (bin_window);
  GtkAdjustment *GSEAL (hadjustment);
  GtkAdjustment *GSEAL (vadjustment);
};

struct _GtkViewportClass
{
  GtkBinClass parent_class;

  void	(*set_scroll_adjustments)	(GtkViewport	*viewport,
					 GtkAdjustment	*hadjustment,
					 GtkAdjustment	*vadjustment);
};


GType          gtk_viewport_get_type        (void) G_GNUC_CONST;
GtkWidget*     gtk_viewport_new             (GtkAdjustment *hadjustment,
					     GtkAdjustment *vadjustment);
GtkAdjustment* gtk_viewport_get_hadjustment (GtkViewport   *viewport);
GtkAdjustment* gtk_viewport_get_vadjustment (GtkViewport   *viewport);
void           gtk_viewport_set_hadjustment (GtkViewport   *viewport,
					     GtkAdjustment *adjustment);
void           gtk_viewport_set_vadjustment (GtkViewport   *viewport,
					     GtkAdjustment *adjustment);
void           gtk_viewport_set_shadow_type (GtkViewport   *viewport,
					     GtkShadowType  type);
GtkShadowType  gtk_viewport_get_shadow_type (GtkViewport   *viewport);
GdkWindow*     gtk_viewport_get_bin_window  (GtkViewport   *viewport);
GdkWindow*     gtk_viewport_get_view_window (GtkViewport   *viewport);


G_END_DECLS


#endif /* __GTK_VIEWPORT_H__ */
