/* gtktoolbutton.c
 *
 * Copyright (C) 2002 Anders Carlsson <andersca@gnome.org>
 * Copyright (C) 2002 James Henstridge <james@daa.com.au>
 * Copyright (C) 2003 Soeren Sandmann <sandmann@daimi.au.dk>
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
#include "gtktoolbutton.h"
#include "gtkbutton.h"
#include "gtkhbox.h"
#include "gtkiconfactory.h"
#include "gtkimage.h"
#include "gtkimagemenuitem.h"
#include "gtklabel.h"
#include "gtkstock.h"
#include "gtkvbox.h"
#include "gtkintl.h"
#include "gtktoolbar.h"
#include "gtkactivatable.h"
#include "gtkprivate.h"
#include "gtkalias.h"

#include <string.h>

#define MENU_ID "gtk-tool-button-menu-id"

enum {
  CLICKED,
  LAST_SIGNAL
};

enum {
  PROP_0,
  PROP_LABEL,
  PROP_USE_UNDERLINE,
  PROP_LABEL_WIDGET,
  PROP_STOCK_ID,
  PROP_ICON_NAME,
  PROP_ICON_WIDGET
};

static void gtk_tool_button_init          (GtkToolButton      *button,
					   GtkToolButtonClass *klass);
static void gtk_tool_button_class_init    (GtkToolButtonClass *klass);
static void gtk_tool_button_set_property  (GObject            *object,
					   guint               prop_id,
					   const GValue       *value,
					   GParamSpec         *pspec);
static void gtk_tool_button_get_property  (GObject            *object,
					   guint               prop_id,
					   GValue             *value,
					   GParamSpec         *pspec);
static void gtk_tool_button_property_notify (GObject          *object,
					     GParamSpec       *pspec);
static void gtk_tool_button_finalize      (GObject            *object);

static void gtk_tool_button_toolbar_reconfigured (GtkToolItem *tool_item);
static gboolean   gtk_tool_button_create_menu_proxy (GtkToolItem     *item);
static void       button_clicked                    (GtkWidget       *widget,
						     GtkToolButton   *button);
static void gtk_tool_button_style_set      (GtkWidget          *widget,
					    GtkStyle           *prev_style);

static void gtk_tool_button_construct_contents (GtkToolItem *tool_item);

static void gtk_tool_button_activatable_interface_init (GtkActivatableIface  *iface);
static void gtk_tool_button_update                     (GtkActivatable       *activatable,
							GtkAction            *action,
							const gchar          *property_name);
static void gtk_tool_button_sync_action_properties     (GtkActivatable       *activatable,
							GtkAction            *action);


struct _GtkToolButtonPrivate
{
  GtkWidget *button;

  gchar *stock_id;
  gchar *icon_name;
  gchar *label_text;
  GtkWidget *label_widget;
  GtkWidget *icon_widget;

  GtkSizeGroup *text_size_group;

  guint use_underline : 1;
  guint contents_invalid : 1;
};

static GObjectClass        *parent_class = NULL;
static GtkActivatableIface *parent_activatable_iface;
static guint                toolbutton_signals[LAST_SIGNAL] = { 0 };

#define GTK_TOOL_BUTTON_GET_PRIVATE(obj)(G_TYPE_INSTANCE_GET_PRIVATE ((obj), GTK_TYPE_TOOL_BUTTON, GtkToolButtonPrivate))

GType
gtk_tool_button_get_type (void)
{
  static GType type = 0;
  
  if (!type)
    {
      const GInterfaceInfo activatable_info =
      {
        (GInterfaceInitFunc) gtk_tool_button_activatable_interface_init,
        (GInterfaceFinalizeFunc) NULL,
        NULL
      };

      type = g_type_register_static_simple (GTK_TYPE_TOOL_ITEM,
					    I_("GtkToolButton"),
					    sizeof (GtkToolButtonClass),
					    (GClassInitFunc) gtk_tool_button_class_init,
					    sizeof (GtkToolButton),
					    (GInstanceInitFunc) gtk_tool_button_init,
					    0);

      g_type_add_interface_static (type, GTK_TYPE_ACTIVATABLE,
                                   &activatable_info);
    }
  return type;
}

static void
gtk_tool_button_class_init (GtkToolButtonClass *klass)
{
  GObjectClass *object_class;
  GtkWidgetClass *widget_class;
  GtkToolItemClass *tool_item_class;
  
  parent_class = g_type_class_peek_parent (klass);
  
  object_class = (GObjectClass *)klass;
  widget_class = (GtkWidgetClass *)klass;
  tool_item_class = (GtkToolItemClass *)klass;
  
  object_class->set_property = gtk_tool_button_set_property;
  object_class->get_property = gtk_tool_button_get_property;
  object_class->notify = gtk_tool_button_property_notify;
  object_class->finalize = gtk_tool_button_finalize;

  widget_class->style_set = gtk_tool_button_style_set;

  tool_item_class->create_menu_proxy = gtk_tool_button_create_menu_proxy;
  tool_item_class->toolbar_reconfigured = gtk_tool_button_toolbar_reconfigured;
  
  klass->button_type = GTK_TYPE_BUTTON;

  /* Properties are interpreted like this:
   *
   *          - if the tool button has an icon_widget, then that widget
   *            will be used as the icon. Otherwise, if the tool button
   *            has a stock id, the corresponding stock icon will be
   *            used. Otherwise, if the tool button has an icon name,
   *            the corresponding icon from the theme will be used.
   *            Otherwise, the tool button will not have an icon.
   *
   *          - if the tool button has a label_widget then that widget
   *            will be used as the label. Otherwise, if the tool button
   *            has a label text, that text will be used as label. Otherwise,
   *            if the toolbutton has a stock id, the corresponding text
   *            will be used as label. Otherwise, if the tool button has
   *            an icon name, the corresponding icon name from the theme will
   *            be used. Otherwise, the toolbutton will have an empty label.
   *
   *	      - The use_underline property only has an effect when the label
   *            on the toolbutton comes from the label property (ie. not from
   *            label_widget or from stock_id).
   *
   *            In that case, if use_underline is set,
   *
   *			- underscores are removed from the label text before
   *                      the label is shown on the toolbutton unless the
   *                      underscore is followed by another underscore
   *
   *			- an underscore indicates that the next character when
   *                      used in the overflow menu should be used as a
   *                      mnemonic.
   *
   *		In short: use_underline = TRUE means that the label text has
   *            the form "_Open" and the toolbar should take appropriate
   *            action.
   */

  g_object_class_install_property (object_class,
				   PROP_LABEL,
				   g_param_spec_string ("label",
							P_("Label"),
							P_("Text to show in the item."),
							NULL,
							GTK_PARAM_READWRITE));
  g_object_class_install_property (object_class,
				   PROP_USE_UNDERLINE,
				   g_param_spec_boolean ("use-underline",
							 P_("Use underline"),
							 P_("If set, an underline in the label property indicates that the next character should be used for the mnemonic accelerator key in the overflow menu"),
							 FALSE,
							 GTK_PARAM_READWRITE));
  g_object_class_install_property (object_class,
				   PROP_LABEL_WIDGET,
				   g_param_spec_object ("label-widget",
							P_("Label widget"),
							P_("Widget to use as the item label"),
							GTK_TYPE_WIDGET,
							GTK_PARAM_READWRITE));
  g_object_class_install_property (object_class,
				   PROP_STOCK_ID,
				   g_param_spec_string ("stock-id",
							P_("Stock Id"),
							P_("The stock icon displayed on the item"),
							NULL,
							GTK_PARAM_READWRITE));

  /**
   * GtkToolButton:icon-name:
   * 
   * The name of the themed icon displayed on the item.
   * This property only has an effect if not overridden by "label", 
   * "icon_widget" or "stock_id" properties.
   *
   * Since: 2.8 
   */
  g_object_class_install_property (object_class,
				   PROP_ICON_NAME,
				   g_param_spec_string ("icon-name",
							P_("Icon name"),
							P_("The name of the themed icon displayed on the item"),
							NULL,
							GTK_PARAM_READWRITE));
  g_object_class_install_property (object_class,
				   PROP_ICON_WIDGET,
				   g_param_spec_object ("icon-widget",
							P_("Icon widget"),
							P_("Icon widget to display in the item"),
							GTK_TYPE_WIDGET,
							GTK_PARAM_READWRITE));

  /**
   * GtkButton:icon-spacing:
   * 
   * Spacing in pixels between the icon and label.
   * 
   * Since: 2.10
   */
  gtk_widget_class_install_style_property (widget_class,
					   g_param_spec_int ("icon-spacing",
							     P_("Icon spacing"),
							     P_("Spacing in pixels between the icon and label"),
							     0,
							     G_MAXINT,
							     3,
							     GTK_PARAM_READWRITE));

