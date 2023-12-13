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
 * Modified by the GTK+ Team and others 1997-2001.  See the AUTHORS
 * file for a list of people on the GTK+ Team.  See the ChangeLog
 * files for a list of changes.  These files are distributed with
 * GTK+ at ftp://ftp.gtk.org/pub/gtk/. 
 */

#include "config.h"
#include "gtkcheckmenuitem.h"
#include "gtkaccellabel.h"
#include "gtkactivatable.h"
#include "gtktoggleaction.h"
#include "gtkmarshalers.h"
#include "gtkprivate.h"
#include "gtkintl.h"
#include "gtkalias.h"

enum {
  TOGGLED,
  LAST_SIGNAL
};

enum {
  PROP_0,
  PROP_ACTIVE,
  PROP_INCONSISTENT,
  PROP_DRAW_AS_RADIO
};

static gint gtk_check_menu_item_expose               (GtkWidget             *widget,
						      GdkEventExpose        *event);
static void gtk_check_menu_item_activate             (GtkMenuItem           *menu_item);
static void gtk_check_menu_item_toggle_size_request  (GtkMenuItem           *menu_item,
						      gint                  *requisition);
static void gtk_check_menu_item_draw_indicator       (GtkCheckMenuItem      *check_menu_item,
						      GdkRectangle          *area);
static void gtk_real_check_menu_item_draw_indicator  (GtkCheckMenuItem      *check_menu_item,
						      GdkRectangle          *area);
static void gtk_check_menu_item_set_property         (GObject               *object,
						      guint                  prop_id,
						      const GValue          *value,
						      GParamSpec            *pspec);
static void gtk_check_menu_item_get_property         (GObject               *object,
						      guint                  prop_id,
						      GValue                *value,
						      GParamSpec            *pspec);

static void gtk_check_menu_item_activatable_interface_init (GtkActivatableIface  *iface);
static void gtk_check_menu_item_update                     (GtkActivatable       *activatable,
							    GtkAction            *action,
							    const gchar          *property_name);
static void gtk_check_menu_item_sync_action_properties     (GtkActivatable       *activatable,
							    GtkAction            *action);

static GtkActivatableIface *parent_activatable_iface;
static guint                check_menu_item_signals[LAST_SIGNAL] = { 0 };

G_DEFINE_TYPE_WITH_CODE (GtkCheckMenuItem, gtk_check_menu_item, GTK_TYPE_MENU_ITEM,
			 G_IMPLEMENT_INTERFACE (GTK_TYPE_ACTIVATABLE,
						gtk_check_menu_item_activatable_interface_init))

