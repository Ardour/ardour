/* GTK - The GIMP Toolkit
 * Copyright (C) 1995-1997 Peter Mattis, Spencer Kimball and Josh MacDonald
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

/*
 * Modified by the GTK+ Team and others 1997-2000.  See the AUTHORS
 * file for a list of people on the GTK+ Team.  See the ChangeLog
 * files for a list of changes.  These files are distributed with
 * GTK+ at ftp://ftp.gtk.org/pub/gtk/. 
 */

#include "config.h"

#undef GTK_DISABLE_DEPRECATED

#include "gtkitem.h"
#include "gtkmarshalers.h"
#include "gtkintl.h"
#include "gtkalias.h"


enum {
  SELECT,
  DESELECT,
  TOGGLE,
  LAST_SIGNAL
};


static void gtk_item_realize    (GtkWidget        *widget);
static gint gtk_item_enter      (GtkWidget        *widget,
				 GdkEventCrossing *event);
static gint gtk_item_leave      (GtkWidget        *widget,
				 GdkEventCrossing *event);


static guint item_signals[LAST_SIGNAL] = { 0 };

G_DEFINE_ABSTRACT_TYPE (GtkItem, gtk_item, GTK_TYPE_BIN)

static void
gtk_item_class_init (GtkItemClass *class)
{
  GObjectClass *object_class;
  GtkWidgetClass *widget_class;

  object_class = (GObjectClass*) class;
  widget_class = (GtkWidgetClass*) class;

  widget_class->realize = gtk_item_realize;
  widget_class->enter_notify_event = gtk_item_enter;
  widget_class->leave_notify_event = gtk_item_leave;

  class->select = NULL;
  class->deselect = NULL;
  class->toggle = NULL;

  item_signals[SELECT] =
    g_signal_new (I_("select"),
		  G_OBJECT_CLASS_TYPE (object_class),
		  G_SIGNAL_RUN_FIRST,
		  G_STRUCT_OFFSET (GtkItemClass, select),
		  NULL, NULL,
		  _gtk_marshal_VOID__VOID,
		  G_TYPE_NONE, 0);
  item_signals[DESELECT] =
    g_signal_new (I_("deselect"),
		  G_OBJECT_CLASS_TYPE (object_class),
		  G_SIGNAL_RUN_FIRST,
		  G_STRUCT_OFFSET (GtkItemClass, deselect),
		  NULL, NULL,
		  _gtk_marshal_VOID__VOID,
		  G_TYPE_NONE, 0);
  item_signals[TOGGLE] =
    g_signal_new (I_("toggle"),
		  G_OBJECT_CLASS_TYPE (object_class),
		  G_SIGNAL_RUN_FIRST,
		  G_STRUCT_OFFSET (GtkItemClass, toggle),
		  NULL, NULL,
		  _gtk_marshal_VOID__VOID,
		  G_TYPE_NONE, 0);
  widget_class->activate_signal = item_signals[TOGGLE];
}

static void
gtk_item_init (GtkItem *item)
{
  gtk_widget_set_has_window (GTK_WIDGET (item), TRUE);
}

void
gtk_item_select (GtkItem *item)
{
  g_signal_emit (item, item_signals[SELECT], 0);
}

void
gtk_item_deselect (GtkItem *item)
{
  g_signal_emit (item, item_signals[DESELECT], 0);
}

void
gtk_item_toggle (GtkItem *item)
{
  g_signal_emit (item, item_signals[TOGGLE], 0);
}


static void
gtk_item_realize (GtkWidget *widget)
{
  GdkWindowAttr attributes;
  gint attributes_mask;

  gtk_widget_set_realized (widget, TRUE);

  attributes.x = widget->allocation.x;
  attributes.y = widget->allocation.y;
  attributes.width = widget->allocation.width;
  attributes.height = widget->allocation.height;
  attributes.window_type = GDK_WINDOW_CHILD;
  attributes.wclass = GDK_INPUT_OUTPUT;
  attributes.visual = gtk_widget_get_visual (widget);
  attributes.colormap = gtk_widget_get_colormap (widget);
  attributes.event_mask = (gtk_widget_get_events (widget) |
			   GDK_EXPOSURE_MASK |
			   GDK_BUTTON_PRESS_MASK |
			   GDK_BUTTON_RELEASE_MASK |
			   GDK_ENTER_NOTIFY_MASK |
			   GDK_LEAVE_NOTIFY_MASK |
			   GDK_POINTER_MOTION_MASK);

  attributes_mask = GDK_WA_X | GDK_WA_Y | GDK_WA_VISUAL | GDK_WA_COLORMAP;
  widget->window = gdk_window_new (gtk_widget_get_parent_window (widget), &attributes, attributes_mask);
  gdk_window_set_user_data (widget->window, widget);

  widget->style = gtk_style_attach (widget->style, widget->window);
  gtk_style_set_background (widget->style, widget->window, GTK_STATE_NORMAL);
  gdk_window_set_back_pixmap (widget->window, NULL, TRUE);
}

static gint
gtk_item_enter (GtkWidget        *widget,
		GdkEventCrossing *event)
{
  g_return_val_if_fail (GTK_IS_ITEM (widget), FALSE);
  g_return_val_if_fail (event != NULL, FALSE);

  return gtk_widget_event (widget->parent, (GdkEvent*) event);
}

static gint
gtk_item_leave (GtkWidget        *widget,
		GdkEventCrossing *event)
{
  g_return_val_if_fail (GTK_IS_ITEM (widget), FALSE);
  g_return_val_if_fail (event != NULL, FALSE);

  return gtk_widget_event (widget->parent, (GdkEvent*) event);
}

#define __GTK_ITEM_C__
#include "gtkaliasdef.c"
