/* Editable GnomeCanvas text item based on GtkTextLayout, borrowed heavily
 * from GtkTextView.
 *
 * Copyright (c) 2000 Red Hat, Inc.
 * Copyright (c) 2001 Joe Shaw
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#include <math.h>
#include <stdio.h>
#include <string.h>

#include <gdk/gdkkeysyms.h>
#include <gtk/gtk.h>
#define GTK_TEXT_USE_INTERNAL_UNSUPPORTED_API
#include <gtk/gtktextdisplay.h>

#include "gnome-canvas.h"
#include "gnome-canvas-util.h"
#include "gnome-canvas-rich-text.h"
#include "gnome-canvas-i18n.h"

struct _GnomeCanvasRichTextPrivate {
	GtkTextLayout *layout;
	GtkTextBuffer *buffer;

	char *text;

	/* Position at anchor */
	double x, y;
	/* Dimensions */
	double width, height;
	/* Top-left canvas coordinates for text */
	int cx, cy;

	gboolean cursor_visible;
	gboolean cursor_blink;
	gboolean editable;
	gboolean visible;
	gboolean grow_height;
	GtkWrapMode wrap_mode;
	GtkJustification justification;
	GtkTextDirection direction;
	GtkAnchorType anchor;
	int pixels_above_lines;
	int pixels_below_lines;
	int pixels_inside_wrap;
	int left_margin;
	int right_margin;
	int indent;

	guint preblink_timeout;
	guint blink_timeout;

	guint selection_drag_handler;

	gint drag_start_x;
	gint drag_start_y;

	gboolean just_selected_element;

	int clicks;
	guint click_timeout;
};

enum {
	PROP_0,
	PROP_TEXT,
	PROP_X,
	PROP_Y,
	PROP_WIDTH,
	PROP_HEIGHT,
	PROP_EDITABLE,
	PROP_VISIBLE,
	PROP_CURSOR_VISIBLE,
	PROP_CURSOR_BLINK,
	PROP_GROW_HEIGHT,
	PROP_WRAP_MODE,
	PROP_JUSTIFICATION,
	PROP_DIRECTION,
	PROP_ANCHOR,
	PROP_PIXELS_ABOVE_LINES,
	PROP_PIXELS_BELOW_LINES,
	PROP_PIXELS_INSIDE_WRAP,
	PROP_LEFT_MARGIN,
	PROP_RIGHT_MARGIN,
	PROP_INDENT
};

enum {
	TAG_CHANGED,
	LAST_SIGNAL
};

static GnomeCanvasItemClass *parent_class;
static guint signals[LAST_SIGNAL] = { 0 };

static void gnome_canvas_rich_text_class_init(GnomeCanvasRichTextClass *klass);
static void gnome_canvas_rich_text_init(GnomeCanvasRichText *text);
static void gnome_canvas_rich_text_set_property(GObject *object, guint property_id,
						const GValue *value, GParamSpec *pspec);
static void gnome_canvas_rich_text_get_property(GObject *object, guint property_id,
						GValue *value, GParamSpec *pspec);
static void gnome_canvas_rich_text_update(GnomeCanvasItem *item, double *affine,
					  ArtSVP *clip_path, int flags);
static void gnome_canvas_rich_text_realize(GnomeCanvasItem *item);
static void gnome_canvas_rich_text_unrealize(GnomeCanvasItem *item);
static double gnome_canvas_rich_text_point(GnomeCanvasItem *item, 
					   double x, double y,
					   int cx, int cy, 
					   GnomeCanvasItem **actual_item);
static void gnome_canvas_rich_text_draw(GnomeCanvasItem *item, 
					GdkDrawable *drawable,
					int x, int y, int width, int height);
static void gnome_canvas_rich_text_render(GnomeCanvasItem *item,
					  GnomeCanvasBuf *buf);
static gint gnome_canvas_rich_text_event(GnomeCanvasItem *item, 
					 GdkEvent *event);
static void gnome_canvas_rich_text_get_bounds(GnomeCanvasItem *text, double *px1, double *py1,
	   double *px2, double *py2);

static void gnome_canvas_rich_text_ensure_layout(GnomeCanvasRichText *text);
static void gnome_canvas_rich_text_destroy_layout(GnomeCanvasRichText *text);
static void gnome_canvas_rich_text_start_cursor_blink(GnomeCanvasRichText *text, gboolean delay);
static void gnome_canvas_rich_text_stop_cursor_blink(GnomeCanvasRichText *text);
static void gnome_canvas_rich_text_move_cursor(GnomeCanvasRichText *text,
					       GtkMovementStep step,
					       gint count,
					       gboolean extend_selection);



static GtkTextBuffer *get_buffer(GnomeCanvasRichText *text);
static gint blink_cb(gpointer data);

#define PREBLINK_TIME 300
#define CURSOR_ON_TIME 800
#define CURSOR_OFF_TIME 400

GType
gnome_canvas_rich_text_get_type(void)
{
	static GType rich_text_type;

	if (!rich_text_type) {
		const GTypeInfo object_info = {
			sizeof (GnomeCanvasRichTextClass),
			(GBaseInitFunc) NULL,
			(GBaseFinalizeFunc) NULL,
			(GClassInitFunc) gnome_canvas_rich_text_class_init,
			(GClassFinalizeFunc) NULL,
			NULL,			/* class_data */
			sizeof (GnomeCanvasRichText),
			0,			/* n_preallocs */
			(GInstanceInitFunc) gnome_canvas_rich_text_init,
			NULL			/* value_table */
		};

		rich_text_type = g_type_register_static (GNOME_TYPE_CANVAS_ITEM, "GnomeCanvasRichText",
							 &object_info, 0);
	}

	return rich_text_type;
}

static void
gnome_canvas_rich_text_finalize(GObject *object)
{
	GnomeCanvasRichText *text;

	text = GNOME_CANVAS_RICH_TEXT(object);

	g_free (text->_priv);
	text->_priv = NULL;

	if (G_OBJECT_CLASS (parent_class)->finalize)
		G_OBJECT_CLASS (parent_class)->finalize (object);
}

