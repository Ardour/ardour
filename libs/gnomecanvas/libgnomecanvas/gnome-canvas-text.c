/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * $Id$
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
 * GnomeCanvas is basically a port of the Tk toolkit's most excellent canvas
 * widget.  Tk is copyrighted by the Regents of the University of California,
 * Sun Microsystems, and other parties.
 *
 *
 * Author: Federico Mena <federico@nuclecu.unam.mx>
 * Port to Pango co-done by Gergõ Érdi <cactus@cactus.rulez.org>
 */

#include <config.h>
#include <math.h>
#include <string.h>
#include "gnome-canvas-text.h"
#include <pango/pangoft2.h>

#include "libart_lgpl/art_affine.h"
#include "libart_lgpl/art_rgb_a_affine.h"
#include "libart_lgpl/art_rgb.h"
#include "libart_lgpl/art_rgb_bitmap_affine.h"
#include "gnome-canvas-util.h"
#include "gnome-canvas-i18n.h"



/* Object argument IDs */
enum {
	PROP_0,

	/* Text contents */
	PROP_TEXT,
	PROP_MARKUP,

	/* Position */
	PROP_X,
	PROP_Y,

	/* Font */
	PROP_FONT,
	PROP_FONT_DESC,
	PROP_FAMILY, PROP_FAMILY_SET,
	
	/* Style */
	PROP_ATTRIBUTES,
	PROP_STYLE,         PROP_STYLE_SET,
	PROP_VARIANT,       PROP_VARIANT_SET,
	PROP_WEIGHT,        PROP_WEIGHT_SET,
	PROP_STRETCH,	    PROP_STRETCH_SET,
	PROP_SIZE,          PROP_SIZE_SET,
	PROP_SIZE_POINTS,
	PROP_STRIKETHROUGH, PROP_STRIKETHROUGH_SET,
	PROP_UNDERLINE,     PROP_UNDERLINE_SET,
	PROP_RISE,          PROP_RISE_SET,
	PROP_SCALE,         PROP_SCALE_SET,

	/* Clipping */
	PROP_ANCHOR,
	PROP_JUSTIFICATION,
	PROP_CLIP_WIDTH,
	PROP_CLIP_HEIGHT,
	PROP_CLIP,
	PROP_X_OFFSET,
	PROP_Y_OFFSET,

	/* Coloring */
	PROP_FILL_COLOR,
	PROP_FILL_COLOR_GDK,
	PROP_FILL_COLOR_RGBA,
	PROP_FILL_STIPPLE,

	/* Rendered size accessors */
	PROP_TEXT_WIDTH,
	PROP_TEXT_HEIGHT
};

struct _GnomeCanvasTextPrivate {
	guint render_dirty : 1;
	FT_Bitmap bitmap;
};


static void gnome_canvas_text_class_init (GnomeCanvasTextClass *class);
static void gnome_canvas_text_init (GnomeCanvasText *text);
static void gnome_canvas_text_destroy (GtkObject *object);
static void gnome_canvas_text_set_property (GObject            *object,
					    guint               param_id,
					    const GValue       *value,
					    GParamSpec         *pspec);
static void gnome_canvas_text_get_property (GObject            *object,
					    guint               param_id,
					    GValue             *value,
					    GParamSpec         *pspec);

static void gnome_canvas_text_update (GnomeCanvasItem *item, double *affine,
				      ArtSVP *clip_path, int flags);
static void gnome_canvas_text_realize (GnomeCanvasItem *item);
static void gnome_canvas_text_unrealize (GnomeCanvasItem *item);
static void gnome_canvas_text_draw (GnomeCanvasItem *item, GdkDrawable *drawable,
				    int x, int y, int width, int height);
static double gnome_canvas_text_point (GnomeCanvasItem *item, double x, double y, int cx, int cy,
				       GnomeCanvasItem **actual_item);
static void gnome_canvas_text_bounds (GnomeCanvasItem *item,
				      double *x1, double *y1, double *x2, double *y2);
static void gnome_canvas_text_render (GnomeCanvasItem *item, GnomeCanvasBuf *buf);

static void gnome_canvas_text_set_markup (GnomeCanvasText *textitem,
					  const gchar     *markup);

static void gnome_canvas_text_set_font_desc    (GnomeCanvasText *textitem,
					        PangoFontDescription *font_desc);

static void gnome_canvas_text_apply_font_desc  (GnomeCanvasText *textitem);
static void gnome_canvas_text_apply_attributes (GnomeCanvasText *textitem);

static void add_attr (PangoAttrList  *attr_list,
		      PangoAttribute *attr);

static GnomeCanvasItemClass *parent_class;



/**
 * gnome_canvas_text_get_type:
 * @void: 
 * 
 * Registers the &GnomeCanvasText class if necessary, and returns the type ID
 * associated to it.
 * 
 * Return value: The type ID of the &GnomeCanvasText class.
 **/
GType
gnome_canvas_text_get_type (void)
{
	static GType text_type;

	if (!text_type) {
		const GTypeInfo object_info = {
			sizeof (GnomeCanvasTextClass),
			(GBaseInitFunc) NULL,
			(GBaseFinalizeFunc) NULL,
			(GClassInitFunc) gnome_canvas_text_class_init,
			(GClassFinalizeFunc) NULL,
			NULL,			/* class_data */
			sizeof (GnomeCanvasText),
			0,			/* n_preallocs */
			(GInstanceInitFunc) gnome_canvas_text_init,
			NULL			/* value_table */
		};

		text_type = g_type_register_static (GNOME_TYPE_CANVAS_ITEM, "GnomeCanvasText",
						    &object_info, 0);
	}

	return text_type;
}

