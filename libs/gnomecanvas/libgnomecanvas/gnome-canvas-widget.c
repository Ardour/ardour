/*
 * Copyright (C) 1997, 1998, 1999, 2000 Free Software Foundation
 * All rights reserved.
 *
 * This file is part of the Gnome Library.
 *
 * The Gnome Library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * The Gnome Library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with the Gnome Library; see the file COPYING.LIB.  If not,
 * write to the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */
/*
  @NOTATION@
 */
/* Widget item type for GnomeCanvas widget
 *
 * GnomeCanvas is basically a port of the Tk toolkit's most excellent canvas widget.  Tk is
 * copyrighted by the Regents of the University of California, Sun Microsystems, and other parties.
 *
 *
 * Author: Federico Mena <federico@nuclecu.unam.mx>
 */

#include <config.h>
#include <math.h>
#include <gtk/gtksignal.h>
#include "gnome-canvas-widget.h"

enum {
	PROP_0,
	PROP_WIDGET,
	PROP_X,
	PROP_Y,
	PROP_WIDTH,
	PROP_HEIGHT,
	PROP_ANCHOR,
	PROP_SIZE_PIXELS
};


static void gnome_canvas_widget_class_init (GnomeCanvasWidgetClass *class);
static void gnome_canvas_widget_init       (GnomeCanvasWidget      *witem);
static void gnome_canvas_widget_destroy    (GtkObject              *object);
static void gnome_canvas_widget_get_property (GObject            *object,
					      guint               param_id,
					      GValue             *value,
					      GParamSpec         *pspec);
static void gnome_canvas_widget_set_property (GObject            *object,
					      guint               param_id,
					      const GValue       *value,
					      GParamSpec         *pspec);

static void   gnome_canvas_widget_update      (GnomeCanvasItem *item, double *affine, ArtSVP *clip_path, int flags);
static double gnome_canvas_widget_point       (GnomeCanvasItem *item, double x, double y,
					       int cx, int cy, GnomeCanvasItem **actual_item);
static void   gnome_canvas_widget_bounds      (GnomeCanvasItem *item, double *x1, double *y1, double *x2, double *y2);

static void gnome_canvas_widget_render (GnomeCanvasItem *item,
					GnomeCanvasBuf *buf);
static void gnome_canvas_widget_draw (GnomeCanvasItem *item,
				      GdkDrawable *drawable,
				      int x, int y,
				      int width, int height);

static GnomeCanvasItemClass *parent_class;


GType
gnome_canvas_widget_get_type (void)
{
	static GType widget_type;

	if (!widget_type) {
		const GTypeInfo object_info = {
			sizeof (GnomeCanvasWidgetClass),
			(GBaseInitFunc) NULL,
			(GBaseFinalizeFunc) NULL,
			(GClassInitFunc) gnome_canvas_widget_class_init,
			(GClassFinalizeFunc) NULL,
			NULL,			/* class_data */
			sizeof (GnomeCanvasWidget),
			0,			/* n_preallocs */
			(GInstanceInitFunc) gnome_canvas_widget_init,
			NULL			/* value_table */
		};

		widget_type = g_type_register_static (GNOME_TYPE_CANVAS_ITEM, "GnomeCanvasWidget",
						      &object_info, 0);
	}

	return widget_type;
}

