/* Generic bezier shape item for GnomeCanvasWidget.  Most code taken
 * from gnome-canvas-bpath but made into a shape item.
 *
 * GnomeCanvas is basically a port of the Tk toolkit's most excellent
 * canvas widget.  Tk is copyrighted by the Regents of the University
 * of California, Sun Microsystems, and other parties.
 *
 * Copyright (C) 1998,1999 The Free Software Foundation
 *
 * Authors: Federico Mena <federico@nuclecu.unam.mx>
 *          Raph Levien <raph@acm.org>
 *          Lauris Kaplinski <lauris@ximian.com>
 *          Miguel de Icaza <miguel@kernel.org>
 *          Cody Russell <bratsche@gnome.org>
 *          Rusty Conover <rconover@bangtail.net>
 */

/* These includes are set up for standalone compile. If/when this codebase
   is integrated into libgnomeui, the includes will need to change. */

#include <math.h>
#include <string.h>

#include <gtk/gtk.h>
#include "gnome-canvas.h"
#include "gnome-canvas-util.h"

#include "gnome-canvas-shape.h"
#include "gnome-canvas-shape-private.h"
#include "gnome-canvas-path-def.h"

#include <libart_lgpl/art_rect.h>
#include <libart_lgpl/art_vpath.h>
#include <libart_lgpl/art_bpath.h>
#include <libart_lgpl/art_vpath_bpath.h>
#include <libart_lgpl/art_svp.h>
#include <libart_lgpl/art_svp_point.h>
#include <libart_lgpl/art_svp_vpath.h>
#include <libart_lgpl/art_vpath_dash.h>
#include <libart_lgpl/art_svp_wind.h>
#include <libart_lgpl/art_svp_intersect.h>
#include <libart_lgpl/art_rect_svp.h>

enum {
	PROP_0,
	PROP_FILL_COLOR,
	PROP_FILL_COLOR_GDK,
	PROP_FILL_COLOR_RGBA,
	PROP_OUTLINE_COLOR,
	PROP_OUTLINE_COLOR_GDK,
	PROP_OUTLINE_COLOR_RGBA,
	PROP_FILL_STIPPLE,
	PROP_OUTLINE_STIPPLE,
	PROP_WIDTH_PIXELS,
	PROP_WIDTH_UNITS,
	PROP_CAP_STYLE,
	PROP_JOIN_STYLE,
	PROP_WIND,
	PROP_MITERLIMIT,
	PROP_DASH
};

static void gnome_canvas_shape_class_init   (GnomeCanvasShapeClass *class);
static void gnome_canvas_shape_init         (GnomeCanvasShape      *bpath);
static void gnome_canvas_shape_destroy      (GtkObject               *object);
static void gnome_canvas_shape_set_property (GObject               *object,
					     guint                  param_id,
					     const GValue          *value,
                                             GParamSpec            *pspec);
static void gnome_canvas_shape_get_property (GObject               *object,
					     guint                  param_id,
					     GValue                *value,
                                             GParamSpec            *pspec);

static void   gnome_canvas_shape_update      (GnomeCanvasItem *item, double *affine, ArtSVP *clip_path, int flags);
static void   gnome_canvas_shape_realize     (GnomeCanvasItem *item);
static void   gnome_canvas_shape_unrealize   (GnomeCanvasItem *item);
static void   gnome_canvas_shape_draw        (GnomeCanvasItem *item, GdkDrawable *drawable,
                                              int x, int y, int width, int height);
static double gnome_canvas_shape_point       (GnomeCanvasItem *item, double x, double y,
                                              int cx, int cy, GnomeCanvasItem **actual_item);
static void   gnome_canvas_shape_render      (GnomeCanvasItem *item, GnomeCanvasBuf *buf);
static void   gnome_canvas_shape_bounds      (GnomeCanvasItem *item,
					      double *x1, double *y1, double *x2, double *y2);

static gulong get_pixel_from_rgba (GnomeCanvasItem *item, guint32 rgba_color);
static guint32 get_rgba_from_color (GdkColor * color);
static void set_gc_foreground (GdkGC *gc, gulong pixel);
static void gcbp_ensure_gdk (GnomeCanvasShape * bpath);
static void gcbp_destroy_gdk (GnomeCanvasShape * bpath);
static void set_stipple (GdkGC *gc, GdkBitmap **internal_stipple, GdkBitmap *stipple, int reconfigure);
static void gcbp_ensure_mask (GnomeCanvasShape * bpath, gint width, gint height);
static void gcbp_draw_ctx_unref (GCBPDrawCtx * ctx);

static GnomeCanvasItemClass *parent_class;

GType
gnome_canvas_shape_get_type (void)
{
	static GType shape_type;

	if (!shape_type) {
		const GTypeInfo object_info = {
			sizeof (GnomeCanvasShapeClass),
			(GBaseInitFunc) NULL,
			(GBaseFinalizeFunc) NULL,
			(GClassInitFunc) gnome_canvas_shape_class_init,
			(GClassFinalizeFunc) NULL,
			NULL,			/* class_data */
			sizeof (GnomeCanvasShape),
			0,			/* n_preallocs */
			(GInstanceInitFunc) gnome_canvas_shape_init,
			NULL			/* value_table */
		};

		shape_type = g_type_register_static (GNOME_TYPE_CANVAS_ITEM, "GnomeCanvasShape",
						     &object_info, 0);
	}

	return shape_type;
}

