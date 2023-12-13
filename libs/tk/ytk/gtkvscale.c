/* GTK - The GIMP Toolkit
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

#include <math.h>
#include <stdlib.h>

#include "gtkvscale.h"
#include "gtkorientable.h"
#include "gtkalias.h"

/**
 * SECTION:gtkvscale
 * @Short_description: A vertical slider widget for selecting a value from a range
 * @Title: GtkVScale
 *
 * The #GtkVScale widget is used to allow the user to select a value using
 * a vertical slider. To create one, use gtk_hscale_new_with_range().
 *
 * The position to show the current value, and the number of decimal places
 * shown can be set using the parent #GtkScale class's functions.
 */

G_DEFINE_TYPE (GtkVScale, gtk_vscale, GTK_TYPE_SCALE)

static void
gtk_vscale_class_init (GtkVScaleClass *class)
{
  GtkRangeClass *range_class = GTK_RANGE_CLASS (class);

  range_class->slider_detail = "vscale";
}

static void
gtk_vscale_init (GtkVScale *vscale)
{
  gtk_orientable_set_orientation (GTK_ORIENTABLE (vscale),
                                  GTK_ORIENTATION_VERTICAL);
}
/**
 * gtk_vscale_new:
 * @adjustment: the #GtkAdjustment which sets the range of the scale.
 *
 * Creates a new #GtkVScale.
 *
 * Returns: a new #GtkVScale.
 */
GtkWidget *
gtk_vscale_new (GtkAdjustment *adjustment)
{
  g_return_val_if_fail (adjustment == NULL || GTK_IS_ADJUSTMENT (adjustment),
                        NULL);

  return g_object_new (GTK_TYPE_VSCALE,
                       "adjustment", adjustment,
                       NULL);
}

/**
 * gtk_vscale_new_with_range:
 * @min: minimum value
 * @max: maximum value
 * @step: step increment (tick size) used with keyboard shortcuts
 *
 * Creates a new vertical scale widget that lets the user input a
 * number between @min and @max (including @min and @max) with the
 * increment @step.  @step must be nonzero; it's the distance the
 * slider moves when using the arrow keys to adjust the scale value.
 *
 * Note that the way in which the precision is derived works best if @step
 * is a power of ten. If the resulting precision is not suitable for your
 * needs, use gtk_scale_set_digits() to correct it.
 *
 * Return value: a new #GtkVScale
 **/
GtkWidget *
gtk_vscale_new_with_range (gdouble min,
                           gdouble max,
                           gdouble step)
{
  GtkObject *adj;
  GtkScale *scale;
  gint digits;

  g_return_val_if_fail (min < max, NULL);
  g_return_val_if_fail (step != 0.0, NULL);

  adj = gtk_adjustment_new (min, min, max, step, 10 * step, 0);

  if (fabs (step) >= 1.0 || step == 0.0)
    {
      digits = 0;
    }
  else
    {
      digits = abs ((gint) floor (log10 (fabs (step))));
      if (digits > 5)
        digits = 5;
    }

  scale = g_object_new (GTK_TYPE_VSCALE,
                        "adjustment", adj,
                        "digits", digits,
                        NULL);

  return GTK_WIDGET (scale);
}

#define __GTK_VSCALE_C__
#include "gtkaliasdef.c"
