#ifndef GNOME_CANVAS_CLIPGROUP_H
#define GNOME_CANVAS_CLIPGROUP_H

/* Clipping group implementation for GnomeCanvas
 *
 * GnomeCanvas is basically a port of the Tk toolkit's most excellent canvas widget.  Tk is
 * copyrighted by the Regents of the University of California, Sun Microsystems, and other parties.
 *
 * TODO: Implement this in libgnomeui, possibly merge with real group
 *
 * Copyright (C) 1998,1999 The Free Software Foundation
 *
 * Author:
 *          Lauris Kaplinski <lauris@ximian.com>
 */

#include <libgnomecanvas/gnome-canvas.h>
#include <libgnomecanvas/gnome-canvas-util.h>

#include <libart_lgpl/art_bpath.h>
#include <libart_lgpl/art_svp_wind.h>
#include <libart_lgpl/art_vpath_dash.h>
#include <libgnomecanvas/gnome-canvas-path-def.h>

G_BEGIN_DECLS


#define GNOME_TYPE_CANVAS_CLIPGROUP            (gnome_canvas_clipgroup_get_type ())
#define GNOME_CANVAS_CLIPGROUP(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), GNOME_TYPE_CANVAS_CLIPGROUP, GnomeCanvasClipgroup))
#define GNOME_CANVAS_CLIPGROUP_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), GNOME_TYPE_CANVAS_CLIPGROUP, GnomeCanvasClipgroupClass))
#define GNOME_IS_CANVAS_CLIPGROUP(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GNOME_TYPE_CANVAS_CLIPGROUP))
#define GNOME_IS_CANVAS_CLIPGROUP_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), GNOME_TYPE_CANVAS_CLIPGROUP))


typedef struct _GnomeCanvasClipgroup GnomeCanvasClipgroup;
typedef struct _GnomeCanvasClipgroupClass GnomeCanvasClipgroupClass;

struct _GnomeCanvasClipgroup {
	GnomeCanvasGroup group;

	GnomeCanvasPathDef * path;
	ArtWindRule wind;

	ArtSVP * svp;
};

struct _GnomeCanvasClipgroupClass {
	GnomeCanvasGroupClass parent_class;
};


/* Standard Gtk function */
GType gnome_canvas_clipgroup_get_type (void) G_GNUC_CONST;


G_END_DECLS

#endif
