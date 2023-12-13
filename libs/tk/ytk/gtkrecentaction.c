/* GTK - The GIMP Toolkit
 * Recent chooser action for GtkUIManager
 *
 * Copyright (C) 2007, Emmanuele Bassi
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

#include "gtkintl.h"
#include "gtkrecentaction.h"
#include "gtkimagemenuitem.h"
#include "gtkmenutoolbutton.h"
#include "gtkrecentchooser.h"
#include "gtkrecentchoosermenu.h"
#include "gtkrecentchooserutils.h"
#include "gtkrecentchooserprivate.h"
#include "gtkprivate.h"
#include "gtkalias.h"

#define FALLBACK_ITEM_LIMIT     10

#define GTK_RECENT_ACTION_GET_PRIVATE(obj)      \
        (G_TYPE_INSTANCE_GET_PRIVATE ((obj),    \
         GTK_TYPE_RECENT_ACTION,                \
         GtkRecentActionPrivate))

struct _GtkRecentActionPrivate
{
  GtkRecentManager *manager;

  guint show_numbers   : 1;

  /* RecentChooser properties */
  guint show_private   : 1;
  guint show_not_found : 1;
  guint show_tips      : 1;
  guint show_icons     : 1;
  guint local_only     : 1;

  gint limit;

  GtkRecentSortType sort_type;
  GtkRecentSortFunc sort_func;
  gpointer          sort_data;
  GDestroyNotify    data_destroy;

  GtkRecentFilter *current_filter;

  GSList *choosers;
  GtkRecentChooser *current_chooser;
};

enum
{
  PROP_0,

  PROP_SHOW_NUMBERS
};

static void gtk_recent_chooser_iface_init (GtkRecentChooserIface *iface);

G_DEFINE_TYPE_WITH_CODE (GtkRecentAction,
                         gtk_recent_action,
                         GTK_TYPE_ACTION,
                         G_IMPLEMENT_INTERFACE (GTK_TYPE_RECENT_CHOOSER,
                                                gtk_recent_chooser_iface_init));

static gboolean
gtk_recent_action_set_current_uri (GtkRecentChooser  *chooser,
                                   const gchar       *uri,
                                   GError           **error)
{
  GtkRecentAction *action = GTK_RECENT_ACTION (chooser);
  GtkRecentActionPrivate *priv = action->priv;
  GSList *l;

  for (l = priv->choosers; l; l = l->next)
    {
      GtkRecentChooser *recent_chooser = l->data;

      if (!gtk_recent_chooser_set_current_uri (recent_chooser, uri, error))
        return FALSE;
    }

  return TRUE;
}

static gchar *
gtk_recent_action_get_current_uri (GtkRecentChooser *chooser)
{
  GtkRecentAction *recent_action = GTK_RECENT_ACTION (chooser);
  GtkRecentActionPrivate *priv = recent_action->priv;

  if (priv->current_chooser)
    return gtk_recent_chooser_get_current_uri (priv->current_chooser);

  return NULL;
}

static gboolean
gtk_recent_action_select_uri (GtkRecentChooser  *chooser,
                              const gchar       *uri,
                              GError           **error)
{
  GtkRecentAction *action = GTK_RECENT_ACTION (chooser);
  GtkRecentActionPrivate *priv = action->priv;
  GSList *l;

  for (l = priv->choosers; l; l = l->next)
    {
      GtkRecentChooser *recent_chooser = l->data;

      if (!gtk_recent_chooser_select_uri (recent_chooser, uri, error))
        return FALSE;
    }

  return TRUE;
}

static void
gtk_recent_action_unselect_uri (GtkRecentChooser *chooser,
                                const gchar      *uri)
{
  GtkRecentAction *action = GTK_RECENT_ACTION (chooser);
  GtkRecentActionPrivate *priv = action->priv;
  GSList *l;

  for (l = priv->choosers; l; l = l->next)
    {
      GtkRecentChooser *chooser = l->data;
      
      gtk_recent_chooser_unselect_uri (chooser, uri);
    }
}

static void
gtk_recent_action_select_all (GtkRecentChooser *chooser)
{
  g_warning (_("This function is not implemented for "
               "widgets of class '%s'"),
             g_type_name (G_OBJECT_TYPE (chooser)));
}

