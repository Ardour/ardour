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
 * Author: Federico Mena <federico@nuclecu.unam.mx>
 */

#ifndef GNOME_CANVAS_RECT_ELLIPSE_H
#define GNOME_CANVAS_RECT_ELLIPSE_H


#include <libgnomecanvas/gnome-canvas.h>

#include <libgnomecanvas/gnome-canvas-shape.h>

#include <libart_lgpl/art_svp.h>

G_BEGIN_DECLS


/* Base class for rectangle and ellipse item types.  These are defined by their top-left and
 * bottom-right corners.  Rectangles and ellipses share the following arguments:
 *
 * name			type		read/write	description
 * ------------------------------------------------------------------------------------------
 * x1			double		RW		Leftmost coordinate of rectangle or ellipse
 * y1			double		RW		Topmost coordinate of rectangle or ellipse
 * x2			double		RW		Rightmost coordinate of rectangle or ellipse
 * y2			double		RW		Bottommost coordinate of rectangle or ellipse
 * fill_color		string		W		X color specification for fill color,
 *							or NULL pointer for no color (transparent)
 * fill_color_gdk	GdkColor*	RW		Allocated GdkColor for fill
 * outline_color	string		W		X color specification for outline color,
 *							or NULL pointer for no color (transparent)
 * outline_color_gdk	GdkColor*	RW		Allocated GdkColor for outline
 * fill_stipple		GdkBitmap*	RW		Stipple pattern for fill
 * outline_stipple	GdkBitmap*	RW		Stipple pattern for outline
 * width_pixels		uint		RW		Width of the outline in pixels.  The outline will
 *							not be scaled when the canvas zoom factor is changed.
 * width_units		double		RW		Width of the outline in canvas units.  The outline
 *							will be scaled when the canvas zoom factor is changed.
 */


#define GNOME_TYPE_CANVAS_RE            (gnome_canvas_re_get_type ())
#define GNOME_CANVAS_RE(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), GNOME_TYPE_CANVAS_RE, GnomeCanvasRE))
#define GNOME_CANVAS_RE_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), GNOME_TYPE_CANVAS_RE, GnomeCanvasREClass))
#define GNOME_IS_CANVAS_RE(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GNOME_TYPE_CANVAS_RE))
#define GNOME_IS_CANVAS_RE_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), GNOME_TYPE_CANVAS_RE))
#define GNOME_CANVAS_RE_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), GNOME_TYPE_CANVAS_RE, GnomeCanvasREClass))


typedef struct _GnomeCanvasRE      GnomeCanvasRE;
typedef struct _GnomeCanvasREClass GnomeCanvasREClass;

struct _GnomeCanvasRE {
	GnomeCanvasShape item;

	double x1, y1, x2, y2;		/* Corners of item */

	unsigned int path_dirty : 1;
};

struct _GnomeCanvasREClass {
	GnomeCanvasShapeClass parent_class;
};


/* Standard Gtk function */
GType gnome_canvas_re_get_type (void) G_GNUC_CONST;


/* Rectangle item.  No configurable or queryable arguments are available (use those in
 * GnomeCanvasRE).
 */


#define GNOME_TYPE_CANVAS_RECT            (gnome_canvas_rect_get_type ())
#define GNOME_CANVAS_RECT(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), GNOME_TYPE_CANVAS_RECT, GnomeCanvasRect))
#define GNOME_CANVAS_RECT_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), GNOME_TYPE_CANVAS_RECT, GnomeCanvasRectClass))
#define GNOME_IS_CANVAS_RECT(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GNOME_TYPE_CANVAS_RECT))
#define GNOME_IS_CANVAS_RECT_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), GNOME_TYPE_CANVAS_RECT))
#define GNOME_CANVAS_RECT_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), GNOME_TYPE_CANVAS_RECT, GnomeCanvasRectClass))


typedef struct _GnomeCanvasRect GnomeCanvasRect;
typedef struct _GnomeCanvasRectClass GnomeCanvasRectClass;

struct _GnomeCanvasRect {
	GnomeCanvasRE re;
};

struct _GnomeCanvasRectClass {
	GnomeCanvasREClass parent_class;
};


/* Standard Gtk function */
GType gnome_canvas_rect_get_type (void) G_GNUC_CONST;


/* Ellipse item.  No configurable or queryable arguments are available (use those in
 * GnomeCanvasRE).
 */


#define GNOME_TYPE_CANVAS_ELLIPSE            (gnome_canvas_ellipse_get_type ())
#define GNOME_CANVAS_ELLIPSE(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), GNOME_TYPE_CANVAS_ELLIPSE, GnomeCanvasEllipse))
#define GNOME_CANVAS_ELLIPSE_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), GNOME_TYPE_CANVAS_ELLIPSE, GnomeCanvasEllipseClass))
#define GNOME_IS_CANVAS_ELLIPSE(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GNOME_TYPE_CANVAS_ELLIPSE))
#define GNOME_IS_CANVAS_ELLIPSE_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), GNOME_TYPE_CANVAS_ELLIPSE))
#define GNOME_CANVAS_ELLIPSE_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), GNOME_TYPE_CANVAS_ELLIPSE, GnomeCanvasEllipseClass))


typedef struct _GnomeCanvasEllipse GnomeCanvasEllipse;
typedef struct _GnomeCanvasEllipseClass GnomeCanvasEllipseClass;

struct _GnomeCanvasEllipse {
	GnomeCanvasRE re;
};

struct _GnomeCanvasEllipseClass {
	GnomeCanvasREClass parent_class;
};


/* Standard Gtk function */
GType gnome_canvas_ellipse_get_type (void) G_GNUC_CONST;


G_END_DECLS

#endif
