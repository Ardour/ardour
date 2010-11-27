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
 *          Rusty Conover <rconover@bangtail.net>
 */

#ifndef GNOME_CANVAS_BPATH_H
#define GNOME_CANVAS_BPATH_H

#include <libgnomecanvas/gnome-canvas.h>
#include <libgnomecanvas/gnome-canvas-shape.h>
#include <libgnomecanvas/gnome-canvas-path-def.h>

G_BEGIN_DECLS


/* Bpath item for the canvas.
 *
 * The following object arguments are available:
 *
 * name			type			read/write	description
 * ------------------------------------------------------------------------------------------
 * bpath		GnomeCanvasPathDef *		RW		Pointer to an GnomeCanvasPathDef structure.
 *								This can be created by a call to
 *								gp_path_new() in (gp-path.h).
 */

#define GNOME_TYPE_CANVAS_BPATH            (gnome_canvas_bpath_get_type ())
#define GNOME_CANVAS_BPATH(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), GNOME_TYPE_CANVAS_BPATH, GnomeCanvasBpath))
#define GNOME_CANVAS_BPATH_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), GNOME_TYPE_CANVAS_BPATH, GnomeCanvasBpathClass))
#define GNOME_IS_CANVAS_BPATH(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GNOME_TYPE_CANVAS_BPATH))
#define GNOME_IS_CANVAS_BPATH_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), GNOME_TYPE_CANVAS_BPATH))


typedef struct _GnomeCanvasBpath GnomeCanvasBpath;
typedef struct _GnomeCanvasBpathPriv GnomeCanvasBpathPriv;
typedef struct _GnomeCanvasBpathClass GnomeCanvasBpathClass;

struct _GnomeCanvasBpath {
	GnomeCanvasShape item;
	
};

struct _GnomeCanvasBpathClass {
	GnomeCanvasShapeClass parent_class;
};


/* Standard Gtk function */
GType gnome_canvas_bpath_get_type (void) G_GNUC_CONST;

G_END_DECLS

#endif