static void
gnome_canvas_rich_text_class_init(GnomeCanvasRichTextClass *klass)
{
	GObjectClass *gobject_class = G_OBJECT_CLASS(klass);
	GtkObjectClass *object_class = GTK_OBJECT_CLASS(klass);
	GnomeCanvasItemClass *item_class = GNOME_CANVAS_ITEM_CLASS(klass);
	
	parent_class = g_type_class_peek_parent (klass);

	gobject_class->set_property = gnome_canvas_rich_text_set_property;
	gobject_class->get_property = gnome_canvas_rich_text_get_property;

	g_object_class_install_property (
		gobject_class,
		PROP_TEXT,
		g_param_spec_string ("text",
				     _("Text"),
				     _("Text to display"),
				     NULL,
				     G_PARAM_READWRITE));
	g_object_class_install_property (
		gobject_class,
		PROP_X,
		g_param_spec_double ("x",
				     _("X"),
				     _("X position"),
				     -G_MAXDOUBLE, G_MAXDOUBLE, 0.0,
				     G_PARAM_READWRITE));
	g_object_class_install_property (
		gobject_class,
		PROP_Y,
		g_param_spec_double ("y",
				     _("Y"),
				     _("Y position"),
				     -G_MAXDOUBLE, G_MAXDOUBLE, 0.0,
				     G_PARAM_READWRITE));
	g_object_class_install_property (
		gobject_class,
		PROP_WIDTH,
		g_param_spec_double ("width",
				     _("Width"),
				     _("Width for text box"),
				     -G_MAXDOUBLE, G_MAXDOUBLE, 0.0,
				     G_PARAM_READWRITE));
	g_object_class_install_property (
		gobject_class,
		PROP_HEIGHT,
		g_param_spec_double ("height",
				     _("Height"),
				     _("Height for text box"),
				     -G_MAXDOUBLE, G_MAXDOUBLE, 0.0,
				     G_PARAM_READWRITE));
	g_object_class_install_property (
		gobject_class,
		PROP_EDITABLE,
		g_param_spec_boolean ("editable",
				      _("Editable"),
				      _("Is this rich text item editable?"),
				      TRUE,
				      G_PARAM_READWRITE));
	g_object_class_install_property (
		gobject_class,
		PROP_VISIBLE,
		g_param_spec_boolean ("visible",
				      _("Visible"),
				      _("Is this rich text item visible?"),
				      TRUE,
				      G_PARAM_READWRITE));
	g_object_class_install_property (
		gobject_class,
		PROP_CURSOR_VISIBLE,
		g_param_spec_boolean ("cursor_visible",
				      _("Cursor Visible"),
				      _("Is the cursor visible in this rich text item?"),
				      TRUE,
				      G_PARAM_READWRITE));
	g_object_class_install_property (
		gobject_class,
		PROP_CURSOR_BLINK,
		g_param_spec_boolean ("cursor_blink",
				      _("Cursor Blink"),
				      _("Does the cursor blink in this rich text item?"),
				      TRUE,
				      G_PARAM_READWRITE));
	g_object_class_install_property (
		gobject_class,
		PROP_GROW_HEIGHT,
		g_param_spec_boolean ("grow_height",
				      _("Grow Height"),
				      _("Should the text box height grow if the text does not fit?"),
				      FALSE,
				      G_PARAM_READWRITE));
	g_object_class_install_property (
		gobject_class,
		PROP_WRAP_MODE,
		g_param_spec_enum ("wrap_mode",
				   _("Wrap Mode"),
				   _("Wrap mode for multiline text"),
				   GTK_TYPE_WRAP_MODE,
				   GTK_WRAP_WORD,
				   G_PARAM_READWRITE));
	g_object_class_install_property (
		gobject_class,
		PROP_JUSTIFICATION,
		g_param_spec_enum ("justification",
				   _("Justification"),
				   _("Justification mode"),
				   GTK_TYPE_JUSTIFICATION,
				   GTK_JUSTIFY_LEFT,
				   G_PARAM_READWRITE));
	g_object_class_install_property (
		gobject_class,
		PROP_DIRECTION,
		g_param_spec_enum ("direction",
				   _("Direction"),
				   _("Text direction"),
				   GTK_TYPE_DIRECTION_TYPE,
				   gtk_widget_get_default_direction (),
				   G_PARAM_READWRITE));
	g_object_class_install_property (
		gobject_class,
		PROP_ANCHOR,
		g_param_spec_enum ("anchor",
				   _("Anchor"),
				   _("Anchor point for text"),
				   GTK_TYPE_ANCHOR_TYPE,
				   GTK_ANCHOR_NW,
				   G_PARAM_READWRITE));
	g_object_class_install_property (
		gobject_class,
		PROP_PIXELS_ABOVE_LINES,
		g_param_spec_int ("pixels_above_lines",
				  _("Pixels Above Lines"),
				  _("Number of pixels to put above lines"),
				  G_MININT, G_MAXINT,
				  0,
				  G_PARAM_READWRITE));
	g_object_class_install_property (
		gobject_class,
		PROP_PIXELS_BELOW_LINES,
		g_param_spec_int ("pixels_below_lines",
				  _("Pixels Below Lines"),
				  _("Number of pixels to put below lines"),
				  G_MININT, G_MAXINT,
				  0,
				  G_PARAM_READWRITE));
	g_object_class_install_property (
		gobject_class,
		PROP_PIXELS_INSIDE_WRAP,
		g_param_spec_int ("pixels_inside_wrap",
				  _("Pixels Inside Wrap"),
				  _("Number of pixels to put inside the wrap"),
				  G_MININT, G_MAXINT,
				  0,
				  G_PARAM_READWRITE));
	g_object_class_install_property (
		gobject_class,
		PROP_LEFT_MARGIN,
		g_param_spec_int ("left_margin",
				  _("Left Margin"),
				  _("Number of pixels in the left margin"),
				  G_MININT, G_MAXINT,
				  0,
				  G_PARAM_READWRITE));
	g_object_class_install_property (
		gobject_class,
		PROP_RIGHT_MARGIN,
		g_param_spec_int ("right_margin",
				  _("Right Margin"),
				  _("Number of pixels in the right margin"),
				  G_MININT, G_MAXINT,
				  0,
				  G_PARAM_READWRITE));
	g_object_class_install_property (
		gobject_class,
		PROP_INDENT,
		g_param_spec_int ("indent",
				  _("Indentation"),
				  _("Number of pixels for indentation"),
				  G_MININT, G_MAXINT,
				  0,
				  G_PARAM_READWRITE));

	/* Signals */
	signals[TAG_CHANGED] = g_signal_new(
		"tag_changed",
		G_TYPE_FROM_CLASS(object_class),
		G_SIGNAL_RUN_LAST,
		G_STRUCT_OFFSET(GnomeCanvasRichTextClass, tag_changed),
		NULL, NULL,
		g_cclosure_marshal_VOID__OBJECT,
		G_TYPE_NONE, 1,
		G_TYPE_OBJECT);

	gobject_class->finalize = gnome_canvas_rich_text_finalize;

	item_class->update = gnome_canvas_rich_text_update;
	item_class->realize = gnome_canvas_rich_text_realize;
	item_class->unrealize = gnome_canvas_rich_text_unrealize;
	item_class->draw = gnome_canvas_rich_text_draw;
	item_class->point = gnome_canvas_rich_text_point;
	item_class->render = gnome_canvas_rich_text_render;
	item_class->event = gnome_canvas_rich_text_event;
	item_class->bounds = gnome_canvas_rich_text_get_bounds;
} /* gnome_canvas_rich_text_class_init */

static void
gnome_canvas_rich_text_init(GnomeCanvasRichText *text)
{
#if 0
	GtkObject *object = GTK_OBJECT(text);

	object->flags |= GNOME_CANVAS_ITEM_ALWAYS_REDRAW;
#endif
	text->_priv = g_new0(GnomeCanvasRichTextPrivate, 1);

	/* Try to set some sane defaults */
	text->_priv->cursor_visible = TRUE;
	text->_priv->cursor_blink = TRUE;
	text->_priv->editable = TRUE;
	text->_priv->visible = TRUE;
	text->_priv->grow_height = FALSE;
	text->_priv->wrap_mode = GTK_WRAP_WORD;
	text->_priv->justification = GTK_JUSTIFY_LEFT;
	text->_priv->direction = gtk_widget_get_default_direction();
	text->_priv->anchor = GTK_ANCHOR_NW;
	
	text->_priv->blink_timeout = 0;
	text->_priv->preblink_timeout = 0;

	text->_priv->clicks = 0;
	text->_priv->click_timeout = 0;
} /* gnome_canvas_rich_text_init */

static void
gnome_canvas_rich_text_set_property (GObject *object, guint property_id,
				     const GValue *value, GParamSpec *pspec)
{
	GnomeCanvasRichText *text = GNOME_CANVAS_RICH_TEXT(object);

	switch (property_id) {
	case PROP_TEXT:
		if (text->_priv->text)
			g_free(text->_priv->text);

		text->_priv->text = g_value_dup_string (value);

		gtk_text_buffer_set_text(
			get_buffer(text), text->_priv->text, strlen(text->_priv->text));

		break;
	case PROP_X:
		text->_priv->x = g_value_get_double (value);
		break;
	case PROP_Y:
		text->_priv->y = g_value_get_double (value);
		break;
	case PROP_WIDTH:
		text->_priv->width = g_value_get_double (value);
		break;
	case PROP_HEIGHT:
		text->_priv->height = g_value_get_double (value);
		break;
	case PROP_EDITABLE:
		text->_priv->editable = g_value_get_boolean (value);
		if (text->_priv->layout) {
			text->_priv->layout->default_style->editable =
				text->_priv->editable;
			gtk_text_layout_default_style_changed(text->_priv->layout);
		}
		break;
	case PROP_VISIBLE:
		text->_priv->visible = g_value_get_boolean (value);
		if (text->_priv->layout) {
			text->_priv->layout->default_style->invisible =
				!text->_priv->visible;
			gtk_text_layout_default_style_changed(text->_priv->layout);
		}
		break;
	case PROP_CURSOR_VISIBLE:
		text->_priv->cursor_visible = g_value_get_boolean (value);
		if (text->_priv->layout) {
			gtk_text_layout_set_cursor_visible(
				text->_priv->layout, text->_priv->cursor_visible);

			if (text->_priv->cursor_visible && text->_priv->cursor_blink) {
				gnome_canvas_rich_text_start_cursor_blink(
					text, FALSE);
			}
			else
				gnome_canvas_rich_text_stop_cursor_blink(text);
		}
		break;
	case PROP_CURSOR_BLINK:
		text->_priv->cursor_blink = g_value_get_boolean (value);
		if (text->_priv->layout && text->_priv->cursor_visible) {
			if (text->_priv->cursor_blink && !text->_priv->blink_timeout) {
				gnome_canvas_rich_text_start_cursor_blink(
					text, FALSE);
			}
			else if (!text->_priv->cursor_blink && text->_priv->blink_timeout) {
				gnome_canvas_rich_text_stop_cursor_blink(text);
				gtk_text_layout_set_cursor_visible(
					text->_priv->layout, TRUE);
			}
		}
		break;
	case PROP_GROW_HEIGHT:
		text->_priv->grow_height = g_value_get_boolean (value);
		/* FIXME: Recalc here */
		break;
	case PROP_WRAP_MODE:
		text->_priv->wrap_mode = g_value_get_enum (value);

		if (text->_priv->layout) {
			text->_priv->layout->default_style->wrap_mode = 
				text->_priv->wrap_mode;
			gtk_text_layout_default_style_changed(text->_priv->layout);
		}
		break;
	case PROP_JUSTIFICATION:
		text->_priv->justification = g_value_get_enum (value);

		if (text->_priv->layout) {
			text->_priv->layout->default_style->justification =
				text->_priv->justification;
			gtk_text_layout_default_style_changed(text->_priv->layout);
		}
		break;
	case PROP_DIRECTION:
		text->_priv->direction = g_value_get_enum (value);

		if (text->_priv->layout) {
			text->_priv->layout->default_style->direction =
				text->_priv->direction;
			gtk_text_layout_default_style_changed(text->_priv->layout);
		}
		break;
	case PROP_ANCHOR:
		text->_priv->anchor = g_value_get_enum (value);
		break;
	case PROP_PIXELS_ABOVE_LINES:
		text->_priv->pixels_above_lines = g_value_get_int (value);
		
		if (text->_priv->layout) {
			text->_priv->layout->default_style->pixels_above_lines =
				text->_priv->pixels_above_lines;
			gtk_text_layout_default_style_changed(text->_priv->layout);
		}
		break;
	case PROP_PIXELS_BELOW_LINES:
		text->_priv->pixels_below_lines = g_value_get_int (value);
		
		if (text->_priv->layout) {
			text->_priv->layout->default_style->pixels_below_lines =
				text->_priv->pixels_below_lines;
			gtk_text_layout_default_style_changed(text->_priv->layout);
		}
		break;
	case PROP_PIXELS_INSIDE_WRAP:
		text->_priv->pixels_inside_wrap = g_value_get_int (value);
		
		if (text->_priv->layout) {
			text->_priv->layout->default_style->pixels_inside_wrap =
				text->_priv->pixels_inside_wrap;
			gtk_text_layout_default_style_changed(text->_priv->layout);
		}
		break;
	case PROP_LEFT_MARGIN:
		text->_priv->left_margin = g_value_get_int (value);
		
		if (text->_priv->layout) {
			text->_priv->layout->default_style->left_margin =
				text->_priv->left_margin;
			gtk_text_layout_default_style_changed(text->_priv->layout);
		}
		break;
	case PROP_RIGHT_MARGIN:
		text->_priv->right_margin = g_value_get_int (value);
		
		if (text->_priv->layout) {
			text->_priv->layout->default_style->right_margin =
				text->_priv->right_margin;
			gtk_text_layout_default_style_changed(text->_priv->layout);
		}
		break;
	case PROP_INDENT:
		text->_priv->pixels_above_lines = g_value_get_int (value);
		
		if (text->_priv->layout) {
			text->_priv->layout->default_style->indent = text->_priv->indent;
			gtk_text_layout_default_style_changed(text->_priv->layout);
		}
		break;
		       
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
		break;
	}

	gnome_canvas_item_request_update(GNOME_CANVAS_ITEM(text));
}