static void
gtk_recent_action_unselect_all (GtkRecentChooser *chooser)
{
  g_warning (_("This function is not implemented for "
               "widgets of class '%s'"),
             g_type_name (G_OBJECT_TYPE (chooser)));
}


static GList *
gtk_recent_action_get_items (GtkRecentChooser *chooser)
{
  GtkRecentAction *action = GTK_RECENT_ACTION (chooser);
  GtkRecentActionPrivate *priv = action->priv;

  return _gtk_recent_chooser_get_items (chooser,
                                        priv->current_filter,
                                        priv->sort_func,
                                        priv->sort_data);
}

static GtkRecentManager *
gtk_recent_action_get_recent_manager (GtkRecentChooser *chooser)
{
  return GTK_RECENT_ACTION_GET_PRIVATE (chooser)->manager;
}

static void
gtk_recent_action_set_sort_func (GtkRecentChooser  *chooser,
                                 GtkRecentSortFunc  sort_func,
                                 gpointer           sort_data,
                                 GDestroyNotify     data_destroy)
{
  GtkRecentAction *action = GTK_RECENT_ACTION (chooser);
  GtkRecentActionPrivate *priv = action->priv;
  GSList *l;
  
  if (priv->data_destroy)
    {
      priv->data_destroy (priv->sort_data);
      priv->data_destroy = NULL;
    }
      
  priv->sort_func = NULL;
  priv->sort_data = NULL;
  
  if (sort_func)
    {
      priv->sort_func = sort_func;
      priv->sort_data = sort_data;
      priv->data_destroy = data_destroy;
    }

  for (l = priv->choosers; l; l = l->next)
    {
      GtkRecentChooser *chooser_menu = l->data;
      
      gtk_recent_chooser_set_sort_func (chooser_menu, priv->sort_func,
                                        priv->sort_data,
                                        priv->data_destroy);
    }
}

static void
set_current_filter (GtkRecentAction *action,
                    GtkRecentFilter *filter)
{
  GtkRecentActionPrivate *priv = action->priv;

  g_object_ref (action);

  if (priv->current_filter)
    g_object_unref (priv->current_filter);

  priv->current_filter = filter;

  if (priv->current_filter)
    g_object_ref_sink (priv->current_filter);

  g_object_notify (G_OBJECT (action), "filter");

  g_object_unref (action);
}

static void
gtk_recent_action_add_filter (GtkRecentChooser *chooser,
                              GtkRecentFilter  *filter)
{
  GtkRecentActionPrivate *priv = GTK_RECENT_ACTION_GET_PRIVATE (chooser);

  if (priv->current_filter != filter)
    set_current_filter (GTK_RECENT_ACTION (chooser), filter);
}

static void
gtk_recent_action_remove_filter (GtkRecentChooser *chooser,
                                 GtkRecentFilter  *filter)
{
  GtkRecentActionPrivate *priv = GTK_RECENT_ACTION_GET_PRIVATE (chooser);

  if (priv->current_filter == filter)
    set_current_filter (GTK_RECENT_ACTION (chooser), NULL);
}

static GSList *
gtk_recent_action_list_filters (GtkRecentChooser *chooser)
{
  GSList *retval = NULL;
  GtkRecentFilter *current_filter;

  current_filter = GTK_RECENT_ACTION_GET_PRIVATE (chooser)->current_filter;
  retval = g_slist_prepend (retval, current_filter);

  return retval;
}


static void
gtk_recent_chooser_iface_init (GtkRecentChooserIface *iface)
{
  iface->set_current_uri = gtk_recent_action_set_current_uri;
  iface->get_current_uri = gtk_recent_action_get_current_uri;
  iface->select_uri = gtk_recent_action_select_uri;
  iface->unselect_uri = gtk_recent_action_unselect_uri;
  iface->select_all = gtk_recent_action_select_all;
  iface->unselect_all = gtk_recent_action_unselect_all;
  iface->get_items = gtk_recent_action_get_items;
  iface->get_recent_manager = gtk_recent_action_get_recent_manager;
  iface->set_sort_func = gtk_recent_action_set_sort_func;
  iface->add_filter = gtk_recent_action_add_filter;
  iface->remove_filter = gtk_recent_action_remove_filter;
  iface->list_filters = gtk_recent_action_list_filters;
}

