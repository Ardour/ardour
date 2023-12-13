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
#include "gtkcontainer.h"
#include "gtkmisc.h"
#include "gtkintl.h"
#include "gtkprivate.h"
#include "gtkalias.h"


enum {
  PROP_0,
  PROP_XALIGN,
  PROP_YALIGN,
  PROP_XPAD,
  PROP_YPAD
};

static void gtk_misc_realize      (GtkWidget    *widget);
static void gtk_misc_set_property (GObject         *object,
				   guint            prop_id,
				   const GValue    *value,
				   GParamSpec      *pspec);
static void gtk_misc_get_property (GObject         *object,
				   guint            prop_id,
				   GValue          *value,
				   GParamSpec      *pspec);


G_DEFINE_ABSTRACT_TYPE (GtkMisc, gtk_misc, GTK_TYPE_WIDGET)

static void
gtk_misc_class_init (GtkMiscClass *class)
{
  GObjectClass   *gobject_class;
  GtkWidgetClass *widget_class;

  gobject_class = G_OBJECT_CLASS (class);
  widget_class = (GtkWidgetClass*) class;

  gobject_class->set_property = gtk_misc_set_property;
  gobject_class->get_property = gtk_misc_get_property;
  
  widget_class->realize = gtk_misc_realize;

  g_object_class_install_property (gobject_class,
                                   PROP_XALIGN,
                                   g_param_spec_float ("xalign",
						       P_("X align"),
						       P_("The horizontal alignment, from 0 (left) to 1 (right). Reversed for RTL layouts."),
						       0.0,
						       1.0,
						       0.5,
						       GTK_PARAM_READWRITE));

  g_object_class_install_property (gobject_class,
                                   PROP_YALIGN,
                                   g_param_spec_float ("yalign",
						       P_("Y align"),
						       P_("The vertical alignment, from 0 (top) to 1 (bottom)"),
						       0.0,
						       1.0,
						       0.5,
						       GTK_PARAM_READWRITE));

  g_object_class_install_property (gobject_class,
                                   PROP_XPAD,
                                   g_param_spec_int ("xpad",
						     P_("X pad"),
						     P_("The amount of space to add on the left and right of the widget, in pixels"),
						     0,
						     G_MAXINT,
						     0,
						     GTK_PARAM_READWRITE));

  g_object_class_install_property (gobject_class,
                                   PROP_YPAD,
                                   g_param_spec_int ("ypad",
						     P_("Y pad"),
						     P_("The amount of space to add on the top and bottom of the widget, in pixels"),
						     0,
						     G_MAXINT,
						     0,
						     GTK_PARAM_READWRITE));
}

static void
gtk_misc_init (GtkMisc *misc)
{
  misc->xalign = 0.5;
  misc->yalign = 0.5;
  misc->xpad = 0;
  misc->ypad = 0;
}