static void
gnome_canvas_rich_text_get_property (GObject *object, guint property_id,
				     GValue *value, GParamSpec *pspec)
{
	GnomeCanvasRichText *text = GNOME_CANVAS_RICH_TEXT(object);

	switch (property_id) {
	case PROP_TEXT:
		g_value_set_string (value, text->_priv->text);
		break;
	case PROP_X:
		g_value_set_double (value, text->_priv->x);
		break;
	case PROP_Y:
		g_value_set_double (value, text->_priv->y);
		break;
	case PROP_HEIGHT:
		g_value_set_double (value, text->_priv->height);
		break;
	case PROP_WIDTH:
		g_value_set_double (value, text->_priv->width);
		break;
	case PROP_EDITABLE:
		g_value_set_boolean (value, text->_priv->editable);
		break;
	case PROP_CURSOR_VISIBLE:
		g_value_set_boolean (value, text->_priv->cursor_visible);
		break;
	case PROP_CURSOR_BLINK:
		g_value_set_boolean (value, text->_priv->cursor_blink);
		break;
	case PROP_GROW_HEIGHT:
		g_value_set_boolean (value, text->_priv->grow_height);
		break;
	case PROP_WRAP_MODE:
		g_value_set_enum (value, text->_priv->wrap_mode);
		break;
	case PROP_JUSTIFICATION:
		g_value_set_enum (value, text->_priv->justification);
		break;
	case PROP_DIRECTION:
		g_value_set_enum (value, text->_priv->direction);
		break;
	case PROP_ANCHOR:
		g_value_set_enum (value, text->_priv->anchor);
		break;
	case PROP_PIXELS_ABOVE_LINES:
		g_value_set_enum (value, text->_priv->pixels_above_lines);
		break;
	case PROP_PIXELS_BELOW_LINES:
		g_value_set_int (value, text->_priv->pixels_below_lines);
		break;
	case PROP_PIXELS_INSIDE_WRAP:
		g_value_set_int (value, text->_priv->pixels_inside_wrap);
		break;
	case PROP_LEFT_MARGIN:
		g_value_set_int (value, text->_priv->left_margin);
		break;
	case PROP_RIGHT_MARGIN:
		g_value_set_int (value, text->_priv->right_margin);
		break;
	case PROP_INDENT:
		g_value_set_int (value, text->_priv->indent);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
		break;
	}
}

static void
gnome_canvas_rich_text_realize(GnomeCanvasItem *item)
{
	GnomeCanvasRichText *text = GNOME_CANVAS_RICH_TEXT(item);

	(* GNOME_CANVAS_ITEM_CLASS(parent_class)->realize)(item);

	gnome_canvas_rich_text_ensure_layout(text);
} /* gnome_canvas_rich_text_realize */

static void
gnome_canvas_rich_text_unrealize(GnomeCanvasItem *item)
{
	GnomeCanvasRichText *text = GNOME_CANVAS_RICH_TEXT(item);

	gnome_canvas_rich_text_destroy_layout(text);

	(* GNOME_CANVAS_ITEM_CLASS(parent_class)->unrealize)(item);
} /* gnome_canvas_rich_text_unrealize */

static void
gnome_canvas_rich_text_move_iter_by_lines(GnomeCanvasRichText *text,
					  GtkTextIter *newplace, gint count)
{
	while (count < 0) {
		gtk_text_layout_move_iter_to_previous_line(
			text->_priv->layout, newplace);
		count++;
	}

	while (count > 0) {
		gtk_text_layout_move_iter_to_next_line(
			text->_priv->layout, newplace);
		count--;
	}
} /* gnome_canvas_rich_text_move_iter_by_lines */

static gint
gnome_canvas_rich_text_get_cursor_x_position(GnomeCanvasRichText *text)
{
	GtkTextIter insert;
	GdkRectangle rect;

	gtk_text_buffer_get_iter_at_mark(
		get_buffer(text), &insert,
		gtk_text_buffer_get_mark(get_buffer(text), "insert"));
	gtk_text_layout_get_cursor_locations(
		text->_priv->layout, &insert, &rect, NULL);

	return rect.x;
} /* gnome_canvas_rich_text_get_cursor_x_position */

static void
gnome_canvas_rich_text_move_cursor(GnomeCanvasRichText *text,
				   GtkMovementStep step,
				   gint count, gboolean extend_selection)
{
	GtkTextIter insert, newplace;

	gtk_text_buffer_get_iter_at_mark(
		get_buffer(text), &insert, 
		gtk_text_buffer_get_mark(get_buffer(text), "insert"));

	newplace = insert;

	switch (step) {
	case GTK_MOVEMENT_LOGICAL_POSITIONS:
		gtk_text_iter_forward_cursor_positions(&newplace, count);
		break;
	case GTK_MOVEMENT_VISUAL_POSITIONS:
		gtk_text_layout_move_iter_visually(
			text->_priv->layout, &newplace, count);
		break;
	case GTK_MOVEMENT_WORDS:
		if (count < 0)
			gtk_text_iter_backward_word_starts(&newplace, -count);
		else if (count > 0)
			gtk_text_iter_forward_word_ends(&newplace, count);
		break;
	case GTK_MOVEMENT_DISPLAY_LINES:
		gnome_canvas_rich_text_move_iter_by_lines(
			text, &newplace, count);
		gtk_text_layout_move_iter_to_x(
			text->_priv->layout, &newplace, 
			gnome_canvas_rich_text_get_cursor_x_position(text));
		break;
	case GTK_MOVEMENT_DISPLAY_LINE_ENDS:
		if (count > 1) {
			gnome_canvas_rich_text_move_iter_by_lines(
				text, &newplace, --count);
		}
		else if (count < -1) {
			gnome_canvas_rich_text_move_iter_by_lines(
				text, &newplace, ++count);
		}
	       
		if (count != 0) {
			gtk_text_layout_move_iter_to_line_end(
				text->_priv->layout, &newplace, count);
		}
		break;
	case GTK_MOVEMENT_PARAGRAPHS:
		/* FIXME: Busted in gtktextview.c too */
		break;
	case GTK_MOVEMENT_PARAGRAPH_ENDS:
		if (count > 0)
			gtk_text_iter_forward_to_line_end(&newplace);
		else if (count < 0)
			gtk_text_iter_set_line_offset(&newplace, 0);
		break;
	case GTK_MOVEMENT_BUFFER_ENDS:
		if (count > 0) {
			gtk_text_buffer_get_end_iter(
				get_buffer(text), &newplace);
		}
		else if (count < 0) {
			gtk_text_buffer_get_iter_at_offset(
				get_buffer(text), &newplace, 0);
		}
		break;
	default:
		break;
	}

	if (!gtk_text_iter_equal(&insert, &newplace)) {
		if (extend_selection) {
			gtk_text_buffer_move_mark(
				get_buffer(text),
				gtk_text_buffer_get_mark(
					get_buffer(text), "insert"),
				&newplace);
		}
		else {
			gtk_text_buffer_place_cursor(
				get_buffer(text), &newplace);
		}
	}

	gnome_canvas_rich_text_start_cursor_blink(text, TRUE);
} /* gnome_canvas_rich_text_move_cursor */

