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
/* Rectangle and ellipse item types for GnomeCanvas widget
 *
 * GnomeCanvas is basically a port of the Tk toolkit's most excellent canvas widget.  Tk is
 * copyrighted by the Regents of the University of California, Sun Microsystems, and other parties.
 *
 *
 * Authors: Federico Mena <federico@nuclecu.unam.mx>
 *          Rusty Conover <rconover@bangtail.net>
 */

#include <config.h>
#include <math.h>
#include "gnome-canvas-rect-ellipse.h"
#include "gnome-canvas-util.h"
#include "gnome-canvas-shape.h"


#include "libart_lgpl/art_vpath.h"
#include "libart_lgpl/art_svp.h"
#include "libart_lgpl/art_svp_vpath.h"
#include "libart_lgpl/art_rgb_svp.h"

/* Base class for rectangle and ellipse item types */

#define noVERBOSE

enum {
	PROP_0,
	PROP_X1,
	PROP_Y1,
	PROP_X2,
	PROP_Y2
};


static void gnome_canvas_re_class_init (GnomeCanvasREClass *class);
static void gnome_canvas_re_init       (GnomeCanvasRE      *re);
static void gnome_canvas_re_destroy    (GtkObject          *object);
static void gnome_canvas_re_set_property (GObject              *object,
					  guint                 param_id,
					  const GValue         *value,
					  GParamSpec           *pspec);
static void gnome_canvas_re_get_property (GObject              *object,
					  guint                 param_id,
					  GValue               *value,
					  GParamSpec           *pspec);

static void gnome_canvas_rect_update      (GnomeCanvasItem *item, double *affine, ArtSVP *clip_path, int flags);
static void gnome_canvas_ellipse_update      (GnomeCanvasItem *item, double *affine, ArtSVP *clip_path, int flags);

static GnomeCanvasItemClass *re_parent_class;


GType
gnome_canvas_re_get_type (void)
{
	static GType re_type;

	if (!re_type) {
		const GTypeInfo object_info = {
			sizeof (GnomeCanvasREClass),
			(GBaseInitFunc) NULL,
			(GBaseFinalizeFunc) NULL,
			(GClassInitFunc) gnome_canvas_re_class_init,
			(GClassFinalizeFunc) NULL,
			NULL,			/* class_data */
			sizeof (GnomeCanvasRE),
			0,			/* n_preallocs */
			(GInstanceInitFunc) gnome_canvas_re_init,
			NULL			/* value_table */
		};

		re_type = g_type_register_static (GNOME_TYPE_CANVAS_SHAPE, "GnomeCanvasRE",
						  &object_info, 0);
	}

	return re_type;
}

static void
gnome_canvas_re_class_init (GnomeCanvasREClass *class)
{
	GObjectClass *gobject_class;
	GtkObjectClass *object_class;

	gobject_class = (GObjectClass *) class;
	object_class = (GtkObjectClass *) class;

	re_parent_class = g_type_class_peek_parent (class);

	gobject_class->set_property = gnome_canvas_re_set_property;
	gobject_class->get_property = gnome_canvas_re_get_property;

        g_object_class_install_property
                (gobject_class,
                 PROP_X1,
                 g_param_spec_double ("x1", NULL, NULL,
				      -G_MAXDOUBLE, G_MAXDOUBLE, 0,
				      (G_PARAM_READABLE | G_PARAM_WRITABLE)));
        g_object_class_install_property
                (gobject_class,
                 PROP_Y1,
                 g_param_spec_double ("y1", NULL, NULL,
				      -G_MAXDOUBLE, G_MAXDOUBLE, 0,
				      (G_PARAM_READABLE | G_PARAM_WRITABLE)));
        g_object_class_install_property
                (gobject_class,
                 PROP_X2,
                 g_param_spec_double ("x2", NULL, NULL,
				      -G_MAXDOUBLE, G_MAXDOUBLE, 0,
				      (G_PARAM_READABLE | G_PARAM_WRITABLE)));
        g_object_class_install_property
                (gobject_class,
                 PROP_Y2,
                 g_param_spec_double ("y2", NULL, NULL,
				      -G_MAXDOUBLE, G_MAXDOUBLE, 0,
				      (G_PARAM_READABLE | G_PARAM_WRITABLE)));

	object_class->destroy = gnome_canvas_re_destroy;
}

static void
gnome_canvas_re_init (GnomeCanvasRE *re)
{
	re->x1 = 0.0;
	re->y1 = 0.0;
	re->x2 = 0.0;
	re->y2 = 0.0;
	re->path_dirty = 0;
}

