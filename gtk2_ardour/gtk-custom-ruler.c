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

#include <stdio.h>
#include "gettext.h"
#define _(Text)  dgettext (PACKAGE,Text)

#include "gtk-custom-ruler.h"

enum
{
	PROP_0,
	PROP_LOWER,
	PROP_UPPER,
	PROP_POSITION,
	PROP_MAX_SIZE,
	PROP_SHOW_POSITION
};

static void gtk_custom_ruler_class_init (GtkCustomRulerClass * klass);
static void gtk_custom_ruler_init (GtkCustomRuler * ruler);
static void gtk_custom_ruler_realize (GtkWidget * widget);
static void gtk_custom_ruler_unrealize (GtkWidget * widget);
static void gtk_custom_ruler_size_allocate (GtkWidget * widget, GtkAllocation * allocation);
static gint gtk_custom_ruler_expose (GtkWidget * widget, GdkEventExpose * event);
static void gtk_custom_ruler_make_pixmap (GtkCustomRuler * ruler);
static void gtk_custom_ruler_set_property  (GObject        *object,
					    guint            prop_id,
					    const GValue   *value,
					    GParamSpec     *pspec);
static void gtk_custom_ruler_get_property  (GObject        *object,
					    guint           prop_id,
					    GValue         *value,
					    GParamSpec     *pspec);


static gint
default_metric_get_marks (GtkCustomRulerMark **marks, gdouble lower, gdouble upper, gint maxchars)
{
	(void) marks;
	(void) lower;
	(void) upper;
	(void) maxchars;

	return 0;
}

static const GtkCustomMetric default_metric = {
	1.0,
	default_metric_get_marks
};

static GtkWidgetClass *parent_class;

GType gtk_custom_ruler_get_type (void)
{
	static GType ruler_type = 0;

	if (!ruler_type)
	{
		static const GTypeInfo ruler_info =
			{
				sizeof (GtkCustomRulerClass),
				(GBaseInitFunc) NULL,		/* base_init */
				(GBaseFinalizeFunc) NULL,		/* base_finalize */
				(GClassInitFunc) gtk_custom_ruler_class_init,
				(GClassFinalizeFunc) NULL,		/* class_finalize */
				NULL,		/* class_data */
				sizeof (GtkCustomRuler),
				0,		/* n_preallocs */
				(GInstanceInitFunc) gtk_custom_ruler_init,
				NULL			/* value_table */
			};

		ruler_type = g_type_register_static (GTK_TYPE_WIDGET, "GtkCustomRuler",
					   &ruler_info, (GTypeFlags)0);
	}

	return ruler_type;
}

static void
gtk_custom_ruler_class_init (GtkCustomRulerClass * class)
{
	GObjectClass   *gobject_class;
	GtkWidgetClass *widget_class;

	gobject_class = (GObjectClass *) class;
	widget_class = (GtkWidgetClass*) class;

	parent_class = g_type_class_peek_parent (class);

	gobject_class->set_property = gtk_custom_ruler_set_property;
	gobject_class->get_property = gtk_custom_ruler_get_property;

	widget_class->realize = gtk_custom_ruler_realize;
	widget_class->unrealize = gtk_custom_ruler_unrealize;
	widget_class->size_allocate = gtk_custom_ruler_size_allocate;
	widget_class->expose_event = gtk_custom_ruler_expose;

	class->draw_ticks = NULL;
	class->draw_pos = NULL;

	g_object_class_install_property (gobject_class,
					 PROP_LOWER,
					 g_param_spec_double ("lower",
							      _("Lower"),
							      _("Lower limit of ruler"),
							      -G_MAXDOUBLE,
							      G_MAXDOUBLE,
							      0.0,
							      G_PARAM_READWRITE));

	g_object_class_install_property (gobject_class,
					 PROP_UPPER,
					 g_param_spec_double ("upper",
							      _("Upper"),
							      _("Upper limit of ruler"),
							      -G_MAXDOUBLE,
							      G_MAXDOUBLE,
							      0.0,
							      G_PARAM_READWRITE));

	g_object_class_install_property (gobject_class,
					 PROP_POSITION,
					 g_param_spec_double ("position",
							      _("Position"),
							      _("Position of mark on the ruler"),
							      -G_MAXDOUBLE,
							      G_MAXDOUBLE,
							      0.0,
							      G_PARAM_READWRITE));

	g_object_class_install_property (gobject_class,
					 PROP_MAX_SIZE,
					 g_param_spec_double ("max_size",
							      _("Max Size"),
							      _("Maximum size of the ruler"),
							      -G_MAXDOUBLE,
							      G_MAXDOUBLE,
							      0.0,
							      G_PARAM_READWRITE));

	g_object_class_install_property (gobject_class,
					 PROP_SHOW_POSITION,
					 g_param_spec_boolean ("show_position",
							       _("Show Position"),
							       _("Draw current ruler position"),
							       TRUE,
							       G_PARAM_READWRITE));
}

