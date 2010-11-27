/* GNOME libraries - GdkPixbuf item for the GNOME canvas
 *
 * Copyright (C) 1999 The Free Software Foundation
 *
 * Author: Federico Mena-Quintero <federico@gimp.org>
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

#include <config.h>
#include <math.h>
#include <libgnomecanvas/gnome-canvas.h>
#include <libgnomecanvas/gnome-canvas-util.h>
#include <gdk-pixbuf/gdk-pixbuf.h>
#include <libart_lgpl/art_rgb_affine.h>
#include <libart_lgpl/art_rgb_rgba_affine.h>
#include "gnome-canvas-pixbuf.h"

/* Private part of the GnomeCanvasPixbuf structure */
typedef struct {
	/* Our gdk-pixbuf */
	GdkPixbuf *pixbuf;

	/* Width value */
	double width;

	/* Height value */
	double height;

	/* X translation */
	double x;

	/* Y translation */
	double y;

	/* Whether dimensions are set and whether they are in pixels or units */
	guint width_set : 1;
	guint width_in_pixels : 1;
	guint height_set : 1;
	guint height_in_pixels : 1;
	guint x_in_pixels : 1;
	guint y_in_pixels : 1;
	
	/* Whether the pixbuf has changed */
	guint need_pixbuf_update : 1;

	/* Whether the transformation or size have changed */
	guint need_xform_update : 1;

	/* Anchor */
	GtkAnchorType anchor;
} PixbufPrivate;

/* Object argument IDs */
enum {
	PROP_0,
	PROP_PIXBUF,
	PROP_WIDTH,
	PROP_WIDTH_SET,
	PROP_WIDTH_IN_PIXELS,
	PROP_HEIGHT,
	PROP_HEIGHT_SET,
	PROP_HEIGHT_IN_PIXELS,
	PROP_X,
	PROP_X_IN_PIXELS,
	PROP_Y,
	PROP_Y_IN_PIXELS,
	PROP_ANCHOR
};

static void gnome_canvas_pixbuf_class_init (GnomeCanvasPixbufClass *class);
static void gnome_canvas_pixbuf_init (GnomeCanvasPixbuf *cpb);
static void gnome_canvas_pixbuf_destroy (GtkObject *object);
static void gnome_canvas_pixbuf_set_property (GObject *object,
					      guint param_id,
					      const GValue *value,
					      GParamSpec *pspec);
static void gnome_canvas_pixbuf_get_property (GObject *object,
					      guint param_id,
					      GValue *value,
					      GParamSpec *pspec);

static void gnome_canvas_pixbuf_update (GnomeCanvasItem *item, double *affine,
					ArtSVP *clip_path, int flags);
static void gnome_canvas_pixbuf_draw (GnomeCanvasItem *item, GdkDrawable *drawable,
				      int x, int y, int width, int height);
static void gnome_canvas_pixbuf_render (GnomeCanvasItem *item, GnomeCanvasBuf *buf);
static double gnome_canvas_pixbuf_point (GnomeCanvasItem *item, double x, double y, int cx, int cy,
					 GnomeCanvasItem **actual_item);
static void gnome_canvas_pixbuf_bounds (GnomeCanvasItem *item,
					double *x1, double *y1, double *x2, double *y2);

static GnomeCanvasItemClass *parent_class;



/**
 * gnome_canvas_pixbuf_get_type:
 * @void:
 *
 * Registers the #GnomeCanvasPixbuf class if necessary, and returns the type ID
 * associated to it.
 *
 * Return value: The type ID of the #GnomeCanvasPixbuf class.
 **/
GType
gnome_canvas_pixbuf_get_type (void)
{
	static GType pixbuf_type;

	if (!pixbuf_type) {
		const GTypeInfo object_info = {
			sizeof (GnomeCanvasPixbufClass),
			(GBaseInitFunc) NULL,
			(GBaseFinalizeFunc) NULL,
			(GClassInitFunc) gnome_canvas_pixbuf_class_init,
			(GClassFinalizeFunc) NULL,
			NULL,			/* class_data */
			sizeof (GnomeCanvasPixbuf),
			0,			/* n_preallocs */
			(GInstanceInitFunc) gnome_canvas_pixbuf_init,
			NULL			/* value_table */
		};

		pixbuf_type = g_type_register_static (GNOME_TYPE_CANVAS_ITEM, "GnomeCanvasPixbuf",
						     &object_info, 0);
	}

	return pixbuf_type;
}