static void
gnome_canvas_re_destroy (GtkObject *object)
{
	g_return_if_fail (object != NULL);
	g_return_if_fail (GNOME_IS_CANVAS_RE (object));

	if (GTK_OBJECT_CLASS (re_parent_class)->destroy)
		(* GTK_OBJECT_CLASS (re_parent_class)->destroy) (object);
}

static void
gnome_canvas_re_set_property (GObject              *object,
			      guint                 param_id,
			      const GValue         *value,
			      GParamSpec           *pspec)
{
	GnomeCanvasItem *item;
	GnomeCanvasRE *re;

	g_return_if_fail (object != NULL);
	g_return_if_fail (GNOME_IS_CANVAS_RE (object));

	item = GNOME_CANVAS_ITEM (object);
	re = GNOME_CANVAS_RE (object);

	switch (param_id) {
	case PROP_X1:
		re->x1 = g_value_get_double (value);
		re->path_dirty = 1;
		gnome_canvas_item_request_update (item);
		break;

	case PROP_Y1:
		re->y1 = g_value_get_double (value);
		re->path_dirty = 1;
		gnome_canvas_item_request_update (item);
		break;

	case PROP_X2:
		re->x2 = g_value_get_double (value);
		re->path_dirty = 1;
		gnome_canvas_item_request_update (item);
		break;

	case PROP_Y2:
		re->y2 = g_value_get_double (value);
		re->path_dirty = 1;
		gnome_canvas_item_request_update (item);
		break;

	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, param_id, pspec);
		break;
	}
}

static void
gnome_canvas_re_get_property (GObject              *object,
			      guint                 param_id,
			      GValue               *value,
			      GParamSpec           *pspec)
{
	GnomeCanvasRE *re;

	g_return_if_fail (object != NULL);
	g_return_if_fail (GNOME_IS_CANVAS_RE (object));

	re = GNOME_CANVAS_RE (object);

	switch (param_id) {
	case PROP_X1:
		g_value_set_double (value,  re->x1);
		break;

	case PROP_Y1:
		g_value_set_double (value,  re->y1);
		break;

	case PROP_X2:
		g_value_set_double (value,  re->x2);
		break;

	case PROP_Y2:
		g_value_set_double (value,  re->y2);
		break;

	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, param_id, pspec);
		break;
	}
}

/* Rectangle item */
static void gnome_canvas_rect_class_init (GnomeCanvasRectClass *class);



GType
gnome_canvas_rect_get_type (void)
{
	static GType rect_type;

	if (!rect_type) {
		const GTypeInfo object_info = {
			sizeof (GnomeCanvasRectClass),
			(GBaseInitFunc) NULL,
			(GBaseFinalizeFunc) NULL,
			(GClassInitFunc) gnome_canvas_rect_class_init,
			(GClassFinalizeFunc) NULL,
			NULL,			/* class_data */
			sizeof (GnomeCanvasRect),
			0,			/* n_preallocs */
			(GInstanceInitFunc) NULL,
			NULL			/* value_table */
		};

		rect_type = g_type_register_static (GNOME_TYPE_CANVAS_RE, "GnomeCanvasRect",
						    &object_info, 0);
	}

	return rect_type;
}

static void
gnome_canvas_rect_class_init (GnomeCanvasRectClass *class)
{
	GnomeCanvasItemClass *item_class;

	item_class = (GnomeCanvasItemClass *) class;

	item_class->update = gnome_canvas_rect_update;
}

static void
gnome_canvas_rect_update (GnomeCanvasItem *item, double affine[6], ArtSVP *clip_path, gint flags)
{	GnomeCanvasRE *re;	

	GnomeCanvasPathDef *path_def;

	re = GNOME_CANVAS_RE(item);

	if (re->path_dirty) {		
		path_def = gnome_canvas_path_def_new ();
		
		gnome_canvas_path_def_moveto(path_def, re->x1, re->y1);
		gnome_canvas_path_def_lineto(path_def, re->x2, re->y1);
		gnome_canvas_path_def_lineto(path_def, re->x2, re->y2);
		gnome_canvas_path_def_lineto(path_def, re->x1, re->y2);
		gnome_canvas_path_def_lineto(path_def, re->x1, re->y1);		
		gnome_canvas_path_def_closepath_current(path_def);		
		gnome_canvas_shape_set_path_def (GNOME_CANVAS_SHAPE (item), path_def);
		gnome_canvas_path_def_unref(path_def);
		re->path_dirty = 0;
	}

	if (re_parent_class->update)
		(* re_parent_class->update) (item, affine, clip_path, flags);
}