static gboolean
whitespace(gunichar ch, gpointer ignored)
{
	return (ch == ' ' || ch == '\t');
} /* whitespace */

static gboolean
not_whitespace(gunichar ch, gpointer ignored)
{
	return !whitespace(ch, ignored);
} /* not_whitespace */

static gboolean
find_whitespace_region(const GtkTextIter *center,
		       GtkTextIter *start, GtkTextIter *end)
{
	*start = *center;
	*end = *center;

	if (gtk_text_iter_backward_find_char(start, not_whitespace, NULL, NULL))
		gtk_text_iter_forward_char(start);
	if (whitespace(gtk_text_iter_get_char(end), NULL))
		gtk_text_iter_forward_find_char(end, not_whitespace, NULL, NULL);

	return !gtk_text_iter_equal(start, end);
} /* find_whitespace_region */

static void
gnome_canvas_rich_text_delete_from_cursor(GnomeCanvasRichText *text,
					  GtkDeleteType type,
					  gint count)
{
	GtkTextIter insert, start, end;

	/* Special case: If the user wants to delete a character and there is
	   a selection, then delete the selection and return */
	if (type == GTK_DELETE_CHARS) {
		if (gtk_text_buffer_delete_selection(get_buffer(text), TRUE, 
						     text->_priv->editable))
			return;
	}

	gtk_text_buffer_get_iter_at_mark(
		get_buffer(text), &insert,
		gtk_text_buffer_get_mark(get_buffer(text), "insert"));

	start = insert;
	end = insert;

	switch (type) {
	case GTK_DELETE_CHARS:
		gtk_text_iter_forward_cursor_positions(&end, count);
		break;
	case GTK_DELETE_WORD_ENDS:
		if (count > 0)
			gtk_text_iter_forward_word_ends(&end, count);
		else if (count < 0)
			gtk_text_iter_backward_word_starts(&start, -count);
		break;
	case GTK_DELETE_WORDS:
		break;
	case GTK_DELETE_DISPLAY_LINE_ENDS:
		break;
	case GTK_DELETE_PARAGRAPH_ENDS:
		if (gtk_text_iter_ends_line(&end)) {
			gtk_text_iter_forward_line(&end);
			--count;
		}

		while (count > 0) {
			if (!gtk_text_iter_forward_to_line_end(&end))
				break;

			--count;
		}
		break;
	case GTK_DELETE_PARAGRAPHS:
		if (count > 0) {
			gtk_text_iter_set_line_offset(&start, 0);
			gtk_text_iter_forward_to_line_end(&end);

			/* Do the lines beyond the first. */
			while (count > 1) {
				gtk_text_iter_forward_to_line_end(&end);
				--count;
			}
		}
		break;
	case GTK_DELETE_WHITESPACE:
		find_whitespace_region(&insert, &start, &end);
		break;

	default:
		break;
	}

	if (!gtk_text_iter_equal(&start, &end)) {
		gtk_text_buffer_begin_user_action(get_buffer(text));
		gtk_text_buffer_delete_interactive(
			get_buffer(text), &start, &end, text->_priv->editable);
		gtk_text_buffer_end_user_action(get_buffer(text));
	}
} /* gnome_canvas_rich_text_delete_from_cursor */

static gint
selection_motion_event_handler(GnomeCanvasRichText *text, GdkEvent *event,
			       gpointer ignored)
{
	GtkTextIter newplace;
	GtkTextMark *mark;
	double newx, newy;

	/* We only want to handle motion events... */
	if (event->type != GDK_MOTION_NOTIFY)
		return FALSE;

	newx = (event->motion.x - text->_priv->x) * 
		GNOME_CANVAS_ITEM(text)->canvas->pixels_per_unit;
	newy = (event->motion.y - text->_priv->y) * 
		GNOME_CANVAS_ITEM(text)->canvas->pixels_per_unit;

	gtk_text_layout_get_iter_at_pixel(text->_priv->layout, &newplace, newx, newy);
	mark = gtk_text_buffer_get_mark(get_buffer(text), "insert");
	gtk_text_buffer_move_mark(get_buffer(text), mark, &newplace);

	return TRUE;
} /* selection_motion_event_handler */

static void
gnome_canvas_rich_text_start_selection_drag(GnomeCanvasRichText *text,
					    const GtkTextIter *iter,
					    GdkEventButton * button)
{
	GtkTextIter newplace;

	g_return_if_fail(text->_priv->selection_drag_handler == 0);

#if 0
	gnome_canvas_item_grab_focus(GNOME_CANVAS_ITEM(text));
#endif

	newplace = *iter;

	gtk_text_buffer_place_cursor(get_buffer(text), &newplace);

	text->_priv->selection_drag_handler = g_signal_connect(
		text, "event",
		G_CALLBACK (selection_motion_event_handler),
		NULL);
} /* gnome_canvas_rich_text_start_selection_drag */

static gboolean
gnome_canvas_rich_text_end_selection_drag(GnomeCanvasRichText *text,
					  GdkEventButton * event)
{
	if (text->_priv->selection_drag_handler == 0)
		return FALSE;

	g_signal_handler_disconnect (text, text->_priv->selection_drag_handler);
	text->_priv->selection_drag_handler = 0;

#if 0
	gnome_canvas_item_grab(NULL);
#endif

	return TRUE;
} /* gnome_canvas_rich_text_end_selection_drag */

static void
gnome_canvas_rich_text_emit_tag_changed(GnomeCanvasRichText *text,
					GtkTextTag *tag)
{
	g_signal_emit(G_OBJECT(text), signals[TAG_CHANGED], 0, tag);
} /* gnome_canvas_rich_text_emit_tag_changed */
						