static void
gnome_canvas_widget_class_init (GnomeCanvasWidgetClass *class)
{
	GObjectClass *gobject_class;
	GtkObjectClass *object_class;
	GnomeCanvasItemClass *item_class;

	gobject_class = (GObjectClass *) class;
	object_class = (GtkObjectClass *) class;
	item_class = (GnomeCanvasItemClass *) class;

	parent_class = g_type_class_peek_parent (class);

	gobject_class->set_property = gnome_canvas_widget_set_property;
	gobject_class->get_property = gnome_canvas_widget_get_property;

        g_object_class_install_property
                (gobject_class,
                 PROP_WIDGET,
                 g_param_spec_object ("widget", NULL, NULL,
                                      GTK_TYPE_WIDGET,
                                      (G_PARAM_READABLE | G_PARAM_WRITABLE)));
        g_object_class_install_property
                (gobject_class,
                 PROP_X,
                 g_param_spec_double ("x", NULL, NULL,
				      -G_MAXDOUBLE, G_MAXDOUBLE, 0.0,
				      (G_PARAM_READABLE | G_PARAM_WRITABLE)));
        g_object_class_install_property
                (gobject_class,
                 PROP_Y,
                 g_param_spec_double ("y", NULL, NULL,
				      -G_MAXDOUBLE, G_MAXDOUBLE, 0.0,
				      (G_PARAM_READABLE | G_PARAM_WRITABLE)));
        g_object_class_install_property
                (gobject_class,
                 PROP_WIDTH,
                 g_param_spec_double ("width", NULL, NULL,
				      -G_MAXDOUBLE, G_MAXDOUBLE, 0.0,
				      (G_PARAM_READABLE | G_PARAM_WRITABLE)));
        g_object_class_install_property
                (gobject_class,
                 PROP_HEIGHT,
                 g_param_spec_double ("height", NULL, NULL,
				      -G_MAXDOUBLE, G_MAXDOUBLE, 0.0,
				      (G_PARAM_READABLE | G_PARAM_WRITABLE)));
        g_object_class_install_property
                (gobject_class,
                 PROP_ANCHOR,
                 g_param_spec_enum ("anchor", NULL, NULL,
                                    GTK_TYPE_ANCHOR_TYPE,
                                    GTK_ANCHOR_NW,
                                    (G_PARAM_READABLE | G_PARAM_WRITABLE)));
        g_object_class_install_property
                (gobject_class,
                 PROP_SIZE_PIXELS,
                 g_param_spec_boolean ("size_pixels", NULL, NULL,
				       FALSE,
				       (G_PARAM_READABLE | G_PARAM_WRITABLE)));

	object_class->destroy = gnome_canvas_widget_destroy;

	item_class->update = gnome_canvas_widget_update;
	item_class->point = gnome_canvas_widget_point;
	item_class->bounds = gnome_canvas_widget_bounds;
	item_class->render = gnome_canvas_widget_render;
	item_class->draw = gnome_canvas_widget_draw;
}

static void
gnome_canvas_widget_init (GnomeCanvasWidget *witem)
{
	witem->x = 0.0;
	witem->y = 0.0;
	witem->width = 0.0;
	witem->height = 0.0;
	witem->anchor = GTK_ANCHOR_NW;
	witem->size_pixels = FALSE;
}

static void
gnome_canvas_widget_destroy (GtkObject *object)
{
	GnomeCanvasWidget *witem;

	g_return_if_fail (object != NULL);
	g_return_if_fail (GNOME_IS_CANVAS_WIDGET (object));

	witem = GNOME_CANVAS_WIDGET (object);

	if (witem->widget && !witem->in_destroy) {
		g_signal_handler_disconnect (witem->widget, witem->destroy_id);
		gtk_widget_destroy (witem->widget);
		witem->widget = NULL;
	}

	if (GTK_OBJECT_CLASS (parent_class)->destroy)
		(* GTK_OBJECT_CLASS (parent_class)->destroy) (object);
}

static void
recalc_bounds (GnomeCanvasWidget *witem)
{
	GnomeCanvasItem *item;
	double wx, wy;

	item = GNOME_CANVAS_ITEM (witem);

	/* Get world coordinates */

	wx = witem->x;
	wy = witem->y;
	gnome_canvas_item_i2w (item, &wx, &wy);

	/* Get canvas pixel coordinates */

	gnome_canvas_w2c (item->canvas, wx, wy, &witem->cx, &witem->cy);

	/* Anchor widget item */

	switch (witem->anchor) {
	case GTK_ANCHOR_NW:
	case GTK_ANCHOR_W:
	case GTK_ANCHOR_SW:
		break;

	case GTK_ANCHOR_N:
	case GTK_ANCHOR_CENTER:
	case GTK_ANCHOR_S:
		witem->cx -= witem->cwidth / 2;
		break;

	case GTK_ANCHOR_NE:
	case GTK_ANCHOR_E:
	case GTK_ANCHOR_SE:
		witem->cx -= witem->cwidth;
		break;

        default:
                break;
	}

	switch (witem->anchor) {
	case GTK_ANCHOR_NW:
	case GTK_ANCHOR_N:
	case GTK_ANCHOR_NE:
		break;

	case GTK_ANCHOR_W:
	case GTK_ANCHOR_CENTER:
	case GTK_ANCHOR_E:
		witem->cy -= witem->cheight / 2;
		break;

	case GTK_ANCHOR_SW:
	case GTK_ANCHOR_S:
	case GTK_ANCHOR_SE:
		witem->cy -= witem->cheight;
		break;

        default:
                break;
	}

	/* Bounds */

	item->x1 = witem->cx;
	item->y1 = witem->cy;
	item->x2 = witem->cx + witem->cwidth;
	item->y2 = witem->cy + witem->cheight;

	if (witem->widget)
		gtk_layout_move (GTK_LAYOUT (item->canvas), witem->widget,
				 witem->cx + item->canvas->zoom_xofs,
				 witem->cy + item->canvas->zoom_yofs);
}