static void
gnome_canvas_shape_class_init (GnomeCanvasShapeClass *class)
{
	GObjectClass         *gobject_class;
	GtkObjectClass       *object_class;
	GnomeCanvasItemClass *item_class;

	gobject_class = (GObjectClass *) class;
	object_class = (GtkObjectClass *) class;
	item_class = (GnomeCanvasItemClass *) class;

	parent_class = g_type_class_peek_parent (class);

	/* when this gets checked into libgnomeui, change the
           GTK_TYPE_POINTER to GTK_TYPE_GNOME_CANVAS_SHAPE, and add an
           entry to gnome-boxed.defs */

	gobject_class->set_property = gnome_canvas_shape_set_property;
	gobject_class->get_property = gnome_canvas_shape_get_property;



        g_object_class_install_property (gobject_class,
                                         PROP_FILL_COLOR,
                                         g_param_spec_string ("fill_color", NULL, NULL,
                                                              NULL,
                                                              (G_PARAM_WRITABLE)));
        g_object_class_install_property (gobject_class,
                                         PROP_FILL_COLOR_GDK,
                                         g_param_spec_boxed ("fill_color_gdk", NULL, NULL,
                                                             GDK_TYPE_COLOR,
                                                             (G_PARAM_READABLE | G_PARAM_WRITABLE)));
        g_object_class_install_property (gobject_class,
                                         PROP_FILL_COLOR_RGBA,
                                         g_param_spec_uint ("fill_color_rgba", NULL, NULL,
                                                            0, G_MAXUINT, 0,
                                                            (G_PARAM_READABLE | G_PARAM_WRITABLE)));
        g_object_class_install_property (gobject_class,
                                         PROP_OUTLINE_COLOR,
                                         g_param_spec_string ("outline_color", NULL, NULL,
                                                              NULL,
                                                              (G_PARAM_WRITABLE)));
        g_object_class_install_property (gobject_class,
                                         PROP_OUTLINE_COLOR_GDK,
                                         g_param_spec_boxed ("outline_color_gdk", NULL, NULL,
                                                             GDK_TYPE_COLOR,
                                                             (G_PARAM_READABLE | G_PARAM_WRITABLE)));
        g_object_class_install_property (gobject_class,
                                         PROP_OUTLINE_COLOR_RGBA,
                                         g_param_spec_uint ("outline_color_rgba", NULL, NULL,
                                                            0, G_MAXUINT, 0,
                                                            (G_PARAM_READABLE | G_PARAM_WRITABLE)));
        g_object_class_install_property (gobject_class,
                                         PROP_FILL_STIPPLE,
                                         g_param_spec_object ("fill_stipple", NULL, NULL,
                                                              GDK_TYPE_DRAWABLE,
                                                              (G_PARAM_READABLE | G_PARAM_WRITABLE)));
        g_object_class_install_property (gobject_class,
                                         PROP_OUTLINE_STIPPLE,
                                         g_param_spec_object ("outline_stipple", NULL, NULL,
                                                              GDK_TYPE_DRAWABLE,
                                                              (G_PARAM_READABLE | G_PARAM_WRITABLE)));
        g_object_class_install_property (gobject_class,
                                         PROP_WIDTH_PIXELS,
                                         g_param_spec_uint ("width_pixels", NULL, NULL,
                                                            0, G_MAXUINT, 0,
                                                            (G_PARAM_READABLE | G_PARAM_WRITABLE)));
        g_object_class_install_property (gobject_class,
                                         PROP_WIDTH_UNITS,
                                         g_param_spec_double ("width_units", NULL, NULL,
                                                              0.0, G_MAXDOUBLE, 0.0,
                                                              (G_PARAM_READABLE | G_PARAM_WRITABLE)));
        g_object_class_install_property (gobject_class,
                                         PROP_CAP_STYLE,
                                         g_param_spec_enum ("cap_style", NULL, NULL,
                                                            GDK_TYPE_CAP_STYLE,
                                                            GDK_CAP_BUTT,
                                                            (G_PARAM_READABLE | G_PARAM_WRITABLE)));
        g_object_class_install_property (gobject_class,
                                         PROP_JOIN_STYLE,
                                         g_param_spec_enum ("join_style", NULL, NULL,
                                                            GDK_TYPE_JOIN_STYLE,
                                                            GDK_JOIN_MITER,
                                                            (G_PARAM_READABLE | G_PARAM_WRITABLE)));
        g_object_class_install_property (gobject_class,
                                         PROP_WIND,
                                         g_param_spec_uint ("wind", NULL, NULL,
                                                            0, G_MAXUINT, 0,
                                                            (G_PARAM_READABLE | G_PARAM_WRITABLE)));
        g_object_class_install_property (gobject_class,
                                         PROP_MITERLIMIT,
                                         g_param_spec_double ("miterlimit", NULL, NULL,
                                                              0.0, G_MAXDOUBLE, 0.0,
                                                              (G_PARAM_READABLE | G_PARAM_WRITABLE)));
        g_object_class_install_property (gobject_class,
                                         PROP_DASH,
                                         g_param_spec_pointer ("dash", NULL, NULL,
                                                               (G_PARAM_READABLE | G_PARAM_WRITABLE)));

	object_class->destroy = gnome_canvas_shape_destroy;

	item_class->update = gnome_canvas_shape_update;
	item_class->realize = gnome_canvas_shape_realize;
	item_class->unrealize = gnome_canvas_shape_unrealize;
	item_class->draw = gnome_canvas_shape_draw;
	item_class->point = gnome_canvas_shape_point;
	item_class->render = gnome_canvas_shape_render;
	item_class->bounds = gnome_canvas_shape_bounds;
}

static void
gnome_canvas_shape_init (GnomeCanvasShape *shape)
{
	shape->priv = g_new (GnomeCanvasShapePriv, 1);

	shape->priv->path = NULL;

	shape->priv->scale = 1.0;

	shape->priv->fill_set = FALSE;
	shape->priv->outline_set = FALSE;
	shape->priv->width_pixels = FALSE;

	shape->priv->width = 1.0;

	shape->priv->fill_rgba = 0x0000003f;
	shape->priv->outline_rgba = 0x0000007f;

	shape->priv->cap = GDK_CAP_BUTT;
	shape->priv->join = GDK_JOIN_MITER;
	shape->priv->wind = ART_WIND_RULE_ODDEVEN;
	shape->priv->miterlimit = 10.43;	   /* X11 default */

	shape->priv->dash.n_dash = 0;
	shape->priv->dash.dash = NULL;

	shape->priv->fill_svp = NULL;
	shape->priv->outline_svp = NULL;

	shape->priv->gdk = NULL;
}

static void
gnome_canvas_shape_destroy (GtkObject *object)
{
	GnomeCanvasShape *shape;
	GnomeCanvasShapePriv *priv;

	g_return_if_fail (object != NULL);
	g_return_if_fail (GNOME_IS_CANVAS_SHAPE (object));

        shape = GNOME_CANVAS_SHAPE (object);

	if (shape->priv) {
		priv = shape->priv;
		if (priv->gdk) gcbp_destroy_gdk (shape);

		if (priv->path) gnome_canvas_path_def_unref (priv->path);

		if (priv->dash.dash) g_free (priv->dash.dash);
		if (priv->fill_svp) art_svp_free (priv->fill_svp);
		if (priv->outline_svp) art_svp_free (priv->outline_svp);
		
		g_free (shape->priv);
	        shape->priv = NULL;
	}

	if (GTK_OBJECT_CLASS (parent_class)->destroy)
		(* GTK_OBJECT_CLASS (parent_class)->destroy) (object);
}

