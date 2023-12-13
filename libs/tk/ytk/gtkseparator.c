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

#include "config.h"

#include "gtkorientable.h"
#include "gtkseparator.h"
#include "gtkprivate.h"
#include "gtkintl.h"
#include "gtkalias.h"


enum {
  PROP_0,
  PROP_ORIENTATION
};


typedef struct _GtkSeparatorPrivate GtkSeparatorPrivate;

struct _GtkSeparatorPrivate
{
  GtkOrientation orientation;
};

#define GTK_SEPARATOR_GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE ((obj), GTK_TYPE_SEPARATOR, GtkSeparatorPrivate))


static void       gtk_separator_set_property (GObject        *object,
                                              guint           prop_id,
                                              const GValue   *value,
                                              GParamSpec     *pspec);
static void       gtk_separator_get_property (GObject        *object,
                                              guint           prop_id,
                                              GValue         *value,
                                              GParamSpec     *pspec);

static void       gtk_separator_size_request (GtkWidget      *widget,
                                              GtkRequisition *requisition);
static gboolean   gtk_separator_expose       (GtkWidget      *widget,
                                              GdkEventExpose *event);


G_DEFINE_ABSTRACT_TYPE_WITH_CODE (GtkSeparator, gtk_separator, GTK_TYPE_WIDGET,
                                  G_IMPLEMENT_INTERFACE (GTK_TYPE_ORIENTABLE,
                                                         NULL))


static void
gtk_separator_class_init (GtkSeparatorClass *class)
{
  GObjectClass *object_class = G_OBJECT_CLASS (class);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (class);

  object_class->set_property = gtk_separator_set_property;
  object_class->get_property = gtk_separator_get_property;

  widget_class->size_request = gtk_separator_size_request;
  widget_class->expose_event = gtk_separator_expose;

  g_object_class_override_property (object_class,
                                    PROP_ORIENTATION,
                                    "orientation");

  g_type_class_add_private (object_class, sizeof (GtkSeparatorPrivate));
}

static void
gtk_separator_init (GtkSeparator *separator)
{
  GtkWidget *widget = GTK_WIDGET (separator);
  GtkSeparatorPrivate *private = GTK_SEPARATOR_GET_PRIVATE (separator);

  gtk_widget_set_has_window (GTK_WIDGET (separator), FALSE);

  private->orientation = GTK_ORIENTATION_HORIZONTAL;

  widget->requisition.width  = 1;
  widget->requisition.height = widget->style->ythickness;
}

static void
gtk_separator_set_property (GObject      *object,
                            guint         prop_id,
                            const GValue *value,
                            GParamSpec   *pspec)
{
  GtkSeparatorPrivate *private = GTK_SEPARATOR_GET_PRIVATE (object);

  switch (prop_id)
    {
    case PROP_ORIENTATION:
      private->orientation = g_value_get_enum (value);
      gtk_widget_queue_resize (GTK_WIDGET (object));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
gtk_separator_get_property (GObject    *object,
                            guint       prop_id,
                            GValue     *value,
                            GParamSpec *pspec)
{
  GtkSeparatorPrivate *private = GTK_SEPARATOR_GET_PRIVATE (object);

  switch (prop_id)
    {
    case PROP_ORIENTATION:
      g_value_set_enum (value, private->orientation);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
gtk_separator_size_request (GtkWidget      *widget,
                            GtkRequisition *requisition)
{
  GtkSeparatorPrivate *private = GTK_SEPARATOR_GET_PRIVATE (widget);
  gboolean wide_separators;
  gint     separator_width;
  gint     separator_height;

  gtk_widget_style_get (widget,
                        "wide-separators",  &wide_separators,
                        "separator-width",  &separator_width,
                        "separator-height", &separator_height,
                        NULL);

  requisition->width  = 1;
  requisition->height = 1;

  if (private->orientation == GTK_ORIENTATION_HORIZONTAL)
    {
      if (wide_separators)
        requisition->height = separator_height;
      else
        requisition->height = widget->style->ythickness;
    }
  else
    {
      if (wide_separators)
        requisition->width = separator_width;
      else
        requisition->width = widget->style->xthickness;
    }
}

static gboolean
gtk_separator_expose (GtkWidget      *widget,
                      GdkEventExpose *event)
{
  GtkSeparatorPrivate *private = GTK_SEPARATOR_GET_PRIVATE (widget);
  gboolean wide_separators;
  gint     separator_width;
  gint     separator_height;

  if (!gtk_widget_is_drawable (widget))
    return FALSE;

  gtk_widget_style_get (widget,
                        "wide-separators",  &wide_separators,
                        "separator-width",  &separator_width,
                        "separator-height", &separator_height,
                        NULL);

  if (private->orientation == GTK_ORIENTATION_HORIZONTAL)
    {
      if (wide_separators)
        gtk_paint_box (widget->style, widget->window,
                       gtk_widget_get_state (widget), GTK_SHADOW_ETCHED_OUT,
                       &event->area, widget, "hseparator",
                       widget->allocation.x,
                       widget->allocation.y + (widget->allocation.height -
                                               separator_height) / 2,
                       widget->allocation.width,
                       separator_height);
      else
        gtk_paint_hline (widget->style, widget->window,
                         gtk_widget_get_state (widget),
                         &event->area, widget, "hseparator",
                         widget->allocation.x,
                         widget->allocation.x + widget->allocation.width - 1,
                         widget->allocation.y + (widget->allocation.height -
                                                 widget->style->ythickness) / 2);
    }
  else
    {
      if (wide_separators)
        gtk_paint_box (widget->style, widget->window,
                       gtk_widget_get_state (widget), GTK_SHADOW_ETCHED_OUT,
                       &event->area, widget, "vseparator",
                       widget->allocation.x + (widget->allocation.width -
                                               separator_width) / 2,
                       widget->allocation.y,
                       separator_width,
                       widget->allocation.height);
      else
        gtk_paint_vline (widget->style, widget->window,
                         gtk_widget_get_state (widget),
                         &event->area, widget, "vseparator",
                         widget->allocation.y,
                         widget->allocation.y + widget->allocation.height - 1,
                         widget->allocation.x + (widget->allocation.width -
                                                 widget->style->xthickness) / 2);
    }

  return FALSE;
}

#if 0
/**
 * gtk_separator_new:
 * @orientation: the separator's orientation.
 *
 * Creates a new #GtkSeparator with the given orientation.
 *
 * Return value: a new #GtkSeparator.
 *
 * Since: 2.16
 **/
GtkWidget *
gtk_separator_new (GtkOrientation orientation)
{
  return g_object_new (GTK_TYPE_SEPARATOR,
                       "orientation", orientation,
                       NULL);
}
#endif


#define __GTK_SEPARATOR_C__
#include "gtkaliasdef.c"