static void
do_destroy (GtkObject *object, gpointer data)
{
	GnomeCanvasWidget *witem;

	witem = data;

	witem->in_destroy = TRUE;

	gtk_object_destroy (data);
}

static void
gnome_canvas_widget_set_property (GObject            *object,
				  guint               param_id,
				  const GValue       *value,
				  GParamSpec         *pspec)
{
	GnomeCanvasItem *item;
	GnomeCanvasWidget *witem;
	GObject *obj;
	int update;
	int calc_bounds;

	g_return_if_fail (object != NULL);
	g_return_if_fail (GNOME_IS_CANVAS_WIDGET (object));

	item = GNOME_CANVAS_ITEM (object);
	witem = GNOME_CANVAS_WIDGET (object);

	update = FALSE;
	calc_bounds = FALSE;

	switch (param_id) {
	case PROP_WIDGET:
		if (witem->widget) {
			g_signal_handler_disconnect (witem->widget, witem->destroy_id);
			gtk_container_remove (GTK_CONTAINER (item->canvas), witem->widget);
		}

		obj = g_value_get_object (value);
		if (obj) {
			witem->widget = GTK_WIDGET (obj);
			witem->destroy_id = g_signal_connect (obj, "destroy",
							      G_CALLBACK (do_destroy),
							      witem);
			gtk_layout_put (GTK_LAYOUT (item->canvas), witem->widget,
					witem->cx + item->canvas->zoom_xofs,
					witem->cy + item->canvas->zoom_yofs);
		}

		update = TRUE;
		break;

	case PROP_X:
	        if (witem->x != g_value_get_double (value))
		{
		        witem->x = g_value_get_double (value);
			calc_bounds = TRUE;
		}
		break;

	case PROP_Y:
	        if (witem->y != g_value_get_double (value))
		{
		        witem->y = g_value_get_double (value);
			calc_bounds = TRUE;
		}
		break;

	case PROP_WIDTH:
	        if (witem->width != fabs (g_value_get_double (value)))
		{
		        witem->width = fabs (g_value_get_double (value));
			update = TRUE;
		}
		break;

	case PROP_HEIGHT:
	        if (witem->height != fabs (g_value_get_double (value)))
		{
		        witem->height = fabs (g_value_get_double (value));
			update = TRUE;
		}
		break;

	case PROP_ANCHOR:
	        if (witem->anchor != (unsigned int) g_value_get_enum (value))
		{
		        witem->anchor = g_value_get_enum (value);
			update = TRUE;
		}
		break;

	case PROP_SIZE_PIXELS:
	        if (witem->size_pixels != g_value_get_boolean (value))
		{
		        witem->size_pixels = g_value_get_boolean (value);
			update = TRUE;
		}
		break;

	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, param_id, pspec);
		break;
	}

	if (update)
		(* GNOME_CANVAS_ITEM_GET_CLASS (item)->update) (item, NULL, NULL, 0);

	if (calc_bounds)
		recalc_bounds (witem);
}

static void
gnome_canvas_widget_get_property (GObject            *object,
				  guint               param_id,
				  GValue             *value,
				  GParamSpec         *pspec)
{
	GnomeCanvasWidget *witem;

	g_return_if_fail (object != NULL);
	g_return_if_fail (GNOME_IS_CANVAS_WIDGET (object));

	witem = GNOME_CANVAS_WIDGET (object);

	switch (param_id) {
	case PROP_WIDGET:
		g_value_set_object (value, (GObject *) witem->widget);
		break;

	case PROP_X:
		g_value_set_double (value, witem->x);
		break;

	case PROP_Y:
		g_value_set_double (value, witem->y);
		break;

	case PROP_WIDTH:
		g_value_set_double (value, witem->width);
		break;

	case PROP_HEIGHT:
		g_value_set_double (value, witem->height);
		break;

	case PROP_ANCHOR:
		g_value_set_enum (value, witem->anchor);
		break;

	case PROP_SIZE_PIXELS:
		g_value_set_boolean (value, witem->size_pixels);
		break;

	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, param_id, pspec);
		break;
	}
}