static void
gtk_check_menu_item_class_init (GtkCheckMenuItemClass *klass)
{
  GObjectClass *gobject_class;
  GtkWidgetClass *widget_class;
  GtkMenuItemClass *menu_item_class;
  
  gobject_class = G_OBJECT_CLASS (klass);
  widget_class = (GtkWidgetClass*) klass;
  menu_item_class = (GtkMenuItemClass*) klass;
  
  gobject_class->set_property = gtk_check_menu_item_set_property;
  gobject_class->get_property = gtk_check_menu_item_get_property;

  g_object_class_install_property (gobject_class,
                                   PROP_ACTIVE,
                                   g_param_spec_boolean ("active",
                                                         P_("Active"),
                                                         P_("Whether the menu item is checked"),
                                                         FALSE,
                                                         GTK_PARAM_READWRITE));
  
  g_object_class_install_property (gobject_class,
                                   PROP_INCONSISTENT,
                                   g_param_spec_boolean ("inconsistent",
                                                         P_("Inconsistent"),
                                                         P_("Whether to display an \"inconsistent\" state"),
                                                         FALSE,
                                                         GTK_PARAM_READWRITE));
  
  g_object_class_install_property (gobject_class,
                                   PROP_DRAW_AS_RADIO,
                                   g_param_spec_boolean ("draw-as-radio",
                                                         P_("Draw as radio menu item"),
                                                         P_("Whether the menu item looks like a radio menu item"),
                                                         FALSE,
                                                         GTK_PARAM_READWRITE));
  
  gtk_widget_class_install_style_property (widget_class,
                                           g_param_spec_int ("indicator-size",
                                                             P_("Indicator Size"),
                                                             P_("Size of check or radio indicator"),
                                                             0,
                                                             G_MAXINT,
                                                             13,
                                                             GTK_PARAM_READABLE));

  widget_class->expose_event = gtk_check_menu_item_expose;
  
  menu_item_class->activate = gtk_check_menu_item_activate;
  menu_item_class->hide_on_activate = FALSE;
  menu_item_class->toggle_size_request = gtk_check_menu_item_toggle_size_request;
  
  klass->toggled = NULL;
  klass->draw_indicator = gtk_real_check_menu_item_draw_indicator;

  check_menu_item_signals[TOGGLED] =
    g_signal_new (I_("toggled"),
		  G_OBJECT_CLASS_TYPE (gobject_class),
		  G_SIGNAL_RUN_FIRST,
		  G_STRUCT_OFFSET (GtkCheckMenuItemClass, toggled),
		  NULL, NULL,
		  _gtk_marshal_VOID__VOID,
		  G_TYPE_NONE, 0);
}

static void 
gtk_check_menu_item_activatable_interface_init (GtkActivatableIface  *iface)
{
  parent_activatable_iface = g_type_interface_peek_parent (iface);
  iface->update = gtk_check_menu_item_update;
  iface->sync_action_properties = gtk_check_menu_item_sync_action_properties;
}

static void
gtk_check_menu_item_update (GtkActivatable *activatable,
			    GtkAction      *action,
			    const gchar    *property_name)
{
  GtkCheckMenuItem *check_menu_item;

  check_menu_item = GTK_CHECK_MENU_ITEM (activatable);

  parent_activatable_iface->update (activatable, action, property_name);

  if (strcmp (property_name, "active") == 0)
    {
      gtk_action_block_activate (action);
      gtk_check_menu_item_set_active (check_menu_item, gtk_toggle_action_get_active (GTK_TOGGLE_ACTION (action)));
      gtk_action_unblock_activate (action);
    }

  if (!gtk_activatable_get_use_action_appearance (activatable))
    return;

  if (strcmp (property_name, "draw-as-radio") == 0)
    gtk_check_menu_item_set_draw_as_radio (check_menu_item,
					   gtk_toggle_action_get_draw_as_radio (GTK_TOGGLE_ACTION (action)));
}

static void
gtk_check_menu_item_sync_action_properties (GtkActivatable *activatable,
		                            GtkAction      *action)
{
  GtkCheckMenuItem *check_menu_item;

  check_menu_item = GTK_CHECK_MENU_ITEM (activatable);

  parent_activatable_iface->sync_action_properties (activatable, action);

  if (!GTK_IS_TOGGLE_ACTION (action))
    return;

  gtk_action_block_activate (action);
  gtk_check_menu_item_set_active (check_menu_item, gtk_toggle_action_get_active (GTK_TOGGLE_ACTION (action)));
  gtk_action_unblock_activate (action);
  
  if (!gtk_activatable_get_use_action_appearance (activatable))
    return;

  gtk_check_menu_item_set_draw_as_radio (check_menu_item,
					 gtk_toggle_action_get_draw_as_radio (GTK_TOGGLE_ACTION (action)));
}

GtkWidget*
gtk_check_menu_item_new (void)
{
  return g_object_new (GTK_TYPE_CHECK_MENU_ITEM, NULL);
}

GtkWidget*
gtk_check_menu_item_new_with_label (const gchar *label)
{
  return g_object_new (GTK_TYPE_CHECK_MENU_ITEM, 
		       "label", label,
		       NULL);
}