/* Ellipse item */


static void gnome_canvas_ellipse_class_init (GnomeCanvasEllipseClass *class);


GType
gnome_canvas_ellipse_get_type (void)
{
	static GType ellipse_type;

	if (!ellipse_type) {
		const GTypeInfo object_info = {
			sizeof (GnomeCanvasEllipseClass),
			(GBaseInitFunc) NULL,
			(GBaseFinalizeFunc) NULL,
			(GClassInitFunc) gnome_canvas_ellipse_class_init,
			(GClassFinalizeFunc) NULL,
			NULL,			/* class_data */
			sizeof (GnomeCanvasEllipse),
			0,			/* n_preallocs */
			(GInstanceInitFunc) NULL,
			NULL			/* value_table */
		};

		ellipse_type = g_type_register_static (GNOME_TYPE_CANVAS_RE, "GnomeCanvasEllipse",
						       &object_info, 0);
	}

	return ellipse_type;
}

static void
gnome_canvas_ellipse_class_init (GnomeCanvasEllipseClass *class)
{
	GnomeCanvasItemClass *item_class;

	item_class = (GnomeCanvasItemClass *) class;

	item_class->update = gnome_canvas_ellipse_update;
}

#define N_PTS 90

static void
gnome_canvas_ellipse_update (GnomeCanvasItem *item, double affine[6], ArtSVP *clip_path, gint flags) {
	GnomeCanvasPathDef *path_def;
	GnomeCanvasRE *re;

	re = GNOME_CANVAS_RE(item);

	if (re->path_dirty) {
		gdouble cx, cy, rx, ry;
		gdouble beta = 0.26521648983954400922; /* 4*(1-cos(pi/8))/(3*sin(pi/8)) */
		gdouble sincosA = 0.70710678118654752440; /* sin (pi/4), cos (pi/4) */
		gdouble dx1, dy1, dx2, dy2;
		gdouble mx, my;

		path_def = gnome_canvas_path_def_new();

		cx = (re->x2 + re->x1) * 0.5;
		cy = (re->y2 + re->y1) * 0.5;
		rx = re->x2 - cx;
		ry = re->y2 - cy;

		dx1 = beta * rx;
		dy1 = beta * ry;
		dx2 = beta * rx * sincosA;
		dy2 = beta * ry * sincosA;
		mx = rx * sincosA;
		my = ry * sincosA;

		gnome_canvas_path_def_moveto (path_def, cx + rx, cy);
		gnome_canvas_path_def_curveto (path_def,
					       cx + rx, cy - dy1,
					       cx + mx + dx2, cy - my + dy2,
					       cx + mx, cy - my);
		gnome_canvas_path_def_curveto (path_def,
					       cx + mx - dx2, cy - my - dy2,
					       cx + dx1, cy - ry,
					       cx, cy - ry);
		gnome_canvas_path_def_curveto (path_def,
					       cx - dx1, cy - ry,
					       cx - mx + dx2, cy - my - dy2,
					       cx - mx, cy - my);
		gnome_canvas_path_def_curveto (path_def,
					       cx - mx - dx2, cy - my + dy2,
					       cx - rx, cy - dy1,
					       cx - rx, cy);
		
		gnome_canvas_path_def_curveto (path_def,
					       cx - rx, cy + dy1,
					       cx - mx - dx2, cy + my - dy2,
					       cx - mx, cy + my);
		gnome_canvas_path_def_curveto (path_def,
					       cx - mx + dx2, cy + my + dy2,
					       cx - dx1, cy + ry,
					       cx, cy + ry);
		gnome_canvas_path_def_curveto (path_def,
					       cx + dx1, cy + ry,
					       cx + mx - dx2, cy + my + dy2,
					       cx + mx, cy + my);
		gnome_canvas_path_def_curveto (path_def,
					       cx + mx + dx2, cy + my - dy2,
					       cx + rx, cy + dy1,
					       cx + rx, cy);
		
		gnome_canvas_path_def_closepath_current(path_def);
		
		gnome_canvas_shape_set_path_def (GNOME_CANVAS_SHAPE (item), path_def);
		gnome_canvas_path_def_unref(path_def);
		re->path_dirty = 0;
	}

	if (re_parent_class->update)
		(* re_parent_class->update) (item, affine, clip_path, flags);
}