/**
 * gnome_canvas_shape_set_path_def:
 * @shape: a GnomeCanvasShape
 * @def: a GnomeCanvasPathDef 
 *
 * This function sets the the GnomeCanvasPathDef used by the
 * GnomeCanvasShape. Notice, that it does not request updates, as
 * it is meant to be used from item implementations, from inside
 * update queue.
 */
 
void
gnome_canvas_shape_set_path_def (GnomeCanvasShape *shape, GnomeCanvasPathDef *def) 
{
	GnomeCanvasShapePriv *priv;

	g_return_if_fail (shape != NULL);
	g_return_if_fail (GNOME_IS_CANVAS_SHAPE (shape));

	priv = shape->priv;

	if (priv->path) {
		gnome_canvas_path_def_unref (priv->path);
		priv->path = NULL;
	}

	if (def) {
		priv->path = gnome_canvas_path_def_duplicate (def);
	}
}

static void
gnome_canvas_shape_set_property (GObject      *object,
                                 guint         param_id,
                                 const GValue *value,
                                 GParamSpec   *pspec)
{
	GnomeCanvasItem         *item;
	GnomeCanvasShape        *shape;
	GnomeCanvasShapePriv    *priv;
	GnomeCanvasShapePrivGdk *gdk;
	GdkColor                 color;
	GdkColor                *colorptr;
	ArtVpathDash            *dash;

	item = GNOME_CANVAS_ITEM (object);
	shape = GNOME_CANVAS_SHAPE (object);
	priv = shape->priv;

	if (!item->canvas->aa) {
		gcbp_ensure_gdk (shape);
		gdk = priv->gdk;
	} else {
		gdk = NULL;
	}

	switch (param_id) {
	case PROP_FILL_COLOR:
		if (gnome_canvas_get_color (item->canvas, g_value_get_string (value), &color)) {
			priv->fill_set = TRUE;
			priv->fill_rgba = get_rgba_from_color (&color);
			if (gdk) gdk->fill_pixel = color.pixel;
		} else if (priv->fill_set)
			priv->fill_set = FALSE;
		else
			break;

		gnome_canvas_item_request_update (item);
		break;

	case PROP_FILL_COLOR_GDK:
		colorptr = g_value_get_boxed (value);
		if (colorptr != NULL) {
			priv->fill_set = TRUE;
			priv->fill_rgba = get_rgba_from_color (colorptr);
			if (gdk) {
				GdkColormap *colormap = gtk_widget_get_colormap (GTK_WIDGET (item->canvas));
				GdkColor tmp = *colorptr;
				gdk_rgb_find_color (colormap, &tmp);
				gdk->fill_pixel = tmp.pixel;
			}
		} else if (priv->fill_set)
			priv->fill_set = FALSE;
		else
			break;

		gnome_canvas_item_request_update (item);
		break;

	case PROP_FILL_COLOR_RGBA:
		priv->fill_set = TRUE;
		priv->fill_rgba = g_value_get_uint (value);
		if (gdk) gdk->fill_pixel = get_pixel_from_rgba (item, priv->fill_rgba);

		gnome_canvas_item_request_update (item);
		break;

	case PROP_OUTLINE_COLOR:
		if (gnome_canvas_get_color (item->canvas, g_value_get_string (value), &color)) {
			priv->outline_set = TRUE;
			priv->outline_rgba = get_rgba_from_color (&color);
			if (gdk) gdk->outline_pixel = color.pixel;
		} else if (priv->outline_set)
			priv->outline_set = FALSE;
		else
			break;

		gnome_canvas_item_request_update (item);
		break;

	case PROP_OUTLINE_COLOR_GDK:
		colorptr = g_value_get_boxed (value);
		if (colorptr != NULL) {
			priv->outline_set = TRUE;
			priv->outline_rgba = get_rgba_from_color (colorptr);
			if (gdk) {
				GdkColormap *colormap = gtk_widget_get_colormap (GTK_WIDGET (item->canvas));
				GdkColor tmp = *colorptr;
				gdk_rgb_find_color (colormap, &tmp);
				gdk->outline_pixel = tmp.pixel;
			}
		} else if (priv->outline_set)
			priv->outline_set = FALSE;
		else
			break;

		gnome_canvas_item_request_update (item);
		break;

	case PROP_OUTLINE_COLOR_RGBA:
		priv->outline_set = TRUE;
		priv->outline_rgba = g_value_get_uint (value);
		if (gdk) gdk->outline_pixel = get_pixel_from_rgba (item, priv->outline_rgba);

		gnome_canvas_item_request_update (item);
		break;

	case PROP_FILL_STIPPLE:
		if (gdk) {
			set_stipple (gdk->fill_gc, &gdk->fill_stipple, (GdkBitmap*) g_value_get_object (value), FALSE);
			gnome_canvas_item_request_update (item);
		}
		break;

	case PROP_OUTLINE_STIPPLE:
		if (gdk) {
			set_stipple (gdk->outline_gc, &gdk->outline_stipple, (GdkBitmap*) g_value_get_object (value), FALSE);
			gnome_canvas_item_request_update (item);
		}
		break;

	case PROP_WIDTH_PIXELS:
		priv->width = g_value_get_uint (value);
		priv->width_pixels = TRUE;

		gnome_canvas_item_request_update (item);
		break;

	case PROP_WIDTH_UNITS:
		priv->width = fabs (g_value_get_double (value));
		priv->width_pixels = FALSE;

		gnome_canvas_item_request_update (item);
		break;

	case PROP_WIND:
		priv->wind = g_value_get_uint (value);
		gnome_canvas_item_request_update (item);
		break;

	case PROP_CAP_STYLE:
		priv->cap = g_value_get_enum (value);
		gnome_canvas_item_request_update (item);
		break;

	case PROP_JOIN_STYLE:
		priv->join = g_value_get_enum (value);
		gnome_canvas_item_request_update (item);
		break;
	
	case PROP_MITERLIMIT:
		priv->miterlimit = g_value_get_double (value);
		gnome_canvas_item_request_update (item);
		break;

	case PROP_DASH:
		dash = g_value_get_pointer (value);
		if (priv->dash.dash) g_free (priv->dash.dash);
		priv->dash.dash = NULL;

		if (dash) {
			priv->dash.offset = dash->offset;
			priv->dash.n_dash = dash->n_dash;
			if (dash->dash != NULL) {
				priv->dash.dash = g_new (double, dash->n_dash);
				memcpy (priv->dash.dash, dash->dash, dash->n_dash * sizeof (double));
			}
		}
		gnome_canvas_item_request_update (item);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, param_id, pspec);
		break;
	}
}