static gint
gnome_canvas_rich_text_key_press_event(GnomeCanvasItem *item, 
				       GdkEventKey *event)
{
	GnomeCanvasRichText *text = GNOME_CANVAS_RICH_TEXT(item);
	gboolean extend_selection = FALSE;
	gboolean handled = FALSE;

#if 0
	printf("Key press event\n");
#endif

	if (!text->_priv->layout || !text->_priv->buffer)
		return FALSE;

	if (event->state & GDK_SHIFT_MASK)
		extend_selection = TRUE;

	switch (event->keyval) {
	case GDK_Return:
	case GDK_KP_Enter:
		gtk_text_buffer_delete_selection(
			get_buffer(text), TRUE, text->_priv->editable);
		gtk_text_buffer_insert_interactive_at_cursor(
			get_buffer(text), "\n", 1, text->_priv->editable);
		handled = TRUE;
		break;

	case GDK_Tab:
		gtk_text_buffer_insert_interactive_at_cursor(
			get_buffer(text), "\t", 1, text->_priv->editable);
		handled = TRUE;
		break;

	/* MOVEMENT */
	case GDK_Right:
		if (event->state & GDK_CONTROL_MASK) {
			gnome_canvas_rich_text_move_cursor(
				text, GTK_MOVEMENT_WORDS, 1, 
				extend_selection);
			handled = TRUE;
		}
		else {
			gnome_canvas_rich_text_move_cursor(
				text, GTK_MOVEMENT_VISUAL_POSITIONS, 1,
				extend_selection);
			handled = TRUE;
		}
		break;
	case GDK_Left:
		if (event->state & GDK_CONTROL_MASK) {
			gnome_canvas_rich_text_move_cursor(
				text, GTK_MOVEMENT_WORDS, -1,
				extend_selection);
			handled = TRUE;
		}
		else {
			gnome_canvas_rich_text_move_cursor(
				text, GTK_MOVEMENT_VISUAL_POSITIONS, -1,
				extend_selection);
			handled = TRUE;
		}
		break;
	case GDK_f:
		if (event->state & GDK_CONTROL_MASK) {
			gnome_canvas_rich_text_move_cursor(
				text, GTK_MOVEMENT_LOGICAL_POSITIONS, 1,
				extend_selection);
			handled = TRUE;
		}
		else if (event->state & GDK_MOD1_MASK) {
			gnome_canvas_rich_text_move_cursor(
				text, GTK_MOVEMENT_WORDS, 1,
				extend_selection);
			handled = TRUE;
		}
		break;
	case GDK_b:
		if (event->state & GDK_CONTROL_MASK) {
			gnome_canvas_rich_text_move_cursor(
				text, GTK_MOVEMENT_LOGICAL_POSITIONS, -1,
				extend_selection);
			handled = TRUE;
		}
		else if (event->state & GDK_MOD1_MASK) {
			gnome_canvas_rich_text_move_cursor(
				text, GTK_MOVEMENT_WORDS, -1,
				extend_selection);
			handled = TRUE;
		}
		break;
	case GDK_Up:
		gnome_canvas_rich_text_move_cursor(
			text, GTK_MOVEMENT_DISPLAY_LINES, -1,
			extend_selection);
		handled = TRUE;
		break;
	case GDK_Down:
		gnome_canvas_rich_text_move_cursor(
			text, GTK_MOVEMENT_DISPLAY_LINES, 1,
			extend_selection);
		handled = TRUE;
		break;
	case GDK_p:
		if (event->state & GDK_CONTROL_MASK) {
			gnome_canvas_rich_text_move_cursor(
				text, GTK_MOVEMENT_DISPLAY_LINES, -1,
				extend_selection);
			handled = TRUE;
		}
		break;
	case GDK_n:
		if (event->state & GDK_CONTROL_MASK) {
			gnome_canvas_rich_text_move_cursor(
				text, GTK_MOVEMENT_DISPLAY_LINES, 1,
				extend_selection);
			handled = TRUE;
		}
		break;
	case GDK_Home:
		gnome_canvas_rich_text_move_cursor(
			text, GTK_MOVEMENT_PARAGRAPH_ENDS, -1,
			extend_selection);
		handled = TRUE;
		break;
	case GDK_End:
		gnome_canvas_rich_text_move_cursor(
			text, GTK_MOVEMENT_PARAGRAPH_ENDS, 1,
			extend_selection);
		handled = TRUE;
		break;
	case GDK_a:
		if (event->state & GDK_CONTROL_MASK) {
			gnome_canvas_rich_text_move_cursor(
				text, GTK_MOVEMENT_PARAGRAPH_ENDS, -1,
				extend_selection);
			handled = TRUE;
		}
		break;
	case GDK_e:
		if (event->state & GDK_CONTROL_MASK) {
			gnome_canvas_rich_text_move_cursor(
				text, GTK_MOVEMENT_PARAGRAPH_ENDS, 1,
				extend_selection);
			handled = TRUE;
		}
		break;

	/* DELETING TEXT */
	case GDK_Delete:
	case GDK_KP_Delete:
		if (event->state & GDK_CONTROL_MASK) {
			gnome_canvas_rich_text_delete_from_cursor(
				text, GTK_DELETE_WORD_ENDS, 1);
			handled = TRUE;
		}
		else {
			gnome_canvas_rich_text_delete_from_cursor(
				text, GTK_DELETE_CHARS, 1);
			handled = TRUE;
		}
		break;
	case GDK_d:
		if (event->state & GDK_CONTROL_MASK) {
			gnome_canvas_rich_text_delete_from_cursor(
				text, GTK_DELETE_CHARS, 1);
			handled = TRUE;
		}
		else if (event->state & GDK_MOD1_MASK) {
			gnome_canvas_rich_text_delete_from_cursor(
				text, GTK_DELETE_WORD_ENDS, 1);
			handled = TRUE;
		}
		break;
	case GDK_BackSpace:
		if (event->state & GDK_CONTROL_MASK) {
			gnome_canvas_rich_text_delete_from_cursor(
				text, GTK_DELETE_WORD_ENDS, -1);
			handled = TRUE;
		}
		else {
			gnome_canvas_rich_text_delete_from_cursor(
				text, GTK_DELETE_CHARS, -1);
		}
		handled = TRUE;
		break;
	case GDK_k:
		if (event->state & GDK_CONTROL_MASK) {
			gnome_canvas_rich_text_delete_from_cursor(
				text, GTK_DELETE_PARAGRAPH_ENDS, 1);
			handled = TRUE;
		}
		break;
	case GDK_u:
		if (event->state & GDK_CONTROL_MASK) {
			gnome_canvas_rich_text_delete_from_cursor(
				text, GTK_DELETE_PARAGRAPHS, 1);
			handled = TRUE;
		}
		break;
	case GDK_space:
		if (event->state & GDK_MOD1_MASK) {
			gnome_canvas_rich_text_delete_from_cursor(
				text, GTK_DELETE_WHITESPACE, 1);
			handled = TRUE;
		}
		break;
	case GDK_backslash:
		if (event->state & GDK_MOD1_MASK) {
			gnome_canvas_rich_text_delete_from_cursor(
				text, GTK_DELETE_WHITESPACE, 1);
			handled = TRUE;
		}
		break;
	default:
		break;
	}

	/* An empty string, click just pressing "Alt" by itself or whatever. */
	if (!event->length)
		return FALSE;

	if (!handled) {
		gtk_text_buffer_delete_selection(
			get_buffer(text), TRUE, text->_priv->editable);
		gtk_text_buffer_insert_interactive_at_cursor(
			get_buffer(text), event->string, event->length,
			text->_priv->editable);
	}

	gnome_canvas_rich_text_start_cursor_blink(text, TRUE);
	
	return TRUE;
} /* gnome_canvas_rich_text_key_press_event */

static gint
gnome_canvas_rich_text_key_release_event(GnomeCanvasItem * item, 
					 GdkEventKey *event)
{
	return FALSE;
} /* gnome_canvas_rich_text_key_release_event */

static gboolean
_click(gpointer data)
{
	GnomeCanvasRichText *text = GNOME_CANVAS_RICH_TEXT(data);

	text->_priv->clicks = 0;
	text->_priv->click_timeout = 0;

	return FALSE;
} /* _click */

static gint
gnome_canvas_rich_text_button_press_event(GnomeCanvasItem *item,
					  GdkEventButton *event)
{
	GnomeCanvasRichText *text = GNOME_CANVAS_RICH_TEXT(item);
	GtkTextIter iter;
	GdkEventType event_type;
	double newx, newy;

	newx = (event->x - text->_priv->x) * item->canvas->pixels_per_unit;
	newy = (event->y - text->_priv->y) * item->canvas->pixels_per_unit;
	
	gtk_text_layout_get_iter_at_pixel(text->_priv->layout, &iter, newx, newy);

	/* The canvas doesn't give us double- or triple-click events, so
	   we have to synthesize them ourselves. Yay. */
	event_type = event->type;
	if (event_type == GDK_BUTTON_PRESS) {
		text->_priv->clicks++;
		text->_priv->click_timeout = g_timeout_add(400, _click, text);

		if (text->_priv->clicks > 3)
			text->_priv->clicks = text->_priv->clicks % 3;

		if (text->_priv->clicks == 1)
			event_type = GDK_BUTTON_PRESS;
		else if (text->_priv->clicks == 2)
			event_type = GDK_2BUTTON_PRESS;
		else if (text->_priv->clicks == 3)
			event_type = GDK_3BUTTON_PRESS;
		else
			printf("ZERO CLICKS!\n");
	}

	if (event->button == 1 && event_type == GDK_BUTTON_PRESS) {
		GtkTextIter start, end;

		if (gtk_text_buffer_get_selection_bounds(
			    get_buffer(text), &start, &end) &&
		    gtk_text_iter_in_range(&iter, &start, &end)) {
			text->_priv->drag_start_x = event->x;
			text->_priv->drag_start_y = event->y;
		}
		else {
			gnome_canvas_rich_text_start_selection_drag(
				text, &iter, event);
		}

		return TRUE;
	}
	else if (event->button == 1 && event_type == GDK_2BUTTON_PRESS) {
		GtkTextIter start, end;

#if 0
		printf("double-click\n");
#endif
		
		gnome_canvas_rich_text_end_selection_drag(text, event);

		start = iter;
		end = start;

		if (gtk_text_iter_inside_word(&start)) {
			if (!gtk_text_iter_starts_word(&start))
				gtk_text_iter_backward_word_start(&start);
			
			if (!gtk_text_iter_ends_word(&end))
				gtk_text_iter_forward_word_end(&end);
		}

		gtk_text_buffer_move_mark(
			get_buffer(text),
			gtk_text_buffer_get_selection_bound(get_buffer(text)),
			&start);
		gtk_text_buffer_move_mark(
			get_buffer(text),
			gtk_text_buffer_get_insert(get_buffer(text)), &end);

		text->_priv->just_selected_element = TRUE;

		return TRUE;
	}
	else if (event->button == 1 && event_type == GDK_3BUTTON_PRESS) {
		GtkTextIter start, end;

#if 0
		printf("triple-click\n");
#endif

		gnome_canvas_rich_text_end_selection_drag(text, event);

		start = iter;
		end = start;

		if (gtk_text_layout_iter_starts_line(text->_priv->layout, &start)) {
			gtk_text_layout_move_iter_to_line_end(
				text->_priv->layout, &start, -1);
		}
		else {
			gtk_text_layout_move_iter_to_line_end(
				text->_priv->layout, &start, -1);

			if (!gtk_text_layout_iter_starts_line(
				    text->_priv->layout, &end)) {
				gtk_text_layout_move_iter_to_line_end(
					text->_priv->layout, &end, 1);
			}
		}

		gtk_text_buffer_move_mark(
			get_buffer(text),
			gtk_text_buffer_get_selection_bound(get_buffer(text)),
			&start);
		gtk_text_buffer_move_mark(
			get_buffer(text),
			gtk_text_buffer_get_insert(get_buffer(text)), &end);

		text->_priv->just_selected_element = TRUE;

		return TRUE;
	}
	else if (event->button == 2 && event_type == GDK_BUTTON_PRESS) {
		gtk_text_buffer_paste_clipboard(
			get_buffer(text),
			gtk_clipboard_get (GDK_SELECTION_PRIMARY),
			&iter, text->_priv->editable);
	}
		
	return FALSE;
} /* gnome_canvas_rich_text_button_press_event */