static void
gtk_recent_action_activate (GtkAction *action)
{
  /* we have probably been invoked by a menu tool button or by a
   * direct call of gtk_action_activate(); since no item has been
   * selected, we must unset the current recent chooser pointer
   */
  GTK_RECENT_ACTION_GET_PRIVATE (action)->current_chooser = NULL;
}

static void
delegate_selection_changed (GtkRecentAction  *action,
                            GtkRecentChooser *chooser)
{
  GtkRecentActionPrivate *priv = action->priv;

  priv->current_chooser = chooser;

  g_signal_emit_by_name (action, "selection-changed");
}

static void
delegate_item_activated (GtkRecentAction  *action,
                         GtkRecentChooser *chooser)
{
  GtkRecentActionPrivate *priv = action->priv;

  priv->current_chooser = chooser;

  g_signal_emit_by_name (action, "item-activated");
}

static void
gtk_recent_action_connect_proxy (GtkAction *action,
                                 GtkWidget *widget)
{
  GtkRecentAction *recent_action = GTK_RECENT_ACTION (action);
  GtkRecentActionPrivate *priv = recent_action->priv;

  /* it can only be a recent chooser implementor anyway... */
  if (GTK_IS_RECENT_CHOOSER (widget) &&
      !g_slist_find (priv->choosers, widget))
    {
      if (priv->sort_func)
        {
          gtk_recent_chooser_set_sort_func (GTK_RECENT_CHOOSER (widget),
                                            priv->sort_func,
                                            priv->sort_data,
                                            priv->data_destroy);
        }

      g_signal_connect_swapped (widget, "selection_changed",
                                G_CALLBACK (delegate_selection_changed),
                                action);
      g_signal_connect_swapped (widget, "item-activated",
                                G_CALLBACK (delegate_item_activated),
                                action);
    }

  if (GTK_ACTION_CLASS (gtk_recent_action_parent_class)->connect_proxy)
    GTK_ACTION_CLASS (gtk_recent_action_parent_class)->connect_proxy (action, widget);
}

static void
gtk_recent_action_disconnect_proxy (GtkAction *action,
                                    GtkWidget *widget)
{
  GtkRecentAction *recent_action = GTK_RECENT_ACTION (action);
  GtkRecentActionPrivate *priv = recent_action->priv;

  /* if it was one of the recent choosers we created, remove it
   * from the list
   */
  if (g_slist_find (priv->choosers, widget))
    priv->choosers = g_slist_remove (priv->choosers, widget);

  if (GTK_ACTION_CLASS (gtk_recent_action_parent_class)->disconnect_proxy)
    GTK_ACTION_CLASS (gtk_recent_action_parent_class)->disconnect_proxy (action, widget);
}

static GtkWidget *
gtk_recent_action_create_menu (GtkAction *action)
{
  GtkRecentAction *recent_action = GTK_RECENT_ACTION (action);
  GtkRecentActionPrivate *priv = recent_action->priv;
  GtkWidget *widget;

  widget = g_object_new (GTK_TYPE_RECENT_CHOOSER_MENU,
                         "show-private", priv->show_private,
                         "show-not-found", priv->show_not_found,
                         "show-tips", priv->show_tips,
                         "show-icons", priv->show_icons,
                         "show-numbers", priv->show_numbers,
                         "limit", priv->limit,
                         "sort-type", priv->sort_type,
                         "recent-manager", priv->manager,
                         "filter", priv->current_filter,
                         "local-only", priv->local_only,
                         NULL);
  
  if (priv->sort_func)
    {
      gtk_recent_chooser_set_sort_func (GTK_RECENT_CHOOSER (widget),
                                        priv->sort_func,
                                        priv->sort_data,
                                        priv->data_destroy);
    }

  g_signal_connect_swapped (widget, "selection_changed",
                            G_CALLBACK (delegate_selection_changed),
                            recent_action);
  g_signal_connect_swapped (widget, "item-activated",
                            G_CALLBACK (delegate_item_activated),
                            recent_action);

  /* keep track of the choosers we create */
  priv->choosers = g_slist_prepend (priv->choosers, widget);

  return widget;
}