/* Class initialization function for the pixbuf canvas item */
static void
gnome_canvas_pixbuf_class_init (GnomeCanvasPixbufClass *class)
{
        GObjectClass *gobject_class;
	GtkObjectClass *object_class;
	GnomeCanvasItemClass *item_class;

        gobject_class = (GObjectClass *) class;
	object_class = (GtkObjectClass *) class;
	item_class = (GnomeCanvasItemClass *) class;

	parent_class = g_type_class_peek_parent (class);

	gobject_class->set_property = gnome_canvas_pixbuf_set_property;
	gobject_class->get_property = gnome_canvas_pixbuf_get_property;

        g_object_class_install_property
                (gobject_class,
                 PROP_PIXBUF,
                 g_param_spec_object ("pixbuf", NULL, NULL,
                                      GDK_TYPE_PIXBUF,
                                      (G_PARAM_READABLE | G_PARAM_WRITABLE)));
        g_object_class_install_property
                (gobject_class,
                 PROP_WIDTH,
                 g_param_spec_double ("width", NULL, NULL,
				      -G_MAXDOUBLE, G_MAXDOUBLE, 0,
				      (G_PARAM_READABLE | G_PARAM_WRITABLE)));
        g_object_class_install_property
                (gobject_class,
                 PROP_WIDTH_SET,
                 g_param_spec_boolean ("width_set", NULL, NULL,
				       FALSE,
				       (G_PARAM_READABLE | G_PARAM_WRITABLE)));
        g_object_class_install_property
                (gobject_class,
                 PROP_WIDTH_IN_PIXELS,
                 g_param_spec_boolean ("width_in_pixels", NULL, NULL,
				       FALSE,
				       (G_PARAM_READABLE | G_PARAM_WRITABLE)));
        g_object_class_install_property
                (gobject_class,
                 PROP_HEIGHT,
                 g_param_spec_double ("height", NULL, NULL,
				      -G_MAXDOUBLE, G_MAXDOUBLE, 0,
				      (G_PARAM_READABLE | G_PARAM_WRITABLE)));
        g_object_class_install_property
                (gobject_class,
                 PROP_HEIGHT_SET,
                 g_param_spec_boolean ("height_set", NULL, NULL,
				       FALSE,
				       (G_PARAM_READABLE | G_PARAM_WRITABLE)));
        g_object_class_install_property
                (gobject_class,
                 PROP_HEIGHT_IN_PIXELS,
                 g_param_spec_boolean ("height_in_pixels", NULL, NULL,
				       FALSE,
				       (G_PARAM_READABLE | G_PARAM_WRITABLE)));
        g_object_class_install_property
                (gobject_class,
                 PROP_X,
                 g_param_spec_double ("x", NULL, NULL,
				      -G_MAXDOUBLE, G_MAXDOUBLE, 0,
                                    (G_PARAM_READABLE | G_PARAM_WRITABLE)));
        g_object_class_install_property
                (gobject_class,
                 PROP_X_IN_PIXELS,
                 g_param_spec_boolean ("x_in_pixels", NULL, NULL,
				       FALSE,
				       (G_PARAM_READABLE | G_PARAM_WRITABLE)));
        g_object_class_install_property
                (gobject_class,
                 PROP_Y,
                 g_param_spec_double ("y", NULL, NULL,
				      -G_MAXDOUBLE, G_MAXDOUBLE, 0,
				      (G_PARAM_READABLE | G_PARAM_WRITABLE)));
        g_object_class_install_property
                (gobject_class,
                 PROP_Y_IN_PIXELS,
                 g_param_spec_boolean ("y_in_pixels", NULL, NULL,
				       FALSE,
				       (G_PARAM_READABLE | G_PARAM_WRITABLE)));
        g_object_class_install_property
                (gobject_class,
                 PROP_ANCHOR,
                 g_param_spec_enum ("anchor", NULL, NULL,
                                    GTK_TYPE_ANCHOR_TYPE,
                                    GTK_ANCHOR_NW,
                                    (G_PARAM_READABLE | G_PARAM_WRITABLE)));

	object_class->destroy = gnome_canvas_pixbuf_destroy;

	item_class->update = gnome_canvas_pixbuf_update;
	item_class->draw = gnome_canvas_pixbuf_draw;
	item_class->render = gnome_canvas_pixbuf_render;
	item_class->point = gnome_canvas_pixbuf_point;
	item_class->bounds = gnome_canvas_pixbuf_bounds;
}

/* Object initialization function for the pixbuf canvas item */
static void
gnome_canvas_pixbuf_init (GnomeCanvasPixbuf *gcp)
{
	PixbufPrivate *priv;

	priv = g_new0 (PixbufPrivate, 1);
	gcp->priv = priv;

	priv->width = 0.0;
	priv->height = 0.0;
	priv->x = 0.0;
	priv->y = 0.0;
	priv->anchor = GTK_ANCHOR_NW;
}

