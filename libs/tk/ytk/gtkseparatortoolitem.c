/* gtkseparatortoolitem.c
 *
 * Copyright (C) 2002 Anders Carlsson <andersca@gnome.org>
 * Copyright (C) 2002 James Henstridge <james@daa.com.au>
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
#include "gtkseparatormenuitem.h"
#include "gtkseparatortoolitem.h"
#include "gtkintl.h"
#include "gtktoolbar.h"
#include "gtkprivate.h"
#include "gtkalias.h"

#define MENU_ID "gtk-separator-tool-item-menu-id"

enum {
  PROP_0,
  PROP_DRAW
};

static gboolean gtk_separator_tool_item_create_menu_proxy (GtkToolItem               *item);
static void     gtk_separator_tool_item_set_property      (GObject                   *object,
							   guint                      prop_id,
							   const GValue              *value,
							   GParamSpec                *pspec);
static void     gtk_separator_tool_item_get_property       (GObject                   *object,
							   guint                      prop_id,
							   GValue                    *value,
							   GParamSpec                *pspec);
static void     gtk_separator_tool_item_size_request      (GtkWidget                 *widget,
							   GtkRequisition            *requisition);
static gboolean gtk_separator_tool_item_expose            (GtkWidget                 *widget,
							   GdkEventExpose            *event);
static void     gtk_separator_tool_item_add               (GtkContainer              *container,
							   GtkWidget                 *child);
static gint     get_space_size                            (GtkToolItem               *tool_item);



#define GTK_SEPARATOR_TOOL_ITEM_GET_PRIVATE(obj)(G_TYPE_INSTANCE_GET_PRIVATE ((obj), GTK_TYPE_SEPARATOR_TOOL_ITEM, GtkSeparatorToolItemPrivate))

struct _GtkSeparatorToolItemPrivate
{
  guint draw : 1;
};

G_DEFINE_TYPE (GtkSeparatorToolItem, gtk_separator_tool_item, GTK_TYPE_TOOL_ITEM)

static gint
get_space_size (GtkToolItem *tool_item)
{
  gint space_size = _gtk_toolbar_get_default_space_size();
  GtkWidget *parent = GTK_WIDGET (tool_item)->parent;
  
  if (GTK_IS_TOOLBAR (parent))
    {
      gtk_widget_style_get (parent,
			    "space-size", &space_size,
			    NULL);
    }
  
  return space_size;
}

static void
gtk_separator_tool_item_class_init (GtkSeparatorToolItemClass *class)
{
  GObjectClass *object_class;
  GtkContainerClass *container_class;
  GtkToolItemClass *toolitem_class;
  GtkWidgetClass *widget_class;
  
  object_class = (GObjectClass *)class;
  container_class = (GtkContainerClass *)class;
  toolitem_class = (GtkToolItemClass *)class;
  widget_class = (GtkWidgetClass *)class;

  object_class->set_property = gtk_separator_tool_item_set_property;
  object_class->get_property = gtk_separator_tool_item_get_property;
  widget_class->size_request = gtk_separator_tool_item_size_request;
  widget_class->expose_event = gtk_separator_tool_item_expose;
  toolitem_class->create_menu_proxy = gtk_separator_tool_item_create_menu_proxy;
  
  container_class->add = gtk_separator_tool_item_add;
  
  g_object_class_install_property (object_class,
				   PROP_DRAW,
				   g_param_spec_boolean ("draw",
							 P_("Draw"),
							 P_("Whether the separator is drawn, or just blank"),
							 TRUE,
							 GTK_PARAM_READWRITE));
  
  g_type_class_add_private (object_class, sizeof (GtkSeparatorToolItemPrivate));
}

static void
gtk_separator_tool_item_init (GtkSeparatorToolItem      *separator_item)
{
  separator_item->priv = GTK_SEPARATOR_TOOL_ITEM_GET_PRIVATE (separator_item);
  separator_item->priv->draw = TRUE;
}

static void
gtk_separator_tool_item_add (GtkContainer *container,
			     GtkWidget    *child)
{
  g_warning ("attempt to add a child to an GtkSeparatorToolItem");
}

static gboolean
gtk_separator_tool_item_create_menu_proxy (GtkToolItem *item)
{
  GtkWidget *menu_item = NULL;
  
  menu_item = gtk_separator_menu_item_new();
  
  gtk_tool_item_set_proxy_menu_item (item, MENU_ID, menu_item);
  
  return TRUE;
}

static void
gtk_separator_tool_item_set_property (GObject      *object,
				      guint         prop_id,
				      const GValue *value,
				      GParamSpec   *pspec)
{
  GtkSeparatorToolItem *item = GTK_SEPARATOR_TOOL_ITEM (object);
  
  switch (prop_id)
    {
    case PROP_DRAW:
      gtk_separator_tool_item_set_draw (item, g_value_get_boolean (value));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
gtk_separator_tool_item_get_property (GObject      *object,
				      guint         prop_id,
				      GValue       *value,
				      GParamSpec   *pspec)
{
  GtkSeparatorToolItem *item = GTK_SEPARATOR_TOOL_ITEM (object);
  
  switch (prop_id)
    {
    case PROP_DRAW:
      g_value_set_boolean (value, gtk_separator_tool_item_get_draw (item));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
gtk_separator_tool_item_size_request (GtkWidget      *widget,
				      GtkRequisition *requisition)
{
  GtkToolItem *item = GTK_TOOL_ITEM (widget);
  GtkOrientation orientation = gtk_tool_item_get_orientation (item);
  
  if (orientation == GTK_ORIENTATION_HORIZONTAL)
    {
      requisition->width = get_space_size (item);
      requisition->height = 1;
    }
  else
    {
      requisition->height = get_space_size (item);
      requisition->width = 1;
    }
}

static gboolean
gtk_separator_tool_item_expose (GtkWidget      *widget,
				GdkEventExpose *event)
{
  GtkToolbar *toolbar = NULL;
  GtkSeparatorToolItemPrivate *priv =
      GTK_SEPARATOR_TOOL_ITEM_GET_PRIVATE (widget);

  if (priv->draw)
    {
      if (GTK_IS_TOOLBAR (widget->parent))
	toolbar = GTK_TOOLBAR (widget->parent);

      _gtk_toolbar_paint_space_line (widget, toolbar,
				     &(event->area), &widget->allocation);
    }
  
  return FALSE;
}

/**
 * gtk_separator_tool_item_new:
 * 
 * Create a new #GtkSeparatorToolItem
 * 
 * Return value: the new #GtkSeparatorToolItem
 * 
 * Since: 2.4
 */