static GtkWidget *
gtk_recent_action_create_menu_item (GtkAction *action)
{
  GtkWidget *menu;
  GtkWidget *menuitem;

  menu = gtk_recent_action_create_menu (action);
  menuitem = g_object_new (GTK_TYPE_IMAGE_MENU_ITEM, NULL);
  gtk_menu_item_set_submenu (GTK_MENU_ITEM (menuitem), menu);
  gtk_widget_show (menu);

  return menuitem;
}

static GtkWidget *
gtk_recent_action_create_tool_item (GtkAction *action)
{
  GtkWidget *menu;
  GtkWidget *toolitem;

  menu = gtk_recent_action_create_menu (action);
  toolitem = g_object_new (GTK_TYPE_MENU_TOOL_BUTTON, NULL);
  gtk_menu_tool_button_set_menu (GTK_MENU_TOOL_BUTTON (toolitem), menu);
  gtk_widget_show (menu);

  return toolitem;
}

static void
set_recent_manager (GtkRecentAction  *action,
                    GtkRecentManager *manager)
{
  GtkRecentActionPrivate *priv = action->priv;

  if (manager)
    priv->manager = NULL;
  else
    priv->manager = gtk_recent_manager_get_default ();
}

static void
gtk_recent_action_finalize (GObject *gobject)
{
  GtkRecentAction *action = GTK_RECENT_ACTION (gobject);
  GtkRecentActionPrivate *priv = action->priv;

  priv->manager = NULL;
  
  if (priv->data_destroy)
    {
      priv->data_destroy (priv->sort_data);
      priv->data_destroy = NULL;
    }

  priv->sort_data = NULL;
  priv->sort_func = NULL;

  g_slist_free (priv->choosers);

  G_OBJECT_CLASS (gtk_recent_action_parent_class)->finalize (gobject);
}

static void
gtk_recent_action_dispose (GObject *gobject)
{
  GtkRecentAction *action = GTK_RECENT_ACTION (gobject);
  GtkRecentActionPrivate *priv = action->priv;

  if (priv->current_filter)
    {
      g_object_unref (priv->current_filter);
      priv->current_filter = NULL;
    }

  G_OBJECT_CLASS (gtk_recent_action_parent_class)->dispose (gobject);
}

static void
gtk_recent_action_set_property (GObject      *gobject,
                                guint         prop_id,
                                const GValue *value,
                                GParamSpec   *pspec)
{
  GtkRecentAction *action = GTK_RECENT_ACTION (gobject);
  GtkRecentActionPrivate *priv = action->priv;

  switch (prop_id)
    {
    case PROP_SHOW_NUMBERS:
      priv->show_numbers = g_value_get_boolean (value);
      break;
    case GTK_RECENT_CHOOSER_PROP_SHOW_PRIVATE:
      priv->show_private = g_value_get_boolean (value);
      break;
    case GTK_RECENT_CHOOSER_PROP_SHOW_NOT_FOUND:
      priv->show_not_found = g_value_get_boolean (value);
      break;
    case GTK_RECENT_CHOOSER_PROP_SHOW_TIPS:
      priv->show_tips = g_value_get_boolean (value);
      break;
    case GTK_RECENT_CHOOSER_PROP_SHOW_ICONS:
      priv->show_icons = g_value_get_boolean (value);
      break;
    case GTK_RECENT_CHOOSER_PROP_LIMIT:
      priv->limit = g_value_get_int (value);
      break;
    case GTK_RECENT_CHOOSER_PROP_LOCAL_ONLY:
      priv->local_only = g_value_get_boolean (value);
      break;
    case GTK_RECENT_CHOOSER_PROP_SORT_TYPE:
      priv->sort_type = g_value_get_enum (value);
      break;
    case GTK_RECENT_CHOOSER_PROP_FILTER:
      set_current_filter (action, g_value_get_object (value));
      break;
    case GTK_RECENT_CHOOSER_PROP_SELECT_MULTIPLE:
      g_warning ("%s: Choosers of type `%s' do not support selecting multiple items.",
                 G_STRFUNC,
                 G_OBJECT_TYPE_NAME (gobject));
      return;
    case GTK_RECENT_CHOOSER_PROP_RECENT_MANAGER:
      set_recent_manager (action, g_value_get_object (value));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (gobject, prop_id, pspec);
      return;
    }
}