static void
gnome_canvas_widget_update (GnomeCanvasItem *item, double *affine, ArtSVP *clip_path, int flags)
{
	GnomeCanvasWidget *witem;

	witem = GNOME_CANVAS_WIDGET (item);

	if (parent_class->update)
		(* parent_class->update) (item, affine, clip_path, flags);

	if (witem->widget) {
		if (witem->size_pixels) {
			witem->cwidth = (int) (witem->width + 0.5);
			witem->cheight = (int) (witem->height + 0.5);
		} else {
			witem->cwidth = (int) (witem->width * item->canvas->pixels_per_unit + 0.5);
			witem->cheight = (int) (witem->height * item->canvas->pixels_per_unit + 0.5);
		}

		gtk_widget_set_size_request (witem->widget, witem->cwidth, witem->cheight);
	} else {
		witem->cwidth = 0.0;
		witem->cheight = 0.0;
	}

	recalc_bounds (witem);
}

static void
gnome_canvas_widget_render (GnomeCanvasItem * item,
			    GnomeCanvasBuf * buf)
{
#if 0
	GnomeCanvasWidget *witem;

	witem = GNOME_CANVAS_WIDGET (item);

	if (witem->widget) 
		gtk_widget_queue_draw (witem->widget);
#endif

}

static void
gnome_canvas_widget_draw (GnomeCanvasItem * item,
			  GdkDrawable * drawable,
			  int x, int y,
			  int width, int height)
{
#if 0
	GnomeCanvasWidget *witem;

	witem = GNOME_CANVAS_WIDGET (item);

	if (witem->widget)
		gtk_widget_queue_draw (witem->widget);
#endif
}

static double
gnome_canvas_widget_point (GnomeCanvasItem *item, double x, double y,
			   int cx, int cy, GnomeCanvasItem **actual_item)
{
	GnomeCanvasWidget *witem;
	double x1, y1, x2, y2;
	double dx, dy;

	witem = GNOME_CANVAS_WIDGET (item);

	*actual_item = item;

	gnome_canvas_c2w (item->canvas, witem->cx, witem->cy, &x1, &y1);

	x2 = x1 + (witem->cwidth - 1) / item->canvas->pixels_per_unit;
	y2 = y1 + (witem->cheight - 1) / item->canvas->pixels_per_unit;

	/* Is point inside widget bounds? */

	if ((x >= x1) && (y >= y1) && (x <= x2) && (y <= y2))
		return 0.0;

	/* Point is outside widget bounds */

	if (x < x1)
		dx = x1 - x;
	else if (x > x2)
		dx = x - x2;
	else
		dx = 0.0;

	if (y < y1)
		dy = y1 - y;
	else if (y > y2)
		dy = y - y2;
	else
		dy = 0.0;

	return sqrt (dx * dx + dy * dy);
}

static void
gnome_canvas_widget_bounds (GnomeCanvasItem *item, double *x1, double *y1, double *x2, double *y2)
{
	GnomeCanvasWidget *witem;

	witem = GNOME_CANVAS_WIDGET (item);

	*x1 = witem->x;
	*y1 = witem->y;

	switch (witem->anchor) {
	case GTK_ANCHOR_NW:
	case GTK_ANCHOR_W:
	case GTK_ANCHOR_SW:
		break;

	case GTK_ANCHOR_N:
	case GTK_ANCHOR_CENTER:
	case GTK_ANCHOR_S:
		*x1 -= witem->width / 2.0;
		break;

	case GTK_ANCHOR_NE:
	case GTK_ANCHOR_E:
	case GTK_ANCHOR_SE:
		*x1 -= witem->width;
		break;

        default:
                break;
	}

	switch (witem->anchor) {
	case GTK_ANCHOR_NW:
	case GTK_ANCHOR_N:
	case GTK_ANCHOR_NE:
		break;

	case GTK_ANCHOR_W:
	case GTK_ANCHOR_CENTER:
	case GTK_ANCHOR_E:
		*y1 -= witem->height / 2.0;
		break;

	case GTK_ANCHOR_SW:
	case GTK_ANCHOR_S:
	case GTK_ANCHOR_SE:
		*y1 -= witem->height;
		break;

        default:
                break;
	}

	*x2 = *x1 + witem->width;
	*y2 = *y1 + witem->height;
}