/* Allocates a GdkColor structure filled with the specified pixel, and
 * puts it into the specified value for returning it in the get_property 
 * method.
 */

static void
get_color_value (GnomeCanvasShape *shape, gulong pixel, GValue *value)
{
	GnomeCanvas *canvas = GNOME_CANVAS_ITEM (shape)->canvas;
	GdkColormap *colormap = gtk_widget_get_colormap (GTK_WIDGET (canvas));
	GdkColor color;

	gdk_colormap_query_color (colormap, pixel, &color);
	g_value_set_boxed (value, &color);
}

/**
 * gnome_canvas_shape_get_path_def:
 * @shape: a GnomeCanvasShape
 *
 * This function returns the #GnomeCanvasPathDef that the shape
 * currently uses.  It adds a reference to the #GnomeCanvasPathDef and
 * returns it, if there is not a #GnomeCanvasPathDef set for the shape
 * it returns NULL.
 *
 * Returns: a #GnomeCanvasPathDef or NULL if none is set for the shape.
 */
 
GnomeCanvasPathDef *
gnome_canvas_shape_get_path_def (GnomeCanvasShape *shape)
{
	GnomeCanvasShapePriv *priv;
	
	g_return_val_if_fail (shape != NULL, NULL);
	g_return_val_if_fail (GNOME_IS_CANVAS_SHAPE (shape), NULL);

	priv = shape->priv;

	if (priv->path) {
		gnome_canvas_path_def_ref (priv->path);
		return priv->path;
	}
	
	return NULL;
}

static void
gnome_canvas_shape_get_property (GObject     *object,
                                 guint        param_id,
                                 GValue      *value,
                                 GParamSpec  *pspec)
{
	GnomeCanvasItem         *item = GNOME_CANVAS_ITEM (object);
	GnomeCanvasShape        *shape = GNOME_CANVAS_SHAPE (object);
	GnomeCanvasShapePriv    *priv = shape->priv;
	GnomeCanvasShapePrivGdk *gdk;

	if (!item->canvas->aa) {
		gcbp_ensure_gdk (shape);
		gdk = priv->gdk;
	}
	else {
		gdk = NULL;
	}

	switch (param_id) {
	case PROP_FILL_COLOR_GDK:
		if (gdk) {
			get_color_value (shape, gdk->fill_pixel, value);
		} else {
			get_color_value (shape, 0, value);
		}
		break;
		
	case PROP_OUTLINE_COLOR_GDK:
		if (gdk) {
			get_color_value (shape, gdk->outline_pixel, value);
		} else {
			get_color_value (shape, 0, value);
		}
		break;

	case PROP_FILL_COLOR_RGBA:
		g_value_set_uint (value, priv->fill_rgba);
		break;

	case PROP_OUTLINE_COLOR_RGBA:
		g_value_set_uint (value, priv->outline_rgba);
		break;

	case PROP_FILL_STIPPLE:
		if (gdk) {
			g_value_set_object (value, gdk->fill_stipple);
		} else {
			g_value_set_object (value, NULL);
		}
		break;

	case PROP_OUTLINE_STIPPLE:
		if (gdk) {
			g_value_set_object (value, gdk->outline_stipple);
		} else {
			g_value_set_object (value, NULL);
		}
		break;

	case PROP_WIND:
		g_value_set_uint (value, priv->wind);
		break;

	case PROP_CAP_STYLE:
		g_value_set_enum (value, priv->cap);
		break;

	case PROP_JOIN_STYLE:
		g_value_set_enum (value, priv->join);
		break;

	case PROP_WIDTH_PIXELS:
		g_value_set_uint (value, priv->width);
		break;

	case PROP_WIDTH_UNITS:
		g_value_set_double (value, priv->width);
		break;

	case PROP_MITERLIMIT:
		g_value_set_double (value, priv->miterlimit);
		break;

	case PROP_DASH:
		g_value_set_pointer (value, &priv->dash);
		break;
		
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, param_id, pspec);
		break;
	}
}

static void
gnome_canvas_shape_realize (GnomeCanvasItem *item)
{
	GnomeCanvasShape *shape;

	shape = GNOME_CANVAS_SHAPE (item);

	if (parent_class->realize)
		(* parent_class->realize) (item);

	if (!item->canvas->aa) {
		gcbp_ensure_gdk (shape);

		g_assert(item->canvas->layout.bin_window != NULL);

		shape->priv->gdk->fill_gc = gdk_gc_new (item->canvas->layout.bin_window);
		shape->priv->gdk->outline_gc = gdk_gc_new (item->canvas->layout.bin_window);
	}
}

static void
gnome_canvas_shape_unrealize (GnomeCanvasItem *item)
{
	GnomeCanvasShape *shape;

	shape = GNOME_CANVAS_SHAPE (item);

	if (!item->canvas->aa) {
		g_assert (shape->priv->gdk != NULL);

		g_object_unref (shape->priv->gdk->fill_gc);
		shape->priv->gdk->fill_gc = NULL;

		g_object_unref (shape->priv->gdk->outline_gc);
		shape->priv->gdk->outline_gc = NULL;
	}

	if (parent_class->unrealize)
		(* parent_class->unrealize) (item);
}

static void
gnome_canvas_shape_render (GnomeCanvasItem *item,
			     GnomeCanvasBuf *buf)
{
	GnomeCanvasShape *shape;

	shape = GNOME_CANVAS_SHAPE (item);

	if (shape->priv->fill_svp != NULL)
		gnome_canvas_render_svp (buf,
			shape->priv->fill_svp,
			shape->priv->fill_rgba);

	if (shape->priv->outline_svp != NULL)
		gnome_canvas_render_svp (buf,
			shape->priv->outline_svp,
			shape->priv->outline_rgba);
}