/**
 * gtk_check_menu_item_new_with_mnemonic:
 * @label: The text of the button, with an underscore in front of the
 *         mnemonic character
 * @returns: a new #GtkCheckMenuItem
 *
 * Creates a new #GtkCheckMenuItem containing a label. The label
 * will be created using gtk_label_new_with_mnemonic(), so underscores
 * in @label indicate the mnemonic for the menu item.
 **/
GtkWidget*
gtk_check_menu_item_new_with_mnemonic (const gchar *label)
{
  return g_object_new (GTK_TYPE_CHECK_MENU_ITEM, 
		       "label", label,
		       "use-underline", TRUE,
		       NULL);
}

void
gtk_check_menu_item_set_active (GtkCheckMenuItem *check_menu_item,
				gboolean          is_active)
{
  g_return_if_fail (GTK_IS_CHECK_MENU_ITEM (check_menu_item));

  is_active = is_active != 0;

  if (check_menu_item->active != is_active)
    gtk_menu_item_activate (GTK_MENU_ITEM (check_menu_item));
}

/**
 * gtk_check_menu_item_get_active:
 * @check_menu_item: a #GtkCheckMenuItem
 * 
 * Returns whether the check menu item is active. See
 * gtk_check_menu_item_set_active ().
 * 
 * Return value: %TRUE if the menu item is checked.
 */
gboolean
gtk_check_menu_item_get_active (GtkCheckMenuItem *check_menu_item)
{
  g_return_val_if_fail (GTK_IS_CHECK_MENU_ITEM (check_menu_item), FALSE);

  return check_menu_item->active;
}

static void
gtk_check_menu_item_toggle_size_request (GtkMenuItem *menu_item,
					 gint        *requisition)
{
  guint toggle_spacing;
  guint indicator_size;
  
  g_return_if_fail (GTK_IS_CHECK_MENU_ITEM (menu_item));
  
  gtk_widget_style_get (GTK_WIDGET (menu_item),
			"toggle-spacing", &toggle_spacing,
			"indicator-size", &indicator_size,
			NULL);

  *requisition = indicator_size + toggle_spacing;
}

void
gtk_check_menu_item_set_show_toggle (GtkCheckMenuItem *menu_item,
				     gboolean          always)
{
  g_return_if_fail (GTK_IS_CHECK_MENU_ITEM (menu_item));

#if 0
  menu_item->always_show_toggle = always != FALSE;
#endif  
}

void
gtk_check_menu_item_toggled (GtkCheckMenuItem *check_menu_item)
{
  g_signal_emit (check_menu_item, check_menu_item_signals[TOGGLED], 0);
}

/**
 * gtk_check_menu_item_set_inconsistent:
 * @check_menu_item: a #GtkCheckMenuItem
 * @setting: %TRUE to display an "inconsistent" third state check
 *
 * If the user has selected a range of elements (such as some text or
 * spreadsheet cells) that are affected by a boolean setting, and the
 * current values in that range are inconsistent, you may want to
 * display the check in an "in between" state. This function turns on
 * "in between" display.  Normally you would turn off the inconsistent
 * state again if the user explicitly selects a setting. This has to be
 * done manually, gtk_check_menu_item_set_inconsistent() only affects
 * visual appearance, it doesn't affect the semantics of the widget.
 * 
 **/
void
gtk_check_menu_item_set_inconsistent (GtkCheckMenuItem *check_menu_item,
                                      gboolean          setting)
{
  g_return_if_fail (GTK_IS_CHECK_MENU_ITEM (check_menu_item));
  
  setting = setting != FALSE;

  if (setting != check_menu_item->inconsistent)
    {
      check_menu_item->inconsistent = setting;
      gtk_widget_queue_draw (GTK_WIDGET (check_menu_item));
      g_object_notify (G_OBJECT (check_menu_item), "inconsistent");
    }
}