/* Destroy handler for the pixbuf canvas item */
static void
gnome_canvas_pixbuf_destroy (GtkObject *object)
{
	GnomeCanvasItem *item;
	GnomeCanvasPixbuf *gcp;
	PixbufPrivate *priv;

	g_return_if_fail (object != NULL);
	g_return_if_fail (GNOME_IS_CANVAS_PIXBUF (object));

	item = GNOME_CANVAS_ITEM (object);
	gcp = (GNOME_CANVAS_PIXBUF (object));
	priv = gcp->priv;

	/* remember, destroy can be run multiple times! */

	if (priv) {
	    gnome_canvas_request_redraw (item->canvas, item->x1, item->y1, item->x2, item->y2);

	    if (priv->pixbuf)
		g_object_unref (priv->pixbuf);

	    g_free (priv);
	    gcp->priv = NULL;
	}

	if (GTK_OBJECT_CLASS (parent_class)->destroy)
		(* GTK_OBJECT_CLASS (parent_class)->destroy) (object);
}



/* Set_property handler for the pixbuf canvas item */
static void
gnome_canvas_pixbuf_set_property (GObject            *object,
				  guint               param_id,
				  const GValue       *value,
				  GParamSpec         *pspec)
{
	GnomeCanvasItem *item;
	GnomeCanvasPixbuf *gcp;
	PixbufPrivate *priv;
	GdkPixbuf *pixbuf;
	double val;

	g_return_if_fail (object != NULL);
	g_return_if_fail (GNOME_IS_CANVAS_PIXBUF (object));

	item = GNOME_CANVAS_ITEM (object);
	gcp = GNOME_CANVAS_PIXBUF (object);
	priv = gcp->priv;

	switch (param_id) {
	case PROP_PIXBUF:
		if (g_value_get_object (value))
			pixbuf = GDK_PIXBUF (g_value_get_object (value));
		else
			pixbuf = NULL;
		if (pixbuf != priv->pixbuf) {
			if (pixbuf) {
				g_return_if_fail
				    (gdk_pixbuf_get_colorspace (pixbuf) == GDK_COLORSPACE_RGB);
				g_return_if_fail
				    (gdk_pixbuf_get_n_channels (pixbuf) == 3
				     || gdk_pixbuf_get_n_channels (pixbuf) == 4);
				g_return_if_fail
				    (gdk_pixbuf_get_bits_per_sample (pixbuf) == 8);

				g_object_ref (pixbuf);
			}

			if (priv->pixbuf)
				g_object_unref (priv->pixbuf);

			priv->pixbuf = pixbuf;
		}

		priv->need_pixbuf_update = TRUE;
		gnome_canvas_item_request_update (item);
		break;

	case PROP_WIDTH:
		val = g_value_get_double (value);
		g_return_if_fail (val >= 0.0);
		priv->width = val;
		priv->need_xform_update = TRUE;
		gnome_canvas_item_request_update (item);
		break;

	case PROP_WIDTH_SET:
		priv->width_set = g_value_get_boolean (value);
		priv->need_xform_update = TRUE;
		gnome_canvas_item_request_update (item);
		break;

	case PROP_WIDTH_IN_PIXELS:
		priv->width_in_pixels = g_value_get_boolean (value);
		priv->need_xform_update = TRUE;
		gnome_canvas_item_request_update (item);
		break;

	case PROP_HEIGHT:
		val = g_value_get_double (value);
		g_return_if_fail (val >= 0.0);
		priv->height = val;
		priv->need_xform_update = TRUE;
		gnome_canvas_item_request_update (item);
		break;

	case PROP_HEIGHT_SET:
		priv->height_set = g_value_get_boolean (value);
		priv->need_xform_update = TRUE;
		gnome_canvas_item_request_update (item);
		break;

	case PROP_HEIGHT_IN_PIXELS:
		priv->height_in_pixels = g_value_get_boolean (value);
		priv->need_xform_update = TRUE;
		gnome_canvas_item_request_update (item);
		break;

	case PROP_X:
		priv->x = g_value_get_double (value);
		priv->need_xform_update = TRUE;
		gnome_canvas_item_request_update (item);
		break;

	case PROP_X_IN_PIXELS:
		priv->x_in_pixels = g_value_get_boolean (value);
		priv->need_xform_update = TRUE;
		gnome_canvas_item_request_update (item);
		break;

	case PROP_Y:
		priv->y = g_value_get_double (value);
		priv->need_xform_update = TRUE;
		gnome_canvas_item_request_update (item);
		break;

	case PROP_Y_IN_PIXELS:
		priv->y_in_pixels = g_value_get_boolean (value);
		priv->need_xform_update = TRUE;
		gnome_canvas_item_request_update (item);
		break;

	case PROP_ANCHOR:
		priv->anchor = g_value_get_enum (value);
		priv->need_xform_update = TRUE;
		gnome_canvas_item_request_update (item);
		break;

	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, param_id, pspec);
		break;
	}
}