static void
gnome_canvas_shape_draw (GnomeCanvasItem *item,
	GdkDrawable *drawable,
	int x,
	int y,
	int width,
	int height)
{
	static GdkPoint * dpoints = NULL;
	static gint num_dpoints = 0;

	GnomeCanvasShape * shape;
	GnomeCanvasShapePriv * priv;
	GnomeCanvasShapePrivGdk * gdk;
	gint i, pos, len;
	GSList * l;

	shape = GNOME_CANVAS_SHAPE (item);
	priv = shape->priv;

	/* We have to be realized, so gdk struct should exist! */

	gdk = shape->priv->gdk;
	g_assert (gdk != NULL);

	/* Build temporary point list, translated by -x, -y */

	if (dpoints == NULL) {
		dpoints = g_new (GdkPoint, gdk->num_points);
		num_dpoints = gdk->num_points;
	} else if (num_dpoints < gdk->num_points) {
		dpoints = g_renew (GdkPoint, dpoints, gdk->num_points);
		num_dpoints = gdk->num_points;
	}

	for (i = 0; i < gdk->num_points; i++) {
		dpoints[i].x = gdk->points[i].x - x;
		dpoints[i].y = gdk->points[i].y - y;
	}

	if (priv->fill_set) {

		/* Ensure, that we have mask and it is big enough */

		gcbp_ensure_mask (shape, width, height);

		/* Clear mask */

		gdk_draw_rectangle (gdk->ctx->mask,
			gdk->ctx->clear_gc,
			TRUE,
			0, 0,
			width, height);

		/* Draw subpaths, using XOR gc */

		pos = 0;

		for (l = gdk->closed_paths; l != NULL; l = l->next) {
			len = GPOINTER_TO_INT (l->data);

			gdk_draw_polygon (gdk->ctx->mask,
				gdk->ctx->xor_gc,
				TRUE,
				&dpoints[pos],
				len);

			pos += len;
		}

		/* Set bitmap to clipping mask */

		gdk_gc_set_clip_mask (gdk->fill_gc, gdk->ctx->mask);

		/* Stipple offset */

		if (gdk->fill_stipple) gnome_canvas_set_stipple_origin (item->canvas, gdk->fill_gc);

		/* Draw clipped rect to drawable */

		gdk_draw_rectangle (drawable,
			gdk->fill_gc,
			TRUE,
			0, 0,
			width, height);
	}

	if (priv->outline_set) {

		/* Stipple offset */

		if (gdk->outline_stipple) gnome_canvas_set_stipple_origin (item->canvas, gdk->outline_gc);
		/* Draw subpaths */

		pos = 0;

		for (l = gdk->closed_paths; l != NULL; l = l->next) {
			len = GPOINTER_TO_INT (l->data);

			gdk_draw_polygon (drawable,
					  gdk->outline_gc,
					  FALSE,
					  &dpoints[pos],
					  len);

			pos += len;
		}

		for (l = gdk->open_paths; l != NULL; l = l->next) {
			len = GPOINTER_TO_INT (l->data);

			gdk_draw_lines (drawable,
					gdk->outline_gc,
					&dpoints[pos],
					len);

			pos += len;
		}
	}
}

#define GDK_POINTS_BLOCK 32

static void
gnome_canvas_shape_ensure_gdk_points (GnomeCanvasShapePrivGdk *gdk, gint num)
{
	if (gdk->len_points < gdk->num_points + num) {
		gdk->len_points = MAX (gdk->len_points + GDK_POINTS_BLOCK, gdk->len_points + num);
		gdk->points = g_renew (GdkPoint, gdk->points, gdk->len_points);
	}
}

