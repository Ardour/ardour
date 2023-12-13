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

#include "gtkradioaction.h"
#include "gtkradiomenuitem.h"
#include "gtktoggleactionprivate.h"
#include "gtktoggletoolbutton.h"
#include "gtkintl.h"
#include "gtkprivate.h"
#include "gtkalias.h"

#define GTK_RADIO_ACTION_GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE ((obj), GTK_TYPE_RADIO_ACTION, GtkRadioActionPrivate))

struct _GtkRadioActionPrivate 
{
  GSList *group;
  gint    value;
};

enum 
{
  CHANGED,
  LAST_SIGNAL
};

enum 
{
  PROP_0,
  PROP_VALUE,
  PROP_GROUP,
  PROP_CURRENT_VALUE
};

static void gtk_radio_action_finalize     (GObject *object);
static void gtk_radio_action_set_property (GObject         *object,
				           guint            prop_id,
				           const GValue    *value,
				           GParamSpec      *pspec);
static void gtk_radio_action_get_property (GObject         *object,
				           guint            prop_id,
				           GValue          *value,
				           GParamSpec      *pspec);
static void gtk_radio_action_activate     (GtkAction *action);
static GtkWidget *create_menu_item        (GtkAction *action);


G_DEFINE_TYPE (GtkRadioAction, gtk_radio_action, GTK_TYPE_TOGGLE_ACTION)

static guint         radio_action_signals[LAST_SIGNAL] = { 0 };

static void
gtk_radio_action_class_init (GtkRadioActionClass *klass)
{
  GObjectClass *gobject_class;
  GtkActionClass *action_class;

  gobject_class = G_OBJECT_CLASS (klass);
  action_class = GTK_ACTION_CLASS (klass);

  gobject_class->finalize = gtk_radio_action_finalize;
  gobject_class->set_property = gtk_radio_action_set_property;
  gobject_class->get_property = gtk_radio_action_get_property;

  action_class->activate = gtk_radio_action_activate;

  action_class->create_menu_item = create_menu_item;

  /**
   * GtkRadioAction:value:
   *
   * The value is an arbitrary integer which can be used as a
   * convenient way to determine which action in the group is 
   * currently active in an ::activate or ::changed signal handler.
   * See gtk_radio_action_get_current_value() and #GtkRadioActionEntry
   * for convenient ways to get and set this property.
   *
   * Since: 2.4
   */
  g_object_class_install_property (gobject_class,
				   PROP_VALUE,
				   g_param_spec_int ("value",
						     P_("The value"),
						     P_("The value returned by gtk_radio_action_get_current_value() when this action is the current action of its group."),
						     G_MININT,
						     G_MAXINT,
						     0,
						     GTK_PARAM_READWRITE));

  /**
   * GtkRadioAction:group:
   *
   * Sets a new group for a radio action.
   *
   * Since: 2.4
   */
  g_object_class_install_property (gobject_class,
				   PROP_GROUP,
				   g_param_spec_object ("group",
							P_("Group"),
							P_("The radio action whose group this action belongs to."),
							GTK_TYPE_RADIO_ACTION,
							GTK_PARAM_WRITABLE));

  /**
   * GtkRadioAction:current-value:
   *
   * The value property of the currently active member of the group to which
   * this action belongs. 
   *
   * Since: 2.10
   */
  g_object_class_install_property (gobject_class,
				   PROP_CURRENT_VALUE,
                                   g_param_spec_int ("current-value",
						     P_("The current value"),
						     P_("The value property of the currently active member of the group to which this action belongs."),
						     G_MININT,
						     G_MAXINT,
						     0,
						     GTK_PARAM_READWRITE));

  /**
   * GtkRadioAction::changed:
   * @action: the action on which the signal is emitted
   * @current: the member of @action<!-- -->s group which has just been activated
   *
   * The ::changed signal is emitted on every member of a radio group when the
   * active member is changed. The signal gets emitted after the ::activate signals
   * for the previous and current active members.
   *
   * Since: 2.4
   */
  radio_action_signals[CHANGED] =
    g_signal_new (I_("changed"),
		  G_OBJECT_CLASS_TYPE (klass),
		  G_SIGNAL_RUN_FIRST | G_SIGNAL_NO_RECURSE,
		  G_STRUCT_OFFSET (GtkRadioActionClass, changed),  NULL, NULL,
		  g_cclosure_marshal_VOID__OBJECT,
		  G_TYPE_NONE, 1, GTK_TYPE_RADIO_ACTION);

  g_type_class_add_private (gobject_class, sizeof (GtkRadioActionPrivate));
}

