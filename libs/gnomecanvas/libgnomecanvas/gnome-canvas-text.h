/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
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
/* Text item type for GnomeCanvas widget
 *
 * GnomeCanvas is basically a port of the Tk toolkit's most excellent canvas widget.  Tk is
 * copyrighted by the Regents of the University of California, Sun Microsystems, and other parties.
 *
 *
 * Author: Federico Mena <federico@nuclecu.unam.mx>
 * Port to Pango co-done by Gergõ Érdi <cactus@cactus.rulez.org>
 */

#ifndef GNOME_CANVAS_TEXT_H
#define GNOME_CANVAS_TEXT_H


#include <libgnomecanvas/gnome-canvas.h>


G_BEGIN_DECLS


/* Text item for the canvas.  Text items are positioned by an anchor point and an anchor direction.
 *
 * A clipping rectangle may be specified for the text.  The rectangle is anchored at the text's anchor
 * point, and is specified by clipping width and height parameters.  If the clipping rectangle is
 * enabled, it will clip the text.
 *
 * In addition, x and y offset values may be specified.  These specify an offset from the anchor
 * position.  If used in conjunction with the clipping rectangle, these could be used to implement
 * simple scrolling of the text within the clipping rectangle.
 *
 * Properties marked with [*] also have _set properties associated
 * with them, that determine if the specified value should be used
 * instead of the default (style-defined) values
 *
 * The following object arguments are available:
 *
 * name			type			read/write	description
 * ------------------------------------------------------------------------------------------
 * text			string			RW		The string of the text label
 * markup		string			 W		A Pango markup string for the text label
 *
 * x			double			RW		X coordinate of anchor point
 * y			double			RW		Y coordinate of anchor point
 *
 * font			string			 W		A string describing the font
 * font_desc	        PangoFontDescription*	RW		Pointer to a PangoFontDescriptor
 * attributes           PangoAttrList*          RW		Pointer to a Pango attribute list
 * style		PangoStyle		RW		Pango style of font to use	[*]
 * variant		PangoVariant		RW		Pango variant of font to use	[*]
 * weight		int			RW		Pango weight of font to use	[*]
 * stretch		PangoStretch		RW		Pango stretch of font to use	[*]
 * size			int			RW		Size (in pixels) of font	[*]
 * size_points		double			RW		Size (in points) of font
 * scale                double                  RW              Ratio to scale font		[*]
 *
 * anchor		GtkAnchorType		RW		Anchor side for the text
 * justification	GtkJustification	RW		Justification for multiline text
 * clip_width		double			RW		Width of clip rectangle
 * clip_height		double			RW		Height of clip rectangle
 * clip			boolean			RW		Use clipping rectangle?
 * x_offset		double			RW		Horizontal offset distance from anchor position
 * y_offset		double			RW		Vertical offset distance from anchor position
 *
 * text_width		double			R		Used to query the width of the rendered text
 * text_height		double			R		Used to query the rendered height of the text
 *
 * fill_color		string			 W		X color specification for text
 * fill_color_gdk	GdkColor*		RW		Pointer to an allocated GdkColor
 * fill_color_rgba	guint   		RW		RGBA value used for AA color.
 * fill_stipple		GdkBitmap*		RW		Stipple pattern for filling the text
 */

#define GNOME_TYPE_CANVAS_TEXT            (gnome_canvas_text_get_type ())
#define GNOME_CANVAS_TEXT(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), GNOME_TYPE_CANVAS_TEXT, GnomeCanvasText))
#define GNOME_CANVAS_TEXT_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), GNOME_TYPE_CANVAS_TEXT, GnomeCanvasTextClass))
#define GNOME_IS_CANVAS_TEXT(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GNOME_TYPE_CANVAS_TEXT))
#define GNOME_IS_CANVAS_TEXT_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), GNOME_TYPE_CANVAS_TEXT))
#define GNOME_CANVAS_TEXT_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), GNOME_TYPE_CANVAS_TEXT, GnomeCanvasTextClass))


typedef struct _GnomeCanvasText GnomeCanvasText;
typedef struct _GnomeCanvasTextClass GnomeCanvasTextClass;

typedef struct _GnomeCanvasTextPrivate GnomeCanvasTextPrivate;

struct _GnomeCanvasText {
	GnomeCanvasItem item;

	PangoFontDescription *font_desc; /* Font description for text */
	PangoAttrList *attr_list;        /* Attribute list of the text (caching) */
	PangoUnderline underline;
	gboolean       strikethrough;
	int            rise;
	double         scale;
	
	char *text;			/* Text to display */
	GdkBitmap *stipple;		/* Stipple for text */
	GdkGC *gc;			/* GC for drawing text */
        PangoLayout *layout;            /* The PangoLayout containing the text */

	gulong pixel;			/* Fill color */

	double x, y;			/* Position at anchor */

	double clip_width;		/* Width of optional clip rectangle */
	double clip_height;		/* Height of optional clip rectangle */

	double xofs, yofs;		/* Text offset distance from anchor position */

	double affine[6];               /* The item -> canvas affine */ /*AA*/

	GtkAnchorType anchor;		/* Anchor side for text */
	GtkJustification justification;	/* Justification for text */

	int cx, cy;			/* Top-left canvas coordinates for text */
	int clip_cx, clip_cy;		/* Top-left canvas coordinates for clip rectangle */
	int clip_cwidth, clip_cheight;	/* Size of clip rectangle in pixels */
	int max_width;			/* Maximum width of text lines */
	int height;			/* Rendered text height in pixels */

        guint32 rgba;			/* RGBA color for text */ /*AA*/

	guint clip : 1;			/* Use clip rectangle? */

	guint underline_set : 1;        /* Apply specified underline style? */
	guint strike_set    : 1;        /* Apply specified strikethrough style? */
	guint rise_set      : 1;        /* Apply specified ascension/descension? */

	guint scale_set     : 1;        /* Apply specified font scaling ratio? */

	GnomeCanvasTextPrivate *priv;	
};

struct _GnomeCanvasTextClass {
	GnomeCanvasItemClass parent_class;
};


/* Standard Gtk function */
GType gnome_canvas_text_get_type (void) G_GNUC_CONST;


G_END_DECLS

#endif
