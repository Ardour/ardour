/*
 * GTK - The GIMP Toolkit
 * Copyright (C) 1998, 1999 Red Hat, Inc.
 * All rights reserved.
 *
 * This Library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This Library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with the Gnome Library; see the file COPYING.LIB.  If not,
 * write to the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

/*
 * Author: James Henstridge <james@daa.com.au>
 *
 * Modified by the GTK+ Team and others 2003.  See the AUTHORS
 * file for a list of people on the GTK+ Team.  See the ChangeLog
 * files for a list of changes.  These files are distributed with
 * GTK+ at ftp://ftp.gtk.org/pub/gtk/. 
 */

#include "config.h"

#include "gtkintl.h"
#include "gtktoggleaction.h"
#include "gtktoggleactionprivate.h"
#include "gtktoggletoolbutton.h"
#include "gtktogglebutton.h"
#include "gtkcheckmenuitem.h"
#include "gtkprivate.h"
#include "gtkalias.h"

enum 
{
  TOGGLED,
  LAST_SIGNAL
};

enum {
  PROP_0,
  PROP_DRAW_AS_RADIO,
  PROP_ACTIVE
};

G_DEFINE_TYPE (GtkToggleAction, gtk_toggle_action, GTK_TYPE_ACTION)

static void gtk_toggle_action_activate     (GtkAction       *action);
static void set_property                   (GObject         *object,
					    guint            prop_id,
					    const GValue    *value,
					    GParamSpec      *pspec);
static void get_property                   (GObject         *object,
					    guint            prop_id,
					    GValue          *value,
					    GParamSpec      *pspec);
static GtkWidget *create_menu_item         (GtkAction       *action);


static GObjectClass *parent_class = NULL;
static guint         action_signals[LAST_SIGNAL] = { 0 };

static void
gtk_toggle_action_class_init (GtkToggleActionClass *klass)
{
  GObjectClass *gobject_class;
  GtkActionClass *action_class;

  parent_class = g_type_class_peek_parent (klass);
  gobject_class = G_OBJECT_CLASS (klass);
  action_class = GTK_ACTION_CLASS (klass);

  gobject_class->set_property = set_property;
  gobject_class->get_property = get_property;

  action_class->activate = gtk_toggle_action_activate;

  action_class->menu_item_type = GTK_TYPE_CHECK_MENU_ITEM;
  action_class->toolbar_item_type = GTK_TYPE_TOGGLE_TOOL_BUTTON;

  action_class->create_menu_item = create_menu_item;

  klass->toggled = NULL;

  /**
   * GtkToggleAction:draw-as-radio:
   *
   * Whether the proxies for this action look like radio action proxies.
   *
   * This is an appearance property and thus only applies if 
   * #GtkActivatable:use-action-appearance is %TRUE.
   */
  g_object_class_install_property (gobject_class,
                                   PROP_DRAW_AS_RADIO,
                                   g_param_spec_boolean ("draw-as-radio",
                                                         P_("Create the same proxies as a radio action"),
                                                         P_("Whether the proxies for this action look like radio action proxies"),
                                                         FALSE,
                                                         GTK_PARAM_READWRITE));

  /**
   * GtkToggleAction:active:
   *
   * If the toggle action should be active in or not.
   *
   * Since: 2.10
   */
  g_object_class_install_property (gobject_class,
                                   PROP_ACTIVE,
                                   g_param_spec_boolean ("active",
                                                         P_("Active"),
                                                         P_("If the toggle action should be active in or not"),
                                                         FALSE,
                                                         GTK_PARAM_READWRITE));

  action_signals[TOGGLED] =
    g_signal_new (I_("toggled"),
                  G_OBJECT_CLASS_TYPE (klass),
                  G_SIGNAL_RUN_FIRST,
                  G_STRUCT_OFFSET (GtkToggleActionClass, toggled),
		  NULL, NULL,
                  g_cclosure_marshal_VOID__VOID,
                  G_TYPE_NONE, 0);

  g_type_class_add_private (gobject_class, sizeof (GtkToggleActionPrivate));
}

static void
gtk_toggle_action_init (GtkToggleAction *action)
{
  action->private_data = GTK_TOGGLE_ACTION_GET_PRIVATE (action);
  action->private_data->active = FALSE;
  action->private_data->draw_as_radio = FALSE;
}

/**
 * gtk_toggle_action_new:
 * @name: A unique name for the action
 * @label: (allow-none): The label displayed in menu items and on buttons, or %NULL
 * @tooltip: (allow-none): A tooltip for the action, or %NULL
 * @stock_id: The stock icon to display in widgets representing the
 *   action, or %NULL
 *
 * Creates a new #GtkToggleAction object. To add the action to
 * a #GtkActionGroup and set the accelerator for the action,
 * call gtk_action_group_add_action_with_accel().
 *
 * Return value: a new #GtkToggleAction
 *
 * Since: 2.4
 */