/* Get_property handler for the pixbuf canvasi item */
static void
gnome_canvas_pixbuf_get_property (GObject            *object,
				  guint               param_id,
				  GValue             *value,
				  GParamSpec         *pspec)
{
	GnomeCanvasPixbuf *gcp;
	PixbufPrivate *priv;

	g_return_if_fail (object != NULL);
	g_return_if_fail (GNOME_IS_CANVAS_PIXBUF (object));

	gcp = GNOME_CANVAS_PIXBUF (object);
	priv = gcp->priv;

	switch (param_id) {
	case PROP_PIXBUF:
		g_value_set_object (value, G_OBJECT (priv->pixbuf));
		break;

	case PROP_WIDTH:
		g_value_set_double (value, priv->width);
		break;

	case PROP_WIDTH_SET:
		g_value_set_boolean (value, priv->width_set);
		break;

	case PROP_WIDTH_IN_PIXELS:
		g_value_set_boolean (value, priv->width_in_pixels);
		break;

	case PROP_HEIGHT:
		g_value_set_double (value, priv->height);
		break;

	case PROP_HEIGHT_SET:
		g_value_set_boolean (value, priv->height_set);
		break;

	case PROP_HEIGHT_IN_PIXELS:
		g_value_set_boolean (value, priv->height_in_pixels);
		break;

	case PROP_X:
		g_value_set_double (value, priv->x);
		break;

	case PROP_X_IN_PIXELS:
		g_value_set_boolean (value, priv->x_in_pixels);
		break;

	case PROP_Y:
		g_value_set_double (value, priv->y);
		break;

	case PROP_Y_IN_PIXELS:
		g_value_set_boolean (value, priv->y_in_pixels);
		break;

	case PROP_ANCHOR:
		g_value_set_enum (value, priv->anchor);
		break;

	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, param_id, pspec);
		break;
	}
}



/* Bounds and utilities */

/* Computes the amount by which the unit horizontal and vertical vectors will be
 * scaled by an affine transformation.
 */
static void
compute_xform_scaling (double *affine, ArtPoint *i_c, ArtPoint *j_c)
{
	ArtPoint orig, orig_c;
	ArtPoint i, j;

	/* Origin */

	orig.x = 0.0;
	orig.y = 0.0;
	art_affine_point (&orig_c, &orig, affine);

	/* Horizontal and vertical vectors */

	i.x = 1.0;
	i.y = 0.0;
	art_affine_point (i_c, &i, affine);
	i_c->x -= orig_c.x;
	i_c->y -= orig_c.y;

	j.x = 0.0;
	j.y = 1.0;
	art_affine_point (j_c, &j, affine);
	j_c->x -= orig_c.x;
	j_c->y -= orig_c.y;
}

/* computes the addtional resolution dependent affine needed to
 * fit the image within its viewport defined by x,y,width and height
 * args
 */
