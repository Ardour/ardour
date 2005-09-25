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
#include "gtk-custom-ruler.h"

enum
{
	ARG_0,
	ARG_LOWER,
	ARG_UPPER,
	ARG_POSITION,
	ARG_MAX_SIZE,
	ARG_SHOW_POSITION
};

static void gtk_custom_ruler_class_init (GtkCustomRulerClass * klass);
static void gtk_custom_ruler_init (GtkCustomRuler * ruler);
static void gtk_custom_ruler_realize (GtkWidget * widget);
static void gtk_custom_ruler_unrealize (GtkWidget * widget);
static void gtk_custom_ruler_size_allocate (GtkWidget * widget, GtkAllocation * allocation);
static gint gtk_custom_ruler_expose (GtkWidget * widget, GdkEventExpose * event);
static void gtk_custom_ruler_make_pixmap (GtkCustomRuler * ruler);
static void gtk_custom_ruler_set_arg (GtkObject * object, GtkArg * arg, guint arg_id);
static void gtk_custom_ruler_get_arg (GtkObject * object, GtkArg * arg, guint arg_id);

static gint
default_metric_get_marks (GtkCustomRulerMark **marks, gulong lower, gulong upper, gint maxchars)
{
	return 0;
}

static const GtkCustomMetric default_metric = {
	1.0,
	default_metric_get_marks
};

static GtkWidgetClass *parent_class;

GtkType gtk_custom_ruler_get_type (void)
{
	static GtkType ruler_type = 0;

	if (!ruler_type) {
		static const GtkTypeInfo ruler_info = {
			"GtkCustomRuler",
			sizeof (GtkCustomRuler),
			sizeof (GtkCustomRulerClass),
			(GtkClassInitFunc) gtk_custom_ruler_class_init,
			(GtkObjectInitFunc) gtk_custom_ruler_init,
			/* reserved_1 */ NULL,
			/* reserved_2 */ NULL,
			(GtkClassInitFunc) NULL,
		};
		ruler_type = gtk_type_unique (GTK_TYPE_WIDGET, &ruler_info);
	}
	return ruler_type;
}