/* Class initialization function for the text item */
static void
gnome_canvas_text_class_init (GnomeCanvasTextClass *class)
{
	GObjectClass *gobject_class;
	GtkObjectClass *object_class;
	GnomeCanvasItemClass *item_class;

	gobject_class = (GObjectClass *) class;
	object_class = (GtkObjectClass *) class;
	item_class = (GnomeCanvasItemClass *) class;

	parent_class = g_type_class_peek_parent (class);

	gobject_class->set_property = gnome_canvas_text_set_property;
	gobject_class->get_property = gnome_canvas_text_get_property;

	/* Text */
        g_object_class_install_property
                (gobject_class,
                 PROP_TEXT,
                 g_param_spec_string ("text",
				      _("Text"),
				      _("Text to render"),
                                      NULL,
                                      (G_PARAM_READABLE | G_PARAM_WRITABLE)));

        g_object_class_install_property
                (gobject_class,
                 PROP_MARKUP,
                 g_param_spec_string ("markup",
				      _("Markup"),
				      _("Marked up text to render"),
				      NULL,
                                      (G_PARAM_WRITABLE)));

	/* Position */
        g_object_class_install_property
                (gobject_class,
                 PROP_X,
                 g_param_spec_double ("x", NULL, NULL,
				      -G_MAXDOUBLE, G_MAXDOUBLE, 0.0,
				      (G_PARAM_READABLE | G_PARAM_WRITABLE)));

        g_object_class_install_property
                (gobject_class,
                 PROP_Y,
                 g_param_spec_double ("y", NULL, NULL,
				      -G_MAXDOUBLE, G_MAXDOUBLE, 0.0,
				      (G_PARAM_READABLE | G_PARAM_WRITABLE)));


	/* Font */
	g_object_class_install_property
                (gobject_class,
                 PROP_FONT,
                 g_param_spec_string ("font",
				      _("Font"),
				      _("Font description as a string"),
                                      NULL,
                                      (G_PARAM_READABLE | G_PARAM_WRITABLE)));
	
        g_object_class_install_property
		(gobject_class,
		 PROP_FONT_DESC,
		 g_param_spec_boxed ("font_desc",
				     _("Font description"),
				     _("Font description as a PangoFontDescription struct"),
				     PANGO_TYPE_FONT_DESCRIPTION,
				     (G_PARAM_READABLE | G_PARAM_WRITABLE)));

	g_object_class_install_property
		(gobject_class,
		 PROP_FAMILY,
		 g_param_spec_string ("family",
				      _("Font family"),
				      _("Name of the font family, e.g. Sans, Helvetica, Times, Monospace"),
				      NULL,
				      (G_PARAM_READABLE | G_PARAM_WRITABLE)));
	
	/* Style */
        g_object_class_install_property
                (gobject_class,
                 PROP_ATTRIBUTES,
                 g_param_spec_boxed ("attributes", NULL, NULL,
				     PANGO_TYPE_ATTR_LIST,
				     (G_PARAM_READABLE | G_PARAM_WRITABLE)));
	
	g_object_class_install_property
		(gobject_class,
		 PROP_STYLE,
		 g_param_spec_enum ("style",
				    _("Font style"),
				    _("Font style"),
				    PANGO_TYPE_STYLE,
				    PANGO_STYLE_NORMAL,
				    G_PARAM_READABLE | G_PARAM_WRITABLE));
	
	g_object_class_install_property
		(gobject_class,
		 PROP_VARIANT,
		 g_param_spec_enum ("variant",
				    _("Font variant"),
				    _("Font variant"),
				    PANGO_TYPE_VARIANT,
				    PANGO_VARIANT_NORMAL,
				    G_PARAM_READABLE | G_PARAM_WRITABLE));
	
	g_object_class_install_property
		(gobject_class,
		 PROP_WEIGHT,
		 g_param_spec_int ("weight",
				   _("Font weight"),
				   _("Font weight"),
				   0,
				   G_MAXINT,
				   PANGO_WEIGHT_NORMAL,
				   G_PARAM_READABLE | G_PARAM_WRITABLE));
	
	
	g_object_class_install_property
		(gobject_class,
		 PROP_STRETCH,
		 g_param_spec_enum ("stretch",
				    _("Font stretch"),
				    _("Font stretch"),
				    PANGO_TYPE_STRETCH,
				    PANGO_STRETCH_NORMAL,
				    G_PARAM_READABLE | G_PARAM_WRITABLE));
	
	g_object_class_install_property
		(gobject_class,
		 PROP_SIZE,
		 g_param_spec_int ("size",
				   _("Font size"),
				   _("Font size (as a multiple of PANGO_SCALE, eg. 12*PANGO_SCALE for a 12pt font size)"),
				   0,
				   G_MAXINT,
				   0,
				   G_PARAM_READABLE | G_PARAM_WRITABLE));
	
	g_object_class_install_property
		(gobject_class,
		PROP_SIZE_POINTS,
		g_param_spec_double ("size_points",
				     _("Font points"),
				     _("Font size in points (eg. 12 for a 12pt font size)"),
				     0.0,
				     G_MAXDOUBLE,
				     0.0,
				     G_PARAM_READABLE | G_PARAM_WRITABLE));  
	
	g_object_class_install_property
		(gobject_class,
		 PROP_RISE,
		 g_param_spec_int ("rise",
				   _("Rise"),
				   _("Offset of text above the baseline (below the baseline if rise is negative)"),
				   -G_MAXINT,
				   G_MAXINT,
				   0,
				   G_PARAM_READABLE | G_PARAM_WRITABLE));
	
	g_object_class_install_property
		(gobject_class,
		 PROP_STRIKETHROUGH,
		 g_param_spec_boolean ("strikethrough",
				       _("Strikethrough"),
				       _("Whether to strike through the text"),
				       FALSE,
				       G_PARAM_READABLE | G_PARAM_WRITABLE));
	
	g_object_class_install_property
		(gobject_class,
		 PROP_UNDERLINE,
		 g_param_spec_enum ("underline",
				    _("Underline"),
				    _("Style of underline for this text"),
				    PANGO_TYPE_UNDERLINE,
				    PANGO_UNDERLINE_NONE,
				    G_PARAM_READABLE | G_PARAM_WRITABLE));

	g_object_class_install_property
		(gobject_class,
		 PROP_SCALE,
		 g_param_spec_double ("scale",
				      _("Scale"),
				      _("Size of font, relative to default size"),
				      0.0,
				      G_MAXDOUBLE,
				      1.0,
				      G_PARAM_READABLE | G_PARAM_WRITABLE));  
	
        g_object_class_install_property
		(gobject_class,
                 PROP_ANCHOR,
                 g_param_spec_enum ("anchor", NULL, NULL,
                                    GTK_TYPE_ANCHOR_TYPE,
                                    GTK_ANCHOR_CENTER,
                                    (G_PARAM_READABLE | G_PARAM_WRITABLE)));
        g_object_class_install_property
                (gobject_class,
                 PROP_JUSTIFICATION,
                 g_param_spec_enum ("justification", NULL, NULL,
                                    GTK_TYPE_JUSTIFICATION,
                                    GTK_JUSTIFY_LEFT,
                                    (G_PARAM_READABLE | G_PARAM_WRITABLE)));
        g_object_class_install_property
                (gobject_class,
                 PROP_CLIP_WIDTH,
                 g_param_spec_double ("clip_width", NULL, NULL,
				      -G_MAXDOUBLE, G_MAXDOUBLE, 0.0,
				      (G_PARAM_READABLE | G_PARAM_WRITABLE)));
        g_object_class_install_property
                (gobject_class,
                 PROP_CLIP_HEIGHT,
                 g_param_spec_double ("clip_height", NULL, NULL,
				      -G_MAXDOUBLE, G_MAXDOUBLE, 0.0,
				      (G_PARAM_READABLE | G_PARAM_WRITABLE)));
        g_object_class_install_property
                (gobject_class,
                 PROP_CLIP,
                 g_param_spec_boolean ("clip", NULL, NULL,
				       FALSE,
				       (G_PARAM_READABLE | G_PARAM_WRITABLE)));
        g_object_class_install_property
                (gobject_class,
                 PROP_X_OFFSET,
                 g_param_spec_double ("x_offset", NULL, NULL,
				      -G_MAXDOUBLE, G_MAXDOUBLE, 0.0,
				      (G_PARAM_READABLE | G_PARAM_WRITABLE)));
        g_object_class_install_property
                (gobject_class,
                 PROP_Y_OFFSET,
                 g_param_spec_double ("y_offset", NULL, NULL,
				      -G_MAXDOUBLE, G_MAXDOUBLE, 0.0,
				      (G_PARAM_READABLE | G_PARAM_WRITABLE)));
        g_object_class_install_property
                (gobject_class,
                 PROP_FILL_COLOR,
                 g_param_spec_string ("fill_color",
				      _("Color"),
				      _("Text color, as string"),
                                      NULL,
                                      (G_PARAM_READABLE | G_PARAM_WRITABLE)));
        g_object_class_install_property
                (gobject_class,
                 PROP_FILL_COLOR_GDK,
                 g_param_spec_boxed ("fill_color_gdk",
				     _("Color"),
				     _("Text color, as a GdkColor"),
				     GDK_TYPE_COLOR,
				     (G_PARAM_READABLE | G_PARAM_WRITABLE)));
        g_object_class_install_property
                (gobject_class,
                 PROP_FILL_COLOR_RGBA,
                 g_param_spec_uint ("fill_color_rgba",
				    _("Color"),
				    _("Text color, as an R/G/B/A combined integer"),
				    0, G_MAXUINT, 0,
				    (G_PARAM_READABLE | G_PARAM_WRITABLE)));
        g_object_class_install_property
                (gobject_class,
                 PROP_FILL_STIPPLE,
                 g_param_spec_object ("fill_stipple", NULL, NULL,
                                      GDK_TYPE_DRAWABLE,
                                      (G_PARAM_READABLE | G_PARAM_WRITABLE)));
        g_object_class_install_property
                (gobject_class,
                 PROP_TEXT_WIDTH,
                 g_param_spec_double ("text_width",
				      _("Text width"),
				      _("Width of the rendered text"),
				      0.0, G_MAXDOUBLE, 0.0,
				      G_PARAM_READABLE));
        g_object_class_install_property
                (gobject_class,
                 PROP_TEXT_HEIGHT,
                 g_param_spec_double ("text_height",
				      _("Text height"),
				      _("Height of the rendered text"),
				      0.0, G_MAXDOUBLE, 0.0,
				      G_PARAM_READABLE));

	/* Style props are set (explicitly applied) or not */
#define ADD_SET_PROP(propname, propval, nick, blurb) g_object_class_install_property (gobject_class, propval, g_param_spec_boolean (propname, nick, blurb, FALSE, G_PARAM_READABLE | G_PARAM_WRITABLE))

	ADD_SET_PROP ("family_set", PROP_FAMILY_SET,
		      _("Font family set"),
		      _("Whether this tag affects the font family"));  
	
	ADD_SET_PROP ("style_set", PROP_STYLE_SET,
		      _("Font style set"),
		      _("Whether this tag affects the font style"));
	
	ADD_SET_PROP ("variant_set", PROP_VARIANT_SET,
		      _("Font variant set"),
		      _("Whether this tag affects the font variant"));
	
	ADD_SET_PROP ("weight_set", PROP_WEIGHT_SET,
		      _("Font weight set"),
		      _("Whether this tag affects the font weight"));
	
	ADD_SET_PROP ("stretch_set", PROP_STRETCH_SET,
		      _("Font stretch set"),
		      _("Whether this tag affects the font stretch"));
	
	ADD_SET_PROP ("size_set", PROP_SIZE_SET,
		      _("Font size set"),
		      _("Whether this tag affects the font size"));
	
	ADD_SET_PROP ("rise_set", PROP_RISE_SET,
		      _("Rise set"),
		      _("Whether this tag affects the rise"));
	
	ADD_SET_PROP ("strikethrough_set", PROP_STRIKETHROUGH_SET,
		      _("Strikethrough set"),
		      _("Whether this tag affects strikethrough"));
	
	ADD_SET_PROP ("underline_set", PROP_UNDERLINE_SET,
		      _("Underline set"),
		      _("Whether this tag affects underlining"));

	ADD_SET_PROP ("scale_set", PROP_SCALE_SET,
		      _("Scale set"),
		      _("Whether this tag affects font scaling"));
#undef ADD_SET_PROP
	
	object_class->destroy = gnome_canvas_text_destroy;

	item_class->update = gnome_canvas_text_update;
	item_class->realize = gnome_canvas_text_realize;
	item_class->unrealize = gnome_canvas_text_unrealize;
	item_class->draw = gnome_canvas_text_draw;
	item_class->point = gnome_canvas_text_point;
	item_class->bounds = gnome_canvas_text_bounds;
	item_class->render = gnome_canvas_text_render;
}

