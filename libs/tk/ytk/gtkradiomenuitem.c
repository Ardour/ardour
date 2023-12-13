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
#include "gtkaccellabel.h"
#include "gtkmarshalers.h"
#include "gtkradiomenuitem.h"
#include "gtkactivatable.h"
#include "gtkprivate.h"
#include "gtkintl.h"
#include "gtkalias.h"


enum {
  PROP_0,
  PROP_GROUP
};


static void gtk_radio_menu_item_destroy        (GtkObject             *object);
static void gtk_radio_menu_item_activate       (GtkMenuItem           *menu_item);
static void gtk_radio_menu_item_set_property   (GObject               *object,
						guint                  prop_id,
						const GValue          *value,
						GParamSpec            *pspec);
static void gtk_radio_menu_item_get_property   (GObject               *object,
						guint                  prop_id,
						GValue                *value,
						GParamSpec            *pspec);

static guint group_changed_signal = 0;

G_DEFINE_TYPE (GtkRadioMenuItem, gtk_radio_menu_item, GTK_TYPE_CHECK_MENU_ITEM)

GtkWidget*
gtk_radio_menu_item_new (GSList *group)
{
  GtkRadioMenuItem *radio_menu_item;

  radio_menu_item = g_object_new (GTK_TYPE_RADIO_MENU_ITEM, NULL);

  gtk_radio_menu_item_set_group (radio_menu_item, group);

  return GTK_WIDGET (radio_menu_item);
}

