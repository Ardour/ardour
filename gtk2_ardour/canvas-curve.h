/* libgnomecanvas/gnome-canvas-curve.h: GnomeCanvas item for constrained spline curves
 *
 * Copyright (C) 2003 Paul Davis <pbd@op.net>
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
 *
 */

#ifndef __GNOME_CANVAS_CURVE_H__
#define __GNOME_CANVAS_CURVE_H__

#include <libgnomecanvas/libgnomecanvas.h>

G_BEGIN_DECLS

/* Wave viewer item for canvas.
 */

#define GNOME_CANVAS_TYPE_CANVAS_CURVE            (gnome_canvas_curve_get_type ())
#define GNOME_CANVAS_CURVE(obj)                   (GTK_CHECK_CAST ((obj), GNOME_CANVAS_TYPE_CANVAS_CURVE, GnomeCanvasCurve))
#define GNOME_CANVAS_CURVE_CLASS(klass)           (GTK_CHECK_CLASS_CAST ((klass), GNOME_CANVAS_TYPE_CANVAS_CURVE, GnomeCanvasCurveClass))
#define GNOME_CANVAS_IS_CANVAS_CURVE(obj)         (GTK_CHECK_TYPE ((obj), GNOME_CANVAS_TYPE_CANVAS_CURVE))
#define GNOME_CANVAS_IS_CANVAS_CURVE_CLASS(klass) (GTK_CHECK_CLASS_TYPE ((klass), GNOME_CANVAS_TYPE_CANVAS_CURVE))

typedef struct _GnomeCanvasCurve            GnomeCanvasCurve;
typedef struct _GnomeCanvasCurveClass       GnomeCanvasCurveClass;

struct _GnomeCanvasCurve
{
    GnomeCanvasItem item;
    double x1, y1, x2, y2;
    void* curve_arg;
    float* vector;
    size_t veclen;
    uint32_t color;
    
    /* cached values set during update/used during render */

    unsigned char r, b, g, a;
    guint32 bbox_ulx, bbox_uly;
    guint32 bbox_lrx, bbox_lry;
};

struct _GnomeCanvasCurveClass {
	GnomeCanvasItemClass parent_class;
};

GtkType gnome_canvas_curve_get_type (void);

G_END_DECLS

#endif /* __GNOME_CANVAS_CURVE_H__ */