static void
gtk_custom_ruler_class_init (GtkCustomRulerClass * class)
{
	GtkObjectClass *object_class;
	GtkWidgetClass *widget_class;

	object_class = (GtkObjectClass *) class;
	widget_class = (GtkWidgetClass *) class;

	parent_class = gtk_type_class (GTK_TYPE_WIDGET);

	object_class->set_arg = gtk_custom_ruler_set_arg;
	object_class->get_arg = gtk_custom_ruler_get_arg;

	widget_class->realize = gtk_custom_ruler_realize;
	widget_class->unrealize = gtk_custom_ruler_unrealize;
	widget_class->size_allocate = gtk_custom_ruler_size_allocate;
	widget_class->expose_event = gtk_custom_ruler_expose;

	class->draw_ticks = NULL;
	class->draw_pos = NULL;

	gtk_object_add_arg_type ("GtkCustomRuler::lower", GTK_TYPE_ULONG, GTK_ARG_READWRITE, ARG_LOWER);
	gtk_object_add_arg_type ("GtkCustomRuler::upper", GTK_TYPE_ULONG, GTK_ARG_READWRITE, ARG_UPPER);
	gtk_object_add_arg_type ("GtkCustomRuler::position", GTK_TYPE_ULONG, GTK_ARG_READWRITE, ARG_POSITION);
	gtk_object_add_arg_type ("GtkCustomRuler::max_size", GTK_TYPE_ULONG, GTK_ARG_READWRITE, ARG_MAX_SIZE);
	gtk_object_add_arg_type ("GtkCustomRuler::show_position", GTK_TYPE_BOOL, GTK_ARG_READWRITE, ARG_SHOW_POSITION);
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
gtk_custom_ruler_set_arg (GtkObject * object, GtkArg * arg, guint arg_id)
{
	GtkCustomRuler *ruler = GTK_CUSTOM_RULER (object);

	switch (arg_id) {
	case ARG_LOWER:
		gtk_custom_ruler_set_range (ruler, GTK_VALUE_ULONG (*arg), ruler->upper, ruler->position, ruler->max_size);
		break;
	case ARG_UPPER:
		gtk_custom_ruler_set_range (ruler, ruler->lower, GTK_VALUE_ULONG (*arg), ruler->position, ruler->max_size);
		break;
	case ARG_POSITION:
		gtk_custom_ruler_set_range (ruler, ruler->lower, ruler->upper, GTK_VALUE_ULONG (*arg), ruler->max_size);
		break;
	case ARG_MAX_SIZE:
		gtk_custom_ruler_set_range (ruler, ruler->lower, ruler->upper, ruler->position, GTK_VALUE_ULONG (*arg));
		break;
	case ARG_SHOW_POSITION:
		// gtk_customer_ruler_set_show_position (ruler, GTK_VALUE_BOOL (*arg));
		break;
	}
}

static void
gtk_custom_ruler_get_arg (GtkObject * object, GtkArg * arg, guint arg_id)
{
	GtkCustomRuler *ruler = GTK_CUSTOM_RULER (object);

	switch (arg_id) {
	case ARG_LOWER:
		GTK_VALUE_ULONG (*arg) = ruler->lower;
		break;
	case ARG_UPPER:
		GTK_VALUE_ULONG (*arg) = ruler->upper;
		break;
	case ARG_POSITION:
		GTK_VALUE_ULONG (*arg) = ruler->position;
		break;
	case ARG_MAX_SIZE:
		GTK_VALUE_ULONG (*arg) = ruler->max_size;
		break;
	case ARG_SHOW_POSITION:
		GTK_VALUE_BOOL (*arg) = ruler->show_position;
		break;
	default:
		arg->type = GTK_TYPE_INVALID;
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
gtk_custom_ruler_set_range (GtkCustomRuler * ruler, gulong lower, gulong upper, gulong position, gulong max_size)
{
	g_return_if_fail (ruler != NULL);
	g_return_if_fail (GTK_IS_CUSTOM_RULER (ruler));

	ruler->lower = lower;
	ruler->upper = upper;
	ruler->position = position;
	ruler->max_size = max_size;

	if (GTK_WIDGET_DRAWABLE (ruler))
		gtk_widget_queue_draw (GTK_WIDGET (ruler));
}

void
gtk_custom_ruler_draw_ticks (GtkCustomRuler * ruler)
{
    GtkCustomRulerClass *klass;
	g_return_if_fail (ruler != NULL);
	g_return_if_fail (GTK_IS_CUSTOM_RULER (ruler));

	klass = GTK_CUSTOM_RULER_CLASS (GTK_OBJECT (ruler));
	if (klass->draw_ticks)
		klass->draw_ticks (ruler);
}

void
gtk_custom_ruler_draw_pos (GtkCustomRuler * ruler)
{
    GtkCustomRulerClass *klass;
    g_return_if_fail (ruler != NULL);
    g_return_if_fail (GTK_IS_CUSTOM_RULER (ruler));
    
    klass = GTK_CUSTOM_RULER_CLASS (GTK_OBJECT (ruler));
    if (klass->draw_pos && ruler->show_position)
	    klass->draw_pos (ruler);
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
	attributes.event_mask |= (GDK_EXPOSURE_MASK | Gdk::POINTER_MOTION_MASK | Gdk::POINTER_MOTION_HINT_MASK);

	attributes_mask = GDK_WA_X | GDK_WA_Y | GDK_WA_VISUAL | GDK_WA_COLORMAP;

	widget->window = gdk_window_new (gtk_widget_get_parent_window (widget), &attributes, attributes_mask);
	gdk_window_set_user_data (widget->window, ruler);

	widget->style = gtk_style_attach (widget->style, widget->window);
	gtk_style_set_background (widget->style, widget->window, GTK_STATE_ACTIVE);

	gtk_custom_ruler_make_pixmap (ruler);
}

static void
gtk_custom_ruler_unrealize (GtkWidget * widget)
{
	GtkCustomRuler *ruler;

	g_return_if_fail (widget != NULL);
	g_return_if_fail (GTK_IS_CUSTOM_RULER (widget));

	ruler = GTK_CUSTOM_RULER (widget);

	if (ruler->backing_store)
		gdk_pixmap_unref (ruler->backing_store);
	if (ruler->non_gr_exp_gc)
		gdk_gc_destroy (ruler->non_gr_exp_gc);

	ruler->backing_store = NULL;
	ruler->non_gr_exp_gc = NULL;

	if (GTK_WIDGET_CLASS (parent_class)->unrealize)
		(*GTK_WIDGET_CLASS (parent_class)->unrealize) (widget);
}

static void
gtk_custom_ruler_size_allocate (GtkWidget * widget, GtkAllocation * allocation)
{
	GtkCustomRuler *ruler;

	g_return_if_fail (widget != NULL);
	g_return_if_fail (GTK_IS_CUSTOM_RULER (widget));

	ruler = GTK_CUSTOM_RULER (widget);
	widget->allocation = *allocation;

	if (GTK_WIDGET_REALIZED (widget)) {
		gdk_window_move_resize (widget->window, allocation->x, allocation->y, allocation->width, allocation->height);

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

		gdk_draw_pixmap (widget->window,
				 ruler->non_gr_exp_gc,
				 ruler->backing_store, 0, 0, 0, 0, widget->allocation.width, widget->allocation.height);
		
		gtk_custom_ruler_draw_pos (ruler);
	}

	return FALSE;
}

static void
gtk_custom_ruler_make_pixmap (GtkCustomRuler * ruler)
{
	GtkWidget *widget;
	gint width;
	gint height;

	widget = GTK_WIDGET (ruler);

	if (ruler->backing_store) {
		gdk_window_get_size (ruler->backing_store, &width, &height);
		if ((width == widget->allocation.width) && (height == widget->allocation.height))
			return;

		gdk_pixmap_unref (ruler->backing_store);
	}

	ruler->backing_store = gdk_pixmap_new (widget->window, widget->allocation.width, widget->allocation.height, -1);

	ruler->xsrc = 0;
	ruler->ysrc = 0;

	if (!ruler->non_gr_exp_gc) {
		ruler->non_gr_exp_gc = gdk_gc_new (widget->window);
		gdk_gc_set_exposures (ruler->non_gr_exp_gc, FALSE);
	}
}

void
gtk_custom_ruler_set_show_position (GtkCustomRuler * ruler, gboolean yn)
{
	ruler->show_position = yn;
}