static void
gtk_radio_menu_item_set_property (GObject      *object,
				  guint         prop_id,
				  const GValue *value,
				  GParamSpec   *pspec)
{
  GtkRadioMenuItem *radio_menu_item;

  radio_menu_item = GTK_RADIO_MENU_ITEM (object);

  switch (prop_id)
    {
      GSList *slist;

    case PROP_GROUP:
      if (G_VALUE_HOLDS_OBJECT (value))
	slist = gtk_radio_menu_item_get_group ((GtkRadioMenuItem*) g_value_get_object (value));
      else
	slist = NULL;
      gtk_radio_menu_item_set_group (radio_menu_item, slist);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
gtk_radio_menu_item_get_property (GObject    *object,
				  guint       prop_id,
				  GValue     *value,
				  GParamSpec *pspec)
{
  switch (prop_id)
    {
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

void
gtk_radio_menu_item_set_group (GtkRadioMenuItem *radio_menu_item,
			       GSList           *group)
{
  GtkWidget *old_group_singleton = NULL;
  GtkWidget *new_group_singleton = NULL;
  
  g_return_if_fail (GTK_IS_RADIO_MENU_ITEM (radio_menu_item));
  g_return_if_fail (!g_slist_find (group, radio_menu_item));

  if (radio_menu_item->group)
    {
      GSList *slist;

      radio_menu_item->group = g_slist_remove (radio_menu_item->group, radio_menu_item);
      
      if (radio_menu_item->group && !radio_menu_item->group->next)
	old_group_singleton = g_object_ref (radio_menu_item->group->data);
	  
      for (slist = radio_menu_item->group; slist; slist = slist->next)
	{
	  GtkRadioMenuItem *tmp_item;
	  
	  tmp_item = slist->data;
	  
	  tmp_item->group = radio_menu_item->group;
	}
    }
  
  if (group && !group->next)
    new_group_singleton = g_object_ref (group->data);
  
  radio_menu_item->group = g_slist_prepend (group, radio_menu_item);
  
  if (group)
    {
      GSList *slist;
      
      for (slist = group; slist; slist = slist->next)
	{
	  GtkRadioMenuItem *tmp_item;
	  
	  tmp_item = slist->data;
	  
	  tmp_item->group = radio_menu_item->group;
	}
    }
  else
    {
      GTK_CHECK_MENU_ITEM (radio_menu_item)->active = TRUE;
      /* gtk_widget_set_state (GTK_WIDGET (radio_menu_item), GTK_STATE_ACTIVE);
       */
    }

  g_object_ref (radio_menu_item);

  g_object_notify (G_OBJECT (radio_menu_item), "group");
  g_signal_emit (radio_menu_item, group_changed_signal, 0);
  if (old_group_singleton)
    {
      g_signal_emit (old_group_singleton, group_changed_signal, 0);
      g_object_unref (old_group_singleton);
    }
  if (new_group_singleton)
    {
      g_signal_emit (new_group_singleton, group_changed_signal, 0);
      g_object_unref (new_group_singleton);
    }

  g_object_unref (radio_menu_item);
}


/**
 * gtk_radio_menu_item_new_with_label:
 * @group: (element-type GtkRadioMenuItem) (transfer full):
 * @label: the text for the label
 *
 * Creates a new #GtkRadioMenuItem whose child is a simple #GtkLabel.
 *
 * Returns: (transfer none): A new #GtkRadioMenuItem
 */
GtkWidget*
gtk_radio_menu_item_new_with_label (GSList *group,
				    const gchar *label)
{
  GtkWidget *radio_menu_item;
  GtkWidget *accel_label;

  radio_menu_item = gtk_radio_menu_item_new (group);
  accel_label = gtk_accel_label_new (label);
  gtk_misc_set_alignment (GTK_MISC (accel_label), 0.0, 0.5);
  gtk_container_add (GTK_CONTAINER (radio_menu_item), accel_label);
  gtk_accel_label_set_accel_widget (GTK_ACCEL_LABEL (accel_label), radio_menu_item);
  gtk_widget_show (accel_label);

  return radio_menu_item;
}


/**
 * gtk_radio_menu_item_new_with_mnemonic:
 * @group: group the radio menu item is inside
 * @label: the text of the button, with an underscore in front of the
 *         mnemonic character
 * @returns: a new #GtkRadioMenuItem
 *
 * Creates a new #GtkRadioMenuItem containing a label. The label
 * will be created using gtk_label_new_with_mnemonic(), so underscores
 * in @label indicate the mnemonic for the menu item.
 **/
GtkWidget*
gtk_radio_menu_item_new_with_mnemonic (GSList *group,
				       const gchar *label)
{
  GtkWidget *radio_menu_item;
  GtkWidget *accel_label;

  radio_menu_item = gtk_radio_menu_item_new (group);
  accel_label = g_object_new (GTK_TYPE_ACCEL_LABEL, NULL);
  gtk_label_set_text_with_mnemonic (GTK_LABEL (accel_label), label);
  gtk_misc_set_alignment (GTK_MISC (accel_label), 0.0, 0.5);

  gtk_container_add (GTK_CONTAINER (radio_menu_item), accel_label);
  gtk_accel_label_set_accel_widget (GTK_ACCEL_LABEL (accel_label), radio_menu_item);
  gtk_widget_show (accel_label);

  return radio_menu_item;
}

/**
 * gtk_radio_menu_item_new_from_widget:
 * @group: An existing #GtkRadioMenuItem
 *
 * Creates a new #GtkRadioMenuItem adding it to the same group as @group.
 *
 * Return value: (transfer none): The new #GtkRadioMenuItem
 *
 * Since: 2.4
 **/
GtkWidget *
gtk_radio_menu_item_new_from_widget (GtkRadioMenuItem *group)
{
  GSList *list = NULL;
  
  g_return_val_if_fail (GTK_IS_RADIO_MENU_ITEM (group), NULL);

  if (group)
    list = gtk_radio_menu_item_get_group (group);
  
  return gtk_radio_menu_item_new (list);
}

/**
 * gtk_radio_menu_item_new_with_mnemonic_from_widget:
 * @group: An existing #GtkRadioMenuItem
 * @label: the text of the button, with an underscore in front of the
 *         mnemonic character
 *
 * Creates a new GtkRadioMenuItem containing a label. The label will be
 * created using gtk_label_new_with_mnemonic(), so underscores in label
 * indicate the mnemonic for the menu item.
 *
 * The new #GtkRadioMenuItem is added to the same group as @group.
 *
 * Return value: (transfer none): The new #GtkRadioMenuItem
 *
 * Since: 2.4
 **/
GtkWidget *
gtk_radio_menu_item_new_with_mnemonic_from_widget (GtkRadioMenuItem *group,
						   const gchar *label)
{
  GSList *list = NULL;

  g_return_val_if_fail (GTK_IS_RADIO_MENU_ITEM (group), NULL);

  if (group)
    list = gtk_radio_menu_item_get_group (group);

  return gtk_radio_menu_item_new_with_mnemonic (list, label);
}

/**
 * gtk_radio_menu_item_new_with_label_from_widget:
 * @group: an existing #GtkRadioMenuItem
 * @label: the text for the label
 *
 * Creates a new GtkRadioMenuItem whose child is a simple GtkLabel.
 * The new #GtkRadioMenuItem is added to the same group as @group.
 *
 * Return value: (transfer none): The new #GtkRadioMenuItem
 *
 * Since: 2.4
 **/
GtkWidget *
gtk_radio_menu_item_new_with_label_from_widget (GtkRadioMenuItem *group,
						const gchar *label)
{
  GSList *list = NULL;

  g_return_val_if_fail (GTK_IS_RADIO_MENU_ITEM (group), NULL);

  if (group)
    list = gtk_radio_menu_item_get_group (group);

  return gtk_radio_menu_item_new_with_label (list, label);
}

/**
 * gtk_radio_menu_item_get_group:
 * @radio_menu_item: a #GtkRadioMenuItem
 *
 * Returns the group to which the radio menu item belongs, as a #GList of
 * #GtkRadioMenuItem. The list belongs to GTK+ and should not be freed.
 *
 * Returns: (transfer none): the group of @radio_menu_item
 */
GSList*
gtk_radio_menu_item_get_group (GtkRadioMenuItem *radio_menu_item)
{
  g_return_val_if_fail (GTK_IS_RADIO_MENU_ITEM (radio_menu_item), NULL);

  return radio_menu_item->group;
}


static void
gtk_radio_menu_item_class_init (GtkRadioMenuItemClass *klass)
{
  GObjectClass *gobject_class;  
  GtkObjectClass *object_class;
  GtkMenuItemClass *menu_item_class;

  gobject_class = G_OBJECT_CLASS (klass);
  object_class = GTK_OBJECT_CLASS (klass);
  menu_item_class = GTK_MENU_ITEM_CLASS (klass);

  gobject_class->set_property = gtk_radio_menu_item_set_property;
  gobject_class->get_property = gtk_radio_menu_item_get_property;

  /**
   * GtkRadioMenuItem:group:
   * 
   * The radio menu item whose group this widget belongs to.
   * 
   * Since: 2.8
   */
  g_object_class_install_property (gobject_class,
				   PROP_GROUP,
				   g_param_spec_object ("group",
							P_("Group"),
							P_("The radio menu item whose group this widget belongs to."),
							GTK_TYPE_RADIO_MENU_ITEM,
							GTK_PARAM_WRITABLE));

  object_class->destroy = gtk_radio_menu_item_destroy;

  menu_item_class->activate = gtk_radio_menu_item_activate;

  /**
   * GtkStyle::group-changed:
   * @style: the object which received the signal
   *
   * Emitted when the group of radio menu items that a radio menu item belongs
   * to changes. This is emitted when a radio menu item switches from
   * being alone to being part of a group of 2 or more menu items, or
   * vice-versa, and when a button is moved from one group of 2 or
   * more menu items ton a different one, but not when the composition
   * of the group that a menu item belongs to changes.
   *
   * Since: 2.4
   */
  group_changed_signal = g_signal_new (I_("group-changed"),
				       G_OBJECT_CLASS_TYPE (object_class),
				       G_SIGNAL_RUN_FIRST,
				       G_STRUCT_OFFSET (GtkRadioMenuItemClass, group_changed),
				       NULL, NULL,
				       _gtk_marshal_VOID__VOID,
				       G_TYPE_NONE, 0);
}

static void
gtk_radio_menu_item_init (GtkRadioMenuItem *radio_menu_item)
{
  radio_menu_item->group = g_slist_prepend (NULL, radio_menu_item);
  gtk_check_menu_item_set_draw_as_radio (GTK_CHECK_MENU_ITEM (radio_menu_item), TRUE);
}

static void
gtk_radio_menu_item_destroy (GtkObject *object)
{
  GtkRadioMenuItem *radio_menu_item = GTK_RADIO_MENU_ITEM (object);
  GtkWidget *old_group_singleton = NULL;
  GtkRadioMenuItem *tmp_menu_item;
  GSList *tmp_list;
  gboolean was_in_group;

  was_in_group = radio_menu_item->group && radio_menu_item->group->next;
  
  radio_menu_item->group = g_slist_remove (radio_menu_item->group,
					   radio_menu_item);
  if (radio_menu_item->group && !radio_menu_item->group->next)
    old_group_singleton = radio_menu_item->group->data;

  tmp_list = radio_menu_item->group;

  while (tmp_list)
    {
      tmp_menu_item = tmp_list->data;
      tmp_list = tmp_list->next;

      tmp_menu_item->group = radio_menu_item->group;
    }

  /* this radio menu item is no longer in the group */
  radio_menu_item->group = NULL;
  
  if (old_group_singleton)
    g_signal_emit (old_group_singleton, group_changed_signal, 0);
  if (was_in_group)
    g_signal_emit (radio_menu_item, group_changed_signal, 0);

  GTK_OBJECT_CLASS (gtk_radio_menu_item_parent_class)->destroy (object);
}

static void
gtk_radio_menu_item_activate (GtkMenuItem *menu_item)
{
  GtkRadioMenuItem *radio_menu_item = GTK_RADIO_MENU_ITEM (menu_item);
  GtkCheckMenuItem *check_menu_item = GTK_CHECK_MENU_ITEM (menu_item);
  GtkCheckMenuItem *tmp_menu_item;
  GtkAction        *action;
  GSList *tmp_list;
  gint toggled;

  action = gtk_activatable_get_related_action (GTK_ACTIVATABLE (menu_item));
  if (action && gtk_menu_item_get_submenu (menu_item) == NULL)
    gtk_action_activate (action);

  toggled = FALSE;

  if (check_menu_item->active)
    {
      tmp_menu_item = NULL;
      tmp_list = radio_menu_item->group;

      while (tmp_list)
	{
	  tmp_menu_item = tmp_list->data;
	  tmp_list = tmp_list->next;

	  if (tmp_menu_item->active && (tmp_menu_item != check_menu_item))
	    break;

	  tmp_menu_item = NULL;
	}

      if (tmp_menu_item)
	{
	  toggled = TRUE;
	  check_menu_item->active = !check_menu_item->active;
	}
    }
  else
    {
      toggled = TRUE;
      check_menu_item->active = !check_menu_item->active;

      tmp_list = radio_menu_item->group;
      while (tmp_list)
	{
	  tmp_menu_item = tmp_list->data;
	  tmp_list = tmp_list->next;

	  if (tmp_menu_item->active && (tmp_menu_item != check_menu_item))
	    {
	      gtk_menu_item_activate (GTK_MENU_ITEM (tmp_menu_item));
	      break;
	    }
	}
    }

  if (toggled)
    {
      gtk_check_menu_item_toggled (check_menu_item);
    }

  gtk_widget_queue_draw (GTK_WIDGET (radio_menu_item));
}

#define __GTK_RADIO_MENU_ITEM_C__
#include "gtkaliasdef.c"