GtkToolItem *
gtk_separator_tool_item_new (void)
{
  GtkToolItem *self;
  
  self = g_object_new (GTK_TYPE_SEPARATOR_TOOL_ITEM,
		       NULL);
  
  return self;
}

/**
 * gtk_separator_tool_item_get_draw:
 * @item: a #GtkSeparatorToolItem 
 * 
 * Returns whether @item is drawn as a line, or just blank. 
 * See gtk_separator_tool_item_set_draw().
 * 
 * Return value: %TRUE if @item is drawn as a line, or just blank.
 * 
 * Since: 2.4
 */
gboolean
gtk_separator_tool_item_get_draw (GtkSeparatorToolItem *item)
{
  g_return_val_if_fail (GTK_IS_SEPARATOR_TOOL_ITEM (item), FALSE);
  
  return item->priv->draw;
}

/**
 * gtk_separator_tool_item_set_draw:
 * @item: a #GtkSeparatorToolItem
 * @draw: whether @item is drawn as a vertical line
 * 
 * Whether @item is drawn as a vertical line, or just blank.
 * Setting this to %FALSE along with gtk_tool_item_set_expand() is useful
 * to create an item that forces following items to the end of the toolbar.
 * 
 * Since: 2.4
 */
void
gtk_separator_tool_item_set_draw (GtkSeparatorToolItem *item,
				  gboolean              draw)
{
  g_return_if_fail (GTK_IS_SEPARATOR_TOOL_ITEM (item));

  draw = draw != FALSE;

  if (draw != item->priv->draw)
    {
      item->priv->draw = draw;

      gtk_widget_queue_draw (GTK_WIDGET (item));

      g_object_notify (G_OBJECT (item), "draw");
    }
}

#define __GTK_SEPARATOR_TOOL_ITEM_C__
#include "gtkaliasdef.c"
