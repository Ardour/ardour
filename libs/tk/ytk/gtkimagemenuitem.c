/* GTK - The GIMP Toolkit
 * Copyright (C) 2001 Red Hat, Inc.
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
#include "gtkimagemenuitem.h"
#include "gtkaccellabel.h"
#include "gtkintl.h"
#include "gtkstock.h"
#include "gtkiconfactory.h"
#include "gtkimage.h"
#include "gtkmenubar.h"
#include "gtkcontainer.h"
#include "gtkwindow.h"
#include "gtkactivatable.h"
#include "gtkprivate.h"
#include "gtkalias.h"

static void gtk_image_menu_item_destroy              (GtkObject        *object);
static void gtk_image_menu_item_size_request         (GtkWidget        *widget,
                                                      GtkRequisition   *requisition);
static void gtk_image_menu_item_size_allocate        (GtkWidget        *widget,
                                                      GtkAllocation    *allocation);
static void gtk_image_menu_item_map                  (GtkWidget        *widget);
static void gtk_image_menu_item_remove               (GtkContainer     *container,
                                                      GtkWidget        *child);
static void gtk_image_menu_item_toggle_size_request  (GtkMenuItem      *menu_item,
						      gint             *requisition);
static void gtk_image_menu_item_set_label            (GtkMenuItem      *menu_item,
						      const gchar      *label);
static const gchar *gtk_image_menu_item_get_label (GtkMenuItem *menu_item);

static void gtk_image_menu_item_forall               (GtkContainer    *container,
						      gboolean	       include_internals,
						      GtkCallback      callback,
						      gpointer         callback_data);

static void gtk_image_menu_item_finalize             (GObject         *object);
static void gtk_image_menu_item_set_property         (GObject         *object,
						      guint            prop_id,
						      const GValue    *value,
						      GParamSpec      *pspec);
static void gtk_image_menu_item_get_property         (GObject         *object,
						      guint            prop_id,
						      GValue          *value,
						      GParamSpec      *pspec);
static void gtk_image_menu_item_screen_changed       (GtkWidget        *widget,
						      GdkScreen        *previous_screen);

static void gtk_image_menu_item_recalculate          (GtkImageMenuItem *image_menu_item);

static void gtk_image_menu_item_activatable_interface_init (GtkActivatableIface  *iface);
static void gtk_image_menu_item_update                     (GtkActivatable       *activatable,
							    GtkAction            *action,
							    const gchar          *property_name);
static void gtk_image_menu_item_sync_action_properties     (GtkActivatable       *activatable,
							    GtkAction            *action);

typedef struct {
  gchar *label;
  guint  use_stock         : 1;
  guint  always_show_image : 1;
} GtkImageMenuItemPrivate;

enum {
  PROP_0,
  PROP_IMAGE,
  PROP_USE_STOCK,
  PROP_ACCEL_GROUP,
  PROP_ALWAYS_SHOW_IMAGE
};

static GtkActivatableIface *parent_activatable_iface;


G_DEFINE_TYPE_WITH_CODE (GtkImageMenuItem, gtk_image_menu_item, GTK_TYPE_MENU_ITEM,
			 G_IMPLEMENT_INTERFACE (GTK_TYPE_ACTIVATABLE,
						gtk_image_menu_item_activatable_interface_init))

#define GET_PRIVATE(object)  \
  (G_TYPE_INSTANCE_GET_PRIVATE ((object), GTK_TYPE_IMAGE_MENU_ITEM, GtkImageMenuItemPrivate))

static void
gtk_image_menu_item_class_init (GtkImageMenuItemClass *klass)
{
  GObjectClass *gobject_class = (GObjectClass*) klass;
  GtkObjectClass *object_class = (GtkObjectClass*) klass;
  GtkWidgetClass *widget_class = (GtkWidgetClass*) klass;
  GtkMenuItemClass *menu_item_class = (GtkMenuItemClass*) klass;
  GtkContainerClass *container_class = (GtkContainerClass*) klass;

  object_class->destroy = gtk_image_menu_item_destroy;

  widget_class->screen_changed = gtk_image_menu_item_screen_changed;
  widget_class->size_request = gtk_image_menu_item_size_request;
  widget_class->size_allocate = gtk_image_menu_item_size_allocate;
  widget_class->map = gtk_image_menu_item_map;

  container_class->forall = gtk_image_menu_item_forall;
  container_class->remove = gtk_image_menu_item_remove;
  
  menu_item_class->toggle_size_request = gtk_image_menu_item_toggle_size_request;
  menu_item_class->set_label           = gtk_image_menu_item_set_label;
  menu_item_class->get_label           = gtk_image_menu_item_get_label;

  gobject_class->finalize     = gtk_image_menu_item_finalize;
  gobject_class->set_property = gtk_image_menu_item_set_property;
  gobject_class->get_property = gtk_image_menu_item_get_property;
  
  g_object_class_install_property (gobject_class,
                                   PROP_IMAGE,
                                   g_param_spec_object ("image",
                                                        P_("Image widget"),
                                                        P_("Child widget to appear next to the menu text"),
                                                        GTK_TYPE_WIDGET,
                                                        GTK_PARAM_READWRITE));
  /**
   * GtkImageMenuItem:use-stock:
   *
   * If %TRUE, the label set in the menuitem is used as a
   * stock id to select the stock item for the item.
   *
   * Since: 2.16
   **/
  g_object_class_install_property (gobject_class,
                                   PROP_USE_STOCK,
                                   g_param_spec_boolean ("use-stock",
							 P_("Use stock"),
							 P_("Whether to use the label text to create a stock menu item"),
							 FALSE,
							 GTK_PARAM_READWRITE | G_PARAM_CONSTRUCT));

  /**
   * GtkImageMenuItem:always-show-image:
   *
   * If %TRUE, the menu item will ignore the #GtkSettings:gtk-menu-images 
   * setting and always show the image, if available.
   *
   * Use this property if the menuitem would be useless or hard to use
   * without the image. 
   *
   * Since: 2.16
   **/
  g_object_class_install_property (gobject_class,
                                   PROP_ALWAYS_SHOW_IMAGE,
                                   g_param_spec_boolean ("always-show-image",
							 P_("Always show image"),
							 P_("Whether the image will always be shown"),
							 FALSE,
							 GTK_PARAM_READWRITE | G_PARAM_CONSTRUCT));

  /**
   * GtkImageMenuItem:accel-group:
   *
   * The Accel Group to use for stock accelerator keys
   *
   * Since: 2.16
   **/
  g_object_class_install_property (gobject_class,
                                   PROP_ACCEL_GROUP,
                                   g_param_spec_object ("accel-group",
							P_("Accel Group"),
							P_("The Accel Group to use for stock accelerator keys"),
							GTK_TYPE_ACCEL_GROUP,
							GTK_PARAM_WRITABLE));

  g_type_class_add_private (klass, sizeof (GtkImageMenuItemPrivate));
}

