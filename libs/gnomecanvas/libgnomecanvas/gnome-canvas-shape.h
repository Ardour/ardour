/* Generic bezier shape item for GnomeCanvas
 *
 * GnomeCanvas is basically a port of the Tk toolkit's most excellent canvas widget.  Tk is
 * copyrighted by the Regents of the University of California, Sun Microsystems, and other parties.
 *
 * Copyright (C) 1998,1999 The Free Software Foundation
 *
 * Authors: Federico Mena <federico@nuclecu.unam.mx>
 *          Raph Levien <raph@acm.org>
 *          Lauris Kaplinski <lauris@ximian.com>
 *          Rusty Conover <rconover@bangtail.net>
 */

#ifndef GNOME_CANVAS_SHAPE_H
#define GNOME_CANVAS_SHAPE_H

#include <libgnomecanvas/gnome-canvas.h>
#include <libgnomecanvas/gnome-canvas-path-def.h>

G_BEGIN_DECLS


/* Shape item for the canvas.
 *
 * The following object arguments are available:
 *
 * name			type			read/write	description
 * ------------------------------------------------------------------------------------------
 * fill_color		string			W		X color specification for fill color,
 *								or NULL pointer for no color (transparent).
 * fill_color_gdk	GdkColor*		RW		Allocated GdkColor for fill.
 * outline_color	string			W		X color specification for outline color,
 *								or NULL pointer for no color (transparent).
 * outline_color_gdk	GdkColor*		RW		Allocated GdkColor for outline.
 * fill_stipple		GdkBitmap*		RW		Stipple pattern for fill
 * outline_stipple	GdkBitmap*		RW		Stipple pattern for outline
 * width_pixels		uint			RW		Width of the outline in pixels.  The outline will
 *								not be scaled when the canvas zoom factor is changed.
 * width_units		double			RW		Width of the outline in canvas units.  The outline
 *								will be scaled when the canvas zoom factor is changed.
 * cap_style		GdkCapStyle		RW		Cap ("endpoint") style for the bpath.
 * join_style		GdkJoinStyle		RW		Join ("vertex") style for the bpath.
 * wind                 ArtWindRule             RW              Winding rule for the bpath.
 * dash			ArtVpathDash		RW		Dashing pattern
 * miterlimit		double			RW		Minimum angle between segments, where miter join
 *								rule is applied.
 */

#define GNOME_TYPE_CANVAS_SHAPE            (gnome_canvas_shape_get_type ())
#define GNOME_CANVAS_SHAPE(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), GNOME_TYPE_CANVAS_SHAPE, GnomeCanvasShape))
#define GNOME_CANVAS_SHAPE_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), GNOME_TYPE_CANVAS_SHAPE, GnomeCanvasShapeClass))
#define GNOME_IS_CANVAS_SHAPE(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GNOME_TYPE_CANVAS_SHAPE))
#define GNOME_IS_CANVAS_SHAPE_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), GNOME_TYPE_CANVAS_SHAPE))


typedef struct _GnomeCanvasShape GnomeCanvasShape;
typedef struct _GnomeCanvasShapePriv GnomeCanvasShapePriv;
typedef struct _GnomeCanvasShapeClass GnomeCanvasShapeClass;

struct _GnomeCanvasShape {
	GnomeCanvasItem item;

	GnomeCanvasShapePriv *priv;	/* Private data */
};

struct _GnomeCanvasShapeClass {
	GnomeCanvasItemClass parent_class;
};


/* WARNING! These are not usable from modifying shapes from user programs */
/* These are meant, to set master shape from subclass ::update method */
void gnome_canvas_shape_set_path_def (GnomeCanvasShape *shape, GnomeCanvasPathDef *def);
GnomeCanvasPathDef *gnome_canvas_shape_get_path_def (GnomeCanvasShape *shape);

/* Standard Gtk function */
GType gnome_canvas_shape_get_type (void) G_GNUC_CONST;

G_END_DECLS

#endif
