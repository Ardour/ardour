/* Image item type for GtkCanvas widget
 *
 * GtkCanvas is basically a port of the Tk toolkit's most excellent canvas widget.  Tk is
 * copyrighted by the Regents of the University of California, Sun Microsystems, and other parties.
 *
 * Copyright (C) 1998 The Free Software Foundation
 *
 * Author: Federico Mena <federico@nuclecu.unam.mx>
 */
 
 
#ifndef __GTK_CANVAS_IMAGEFRAME_H__
#define __GTK_CANVAS_IMAGEFRAME_H__

#include <stdint.h>

#include <gtk-canvas/gtk-canvas-defs.h>
#include <gtk/gtkpacker.h> /* why the hell is GtkAnchorType here and not in gtkenums.h? */
#include <libart_lgpl/art_misc.h>
#include <libart_lgpl/art_pixbuf.h>
#include "gtk-canvas/gtk-canvas.h"


BEGIN_GTK_CANVAS_DECLS


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


#define GTK_CANVAS_TYPE_CANVAS_IMAGEFRAME            (gtk_canvas_imageframe_get_type ())
#define GTK_CANVAS_IMAGEFRAME(obj)                   (GTK_CHECK_CAST ((obj), GTK_CANVAS_TYPE_CANVAS_IMAGEFRAME, GtkCanvasImageFrame))
#define GTK_CANVAS_IMAGEFRAME_CLASS(klass)           (GTK_CHECK_CLASS_CAST ((klass), GTK_CANVAS_TYPE_CANVAS_IMAGEFRAME, GtkCanvasImageFrameClass))
#define GTK_CANVAS_IS_CANVAS_IMAGEFRAME(obj)         (GTK_CHECK_TYPE ((obj), GTK_CANVAS_TYPE_CANVAS_IMAGEFRAME))
#define GTK_CANVAS_IS_CANVAS_IMAGEFRAME_CLASS(klass) (GTK_CHECK_CLASS_TYPE ((klass), GTK_CANVAS_TYPE_CANVAS_IMAGEFRAME))


typedef struct _GtkCanvasImageFrame GtkCanvasImageFrame;
typedef struct _GtkCanvasImageFrameClass GtkCanvasImageFrameClass;

struct _GtkCanvasImageFrame {
	GtkCanvasItem item;

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

struct _GtkCanvasImageFrameClass {
	GtkCanvasItemClass parent_class;
};


/* Standard Gtk function */
GtkType gtk_canvas_imageframe_get_type (void);


END_GTK_CANVAS_DECLS

#endif