/* Object initialization function for the text item */
static void
gnome_canvas_text_init (GnomeCanvasText *text)
{
	text->x = 0.0;
	text->y = 0.0;
	text->anchor = GTK_ANCHOR_CENTER;
	text->justification = GTK_JUSTIFY_LEFT;
	text->clip_width = 0.0;
	text->clip_height = 0.0;
	text->xofs = 0.0;
	text->yofs = 0.0;
	text->layout = NULL;

	text->font_desc = NULL;
	
	text->underline     = PANGO_UNDERLINE_NONE;
	text->strikethrough = FALSE;
	text->rise          = 0;
	
	text->underline_set = FALSE;
	text->strike_set    = FALSE;
	text->rise_set      = FALSE;
	
	text->priv = g_new (GnomeCanvasTextPrivate, 1);
	text->priv->bitmap.buffer = NULL;
	text->priv->render_dirty = 1;
}

/* Destroy handler for the text item */
static void
gnome_canvas_text_destroy (GtkObject *object)
{
	GnomeCanvasText *text;

	g_return_if_fail (GNOME_IS_CANVAS_TEXT (object));

	text = GNOME_CANVAS_TEXT (object);

	/* remember, destroy can be run multiple times! */

	g_free (text->text);
	text->text = NULL;

	if (text->layout)
	    g_object_unref (G_OBJECT (text->layout));
	text->layout = NULL;
	
	if (text->font_desc) {
		pango_font_description_free (text->font_desc);
		text->font_desc = NULL;
	}

	if (text->attr_list)
		pango_attr_list_unref (text->attr_list);
	text->attr_list = NULL;
	
	if (text->stipple)
		g_object_unref (text->stipple);
	text->stipple = NULL;

	if (text->priv && text->priv->bitmap.buffer) {
		g_free (text->priv->bitmap.buffer);		
	}
	g_free (text->priv);
	text->priv = NULL;
	
	if (GTK_OBJECT_CLASS (parent_class)->destroy)
		(* GTK_OBJECT_CLASS (parent_class)->destroy) (object);
}