static gint
gnome_canvas_rich_text_button_release_event(GnomeCanvasItem *item,
					    GdkEventButton *event)
{
	GnomeCanvasRichText *text = GNOME_CANVAS_RICH_TEXT(item);
	double newx, newy;

	newx = (event->x - text->_priv->x) * item->canvas->pixels_per_unit;
	newy = (event->y - text->_priv->y) * item->canvas->pixels_per_unit;
	
	if (event->button == 1) {
		if (text->_priv->drag_start_x >= 0) {
			text->_priv->drag_start_x = -1;
			text->_priv->drag_start_y = -1;
		}

		if (gnome_canvas_rich_text_end_selection_drag(text, event))
			return TRUE;
		else if (text->_priv->just_selected_element) {
			text->_priv->just_selected_element = FALSE;
			return FALSE;
		}
		else {
			GtkTextIter iter;

			gtk_text_layout_get_iter_at_pixel(
				text->_priv->layout, &iter, newx, newy);

			gtk_text_buffer_place_cursor(get_buffer(text), &iter);

			return FALSE;
		}
	}

	return FALSE;
} /* gnome_canvas_rich_text_button_release_event */

static gint
gnome_canvas_rich_text_focus_in_event(GnomeCanvasItem *item,
				      GdkEventFocus * event)
{
	GnomeCanvasRichText *text = GNOME_CANVAS_RICH_TEXT(item);

	if (text->_priv->cursor_visible && text->_priv->layout) {
		gtk_text_layout_set_cursor_visible(text->_priv->layout, TRUE);
		gnome_canvas_rich_text_start_cursor_blink(text, FALSE);
	}

	return FALSE;
} /* gnome_canvas_rich_text_focus_in_event */

static gint
gnome_canvas_rich_text_focus_out_event(GnomeCanvasItem *item,
				       GdkEventFocus * event)
{
	GnomeCanvasRichText *text = GNOME_CANVAS_RICH_TEXT(item);

	if (text->_priv->cursor_visible && text->_priv->layout) {
		gtk_text_layout_set_cursor_visible(text->_priv->layout, FALSE);
		gnome_canvas_rich_text_stop_cursor_blink(text);
	}

	return FALSE;
} /* gnome_canvas_rich_text_focus_out_event */

static gboolean
get_event_coordinates(GdkEvent *event, gint *x, gint *y)
{
	g_return_val_if_fail(event, FALSE);
	
	switch (event->type) {
	case GDK_MOTION_NOTIFY:
		*x = event->motion.x;
		*y = event->motion.y;
		return TRUE;
	case GDK_BUTTON_PRESS:
	case GDK_2BUTTON_PRESS:
	case GDK_3BUTTON_PRESS:
	case GDK_BUTTON_RELEASE:
		*x = event->button.x;
		*y = event->button.y;
		return TRUE;

	default:
		return FALSE;
	}
} /* get_event_coordinates */

static void
emit_event_on_tags(GnomeCanvasRichText *text, GdkEvent *event, 
		   GtkTextIter *iter)
{
	GSList *tags;
	GSList *i;
	
	tags = gtk_text_iter_get_tags(iter);

	i = tags;
	while (i) {
		GtkTextTag *tag = i->data;

		gtk_text_tag_event(tag, G_OBJECT(text), event, iter);

		/* The cursor has been moved to within this tag. Emit the
		   tag_changed signal */
		if (event->type == GDK_BUTTON_RELEASE || 
		    event->type == GDK_KEY_PRESS ||
		    event->type == GDK_KEY_RELEASE) {
			gnome_canvas_rich_text_emit_tag_changed(
				text, tag);
		}

		i = g_slist_next(i);
	}

	g_slist_free(tags);
} /* emit_event_on_tags */

static gint
gnome_canvas_rich_text_event(GnomeCanvasItem *item, GdkEvent *event)
{
	GnomeCanvasRichText *text = GNOME_CANVAS_RICH_TEXT(item);
	int x, y;

	if (get_event_coordinates(event, &x, &y)) {
		GtkTextIter iter;

		x -= text->_priv->x;
		y -= text->_priv->y;

		gtk_text_layout_get_iter_at_pixel(text->_priv->layout, &iter, x, y);
		emit_event_on_tags(text, event, &iter);
	}
	else if (event->type == GDK_KEY_PRESS ||
		 event->type == GDK_KEY_RELEASE) {
		GtkTextMark *insert;
		GtkTextIter iter;

		insert = gtk_text_buffer_get_mark(get_buffer(text), "insert");
		gtk_text_buffer_get_iter_at_mark(
			get_buffer(text), &iter, insert);
		emit_event_on_tags(text, event, &iter);
	}

	switch (event->type) {
	case GDK_KEY_PRESS:
		return gnome_canvas_rich_text_key_press_event(
			item, (GdkEventKey *) event);
	case GDK_KEY_RELEASE:
		return gnome_canvas_rich_text_key_release_event(
			item, (GdkEventKey *) event);
	case GDK_BUTTON_PRESS:
		return gnome_canvas_rich_text_button_press_event(
			item, (GdkEventButton *) event);
	case GDK_BUTTON_RELEASE:
		return gnome_canvas_rich_text_button_release_event(
			item, (GdkEventButton *) event);
	case GDK_FOCUS_CHANGE:
		if (((GdkEventFocus *) event)->window !=
		    item->canvas->layout.bin_window)
			return FALSE;

		if (((GdkEventFocus *) event)->in)
			return gnome_canvas_rich_text_focus_in_event(
				item, (GdkEventFocus *) event);
		else
			return gnome_canvas_rich_text_focus_out_event(
				item, (GdkEventFocus *) event);
	default:
		return FALSE;
	}
} /* gnome_canvas_rich_text_event */

/* Cut/Copy/Paste */

/**
 * gnome_canvas_rich_text_cut_clipboard:
 * @text: a #GnomeCanvasRichText.
 * 
 * Copies the currently selected @text to clipboard, then deletes said text
 * if it's editable.
 **/
void
gnome_canvas_rich_text_cut_clipboard(GnomeCanvasRichText *text)
{
	g_return_if_fail(text);
	g_return_if_fail(get_buffer(text));

	gtk_text_buffer_cut_clipboard(get_buffer(text),
				      gtk_clipboard_get (GDK_SELECTION_PRIMARY),
				      text->_priv->editable);
} /* gnome_canvas_rich_text_cut_clipboard */


/**
 * gnome_canvas_rich_text_copy_clipboard:
 * @text: a #GnomeCanvasRichText.
 *
 * Copies the currently selected @text to clipboard.
 **/
void
gnome_canvas_rich_text_copy_clipboard(GnomeCanvasRichText *text)
{
	g_return_if_fail(text);
	g_return_if_fail(get_buffer(text));

	gtk_text_buffer_copy_clipboard(get_buffer(text),
				       gtk_clipboard_get (GDK_SELECTION_PRIMARY));
} /* gnome_canvas_rich_text_cut_clipboard */


/**
 * gnome_canvas_rich_text_paste_clipboard:
 * @text: a #GnomeCanvasRichText.
 *
 * Pastes the contents of the clipboard at the insertion point.
 **/
void
gnome_canvas_rich_text_paste_clipboard(GnomeCanvasRichText *text)
{
	g_return_if_fail(text);
	g_return_if_fail(get_buffer(text));

	gtk_text_buffer_paste_clipboard(get_buffer(text),
					gtk_clipboard_get (GDK_SELECTION_PRIMARY),
					NULL,
					text->_priv->editable);
} /* gnome_canvas_rich_text_cut_clipboard */

static gint
preblink_cb(gpointer data)
{
	GnomeCanvasRichText *text = GNOME_CANVAS_RICH_TEXT(data);

	text->_priv->preblink_timeout = 0;
	gnome_canvas_rich_text_start_cursor_blink(text, FALSE);

	/* Remove ourselves */
	return FALSE;
} /* preblink_cb */

static gint
blink_cb(gpointer data)
{
	GnomeCanvasRichText *text = GNOME_CANVAS_RICH_TEXT(data);
	gboolean visible;

	g_assert(text->_priv->layout);
	g_assert(text->_priv->cursor_visible);

	visible = gtk_text_layout_get_cursor_visible(text->_priv->layout);
	if (visible)
		text->_priv->blink_timeout = g_timeout_add(
			CURSOR_OFF_TIME, blink_cb, text);
	else
		text->_priv->blink_timeout = g_timeout_add(
			CURSOR_ON_TIME, blink_cb, text);

	gtk_text_layout_set_cursor_visible(text->_priv->layout, !visible);

	/* Remove ourself */
	return FALSE;
} /* blink_cb */

static void
gnome_canvas_rich_text_start_cursor_blink(GnomeCanvasRichText *text,
					  gboolean with_delay)
{
	if (!text->_priv->layout)
		return;

	if (!text->_priv->cursor_visible || !text->_priv->cursor_blink)
		return;

	if (text->_priv->preblink_timeout != 0) {
		g_source_remove(text->_priv->preblink_timeout);
		text->_priv->preblink_timeout = 0;
	}

	if (with_delay) {
		if (text->_priv->blink_timeout != 0) {
			g_source_remove(text->_priv->blink_timeout);
			text->_priv->blink_timeout = 0;
		}

		gtk_text_layout_set_cursor_visible(text->_priv->layout, TRUE);

		text->_priv->preblink_timeout = g_timeout_add(
			PREBLINK_TIME, preblink_cb, text);
	}
	else {
		if (text->_priv->blink_timeout == 0) {
			gtk_text_layout_set_cursor_visible(text->_priv->layout, TRUE);
			text->_priv->blink_timeout = g_timeout_add(
				CURSOR_ON_TIME, blink_cb, text);
		}
	}
} /* gnome_canvas_rich_text_start_cursor_blink */