/**
 * GtkToolButton::clicked:
 * @toolbutton: the object that emitted the signal
 *
 * This signal is emitted when the tool button is clicked with the mouse
 * or activated with the keyboard.
 **/
  toolbutton_signals[CLICKED] =
    g_signal_new (I_("clicked"),
		  G_OBJECT_CLASS_TYPE (klass),
		  G_SIGNAL_RUN_FIRST | G_SIGNAL_ACTION,
		  G_STRUCT_OFFSET (GtkToolButtonClass, clicked),
		  NULL, NULL,
		  g_cclosure_marshal_VOID__VOID,
		  G_TYPE_NONE, 0);
  
  g_type_class_add_private (object_class, sizeof (GtkToolButtonPrivate));
}

static void
gtk_tool_button_init (GtkToolButton      *button,
		      GtkToolButtonClass *klass)
{
  GtkToolItem *toolitem = GTK_TOOL_ITEM (button);
  
  button->priv = GTK_TOOL_BUTTON_GET_PRIVATE (button);

  button->priv->contents_invalid = TRUE;

  gtk_tool_item_set_homogeneous (toolitem, TRUE);

  /* create button */
  button->priv->button = g_object_new (klass->button_type, NULL);
  gtk_button_set_focus_on_click (GTK_BUTTON (button->priv->button), FALSE);
  g_signal_connect_object (button->priv->button, "clicked",
			   G_CALLBACK (button_clicked), button, 0);

  gtk_container_add (GTK_CONTAINER (button), button->priv->button);
  gtk_widget_show (button->priv->button);
}