static void
gnome_canvas_shape_update_gdk (GnomeCanvasShape * shape, double * affine, ArtSVP * clip, int flags)
{
	GnomeCanvasShapePriv * priv;
	GnomeCanvasShapePrivGdk * gdk;
	int x1 = 0, y1 = 0, x2 = 0, y2 = 0;
	gboolean bbox_set = FALSE;
	gint width = 0; /* silence gcc */
	
	g_assert (!((GnomeCanvasItem *) shape)->canvas->aa);

	priv = shape->priv;
	gdk = priv->gdk;
	g_assert (gdk != NULL);

	if (priv->outline_set) {
		GdkLineStyle style;

		if (priv->width_pixels) {
			width = (int) floor (priv->width + 0.5);
			/* Never select 0 pixels unless the user asked for it,
			 * since that is the X11 zero width lines are non-specified */
			if (priv->width_pixels != 0 && width == 0) {
				width = 1;
			}
		} else {
			width = (int) floor ((priv->width * priv->scale) + 0.5);
			/* Never select 0 pixels unless the user asked for it,
			 * since that is the X11 zero width lines are non-speciifed */
			if (priv->width != 0 && width == 0) {
				width = 1;
			}
		}

		/* If dashed, set it in GdkGC */

		if ((shape->priv->dash.dash != NULL) && (shape->priv->dash.n_dash > 0)) {
			gint8 * dash_list;
			gint i;

			dash_list = g_new (gint8, shape->priv->dash.n_dash);

			for (i = 0; i < priv->dash.n_dash; i++) {
				dash_list[i] = (gint8) shape->priv->dash.dash[i];
			}

			gdk_gc_set_dashes (gdk->outline_gc,
				(gint) priv->dash.offset,
				dash_list,
				priv->dash.n_dash);

			g_free (dash_list);

			style = GDK_LINE_ON_OFF_DASH;
		} else {
			style = GDK_LINE_SOLID;
		}

		/* Set line width, cap, join */
		if(gdk->outline_gc) {
			
			gdk_gc_set_line_attributes (gdk->outline_gc,
						    width,
						    style,
						    priv->cap,
						    priv->join);
			
			/* Colors and stipples */
			set_gc_foreground (gdk->outline_gc, gdk->outline_pixel);
			set_stipple (gdk->outline_gc, &gdk->outline_stipple, gdk->outline_stipple, TRUE);
		}
	}

	if (priv->fill_set) {

		/* Colors and stipples */
		if(gdk->fill_gc) {
			set_gc_foreground (gdk->fill_gc, gdk->fill_pixel);
			set_stipple (gdk->fill_gc, &gdk->fill_stipple, gdk->fill_stipple, TRUE);
		}
	}

	/* Now the crazy part */

	/* Free existing GdkPoint array */

	if (gdk->points) {
		g_free (gdk->points);
		gdk->points = NULL;
		gdk->len_points = 0;
		gdk->num_points = 0;
	}

	/* Free subpath lists */

	while (gdk->closed_paths) gdk->closed_paths = g_slist_remove (gdk->closed_paths, gdk->closed_paths->data);
	while (gdk->open_paths) gdk->open_paths = g_slist_remove (gdk->open_paths, gdk->open_paths->data);

	/* Calcualte new GdkPoints array and subpath lists */

	if (priv->path) {
		GnomeCanvasPathDef * apath, * cpath, * opath;
		ArtBpath * abpath;
		GSList * clist, * olist;
		gint pos;

#if 0
		/* Allocate array */
		gdk->num_points = gnome_canvas_path_def_length (priv->path) * 1000 - 1;
		gdk->points = g_new (GdkPoint, gdk->num_points);
		g_print ("Points %d\n", gdk->num_points);
		/* Transform path */
#endif

		abpath = art_bpath_affine_transform (gnome_canvas_path_def_bpath (priv->path), affine);
		apath = gnome_canvas_path_def_new_from_bpath (abpath);

		/* Split path into open and closed parts */

		cpath = gnome_canvas_path_def_closed_parts (apath);
		opath = gnome_canvas_path_def_open_parts (apath);
		gnome_canvas_path_def_unref (apath);

		/* Split partial paths into subpaths */

		clist = gnome_canvas_path_def_split (cpath);
		gnome_canvas_path_def_unref (cpath);
		olist = gnome_canvas_path_def_split (opath);
		gnome_canvas_path_def_unref (opath);

		pos = 0;

		/* Fill GdkPoints and add subpaths to list: closed subpaths */

		while (clist) {
			GnomeCanvasPathDef * path;
			ArtBpath * bpath;
			ArtVpath * vpath;
			gint len, i;

			path = (GnomeCanvasPathDef *) clist->data;
			bpath = gnome_canvas_path_def_bpath (path);
			vpath = art_bez_path_to_vec (bpath, 0.1);
			for (len = 0; vpath[len].code != ART_END; len++) ;

			gnome_canvas_shape_ensure_gdk_points (gdk, len);
			for (i = 0; i < len; i++) {
				gdk->points[pos + i].x = (gint) floor (vpath[i].x + 0.5);
				gdk->points[pos + i].y = (gint) floor (vpath[i].y + 0.5);

				if (bbox_set) {
					x1 = MIN (x1, gdk->points[pos + i].x);
					x2 = MAX (x2, gdk->points[pos + i].x);
					y1 = MIN (y1, gdk->points[pos + i].y);
					y2 = MAX (y2, gdk->points[pos + i].y);
				} else {
					bbox_set = TRUE;
					x1 = x2 = gdk->points[pos + i].x;
					y1 = y2 = gdk->points[pos + i].y;
				}
			}
			gdk->num_points += len;

			art_free (vpath);

			if (len > 0) {
				pos += len;
				gdk->closed_paths = g_slist_append (gdk->closed_paths, GINT_TO_POINTER (len));
			}

			gnome_canvas_path_def_unref (path);
			clist = g_slist_remove (clist, clist->data);
		}

		/* Fill GdkPoints and add subpaths to list: open subpaths */

		while (olist) {
			GnomeCanvasPathDef * path;
			ArtBpath * bpath;
			ArtVpath * vpath;
			gint len, i;

			path = (GnomeCanvasPathDef *) olist->data;
			bpath = gnome_canvas_path_def_bpath (path);
			vpath = art_bez_path_to_vec (bpath, 0.1);
			for (len = 0; vpath[len].code != ART_END; len++) ;

			gnome_canvas_shape_ensure_gdk_points (gdk, len);
			for (i = 0; i < len; i++) {
				gdk->points[pos + i].x = (gint) floor (vpath[i].x + 0.5);
				gdk->points[pos + i].y = (gint) floor (vpath[i].y + 0.5);
				
				if (bbox_set) {
					x1 = MIN (x1, gdk->points[pos + i].x);
					x2 = MAX (x2, gdk->points[pos + i].x);
					y1 = MIN (y1, gdk->points[pos + i].y);
					y2 = MAX (y2, gdk->points[pos + i].y);
				} else {
					bbox_set = TRUE;
					x1 = x2 = gdk->points[pos + i].x;
					y1 = y2 = gdk->points[pos + i].y;
				}
			}
			gdk->num_points += len;

			art_free (vpath);

			if (len > 0) {
				pos += len;
				gdk->open_paths = g_slist_append (gdk->open_paths, GINT_TO_POINTER (len));
			}

			gnome_canvas_path_def_unref (path);
			olist = g_slist_remove (olist, olist->data);
		}

	}

	if (bbox_set) {
		if (priv->outline_set) {
			int stroke_border = (priv->join == GDK_JOIN_MITER)
				? ceil (10.43*width/2) /* 10.43 is the miter limit for X11 */
				: ceil (width/2);
			x1 -= stroke_border;
			x2 += stroke_border;
			y1 -= stroke_border;
			y2 += stroke_border;
		}
		
		gnome_canvas_update_bbox (GNOME_CANVAS_ITEM (shape),
					  x1, y1,
					  x2 + 1, y2 + 1);
	}
	
}

static void
gnome_canvas_shape_update (GnomeCanvasItem *item, double *affine, ArtSVP *clip_path, int flags)
{
	GnomeCanvasShape * shape;
	GnomeCanvasShapePriv * priv;
	ArtSVP * svp;

	shape = GNOME_CANVAS_SHAPE (item);

	priv = shape->priv;

	/* Common part */
	if (parent_class->update) {
		(* parent_class->update) (item, affine, clip_path, flags);
	}

	/* Outline width */
	shape->priv->scale = art_affine_expansion (affine);

	/* Reset bbox */
	if (item->canvas->aa) {
		gnome_canvas_item_reset_bounds (item);
	}
	
	/* Clipped fill SVP */

	if ((priv->fill_set) && (priv->path) && (gnome_canvas_path_def_any_closed (priv->path))) {
		GnomeCanvasPathDef * cpath;
		ArtSvpWriter *swr;
		ArtVpath *vpath;
		ArtBpath *abp;
		ArtSVP *svp2;

		/* Get closed part of path */

		cpath = gnome_canvas_path_def_closed_parts (shape->priv->path);
		abp = art_bpath_affine_transform (gnome_canvas_path_def_bpath (cpath), affine);
		gnome_canvas_path_def_unref (cpath);

		/* Render, until SVP */

		vpath = art_bez_path_to_vec (abp, 0.1);
		art_free (abp);

		svp = art_svp_from_vpath (vpath);
		art_free (vpath);

		swr = art_svp_writer_rewind_new (shape->priv->wind);
		art_svp_intersector (svp, swr);

		svp2 = art_svp_writer_rewind_reap (swr);
		art_svp_free (svp);

		if (item->canvas->aa) {
			/* Update clipped path */
			gnome_canvas_item_update_svp_clip (item,
							   &shape->priv->fill_svp,
							   svp2,
							   clip_path);
		} else {
			if (priv->fill_svp) {
				art_svp_free (priv->fill_svp);
				priv->fill_svp = NULL;
			}
			/* No clipping */
			shape->priv->fill_svp = svp2;
		}
	}

	if (priv->outline_set && priv->path && !gnome_canvas_path_def_is_empty (priv->path)) {
		gdouble width;
		ArtBpath * abp;
		ArtVpath * vpath;

		/* Set linewidth */

		if (priv->width_pixels) {
			width = priv->width;
		} else {
			width = priv->width * priv->scale;
		}
		
		if (width < 0.5) width = 0.5;
		
		/* Render full path until vpath */

		abp = art_bpath_affine_transform (gnome_canvas_path_def_bpath (priv->path), affine);

		vpath = art_bez_path_to_vec (abp, 0.1);
		art_free (abp);

		/* If dashed, apply dash */

		if (priv->dash.dash != NULL)
		{
			ArtVpath *old = vpath;
			
			vpath = art_vpath_dash (old, &priv->dash);
			art_free (old);
		}
		
		/* Stroke vpath to SVP */

		svp = art_svp_vpath_stroke (vpath,
					    gnome_canvas_join_gdk_to_art (priv->join),
					    gnome_canvas_cap_gdk_to_art (priv->cap),
					    width,
					    priv->miterlimit,
					    0.25);
		art_free (vpath);

		if (item->canvas->aa) {
			/* Update clipped */
			gnome_canvas_item_update_svp_clip (item, &priv->outline_svp, svp, clip_path);
		} else {
			if (priv->outline_svp) {
				art_svp_free (priv->outline_svp);
				priv->outline_svp = NULL;
			}
			/* No clipping (yet) */
			shape->priv->outline_svp = svp;
		}
	}

	/* Gdk requires additional handling */
	
	if (!item->canvas->aa) {
		gnome_canvas_shape_update_gdk (shape, affine, clip_path, flags);
	}
}