static void
gnome_canvas_rich_text_stop_cursor_blink(GnomeCanvasRichText *text)
{
	if (text->_priv->blink_timeout) {
		g_source_remove(text->_priv->blink_timeout);
		text->_priv->blink_timeout = 0;
	}
} /* gnome_canvas_rich_text_stop_cursor_blink */

/* We have to request updates this way because the update cycle is not
   re-entrant. This will fire off a request in an idle loop. */
static gboolean
request_update(gpointer data)
{
	GnomeCanvasRichText *text = GNOME_CANVAS_RICH_TEXT(data);

	gnome_canvas_item_request_update(GNOME_CANVAS_ITEM(text));

	return FALSE;
} /* request_update */

static void
invalidated_handler(GtkTextLayout * layout, gpointer data)
{
	GnomeCanvasRichText *text = GNOME_CANVAS_RICH_TEXT(data);

#if 0
	printf("Text is being invalidated.\n");
#endif

	gtk_text_layout_validate(text->_priv->layout, 2000);

	/* We are called from the update cycle; gotta put this in an idle
	   loop. */
	g_idle_add(request_update, text);
} /* invalidated_handler */

static void
scale_fonts(GtkTextTag *tag, gpointer data)
{
	GnomeCanvasRichText *text = GNOME_CANVAS_RICH_TEXT(data);

	if (!tag->values)
		return;

	g_object_set(
		G_OBJECT(tag), "scale", 
		text->_priv->layout->default_style->font_scale, NULL);
} /* scale_fonts */

static void
changed_handler(GtkTextLayout * layout, gint start_y, 
		gint old_height, gint new_height, gpointer data)
{
	GnomeCanvasRichText *text = GNOME_CANVAS_RICH_TEXT(data);

#if 0
	printf("Layout %p is being changed.\n", text->_priv->layout);
#endif

	if (text->_priv->layout->default_style->font_scale != 
	    GNOME_CANVAS_ITEM(text)->canvas->pixels_per_unit) {
		GtkTextTagTable *tag_table;

		text->_priv->layout->default_style->font_scale = 
			GNOME_CANVAS_ITEM(text)->canvas->pixels_per_unit;

		tag_table = gtk_text_buffer_get_tag_table(get_buffer(text));
		gtk_text_tag_table_foreach(tag_table, scale_fonts, text);

		gtk_text_layout_default_style_changed(text->_priv->layout);
	}

	if (text->_priv->grow_height) {
		int width, height;

		gtk_text_layout_get_size(text->_priv->layout, &width, &height);

		if (height > text->_priv->height)
			text->_priv->height = height;
	}

	/* We are called from the update cycle; gotta put this in an idle
	   loop. */
	g_idle_add(request_update, text);
} /* changed_handler */


/**
 * gnome_canvas_rich_text_set_buffer:
 * @text: a #GnomeCanvasRichText.
 * @buffer: a #GtkTextBuffer.
 *
 * Sets the buffer field of the @text to @buffer. 
 **/ 
void
gnome_canvas_rich_text_set_buffer(GnomeCanvasRichText *text, 
				  GtkTextBuffer *buffer)
{
	g_return_if_fail(GNOME_IS_CANVAS_RICH_TEXT(text));
	g_return_if_fail(buffer == NULL || GTK_IS_TEXT_BUFFER(buffer));

	if (text->_priv->buffer == buffer)
		return;

	if (text->_priv->buffer != NULL) {
		g_object_unref(G_OBJECT(text->_priv->buffer));
	}

	text->_priv->buffer = buffer;

	if (buffer) {
		g_object_ref(G_OBJECT(buffer));

		if (text->_priv->layout)
			gtk_text_layout_set_buffer(text->_priv->layout, buffer);
	}

	gnome_canvas_item_request_update(GNOME_CANVAS_ITEM(text));
} /* gnome_canvas_rich_text_set_buffer */

static GtkTextBuffer *
get_buffer(GnomeCanvasRichText *text)
{
	if (!text->_priv->buffer) {
		GtkTextBuffer *b;

		b = gtk_text_buffer_new(NULL);
		gnome_canvas_rich_text_set_buffer(text, b);
		g_object_unref(G_OBJECT(b));
	}

	return text->_priv->buffer;
} /* get_buffer */


/**
 * gnome_canvas_rich_text_get_buffer:
 * @text: a #GnomeCanvasRichText.
 *
 * Returns a #GtkTextBuffer associated with the #GnomeCanvasRichText.
 * This function creates a new #GtkTextBuffer if the text buffer is NULL.
 *
 * Return value: the #GtkTextBuffer.
 **/
GtkTextBuffer *
gnome_canvas_rich_text_get_buffer(GnomeCanvasRichText *text)
{
	g_return_val_if_fail(GNOME_IS_CANVAS_RICH_TEXT(text), NULL);

	return get_buffer(text);
} /* gnome_canvas_rich_text_get_buffer */


/**
 * gnome_canvas_rich_text_get_iter_location:
 * @text: a #GnomeCanvasRichText.
 * @iter: a #GtkTextIter.
 * @location: a #GdkRectangle containing the bounds of the character at @iter.
 *
 * Gets a rectangle which roughly contains the character at @iter.
 **/
void
gnome_canvas_rich_text_get_iter_location (GnomeCanvasRichText *text,
					  const GtkTextIter *iter,
					  GdkRectangle      *location)
{
  g_return_if_fail (GNOME_IS_CANVAS_RICH_TEXT (text));
  g_return_if_fail (gtk_text_iter_get_buffer (iter) == text->_priv->buffer);

  gtk_text_layout_get_iter_location (text->_priv->layout, iter, location);
}


/**
 * gnome_canvas_rich_text_get_iter_at_location:
 * @text: a #GnomeCanvasRichText.
 * @iter: a #GtkTextIter.
 * @x: x position, in buffer coordinates.
 * @y: y position, in buffer coordinates.
 *
 * Retrieves the iterator at the buffer coordinates x and y. 
 **/ 
void
gnome_canvas_rich_text_get_iter_at_location (GnomeCanvasRichText *text,
                                    GtkTextIter *iter,
                                    gint         x,
                                    gint         y)
{
  g_return_if_fail (GNOME_IS_CANVAS_RICH_TEXT (text));
  g_return_if_fail (iter != NULL);
  g_return_if_fail (text->_priv->layout != NULL);

  gtk_text_layout_get_iter_at_pixel (text->_priv->layout,
                                     iter,
                                     x,
                                     y);
}


static void
gnome_canvas_rich_text_set_attributes_from_style(GnomeCanvasRichText * text,
						 GtkTextAttributes *values,
						 GtkStyle *style)
{
	values->appearance.bg_color = style->base[GTK_STATE_NORMAL];
	values->appearance.fg_color = style->fg[GTK_STATE_NORMAL];
	
	if (values->font)
		pango_font_description_free (values->font);

	values->font = pango_font_description_copy (style->font_desc);

} /* gnome_canvas_rich_text_set_attributes_from_style */

static void
gnome_canvas_rich_text_ensure_layout(GnomeCanvasRichText *text)
{
	if (!text->_priv->layout) {
		GtkWidget *canvas;
		GtkTextAttributes *style;
		PangoContext *ltr_context, *rtl_context;

		text->_priv->layout = gtk_text_layout_new();

		gtk_text_layout_set_screen_width(text->_priv->layout, text->_priv->width);

		if (get_buffer(text)) {
			gtk_text_layout_set_buffer(
				text->_priv->layout, get_buffer(text));
		}

		/* Setup the cursor stuff */
		gtk_text_layout_set_cursor_visible(
			text->_priv->layout, text->_priv->cursor_visible);
		if (text->_priv->cursor_visible && text->_priv->cursor_blink)
			gnome_canvas_rich_text_start_cursor_blink(text, FALSE);
		else
			gnome_canvas_rich_text_stop_cursor_blink(text);

		canvas = GTK_WIDGET(GNOME_CANVAS_ITEM(text)->canvas);

		ltr_context = gtk_widget_create_pango_context(canvas);
		pango_context_set_base_dir(ltr_context, PANGO_DIRECTION_LTR);
		rtl_context = gtk_widget_create_pango_context(canvas);
		pango_context_set_base_dir(rtl_context, PANGO_DIRECTION_RTL);

		gtk_text_layout_set_contexts(
			text->_priv->layout, ltr_context, rtl_context);

		g_object_unref(G_OBJECT(ltr_context));
		g_object_unref(G_OBJECT(rtl_context));

		style = gtk_text_attributes_new();

		gnome_canvas_rich_text_set_attributes_from_style(
			text, style, canvas->style);

		style->pixels_above_lines = text->_priv->pixels_above_lines;
		style->pixels_below_lines = text->_priv->pixels_below_lines;
		style->pixels_inside_wrap = text->_priv->pixels_inside_wrap;
		style->left_margin = text->_priv->left_margin;
		style->right_margin = text->_priv->right_margin;
		style->indent = text->_priv->indent;
		style->tabs = NULL;
		style->wrap_mode = text->_priv->wrap_mode;
		style->justification = text->_priv->justification;
		style->direction = text->_priv->direction;
		style->editable = text->_priv->editable;
		style->invisible = !text->_priv->visible;

		gtk_text_layout_set_default_style(text->_priv->layout, style);

		gtk_text_attributes_unref(style);

		g_signal_connect(
			G_OBJECT(text->_priv->layout), "invalidated",
			G_CALLBACK (invalidated_handler), text);

		g_signal_connect(
			G_OBJECT(text->_priv->layout), "changed",
			G_CALLBACK (changed_handler), text);
	}
} /* gnome_canvas_rich_text_ensure_layout */