static void
gtk_custom_ruler_init (GtkCustomRuler * ruler)
{
	ruler->backing_store = NULL;
	ruler->non_gr_exp_gc = NULL;
	ruler->xsrc = 0;
	ruler->ysrc = 0;
	ruler->slider_size = 0;
	ruler->lower = 0;
	ruler->upper = 0;
	ruler->position = 0;
	ruler->max_size = 0;
	ruler->show_position = FALSE;

	gtk_custom_ruler_set_metric (ruler, NULL);
}

static void
gtk_custom_ruler_set_property (GObject      *object,
			       guint         prop_id,
			       const GValue *value,
			       GParamSpec   *pspec)
{
  GtkCustomRuler *ruler = GTK_CUSTOM_RULER (object);
  (void) pspec;

  switch (prop_id)
    {
    case PROP_LOWER:
      gtk_custom_ruler_set_range (ruler, g_value_get_double (value), ruler->upper,
			   ruler->position, ruler->max_size);
      break;
    case PROP_UPPER:
      gtk_custom_ruler_set_range (ruler, ruler->lower, g_value_get_double (value),
			   ruler->position, ruler->max_size);
      break;
    case PROP_POSITION:
      gtk_custom_ruler_set_range (ruler, ruler->lower, ruler->upper,
			   g_value_get_double (value), ruler->max_size);
      break;
    case PROP_MAX_SIZE:
      gtk_custom_ruler_set_range (ruler, ruler->lower, ruler->upper,
			   ruler->position,  g_value_get_double (value));
      break;
    case PROP_SHOW_POSITION:
      gtk_custom_ruler_set_show_position (ruler, g_value_get_boolean (value));
      break;
    }
}