static void
get_bounds (GnomeCanvasText *text, double *px1, double *py1, double *px2, double *py2)
{
	GnomeCanvasItem *item;
	double wx, wy;

	item = GNOME_CANVAS_ITEM (text);

	/* Get canvas pixel coordinates for text position */

	
	wx = text->x;
	wy = text->y;
	gnome_canvas_item_i2w (item, &wx, &wy);
	gnome_canvas_w2c (item->canvas, wx + text->xofs, wy + text->yofs, &text->cx, &text->cy);

	/* Get canvas pixel coordinates for clip rectangle position */

	gnome_canvas_w2c (item->canvas, wx, wy, &text->clip_cx, &text->clip_cy);
	text->clip_cwidth = text->clip_width * item->canvas->pixels_per_unit;
	text->clip_cheight = text->clip_height * item->canvas->pixels_per_unit;

	/* Anchor text */

	switch (text->anchor) {
	case GTK_ANCHOR_NW:
	case GTK_ANCHOR_W:
	case GTK_ANCHOR_SW:
		break;

	case GTK_ANCHOR_N:
	case GTK_ANCHOR_CENTER:
	case GTK_ANCHOR_S:
		text->cx -= text->max_width / 2;
		text->clip_cx -= text->clip_cwidth / 2;
		break;

	case GTK_ANCHOR_NE:
	case GTK_ANCHOR_E:
	case GTK_ANCHOR_SE:
		text->cx -= text->max_width;
		text->clip_cx -= text->clip_cwidth;
		break;

	default:
		break;
	}

	switch (text->anchor) {
	case GTK_ANCHOR_NW:
	case GTK_ANCHOR_N:
	case GTK_ANCHOR_NE:
		break;

	case GTK_ANCHOR_W:
	case GTK_ANCHOR_CENTER:
	case GTK_ANCHOR_E:
		text->cy -= text->height / 2;
		text->clip_cy -= text->clip_cheight / 2;
		break;

	case GTK_ANCHOR_SW:
	case GTK_ANCHOR_S:
	case GTK_ANCHOR_SE:
		text->cy -= text->height;
		text->clip_cy -= text->clip_cheight;
		break;

	default:
		break;
	}

	/* Bounds */

	if (text->clip) {
		*px1 = text->clip_cx;
		*py1 = text->clip_cy;
		*px2 = text->clip_cx + text->clip_cwidth;
		*py2 = text->clip_cy + text->clip_cheight;
	} else {
		*px1 = text->cx;
		*py1 = text->cy;
		*px2 = text->cx + text->max_width;
		*py2 = text->cy + text->height;
	}
}

/* Convenience function to set the text's GC's foreground color */
static void
set_text_gc_foreground (GnomeCanvasText *text)
{
	GdkColor c;

	if (!text->gc)
		return;

	c.pixel = text->pixel;
	gdk_gc_set_foreground (text->gc, &c);
}

/* Sets the stipple pattern for the text */
static void
set_stipple (GnomeCanvasText *text, GdkBitmap *stipple, int reconfigure)
{
	if (text->stipple && !reconfigure)
		g_object_unref (text->stipple);

	text->stipple = stipple;
	if (stipple && !reconfigure)
		g_object_ref (stipple);

	if (text->gc) {
		if (stipple) {
			gdk_gc_set_stipple (text->gc, stipple);
			gdk_gc_set_fill (text->gc, GDK_STIPPLED);
		} else
			gdk_gc_set_fill (text->gc, GDK_SOLID);
	}
}

static PangoFontMask
get_property_font_set_mask (guint prop_id)
{
  switch (prop_id)
    {
    case PROP_FAMILY_SET:
      return PANGO_FONT_MASK_FAMILY;
    case PROP_STYLE_SET:
      return PANGO_FONT_MASK_STYLE;
    case PROP_VARIANT_SET:
      return PANGO_FONT_MASK_VARIANT;
    case PROP_WEIGHT_SET:
      return PANGO_FONT_MASK_WEIGHT;
    case PROP_STRETCH_SET:
      return PANGO_FONT_MASK_STRETCH;
    case PROP_SIZE_SET:
      return PANGO_FONT_MASK_SIZE;
    }

  return 0;
}

static void
ensure_font (GnomeCanvasText *text)
{
	if (!text->font_desc)
		text->font_desc = pango_font_description_new ();
}