/**
 * gtk_check_menu_item_get_inconsistent:
 * @check_menu_item: a #GtkCheckMenuItem
 * 
 * Retrieves the value set by gtk_check_menu_item_set_inconsistent().
 * 
 * Return value: %TRUE if inconsistent
 **/
gboolean
gtk_check_menu_item_get_inconsistent (GtkCheckMenuItem *check_menu_item)
{
  g_return_val_if_fail (GTK_IS_CHECK_MENU_ITEM (check_menu_item), FALSE);

  return check_menu_item->inconsistent;
}

/**
 * gtk_check_menu_item_set_draw_as_radio:
 * @check_menu_item: a #GtkCheckMenuItem
 * @draw_as_radio: whether @check_menu_item is drawn like a #GtkRadioMenuItem
 *
 * Sets whether @check_menu_item is drawn like a #GtkRadioMenuItem
 *
 * Since: 2.4
 **/
void
gtk_check_menu_item_set_draw_as_radio (GtkCheckMenuItem *check_menu_item,
				       gboolean          draw_as_radio)
{
  g_return_if_fail (GTK_IS_CHECK_MENU_ITEM (check_menu_item));
  
  draw_as_radio = draw_as_radio != FALSE;

  if (draw_as_radio != check_menu_item->draw_as_radio)
    {
      check_menu_item->draw_as_radio = draw_as_radio;

      gtk_widget_queue_draw (GTK_WIDGET (check_menu_item));

      g_object_notify (G_OBJECT (check_menu_item), "draw-as-radio");
    }
}

/**
 * gtk_check_menu_item_get_draw_as_radio:
 * @check_menu_item: a #GtkCheckMenuItem
 * 
 * Returns whether @check_menu_item looks like a #GtkRadioMenuItem
 * 
 * Return value: Whether @check_menu_item looks like a #GtkRadioMenuItem
 * 
 * Since: 2.4
 **/
gboolean
gtk_check_menu_item_get_draw_as_radio (GtkCheckMenuItem *check_menu_item)
{
  g_return_val_if_fail (GTK_IS_CHECK_MENU_ITEM (check_menu_item), FALSE);
  
  return check_menu_item->draw_as_radio;
}

static void
gtk_check_menu_item_init (GtkCheckMenuItem *check_menu_item)
{
  check_menu_item->active = FALSE;
  check_menu_item->always_show_toggle = TRUE;
}

static gint
gtk_check_menu_item_expose (GtkWidget      *widget,
			    GdkEventExpose *event)
{
  if (GTK_WIDGET_CLASS (gtk_check_menu_item_parent_class)->expose_event)
    GTK_WIDGET_CLASS (gtk_check_menu_item_parent_class)->expose_event (widget, event);

  gtk_check_menu_item_draw_indicator (GTK_CHECK_MENU_ITEM (widget), &event->area);

  return FALSE;
}

static void
gtk_check_menu_item_activate (GtkMenuItem *menu_item)
{
  GtkCheckMenuItem *check_menu_item = GTK_CHECK_MENU_ITEM (menu_item);
  check_menu_item->active = !check_menu_item->active;

  gtk_check_menu_item_toggled (check_menu_item);
  gtk_widget_queue_draw (GTK_WIDGET (check_menu_item));

  GTK_MENU_ITEM_CLASS (gtk_check_menu_item_parent_class)->activate (menu_item);

  g_object_notify (G_OBJECT (check_menu_item), "active");
}

static void
gtk_check_menu_item_draw_indicator (GtkCheckMenuItem *check_menu_item,
				    GdkRectangle     *area)
{
  if (GTK_CHECK_MENU_ITEM_GET_CLASS (check_menu_item)->draw_indicator)
    GTK_CHECK_MENU_ITEM_GET_CLASS (check_menu_item)->draw_indicator (check_menu_item, area);
}

