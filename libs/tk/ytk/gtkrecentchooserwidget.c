/* GTK - The GIMP Toolkit
 * gtkrecentchooserwidget.c: embeddable recently used resources chooser widget
 * Copyright (C) 2006 Emmanuele Bassi
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

#include "gtkrecentchooserwidget.h"
#include "gtkrecentchooserdefault.h"
#include "gtkrecentchooserutils.h"
#include "gtktypebuiltins.h"
#include "gtkalias.h"

struct _GtkRecentChooserWidgetPrivate
{
  GtkRecentManager *manager;
  
  GtkWidget *chooser;
};

#define GTK_RECENT_CHOOSER_WIDGET_GET_PRIVATE(obj)	(GTK_RECENT_CHOOSER_WIDGET (obj)->priv)

static GObject *gtk_recent_chooser_widget_constructor  (GType                  type,
						        guint                  n_params,
						        GObjectConstructParam *params);
static void     gtk_recent_chooser_widget_set_property (GObject               *object,
						        guint                  prop_id,
						        const GValue          *value,
						        GParamSpec            *pspec);
static void     gtk_recent_chooser_widget_get_property (GObject               *object,
						        guint                  prop_id,
						        GValue                *value,
						        GParamSpec            *pspec);
static void     gtk_recent_chooser_widget_finalize     (GObject               *object);


G_DEFINE_TYPE_WITH_CODE (GtkRecentChooserWidget,
		         gtk_recent_chooser_widget,
			 GTK_TYPE_VBOX,
			 G_IMPLEMENT_INTERFACE (GTK_TYPE_RECENT_CHOOSER,
						_gtk_recent_chooser_delegate_iface_init))

static void
gtk_recent_chooser_widget_class_init (GtkRecentChooserWidgetClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

  gobject_class->constructor = gtk_recent_chooser_widget_constructor;
  gobject_class->set_property = gtk_recent_chooser_widget_set_property;
  gobject_class->get_property = gtk_recent_chooser_widget_get_property;
  gobject_class->finalize = gtk_recent_chooser_widget_finalize;

  _gtk_recent_chooser_install_properties (gobject_class);

  g_type_class_add_private (klass, sizeof (GtkRecentChooserWidgetPrivate));
}


static void
gtk_recent_chooser_widget_init (GtkRecentChooserWidget *widget)
{
  widget->priv = G_TYPE_INSTANCE_GET_PRIVATE (widget, GTK_TYPE_RECENT_CHOOSER_WIDGET,
					      GtkRecentChooserWidgetPrivate);
}

static GObject *
gtk_recent_chooser_widget_constructor (GType                  type,
				       guint                  n_params,
				       GObjectConstructParam *params)
{
  GObject *object;
  GtkRecentChooserWidgetPrivate *priv;

  object = G_OBJECT_CLASS (gtk_recent_chooser_widget_parent_class)->constructor (type,
										 n_params,
										 params);

  priv = GTK_RECENT_CHOOSER_WIDGET_GET_PRIVATE (object);
  priv->chooser = _gtk_recent_chooser_default_new (priv->manager);
  
  
  gtk_container_add (GTK_CONTAINER (object), priv->chooser);
  gtk_widget_show (priv->chooser);
  _gtk_recent_chooser_set_delegate (GTK_RECENT_CHOOSER (object),
				    GTK_RECENT_CHOOSER (priv->chooser));

  return object;
}

static void
gtk_recent_chooser_widget_set_property (GObject      *object,
				        guint         prop_id,
				        const GValue *value,
				        GParamSpec   *pspec)
{
  GtkRecentChooserWidgetPrivate *priv;

  priv = GTK_RECENT_CHOOSER_WIDGET_GET_PRIVATE (object);
  
  switch (prop_id)
    {
    case GTK_RECENT_CHOOSER_PROP_RECENT_MANAGER:
      priv->manager = g_value_get_object (value);
      break;
    default:
      g_object_set_property (G_OBJECT (priv->chooser), pspec->name, value);
      break;
    }
}

static void
gtk_recent_chooser_widget_get_property (GObject    *object,
				        guint       prop_id,
				        GValue     *value,
				        GParamSpec *pspec)
{
  GtkRecentChooserWidgetPrivate *priv;

  priv = GTK_RECENT_CHOOSER_WIDGET_GET_PRIVATE (object);

  g_object_get_property (G_OBJECT (priv->chooser), pspec->name, value);
}

static void
gtk_recent_chooser_widget_finalize (GObject *object)
{
  GtkRecentChooserWidgetPrivate *priv;
  
  priv = GTK_RECENT_CHOOSER_WIDGET_GET_PRIVATE (object);
  priv->manager = NULL;
  
  G_OBJECT_CLASS (gtk_recent_chooser_widget_parent_class)->finalize (object);
}

/*
 * Public API
 */

/**
 * gtk_recent_chooser_widget_new:
 * 
 * Creates a new #GtkRecentChooserWidget object.  This is an embeddable widget
 * used to access the recently used resources list.
 *
 * Return value: a new #GtkRecentChooserWidget
 *
 * Since: 2.10
 */
GtkWidget *
gtk_recent_chooser_widget_new (void)
{
  return g_object_new (GTK_TYPE_RECENT_CHOOSER_WIDGET, NULL);
}

/**
 * gtk_recent_chooser_widget_new_for_manager:
 * @manager: a #GtkRecentManager
 *
 * Creates a new #GtkRecentChooserWidget with a specified recent manager.
 *
 * This is useful if you have implemented your own recent manager, or if you
 * have a customized instance of a #GtkRecentManager object.
 *
 * Return value: a new #GtkRecentChooserWidget
 *
 * Since: 2.10
 */
GtkWidget *
gtk_recent_chooser_widget_new_for_manager (GtkRecentManager *manager)
{
  g_return_val_if_fail (manager == NULL || GTK_IS_RECENT_MANAGER (manager), NULL);
  
  return g_object_new (GTK_TYPE_RECENT_CHOOSER_WIDGET,
  		       "recent-manager", manager,
  		       NULL);
}

#define __GTK_RECENT_CHOOSER_WIDGET_C__
#include "gtkaliasdef.c"