static void
gtk_misc_set_property (GObject      *object,
		       guint         prop_id,
		       const GValue *value,
		       GParamSpec   *pspec)
{
  GtkMisc *misc;

  misc = GTK_MISC (object);

  switch (prop_id)
    {
    case PROP_XALIGN:
      gtk_misc_set_alignment (misc, g_value_get_float (value), misc->yalign);
      break;
    case PROP_YALIGN:
      gtk_misc_set_alignment (misc, misc->xalign, g_value_get_float (value));
      break;
    case PROP_XPAD:
      gtk_misc_set_padding (misc, g_value_get_int (value), misc->ypad);
      break;
    case PROP_YPAD:
      gtk_misc_set_padding (misc, misc->xpad, g_value_get_int (value));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
gtk_misc_get_property (GObject      *object,
		       guint         prop_id,
		       GValue       *value,
		       GParamSpec   *pspec)
{
  GtkMisc *misc;

  misc = GTK_MISC (object);

  switch (prop_id)
    {
    case PROP_XALIGN:
      g_value_set_float (value, misc->xalign);
      break;
    case PROP_YALIGN:
      g_value_set_float (value, misc->yalign);
      break;
    case PROP_XPAD:
      g_value_set_int (value, misc->xpad);
      break;
    case PROP_YPAD:
      g_value_set_int (value, misc->ypad);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

void
gtk_misc_set_alignment (GtkMisc *misc,
			gfloat   xalign,
			gfloat   yalign)
{
  GtkWidget *widget;

  g_return_if_fail (GTK_IS_MISC (misc));

  if (xalign < 0.0)
    xalign = 0.0;
  else if (xalign > 1.0)
    xalign = 1.0;

  if (yalign < 0.0)
    yalign = 0.0;
  else if (yalign > 1.0)
    yalign = 1.0;

  if ((xalign != misc->xalign) || (yalign != misc->yalign))
    {
      g_object_freeze_notify (G_OBJECT (misc));
      if (xalign != misc->xalign)
	g_object_notify (G_OBJECT (misc), "xalign");

      if (yalign != misc->yalign)
	g_object_notify (G_OBJECT (misc), "yalign");

      misc->xalign = xalign;
      misc->yalign = yalign;
      
      /* clear the area that was allocated before the change
       */
      widget = GTK_WIDGET (misc);
      if (gtk_widget_is_drawable (widget))
        gtk_widget_queue_draw (widget);

      g_object_thaw_notify (G_OBJECT (misc));
    }
}

/**
 * gtk_misc_get_alignment:
 * @misc: a #GtkMisc
 * @xalign: (out) (allow-none): location to store X alignment of @misc, or %NULL
 * @yalign: (out) (allow-none): location to store Y alignment of @misc, or %NULL
 *
 * Gets the X and Y alignment of the widget within its allocation. 
 * See gtk_misc_set_alignment().
 **/
void
gtk_misc_get_alignment (GtkMisc *misc,
		        gfloat  *xalign,
			gfloat  *yalign)
{
  g_return_if_fail (GTK_IS_MISC (misc));

  if (xalign)
    *xalign = misc->xalign;
  if (yalign)
    *yalign = misc->yalign;
}

void
gtk_misc_set_padding (GtkMisc *misc,
		      gint     xpad,
		      gint     ypad)
{
  GtkRequisition *requisition;
  
  g_return_if_fail (GTK_IS_MISC (misc));
  
  if (xpad < 0)
    xpad = 0;
  if (ypad < 0)
    ypad = 0;
  
  if ((xpad != misc->xpad) || (ypad != misc->ypad))
    {
      g_object_freeze_notify (G_OBJECT (misc));
      if (xpad != misc->xpad)
	g_object_notify (G_OBJECT (misc), "xpad");

      if (ypad != misc->ypad)
	g_object_notify (G_OBJECT (misc), "ypad");

      requisition = &(GTK_WIDGET (misc)->requisition);
      requisition->width -= misc->xpad * 2;
      requisition->height -= misc->ypad * 2;
      
      misc->xpad = xpad;
      misc->ypad = ypad;
      
      requisition->width += misc->xpad * 2;
      requisition->height += misc->ypad * 2;
      
      if (gtk_widget_is_drawable (GTK_WIDGET (misc)))
	gtk_widget_queue_resize (GTK_WIDGET (misc));

      g_object_thaw_notify (G_OBJECT (misc));
    }
}

/**
 * gtk_misc_get_padding:
 * @misc: a #GtkMisc
 * @xpad: (out) (allow-none): location to store padding in the X
 *        direction, or %NULL
 * @ypad: (out) (allow-none): location to store padding in the Y
 *        direction, or %NULL
 *
 * Gets the padding in the X and Y directions of the widget. 
 * See gtk_misc_set_padding().
 **/
void
gtk_misc_get_padding (GtkMisc *misc,
		      gint    *xpad,
		      gint    *ypad)
{
  g_return_if_fail (GTK_IS_MISC (misc));

  if (xpad)
    *xpad = misc->xpad;
  if (ypad)
    *ypad = misc->ypad;
}

static void
gtk_misc_realize (GtkWidget *widget)
{
  GdkWindowAttr attributes;
  gint attributes_mask;

  gtk_widget_set_realized (widget, TRUE);

  if (!gtk_widget_get_has_window (widget))
    {
      widget->window = gtk_widget_get_parent_window (widget);
      g_object_ref (widget->window);
      widget->style = gtk_style_attach (widget->style, widget->window);
    }
  else
    {
      attributes.window_type = GDK_WINDOW_CHILD;
      attributes.x = widget->allocation.x;
      attributes.y = widget->allocation.y;
      attributes.width = widget->allocation.width;
      attributes.height = widget->allocation.height;
      attributes.wclass = GDK_INPUT_OUTPUT;
      attributes.visual = gtk_widget_get_visual (widget);
      attributes.colormap = gtk_widget_get_colormap (widget);
      attributes.event_mask = gtk_widget_get_events (widget) | GDK_EXPOSURE_MASK;
      attributes_mask = GDK_WA_X | GDK_WA_Y | GDK_WA_VISUAL | GDK_WA_COLORMAP;

      widget->window = gdk_window_new (gtk_widget_get_parent_window (widget), &attributes, attributes_mask);
      gdk_window_set_user_data (widget->window, widget);

      widget->style = gtk_style_attach (widget->style, widget->window);
      gdk_window_set_back_pixmap (widget->window, NULL, TRUE);
    }
}


#define MARSHALLER(name) \
G_MODULE_EXPORT \
void gtk_marshal_ ## name (GClosure *closure, \
                           GValue *return_value, \
                           guint n_param_values, \
                           const GValue *param_values, \
                           gpointer invocation_hint, \
                           gpointer marshal_data); \
void \
gtk_marshal_ ## name (GClosure *closure, \
                      GValue *return_value, \
                      guint n_param_values, \
                      const GValue *param_values, \
                      gpointer invocation_hint, \
                      gpointer marshal_data) \
{ \
  g_cclosure_marshal_ ## name (closure, return_value, n_param_values, \
                               param_values, invocation_hint, \
                               marshal_data); \
}

MARSHALLER (VOID__BOOLEAN)
MARSHALLER (VOID__BOXED)
MARSHALLER (VOID__ENUM)
MARSHALLER (VOID__INT)
MARSHALLER (VOID__OBJECT)
MARSHALLER (VOID__POINTER)
MARSHALLER (VOID__STRING)
MARSHALLER (VOID__UINT)
MARSHALLER (VOID__VOID)

#define __GTK_MISC_C__
#include "gtkaliasdef.c"