static void
compute_viewport_affine (GnomeCanvasPixbuf *gcp, double *viewport_affine, double *i2c)
{
	PixbufPrivate *priv;
	ArtPoint i_c, j_c;
	double i_len, j_len;
	double si_len, sj_len;
	double ti_len, tj_len;
	double scale[6], translate[6];
	double w, h;
	double x, y;

	priv = gcp->priv;

	/* Compute scaling vectors and required width/height */

	compute_xform_scaling (i2c, &i_c, &j_c);

	i_len = sqrt (i_c.x * i_c.x + i_c.y * i_c.y);
	j_len = sqrt (j_c.x * j_c.x + j_c.y * j_c.y);

	if (priv->width_set)
		w = priv->width;
	else
		w = gdk_pixbuf_get_width (priv->pixbuf);

	if (priv->height_set)
		h = priv->height;
	else
		h = gdk_pixbuf_get_height (priv->pixbuf);

	x = priv->x;
	y = priv->y;

	/* Convert i_len and j_len into scaling factors */

	if (priv->width_in_pixels) {
		if (i_len > GNOME_CANVAS_EPSILON)
			si_len = 1.0 / i_len;
		else
			si_len = 0.0;
	} else
		si_len = 1.0;

	si_len *= w / gdk_pixbuf_get_width (priv->pixbuf);

	if (priv->height_in_pixels) {
		if (j_len > GNOME_CANVAS_EPSILON)
			sj_len = 1.0 / j_len;
		else
			sj_len = 0.0;
	} else
		sj_len = 1.0;

	sj_len *= h / gdk_pixbuf_get_height (priv->pixbuf);

	/* Calculate translation offsets */

	if (priv->x_in_pixels) {
		if (i_len > GNOME_CANVAS_EPSILON)
			ti_len = 1.0 / i_len;
		else
			ti_len = 0.0;
	} else
		ti_len = 1.0;

	switch (priv->anchor) {
	case GTK_ANCHOR_NW:
	case GTK_ANCHOR_W:
	case GTK_ANCHOR_SW:
		ti_len *= x;
		break;

	case GTK_ANCHOR_N:
	case GTK_ANCHOR_CENTER:
	case GTK_ANCHOR_S:
		ti_len *= x - w * si_len / 2;
		break;

	case GTK_ANCHOR_NE:
	case GTK_ANCHOR_E:
	case GTK_ANCHOR_SE:
		ti_len *= x - w * si_len;
		break;

        default:
                break;
	}

	if (priv->y_in_pixels) {
		if (j_len > GNOME_CANVAS_EPSILON)
			tj_len = 1.0 / j_len;
		else
			tj_len = 0.0;
	} else
		tj_len = 1.0;

	switch (priv->anchor) {
	case GTK_ANCHOR_NW:
	case GTK_ANCHOR_N:
	case GTK_ANCHOR_NE:
		tj_len *= y;
		break;
		
	case GTK_ANCHOR_W:
	case GTK_ANCHOR_CENTER:
	case GTK_ANCHOR_E:
		tj_len *= y - h * sj_len / 2;
		break;

	case GTK_ANCHOR_SW:
	case GTK_ANCHOR_S:
	case GTK_ANCHOR_SE:
		tj_len *= y - h * sj_len;
		break;

        default:
                break;
	}

	/* Compute the final affine */

	art_affine_scale (scale, si_len, sj_len);
	art_affine_translate (translate, ti_len, tj_len);
  	art_affine_multiply (viewport_affine, scale, translate); 
}

/* Computes the affine transformation with which the pixbuf needs to be
 * transformed to render it on the canvas.  This is not the same as the
 * item_to_canvas transformation because we may need to scale the pixbuf
 * by some other amount.
 */
static void
compute_render_affine (GnomeCanvasPixbuf *gcp, double *ra, double *i2c)
{
	double va[6];

	compute_viewport_affine (gcp, va, i2c);
#ifdef GNOME_CANVAS_PIXBUF_VERBOSE
	g_print ("va %g %g %g %g %g %g\n", va[0], va[1], va[2], va[3], va[4], va[5]);
#endif
	art_affine_multiply (ra, va, i2c);
#ifdef GNOME_CANVAS_PIXBUF_VERBOSE
	g_print ("ra %g %g %g %g %g %g\n", ra[0], ra[1], ra[2], ra[3], ra[4], ra[5]);
#endif
}

/* Recomputes the bounding box of a pixbuf canvas item.  The horizontal and
 * vertical dimensions may be specified in units or pixels, separately, so we
 * have to compute the components individually for each dimension.
 */
static void
recompute_bounding_box (GnomeCanvasPixbuf *gcp, gdouble *i2c)
{
	GnomeCanvasItem *item;
	PixbufPrivate *priv;
	double ra[6];
	ArtDRect rect;

	item = GNOME_CANVAS_ITEM (gcp);
	priv = gcp->priv;

	if (!priv->pixbuf) {
		item->x1 = item->y1 = item->x2 = item->y2 = 0.0;
		return;
	}

	rect.x0 = 0.0;
	rect.x1 = gdk_pixbuf_get_width (priv->pixbuf);

	rect.y0 = 0.0;
	rect.y1 = gdk_pixbuf_get_height (priv->pixbuf);

#ifdef GNOME_CANVAS_PIXBUF_VERBOSE
	g_print ("i2c %g %g %g %g %g %g\n", i2c[0], i2c[1], i2c[2], i2c[3], i2c[4], i2c[5]);
#endif
	gnome_canvas_item_i2c_affine (item, i2c);
#ifdef GNOME_CANVAS_PIXBUF_VERBOSE
	g_print ("i2c %g %g %g %g %g %g\n", i2c[0], i2c[1], i2c[2], i2c[3], i2c[4], i2c[5]);
#endif
	compute_render_affine (gcp, ra, i2c);
#ifdef GNOME_CANVAS_PIXBUF_VERBOSE
	g_print ("ra %g %g %g %g %g %g\n", ra[0], ra[1], ra[2], ra[3], ra[4], ra[5]);
#endif
	art_drect_affine_transform (&rect, &rect, ra);

	item->x1 = floor (rect.x0);
	item->y1 = floor (rect.y0);
	item->x2 = ceil (rect.x1);
	item->y2 = ceil (rect.y1);
}