/* Set_arg handler for the text item */
static void
gnome_canvas_text_set_property (GObject            *object,
				guint               param_id,
				const GValue       *value,
				GParamSpec         *pspec)
{
	GnomeCanvasItem *item;
	GnomeCanvasText *text;
	GdkColor color = { 0, 0, 0, 0, };
	GdkColor *pcolor;
	gboolean color_changed;
	int have_pixel;
	PangoAlignment align;

	g_return_if_fail (object != NULL);
	g_return_if_fail (GNOME_IS_CANVAS_TEXT (object));

	item = GNOME_CANVAS_ITEM (object);
	text = GNOME_CANVAS_TEXT (object);

	color_changed = FALSE;
	have_pixel = FALSE;
	

	if (!text->layout) {

	        PangoContext *gtk_context, *context;
		gtk_context = gtk_widget_get_pango_context (GTK_WIDGET (item->canvas));
		
	        if (item->canvas->aa)  {
			PangoLanguage *language;
			gint	pixels, mm;
			double	dpi_x;
			double	dpi_y;
			
			pixels = gdk_screen_width ();
			mm = gdk_screen_width_mm ();
			dpi_x = (((double) pixels * 25.4) / (double) mm);
			
			pixels = gdk_screen_height ();
			mm = gdk_screen_height_mm ();
			dpi_y = (((double) pixels * 25.4) / (double) mm);
			
		        context = pango_ft2_get_context (dpi_x, dpi_y);
			language = pango_context_get_language (gtk_context);
			pango_context_set_language (context, language);
			pango_context_set_base_dir (context,
						    pango_context_get_base_dir (gtk_context));
			pango_context_set_font_description (context,
							    pango_context_get_font_description (gtk_context));
			
		} else
			context = gtk_context;
			

		text->layout = pango_layout_new (context);
		
	        if (item->canvas->aa)
		        g_object_unref (G_OBJECT (context));
	}

	switch (param_id) {
	case PROP_TEXT:
		g_free (text->text);

		text->text = g_value_dup_string (value);
		pango_layout_set_text (text->layout, text->text, -1);

		text->priv->render_dirty = 1;
		break;

	case PROP_MARKUP:
		gnome_canvas_text_set_markup (text,
					      g_value_get_string (value));
		text->priv->render_dirty = 1;
		break;

	case PROP_X:
		text->x = g_value_get_double (value);
		break;

	case PROP_Y:
		text->y = g_value_get_double (value);
		break;

	case PROP_FONT: {
		const char *font_name;
		PangoFontDescription *font_desc;

		font_name = g_value_get_string (value);
		if (font_name)
			font_desc = pango_font_description_from_string (font_name);
		else
			font_desc = NULL;
		
		gnome_canvas_text_set_font_desc (text, font_desc);
		if (font_desc)
			pango_font_description_free (font_desc);

		break;
	}

	case PROP_FONT_DESC:
		gnome_canvas_text_set_font_desc (text, g_value_peek_pointer (value));
		break;

	case PROP_FAMILY:
	case PROP_STYLE:
	case PROP_VARIANT:
	case PROP_WEIGHT:
	case PROP_STRETCH:
	case PROP_SIZE:
	case PROP_SIZE_POINTS:
		ensure_font (text);

		switch (param_id) {
		case PROP_FAMILY:
			pango_font_description_set_family (text->font_desc,
							   g_value_get_string (value));
			break;
		case PROP_STYLE:
			pango_font_description_set_style (text->font_desc,
							  g_value_get_enum (value));
			break;
		case PROP_VARIANT:
			pango_font_description_set_variant (text->font_desc,
							    g_value_get_enum (value));
			break;
		case PROP_WEIGHT:
			pango_font_description_set_weight (text->font_desc,
							   g_value_get_int (value));
			break;
		case PROP_STRETCH:
			pango_font_description_set_stretch (text->font_desc,
							    g_value_get_enum (value));
			break;
		case PROP_SIZE:
			/* FIXME: This is bogus! It should be pixels, not points/PANGO_SCALE! */
			pango_font_description_set_size (text->font_desc,
							 g_value_get_int (value));
			break;
		case PROP_SIZE_POINTS:
			pango_font_description_set_size (text->font_desc,
							 g_value_get_double (value) * PANGO_SCALE);
			break;
		}
		
		gnome_canvas_text_apply_font_desc (text);
		text->priv->render_dirty = 1;
		break;

	case PROP_FAMILY_SET:
	case PROP_STYLE_SET:
	case PROP_VARIANT_SET:
	case PROP_WEIGHT_SET:
	case PROP_STRETCH_SET:
	case PROP_SIZE_SET:
		if (!g_value_get_boolean (value) && text->font_desc)
			pango_font_description_unset_fields (text->font_desc,
							     get_property_font_set_mask (param_id));
		break;

	case PROP_SCALE:
		text->scale = g_value_get_double (value);
		text->scale_set = TRUE;
		
		gnome_canvas_text_apply_font_desc (text);
		text->priv->render_dirty = 1;
		break;
		
	case PROP_SCALE_SET:
		text->scale_set = g_value_get_boolean (value);
		
		gnome_canvas_text_apply_font_desc (text);
		text->priv->render_dirty = 1;
		break;		
		
	case PROP_UNDERLINE:
		text->underline = g_value_get_enum (value);
		text->underline_set = TRUE;
		
		gnome_canvas_text_apply_attributes (text);
		text->priv->render_dirty = 1;
		break;

	case PROP_UNDERLINE_SET:
		text->underline_set = g_value_get_boolean (value);
		
		gnome_canvas_text_apply_attributes (text);
		text->priv->render_dirty = 1;
		break;

	case PROP_STRIKETHROUGH:
		text->strikethrough = g_value_get_boolean (value);
		text->strike_set = TRUE;
		
		gnome_canvas_text_apply_attributes (text);
		text->priv->render_dirty = 1;
		break;

	case PROP_STRIKETHROUGH_SET:
		text->strike_set = g_value_get_boolean (value);
		
		gnome_canvas_text_apply_attributes (text);
		text->priv->render_dirty = 1;
		break;

	case PROP_RISE:
		text->rise = g_value_get_int (value);
		text->rise_set = TRUE;
		
		gnome_canvas_text_apply_attributes (text);
		text->priv->render_dirty = 1;
		break;

	case PROP_RISE_SET:
		text->rise_set = TRUE;
		
		gnome_canvas_text_apply_attributes (text);
		text->priv->render_dirty = 1;
		break;

	case PROP_ATTRIBUTES:
		if (text->attr_list)
			pango_attr_list_unref (text->attr_list);

		text->attr_list = g_value_peek_pointer (value);
		pango_attr_list_ref (text->attr_list);
		
		gnome_canvas_text_apply_attributes (text);
		text->priv->render_dirty = 1;
		break;

	case PROP_ANCHOR:
		text->anchor = g_value_get_enum (value);
		break;

	case PROP_JUSTIFICATION:
		text->justification = g_value_get_enum (value);

		switch (text->justification) {
		case GTK_JUSTIFY_LEFT:
		        align = PANGO_ALIGN_LEFT;
			break;
		case GTK_JUSTIFY_CENTER:
		        align = PANGO_ALIGN_CENTER;
			break;
		case GTK_JUSTIFY_RIGHT:
		        align = PANGO_ALIGN_RIGHT;
			break;
		default:
		        /* GTK_JUSTIFY_FILL isn't supported yet. */
		        align = PANGO_ALIGN_LEFT;
			break;
		}		  
		pango_layout_set_alignment (text->layout, align);
		text->priv->render_dirty = 1;				
		break;

	case PROP_CLIP_WIDTH:
		text->clip_width = fabs (g_value_get_double (value));
		text->priv->render_dirty = 1;				
		break;

	case PROP_CLIP_HEIGHT:
		text->clip_height = fabs (g_value_get_double (value));
		text->priv->render_dirty = 1;				
		break;

	case PROP_CLIP:
		text->clip = g_value_get_boolean (value);
		text->priv->render_dirty = 1;
		break;

	case PROP_X_OFFSET:
		text->xofs = g_value_get_double (value);
		break;

	case PROP_Y_OFFSET:
		text->yofs = g_value_get_double (value);
		break;

        case PROP_FILL_COLOR: {
		const char *color_name;

		color_name = g_value_get_string (value);
		if (color_name) {
			gdk_color_parse (color_name, &color);

			text->rgba = ((color.red & 0xff00) << 16 |
				      (color.green & 0xff00) << 8 |
				      (color.blue & 0xff00) |
				      0xff);
			color_changed = TRUE;
		}
		text->priv->render_dirty = 1;
		break;
	}

	case PROP_FILL_COLOR_GDK:
		pcolor = g_value_get_boxed (value);
		if (pcolor) {
		    GdkColormap *colormap;

		    color = *pcolor;
		    colormap = gtk_widget_get_colormap (GTK_WIDGET (item->canvas));
		    gdk_rgb_find_color (colormap, &color);
		    have_pixel = TRUE;
		}

		text->rgba = ((color.red & 0xff00) << 16 |
			      (color.green & 0xff00) << 8|
			      (color.blue & 0xff00) |
			      0xff);
		color_changed = TRUE;
		break;

        case PROP_FILL_COLOR_RGBA:
		text->rgba = g_value_get_uint (value);
		color_changed = TRUE;
		text->priv->render_dirty = 1;
		break;

	case PROP_FILL_STIPPLE:
		set_stipple (text, (GdkBitmap *)g_value_get_object (value), FALSE);
		break;

	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, param_id, pspec);
		break;
	}

	if (color_changed) {
		if (have_pixel)
			text->pixel = color.pixel;
		else
			text->pixel = gnome_canvas_get_color_pixel (item->canvas, text->rgba);

		if (!item->canvas->aa)
			set_text_gc_foreground (text);
	}

	/* Calculate text dimensions */

	if (text->layout)
	        pango_layout_get_pixel_size (text->layout,
					     &text->max_width,
					     &text->height);
	else {
		text->max_width = 0;
		text->height = 0;
	}
	
	gnome_canvas_item_request_update (item);
}

