#ifndef GNOME_CANVAS_SHAPE_PRIVATE_H
#define GNOME_CANVAS_SHAPE_PRIVATE_H

/* Bpath item type for GnomeCanvas widget
 *
 * GnomeCanvas is basically a port of the Tk toolkit's most excellent canvas widget.  Tk is
 * copyrighted by the Regents of the University of California, Sun Microsystems, and other parties.
 *
 * Copyright (C) 1998,1999 The Free Software Foundation
 *
 * Authors: Federico Mena <federico@nuclecu.unam.mx>
 *          Raph Levien <raph@acm.org>
 *          Lauris Kaplinski <lauris@ariman.ee>
 */

#include <gdk/gdk.h>
#include <libart_lgpl/art_vpath.h>
#include <libart_lgpl/art_svp.h>
#include <libart_lgpl/art_vpath_dash.h>
#include <libart_lgpl/art_svp_wind.h>
#include <libgnomecanvas/gnome-canvas.h>

#include <libgnomecanvas/gnome-canvas-path-def.h>

G_BEGIN_DECLS

typedef struct _GnomeCanvasShapePrivGdk GnomeCanvasShapePrivGdk;
typedef struct _GCBPDrawCtx GCBPDrawCtx;

/* Per canvas private structure, holding necessary data for rendering
 * temporary masks, which are needed for drawing multipart bpaths.
 * As canvas cannot multithread, we can be sure, that masks are used
 * serially, also one set of masks per canvas is sufficent to guarantee,
 * that masks are created on needed X server. Masks grow as needed.
 * Full structure is refcounted in Bpath implementation
 */

struct _GCBPDrawCtx {
	gint refcount;

	GnomeCanvas * canvas;

	gint width;
	gint height;

	GdkBitmap * mask;
	GdkBitmap * clip;

	GdkGC * clear_gc;
	GdkGC * xor_gc;
};

/* Per Bpath private structure, holding Gdk specific data */

struct _GnomeCanvasShapePrivGdk {
	gulong fill_pixel;		/* Color for fill */
	gulong outline_pixel;		/* Color for outline */

	GdkBitmap *fill_stipple;	/* Stipple for fill */
	GdkBitmap *outline_stipple;	/* Stipple for outline */

	GdkGC * fill_gc;		/* GC for filling */
	GdkGC * outline_gc;		/* GC for outline */

	gint len_points;		/* Size of allocated points array */
	gint num_points;		/* Gdk points in canvas coords */
	GdkPoint * points;		/* Ivariant: closed paths are before open ones */
	GSList * closed_paths;		/* List of lengths */
	GSList * open_paths;		/* List of lengths */

	GCBPDrawCtx * ctx;		/* Pointer to per-canvas drawing context */
};

struct _GnomeCanvasShapePriv {
	GnomeCanvasPathDef * path;      /* Our bezier path representation */

	gdouble scale;			/* CTM scaling (for pen) */

	guint fill_set : 1;		/* Is fill color set? */
	guint outline_set : 1;		/* Is outline color set? */
	guint width_pixels : 1;		/* Is outline width specified in pixels or units? */

	double width;			/* Width of outline, in user coords */

	guint32 fill_rgba;		/* Fill color, RGBA */
	guint32 outline_rgba;		/* Outline color, RGBA */

	GdkCapStyle cap;		/* Cap style for line */
	GdkJoinStyle join;		/* Join style for line */
	ArtWindRule wind;		/* Winding rule */
	double miterlimit;		/* Miter limit */

	ArtVpathDash dash;		/* Dashing pattern */

	ArtSVP * fill_svp;		/* The SVP for the filled shape */
	ArtSVP * outline_svp;		/* The SVP for the outline shape */

	GnomeCanvasShapePrivGdk * gdk;	/* Gdk specific things */
};

G_END_DECLS

#endif
