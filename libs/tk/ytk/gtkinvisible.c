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
#include <gdk/gdk.h>
#include "gtkinvisible.h"
#include "gtkprivate.h"
#include "gtkintl.h"
#include "gtkalias.h"

enum {
  PROP_0,
  PROP_SCREEN,
  LAST_ARG
};

static void gtk_invisible_destroy       (GtkObject         *object);
static void gtk_invisible_realize       (GtkWidget         *widget);
static void gtk_invisible_style_set     (GtkWidget         *widget,
					 GtkStyle          *previous_style);
static void gtk_invisible_show          (GtkWidget         *widget);
static void gtk_invisible_size_allocate (GtkWidget         *widget,
					 GtkAllocation     *allocation);
static void gtk_invisible_set_property  (GObject           *object,
					 guint              prop_id,
					 const GValue      *value,
					 GParamSpec        *pspec);
static void gtk_invisible_get_property  (GObject           *object,
					 guint              prop_id,
					 GValue		   *value,
					 GParamSpec        *pspec);

static GObject *gtk_invisible_constructor (GType                  type,
					   guint                  n_construct_properties,
					   GObjectConstructParam *construct_params);

G_DEFINE_TYPE (GtkInvisible, gtk_invisible, GTK_TYPE_WIDGET)

static void
gtk_invisible_class_init (GtkInvisibleClass *class)
{
  GObjectClass	 *gobject_class;
  GtkObjectClass *object_class;
  GtkWidgetClass *widget_class;

  widget_class = (GtkWidgetClass*) class;
  object_class = (GtkObjectClass*) class;
  gobject_class = (GObjectClass*) class;

  widget_class->realize = gtk_invisible_realize;
  widget_class->style_set = gtk_invisible_style_set;
  widget_class->show = gtk_invisible_show;
  widget_class->size_allocate = gtk_invisible_size_allocate;

  object_class->destroy = gtk_invisible_destroy;
  gobject_class->set_property = gtk_invisible_set_property;
  gobject_class->get_property = gtk_invisible_get_property;
  gobject_class->constructor = gtk_invisible_constructor;

  g_object_class_install_property (gobject_class,
				   PROP_SCREEN,
				   g_param_spec_object ("screen",
 							P_("Screen"),
 							P_("The screen where this window will be displayed"),
							GDK_TYPE_SCREEN,
 							GTK_PARAM_READWRITE));
}

static void
gtk_invisible_init (GtkInvisible *invisible)
{
  GdkColormap *colormap;
  
  gtk_widget_set_has_window (GTK_WIDGET (invisible), TRUE);
  _gtk_widget_set_is_toplevel (GTK_WIDGET (invisible), TRUE);

  g_object_ref_sink (invisible);

  invisible->has_user_ref_count = TRUE;
  invisible->screen = gdk_screen_get_default ();
  
  colormap = _gtk_widget_peek_colormap ();
  if (colormap)
    gtk_widget_set_colormap (GTK_WIDGET (invisible), colormap);
}

static void
gtk_invisible_destroy (GtkObject *object)
{
  GtkInvisible *invisible = GTK_INVISIBLE (object);
  
  if (invisible->has_user_ref_count)
    {
      invisible->has_user_ref_count = FALSE;
      g_object_unref (invisible);
    }

  GTK_OBJECT_CLASS (gtk_invisible_parent_class)->destroy (object);  
}

/**
 * gtk_invisible_new_for_screen:
 * @screen: a #GdkScreen which identifies on which
 *	    the new #GtkInvisible will be created.
 *
 * Creates a new #GtkInvisible object for a specified screen
 *
 * Return value: a newly created #GtkInvisible object
 *
 * Since: 2.2
 **/
GtkWidget* 
gtk_invisible_new_for_screen (GdkScreen *screen)
{
  g_return_val_if_fail (GDK_IS_SCREEN (screen), NULL);
  
  return g_object_new (GTK_TYPE_INVISIBLE, "screen", screen, NULL);
}

/**
 * gtk_invisible_new:
 * 
 * Creates a new #GtkInvisible.
 * 
 * Return value: a new #GtkInvisible.
 **/
GtkWidget*
gtk_invisible_new (void)
{
  return g_object_new (GTK_TYPE_INVISIBLE, NULL);
}