static double
gnome_canvas_shape_point (GnomeCanvasItem *item, double x, double y,
			    int cx, int cy, GnomeCanvasItem **actual_item)
{
	GnomeCanvasShape *shape;
	double dist;
	int wind;

#if 0
	/* fixme: This is just for debugging, canvas should ensure that */
	/* fixme: IF YOU ARE SURE THAT IT IS CORRECT BEHAVIOUR, you can remove warning */
	/* fixme: and make it to return silently */
	g_return_val_if_fail (!item->canvas->need_update, 1e18);
#endif

	shape = GNOME_CANVAS_SHAPE (item);

	/* todo: update? */
	if (shape->priv->fill_set && shape->priv->fill_svp) {
		wind = art_svp_point_wind (shape->priv->fill_svp, cx, cy);
		if ((shape->priv->wind == ART_WIND_RULE_NONZERO) && (wind != 0)) {
			*actual_item = item;
			return 0.0;
		}
		if ((shape->priv->wind == ART_WIND_RULE_ODDEVEN) && ((wind & 0x1) != 0)) {
			*actual_item = item;
			return 0.0;
		}
	}

	if (shape->priv->outline_set && shape->priv->outline_svp) {
		wind = art_svp_point_wind (shape->priv->outline_svp, cx, cy);
		if (wind) {
			*actual_item = item;
			return 0.0;
		}
	}

	if (shape->priv->outline_set && shape->priv->outline_svp) {
		dist = art_svp_point_dist (shape->priv->outline_svp, cx, cy);
	} else if (shape->priv->fill_set && shape->priv->outline_svp) {
		dist = art_svp_point_dist (shape->priv->fill_svp, cx, cy);
	} else {
		return 1e12;
	}

	*actual_item = item;

	return dist;
}

/* Helpers */

/* Get 32bit rgba color from GdkColor */

static guint32
get_rgba_from_color (GdkColor * color)
{
	return ((color->red & 0xff00) << 16) | ((color->green & 0xff00) << 8) | (color->blue & 0xff00) | 0xff;
}

/* Get Gdk pixel value from 32bit rgba color */

static gulong
get_pixel_from_rgba (GnomeCanvasItem *item, guint32 rgba_color)
{
	return gnome_canvas_get_color_pixel (item->canvas, rgba_color);
}

/* Convenience function to set a GC's foreground color to the specified pixel value */

static void
set_gc_foreground (GdkGC *gc, gulong pixel)
{
	GdkColor c;

	g_assert (gc != NULL);

	c.pixel = pixel;

	gdk_gc_set_foreground (gc, &c);
}

/* Sets the stipple pattern for the specified gc */

static void
set_stipple (GdkGC *gc, GdkBitmap **internal_stipple, GdkBitmap *stipple, int reconfigure)
{
	if (*internal_stipple && !reconfigure)
		g_object_unref (*internal_stipple);

	*internal_stipple = stipple;
	if (stipple && !reconfigure)
		g_object_ref (stipple);

	if (gc) {
		if (stipple) {
			gdk_gc_set_stipple (gc, stipple);
			gdk_gc_set_fill (gc, GDK_STIPPLED);
		} else
			gdk_gc_set_fill (gc, GDK_SOLID);
	}
}

/* Creates private Gdk struct, if not present */
/* We cannot do it during ::init, as we have to know canvas */

static void
gcbp_ensure_gdk (GnomeCanvasShape * shape)
{
	g_assert (!((GnomeCanvasItem *) shape)->canvas->aa);

	if (!shape->priv->gdk) {
		GnomeCanvasShapePrivGdk * gdk;

		gdk = g_new (GnomeCanvasShapePrivGdk, 1);

		gdk->fill_pixel = get_pixel_from_rgba ((GnomeCanvasItem *) shape, shape->priv->fill_rgba);
		gdk->outline_pixel = get_pixel_from_rgba ((GnomeCanvasItem *) shape, shape->priv->outline_rgba);

		gdk->fill_stipple = NULL;
		gdk->outline_stipple = NULL;

		gdk->fill_gc = NULL;
		gdk->outline_gc = NULL;

		gdk->len_points = 0;
		gdk->num_points = 0;
		gdk->points = NULL;
		gdk->closed_paths = NULL;
		gdk->open_paths = NULL;

		gdk->ctx = NULL;

		shape->priv->gdk = gdk;
	}
}

/* Destroy private Gdk struct */
/* It is here, to make ::destroy implementation shorter :) */

