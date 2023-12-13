/* GTK - The GIMP Toolkit
 * gtkfilechooserwidget.c: Embeddable file selector widget
 * Copyright (C) 2003, Red Hat, Inc.
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

#include "config.h"
#include "gtkfilechooserprivate.h"

#include "gtkfilechooserwidget.h"
#include "gtkfilechooserdefault.h"
#include "gtkfilechooserutils.h"
#include "gtktypebuiltins.h"
#include "gtkfilechooserembed.h"
#include "gtkintl.h"
#include "gtkalias.h"

#define GTK_FILE_CHOOSER_WIDGET_GET_PRIVATE(o)  (GTK_FILE_CHOOSER_WIDGET (o)->priv)

static void gtk_file_chooser_widget_finalize     (GObject                   *object);

static GObject* gtk_file_chooser_widget_constructor  (GType                  type,
						      guint                  n_construct_properties,
						      GObjectConstructParam *construct_params);
static void     gtk_file_chooser_widget_set_property (GObject               *object,
						      guint                  prop_id,
						      const GValue          *value,
						      GParamSpec            *pspec);
static void     gtk_file_chooser_widget_get_property (GObject               *object,
						      guint                  prop_id,
						      GValue                *value,
						      GParamSpec            *pspec);

G_DEFINE_TYPE_WITH_CODE (GtkFileChooserWidget, gtk_file_chooser_widget, GTK_TYPE_VBOX,
			 G_IMPLEMENT_INTERFACE (GTK_TYPE_FILE_CHOOSER,
						_gtk_file_chooser_delegate_iface_init)
			 G_IMPLEMENT_INTERFACE (GTK_TYPE_FILE_CHOOSER_EMBED,
						_gtk_file_chooser_embed_delegate_iface_init))

static void
gtk_file_chooser_widget_class_init (GtkFileChooserWidgetClass *class)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (class);

  gobject_class->constructor = gtk_file_chooser_widget_constructor;
  gobject_class->set_property = gtk_file_chooser_widget_set_property;
  gobject_class->get_property = gtk_file_chooser_widget_get_property;
  gobject_class->finalize = gtk_file_chooser_widget_finalize;

  _gtk_file_chooser_install_properties (gobject_class);

  g_type_class_add_private (class, sizeof (GtkFileChooserWidgetPrivate));
}

static void
gtk_file_chooser_widget_init (GtkFileChooserWidget *chooser_widget)
{
  GtkFileChooserWidgetPrivate *priv = G_TYPE_INSTANCE_GET_PRIVATE (chooser_widget,
								   GTK_TYPE_FILE_CHOOSER_WIDGET,
								   GtkFileChooserWidgetPrivate);
  chooser_widget->priv = priv;
}

static void
gtk_file_chooser_widget_finalize (GObject *object)
{
  GtkFileChooserWidget *chooser = GTK_FILE_CHOOSER_WIDGET (object);

  g_free (chooser->priv->file_system);

  G_OBJECT_CLASS (gtk_file_chooser_widget_parent_class)->finalize (object);
}

static GObject*
gtk_file_chooser_widget_constructor (GType                  type,
				     guint                  n_construct_properties,
				     GObjectConstructParam *construct_params)
{
  GtkFileChooserWidgetPrivate *priv;
  GObject *object;
  
  object = G_OBJECT_CLASS (gtk_file_chooser_widget_parent_class)->constructor (type,
									       n_construct_properties,
									       construct_params);
  priv = GTK_FILE_CHOOSER_WIDGET_GET_PRIVATE (object);

  gtk_widget_push_composite_child ();

  priv->impl = _gtk_file_chooser_default_new ();
  
  gtk_box_pack_start (GTK_BOX (object), priv->impl, TRUE, TRUE, 0);
  gtk_widget_show (priv->impl);

  _gtk_file_chooser_set_delegate (GTK_FILE_CHOOSER (object),
				  GTK_FILE_CHOOSER (priv->impl));

  _gtk_file_chooser_embed_set_delegate (GTK_FILE_CHOOSER_EMBED (object),
					GTK_FILE_CHOOSER_EMBED (priv->impl));
  
  gtk_widget_pop_composite_child ();

  return object;
}

static void
gtk_file_chooser_widget_set_property (GObject         *object,
				      guint            prop_id,
				      const GValue    *value,
				      GParamSpec      *pspec)
{
  GtkFileChooserWidgetPrivate *priv = GTK_FILE_CHOOSER_WIDGET_GET_PRIVATE (object);

  switch (prop_id)
    {
    case GTK_FILE_CHOOSER_PROP_FILE_SYSTEM_BACKEND:
      g_free (priv->file_system);
      priv->file_system = g_value_dup_string (value);
      break;
    default:
      g_object_set_property (G_OBJECT (priv->impl), pspec->name, value);
      break;
    }
}

static void
gtk_file_chooser_widget_get_property (GObject         *object,
				      guint            prop_id,
				      GValue          *value,
				      GParamSpec      *pspec)
{
  GtkFileChooserWidgetPrivate *priv = GTK_FILE_CHOOSER_WIDGET_GET_PRIVATE (object);
  
  g_object_get_property (G_OBJECT (priv->impl), pspec->name, value);
}

/**
 * gtk_file_chooser_widget_new:
 * @action: Open or save mode for the widget
 * 
 * Creates a new #GtkFileChooserWidget.  This is a file chooser widget that can
 * be embedded in custom windows, and it is the same widget that is used by
 * #GtkFileChooserDialog.
 * 
 * Return value: a new #GtkFileChooserWidget
 *
 * Since: 2.4
 **/
GtkWidget *
gtk_file_chooser_widget_new (GtkFileChooserAction action)
{
  return g_object_new (GTK_TYPE_FILE_CHOOSER_WIDGET,
		       "action", action,
		       NULL);
}

/**
 * gtk_file_chooser_widget_new_with_backend:
 * @action: Open or save mode for the widget
 * @backend: The name of the specific filesystem backend to use.
 * 
 * Creates a new #GtkFileChooserWidget with a specified backend.  This is
 * especially useful if you use gtk_file_chooser_set_local_only() to allow
 * non-local files.  This is a file chooser widget that can be embedded in
 * custom windows and it is the same widget that is used by
 * #GtkFileChooserDialog.
 * 
 * Return value: a new #GtkFileChooserWidget
 *
 * Since: 2.4
 * Deprecated: 2.14: Use gtk_file_chooser_widget_new() instead.
 **/
GtkWidget *
gtk_file_chooser_widget_new_with_backend (GtkFileChooserAction  action,
					  const gchar          *backend)
{
  return g_object_new (GTK_TYPE_FILE_CHOOSER_WIDGET,
		       "action", action,
		       NULL);
}

#define __GTK_FILE_CHOOSER_WIDGET_C__
#include "gtkaliasdef.c"