/**
 * gtk_invisible_set_screen:
 * @invisible: a #GtkInvisible.
 * @screen: a #GdkScreen.
 *
 * Sets the #GdkScreen where the #GtkInvisible object will be displayed.
 *
 * Since: 2.2
 **/ 
void
gtk_invisible_set_screen (GtkInvisible *invisible,
			  GdkScreen    *screen)
{
  GtkWidget *widget;
  GdkScreen *previous_screen;
  gboolean was_realized;
  
  g_return_if_fail (GTK_IS_INVISIBLE (invisible));
  g_return_if_fail (GDK_IS_SCREEN (screen));

  if (screen == invisible->screen)
    return;

  widget = GTK_WIDGET (invisible);

  previous_screen = invisible->screen;
  was_realized = gtk_widget_get_realized (widget);

  if (was_realized)
    gtk_widget_unrealize (widget);
  
  invisible->screen = screen;
  if (screen != previous_screen)
    _gtk_widget_propagate_screen_changed (widget, previous_screen);
  g_object_notify (G_OBJECT (invisible), "screen");
  
  if (was_realized)
    gtk_widget_realize (widget);
}

/**
 * gtk_invisible_get_screen:
 * @invisible: a #GtkInvisible.
 *
 * Returns the #GdkScreen object associated with @invisible
 *
 * Return value: (transfer none): the associated #GdkScreen.
 *
 * Since: 2.2
 **/
GdkScreen *
gtk_invisible_get_screen (GtkInvisible *invisible)
{
  g_return_val_if_fail (GTK_IS_INVISIBLE (invisible), NULL);
  
  return invisible->screen;
}

static void
gtk_invisible_realize (GtkWidget *widget)
{
  GdkWindow *parent;
  GdkWindowAttr attributes;
  gint attributes_mask;

  gtk_widget_set_realized (widget, TRUE);

  parent = gtk_widget_get_parent_window (widget);
  if (parent == NULL)
    parent = gtk_widget_get_root_window (widget);

  attributes.x = -100;
  attributes.y = -100;
  attributes.width = 10;
  attributes.height = 10;
  attributes.window_type = GDK_WINDOW_TEMP;
  attributes.wclass = GDK_INPUT_ONLY;
  attributes.override_redirect = TRUE;
  attributes.event_mask = gtk_widget_get_events (widget);

  attributes_mask = GDK_WA_X | GDK_WA_Y | GDK_WA_NOREDIR;

  widget->window = gdk_window_new (parent, &attributes, attributes_mask);
					      
  gdk_window_set_user_data (widget->window, widget);
  
  widget->style = gtk_style_attach (widget->style, widget->window);
}

static void
gtk_invisible_style_set (GtkWidget *widget,
			 GtkStyle  *previous_style)
{
  /* Don't chain up to parent implementation */
}

static void
gtk_invisible_show (GtkWidget *widget)
{
  GTK_WIDGET_SET_FLAGS (widget, GTK_VISIBLE);
  gtk_widget_map (widget);
}

static void
gtk_invisible_size_allocate (GtkWidget     *widget,
			    GtkAllocation *allocation)
{
  widget->allocation = *allocation;
} 


static void 
gtk_invisible_set_property  (GObject      *object,
			     guint         prop_id,
			     const GValue *value,
			     GParamSpec   *pspec)
{
  GtkInvisible *invisible = GTK_INVISIBLE (object);
  
  switch (prop_id)
    {
    case PROP_SCREEN:
      gtk_invisible_set_screen (invisible, g_value_get_object (value));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void 
gtk_invisible_get_property  (GObject      *object,
			     guint         prop_id,
			     GValue	  *value,
			     GParamSpec   *pspec)
{
  GtkInvisible *invisible = GTK_INVISIBLE (object);

  switch (prop_id)
    {
    case PROP_SCREEN:
      g_value_set_object (value, invisible->screen);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

/* We use a constructor here so that we can realize the invisible on
 * the correct screen after the "screen" property has been set
 */
static GObject*
gtk_invisible_constructor (GType                  type,
			   guint                  n_construct_properties,
			   GObjectConstructParam *construct_params)
{
  GObject *object;

  object = G_OBJECT_CLASS (gtk_invisible_parent_class)->constructor (type,
                                                                     n_construct_properties,
                                                                     construct_params);

  gtk_widget_realize (GTK_WIDGET (object));

  return object;
}

#define __GTK_INVISIBLE_C__
#include "gtkaliasdef.c"