static void
gtk_radio_action_init (GtkRadioAction *action)
{
  action->private_data = GTK_RADIO_ACTION_GET_PRIVATE (action);
  action->private_data->group = g_slist_prepend (NULL, action);
  action->private_data->value = 0;

  gtk_toggle_action_set_draw_as_radio (GTK_TOGGLE_ACTION (action), TRUE);
}

/**
 * gtk_radio_action_new:
 * @name: A unique name for the action
 * @label: (allow-none): The label displayed in menu items and on buttons, or %NULL
 * @tooltip: (allow-none): A tooltip for this action, or %NULL
 * @stock_id: The stock icon to display in widgets representing this
 *   action, or %NULL
 * @value: The value which gtk_radio_action_get_current_value() should
 *   return if this action is selected.
 *
 * Creates a new #GtkRadioAction object. To add the action to
 * a #GtkActionGroup and set the accelerator for the action,
 * call gtk_action_group_add_action_with_accel().
 *
 * Return value: a new #GtkRadioAction
 *
 * Since: 2.4
 */
GtkRadioAction *
gtk_radio_action_new (const gchar *name,
		      const gchar *label,
		      const gchar *tooltip,
		      const gchar *stock_id,
		      gint value)
{
  g_return_val_if_fail (name != NULL, NULL);

  return g_object_new (GTK_TYPE_RADIO_ACTION,
                       "name", name,
                       "label", label,
                       "tooltip", tooltip,
                       "stock-id", stock_id,
                       "value", value,
                       NULL);
}

static void
gtk_radio_action_finalize (GObject *object)
{
  GtkRadioAction *action;
  GSList *tmp_list;

  action = GTK_RADIO_ACTION (object);

  action->private_data->group = g_slist_remove (action->private_data->group, action);

  tmp_list = action->private_data->group;

  while (tmp_list)
    {
      GtkRadioAction *tmp_action = tmp_list->data;

      tmp_list = tmp_list->next;
      tmp_action->private_data->group = action->private_data->group;
    }

  G_OBJECT_CLASS (gtk_radio_action_parent_class)->finalize (object);
}

