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
/* subsequently specialized for audio time displays by paul davis <paul@linuxaudiosystems.com> */

#include <math.h>
#include <stdio.h>
#include <string.h>
#include "gtk-custom-hruler.h"

#define RULER_HEIGHT          14
#define MINIMUM_INCR          5
#define MAXIMUM_SUBDIVIDE     5

#define ROUND(x) ((int) ((x) + 0.5))

static void gtk_custom_hruler_class_init (GtkCustomHRulerClass * klass);
static void gtk_custom_hruler_init (GtkCustomHRuler * custom_hruler);
static gint gtk_custom_hruler_motion_notify (GtkWidget * widget, GdkEventMotion * event);
static void gtk_custom_hruler_draw_ticks (GtkCustomRuler * ruler);
static void gtk_custom_hruler_draw_pos (GtkCustomRuler * ruler);

GType gtk_custom_hruler_get_type (void)
{
	static GType hruler_type = 0;

	if (!hruler_type) {
		static const GTypeInfo hruler_info =
			{
				sizeof (GtkCustomHRulerClass),
				NULL,		/* base_init */
				NULL,		/* base_finalize */
				(GClassInitFunc) gtk_custom_hruler_class_init,
				NULL,		/* class_finalize */
				NULL,		/* class_data */
				sizeof (GtkCustomHRuler),
				0,		/* n_preallocs */
				(GInstanceInitFunc) gtk_custom_hruler_init,
				NULL /* value_table */
			};

		hruler_type = g_type_register_static (gtk_custom_ruler_get_type(), "GtkCustomHRuler",
						      &hruler_info, (GTypeFlags)0);
	}

	return hruler_type;
}

static void
gtk_custom_hruler_class_init (GtkCustomHRulerClass * klass)
{
	GtkWidgetClass *widget_class;
	GtkCustomRulerClass *ruler_class;

	widget_class = (GtkWidgetClass *) klass;
	ruler_class = (GtkCustomRulerClass *) klass;

	widget_class->motion_notify_event = gtk_custom_hruler_motion_notify;

	ruler_class->draw_ticks = gtk_custom_hruler_draw_ticks;
	ruler_class->draw_pos = gtk_custom_hruler_draw_pos;
}

static void
gtk_custom_hruler_init (GtkCustomHRuler * custom_hruler)
{
	GtkWidget *widget;

	widget = GTK_WIDGET (custom_hruler);
	widget->requisition.width = widget->style->xthickness * 2 + 1;
	widget->requisition.height = widget->style->ythickness * 2 + RULER_HEIGHT;
}


GtkWidget *
gtk_custom_hruler_new (void)
{
	return GTK_WIDGET (gtk_type_new (gtk_custom_hruler_get_type ()));
}

static gint
gtk_custom_hruler_motion_notify (GtkWidget * widget, GdkEventMotion * event)
{
	GtkCustomRuler *ruler;
	gint x;

	g_return_val_if_fail (widget != NULL, FALSE);
	g_return_val_if_fail (GTK_IS_CUSTOM_HRULER (widget), FALSE);
	g_return_val_if_fail (event != NULL, FALSE);

	ruler = GTK_CUSTOM_RULER (widget);

	if (event->is_hint)
		gdk_window_get_pointer (widget->window, &x, NULL, NULL);
	else
		x = event->x;

	ruler->position = ruler->lower + ((ruler->upper - ruler->lower) * x) / widget->allocation.width;

	/*  Make sure the ruler has been allocated already  */
	if (ruler->backing_store != NULL)
		gtk_custom_ruler_draw_pos (ruler);

	return FALSE;
}