static void
gtk_tool_button_construct_contents (GtkToolItem *tool_item)
{
  GtkToolButton *button = GTK_TOOL_BUTTON (tool_item);
  GtkWidget *label = NULL;
  GtkWidget *icon = NULL;
  GtkToolbarStyle style;
  gboolean need_label = FALSE;
  gboolean need_icon = FALSE;
  GtkIconSize icon_size;
  GtkWidget *box = NULL;
  guint icon_spacing;
  GtkOrientation text_orientation = GTK_ORIENTATION_HORIZONTAL;
  GtkSizeGroup *size_group = NULL;

  button->priv->contents_invalid = FALSE;

  gtk_widget_style_get (GTK_WIDGET (tool_item), 
			"icon-spacing", &icon_spacing,
			NULL);

  if (button->priv->icon_widget && button->priv->icon_widget->parent)
    {
      gtk_container_remove (GTK_CONTAINER (button->priv->icon_widget->parent),
			    button->priv->icon_widget);
    }

  if (button->priv->label_widget && button->priv->label_widget->parent)
    {
      gtk_container_remove (GTK_CONTAINER (button->priv->label_widget->parent),
			    button->priv->label_widget);
    }

  if (GTK_BIN (button->priv->button)->child)
    {
      /* Note: we are not destroying the label_widget or icon_widget
       * here because they were removed from their containers above
       */
      gtk_widget_destroy (GTK_BIN (button->priv->button)->child);
    }

  style = gtk_tool_item_get_toolbar_style (GTK_TOOL_ITEM (button));
  
  if (style != GTK_TOOLBAR_TEXT)
    need_icon = TRUE;

  if (style != GTK_TOOLBAR_ICONS && style != GTK_TOOLBAR_BOTH_HORIZ)
    need_label = TRUE;

  if (style == GTK_TOOLBAR_BOTH_HORIZ &&
      (gtk_tool_item_get_is_important (GTK_TOOL_ITEM (button)) ||
       gtk_tool_item_get_orientation (GTK_TOOL_ITEM (button)) == GTK_ORIENTATION_VERTICAL ||
       gtk_tool_item_get_text_orientation (GTK_TOOL_ITEM (button)) == GTK_ORIENTATION_VERTICAL))
    {
      need_label = TRUE;
    }
  
  if (style == GTK_TOOLBAR_ICONS && button->priv->icon_widget == NULL &&
      button->priv->stock_id == NULL && button->priv->icon_name == NULL)
    {
      need_label = TRUE;
      need_icon = FALSE;
      style = GTK_TOOLBAR_TEXT;
    }

  if (style == GTK_TOOLBAR_TEXT && button->priv->label_widget == NULL &&
      button->priv->stock_id == NULL && button->priv->label_text == NULL)
    {
      need_label = FALSE;
      need_icon = TRUE;
      style = GTK_TOOLBAR_ICONS;
    }

  if (need_label)
    {
      if (button->priv->label_widget)
	{
	  label = button->priv->label_widget;
	}
      else
	{
	  GtkStockItem stock_item;
	  gboolean elide;
	  gchar *label_text;

	  if (button->priv->label_text)
	    {
	      label_text = button->priv->label_text;
	      elide = button->priv->use_underline;
	    }
	  else if (button->priv->stock_id && gtk_stock_lookup (button->priv->stock_id, &stock_item))
	    {
	      label_text = stock_item.label;
	      elide = TRUE;
	    }
	  else
	    {
	      label_text = "";
	      elide = FALSE;
	    }

	  if (elide)
	    label_text = _gtk_toolbar_elide_underscores (label_text);
	  else
	    label_text = g_strdup (label_text);

	  label = gtk_label_new (label_text);

	  g_free (label_text);
	  
	  gtk_widget_show (label);
	}

      if (GTK_IS_LABEL (label))
        {
          gtk_label_set_ellipsize (GTK_LABEL (label),
			           gtk_tool_item_get_ellipsize_mode (GTK_TOOL_ITEM (button)));
          text_orientation = gtk_tool_item_get_text_orientation (GTK_TOOL_ITEM (button));
          if (text_orientation == GTK_ORIENTATION_HORIZONTAL)
	    {
              gtk_label_set_angle (GTK_LABEL (label), 0);
              gtk_misc_set_alignment (GTK_MISC (label),
                                      gtk_tool_item_get_text_alignment (GTK_TOOL_ITEM (button)),
                                      0.5);
            }
          else
            {
              gtk_label_set_ellipsize (GTK_LABEL (label), PANGO_ELLIPSIZE_NONE);
	      if (gtk_widget_get_direction (GTK_WIDGET (tool_item)) == GTK_TEXT_DIR_RTL)
	        gtk_label_set_angle (GTK_LABEL (label), -90);
	      else
	        gtk_label_set_angle (GTK_LABEL (label), 90);
              gtk_misc_set_alignment (GTK_MISC (label),
                                      0.5,
                                      1 - gtk_tool_item_get_text_alignment (GTK_TOOL_ITEM (button)));
            }
        }
    }

  icon_size = gtk_tool_item_get_icon_size (GTK_TOOL_ITEM (button));
  if (need_icon)
    {
      if (button->priv->icon_widget)
	{
	  icon = button->priv->icon_widget;
	  
	  if (GTK_IS_IMAGE (icon))
	    {
	      g_object_set (button->priv->icon_widget,
			    "icon-size", icon_size,
			    NULL);
	    }
	}
      else if (button->priv->stock_id && 
	       gtk_icon_factory_lookup_default (button->priv->stock_id))
	{
	  icon = gtk_image_new_from_stock (button->priv->stock_id, icon_size);
	  gtk_widget_show (icon);
	}
      else if (button->priv->icon_name)
	{
	  icon = gtk_image_new_from_icon_name (button->priv->icon_name, icon_size);
	  gtk_widget_show (icon);
	}

      if (GTK_IS_MISC (icon) && text_orientation == GTK_ORIENTATION_HORIZONTAL)
	gtk_misc_set_alignment (GTK_MISC (icon),
				1.0 - gtk_tool_item_get_text_alignment (GTK_TOOL_ITEM (button)),
				0.5);
      else if (GTK_IS_MISC (icon))
	gtk_misc_set_alignment (GTK_MISC (icon),
				0.5,
				gtk_tool_item_get_text_alignment (GTK_TOOL_ITEM (button)));

      if (icon)
	{
	  size_group = gtk_tool_item_get_text_size_group (GTK_TOOL_ITEM (button));
	  if (size_group != NULL)
	    gtk_size_group_add_widget (size_group, icon);
	}
    }

  switch (style)
    {
    case GTK_TOOLBAR_ICONS:
      if (icon)
	gtk_container_add (GTK_CONTAINER (button->priv->button), icon);
      break;

    case GTK_TOOLBAR_BOTH:
      if (text_orientation == GTK_ORIENTATION_HORIZONTAL)
	box = gtk_vbox_new (FALSE, icon_spacing);
      else
	box = gtk_hbox_new (FALSE, icon_spacing);
      if (icon)
	gtk_box_pack_start (GTK_BOX (box), icon, TRUE, TRUE, 0);
      gtk_box_pack_end (GTK_BOX (box), label, FALSE, TRUE, 0);
      gtk_container_add (GTK_CONTAINER (button->priv->button), box);
      break;

    case GTK_TOOLBAR_BOTH_HORIZ:
      if (text_orientation == GTK_ORIENTATION_HORIZONTAL)
	{
	  box = gtk_hbox_new (FALSE, icon_spacing);
	  if (icon)
	    gtk_box_pack_start (GTK_BOX (box), icon, label? FALSE : TRUE, TRUE, 0);
	  if (label)
	    gtk_box_pack_end (GTK_BOX (box), label, TRUE, TRUE, 0);
	}
      else
	{
	  box = gtk_vbox_new (FALSE, icon_spacing);
	  if (icon)
	    gtk_box_pack_end (GTK_BOX (box), icon, label ? FALSE : TRUE, TRUE, 0);
	  if (label)
	    gtk_box_pack_start (GTK_BOX (box), label, TRUE, TRUE, 0);
	}
      gtk_container_add (GTK_CONTAINER (button->priv->button), box);
      break;

    case GTK_TOOLBAR_TEXT:
      gtk_container_add (GTK_CONTAINER (button->priv->button), label);
      break;
    }

  if (box)
    gtk_widget_show (box);

  gtk_button_set_relief (GTK_BUTTON (button->priv->button),
			 gtk_tool_item_get_relief_style (GTK_TOOL_ITEM (button)));

  gtk_tool_item_rebuild_menu (tool_item);
  
  gtk_widget_queue_resize (GTK_WIDGET (button));
}