static void
gtk_custom_ruler_get_property (GObject      *object,
			       guint         prop_id,
			       GValue       *value,
			       GParamSpec   *pspec)
{
  GtkCustomRuler *ruler = GTK_CUSTOM_RULER (object);

  switch (prop_id)
    {
    case PROP_LOWER:
      g_value_set_double (value, ruler->lower);
      break;
    case PROP_UPPER:
      g_value_set_double (value, ruler->upper);
      break;
    case PROP_POSITION:
      g_value_set_double (value, ruler->position);
      break;
    case PROP_MAX_SIZE:
      g_value_set_double (value, ruler->max_size);
      break;
    case PROP_SHOW_POSITION:
      g_value_set_boolean (value, ruler->show_position);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

void
gtk_custom_ruler_set_metric (GtkCustomRuler * ruler, GtkCustomMetric * metric)
{
	g_return_if_fail (ruler != NULL);
	g_return_if_fail (GTK_IS_CUSTOM_RULER (ruler));

	if (metric == NULL)
		ruler->metric = (GtkCustomMetric *) & default_metric;
	else
		ruler->metric = metric;

	if (GTK_WIDGET_DRAWABLE (ruler))
		gtk_widget_queue_draw (GTK_WIDGET (ruler));
}

void
gtk_custom_ruler_set_range (GtkCustomRuler *ruler,
			    gdouble   lower,
			    gdouble   upper,
			    gdouble   position,
			    gdouble   max_size)
{
  g_return_if_fail (GTK_IS_CUSTOM_RULER (ruler));

  g_object_freeze_notify (G_OBJECT (ruler));
  if (ruler->lower != lower)
    {
      ruler->lower = lower;
      g_object_notify (G_OBJECT (ruler), "lower");
    }
  if (ruler->upper != upper)
    {
      ruler->upper = upper;
      g_object_notify (G_OBJECT (ruler), "upper");
    }
  if (ruler->position != position)
    {
      ruler->position = position;
      g_object_notify (G_OBJECT (ruler), "position");
    }
  if (ruler->max_size != max_size)
    {
      ruler->max_size = max_size;
      g_object_notify (G_OBJECT (ruler), "max-size");
    }
  g_object_thaw_notify (G_OBJECT (ruler));

  if (GTK_WIDGET_DRAWABLE (ruler))
    gtk_widget_queue_draw (GTK_WIDGET (ruler));
}

/**
 * gtk_custom_ruler_get_range:
 * @ruler: a #GtkCustomRuler
 * @lower: location to store lower limit of the ruler, or %NULL
 * @upper: location to store upper limit of the ruler, or %NULL
 * @position: location to store the current position of the mark on the ruler, or %NULL
 * @max_size: location to store the maximum size of the ruler used when calculating
 *            the space to leave for the text, or %NULL.
 *
 * Retrieves values indicating the range and current position of a #GtkCustomRuler.
 * See gtk_custom_ruler_set_range().
 **/
void
gtk_custom_ruler_get_range (GtkCustomRuler *ruler,
		     gdouble  *lower,
		     gdouble  *upper,
		     gdouble  *position,
		     gdouble  *max_size)
{
  g_return_if_fail (GTK_IS_CUSTOM_RULER (ruler));

  if (lower)
    *lower = ruler->lower;
  if (upper)
    *upper = ruler->upper;
  if (position)
    *position = ruler->position;
  if (max_size)
    *max_size = ruler->max_size;
}

void
gtk_custom_ruler_draw_ticks (GtkCustomRuler * ruler)
{
        g_return_if_fail (GTK_IS_CUSTOM_RULER (ruler));

        if (GTK_CUSTOM_RULER_GET_CLASS (ruler)->draw_ticks)
                GTK_CUSTOM_RULER_GET_CLASS (ruler)->draw_ticks (ruler);

}

void
gtk_custom_ruler_draw_pos (GtkCustomRuler * ruler)
{
        g_return_if_fail (GTK_IS_CUSTOM_RULER (ruler));

        if (GTK_CUSTOM_RULER_GET_CLASS (ruler)->draw_pos && ruler->show_position)
                GTK_CUSTOM_RULER_GET_CLASS (ruler)->draw_pos (ruler);
}

static void
gtk_custom_ruler_realize (GtkWidget * widget)
{
	GtkCustomRuler *ruler;
	GdkWindowAttr attributes;
	gint attributes_mask;

	g_return_if_fail (widget != NULL);
	g_return_if_fail (GTK_IS_CUSTOM_RULER (widget));

	ruler = GTK_CUSTOM_RULER (widget);
	GTK_WIDGET_SET_FLAGS (ruler, GTK_REALIZED);

	attributes.window_type = GDK_WINDOW_CHILD;
	attributes.x = widget->allocation.x;
	attributes.y = widget->allocation.y;
	attributes.width = widget->allocation.width;
	attributes.height = widget->allocation.height;
	attributes.wclass = GDK_INPUT_OUTPUT;
	attributes.visual = gtk_widget_get_visual (widget);
	attributes.colormap = gtk_widget_get_colormap (widget);
	attributes.event_mask = gtk_widget_get_events (widget);
	attributes.event_mask |= (GDK_EXPOSURE_MASK | GDK_POINTER_MOTION_MASK | GDK_POINTER_MOTION_HINT_MASK);

	attributes_mask = GDK_WA_X | GDK_WA_Y | GDK_WA_VISUAL | GDK_WA_COLORMAP;

	widget->window = gdk_window_new (gtk_widget_get_parent_window (widget), &attributes, attributes_mask);
	gdk_window_set_user_data (widget->window, ruler);

	widget->style = gtk_style_attach (widget->style, widget->window);
	gtk_style_set_background (widget->style, widget->window, GTK_STATE_ACTIVE);

	gtk_custom_ruler_make_pixmap (ruler);
}

static void
gtk_custom_ruler_unrealize (GtkWidget *widget)
{
  GtkCustomRuler *ruler = GTK_CUSTOM_RULER (widget);

  if (ruler->backing_store)
    g_object_unref (ruler->backing_store);
  if (ruler->non_gr_exp_gc)
    g_object_unref (ruler->non_gr_exp_gc);

  ruler->backing_store = NULL;
  ruler->non_gr_exp_gc = NULL;

  if (GTK_WIDGET_CLASS (parent_class)->unrealize)
    (* GTK_WIDGET_CLASS (parent_class)->unrealize) (widget);
}

static void
gtk_custom_ruler_size_allocate (GtkWidget     *widget,
			 GtkAllocation *allocation)
{
  GtkCustomRuler *ruler = GTK_CUSTOM_RULER (widget);

  widget->allocation = *allocation;

  if (GTK_WIDGET_REALIZED (widget))
    {
      gdk_window_move_resize (widget->window,
			      allocation->x, allocation->y,
			      allocation->width, allocation->height);

      gtk_custom_ruler_make_pixmap (ruler);
    }
}

static gint
gtk_custom_ruler_expose (GtkWidget * widget, GdkEventExpose * event)
{
	GtkCustomRuler *ruler;

	g_return_val_if_fail (widget != NULL, FALSE);
	g_return_val_if_fail (GTK_IS_CUSTOM_RULER (widget), FALSE);
	g_return_val_if_fail (event != NULL, FALSE);

	if (GTK_WIDGET_DRAWABLE (widget)) {
		ruler = GTK_CUSTOM_RULER (widget);

		gtk_custom_ruler_draw_ticks (ruler);

		gdk_draw_drawable (widget->window,
                                   ruler->non_gr_exp_gc,
                                   GDK_DRAWABLE(ruler->backing_store), 0, 0, 0, 0, widget->allocation.width, widget->allocation.height);

		gtk_custom_ruler_draw_pos (ruler);
	}

	return FALSE;
}


static void
gtk_custom_ruler_make_pixmap (GtkCustomRuler *ruler)
{
  GtkWidget *widget;
  gint width;
  gint height;

  widget = GTK_WIDGET (ruler);

  if (ruler->backing_store)
    {
      gdk_drawable_get_size (ruler->backing_store, &width, &height);
      if ((width == widget->allocation.width) &&
	  (height == widget->allocation.height))
	return;

      g_object_unref (ruler->backing_store);
    }

  ruler->backing_store = gdk_pixmap_new (widget->window,
					 widget->allocation.width,
					 widget->allocation.height,
					 -1);

  ruler->xsrc = 0;
  ruler->ysrc = 0;

  if (!ruler->non_gr_exp_gc)
    {
      ruler->non_gr_exp_gc = gdk_gc_new (widget->window);
      gdk_gc_set_exposures (ruler->non_gr_exp_gc, FALSE);
    }
}

void
gtk_custom_ruler_set_show_position (GtkCustomRuler * ruler, gboolean yn)
{
	ruler->show_position = yn;
}