static void
gnome_canvas_rich_text_destroy_layout(GnomeCanvasRichText *text)
{
	if (text->_priv->layout) {
		g_signal_handlers_disconnect_by_func(
			G_OBJECT(text->_priv->layout), invalidated_handler, text);
		g_signal_handlers_disconnect_by_func(
			G_OBJECT(text->_priv->layout), changed_handler, text);
		g_object_unref(G_OBJECT(text->_priv->layout));
		text->_priv->layout = NULL;
	}
} /* gnome_canvas_rich_text_destroy_layout */

static void
adjust_for_anchors(GnomeCanvasRichText *text, double *ax, double *ay)
{
	double x, y;

	x = text->_priv->x;
	y = text->_priv->y;

	/* Anchor text */
	/* X coordinates */
	switch (text->_priv->anchor) {
	case GTK_ANCHOR_NW:
	case GTK_ANCHOR_W:
	case GTK_ANCHOR_SW:
		break;

	case GTK_ANCHOR_N:
	case GTK_ANCHOR_CENTER:
	case GTK_ANCHOR_S:
		x -= text->_priv->width / 2;
		break;

	case GTK_ANCHOR_NE:
	case GTK_ANCHOR_E:
	case GTK_ANCHOR_SE:
		x -= text->_priv->width;
		break;
	default:
		break;
	}

	/* Y coordinates */
	switch (text->_priv->anchor) {
	case GTK_ANCHOR_NW:
	case GTK_ANCHOR_N:
	case GTK_ANCHOR_NE:
		break;

	case GTK_ANCHOR_W:
	case GTK_ANCHOR_CENTER:
	case GTK_ANCHOR_E:
		y -= text->_priv->height / 2;
		break;

	case GTK_ANCHOR_SW:
	case GTK_ANCHOR_S:
	case GTK_ANCHOR_SE:
		y -= text->_priv->height;
		break;
	default:
		break;
	}

	if (ax)
		*ax = x;
	if (ay)
		*ay = y;
} /* adjust_for_anchors */

static void
get_bounds(GnomeCanvasRichText *text, double *px1, double *py1,
	   double *px2, double *py2)
{
	GnomeCanvasItem *item = GNOME_CANVAS_ITEM(text);
	double x, y;
	double x1, x2, y1, y2;
	int cx1, cx2, cy1, cy2;

	adjust_for_anchors(text, &x, &y);

	x1 = x;
	y1 = y;
	x2 = x + text->_priv->width;
	y2 = y + text->_priv->height;

	gnome_canvas_item_i2w(item, &x1, &y1);
	gnome_canvas_item_i2w(item, &x2, &y2);
	gnome_canvas_w2c(item->canvas, x1, y1, &cx1, &cy1);
	gnome_canvas_w2c(item->canvas, x2, y2, &cx2, &cy2);

	*px1 = cx1;
	*py1 = cy1;
	*px2 = cx2;
	*py2 = cy2;
} /* get_bounds */

static void gnome_canvas_rich_text_get_bounds(GnomeCanvasItem *item, double *px1, double *py1,
	   double *px2, double *py2)
{
	GnomeCanvasRichText *text = GNOME_CANVAS_RICH_TEXT(item);
	get_bounds (text, px1, py1, px2, py2);
}

static void
gnome_canvas_rich_text_update(GnomeCanvasItem *item, double *affine,
			      ArtSVP *clip_path, int flags)
{
	GnomeCanvasRichText *text = GNOME_CANVAS_RICH_TEXT(item);
	double x1, y1, x2, y2;
	GtkTextIter start;

	(* GNOME_CANVAS_ITEM_CLASS(parent_class)->update)(
		item, affine, clip_path, flags);

	get_bounds(text, &x1, &y1, &x2, &y2);

	gtk_text_buffer_get_iter_at_offset(text->_priv->buffer, &start, 0);
	if (text->_priv->layout)
		gtk_text_layout_validate_yrange(
			text->_priv->layout, &start, 0, y2 - y1);

	gnome_canvas_update_bbox(item, x1, y1, x2, y2);
} /* gnome_canvas_rich_text_update */
			       			  
static double
gnome_canvas_rich_text_point(GnomeCanvasItem *item, double x, double y,
			     int cx, int cy, GnomeCanvasItem **actual_item)
{
	GnomeCanvasRichText *text = GNOME_CANVAS_RICH_TEXT(item);
	double ax, ay;
	double x1, x2, y1, y2;
	double dx, dy;

	*actual_item = item;

	/* This is a lame cop-out. Anywhere inside of the bounding box. */

	adjust_for_anchors(text, &ax, &ay);

	x1 = ax;
	y1 = ay;
	x2 = ax + text->_priv->width;
	y2 = ay + text->_priv->height;

	if ((x > x1) && (y > y1) && (x < x2) && (y < y2))
		return 0.0;

	if (x < x1)
		dx = x1 - x;
	else if (x > x2)
		dx = x - x2;
	else
		dx = 0.0;

	if (y < y1)
		dy = y1 - y;
	else if (y > y2)
		dy = y - y2;
	else
		dy = 0.0;

	return sqrt(dx * dx + dy * dy);
} /* gnome_canvas_rich_text_point */

static void
gnome_canvas_rich_text_draw(GnomeCanvasItem *item, GdkDrawable *drawable,
			    int x, int y, int width, int height)
{
	GnomeCanvasRichText *text = GNOME_CANVAS_RICH_TEXT(item);
	double i2w[6], w2c[6], i2c[6];
	double ax, ay;
	int x1, y1, x2, y2;
	ArtPoint i1, i2;
	ArtPoint c1, c2;

	gnome_canvas_item_i2w_affine(item, i2w);
	gnome_canvas_w2c_affine(item->canvas, w2c);
	art_affine_multiply(i2c, i2w, w2c);

	adjust_for_anchors(text, &ax, &ay);

	i1.x = ax;
	i1.y = ay;
	i2.x = ax + text->_priv->width;
	i2.y = ay + text->_priv->height;
	art_affine_point(&c1, &i1, i2c);
	art_affine_point(&c2, &i2, i2c);

	x1 = c1.x;
	y1 = c1.y;
	x2 = c2.x;
	y2 = c2.y;

	gtk_text_layout_set_screen_width(text->_priv->layout, x2 - x1);
      
        /* FIXME: should last arg be NULL? */
	gtk_text_layout_draw(
		text->_priv->layout,
		GTK_WIDGET(item->canvas),
		drawable,
		GTK_WIDGET (item->canvas)->style->text_gc[GTK_STATE_NORMAL],
		x - x1, y - y1,
		0, 0, (x2 - x1) - (x - x1), (y2 - y1) - (y - y1),
		NULL);
} /* gnome_canvas_rich_text_draw */

static void
gnome_canvas_rich_text_render(GnomeCanvasItem *item, GnomeCanvasBuf *buf)
{
	g_warning ("rich text item not implemented for anti-aliased canvas");
} /* gnome_canvas_rich_text_render */

#if 0
static GtkTextTag *
gnome_canvas_rich_text_add_tag(GnomeCanvasRichText *text, char *tag_name,
			       int start_offset, int end_offset, 
			       const char *first_property_name, ...)
{
	GtkTextTag *tag;
	GtkTextIter start, end;
	va_list var_args;

	g_return_val_if_fail(text, NULL);
	g_return_val_if_fail(start_offset >= 0, NULL);
	g_return_val_if_fail(end_offset >= 0, NULL);

	if (tag_name) {
		GtkTextTagTable *tag_table;

		tag_table = gtk_text_buffer_get_tag_table(get_buffer(text));
		g_return_val_if_fail(gtk_text_tag_table_lookup(tag_table, tag_name) == NULL, NULL);
	}

	tag = gtk_text_buffer_create_tag(
		get_buffer(text), tag_name, NULL);

	va_start(var_args, first_property_name);
	g_object_set_valist(G_OBJECT(tag), first_property_name, var_args);
	va_end(var_args);

	gtk_text_buffer_get_iter_at_offset(
		get_buffer(text), &start, start_offset);
	gtk_text_buffer_get_iter_at_offset(
		get_buffer(text), &end, end_offset);
	gtk_text_buffer_apply_tag(get_buffer(text), tag, &start, &end);

	return tag;
} /* gnome_canvas_rich_text_add_tag */
#endif