static void
gtk_recent_action_get_property (GObject    *gobject,
                                guint       prop_id,
                                GValue     *value,
                                GParamSpec *pspec)
{
  GtkRecentActionPrivate *priv = GTK_RECENT_ACTION_GET_PRIVATE (gobject);

  switch (prop_id)
    {
    case PROP_SHOW_NUMBERS:
      g_value_set_boolean (value, priv->show_numbers);
      break;
    case GTK_RECENT_CHOOSER_PROP_SHOW_PRIVATE:
      g_value_set_boolean (value, priv->show_private);
      break;
    case GTK_RECENT_CHOOSER_PROP_SHOW_NOT_FOUND:
      g_value_set_boolean (value, priv->show_not_found);
      break;
    case GTK_RECENT_CHOOSER_PROP_SHOW_TIPS:
      g_value_set_boolean (value, priv->show_tips);
      break;
    case GTK_RECENT_CHOOSER_PROP_SHOW_ICONS:
      g_value_set_boolean (value, priv->show_icons);
      break;
    case GTK_RECENT_CHOOSER_PROP_LIMIT:
      g_value_set_int (value, priv->limit);
      break;
    case GTK_RECENT_CHOOSER_PROP_LOCAL_ONLY:
      g_value_set_boolean (value, priv->local_only);
      break;
    case GTK_RECENT_CHOOSER_PROP_SORT_TYPE:
      g_value_set_enum (value, priv->sort_type);
      break;
    case GTK_RECENT_CHOOSER_PROP_FILTER:
      g_value_set_object (value, priv->current_filter);
      break;
    case GTK_RECENT_CHOOSER_PROP_SELECT_MULTIPLE:
      g_value_set_boolean (value, FALSE);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (gobject, prop_id, pspec);
      break;
    }
}

static void
gtk_recent_action_class_init (GtkRecentActionClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
  GtkActionClass *action_class = GTK_ACTION_CLASS (klass);

  g_type_class_add_private (klass, sizeof (GtkRecentActionPrivate));

  gobject_class->finalize = gtk_recent_action_finalize;
  gobject_class->dispose = gtk_recent_action_dispose;
  gobject_class->set_property = gtk_recent_action_set_property;
  gobject_class->get_property = gtk_recent_action_get_property;

  action_class->activate = gtk_recent_action_activate;
  action_class->connect_proxy = gtk_recent_action_connect_proxy;
  action_class->disconnect_proxy = gtk_recent_action_disconnect_proxy;
  action_class->create_menu_item = gtk_recent_action_create_menu_item;
  action_class->create_tool_item = gtk_recent_action_create_tool_item;
  action_class->create_menu = gtk_recent_action_create_menu;
  action_class->menu_item_type = GTK_TYPE_IMAGE_MENU_ITEM;
  action_class->toolbar_item_type = GTK_TYPE_MENU_TOOL_BUTTON;

  _gtk_recent_chooser_install_properties (gobject_class);

  g_object_class_install_property (gobject_class,
                                   PROP_SHOW_NUMBERS,
                                   g_param_spec_boolean ("show-numbers",
                                                         P_("Show Numbers"),
                                                         P_("Whether the items should be displayed with a number"),
                                                         FALSE,
                                                         G_PARAM_READWRITE));

}

static void
gtk_recent_action_init (GtkRecentAction *action)
{
  GtkRecentActionPrivate *priv;

  action->priv = priv = GTK_RECENT_ACTION_GET_PRIVATE (action);

  priv->show_numbers = FALSE;
  priv->show_icons = TRUE;
  priv->show_tips = FALSE;
  priv->show_not_found = TRUE;
  priv->show_private = FALSE;
  priv->local_only = TRUE;

  priv->limit = FALLBACK_ITEM_LIMIT;

  priv->sort_type = GTK_RECENT_SORT_NONE;
  priv->sort_func = NULL;
  priv->sort_data = NULL;
  priv->data_destroy = NULL;

  priv->current_filter = NULL;

  priv->manager = NULL;
}