/* Get_arg handler for the text item */
static void
gnome_canvas_text_get_property (GObject            *object,
				guint               param_id,
				GValue             *value,
				GParamSpec         *pspec)
{
	GnomeCanvasText *text;

	g_return_if_fail (object != NULL);
	g_return_if_fail (GNOME_IS_CANVAS_TEXT (object));

	text = GNOME_CANVAS_TEXT (object);

	switch (param_id) {
	case PROP_TEXT:
		g_value_set_string (value, text->text);
		break;

	case PROP_X:
		g_value_set_double (value, text->x);
		break;

	case PROP_Y:
		g_value_set_double (value, text->y);
		break;

	case PROP_FONT:
	case PROP_FONT_DESC:
	case PROP_FAMILY:
	case PROP_STYLE:
	case PROP_VARIANT:
	case PROP_WEIGHT:
	case PROP_STRETCH:
	case PROP_SIZE:
	case PROP_SIZE_POINTS:
		ensure_font (text);
		
		switch (param_id) {
		case PROP_FONT:
		{
			/* FIXME GValue imposes a totally gratuitous string copy
			 * here, we could just hand off string ownership
			 */
			gchar *str;
			
			str = pango_font_description_to_string (text->font_desc);
			g_value_set_string (value, str);
			g_free (str);

			break;
		}
			
		case PROP_FONT_DESC:
			g_value_set_boxed (value, text->font_desc);
			break;

		case PROP_FAMILY:
			g_value_set_string (value, pango_font_description_get_family (text->font_desc));
			break;
			
		case PROP_STYLE:
			g_value_set_enum (value, pango_font_description_get_style (text->font_desc));
			break;
			
		case PROP_VARIANT:
			g_value_set_enum (value, pango_font_description_get_variant (text->font_desc));
			break;
			
		case PROP_WEIGHT:
			g_value_set_int (value, pango_font_description_get_weight (text->font_desc));
			break;
			
		case PROP_STRETCH:
			g_value_set_enum (value, pango_font_description_get_stretch (text->font_desc));
			break;
			
		case PROP_SIZE:
			g_value_set_int (value, pango_font_description_get_size (text->font_desc));
			break;
			
		case PROP_SIZE_POINTS:
			g_value_set_double (value, ((double)pango_font_description_get_size (text->font_desc)) / (double)PANGO_SCALE);
			break;
		}
		break;

	case PROP_FAMILY_SET:
	case PROP_STYLE_SET:
	case PROP_VARIANT_SET:
	case PROP_WEIGHT_SET:
	case PROP_STRETCH_SET:
	case PROP_SIZE_SET:
	{
		PangoFontMask set_mask = text->font_desc ? pango_font_description_get_set_fields (text->font_desc) : 0;
		PangoFontMask test_mask = get_property_font_set_mask (param_id);
		g_value_set_boolean (value, (set_mask & test_mask) != 0);

		break;
	}

	case PROP_SCALE:
		g_value_set_double (value, text->scale);
		break;
	case PROP_SCALE_SET:
		g_value_set_boolean (value, text->scale_set);
		break;
		
	case PROP_UNDERLINE:
		g_value_set_enum (value, text->underline);
		break;
	case PROP_UNDERLINE_SET:
		g_value_set_boolean (value, text->underline_set);
		break;
		
	case PROP_STRIKETHROUGH:
		g_value_set_boolean (value, text->strikethrough);
		break;
	case PROP_STRIKETHROUGH_SET:
		g_value_set_boolean (value, text->strike_set);
		break;
		
	case PROP_RISE:
		g_value_set_int (value, text->rise);
		break;
	case PROP_RISE_SET:
		g_value_set_boolean (value, text->rise_set);
		break;
		
	case PROP_ATTRIBUTES:
		g_value_set_boxed (value, text->attr_list);
		break;

	case PROP_ANCHOR:
		g_value_set_enum (value, text->anchor);
		break;

	case PROP_JUSTIFICATION:
		g_value_set_enum (value, text->justification);
		break;

	case PROP_CLIP_WIDTH:
		g_value_set_double (value, text->clip_width);
		break;

	case PROP_CLIP_HEIGHT:
		g_value_set_double (value, text->clip_height);
		break;

	case PROP_CLIP:
		g_value_set_boolean (value, text->clip);
		break;

	case PROP_X_OFFSET:
		g_value_set_double (value, text->xofs);
		break;

	case PROP_Y_OFFSET:
		g_value_set_double (value, text->yofs);
		break;

	case PROP_FILL_COLOR:
		g_value_take_string (value,
				     g_strdup_printf ("#%02x%02x%02x",
				     text->rgba >> 24,
				     (text->rgba >> 16) & 0xff,
				     (text->rgba >> 8) & 0xff));
		break;

	case PROP_FILL_COLOR_GDK: {
		GnomeCanvas *canvas = GNOME_CANVAS_ITEM (text)->canvas;
		GdkColormap *colormap = gtk_widget_get_colormap (GTK_WIDGET (canvas));
		GdkColor color;

		gdk_colormap_query_color (colormap, text->pixel, &color);
		g_value_set_boxed (value, &color);
		break;
	}
	case PROP_FILL_COLOR_RGBA:
		g_value_set_uint (value, text->rgba);
		break;

	case PROP_FILL_STIPPLE:
		g_value_set_object (value, text->stipple);
		break;

	case PROP_TEXT_WIDTH:
		g_value_set_double (value, text->max_width / text->item.canvas->pixels_per_unit);
		break;

	case PROP_TEXT_HEIGHT:
		g_value_set_double (value, text->height / text->item.canvas->pixels_per_unit);
		break;

	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, param_id, pspec);
		break;
	}
}