static void
gtk_real_check_menu_item_draw_indicator (GtkCheckMenuItem *check_menu_item,
					 GdkRectangle     *area)
{
  GtkWidget *widget;
  GtkStateType state_type;
  GtkShadowType shadow_type;
  gint x, y;

  widget = GTK_WIDGET (check_menu_item);

  if (gtk_widget_is_drawable (widget))
    {
      guint offset;
      guint toggle_size;
      guint toggle_spacing;
      guint horizontal_padding;
      guint indicator_size;

      gtk_widget_style_get (widget,
 			    "toggle-spacing", &toggle_spacing,
 			    "horizontal-padding", &horizontal_padding,
			    "indicator-size", &indicator_size,
 			    NULL);

      toggle_size = GTK_MENU_ITEM (check_menu_item)->toggle_size;
      offset = GTK_CONTAINER (check_menu_item)->border_width +
	widget->style->xthickness + 2; 

      if (gtk_widget_get_direction (widget) == GTK_TEXT_DIR_LTR)
	{
	  x = widget->allocation.x + offset + horizontal_padding +
	    (toggle_size - toggle_spacing - indicator_size) / 2;
	}
      else 
	{
	  x = widget->allocation.x + widget->allocation.width -
	    offset - horizontal_padding - toggle_size + toggle_spacing +
	    (toggle_size - toggle_spacing - indicator_size) / 2;
	}
      
      y = widget->allocation.y + (widget->allocation.height - indicator_size) / 2;

      if (check_menu_item->active ||
	  check_menu_item->always_show_toggle ||
	  (gtk_widget_get_state (widget) == GTK_STATE_PRELIGHT))
	{
	  state_type = gtk_widget_get_state (widget);
	  
	  if (check_menu_item->inconsistent)
	    shadow_type = GTK_SHADOW_ETCHED_IN;
	  else if (check_menu_item->active)
	    shadow_type = GTK_SHADOW_IN;
	  else 
	    shadow_type = GTK_SHADOW_OUT;
	  
	  if (!gtk_widget_is_sensitive (widget))
	    state_type = GTK_STATE_INSENSITIVE;

	  if (check_menu_item->draw_as_radio)
	    {
	      gtk_paint_option (widget->style, widget->window,
				state_type, shadow_type,
				area, widget, "option",
				x, y, indicator_size, indicator_size);
	    }
	  else
	    {
	      gtk_paint_check (widget->style, widget->window,
			       state_type, shadow_type,
			       area, widget, "check",
			       x, y, indicator_size, indicator_size);
	    }
	}
    }
}


static void
gtk_check_menu_item_get_property (GObject     *object,
				  guint        prop_id,
				  GValue      *value,
				  GParamSpec  *pspec)
{
  GtkCheckMenuItem *checkitem = GTK_CHECK_MENU_ITEM (object);
  
  switch (prop_id)
    {
    case PROP_ACTIVE:
      g_value_set_boolean (value, checkitem->active);
      break;
    case PROP_INCONSISTENT:
      g_value_set_boolean (value, checkitem->inconsistent);
      break;
    case PROP_DRAW_AS_RADIO:
      g_value_set_boolean (value, checkitem->draw_as_radio);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}


static void
gtk_check_menu_item_set_property (GObject      *object,
				  guint         prop_id,
				  const GValue *value,
				  GParamSpec   *pspec)
{
  GtkCheckMenuItem *checkitem = GTK_CHECK_MENU_ITEM (object);
  
  switch (prop_id)
    {
    case PROP_ACTIVE:
      gtk_check_menu_item_set_active (checkitem, g_value_get_boolean (value));
      break;
    case PROP_INCONSISTENT:
      gtk_check_menu_item_set_inconsistent (checkitem, g_value_get_boolean (value));
      break;
    case PROP_DRAW_AS_RADIO:
      gtk_check_menu_item_set_draw_as_radio (checkitem, g_value_get_boolean (value));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

#define __GTK_CHECK_MENU_ITEM_C__
#include "gtkaliasdef.c"