/**
 * gtk_recent_action_new:
 * @name: a unique name for the action
 * @label: (allow-none): the label displayed in menu items and on buttons, or %NULL
 * @tooltip: (allow-none): a tooltip for the action, or %NULL
 * @stock_id: the stock icon to display in widgets representing the
 *   action, or %NULL
 *
 * Creates a new #GtkRecentAction object. To add the action to
 * a #GtkActionGroup and set the accelerator for the action,
 * call gtk_action_group_add_action_with_accel().
 *
 * Return value: the newly created #GtkRecentAction.
 *
 * Since: 2.12
 */
GtkAction *
gtk_recent_action_new (const gchar *name,
                       const gchar *label,
                       const gchar *tooltip,
                       const gchar *stock_id)
{
  g_return_val_if_fail (name != NULL, NULL);

  return g_object_new (GTK_TYPE_RECENT_ACTION,
                       "name", name,
                       "label", label,
                       "tooltip", tooltip,
                       "stock-id", stock_id,
                       NULL);
}

/**
 * gtk_recent_action_new_for_manager:
 * @name: a unique name for the action
 * @label: (allow-none): the label displayed in menu items and on buttons, or %NULL
 * @tooltip: (allow-none): a tooltip for the action, or %NULL
 * @stock_id: the stock icon to display in widgets representing the
 *   action, or %NULL
 * @manager: (allow-none): a #GtkRecentManager, or %NULL for using the default
 *   #GtkRecentManager
 *
 * Creates a new #GtkRecentAction object. To add the action to
 * a #GtkActionGroup and set the accelerator for the action,
 * call gtk_action_group_add_action_with_accel().
 *
 * Return value: the newly created #GtkRecentAction
 * 
 * Since: 2.12
 */
GtkAction *
gtk_recent_action_new_for_manager (const gchar      *name,
                                   const gchar      *label,
                                   const gchar      *tooltip,
                                   const gchar      *stock_id,
                                   GtkRecentManager *manager)
{
  g_return_val_if_fail (name != NULL, NULL);
  g_return_val_if_fail (manager == NULL || GTK_IS_RECENT_MANAGER (manager), NULL);

  return g_object_new (GTK_TYPE_RECENT_ACTION,
                       "name", name,
                       "label", label,
                       "tooltip", tooltip,
                       "stock-id", stock_id,
                       "recent-manager", manager,
                       NULL);
}

/**
 * gtk_recent_action_get_show_numbers:
 * @action: a #GtkRecentAction
 *
 * Returns the value set by gtk_recent_chooser_menu_set_show_numbers().
 *
 * Return value: %TRUE if numbers should be shown.
 *
 * Since: 2.12
 */
gboolean
gtk_recent_action_get_show_numbers (GtkRecentAction *action)
{
  g_return_val_if_fail (GTK_IS_RECENT_ACTION (action), FALSE);

  return action->priv->show_numbers;
}

/**
 * gtk_recent_action_set_show_numbers:
 * @action: a #GtkRecentAction
 * @show_numbers: %TRUE if the shown items should be numbered
 *
 * Sets whether a number should be added to the items shown by the
 * widgets representing @action. The numbers are shown to provide
 * a unique character for a mnemonic to be used inside the menu item's
 * label. Only the first ten items get a number to avoid clashes.
 *
 * Since: 2.12
 */
void
gtk_recent_action_set_show_numbers (GtkRecentAction *action,
                                    gboolean         show_numbers)
{
  GtkRecentActionPrivate *priv;

  g_return_if_fail (GTK_IS_RECENT_ACTION (action));

  priv = action->priv;

  if (priv->show_numbers != show_numbers)
    {
      g_object_ref (action);

      priv->show_numbers = show_numbers;

      g_object_notify (G_OBJECT (action), "show-numbers");
      g_object_unref (action);
    }
}

#define __GTK_RECENT_ACTION_C__
#include "gtkaliasdef.c"