GtkToggleAction *
gtk_toggle_action_new (const gchar *name,
		       const gchar *label,
		       const gchar *tooltip,
		       const gchar *stock_id)
{
  g_return_val_if_fail (name != NULL, NULL);

  return g_object_new (GTK_TYPE_TOGGLE_ACTION,
		       "name", name,
		       "label", label,
		       "tooltip", tooltip,
		       "stock-id", stock_id,
		       NULL);
}

static void
get_property (GObject     *object,
	      guint        prop_id,
	      GValue      *value,
	      GParamSpec  *pspec)
{
  GtkToggleAction *action = GTK_TOGGLE_ACTION (object);
  
  switch (prop_id)
    {
    case PROP_DRAW_AS_RADIO:
      g_value_set_boolean (value, gtk_toggle_action_get_draw_as_radio (action));
      break;
    case PROP_ACTIVE:
      g_value_set_boolean (value, gtk_toggle_action_get_active (action));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
set_property (GObject      *object,
	      guint         prop_id,
	      const GValue *value,
	      GParamSpec   *pspec)
{
  GtkToggleAction *action = GTK_TOGGLE_ACTION (object);
  
  switch (prop_id)
    {
    case PROP_DRAW_AS_RADIO:
      gtk_toggle_action_set_draw_as_radio (action, g_value_get_boolean (value));
      break;
    case PROP_ACTIVE:
      gtk_toggle_action_set_active (action, g_value_get_boolean (value));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
gtk_toggle_action_activate (GtkAction *action)
{
  GtkToggleAction *toggle_action;

  g_return_if_fail (GTK_IS_TOGGLE_ACTION (action));

  toggle_action = GTK_TOGGLE_ACTION (action);

  toggle_action->private_data->active = !toggle_action->private_data->active;

  g_object_notify (G_OBJECT (action), "active");

  gtk_toggle_action_toggled (toggle_action);
}

/**
 * gtk_toggle_action_toggled:
 * @action: the action object
 *
 * Emits the "toggled" signal on the toggle action.
 *
 * Since: 2.4
 */
void
gtk_toggle_action_toggled (GtkToggleAction *action)
{
  g_return_if_fail (GTK_IS_TOGGLE_ACTION (action));

  g_signal_emit (action, action_signals[TOGGLED], 0);
}

/**
 * gtk_toggle_action_set_active:
 * @action: the action object
 * @is_active: whether the action should be checked or not
 *
 * Sets the checked state on the toggle action.
 *
 * Since: 2.4
 */
void
gtk_toggle_action_set_active (GtkToggleAction *action, 
			      gboolean         is_active)
{
  g_return_if_fail (GTK_IS_TOGGLE_ACTION (action));

  is_active = is_active != FALSE;

  if (action->private_data->active != is_active)
    _gtk_action_emit_activate (GTK_ACTION (action));
}

/**
 * gtk_toggle_action_get_active:
 * @action: the action object
 *
 * Returns the checked state of the toggle action.

 * Returns: the checked state of the toggle action
 *
 * Since: 2.4
 */
gboolean
gtk_toggle_action_get_active (GtkToggleAction *action)
{
  g_return_val_if_fail (GTK_IS_TOGGLE_ACTION (action), FALSE);

  return action->private_data->active;
}


/**
 * gtk_toggle_action_set_draw_as_radio:
 * @action: the action object
 * @draw_as_radio: whether the action should have proxies like a radio 
 *    action
 *
 * Sets whether the action should have proxies like a radio action.
 *
 * Since: 2.4
 */
void
gtk_toggle_action_set_draw_as_radio (GtkToggleAction *action, 
				     gboolean         draw_as_radio)
{
  g_return_if_fail (GTK_IS_TOGGLE_ACTION (action));

  draw_as_radio = draw_as_radio != FALSE;

  if (action->private_data->draw_as_radio != draw_as_radio)
    {
      action->private_data->draw_as_radio = draw_as_radio;
      
      g_object_notify (G_OBJECT (action), "draw-as-radio");      
    }
}

/**
 * gtk_toggle_action_get_draw_as_radio:
 * @action: the action object
 *
 * Returns whether the action should have proxies like a radio action.
 *
 * Returns: whether the action should have proxies like a radio action.
 *
 * Since: 2.4
 */
gboolean
gtk_toggle_action_get_draw_as_radio (GtkToggleAction *action)
{
  g_return_val_if_fail (GTK_IS_TOGGLE_ACTION (action), FALSE);

  return action->private_data->draw_as_radio;
}

static GtkWidget *
create_menu_item (GtkAction *action)
{
  GtkToggleAction *toggle_action = GTK_TOGGLE_ACTION (action);

  return g_object_new (GTK_TYPE_CHECK_MENU_ITEM, 
		       "draw-as-radio", toggle_action->private_data->draw_as_radio,
		       NULL);
}

#define __GTK_TOGGLE_ACTION_C__
#include "gtkaliasdef.c"
