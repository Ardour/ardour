/* GTK - The GIMP Toolkit
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

/* modified by andreas meyer <hexx3000@gmx.de> */

#ifndef __GTK_CUSTOM_RULER_H__
#define __GTK_CUSTOM_RULER_H__

#include <gdk/gdk.h>
#include <gtk/gtkwidget.h>


G_BEGIN_DECLS

#define GTK_TYPE_CUSTOM_RULER            (gtk_custom_ruler_get_type ())
#define GTK_CUSTOM_RULER(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), GTK_TYPE_CUSTOM_RULER, GtkCustomRuler))
#define GTK_CUSTOM_RULER_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), GTK_TYPE_CUSTOM_RULER, GtkCustomRulerClass))
#define GTK_IS_CUSTOM_RULER(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GTK_TYPE_CUSTOM_RULER))
#define GTK_IS_CUSTOM_RULER_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), GTK_TYPE_CUSTOM_RULER))
#define GTK_CUSTOM_RULER_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), GTK_TYPE_CUSTOM_RULER, GtkCustomRulerClass))

typedef struct _GtkCustomRuler          GtkCustomRuler;
typedef struct _GtkCustomRulerClass     GtkCustomRulerClass;
typedef struct _GtkCustomMetric         GtkCustomMetric;
typedef struct _GtkCustomRulerMark      GtkCustomRulerMark;

struct _GtkCustomRuler {
  GtkWidget widget;

  GdkPixmap *backing_store;
  GdkGC *non_gr_exp_gc;
  GtkCustomMetric *metric;
  gint xsrc, ysrc;
  gint slider_size;
  gboolean show_position;
    
  /* The upper limit of the ruler (in points) */
  gdouble lower;
  /* The lower limit of the ruler */
  gdouble upper;
  /* The position of the mark on the ruler */
  gdouble position;
  /* The maximum size of the ruler */
  gdouble max_size;
};

struct _GtkCustomRulerClass {
  GtkWidgetClass parent_class;

  void (* draw_ticks) (GtkCustomRuler *ruler);
  void (* draw_pos)   (GtkCustomRuler *ruler);
};

typedef enum {
   GtkCustomRulerMarkMajor,
   GtkCustomRulerMarkMinor,
   GtkCustomRulerMarkMicro
} GtkCustomRulerMarkStyle;

struct _GtkCustomRulerMark {
  gchar                  *label;
  gdouble                 position;
  GtkCustomRulerMarkStyle style;
};

struct _GtkCustomMetric {
  gfloat units_per_pixel;
  gint (* get_marks) (GtkCustomRulerMark **marks, gdouble lower, gdouble upper, gint maxchars);
};

GType   gtk_custom_ruler_get_type            (void);
void    gtk_custom_ruler_set_metric          (GtkCustomRuler *ruler, GtkCustomMetric *metric);
void    gtk_custom_ruler_set_range           (GtkCustomRuler *ruler,
					      gdouble lower,
					      gdouble upper,
					      gdouble position,
					      gdouble  max_size);
void    gtk_custom_ruler_draw_ticks          (GtkCustomRuler *ruler);
void    gtk_custom_ruler_draw_pos            (GtkCustomRuler *ruler);
void    gtk_custom_ruler_set_show_position   (GtkCustomRuler *rule, gboolean yn);

G_END_DECLS

#endif /* __GTK_CUSTOM_RULER_H__ */