static void
gtk_image_menu_item_init (GtkImageMenuItem *image_menu_item)
{
  GtkImageMenuItemPrivate *priv = GET_PRIVATE (image_menu_item);

  priv->use_stock   = FALSE;
  priv->label  = NULL;

  image_menu_item->image = NULL;
}

static void 
gtk_image_menu_item_finalize (GObject *object)
{
  GtkImageMenuItemPrivate *priv = GET_PRIVATE (object);

  g_free (priv->label);
  priv->label  = NULL;

  G_OBJECT_CLASS (gtk_image_menu_item_parent_class)->finalize (object);
}

static void
gtk_image_menu_item_set_property (GObject         *object,
                                  guint            prop_id,
                                  const GValue    *value,
                                  GParamSpec      *pspec)
{
  GtkImageMenuItem *image_menu_item = GTK_IMAGE_MENU_ITEM (object);
  
  switch (prop_id)
    {
    case PROP_IMAGE:
      gtk_image_menu_item_set_image (image_menu_item, (GtkWidget *) g_value_get_object (value));
      break;
    case PROP_USE_STOCK:
      gtk_image_menu_item_set_use_stock (image_menu_item, g_value_get_boolean (value));
      break;
    case PROP_ALWAYS_SHOW_IMAGE:
      gtk_image_menu_item_set_always_show_image (image_menu_item, g_value_get_boolean (value));
      break;
    case PROP_ACCEL_GROUP:
      gtk_image_menu_item_set_accel_group (image_menu_item, g_value_get_object (value));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
gtk_image_menu_item_get_property (GObject         *object,
                                  guint            prop_id,
                                  GValue          *value,
                                  GParamSpec      *pspec)
{
  GtkImageMenuItem *image_menu_item = GTK_IMAGE_MENU_ITEM (object);
  
  switch (prop_id)
    {
    case PROP_IMAGE:
      g_value_set_object (value, gtk_image_menu_item_get_image (image_menu_item));
      break;
    case PROP_USE_STOCK:
      g_value_set_boolean (value, gtk_image_menu_item_get_use_stock (image_menu_item));      
      break;
    case PROP_ALWAYS_SHOW_IMAGE:
      g_value_set_boolean (value, gtk_image_menu_item_get_always_show_image (image_menu_item));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static gboolean
show_image (GtkImageMenuItem *image_menu_item)
{
  GtkImageMenuItemPrivate *priv = GET_PRIVATE (image_menu_item);
  GtkSettings *settings = gtk_widget_get_settings (GTK_WIDGET (image_menu_item));
  gboolean show;

  if (priv->always_show_image)
    show = TRUE;
  else
    g_object_get (settings, "gtk-menu-images", &show, NULL);

  return show;
}

static void
gtk_image_menu_item_map (GtkWidget *widget)
{
  GtkImageMenuItem *image_menu_item = GTK_IMAGE_MENU_ITEM (widget);

  GTK_WIDGET_CLASS (gtk_image_menu_item_parent_class)->map (widget);

  if (image_menu_item->image)
    g_object_set (image_menu_item->image,
                  "visible", show_image (image_menu_item),
                  NULL);
}

static void
gtk_image_menu_item_destroy (GtkObject *object)
{
  GtkImageMenuItem *image_menu_item = GTK_IMAGE_MENU_ITEM (object);

  if (image_menu_item->image)
    gtk_container_remove (GTK_CONTAINER (image_menu_item),
                          image_menu_item->image);

  GTK_OBJECT_CLASS (gtk_image_menu_item_parent_class)->destroy (object);
}

static void
gtk_image_menu_item_toggle_size_request (GtkMenuItem *menu_item,
					 gint        *requisition)
{
  GtkImageMenuItem *image_menu_item = GTK_IMAGE_MENU_ITEM (menu_item);
  GtkPackDirection pack_dir;
  
  if (GTK_IS_MENU_BAR (GTK_WIDGET (menu_item)->parent))
    pack_dir = gtk_menu_bar_get_child_pack_direction (GTK_MENU_BAR (GTK_WIDGET (menu_item)->parent));
  else
    pack_dir = GTK_PACK_DIRECTION_LTR;

  *requisition = 0;

  if (image_menu_item->image && gtk_widget_get_visible (image_menu_item->image))
    {
      GtkRequisition image_requisition;
      guint toggle_spacing;
      gtk_widget_get_child_requisition (image_menu_item->image,
                                        &image_requisition);

      gtk_widget_style_get (GTK_WIDGET (menu_item),
			    "toggle-spacing", &toggle_spacing,
			    NULL);
      
      if (pack_dir == GTK_PACK_DIRECTION_LTR || pack_dir == GTK_PACK_DIRECTION_RTL)
	{
	  if (image_requisition.width > 0)
	    *requisition = image_requisition.width + toggle_spacing;
	}
      else
	{
	  if (image_requisition.height > 0)
	    *requisition = image_requisition.height + toggle_spacing;
	}
    }
}

static void
gtk_image_menu_item_recalculate (GtkImageMenuItem *image_menu_item)
{
  GtkImageMenuItemPrivate *priv = GET_PRIVATE (image_menu_item);
  GtkStockItem             stock_item;
  GtkWidget               *image;
  const gchar             *resolved_label = priv->label;

  if (priv->use_stock && priv->label)
    {

      if (!image_menu_item->image)
	{
	  image = gtk_image_new_from_stock (priv->label, GTK_ICON_SIZE_MENU);
	  gtk_image_menu_item_set_image (image_menu_item, image);
	}

      if (gtk_stock_lookup (priv->label, &stock_item))
	  resolved_label = stock_item.label;

	gtk_menu_item_set_use_underline (GTK_MENU_ITEM (image_menu_item), TRUE);
    }

  GTK_MENU_ITEM_CLASS
    (gtk_image_menu_item_parent_class)->set_label (GTK_MENU_ITEM (image_menu_item), resolved_label);

}

static void 
gtk_image_menu_item_set_label (GtkMenuItem      *menu_item,
			       const gchar      *label)
{
  GtkImageMenuItemPrivate *priv = GET_PRIVATE (menu_item);

  if (priv->label != label)
    {
      g_free (priv->label);
      priv->label = g_strdup (label);

      gtk_image_menu_item_recalculate (GTK_IMAGE_MENU_ITEM (menu_item));

      g_object_notify (G_OBJECT (menu_item), "label");

    }
}

static const gchar *
gtk_image_menu_item_get_label (GtkMenuItem *menu_item)
{
  GtkImageMenuItemPrivate *priv = GET_PRIVATE (menu_item);
  
  return priv->label;
}

static void
gtk_image_menu_item_size_request (GtkWidget      *widget,
                                  GtkRequisition *requisition)
{
  GtkImageMenuItem *image_menu_item;
  gint child_width = 0;
  gint child_height = 0;
  GtkPackDirection pack_dir;
  
  if (GTK_IS_MENU_BAR (widget->parent))
    pack_dir = gtk_menu_bar_get_child_pack_direction (GTK_MENU_BAR (widget->parent));
  else
    pack_dir = GTK_PACK_DIRECTION_LTR;

  image_menu_item = GTK_IMAGE_MENU_ITEM (widget);

  if (image_menu_item->image && gtk_widget_get_visible (image_menu_item->image))
    {
      GtkRequisition child_requisition;
      
      gtk_widget_size_request (image_menu_item->image,
                               &child_requisition);

      child_width = child_requisition.width;
      child_height = child_requisition.height;
    }

  GTK_WIDGET_CLASS (gtk_image_menu_item_parent_class)->size_request (widget, requisition);

  /* not done with height since that happens via the
   * toggle_size_request
   */
  if (pack_dir == GTK_PACK_DIRECTION_LTR || pack_dir == GTK_PACK_DIRECTION_RTL)
    requisition->height = MAX (requisition->height, child_height);
  else
    requisition->width = MAX (requisition->width, child_width);
    
  
  /* Note that GtkMenuShell always size requests before
   * toggle_size_request, so toggle_size_request will be able to use
   * image_menu_item->image->requisition
   */
}

static void
gtk_image_menu_item_size_allocate (GtkWidget     *widget,
                                   GtkAllocation *allocation)
{
  GtkImageMenuItem *image_menu_item;
  GtkPackDirection pack_dir;
  
  if (GTK_IS_MENU_BAR (widget->parent))
    pack_dir = gtk_menu_bar_get_child_pack_direction (GTK_MENU_BAR (widget->parent));
  else
    pack_dir = GTK_PACK_DIRECTION_LTR;
  
  image_menu_item = GTK_IMAGE_MENU_ITEM (widget);  

  GTK_WIDGET_CLASS (gtk_image_menu_item_parent_class)->size_allocate (widget, allocation);

  if (image_menu_item->image && gtk_widget_get_visible (image_menu_item->image))
    {
      gint x, y, offset;
      GtkRequisition child_requisition;
      GtkAllocation child_allocation;
      guint horizontal_padding, toggle_spacing;

      gtk_widget_style_get (widget,
			    "horizontal-padding", &horizontal_padding,
			    "toggle-spacing", &toggle_spacing,
			    NULL);
      
      /* Man this is lame hardcoding action, but I can't
       * come up with a solution that's really better.
       */

      gtk_widget_get_child_requisition (image_menu_item->image,
                                        &child_requisition);

      if (pack_dir == GTK_PACK_DIRECTION_LTR ||
	  pack_dir == GTK_PACK_DIRECTION_RTL)
	{
	  offset = GTK_CONTAINER (image_menu_item)->border_width +
	    widget->style->xthickness;
	  
	  if ((gtk_widget_get_direction (widget) == GTK_TEXT_DIR_LTR) ==
	      (pack_dir == GTK_PACK_DIRECTION_LTR))
	    x = offset + horizontal_padding +
	      (GTK_MENU_ITEM (image_menu_item)->toggle_size -
	       toggle_spacing - child_requisition.width) / 2;
	  else
	    x = widget->allocation.width - offset - horizontal_padding -
	      GTK_MENU_ITEM (image_menu_item)->toggle_size + toggle_spacing +
	      (GTK_MENU_ITEM (image_menu_item)->toggle_size -
	       toggle_spacing - child_requisition.width) / 2;
	  
	  y = (widget->allocation.height - child_requisition.height) / 2;
	}
      else
	{
	  offset = GTK_CONTAINER (image_menu_item)->border_width +
	    widget->style->ythickness;
	  
	  if ((gtk_widget_get_direction (widget) == GTK_TEXT_DIR_LTR) ==
	      (pack_dir == GTK_PACK_DIRECTION_TTB))
	    y = offset + horizontal_padding +
	      (GTK_MENU_ITEM (image_menu_item)->toggle_size -
	       toggle_spacing - child_requisition.height) / 2;
	  else
	    y = widget->allocation.height - offset - horizontal_padding -
	      GTK_MENU_ITEM (image_menu_item)->toggle_size + toggle_spacing +
	      (GTK_MENU_ITEM (image_menu_item)->toggle_size -
	       toggle_spacing - child_requisition.height) / 2;

	  x = (widget->allocation.width - child_requisition.width) / 2;
	}
      
      child_allocation.width = child_requisition.width;
      child_allocation.height = child_requisition.height;
      child_allocation.x = widget->allocation.x + MAX (x, 0);
      child_allocation.y = widget->allocation.y + MAX (y, 0);

      gtk_widget_size_allocate (image_menu_item->image, &child_allocation);
    }
}

static void
gtk_image_menu_item_forall (GtkContainer   *container,
                            gboolean	    include_internals,
                            GtkCallback     callback,
                            gpointer        callback_data)
{
  GtkImageMenuItem *image_menu_item = GTK_IMAGE_MENU_ITEM (container);

  GTK_CONTAINER_CLASS (gtk_image_menu_item_parent_class)->forall (container,
                                                                  include_internals,
                                                                  callback,
                                                                  callback_data);

  if (include_internals && image_menu_item->image)
    (* callback) (image_menu_item->image, callback_data);
}


static void 
gtk_image_menu_item_activatable_interface_init (GtkActivatableIface  *iface)
{
  parent_activatable_iface = g_type_interface_peek_parent (iface);
  iface->update = gtk_image_menu_item_update;
  iface->sync_action_properties = gtk_image_menu_item_sync_action_properties;
}

static gboolean
activatable_update_stock_id (GtkImageMenuItem *image_menu_item, GtkAction *action)
{
  GtkWidget   *image;
  const gchar *stock_id  = gtk_action_get_stock_id (action);

  image = gtk_image_menu_item_get_image (image_menu_item);
	  
  if (GTK_IS_IMAGE (image) &&
      stock_id && gtk_icon_factory_lookup_default (stock_id))
    {
      gtk_image_set_from_stock (GTK_IMAGE (image), stock_id, GTK_ICON_SIZE_MENU);
      return TRUE;
    }

  return FALSE;
}

static gboolean
activatable_update_gicon (GtkImageMenuItem *image_menu_item, GtkAction *action)
{
  GtkWidget   *image;
  GIcon       *icon = gtk_action_get_gicon (action);
  const gchar *stock_id = gtk_action_get_stock_id (action);

  image = gtk_image_menu_item_get_image (image_menu_item);

  if (icon && GTK_IS_IMAGE (image) &&
      !(stock_id && gtk_icon_factory_lookup_default (stock_id)))
    {
      gtk_image_set_from_gicon (GTK_IMAGE (image), icon, GTK_ICON_SIZE_MENU);
      return TRUE;
    }

  return FALSE;
}

static void
activatable_update_icon_name (GtkImageMenuItem *image_menu_item, GtkAction *action)
{
  GtkWidget   *image;
  const gchar *icon_name = gtk_action_get_icon_name (action);

  image = gtk_image_menu_item_get_image (image_menu_item);
	  
  if (GTK_IS_IMAGE (image) && 
      (gtk_image_get_storage_type (GTK_IMAGE (image)) == GTK_IMAGE_EMPTY ||
       gtk_image_get_storage_type (GTK_IMAGE (image)) == GTK_IMAGE_ICON_NAME))
    {
      gtk_image_set_from_icon_name (GTK_IMAGE (image), icon_name, GTK_ICON_SIZE_MENU);
    }
}

static void
gtk_image_menu_item_update (GtkActivatable *activatable,
			    GtkAction      *action,
			    const gchar    *property_name)
{
  GtkImageMenuItem *image_menu_item;
  gboolean   use_appearance;

  image_menu_item = GTK_IMAGE_MENU_ITEM (activatable);

  parent_activatable_iface->update (activatable, action, property_name);

  use_appearance = gtk_activatable_get_use_action_appearance (activatable);
  if (!use_appearance)
    return;

  if (strcmp (property_name, "stock-id") == 0)
    activatable_update_stock_id (image_menu_item, action);
  else if (strcmp (property_name, "gicon") == 0)
    activatable_update_gicon (image_menu_item, action);
  else if (strcmp (property_name, "icon-name") == 0)
    activatable_update_icon_name (image_menu_item, action);
}

static void 
gtk_image_menu_item_sync_action_properties (GtkActivatable *activatable,
			                    GtkAction      *action)
{
  GtkImageMenuItem *image_menu_item;
  GtkWidget *image;
  gboolean   use_appearance;

  image_menu_item = GTK_IMAGE_MENU_ITEM (activatable);

  parent_activatable_iface->sync_action_properties (activatable, action);

  if (!action)
    return;

  use_appearance = gtk_activatable_get_use_action_appearance (activatable);
  if (!use_appearance)
    return;

  image = gtk_image_menu_item_get_image (image_menu_item);
  if (image && !GTK_IS_IMAGE (image))
    {
      gtk_image_menu_item_set_image (image_menu_item, NULL);
      image = NULL;
    }
  
  if (!image)
    {
      image = gtk_image_new ();
      gtk_widget_show (image);
      gtk_image_menu_item_set_image (GTK_IMAGE_MENU_ITEM (activatable),
				     image);
    }
  
  if (!activatable_update_stock_id (image_menu_item, action) &&
      !activatable_update_gicon (image_menu_item, action))
    activatable_update_icon_name (image_menu_item, action);

  gtk_image_menu_item_set_always_show_image (image_menu_item,
                                             gtk_action_get_always_show_image (action));
}


/**
 * gtk_image_menu_item_new:
 * @returns: a new #GtkImageMenuItem.
 *
 * Creates a new #GtkImageMenuItem with an empty label.
 **/
GtkWidget*
gtk_image_menu_item_new (void)
{
  return g_object_new (GTK_TYPE_IMAGE_MENU_ITEM, NULL);
}

/**
 * gtk_image_menu_item_new_with_label:
 * @label: the text of the menu item.
 * @returns: a new #GtkImageMenuItem.
 *
 * Creates a new #GtkImageMenuItem containing a label. 
 **/
GtkWidget*
gtk_image_menu_item_new_with_label (const gchar *label)
{
  return g_object_new (GTK_TYPE_IMAGE_MENU_ITEM, 
		       "label", label,
		       NULL);
}


/**
 * gtk_image_menu_item_new_with_mnemonic:
 * @label: the text of the menu item, with an underscore in front of the
 *         mnemonic character
 * @returns: a new #GtkImageMenuItem
 *
 * Creates a new #GtkImageMenuItem containing a label. The label
 * will be created using gtk_label_new_with_mnemonic(), so underscores
 * in @label indicate the mnemonic for the menu item.
 **/
GtkWidget*
gtk_image_menu_item_new_with_mnemonic (const gchar *label)
{
  return g_object_new (GTK_TYPE_IMAGE_MENU_ITEM, 
		       "use-underline", TRUE,
		       "label", label,
		       NULL);
}

/**
 * gtk_image_menu_item_new_from_stock:
 * @stock_id: the name of the stock item.
 * @accel_group: (allow-none): the #GtkAccelGroup to add the menu items 
 *   accelerator to, or %NULL.
 * @returns: a new #GtkImageMenuItem.
 *
 * Creates a new #GtkImageMenuItem containing the image and text from a 
 * stock item. Some stock ids have preprocessor macros like #GTK_STOCK_OK 
 * and #GTK_STOCK_APPLY.
 *
 * If you want this menu item to have changeable accelerators, then pass in
 * %NULL for accel_group. Next call gtk_menu_item_set_accel_path() with an
 * appropriate path for the menu item, use gtk_stock_lookup() to look up the
 * standard accelerator for the stock item, and if one is found, call
 * gtk_accel_map_add_entry() to register it.
 **/
GtkWidget*
gtk_image_menu_item_new_from_stock (const gchar      *stock_id,
				    GtkAccelGroup    *accel_group)
{
  return g_object_new (GTK_TYPE_IMAGE_MENU_ITEM, 
		       "label", stock_id,
		       "use-stock", TRUE,
		       "accel-group", accel_group,
		       NULL);
}

/**
 * gtk_image_menu_item_set_use_stock:
 * @image_menu_item: a #GtkImageMenuItem
 * @use_stock: %TRUE if the menuitem should use a stock item
 *
 * If %TRUE, the label set in the menuitem is used as a
 * stock id to select the stock item for the item.
 *
 * Since: 2.16
 */
void
gtk_image_menu_item_set_use_stock (GtkImageMenuItem *image_menu_item,
				   gboolean          use_stock)
{
  GtkImageMenuItemPrivate *priv;

  g_return_if_fail (GTK_IS_IMAGE_MENU_ITEM (image_menu_item));

  priv = GET_PRIVATE (image_menu_item);

  if (priv->use_stock != use_stock)
    {
      priv->use_stock = use_stock;

      gtk_image_menu_item_recalculate (image_menu_item);

      g_object_notify (G_OBJECT (image_menu_item), "use-stock");
    }
}

/**
 * gtk_image_menu_item_get_use_stock:
 * @image_menu_item: a #GtkImageMenuItem
 *
 * Checks whether the label set in the menuitem is used as a
 * stock id to select the stock item for the item.
 *
 * Returns: %TRUE if the label set in the menuitem is used as a
 *     stock id to select the stock item for the item
 *
 * Since: 2.16
 */
gboolean
gtk_image_menu_item_get_use_stock (GtkImageMenuItem *image_menu_item)
{
  GtkImageMenuItemPrivate *priv;

  g_return_val_if_fail (GTK_IS_IMAGE_MENU_ITEM (image_menu_item), FALSE);

  priv = GET_PRIVATE (image_menu_item);

  return priv->use_stock;
}

/**
 * gtk_image_menu_item_set_always_show_image:
 * @image_menu_item: a #GtkImageMenuItem
 * @always_show: %TRUE if the menuitem should always show the image
 *
 * If %TRUE, the menu item will ignore the #GtkSettings:gtk-menu-images 
 * setting and always show the image, if available.
 *
 * Use this property if the menuitem would be useless or hard to use
 * without the image. 
 * 
 * Since: 2.16
 */
void
gtk_image_menu_item_set_always_show_image (GtkImageMenuItem *image_menu_item,
                                           gboolean          always_show)
{
  GtkImageMenuItemPrivate *priv;

  g_return_if_fail (GTK_IS_IMAGE_MENU_ITEM (image_menu_item));

  priv = GET_PRIVATE (image_menu_item);

  if (priv->always_show_image != always_show)
    {
      priv->always_show_image = always_show;

      if (image_menu_item->image)
        {
          if (show_image (image_menu_item))
            gtk_widget_show (image_menu_item->image);
          else
            gtk_widget_hide (image_menu_item->image);
        }

      g_object_notify (G_OBJECT (image_menu_item), "always-show-image");
    }
}

/**
 * gtk_image_menu_item_get_always_show_image:
 * @image_menu_item: a #GtkImageMenuItem
 *
 * Returns whether the menu item will ignore the #GtkSettings:gtk-menu-images
 * setting and always show the image, if available.
 * 
 * Returns: %TRUE if the menu item will always show the image
 *
 * Since: 2.16
 */
gboolean
gtk_image_menu_item_get_always_show_image (GtkImageMenuItem *image_menu_item)
{
  GtkImageMenuItemPrivate *priv;

  g_return_val_if_fail (GTK_IS_IMAGE_MENU_ITEM (image_menu_item), FALSE);

  priv = GET_PRIVATE (image_menu_item);

  return priv->always_show_image;
}


/**
 * gtk_image_menu_item_set_accel_group:
 * @image_menu_item: a #GtkImageMenuItem
 * @accel_group: the #GtkAccelGroup
 *
 * Specifies an @accel_group to add the menu items accelerator to
 * (this only applies to stock items so a stock item must already
 * be set, make sure to call gtk_image_menu_item_set_use_stock()
 * and gtk_menu_item_set_label() with a valid stock item first).
 *
 * If you want this menu item to have changeable accelerators then
 * you shouldnt need this (see gtk_image_menu_item_new_from_stock()).
 *
 * Since: 2.16
 */
void
gtk_image_menu_item_set_accel_group (GtkImageMenuItem *image_menu_item, 
				     GtkAccelGroup    *accel_group)
{
  GtkImageMenuItemPrivate *priv;
  GtkStockItem             stock_item;

  /* Silent return for the constructor */
  if (!accel_group) 
    return;
  
  g_return_if_fail (GTK_IS_IMAGE_MENU_ITEM (image_menu_item));
  g_return_if_fail (GTK_IS_ACCEL_GROUP (accel_group));

  priv = GET_PRIVATE (image_menu_item);

  if (priv->use_stock && priv->label && gtk_stock_lookup (priv->label, &stock_item))
    if (stock_item.keyval)
      {
	gtk_widget_add_accelerator (GTK_WIDGET (image_menu_item),
				    "activate",
				    accel_group,
				    stock_item.keyval,
				    stock_item.modifier,
				    GTK_ACCEL_VISIBLE);
	
	g_object_notify (G_OBJECT (image_menu_item), "accel-group");
      }
}

/** 
 * gtk_image_menu_item_set_image:
 * @image_menu_item: a #GtkImageMenuItem.
 * @image: (allow-none): a widget to set as the image for the menu item.
 *
 * Sets the image of @image_menu_item to the given widget.
 * Note that it depends on the show-menu-images setting whether
 * the image will be displayed or not.
 **/ 
void
gtk_image_menu_item_set_image (GtkImageMenuItem *image_menu_item,
                               GtkWidget        *image)
{
  g_return_if_fail (GTK_IS_IMAGE_MENU_ITEM (image_menu_item));

  if (image == image_menu_item->image)
    return;

  if (image_menu_item->image)
    gtk_container_remove (GTK_CONTAINER (image_menu_item),
			  image_menu_item->image);

  image_menu_item->image = image;

  if (image == NULL)
    return;

  gtk_widget_set_parent (image, GTK_WIDGET (image_menu_item));
  g_object_set (image,
		"visible", show_image (image_menu_item),
		"no-show-all", TRUE,
		NULL);

  g_object_notify (G_OBJECT (image_menu_item), "image");
}

/**
 * gtk_image_menu_item_get_image:
 * @image_menu_item: a #GtkImageMenuItem
 *
 * Gets the widget that is currently set as the image of @image_menu_item.
 * See gtk_image_menu_item_set_image().
 *
 * Return value: (transfer none): the widget set as image of @image_menu_item
 **/
GtkWidget*
gtk_image_menu_item_get_image (GtkImageMenuItem *image_menu_item)
{
  g_return_val_if_fail (GTK_IS_IMAGE_MENU_ITEM (image_menu_item), NULL);

  return image_menu_item->image;
}

static void
gtk_image_menu_item_remove (GtkContainer *container,
                            GtkWidget    *child)
{
  GtkImageMenuItem *image_menu_item;

  image_menu_item = GTK_IMAGE_MENU_ITEM (container);

  if (child == image_menu_item->image)
    {
      gboolean widget_was_visible;
      
      widget_was_visible = gtk_widget_get_visible (child);
      
      gtk_widget_unparent (child);
      image_menu_item->image = NULL;
      
      if (widget_was_visible &&
          gtk_widget_get_visible (GTK_WIDGET (container)))
        gtk_widget_queue_resize (GTK_WIDGET (container));

      g_object_notify (G_OBJECT (image_menu_item), "image");
    }
  else
    {
      GTK_CONTAINER_CLASS (gtk_image_menu_item_parent_class)->remove (container, child);
    }
}

static void 
show_image_change_notify (GtkImageMenuItem *image_menu_item)
{
  if (image_menu_item->image)
    {
      if (show_image (image_menu_item))
	gtk_widget_show (image_menu_item->image);
      else
	gtk_widget_hide (image_menu_item->image);
    }
}

static void
traverse_container (GtkWidget *widget,
		    gpointer   data)
{
  if (GTK_IS_IMAGE_MENU_ITEM (widget))
    show_image_change_notify (GTK_IMAGE_MENU_ITEM (widget));
  else if (GTK_IS_CONTAINER (widget))
    gtk_container_forall (GTK_CONTAINER (widget), traverse_container, NULL);
}

static void
gtk_image_menu_item_setting_changed (GtkSettings *settings)
{
  GList *list, *l;

  list = gtk_window_list_toplevels ();

  for (l = list; l; l = l->next)
    gtk_container_forall (GTK_CONTAINER (l->data), 
			  traverse_container, NULL);

  g_list_free (list);  
}

static void
gtk_image_menu_item_screen_changed (GtkWidget *widget,
				    GdkScreen *previous_screen)
{
  GtkSettings *settings;
  guint show_image_connection;

  if (!gtk_widget_has_screen (widget))
    return;

  settings = gtk_widget_get_settings (widget);
  
  show_image_connection = 
    GPOINTER_TO_UINT (g_object_get_data (G_OBJECT (settings), 
					 "gtk-image-menu-item-connection"));
  
  if (show_image_connection)
    return;

  show_image_connection =
    g_signal_connect (settings, "notify::gtk-menu-images",
		      G_CALLBACK (gtk_image_menu_item_setting_changed), NULL);
  g_object_set_data (G_OBJECT (settings), 
		     I_("gtk-image-menu-item-connection"),
		     GUINT_TO_POINTER (show_image_connection));

  show_image_change_notify (GTK_IMAGE_MENU_ITEM (widget));
}

#define __GTK_IMAGE_MENU_ITEM_C__
#include "gtkaliasdef.c"