/* */
static void
gnome_canvas_text_apply_font_desc (GnomeCanvasText *text)
{
	PangoFontDescription *font_desc =
		pango_font_description_copy (
			GTK_WIDGET (GNOME_CANVAS_ITEM (text)->canvas)->style->font_desc);

	if (text->font_desc)
		pango_font_description_merge (font_desc, text->font_desc, TRUE);

	pango_layout_set_font_description (text->layout, font_desc);
	pango_font_description_free (font_desc);
}

static void
add_attr (PangoAttrList  *attr_list,
	  PangoAttribute *attr)
{
	attr->start_index = 0;
	attr->end_index = G_MAXINT;

	pango_attr_list_insert (attr_list, attr);
}

/* */
static void
gnome_canvas_text_apply_attributes (GnomeCanvasText *text)
{
	PangoAttrList *attr_list;

	if (text->attr_list)
		attr_list = pango_attr_list_copy (text->attr_list);
	else
		attr_list = pango_attr_list_new ();
	
	if (text->underline_set)
		add_attr (attr_list, pango_attr_underline_new (text->underline));
	if (text->strike_set)
		add_attr (attr_list, pango_attr_strikethrough_new (text->strikethrough));
	if (text->rise_set)
		add_attr (attr_list, pango_attr_rise_new (text->rise));
	
	pango_layout_set_attributes (text->layout, attr_list);
	pango_attr_list_unref (attr_list);
}

static void
gnome_canvas_text_set_font_desc (GnomeCanvasText      *text,
				 PangoFontDescription *font_desc)
{
	if (text->font_desc)
		pango_font_description_free (text->font_desc);

	if (font_desc)
		text->font_desc = pango_font_description_copy (font_desc);
	else
		text->font_desc = NULL;

	gnome_canvas_text_apply_font_desc (text);
	text->priv->render_dirty = 1;
}

/* Setting the text from a Pango markup string */
static void
gnome_canvas_text_set_markup (GnomeCanvasText *textitem,
			      const gchar     *markup)
{
	PangoAttrList *attr_list = NULL;
	gchar         *text = NULL;
	GError        *error = NULL;

	if (markup && !pango_parse_markup (markup, -1,
					   0,
					   &attr_list, &text, NULL,
					   &error))
	{
		g_warning ("Failed to set cell text from markup due to error parsing markup: %s",
			   error->message);
		g_error_free (error);
		return;
	}

	g_free (textitem->text);
	if (textitem->attr_list)
		pango_attr_list_unref (textitem->attr_list);

	textitem->text = text;
	textitem->attr_list = attr_list;

	pango_layout_set_text (textitem->layout, text, -1);

	gnome_canvas_text_apply_attributes (textitem);
}

/* Update handler for the text item */
static void
gnome_canvas_text_update (GnomeCanvasItem *item, double *affine, ArtSVP *clip_path, int flags)
{
	GnomeCanvasText *text;
	double x1, y1, x2, y2;

	text = GNOME_CANVAS_TEXT (item);

	if (parent_class->update)
		(* parent_class->update) (item, affine, clip_path, flags);

	set_text_gc_foreground (text);
	set_stipple (text, text->stipple, TRUE);
	get_bounds (text, &x1, &y1, &x2, &y2);

	gnome_canvas_update_bbox (item,
				  floor (x1), floor (y1),
				  ceil (x2), ceil (y2));
}

/* Realize handler for the text item */
static void
gnome_canvas_text_realize (GnomeCanvasItem *item)
{
	GnomeCanvasText *text;

	text = GNOME_CANVAS_TEXT (item);

	if (parent_class->realize)
		(* parent_class->realize) (item);

	text->gc = gdk_gc_new (item->canvas->layout.bin_window);
}

/* Unrealize handler for the text item */
static void
gnome_canvas_text_unrealize (GnomeCanvasItem *item)
{
	GnomeCanvasText *text;

	text = GNOME_CANVAS_TEXT (item);

	g_object_unref (text->gc);
	text->gc = NULL;

	if (parent_class->unrealize)
		(* parent_class->unrealize) (item);
}

/* Draw handler for the text item */
static void
gnome_canvas_text_draw (GnomeCanvasItem *item, GdkDrawable *drawable,
			int x, int y, int width, int height)
{
	GnomeCanvasText *text;
	GdkRectangle rect;

	text = GNOME_CANVAS_TEXT (item);

	if (!text->text)
		return;

	if (text->clip) {
		rect.x = text->clip_cx - x;
		rect.y = text->clip_cy - y;
		rect.width = text->clip_cwidth;
		rect.height = text->clip_cheight;

		gdk_gc_set_clip_rectangle (text->gc, &rect);
	}

	if (text->stipple)
		gnome_canvas_set_stipple_origin (item->canvas, text->gc);


	gdk_draw_layout (drawable, text->gc, text->cx - x, text->cy - y, text->layout);

	if (text->clip)
		gdk_gc_set_clip_rectangle (text->gc, NULL);
}