static void
gtk_tool_button_set_property (GObject         *object,
			      guint            prop_id,
			      const GValue    *value,
			      GParamSpec      *pspec)
{
  GtkToolButton *button = GTK_TOOL_BUTTON (object);
  
  switch (prop_id)
    {
    case PROP_LABEL:
      gtk_tool_button_set_label (button, g_value_get_string (value));
      break;
    case PROP_USE_UNDERLINE:
      gtk_tool_button_set_use_underline (button, g_value_get_boolean (value));
      break;
    case PROP_LABEL_WIDGET:
      gtk_tool_button_set_label_widget (button, g_value_get_object (value));
      break;
    case PROP_STOCK_ID:
      gtk_tool_button_set_stock_id (button, g_value_get_string (value));
      break;
    case PROP_ICON_NAME:
      gtk_tool_button_set_icon_name (button, g_value_get_string (value));
      break;
    case PROP_ICON_WIDGET:
      gtk_tool_button_set_icon_widget (button, g_value_get_object (value));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
gtk_tool_button_property_notify (GObject          *object,
				 GParamSpec       *pspec)
{
  GtkToolButton *button = GTK_TOOL_BUTTON (object);

  if (button->priv->contents_invalid ||
      strcmp ("is-important", pspec->name) == 0)
    gtk_tool_button_construct_contents (GTK_TOOL_ITEM (object));

  if (parent_class->notify)
    parent_class->notify (object, pspec);
}

static void
gtk_tool_button_get_property (GObject         *object,
			      guint            prop_id,
			      GValue          *value,
			      GParamSpec      *pspec)
{
  GtkToolButton *button = GTK_TOOL_BUTTON (object);

  switch (prop_id)
    {
    case PROP_LABEL:
      g_value_set_string (value, gtk_tool_button_get_label (button));
      break;
    case PROP_LABEL_WIDGET:
      g_value_set_object (value, gtk_tool_button_get_label_widget (button));
      break;
    case PROP_USE_UNDERLINE:
      g_value_set_boolean (value, gtk_tool_button_get_use_underline (button));
      break;
    case PROP_STOCK_ID:
      g_value_set_string (value, button->priv->stock_id);
      break;
    case PROP_ICON_NAME:
      g_value_set_string (value, button->priv->icon_name);
      break;
    case PROP_ICON_WIDGET:
      g_value_set_object (value, button->priv->icon_widget);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
gtk_tool_button_finalize (GObject *object)
{
  GtkToolButton *button = GTK_TOOL_BUTTON (object);

  g_free (button->priv->stock_id);
  g_free (button->priv->icon_name);
  g_free (button->priv->label_text);

  if (button->priv->label_widget)
    g_object_unref (button->priv->label_widget);

  if (button->priv->icon_widget)
    g_object_unref (button->priv->icon_widget);
  
  parent_class->finalize (object);
}

static GtkWidget *
clone_image_menu_size (GtkImage *image, GtkSettings *settings)
{
  GtkImageType storage_type = gtk_image_get_storage_type (image);

  if (storage_type == GTK_IMAGE_STOCK)
    {
      gchar *stock_id;
      gtk_image_get_stock (image, &stock_id, NULL);
      return gtk_image_new_from_stock (stock_id, GTK_ICON_SIZE_MENU);
    }
  else if (storage_type == GTK_IMAGE_ICON_NAME)
    {
      const gchar *icon_name;
      gtk_image_get_icon_name (image, &icon_name, NULL);
      return gtk_image_new_from_icon_name (icon_name, GTK_ICON_SIZE_MENU);
    }
  else if (storage_type == GTK_IMAGE_ICON_SET)
    {
      GtkIconSet *icon_set;
      gtk_image_get_icon_set (image, &icon_set, NULL);
      return gtk_image_new_from_icon_set (icon_set, GTK_ICON_SIZE_MENU);
    }
  else if (storage_type == GTK_IMAGE_GICON)
    {
      GIcon *icon;
      gtk_image_get_gicon (image, &icon, NULL);
      return gtk_image_new_from_gicon (icon, GTK_ICON_SIZE_MENU);
    }
  else if (storage_type == GTK_IMAGE_PIXBUF)
    {
      gint width, height;
      
      if (settings &&
	  gtk_icon_size_lookup_for_settings (settings, GTK_ICON_SIZE_MENU,
					     &width, &height))
	{
	  GdkPixbuf *src_pixbuf, *dest_pixbuf;
	  GtkWidget *cloned_image;

	  src_pixbuf = gtk_image_get_pixbuf (image);
	  dest_pixbuf = gdk_pixbuf_scale_simple (src_pixbuf, width, height,
						 GDK_INTERP_BILINEAR);

	  cloned_image = gtk_image_new_from_pixbuf (dest_pixbuf);
	  g_object_unref (dest_pixbuf);

	  return cloned_image;
	}
    }

  return NULL;
}
      
static gboolean
gtk_tool_button_create_menu_proxy (GtkToolItem *item)
{
  GtkToolButton *button = GTK_TOOL_BUTTON (item);
  GtkWidget *menu_item;
  GtkWidget *menu_image = NULL;
  GtkStockItem stock_item;
  gboolean use_mnemonic = TRUE;
  const char *label;

  if (_gtk_tool_item_create_menu_proxy (item))
    return TRUE;
 
  if (GTK_IS_LABEL (button->priv->label_widget))
    {
      label = gtk_label_get_label (GTK_LABEL (button->priv->label_widget));
      use_mnemonic = gtk_label_get_use_underline (GTK_LABEL (button->priv->label_widget));
    }
  else if (button->priv->label_text)
    {
      label = button->priv->label_text;
      use_mnemonic = button->priv->use_underline;
    }
  else if (button->priv->stock_id && gtk_stock_lookup (button->priv->stock_id, &stock_item))
    {
      label = stock_item.label;
    }
  else
    {
      label = "";
    }
  
  if (use_mnemonic)
    menu_item = gtk_image_menu_item_new_with_mnemonic (label);
  else
    menu_item = gtk_image_menu_item_new_with_label (label);

  if (GTK_IS_IMAGE (button->priv->icon_widget))
    {
      menu_image = clone_image_menu_size (GTK_IMAGE (button->priv->icon_widget),
					  gtk_widget_get_settings (GTK_WIDGET (button)));
    }
  else if (button->priv->stock_id)
    {
      menu_image = gtk_image_new_from_stock (button->priv->stock_id, GTK_ICON_SIZE_MENU);
    }

  if (menu_image)
    gtk_image_menu_item_set_image (GTK_IMAGE_MENU_ITEM (menu_item), menu_image);

  g_signal_connect_closure_by_id (menu_item,
				  g_signal_lookup ("activate", G_OBJECT_TYPE (menu_item)), 0,
				  g_cclosure_new_object_swap (G_CALLBACK (gtk_button_clicked),
							      G_OBJECT (GTK_TOOL_BUTTON (button)->priv->button)),
				  FALSE);

  gtk_tool_item_set_proxy_menu_item (GTK_TOOL_ITEM (button), MENU_ID, menu_item);
  
  return TRUE;
}

static void
button_clicked (GtkWidget     *widget,
		GtkToolButton *button)
{
  GtkAction *action;

  action = gtk_activatable_get_related_action (GTK_ACTIVATABLE (button));
  
  if (action)
    gtk_action_activate (action);

  g_signal_emit_by_name (button, "clicked");
}

static void
gtk_tool_button_toolbar_reconfigured (GtkToolItem *tool_item)
{
  gtk_tool_button_construct_contents (tool_item);
}

static void 
gtk_tool_button_update_icon_spacing (GtkToolButton *button)
{
  GtkWidget *box;
  guint spacing;

  box = GTK_BIN (button->priv->button)->child;
  if (GTK_IS_BOX (box))
    {
      gtk_widget_style_get (GTK_WIDGET (button), 
			    "icon-spacing", &spacing,
			    NULL);
      gtk_box_set_spacing (GTK_BOX (box), spacing);      
    }
}

static void
gtk_tool_button_style_set (GtkWidget *widget,
			   GtkStyle  *prev_style)
{
  gtk_tool_button_update_icon_spacing (GTK_TOOL_BUTTON (widget));
}

static void 
gtk_tool_button_activatable_interface_init (GtkActivatableIface  *iface)
{
  parent_activatable_iface = g_type_interface_peek_parent (iface);
  iface->update = gtk_tool_button_update;
  iface->sync_action_properties = gtk_tool_button_sync_action_properties;
}

static void
gtk_tool_button_update (GtkActivatable *activatable,
			GtkAction      *action,
			const gchar    *property_name)
{
  GtkToolButton *button;
  GtkWidget *image;

  parent_activatable_iface->update (activatable, action, property_name);

  if (!gtk_activatable_get_use_action_appearance (activatable))
    return;

  button = GTK_TOOL_BUTTON (activatable);
  
  if (strcmp (property_name, "short-label") == 0)
    gtk_tool_button_set_label (button, gtk_action_get_short_label (action));
  else if (strcmp (property_name, "stock-id") == 0)
    gtk_tool_button_set_stock_id (button, gtk_action_get_stock_id (action));
  else if (strcmp (property_name, "gicon") == 0)
    {
      const gchar *stock_id = gtk_action_get_stock_id (action);
      GIcon *icon = gtk_action_get_gicon (action);
      GtkIconSize icon_size = GTK_ICON_SIZE_BUTTON;

      if ((stock_id && gtk_icon_factory_lookup_default (stock_id)) || !icon)
	image = NULL;
      else 
	{   
	  image = gtk_tool_button_get_icon_widget (button);
	  icon_size = gtk_tool_item_get_icon_size (GTK_TOOL_ITEM (button));

	  if (!image)
	    image = gtk_image_new ();
	}

      gtk_tool_button_set_icon_widget (button, image);
      gtk_image_set_from_gicon (GTK_IMAGE (image), icon, icon_size);

    }
  else if (strcmp (property_name, "icon-name") == 0)
    gtk_tool_button_set_icon_name (button, gtk_action_get_icon_name (action));
}

static void
gtk_tool_button_sync_action_properties (GtkActivatable *activatable,
				        GtkAction      *action)
{
  GtkToolButton *button;
  GIcon         *icon;
  const gchar   *stock_id;

  parent_activatable_iface->sync_action_properties (activatable, action);

  if (!action)
    return;

  if (!gtk_activatable_get_use_action_appearance (activatable))
    return;

  button = GTK_TOOL_BUTTON (activatable);
  stock_id = gtk_action_get_stock_id (action);

  gtk_tool_button_set_label (button, gtk_action_get_short_label (action));
  gtk_tool_button_set_use_underline (button, TRUE);
  gtk_tool_button_set_stock_id (button, stock_id);
  gtk_tool_button_set_icon_name (button, gtk_action_get_icon_name (action));

  if (stock_id && gtk_icon_factory_lookup_default (stock_id))
      gtk_tool_button_set_icon_widget (button, NULL);
  else if ((icon = gtk_action_get_gicon (action)) != NULL)
    {
      GtkIconSize icon_size = gtk_tool_item_get_icon_size (GTK_TOOL_ITEM (button));
      GtkWidget  *image = gtk_tool_button_get_icon_widget (button);
      
      if (!image)
	{
	  image = gtk_image_new ();
	  gtk_widget_show (image);
	  gtk_tool_button_set_icon_widget (button, image);
	}

      gtk_image_set_from_gicon (GTK_IMAGE (image), icon, icon_size);
    }
  else if (gtk_action_get_icon_name (action))
    gtk_tool_button_set_icon_name (button, gtk_action_get_icon_name (action));
  else
    gtk_tool_button_set_label (button, gtk_action_get_short_label (action));
}

/**
 * gtk_tool_button_new_from_stock:
 * @stock_id: the name of the stock item 
 *
 * Creates a new #GtkToolButton containing the image and text from a
 * stock item. Some stock ids have preprocessor macros like #GTK_STOCK_OK
 * and #GTK_STOCK_APPLY.
 *
 * It is an error if @stock_id is not a name of a stock item.
 * 
 * Return value: A new #GtkToolButton
 * 
 * Since: 2.4
 **/
GtkToolItem *
gtk_tool_button_new_from_stock (const gchar *stock_id)
{
  GtkToolButton *button;

  g_return_val_if_fail (stock_id != NULL, NULL);
    
  button = g_object_new (GTK_TYPE_TOOL_BUTTON,
			 "stock-id", stock_id,
			 NULL);

  return GTK_TOOL_ITEM (button);
}

/**
 * gtk_tool_button_new:
 * @label: (allow-none): a string that will be used as label, or %NULL
 * @icon_widget: (allow-none): a #GtkMisc widget that will be used as icon widget, or %NULL
 *
 * Creates a new %GtkToolButton using @icon_widget as icon and @label as
 * label.
 *
 * Return value: A new #GtkToolButton
 * 
 * Since: 2.4
 **/
GtkToolItem *
gtk_tool_button_new (GtkWidget	 *icon_widget,
		     const gchar *label)
{
  GtkToolButton *button;

  g_return_val_if_fail (icon_widget == NULL || GTK_IS_MISC (icon_widget), NULL);

  button = g_object_new (GTK_TYPE_TOOL_BUTTON,
                         "label", label,
                         "icon-widget", icon_widget,
			 NULL);

  return GTK_TOOL_ITEM (button);  
}

/**
 * gtk_tool_button_set_label:
 * @button: a #GtkToolButton
 * @label: (allow-none): a string that will be used as label, or %NULL.
 *
 * Sets @label as the label used for the tool button. The "label" property
 * only has an effect if not overridden by a non-%NULL "label_widget" property.
 * If both the "label_widget" and "label" properties are %NULL, the label
 * is determined by the "stock_id" property. If the "stock_id" property is also
 * %NULL, @button will not have a label.
 * 
 * Since: 2.4
 **/
void
gtk_tool_button_set_label (GtkToolButton *button,
			   const gchar   *label)
{
  gchar *old_label;
  gchar *elided_label;
  AtkObject *accessible;
  
  g_return_if_fail (GTK_IS_TOOL_BUTTON (button));

  old_label = button->priv->label_text;

  button->priv->label_text = g_strdup (label);
  button->priv->contents_invalid = TRUE;     

  if (label)
    {
      elided_label = _gtk_toolbar_elide_underscores (label);
      accessible = gtk_widget_get_accessible (GTK_WIDGET (button->priv->button));
      atk_object_set_name (accessible, elided_label);
      g_free (elided_label);
    }

  g_free (old_label);
 
  g_object_notify (G_OBJECT (button), "label");
}

/**
 * gtk_tool_button_get_label:
 * @button: a #GtkToolButton
 * 
 * Returns the label used by the tool button, or %NULL if the tool button
 * doesn't have a label. or uses a the label from a stock item. The returned
 * string is owned by GTK+, and must not be modified or freed.
 * 
 * Return value: The label, or %NULL
 * 
 * Since: 2.4
 **/
const gchar *
gtk_tool_button_get_label (GtkToolButton *button)
{
  g_return_val_if_fail (GTK_IS_TOOL_BUTTON (button), NULL);

  return button->priv->label_text;
}

/**
 * gtk_tool_button_set_use_underline:
 * @button: a #GtkToolButton
 * @use_underline: whether the button label has the form "_Open"
 *
 * If set, an underline in the label property indicates that the next character
 * should be used for the mnemonic accelerator key in the overflow menu. For
 * example, if the label property is "_Open" and @use_underline is %TRUE,
 * the label on the tool button will be "Open" and the item on the overflow
 * menu will have an underlined 'O'.
 * 
 * Labels shown on tool buttons never have mnemonics on them; this property
 * only affects the menu item on the overflow menu.
 * 
 * Since: 2.4
 **/
void
gtk_tool_button_set_use_underline (GtkToolButton *button,
				   gboolean       use_underline)
{
  g_return_if_fail (GTK_IS_TOOL_BUTTON (button));

  use_underline = use_underline != FALSE;

  if (use_underline != button->priv->use_underline)
    {
      button->priv->use_underline = use_underline;
      button->priv->contents_invalid = TRUE;

      g_object_notify (G_OBJECT (button), "use-underline");
    }
}

/**
 * gtk_tool_button_get_use_underline:
 * @button: a #GtkToolButton
 * 
 * Returns whether underscores in the label property are used as mnemonics
 * on menu items on the overflow menu. See gtk_tool_button_set_use_underline().
 * 
 * Return value: %TRUE if underscores in the label property are used as
 * mnemonics on menu items on the overflow menu.
 * 
 * Since: 2.4
 **/
gboolean
gtk_tool_button_get_use_underline (GtkToolButton *button)
{
  g_return_val_if_fail (GTK_IS_TOOL_BUTTON (button), FALSE);

  return button->priv->use_underline;
}

/**
 * gtk_tool_button_set_stock_id:
 * @button: a #GtkToolButton
 * @stock_id: (allow-none): a name of a stock item, or %NULL
 *
 * Sets the name of the stock item. See gtk_tool_button_new_from_stock().
 * The stock_id property only has an effect if not
 * overridden by non-%NULL "label" and "icon_widget" properties.
 * 
 * Since: 2.4
 **/
void
gtk_tool_button_set_stock_id (GtkToolButton *button,
			      const gchar   *stock_id)
{
  gchar *old_stock_id;
  
  g_return_if_fail (GTK_IS_TOOL_BUTTON (button));

  old_stock_id = button->priv->stock_id;

  button->priv->stock_id = g_strdup (stock_id);
  button->priv->contents_invalid = TRUE;

  g_free (old_stock_id);
  
  g_object_notify (G_OBJECT (button), "stock-id");
}

/**
 * gtk_tool_button_get_stock_id:
 * @button: a #GtkToolButton
 * 
 * Returns the name of the stock item. See gtk_tool_button_set_stock_id().
 * The returned string is owned by GTK+ and must not be freed or modifed.
 * 
 * Return value: the name of the stock item for @button.
 * 
 * Since: 2.4
 **/
const gchar *
gtk_tool_button_get_stock_id (GtkToolButton *button)
{
  g_return_val_if_fail (GTK_IS_TOOL_BUTTON (button), NULL);

  return button->priv->stock_id;
}

/**
 * gtk_tool_button_set_icon_name
 * @button: a #GtkToolButton
 * @icon_name: (allow-none): the name of the themed icon
 *
 * Sets the icon for the tool button from a named themed icon.
 * See the docs for #GtkIconTheme for more details.
 * The "icon_name" property only has an effect if not
 * overridden by non-%NULL "label", "icon_widget" and "stock_id"
 * properties.
 * 
 * Since: 2.8
 **/
void
gtk_tool_button_set_icon_name (GtkToolButton *button,
			       const gchar   *icon_name)
{
  gchar *old_icon_name;

  g_return_if_fail (GTK_IS_TOOL_BUTTON (button));

  old_icon_name = button->priv->icon_name;

  button->priv->icon_name = g_strdup (icon_name);
  button->priv->contents_invalid = TRUE; 

  g_free (old_icon_name);

  g_object_notify (G_OBJECT (button), "icon-name");
}

/**
 * gtk_tool_button_get_icon_name
 * @button: a #GtkToolButton
 * 
 * Returns the name of the themed icon for the tool button,
 * see gtk_tool_button_set_icon_name().
 *
 * Returns: the icon name or %NULL if the tool button has
 * no themed icon
 * 
 * Since: 2.8
 **/
const gchar*
gtk_tool_button_get_icon_name (GtkToolButton *button)
{
  g_return_val_if_fail (GTK_IS_TOOL_BUTTON (button), NULL);

  return button->priv->icon_name;
}

/**
 * gtk_tool_button_set_icon_widget:
 * @button: a #GtkToolButton
 * @icon_widget: (allow-none): the widget used as icon, or %NULL
 *
 * Sets @icon as the widget used as icon on @button. If @icon_widget is
 * %NULL the icon is determined by the "stock_id" property. If the
 * "stock_id" property is also %NULL, @button will not have an icon.
 * 
 * Since: 2.4
 **/
void
gtk_tool_button_set_icon_widget (GtkToolButton *button,
				 GtkWidget     *icon_widget)
{
  g_return_if_fail (GTK_IS_TOOL_BUTTON (button));
  g_return_if_fail (icon_widget == NULL || GTK_IS_WIDGET (icon_widget));

  if (icon_widget != button->priv->icon_widget)
    {
      if (button->priv->icon_widget)
	{
	  if (button->priv->icon_widget->parent)
	    gtk_container_remove (GTK_CONTAINER (button->priv->icon_widget->parent),
				    button->priv->icon_widget);

	  g_object_unref (button->priv->icon_widget);
	}
      
      if (icon_widget)
	g_object_ref_sink (icon_widget);

      button->priv->icon_widget = icon_widget;
      button->priv->contents_invalid = TRUE;
      
      g_object_notify (G_OBJECT (button), "icon-widget");
    }
}

/**
 * gtk_tool_button_set_label_widget:
 * @button: a #GtkToolButton
 * @label_widget: (allow-none): the widget used as label, or %NULL
 *
 * Sets @label_widget as the widget that will be used as the label
 * for @button. If @label_widget is %NULL the "label" property is used
 * as label. If "label" is also %NULL, the label in the stock item
 * determined by the "stock_id" property is used as label. If
 * "stock_id" is also %NULL, @button does not have a label.
 * 
 * Since: 2.4
 **/
void
gtk_tool_button_set_label_widget (GtkToolButton *button,
				  GtkWidget     *label_widget)
{
  g_return_if_fail (GTK_IS_TOOL_BUTTON (button));
  g_return_if_fail (label_widget == NULL || GTK_IS_WIDGET (label_widget));

  if (label_widget != button->priv->label_widget)
    {
      if (button->priv->label_widget)
	{
	  if (button->priv->label_widget->parent)
	    gtk_container_remove (GTK_CONTAINER (button->priv->label_widget->parent),
		    	          button->priv->label_widget);
	  
	  g_object_unref (button->priv->label_widget);
	}
      
      if (label_widget)
	g_object_ref_sink (label_widget);

      button->priv->label_widget = label_widget;
      button->priv->contents_invalid = TRUE;
      
      g_object_notify (G_OBJECT (button), "label-widget");
    }
}

/**
 * gtk_tool_button_get_label_widget:
 * @button: a #GtkToolButton
 *
 * Returns the widget used as label on @button.
 * See gtk_tool_button_set_label_widget().
 *
 * Return value: (transfer none): The widget used as label
 *     on @button, or %NULL.
 *
 * Since: 2.4
 **/
GtkWidget *
gtk_tool_button_get_label_widget (GtkToolButton *button)
{
  g_return_val_if_fail (GTK_IS_TOOL_BUTTON (button), NULL);

  return button->priv->label_widget;
}

/**
 * gtk_tool_button_get_icon_widget:
 * @button: a #GtkToolButton
 *
 * Return the widget used as icon widget on @button.
 * See gtk_tool_button_set_icon_widget().
 *
 * Return value: (transfer none): The widget used as icon
 *     on @button, or %NULL.
 *
 * Since: 2.4
 **/
GtkWidget *
gtk_tool_button_get_icon_widget (GtkToolButton *button)
{
  g_return_val_if_fail (GTK_IS_TOOL_BUTTON (button), NULL);

  return button->priv->icon_widget;
}

GtkWidget *
_gtk_tool_button_get_button (GtkToolButton *button)
{
  g_return_val_if_fail (GTK_IS_TOOL_BUTTON (button), NULL);

  return button->priv->button;
}


#define __GTK_TOOL_BUTTON_C__
#include "gtkaliasdef.c"
