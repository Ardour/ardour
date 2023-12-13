/* GTK - The GIMP Toolkit
 * Copyright (C) 1995-1997 Peter Mattis, Spencer Kimball and Josh MacDonald
 * Copyright (C) 2001 Red Hat, Inc.
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

#include "gtkscrollbar.h"
#include "gtkintl.h"
#include "gtkprivate.h"
#include "gtkalias.h"

static void gtk_scrollbar_style_set (GtkWidget *widget,
                                     GtkStyle  *previous);

G_DEFINE_ABSTRACT_TYPE (GtkScrollbar, gtk_scrollbar, GTK_TYPE_RANGE)

static void
gtk_scrollbar_class_init (GtkScrollbarClass *class)
{
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (class);

  widget_class->style_set = gtk_scrollbar_style_set;

  GTK_RANGE_CLASS (class)->stepper_detail = "Xscrollbar";

  gtk_widget_class_install_style_property (widget_class,
					   g_param_spec_int ("min-slider-length",
							     P_("Minimum Slider Length"),
							     P_("Minimum length of scrollbar slider"),
							     0,
							     G_MAXINT,
							     21,
							     GTK_PARAM_READABLE));

  gtk_widget_class_install_style_property (widget_class,
					   g_param_spec_boolean ("fixed-slider-length",
                                                                 P_("Fixed slider size"),
                                                                 P_("Don't change slider size, just lock it to the minimum length"),
                                                                 FALSE,
                                                                 GTK_PARAM_READABLE));

  gtk_widget_class_install_style_property (widget_class,
					   g_param_spec_boolean ("has-backward-stepper",
                                                                 P_("Backward stepper"),
                                                                 P_("Display the standard backward arrow button"),
                                                                 TRUE,
                                                                 GTK_PARAM_READABLE));

  gtk_widget_class_install_style_property (widget_class,
                                           g_param_spec_boolean ("has-forward-stepper",
                                                                 P_("Forward stepper"),
                                                                 P_("Display the standard forward arrow button"),
                                                                 TRUE,
                                                                 GTK_PARAM_READABLE));

  gtk_widget_class_install_style_property (widget_class,
					   g_param_spec_boolean ("has-secondary-backward-stepper",
                                                                 P_("Secondary backward stepper"),
                                                                 P_("Display a second backward arrow button on the opposite end of the scrollbar"),
                                                                 FALSE,
                                                                 GTK_PARAM_READABLE));

  gtk_widget_class_install_style_property (widget_class,
                                           g_param_spec_boolean ("has-secondary-forward-stepper",
                                                                 P_("Secondary forward stepper"),
                                                                 P_("Display a second forward arrow button on the opposite end of the scrollbar"),
                                                                 FALSE,
                                                                 GTK_PARAM_READABLE));
}

static void
gtk_scrollbar_init (GtkScrollbar *scrollbar)
{
}

static void
gtk_scrollbar_style_set (GtkWidget *widget,
                         GtkStyle  *previous)
{
  GtkRange *range = GTK_RANGE (widget);
  gint slider_length;
  gboolean fixed_size;
  gboolean has_a, has_b, has_c, has_d;

  gtk_widget_style_get (widget,
                        "min-slider-length", &slider_length,
                        "fixed-slider-length", &fixed_size,
                        "has-backward-stepper", &has_a,
                        "has-secondary-forward-stepper", &has_b,
                        "has-secondary-backward-stepper", &has_c,
                        "has-forward-stepper", &has_d,
                        NULL);

  range->min_slider_size = slider_length;
  range->slider_size_fixed = fixed_size;

  range->has_stepper_a = has_a;
  range->has_stepper_b = has_b;
  range->has_stepper_c = has_c;
  range->has_stepper_d = has_d;

  GTK_WIDGET_CLASS (gtk_scrollbar_parent_class)->style_set (widget, previous);
}

#if 0
/**
 * gtk_scrollbar_new:
 * @orientation: the scrollbar's orientation.
 * @adjustment: (allow-none): the #GtkAdjustment to use, or %NULL to create a new adjustment.
 *
 * Creates a new scrollbar with the given orientation.
 *
 * Return value:  the new #GtkScrollbar.
 *
 * Since: 2.16
 **/
GtkWidget *
gtk_scrollbar_new (GtkOrientation  orientation,
                   GtkAdjustment  *adjustment)
{
  g_return_val_if_fail (adjustment == NULL || GTK_IS_ADJUSTMENT (adjustment),
                        NULL);

  return g_object_new (GTK_TYPE_SCROLLBAR,
                       "orientation", orientation,
                       "adjustment",  adjustment,
                       NULL);
}
#endif


#define __GTK_SCROLLBAR_C__
#include "gtkaliasdef.c"
