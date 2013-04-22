/* Image item type for GnomeCanvas widget
 *
 * GnomeCanvas is basically a port of the Tk toolkit's most excellent canvas widget.  Tk is
 * copyrighted by the Regents of the University of California, Sun Microsystems, and other parties.
 *
 * Copyright (C) 1998 The Free Software Foundation
 * All rights reserved.
 *
 * Author: Federico Mena <federico@nuclecu.unam.mx>
 *
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


#ifndef __GNOME_CANVAS_IMAGEFRAME_H__
#define __GNOME_CANVAS_IMAGEFRAME_H__

#include <stdint.h>

#include <libgnomecanvas/libgnomecanvas.h>
#include <gtk/gtkenums.h>

#ifdef __cplusplus
extern "C" {
#endif
#include <libart_lgpl/art_misc.h>
#ifdef __cplusplus
}
#endif

#include <libart_lgpl/art_pixbuf.h>


G_BEGIN_DECLS


/* Image item for the canvas.  Images are positioned by anchoring them to a point.
 * The following arguments are available:
 *
 * name		type			read/write	description
 * ------------------------------------------------------------------------------------------
 * pixbuf     ArtPixBuf*      W		Pointer to an ArtPixBuf (aa-mode)
 * x          double          RW		X coordinate of anchor point
 * y          double          RW		Y coordinate of anchor point
 * width      double          RW		Width to scale image to, in canvas units
 * height     double          RW		Height to scale image to, in canvas units
 * drawwidth  double          RW		Width to scale image to, in canvas units
 * anchor     GtkAnchorType   RW		Anchor side for the image
 */


#define GNOME_CANVAS_TYPE_CANVAS_IMAGEFRAME            (gnome_canvas_imageframe_get_type ())
#define GNOME_CANVAS_IMAGEFRAME(obj)                   (GTK_CHECK_CAST ((obj), GNOME_CANVAS_TYPE_CANVAS_IMAGEFRAME, GnomeCanvasImageFrame))
#define GNOME_CANVAS_IMAGEFRAME_CLASS(klass)           (GTK_CHECK_CLASS_CAST ((klass), GNOME_CANVAS_TYPE_CANVAS_IMAGEFRAME, GnomeCanvasImageFrameClass))
#define GNOME_CANVAS_IS_CANVAS_IMAGEFRAME(obj)         (GTK_CHECK_TYPE ((obj), GNOME_CANVAS_TYPE_CANVAS_IMAGEFRAME))
#define GNOME_CANVAS_IS_CANVAS_IMAGEFRAME_CLASS(klass) (GTK_CHECK_CLASS_TYPE ((klass), GNOME_CANVAS_TYPE_CANVAS_IMAGEFRAME))


typedef struct _GnomeCanvasImageFrame GnomeCanvasImageFrame;
typedef struct _GnomeCanvasImageFrameClass GnomeCanvasImageFrameClass;

struct _GnomeCanvasImageFrame {
	GnomeCanvasItem item;

	double x, y;			/* Position at anchor, item relative */
	double width, height;		/* Size of image, item relative */
	double drawwidth ;		/* the amount of the image we draw width-wise (0-drawwidth)*/
	GtkAnchorType anchor;		/* Anchor side for image */

	int cx, cy;			/* Top-left canvas coordinates for display */
	int cwidth, cheight;		/* Rendered size in pixels */

	uint32_t need_recalc : 1;	/* Do we need to rescale the image? */

	ArtPixBuf *pixbuf;		/* A pixbuf, for aa rendering */
	double affine[6];               /* The item -> canvas affine */
};

struct _GnomeCanvasImageFrameClass {
	GnomeCanvasItemClass parent_class;
};


/* Standard Gtk function */
GtkType gnome_canvas_imageframe_get_type (void);


G_END_DECLS

#endif