/* Update sequence */

/* Update handler for the pixbuf canvas item */
static void
gnome_canvas_pixbuf_update (GnomeCanvasItem *item, double *affine, ArtSVP *clip_path, int flags)
{
	GnomeCanvasPixbuf *gcp;
	PixbufPrivate *priv;

	gcp = GNOME_CANVAS_PIXBUF (item);
	priv = gcp->priv;

	if (parent_class->update)
		(* parent_class->update) (item, affine, clip_path, flags);

        /* the optimzations below cause rarely triggered redrawing bugs and
	 * don't seem to actually save much performance. so it's probably
	 * better to turn them off, than to chase subtle optimization bugs
	 * throughgout all of gnome-canvas-pixbuf.c - TIMJ
	 */
#ifdef USE_BROKEN_OPTIMIZATIONS
	if (((flags & GNOME_CANVAS_UPDATE_VISIBILITY)
	     && !(GTK_OBJECT_FLAGS (item) & GNOME_CANVAS_ITEM_VISIBLE))
	    || (flags & GNOME_CANVAS_UPDATE_AFFINE)
	    || priv->need_pixbuf_update
	    || priv->need_xform_update) {
		gnome_canvas_request_redraw (item->canvas, item->x1, item->y1, item->x2, item->y2);
	}

	/* If we need a pixbuf update, or if the item changed visibility to
	 * shown, recompute the bounding box.
	 */
	if (priv->need_pixbuf_update
	    || priv->need_xform_update
	    || ((flags & GNOME_CANVAS_UPDATE_VISIBILITY)
		&& (GTK_OBJECT_FLAGS (gcp) & GNOME_CANVAS_ITEM_VISIBLE))
	    || (flags & GNOME_CANVAS_UPDATE_AFFINE)) {
		recompute_bounding_box (gcp, affine);
#ifdef GNOME_CANVAS_PIXBUF_VERBOSE
		g_print ("BBox is %g %g %g %g\n", item->x1, item->y1, item->x2, item->y2);
#endif
		gnome_canvas_request_redraw (item->canvas, item->x1, item->y1, item->x2, item->y2);
		priv->need_pixbuf_update = FALSE;
		priv->need_xform_update = FALSE;
	}
#else   /* ordinary update logic */
        gnome_canvas_request_redraw (item->canvas, item->x1, item->y1, item->x2, item->y2);
        recompute_bounding_box (gcp, affine);
        gnome_canvas_request_redraw (item->canvas, item->x1, item->y1, item->x2, item->y2);
        priv->need_pixbuf_update = FALSE;
        priv->need_xform_update = FALSE;
#endif
}



/* Rendering */

/* This is private to libart, but we need it.  Sigh. */
extern void art_rgb_affine_run (int *p_x0, int *p_x1, int y, int src_width, int src_height,
				const double affine[6]);

/* Fills the specified buffer with the transformed version of a pixbuf */
static void
transform_pixbuf (guchar *dest, int x, int y, int width, int height, int rowstride,
		  GdkPixbuf *pixbuf, double *affine)
{
	int xx, yy;
	double inv[6];
	guchar *src, *d;
	ArtPoint src_p, dest_p;
	int run_x1, run_x2;
	int src_x, src_y;
	int i;

	art_affine_invert (inv, affine);

	for (yy = 0; yy < height; yy++) {
		dest_p.y = y + yy + 0.5;

		run_x1 = x;
		run_x2 = x + width;
		art_rgb_affine_run (&run_x1, &run_x2, yy + y,
				    gdk_pixbuf_get_width (pixbuf),
				    gdk_pixbuf_get_height (pixbuf),
				    inv);

		d = dest + yy * rowstride + (run_x1 - x) * 4;

		for (xx = run_x1; xx < run_x2; xx++) {
			dest_p.x = xx + 0.5;
			art_affine_point (&src_p, &dest_p, inv);
			src_x = floor (src_p.x);
			src_y = floor (src_p.y);

			src =
			    gdk_pixbuf_get_pixels (pixbuf) + src_y *
			    gdk_pixbuf_get_rowstride (pixbuf) + src_x *
			    gdk_pixbuf_get_n_channels (pixbuf);

			for (i = 0; i < gdk_pixbuf_get_n_channels (pixbuf); i++)
				*d++ = *src++;

			if (!gdk_pixbuf_get_has_alpha(pixbuf))
				*d++ = 255; /* opaque */
		}
	}
}