static void
gcbp_destroy_gdk (GnomeCanvasShape * shape)
{
	GnomeCanvasShapePrivGdk * gdk;

	g_assert (!((GnomeCanvasItem *)shape)->canvas->aa);

	gdk = shape->priv->gdk;

	if (gdk) {
		g_assert (!gdk->fill_gc);
		g_assert (!gdk->outline_gc);

		if (gdk->fill_stipple)
			g_object_unref (gdk->fill_stipple);

		if (gdk->outline_stipple)
			g_object_unref (gdk->outline_stipple);

		if (gdk->points)
			g_free (gdk->points);

		while (gdk->closed_paths)
			gdk->closed_paths = g_slist_remove (gdk->closed_paths, gdk->closed_paths->data);
		while (gdk->open_paths)
			gdk->open_paths = g_slist_remove (gdk->open_paths, gdk->open_paths->data);

		if (gdk->ctx)
			gcbp_draw_ctx_unref (gdk->ctx);

		g_free (gdk);

		shape->priv->gdk = NULL;
	}
}

/*
 * Ensure, that per-canvas Ctx struct is present and bitmaps are
 * big enough, to mask full redraw area. Ctx is refcounted and
 * defined as "BpathDrawCtx" data member on parent canvas
 */

static void
gcbp_ensure_mask (GnomeCanvasShape * shape, gint width, gint height)
{
	GnomeCanvasShapePrivGdk * gdk;
	GCBPDrawCtx * ctx;

	gdk = shape->priv->gdk;
	g_assert (gdk != NULL);
	ctx = gdk->ctx;

	if (!ctx) {
		/* Ctx is not yet defined for us */

		GnomeCanvas * canvas;

		canvas = GNOME_CANVAS_ITEM (shape)->canvas;

		ctx = g_object_get_data (G_OBJECT (canvas), "BpathDrawCtx");

		if (!ctx) {
			/* Ctx is not defined for parent canvas yet */

			ctx = g_new (GCBPDrawCtx, 1);

			ctx->refcount = 1;
			ctx->canvas = canvas;
			ctx->width = 0;
			ctx->height = 0;

			ctx->mask = NULL;
			ctx->clip = NULL;

			ctx->clear_gc = NULL;
			ctx->xor_gc = NULL;

			g_object_set_data (G_OBJECT (canvas), "BpathDrawCtx", ctx);

		} else {
			ctx->refcount++;
		}

		gdk->ctx = ctx;

	}

	/* Now we are sure, that ctx is present and properly refcounted */

	if ((width > ctx->width) || (height > ctx->height)) {
		/* Ctx is too small */

		GdkWindow * window;

		window = ((GtkWidget *) (((GnomeCanvasItem *) shape)->canvas))->window;

		if (ctx->clear_gc) g_object_unref (ctx->clear_gc);
		if (ctx->xor_gc) g_object_unref (ctx->xor_gc);
		if (ctx->mask) g_object_unref (ctx->mask);
		if (ctx->clip) g_object_unref (ctx->clip);

		ctx->mask = gdk_pixmap_new (window, width, height, 1);
		ctx->clip = NULL;

		ctx->clear_gc = gdk_gc_new (ctx->mask);
		gdk_gc_set_function (ctx->clear_gc, GDK_CLEAR);

		ctx->xor_gc = gdk_gc_new (ctx->mask);
		gdk_gc_set_function (ctx->xor_gc, GDK_INVERT);
	}
}

/* It is cleaner to have it here, not in parent function */

static void
gcbp_draw_ctx_unref (GCBPDrawCtx * ctx)
{
	if (--ctx->refcount < 1) {
		if (ctx->clear_gc)
			g_object_unref (ctx->clear_gc);
		if (ctx->xor_gc)
			g_object_unref (ctx->xor_gc);

		if (ctx->mask)
			g_object_unref (ctx->mask);
		if (ctx->clip)
			g_object_unref (ctx->clip);
		
		g_object_set_data (G_OBJECT (ctx->canvas), "BpathDrawCtx", NULL);
		g_free (ctx);
	}
}

static void
gnome_canvas_shape_bounds (GnomeCanvasItem *item, double *x1, double *y1, double *x2, double *y2)
{
	GnomeCanvasShape * shape;
	GnomeCanvasShapePriv * priv;
	ArtDRect bbox;
	ArtSVP * svp;

	shape = GNOME_CANVAS_SHAPE (item);

	priv = shape->priv;

	bbox.x0 = *x1;
	bbox.y0 = *y1;
	bbox.x1 = *x2;
	bbox.y1 = *y2;

	if (priv->outline_set && priv->path && !gnome_canvas_path_def_is_empty (priv->path)) {
		gdouble width;
		ArtVpath * vpath;

		/* Set linewidth */

		if (priv->width_pixels) {
			width = priv->width;
		} else {
			width = priv->width * priv->scale;
		}
		
		if (width < 0.5) width = 0.5;
		
		/* Render full path until vpath */

		vpath = art_bez_path_to_vec (gnome_canvas_path_def_bpath (priv->path), 0.1);

		/* If dashed, apply dash */

		if (priv->dash.dash != NULL)
		{
			ArtVpath *old = vpath;
			
			vpath = art_vpath_dash (old, &priv->dash);
			art_free (old);
		}
		
		/* Stroke vpath to SVP */

		svp = art_svp_vpath_stroke (vpath,
					    gnome_canvas_join_gdk_to_art (priv->join),
					    gnome_canvas_cap_gdk_to_art (priv->cap),
					    width,
					    priv->miterlimit,
					    0.25);
		art_free (vpath);
		art_drect_svp (&bbox, svp);
		art_svp_free (svp);
	} else if ((priv->fill_set) && (priv->path) && (gnome_canvas_path_def_any_closed (priv->path))) {
		GnomeCanvasPathDef *cpath;
		ArtSvpWriter *swr;
		ArtVpath *vpath;
		ArtSVP *svp2;

		/* Get closed part of path */
		cpath = gnome_canvas_path_def_closed_parts (shape->priv->path);
		/* Render, until SVP */
		vpath = art_bez_path_to_vec (gnome_canvas_path_def_bpath (cpath), 0.1);
		gnome_canvas_path_def_unref (cpath);

		svp = art_svp_from_vpath (vpath);
		art_free (vpath);
		
		swr = art_svp_writer_rewind_new (shape->priv->wind);
		art_svp_intersector (svp, swr);
		
		svp2 = art_svp_writer_rewind_reap (swr);
		art_svp_free (svp);
  
		art_drect_svp (&bbox, svp2);
		art_svp_free (svp2);
	}

	*x1 = bbox.x0;
	*y1 = bbox.y0;
	*x2 = bbox.x1;
	*y2 = bbox.y1;
}
