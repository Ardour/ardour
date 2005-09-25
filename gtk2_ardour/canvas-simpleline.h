/* gtk-canvas-simpleline.h: GtkCanvas item for simple rects
 *
 * Copyright (C) 2001 Paul Davis <pbd@op.net>
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

#ifndef __GTK_CANVAS_SIMPLELINE_H__
#define __GTK_CANVAS_SIMPLELINE_H__

#include <stdint.h>

#include <gtk-canvas/gtk-canvas-defs.h>
#include "gtk-canvas/gtk-canvas.h"

BEGIN_GTK_CANVAS_DECLS

/* Wave viewer item for canvas.
 */

#define GTK_CANVAS_TYPE_CANVAS_SIMPLELINE            (gtk_canvas_simpleline_get_type ())
#define GTK_CANVAS_SIMPLELINE(obj)                   (GTK_CHECK_CAST ((obj), GTK_CANVAS_TYPE_CANVAS_SIMPLELINE, GtkCanvasSimpleLine))
#define GTK_CANVAS_SIMPLELINE_CLASS(klass)           (GTK_CHECK_CLASS_CAST ((klass), GTK_CANVAS_TYPE_CANVAS_SIMPLELINE, GtkCanvasSimpleLineClass))
#define GTK_CANVAS_IS_CANVAS_SIMPLELINE(obj)         (GTK_CHECK_TYPE ((obj), GTK_CANVAS_TYPE_CANVAS_SIMPLELINE))
#define GTK_CANVAS_IS_CANVAS_SIMPLELINE_CLASS(klass) (GTK_CHECK_CLASS_TYPE ((klass), GTK_CANVAS_TYPE_CANVAS_SIMPLELINE))

typedef struct _GtkCanvasSimpleLine            GtkCanvasSimpleLine;
typedef struct _GtkCanvasSimpleLineClass       GtkCanvasSimpleLineClass;

struct _GtkCanvasSimpleLine
{
    GtkCanvasItem item;
    double x1, y1, x2, y2;
    uint32_t color;
    gboolean horizontal;

    /* cached values set during update/used during render */

    unsigned char r, b, g, a;
    guint32 bbox_ulx, bbox_uly;
    guint32 bbox_lrx, bbox_lry;
};

struct _GtkCanvasSimpleLineClass {
	GtkCanvasItemClass parent_class;
};

GtkType gtk_canvas_simpleline_get_type (void);

END_GTK_CANVAS_DECLS

#endif /* __GTK_CANVAS_SIMPLELINE_H__ */
