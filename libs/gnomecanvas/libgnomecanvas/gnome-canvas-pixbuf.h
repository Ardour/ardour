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

#ifndef GNOME_CANVAS_PIXBUF_H
#define GNOME_CANVAS_PIXBUF_H


#include <libgnomecanvas/gnome-canvas.h>

G_BEGIN_DECLS



#define GNOME_TYPE_CANVAS_PIXBUF            (gnome_canvas_pixbuf_get_type ())
#define GNOME_CANVAS_PIXBUF(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), GNOME_TYPE_CANVAS_PIXBUF, GnomeCanvasPixbuf))
#define GNOME_CANVAS_PIXBUF_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), GNOME_TYPE_CANVAS_PIXBUF, GnomeCanvasPixbufClass))
#define GNOME_IS_CANVAS_PIXBUF(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GNOME_TYPE_CANVAS_PIXBUF))
#define GNOME_IS_CANVAS_PIXBUF_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), GNOME_TYPE_CANVAS_PIXBUF))
#define GNOME_CANVAS_PIXBUF_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), GNOME_TYPE_CANVAS_PIXBUF, GnomeCanvasPixbufClass))


typedef struct _GnomeCanvasPixbuf GnomeCanvasPixbuf;
typedef struct _GnomeCanvasPixbufClass GnomeCanvasPixbufClass;

struct _GnomeCanvasPixbuf {
	GnomeCanvasItem item;

	/* Private data */
	gpointer priv;
};

struct _GnomeCanvasPixbufClass {
	GnomeCanvasItemClass parent_class;
};


GType gnome_canvas_pixbuf_get_type (void) G_GNUC_CONST;



G_END_DECLS

#endif