/* Draw handler for the pixbuf canvas item */
static void
gnome_canvas_pixbuf_draw (GnomeCanvasItem *item, GdkDrawable *drawable,
			  int x, int y, int width, int height)
{
	GnomeCanvasPixbuf *gcp;
	PixbufPrivate *priv;
	double i2c[6], render_affine[6];
	guchar *buf;
	GdkPixbuf *pixbuf;
	ArtIRect p_rect, a_rect, d_rect;
	int w, h;

	gcp = GNOME_CANVAS_PIXBUF (item);
	priv = gcp->priv;

	if (!priv->pixbuf)
		return;

	gnome_canvas_item_i2c_affine (item, i2c);
	compute_render_affine (gcp, render_affine, i2c);

	/* Compute the area we need to repaint */

	p_rect.x0 = item->x1;
	p_rect.y0 = item->y1;
	p_rect.x1 = item->x2;
	p_rect.y1 = item->y2;

	a_rect.x0 = x;
	a_rect.y0 = y;
	a_rect.x1 = x + width;
	a_rect.y1 = y + height;

	art_irect_intersect (&d_rect, &p_rect, &a_rect);
	if (art_irect_empty (&d_rect))
		return;

	/* Create a temporary buffer and transform the pixbuf there */

	w = d_rect.x1 - d_rect.x0;
	h = d_rect.y1 - d_rect.y0;

	buf = g_new0 (guchar, w * h * 4);
	transform_pixbuf (buf,
			  d_rect.x0, d_rect.y0,
			  w, h,
			  w * 4,
			  priv->pixbuf, render_affine);

	pixbuf = gdk_pixbuf_new_from_data (buf, GDK_COLORSPACE_RGB,
					   TRUE,
					   8, w, h,
					   w * 4,
					   NULL, NULL);

	gdk_draw_pixbuf (drawable, NULL, pixbuf,
			 0, 0,
			 d_rect.x0 - x, d_rect.y0 - y,
			 w, h,
			 GDK_RGB_DITHER_MAX,
			 d_rect.x0, d_rect.y0);

	g_object_unref (pixbuf);
	g_free (buf);
}

/* Render handler for the pixbuf canvas item */
static void
gnome_canvas_pixbuf_render (GnomeCanvasItem *item, GnomeCanvasBuf *buf)
{
	GnomeCanvasPixbuf *gcp;
	PixbufPrivate *priv;
	double i2c[6], render_affine[6];

	gcp = GNOME_CANVAS_PIXBUF (item);
	priv = gcp->priv;

	if (!priv->pixbuf)
		return;

	gnome_canvas_item_i2c_affine (item, i2c);
	compute_render_affine (gcp, render_affine, i2c);
        gnome_canvas_buf_ensure_buf (buf);


	if ((fabs (render_affine[1]) < GNOME_CANVAS_EPSILON) &&
	    (fabs (render_affine[2]) < GNOME_CANVAS_EPSILON) &&
	    render_affine[0] > 0.0 &&
	    render_affine[3] > 0.0)
	  {
	    GdkPixbuf *dest_pixbuf;
	    int x0, y0, x1, y1;

	    dest_pixbuf = gdk_pixbuf_new_from_data (buf->buf,
						    GDK_COLORSPACE_RGB,
						    FALSE,
						    8,
						    buf->rect.x1 - buf->rect.x0,
						    buf->rect.y1 - buf->rect.y0,
						    buf->buf_rowstride,
						    NULL, NULL);


	    x0 = floor (render_affine[4] - buf->rect.x0 + 0.5);
	    y0 = floor (render_affine[5] - buf->rect.y0 + 0.5);

	    x1 = x0 + floor (gdk_pixbuf_get_width (priv->pixbuf) * render_affine[0] + 0.5);
	    y1 = y0 + floor (gdk_pixbuf_get_height (priv->pixbuf) * render_affine[3] + 0.5);

	    x0 = MAX (x0, 0);
	    x0 = MIN (x0, buf->rect.x1 - buf->rect.x0);
	    y0 = MAX (y0, 0);
	    y0 = MIN (y0, buf->rect.y1 - buf->rect.y0);
	    
	    x1 = MAX (x1, 0);
	    x1 = MIN (x1, buf->rect.x1 - buf->rect.x0);
	    y1 = MAX (y1, 0);
	    y1 = MIN (y1, buf->rect.y1 - buf->rect.y0);
	    
	    gdk_pixbuf_composite  (priv->pixbuf,
				   dest_pixbuf,
				   x0, y0,
				   x1 - x0, y1 - y0,
				   render_affine[4] - buf->rect.x0,
				   render_affine[5] - buf->rect.y0,
				   render_affine[0],
				   render_affine[3],
				   GDK_INTERP_BILINEAR,
				   255);

	    g_object_unref (dest_pixbuf);
	  }
	else if (gdk_pixbuf_get_has_alpha(priv->pixbuf))
		art_rgb_rgba_affine (buf->buf,
				     buf->rect.x0, buf->rect.y0, buf->rect.x1, buf->rect.y1,
				     buf->buf_rowstride,
				     gdk_pixbuf_get_pixels(priv->pixbuf),
				     gdk_pixbuf_get_width(priv->pixbuf),
				     gdk_pixbuf_get_height(priv->pixbuf),
				     gdk_pixbuf_get_rowstride(priv->pixbuf),
				     render_affine,
				     ART_FILTER_NEAREST,
				     NULL);
	else
		art_rgb_affine (buf->buf,
				buf->rect.x0, buf->rect.y0, buf->rect.x1, buf->rect.y1,
				buf->buf_rowstride,
				gdk_pixbuf_get_pixels(priv->pixbuf),
				gdk_pixbuf_get_width(priv->pixbuf),
				gdk_pixbuf_get_height(priv->pixbuf),
				gdk_pixbuf_get_rowstride(priv->pixbuf),
				render_affine,
				ART_FILTER_NEAREST,
				NULL);

	buf->is_bg = 0;
}