static void
gtk_custom_hruler_draw_ticks (GtkCustomRuler * ruler)
{
	GtkWidget *widget;
	GdkGC *gc;
	gint i;
	GtkCustomRulerMark *marks;
	gint ythickness;
	gint nmarks;
	gint max_chars;
	gint digit_offset;
	PangoLayout *layout;
	PangoRectangle logical_rect, ink_rect;

	g_return_if_fail (ruler != NULL);
	g_return_if_fail (GTK_IS_CUSTOM_HRULER (ruler));

	if (!GTK_WIDGET_DRAWABLE (ruler))
		return;

	widget = GTK_WIDGET (ruler);

	gc = widget->style->fg_gc[GTK_STATE_NORMAL];

	layout = gtk_widget_create_pango_layout (widget, "012456789");
	pango_layout_get_extents (layout, &ink_rect, &logical_rect);

	digit_offset = ink_rect.y;

	ythickness = widget->style->ythickness;

	gtk_paint_box (widget->style, ruler->backing_store,
		       GTK_STATE_NORMAL, GTK_SHADOW_NONE,
		       NULL, widget, "custom_hruler", 0, 0, widget->allocation.width, widget->allocation.height);

	gdk_draw_line (ruler->backing_store, gc, 0, widget->allocation.height - 1,
		       widget->allocation.width, widget->allocation.height - 1);

	if ((ruler->upper - ruler->lower) == 0) {
		return;
	}

	/* we have to assume a fixed width font here */

	max_chars = widget->allocation.width / 12; // XXX FIX ME: pixel with of the char `8'

	nmarks = ruler->metric->get_marks (&marks, ruler->lower, ruler->upper, max_chars);

	for (i = 0; i < nmarks; i++) {
		gint pos;
		gint height;

		pos = ROUND ((marks[i].position - ruler->lower) / ruler->metric->units_per_pixel);
		height = widget->allocation.height;

		switch (marks[i].style) {
		case GtkCustomRulerMarkMajor:
			gdk_draw_line (ruler->backing_store, gc, pos, height, pos, 0);
			break;
		case GtkCustomRulerMarkMinor:
			gdk_draw_line (ruler->backing_store, gc, pos, height, pos, height - (height/2));
			break;
		case GtkCustomRulerMarkMicro:
			gdk_draw_line (ruler->backing_store, gc, pos, height, pos, height - 3);
			break;
		}

		pango_layout_set_text (layout, marks[i].label, -1);
		pango_layout_get_extents (layout, &logical_rect, NULL);

		gtk_paint_layout (widget->style,
				  ruler->backing_store,
				  GTK_WIDGET_STATE (widget),
				  FALSE,
				  NULL,
				  widget,
				  "hruler",
				  pos + 2, ythickness + PANGO_PIXELS (logical_rect.y - digit_offset),
				  layout);

		g_free (marks[i].label);
	}

	if (nmarks) {
		g_free (marks);
	}

	g_object_unref (layout);
}

static void
gtk_custom_hruler_draw_pos (GtkCustomRuler * ruler)
{
	GtkWidget *widget;
	GdkGC *gc;
	int i;
	gint x, y;
	gint width, height;
	gint bs_width, bs_height;
	gint xthickness;
	gint ythickness;
	gfloat increment;

	g_return_if_fail (ruler != NULL);
	g_return_if_fail (GTK_IS_CUSTOM_HRULER (ruler));
	if (GTK_WIDGET_DRAWABLE (ruler) && (ruler->upper - ruler->lower) > 0) {
		widget = GTK_WIDGET (ruler);
		gc = widget->style->fg_gc[GTK_STATE_NORMAL];
		xthickness = widget->style->xthickness;
		ythickness = widget->style->ythickness;
		width = widget->allocation.width;
		height = widget->allocation.height - ythickness * 2;

		bs_width = height / 2;
		bs_width |= 1;			/* make sure it's odd */
		bs_height = bs_width / 2 + 1;

		if ((bs_width > 0) && (bs_height > 0)) {
			/*  If a backing store exists, restore the ruler  */
			if (ruler->backing_store && ruler->non_gr_exp_gc)
				gdk_draw_drawable (ruler->widget.window,
                                                   ruler->non_gr_exp_gc,
                                                   GDK_DRAWABLE(ruler->backing_store), ruler->xsrc, ruler->ysrc, ruler->xsrc, ruler->ysrc, bs_width, bs_height);

			increment = (gfloat) width / (ruler->upper - ruler->lower);
			x = ROUND ((ruler->position - ruler->lower) * increment) + (xthickness - bs_width) / 2 - 1;
			y = (height + bs_height) / 2 + ythickness;

			for (i = 0; i < bs_height; i++)
				gdk_draw_line (widget->window, gc, x + i, y + i, x + bs_width - 1 - i, y + i);


			ruler->xsrc = x;
			ruler->ysrc = y;
		}
	}
}
