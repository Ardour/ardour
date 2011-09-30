/* Bpath item type for GnomeCanvas widget
 *
 * GnomeCanvas is basically a port of the Tk toolkit's most excellent canvas widget.  Tk is
 * copyrighted by the Regents of the University of California, Sun Microsystems, and other parties.
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

#include "gnome-canvas-bpath.h"
#include "gnome-canvas-shape.h"
#include "gnome-canvas-shape-private.h"
#include "gnome-canvas-path-def.h"

enum {
	PROP_0,
	PROP_BPATH
};

static void gnome_canvas_bpath_class_init   (GnomeCanvasBpathClass *class);
static void gnome_canvas_bpath_init         (GnomeCanvasBpath      *bpath);
static void gnome_canvas_bpath_destroy      (GtkObject               *object);
static void gnome_canvas_bpath_set_property (GObject               *object,
					     guint                  param_id,
					     const GValue          *value,
                                             GParamSpec            *pspec);
static void gnome_canvas_bpath_get_property (GObject               *object,
					     guint                  param_id,
					     GValue                *value,
                                             GParamSpec            *pspec);

static void   gnome_canvas_bpath_update      (GnomeCanvasItem *item, double *affine, ArtSVP *clip_path, int flags);


static GnomeCanvasShapeClass *parent_class;

GType
gnome_canvas_bpath_get_type (void)
{
	static GType bpath_type;

	if (!bpath_type) {
		const GTypeInfo object_info = {
			sizeof (GnomeCanvasBpathClass),
			(GBaseInitFunc) NULL,
			(GBaseFinalizeFunc) NULL,
			(GClassInitFunc) gnome_canvas_bpath_class_init,
			(GClassFinalizeFunc) NULL,
			NULL,			/* class_data */
			sizeof (GnomeCanvasBpath),
			0,			/* n_preallocs */
			(GInstanceInitFunc) gnome_canvas_bpath_init,
			NULL			/* value_table */
		};

		bpath_type = g_type_register_static (GNOME_TYPE_CANVAS_SHAPE, "GnomeCanvasBpath",
						     &object_info, 0);
	}

	return bpath_type;
}

static void
gnome_canvas_bpath_class_init (GnomeCanvasBpathClass *class)
{
	GObjectClass         *gobject_class;
	GtkObjectClass       *object_class;
	GnomeCanvasItemClass *item_class;

	gobject_class = (GObjectClass *) class;
	object_class = (GtkObjectClass *) class;
	item_class = (GnomeCanvasItemClass *) class;

	parent_class = g_type_class_peek_parent (class);

	/* when this gets checked into libgnomeui, change the
           GTK_TYPE_POINTER to GTK_TYPE_GNOME_CANVAS_BPATH, and add an
           entry to gnome-boxed.defs */

	gobject_class->set_property = gnome_canvas_bpath_set_property;
	gobject_class->get_property = gnome_canvas_bpath_get_property;

	object_class->destroy = gnome_canvas_bpath_destroy;

	g_object_class_install_property (gobject_class,
                                         PROP_BPATH,
                                         g_param_spec_boxed ("bpath", NULL, NULL,
                                                             GNOME_TYPE_CANVAS_PATH_DEF,
                                                             (G_PARAM_READABLE | G_PARAM_WRITABLE)));

	item_class->update = gnome_canvas_bpath_update;
}

static void
gnome_canvas_bpath_init (GnomeCanvasBpath* bpath)
{

}

static void
gnome_canvas_bpath_destroy (GtkObject *object)
{
	if (GTK_OBJECT_CLASS (parent_class)->destroy)
		(* GTK_OBJECT_CLASS (parent_class)->destroy) (object);
}

static void
gnome_canvas_bpath_set_property (GObject      *object,
                                 guint         param_id,
                                 const GValue *value,
                                 GParamSpec   *pspec)
{
	GnomeCanvasItem         *item;
	GnomeCanvasPathDef      *gpp;

	item = GNOME_CANVAS_ITEM (object);

	switch (param_id) {
	case PROP_BPATH:
		gpp = (GnomeCanvasPathDef*) g_value_get_boxed (value);

		gnome_canvas_shape_set_path_def (GNOME_CANVAS_SHAPE (object), gpp);

		gnome_canvas_item_request_update (item);
		break;

	default:
		break;
	}
}


static void
gnome_canvas_bpath_get_property (GObject     *object,
                                 guint        param_id,
                                 GValue      *value,
                                 GParamSpec  *pspec)
{
	GnomeCanvasShape        *shape;

	shape = GNOME_CANVAS_SHAPE(object);

	switch (param_id) {
	case PROP_BPATH:
		g_value_set_boxed (value, shape->priv->path);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, param_id, pspec);
		break;
	}
}

static void
gnome_canvas_bpath_update (GnomeCanvasItem *item, double *affine, ArtSVP *clip_path, int flags)
{
	if(GNOME_CANVAS_ITEM_CLASS(parent_class)->update) {
		(* GNOME_CANVAS_ITEM_CLASS(parent_class)->update)(item, affine, clip_path, flags);
	}
}