/* Point handler for the pixbuf canvas item */
static double
gnome_canvas_pixbuf_point (GnomeCanvasItem *item, double x, double y, int cx, int cy,
			   GnomeCanvasItem **actual_item)
{
	GnomeCanvasPixbuf *gcp;
	PixbufPrivate *priv;
	double i2c[6], render_affine[6], inv[6];
	ArtPoint c, p;
	int px, py;
	double no_hit;
	guchar *src;
	GdkPixbuf *pixbuf;

	gcp = GNOME_CANVAS_PIXBUF (item);
	priv = gcp->priv;
	pixbuf = priv->pixbuf;

	*actual_item = item;

	no_hit = item->canvas->pixels_per_unit * 2 + 10;

	if (!priv->pixbuf)
		return no_hit;

	gnome_canvas_item_i2c_affine (item, i2c);
	compute_render_affine (gcp, render_affine, i2c);
	art_affine_invert (inv, render_affine);

	c.x = cx;
	c.y = cy;
	art_affine_point (&p, &c, inv);
	px = p.x;
	py = p.y;

	if (px < 0 || px >= gdk_pixbuf_get_width (pixbuf) ||
	    py < 0 || py >= gdk_pixbuf_get_height (pixbuf))
		return no_hit;

	if (!gdk_pixbuf_get_has_alpha (pixbuf))
		return 0.0;

	src = gdk_pixbuf_get_pixels (pixbuf) +
	    py * gdk_pixbuf_get_rowstride (pixbuf) +
	    px * gdk_pixbuf_get_n_channels (pixbuf);

	if (src[3] < 128)
		return no_hit;
	else
		return 0.0;
}



/* Bounds handler for the pixbuf canvas item */
static void
gnome_canvas_pixbuf_bounds (GnomeCanvasItem *item, double *x1, double *y1, double *x2, double *y2)
{
	GnomeCanvasPixbuf *gcp;
	PixbufPrivate *priv;
	double i2c[6], viewport_affine[6];
	ArtDRect rect;

	gcp = GNOME_CANVAS_PIXBUF (item);
	priv = gcp->priv;

	if (!priv->pixbuf) {
		*x1 = *y1 = *x2 = *y2 = 0.0;
		return;
	}

	rect.x0 = 0.0;
	rect.x1 = gdk_pixbuf_get_width (priv->pixbuf);

	rect.y0 = 0.0;
	rect.y1 = gdk_pixbuf_get_height (priv->pixbuf);

	gnome_canvas_item_i2c_affine (item, i2c);
	compute_viewport_affine (gcp, viewport_affine, i2c);
	art_drect_affine_transform (&rect, &rect, viewport_affine);

	*x1 = rect.x0;
	*y1 = rect.y0;
	*x2 = rect.x1;
	*y2 = rect.y1;
}