/* Render handler for the text item */
static void
gnome_canvas_text_render (GnomeCanvasItem *item, GnomeCanvasBuf *buf)
{
	GnomeCanvasText *text;
	guint32 fg_color;
	int render_x = 0, render_y = 0; /* offsets for text rendering,
					 * for clipping rectangles */
	int x, y;
	int w, h;
	guchar *dst, *src;
	int src_dx, src_dy;
	int i, alpha;
	int bm_rows, bm_width;
	
	text = GNOME_CANVAS_TEXT (item);

	if (!text->text)
		return;

	fg_color = text->rgba;

        gnome_canvas_buf_ensure_buf (buf);

	bm_rows = (text->clip) ? text->clip_cheight : text->height;
	bm_width = (text->clip) ? text->clip_cwidth : text->max_width;
	if(text->priv->render_dirty ||
	   bm_rows != text->priv->bitmap.rows ||
	   bm_width != text->priv->bitmap.width) {		
		if(text->priv->bitmap.buffer) {
			g_free(text->priv->bitmap.buffer);
		}
		text->priv->bitmap.rows =  bm_rows;
		text->priv->bitmap.width = bm_width;
		text->priv->bitmap.pitch = (text->priv->bitmap.width+3)&~3;
		text->priv->bitmap.buffer = g_malloc0 (text->priv->bitmap.rows * text->priv->bitmap.pitch);
		text->priv->bitmap.num_grays = 256;
		text->priv->bitmap.pixel_mode = ft_pixel_mode_grays;

		/* What this does is when a clipping rectangle is
		   being used shift the rendering of the text by the
		   correct amount so that the correct result is
		   obtained as if all text was rendered, then clipped.
		   In this sense we can use smaller buffers and less
		   rendeirng since hopefully FreeType2 checks to see
		   if the glyph falls in the bounding box before
		   rasterizing it. */

		if(text->clip) {
			render_x = text->cx - text->clip_cx;
			render_y = text->cy - text->clip_cy;
		}

		pango_ft2_render_layout (&text->priv->bitmap, text->layout, render_x, render_y);

		text->priv->render_dirty = 0;
	}

	if (text->clip) {
		x = text->clip_cx - buf->rect.x0;
		y = text->clip_cy - buf->rect.y0;
	} else {
		x = text->cx - buf->rect.x0;
		y = text->cy - buf->rect.y0;
	}
		
	w = text->priv->bitmap.width;
	h = text->priv->bitmap.rows;

	src_dx = src_dy = 0;
	
	if (x + w > buf->rect.x1 - buf->rect.x0) {
		w = buf->rect.x1 - buf->rect.x0 - x;
	}
	
	if (y + h > buf->rect.y1 - buf->rect.y0) {
		h = buf->rect.y1 - buf->rect.y0 - y;
	}

	if (x < 0) {
		w -= - x;
		src_dx += - x;
		x = 0;
	}
	
	if (y < 0) {
		h -= -y;
		src_dy += - y;
		y = 0;
	}
	
	dst = buf->buf + y * buf->buf_rowstride + x * 3;
	src = text->priv->bitmap.buffer +
		src_dy * text->priv->bitmap.pitch + src_dx;
	while (h-- > 0) {
		i = w;
		while (i-- > 0) {
			/* FIXME: Do the libart thing instead of divide by 255 */
			alpha = ((fg_color & 0xff) * (*src)) / 255;
			dst[0] = (dst[0] * (255 - alpha) + ((fg_color >> 24) & 0xff) * alpha) / 255;
			dst[1] = (dst[1] * (255 - alpha) + ((fg_color >> 16) & 0xff) * alpha) / 255;
			dst[2] = (dst[2] * (255 - alpha) + ((fg_color >> 8) & 0xff) * alpha) / 255;
			dst += 3;
			src += 1;
		}
		dst += buf->buf_rowstride - w*3;
		src += text->priv->bitmap.pitch - w;
	}
	
	buf->is_bg = 0;
	return;
}

/* Point handler for the text item */
static double
gnome_canvas_text_point (GnomeCanvasItem *item, double x, double y,
			 int cx, int cy, GnomeCanvasItem **actual_item)
{
	GnomeCanvasText *text;
	PangoLayoutIter *iter;
	int x1, y1, x2, y2;
	int dx, dy;
	double dist, best;

	text = GNOME_CANVAS_TEXT (item);

	*actual_item = item;

	/* The idea is to build bounding rectangles for each of the lines of
	 * text (clipped by the clipping rectangle, if it is activated) and see
	 * whether the point is inside any of these.  If it is, we are done.
	 * Otherwise, calculate the distance to the nearest rectangle.
	 */

	best = 1.0e36;

	iter = pango_layout_get_iter (text->layout);
	do {
 	        PangoRectangle log_rect;

		pango_layout_iter_get_line_extents (iter, NULL, &log_rect);
				
		x1 = text->cx + PANGO_PIXELS (log_rect.x);
		y1 = text->cy + PANGO_PIXELS (log_rect.y);
		x2 = x1 + PANGO_PIXELS (log_rect.width);
		y2 = y1 + PANGO_PIXELS (log_rect.height);

		if (text->clip) {
			if (x1 < text->clip_cx)
				x1 = text->clip_cx;

			if (y1 < text->clip_cy)
				y1 = text->clip_cy;

			if (x2 > (text->clip_cx + text->clip_width))
				x2 = text->clip_cx + text->clip_width;

			if (y2 > (text->clip_cy + text->clip_height))
				y2 = text->clip_cy + text->clip_height;

			if ((x1 >= x2) || (y1 >= y2))
				continue;
		}

		/* Calculate distance from point to rectangle */

		if (cx < x1)
			dx = x1 - cx;
		else if (cx >= x2)
			dx = cx - x2 + 1;
		else
			dx = 0;

		if (cy < y1)
			dy = y1 - cy;
		else if (cy >= y2)
			dy = cy - y2 + 1;
		else
			dy = 0;

		if ((dx == 0) && (dy == 0)) {
			pango_layout_iter_free(iter);
			return 0.0;
		}

		dist = sqrt (dx * dx + dy * dy);
		if (dist < best)
			best = dist;
		
	} while (pango_layout_iter_next_line(iter));

	pango_layout_iter_free(iter);
	
	return best / item->canvas->pixels_per_unit;
}

/* Bounds handler for the text item */
static void
gnome_canvas_text_bounds (GnomeCanvasItem *item, double *x1, double *y1, double *x2, double *y2)
{
	GnomeCanvasText *text;
	double width, height;

	text = GNOME_CANVAS_TEXT (item);

	*x1 = text->x;
	*y1 = text->y;

	if (text->clip) {
		width = text->clip_width;
		height = text->clip_height;
	} else {
		width = text->max_width / item->canvas->pixels_per_unit;
		height = text->height / item->canvas->pixels_per_unit;
	}

	switch (text->anchor) {
	case GTK_ANCHOR_NW:
	case GTK_ANCHOR_W:
	case GTK_ANCHOR_SW:
		break;

	case GTK_ANCHOR_N:
	case GTK_ANCHOR_CENTER:
	case GTK_ANCHOR_S:
		*x1 -= width / 2.0;
		break;

	case GTK_ANCHOR_NE:
	case GTK_ANCHOR_E:
	case GTK_ANCHOR_SE:
		*x1 -= width;
		break;

	default:
		break;
	}

	switch (text->anchor) {
	case GTK_ANCHOR_NW:
	case GTK_ANCHOR_N:
	case GTK_ANCHOR_NE:
		break;

	case GTK_ANCHOR_W:
	case GTK_ANCHOR_CENTER:
	case GTK_ANCHOR_E:
		*y1 -= height / 2.0;
		break;

	case GTK_ANCHOR_SW:
	case GTK_ANCHOR_S:
	case GTK_ANCHOR_SE:
		*y1 -= height;
		break;

	default:
		break;
	}

	*x2 = *x1 + width;
	*y2 = *y1 + height;	
}