static void
gtk_radio_action_set_property (GObject         *object,
			       guint            prop_id,
			       const GValue    *value,
			       GParamSpec      *pspec)
{
  GtkRadioAction *radio_action;
  
  radio_action = GTK_RADIO_ACTION (object);

  switch (prop_id)
    {
    case PROP_VALUE:
      radio_action->private_data->value = g_value_get_int (value);
      break;
    case PROP_GROUP: 
      {
	GtkRadioAction *arg;
	GSList *slist = NULL;
	
	if (G_VALUE_HOLDS_OBJECT (value)) 
	  {
	    arg = GTK_RADIO_ACTION (g_value_get_object (value));
	    if (arg)
	      slist = gtk_radio_action_get_group (arg);
	    gtk_radio_action_set_group (radio_action, slist);
	  }
      }
      break;
    case PROP_CURRENT_VALUE:
      gtk_radio_action_set_current_value (radio_action,
                                          g_value_get_int (value));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
gtk_radio_action_get_property (GObject    *object,
			       guint       prop_id,
			       GValue     *value,
			       GParamSpec *pspec)
{
  GtkRadioAction *radio_action;

  radio_action = GTK_RADIO_ACTION (object);

  switch (prop_id)
    {
    case PROP_VALUE:
      g_value_set_int (value, radio_action->private_data->value);
      break;
    case PROP_CURRENT_VALUE:
      g_value_set_int (value,
                       gtk_radio_action_get_current_value (radio_action));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
gtk_radio_action_activate (GtkAction *action)
{
  GtkRadioAction *radio_action;
  GtkToggleAction *toggle_action;
  GtkToggleAction *tmp_action;
  GSList *tmp_list;

  radio_action = GTK_RADIO_ACTION (action);
  toggle_action = GTK_TOGGLE_ACTION (action);

  if (toggle_action->private_data->active)
    {
      tmp_list = radio_action->private_data->group;

      while (tmp_list)
	{
	  tmp_action = tmp_list->data;
	  tmp_list = tmp_list->next;

	  if (tmp_action->private_data->active && (tmp_action != toggle_action)) 
	    {
	      toggle_action->private_data->active = !toggle_action->private_data->active;

	      break;
	    }
	}
      g_object_notify (G_OBJECT (action), "active");
    }
  else
    {
      toggle_action->private_data->active = !toggle_action->private_data->active;
      g_object_notify (G_OBJECT (action), "active");

      tmp_list = radio_action->private_data->group;
      while (tmp_list)
	{
	  tmp_action = tmp_list->data;
	  tmp_list = tmp_list->next;

	  if (tmp_action->private_data->active && (tmp_action != toggle_action))
	    {
	      _gtk_action_emit_activate (GTK_ACTION (tmp_action));
	      break;
	    }
	}

      tmp_list = radio_action->private_data->group;
      while (tmp_list)
	{
	  tmp_action = tmp_list->data;
	  tmp_list = tmp_list->next;
	  
          g_object_notify (G_OBJECT (tmp_action), "current-value");

	  g_signal_emit (tmp_action, radio_action_signals[CHANGED], 0, radio_action);
	}
    }

  gtk_toggle_action_toggled (toggle_action);
}

static GtkWidget *
create_menu_item (GtkAction *action)
{
  return g_object_new (GTK_TYPE_CHECK_MENU_ITEM, 
		       "draw-as-radio", TRUE,
		       NULL);
}

/**
 * gtk_radio_action_get_group:
 * @action: the action object
 *
 * Returns the list representing the radio group for this object.
 * Note that the returned list is only valid until the next change
 * to the group. 
 *
 * A common way to set up a group of radio group is the following:
 * |[
 *   GSList *group = NULL;
 *   GtkRadioAction *action;
 *  
 *   while (/&ast; more actions to add &ast;/)
 *     {
 *        action = gtk_radio_action_new (...);
 *        
 *        gtk_radio_action_set_group (action, group);
 *        group = gtk_radio_action_get_group (action);
 *     }
 * ]|
 *
 * Returns:  (element-type GtkAction) (transfer none): the list representing the radio group for this object
 *
 * Since: 2.4
 */
GSList *
gtk_radio_action_get_group (GtkRadioAction *action)
{
  g_return_val_if_fail (GTK_IS_RADIO_ACTION (action), NULL);

  return action->private_data->group;
}

/**
 * gtk_radio_action_set_group:
 * @action: the action object
 * @group: a list representing a radio group
 *
 * Sets the radio group for the radio action object.
 *
 * Since: 2.4
 */
void
gtk_radio_action_set_group (GtkRadioAction *action, 
			    GSList         *group)
{
  g_return_if_fail (GTK_IS_RADIO_ACTION (action));
  g_return_if_fail (!g_slist_find (group, action));

  if (action->private_data->group)
    {
      GSList *slist;

      action->private_data->group = g_slist_remove (action->private_data->group, action);

      for (slist = action->private_data->group; slist; slist = slist->next)
	{
	  GtkRadioAction *tmp_action = slist->data;

	  tmp_action->private_data->group = action->private_data->group;
	}
    }

  action->private_data->group = g_slist_prepend (group, action);

  if (group)
    {
      GSList *slist;

      for (slist = action->private_data->group; slist; slist = slist->next)
	{
	  GtkRadioAction *tmp_action = slist->data;

	  tmp_action->private_data->group = action->private_data->group;
	}
    }
  else
    {
      gtk_toggle_action_set_active (GTK_TOGGLE_ACTION (action), TRUE);
    }
}

/**
 * gtk_radio_action_get_current_value:
 * @action: a #GtkRadioAction
 * 
 * Obtains the value property of the currently active member of 
 * the group to which @action belongs.
 * 
 * Return value: The value of the currently active group member
 *
 * Since: 2.4
 **/
gint
gtk_radio_action_get_current_value (GtkRadioAction *action)
{
  GSList *slist;

  g_return_val_if_fail (GTK_IS_RADIO_ACTION (action), 0);

  if (action->private_data->group)
    {
      for (slist = action->private_data->group; slist; slist = slist->next)
	{
	  GtkToggleAction *toggle_action = slist->data;

	  if (toggle_action->private_data->active)
	    return GTK_RADIO_ACTION (toggle_action)->private_data->value;
	}
    }

  return action->private_data->value;
}

/**
 * gtk_radio_action_set_current_value:
 * @action: a #GtkRadioAction
 * @current_value: the new value
 * 
 * Sets the currently active group member to the member with value
 * property @current_value.
 *
 * Since: 2.10
 **/
void
gtk_radio_action_set_current_value (GtkRadioAction *action,
                                    gint            current_value)
{
  GSList *slist;

  g_return_if_fail (GTK_IS_RADIO_ACTION (action));

  if (action->private_data->group)
    {
      for (slist = action->private_data->group; slist; slist = slist->next)
	{
	  GtkRadioAction *radio_action = slist->data;

	  if (radio_action->private_data->value == current_value)
            {
              gtk_toggle_action_set_active (GTK_TOGGLE_ACTION (radio_action),
                                            TRUE);
              return;
            }
	}
    }

  if (action->private_data->value == current_value)
    gtk_toggle_action_set_active (GTK_TOGGLE_ACTION (action), TRUE);
  else
    g_warning ("Radio group does not contain an action with value '%d'",
	       current_value);
}

#define __GTK_RADIO_ACTION_C__
#include "gtkaliasdef.c"
