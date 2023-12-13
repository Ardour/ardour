/* ATK -  Accessibility Toolkit
 * Copyright 2001 Sun Microsystems Inc.
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

#include "atkaction.h"

/**
 * SECTION:atkaction
 * @Short_description: The ATK interface provided by UI components
 * which the user can activate/interact with.
 * @Title:AtkAction
 *
 * #AtkAction should be implemented by instances of #AtkObject classes
 * with which the user can interact directly, i.e. buttons,
 * checkboxes, scrollbars, e.g. components which are not "passive"
 * providers of UI information.
 *
 * Exceptions: when the user interaction is already covered by another
 * appropriate interface such as #AtkEditableText (insert/delete text,
 * etc.) or #AtkValue (set value) then these actions should not be
 * exposed by #AtkAction as well.
 *
 * Though most UI interactions on components should be invocable via
 * keyboard as well as mouse, there will generally be a close mapping
 * between "mouse actions" that are possible on a component and the
 * AtkActions.  Where mouse and keyboard actions are redundant in
 * effect, #AtkAction should expose only one action rather than
 * exposing redundant actions if possible.  By convention we have been
 * using "mouse centric" terminology for #AtkAction names.
 *
 */

GType
atk_action_get_type (void)
{
  static GType type = 0;

  if (!type) {
    GTypeInfo tinfo =
    {
      sizeof (AtkActionIface),
      (GBaseInitFunc) NULL,
      (GBaseFinalizeFunc) NULL,

    };

    type = g_type_register_static (G_TYPE_INTERFACE, "AtkAction", &tinfo, 0);
  }

  return type;
}

/**
 * atk_action_do_action:
 * @action: a #GObject instance that implements AtkActionIface
 * @i: the action index corresponding to the action to be performed 
 *
 * Perform the specified action on the object.
 *
 * Returns: %TRUE if success, %FALSE otherwise
 *
 **/
gboolean
atk_action_do_action (AtkAction *obj,
                      gint      i)
{
  AtkActionIface *iface;

  g_return_val_if_fail (ATK_IS_ACTION (obj), FALSE);

  iface = ATK_ACTION_GET_IFACE (obj);

  if (iface->do_action)
    return (iface->do_action) (obj, i);
  else
    return FALSE;
}

/**
 * atk_action_get_n_actions:
 * @action: a #GObject instance that implements AtkActionIface
 * 
 * Gets the number of accessible actions available on the object.
 * If there are more than one, the first one is considered the
 * "default" action of the object.
 *
 * Returns: a the number of actions, or 0 if @action does not
 * implement this interface.
 **/
gint
atk_action_get_n_actions  (AtkAction *obj)
{
  AtkActionIface *iface;

  g_return_val_if_fail (ATK_IS_ACTION (obj), 0);

  iface = ATK_ACTION_GET_IFACE (obj);

  if (iface->get_n_actions)
    return (iface->get_n_actions) (obj);
  else
    return 0;
}

/**
 * atk_action_get_description:
 * @action: a #GObject instance that implements AtkActionIface
 * @i: the action index corresponding to the action to be performed 
 *
 * Returns a description of the specified action of the object.
 *
 * Returns: (nullable): a description string, or %NULL if @action does
 * not implement this interface.
 **/
const gchar*
atk_action_get_description (AtkAction *obj,
                            gint      i)
{
  AtkActionIface *iface;

  g_return_val_if_fail (ATK_IS_ACTION (obj), NULL);

  iface = ATK_ACTION_GET_IFACE (obj);

  if (iface->get_description)
    return (iface->get_description) (obj, i);
  else
    return NULL;
}

/**
 * atk_action_get_name:
 * @action: a #GObject instance that implements AtkActionIface
 * @i: the action index corresponding to the action to be performed 
 *
 * Returns a non-localized string naming the specified action of the 
 * object. This name is generally not descriptive of the end result 
 * of the action, but instead names the 'interaction type' which the 
 * object supports. By convention, the above strings should be used to 
 * represent the actions which correspond to the common point-and-click 
 * interaction techniques of the same name: i.e. 
 * "click", "press", "release", "drag", "drop", "popup", etc.
 * The "popup" action should be used to pop up a context menu for the 
 * object, if one exists.
 *
 * For technical reasons, some toolkits cannot guarantee that the 
 * reported action is actually 'bound' to a nontrivial user event;
 * i.e. the result of some actions via atk_action_do_action() may be
 * NIL.
 *
 * Returns: (nullable): a name string, or %NULL if @action does not
 * implement this interface.
 **/
const gchar*
atk_action_get_name (AtkAction *obj,
                     gint      i)
{
  AtkActionIface *iface;

  g_return_val_if_fail (ATK_IS_ACTION (obj), NULL);

  iface = ATK_ACTION_GET_IFACE (obj);

  if (iface->get_name)
    return (iface->get_name) (obj, i);
  else
    return NULL;
}

/**
 * atk_action_get_localized_name:
 * @action: a #GObject instance that implements AtkActionIface
 * @i: the action index corresponding to the action to be performed 
 *
 * Returns the localized name of the specified action of the object.
 *
 * Returns: (nullable): a name string, or %NULL if @action does not
 * implement this interface.
 **/
const gchar*
atk_action_get_localized_name (AtkAction *obj,
                               gint      i)
{
  AtkActionIface *iface;

  g_return_val_if_fail (ATK_IS_ACTION (obj), NULL);

  iface = ATK_ACTION_GET_IFACE (obj);

  if (iface->get_localized_name)
    return (iface->get_localized_name) (obj, i);
  else
    return NULL;
}

/**
 * atk_action_get_keybinding:
 * @action: a #GObject instance that implements AtkActionIface
 * @i: the action index corresponding to the action to be performed
 *
 * Gets the keybinding which can be used to activate this action, if one
 * exists. The string returned should contain localized, human-readable,
 * key sequences as they would appear when displayed on screen. It must
 * be in the format "mnemonic;sequence;shortcut".
 *
 * - The mnemonic key activates the object if it is presently enabled onscreen.
 *   This typically corresponds to the underlined letter within the widget.
 *   Example: "n" in a traditional "New..." menu item or the "a" in "Apply" for
 *   a button.
 * - The sequence is the full list of keys which invoke the action even if the
 *   relevant element is not currently shown on screen. For instance, for a menu
 *   item the sequence is the keybindings used to open the parent menus before
 *   invoking. The sequence string is colon-delimited. Example: "Alt+F:N" in a
 *   traditional "New..." menu item.
 * - The shortcut, if it exists, will invoke the same action without showing
 *   the component or its enclosing menus or dialogs. Example: "Ctrl+N" in a
 *   traditional "New..." menu item.
 *
 * Example: For a traditional "New..." menu item, the expected return value
 * would be: "N;Alt+F:N;Ctrl+N" for the English locale and "N;Alt+D:N;Strg+N"
 * for the German locale. If, hypothetically, this menu item lacked a mnemonic,
 * it would be represented by ";;Ctrl+N" and ";;Strg+N" respectively.
 *
 * Returns: (nullable): the keybinding which can be used to activate
 * this action, or %NULL if there is no keybinding for this action.
 *
 **/
const gchar*
atk_action_get_keybinding (AtkAction *obj,
                           gint      i)
{
  AtkActionIface *iface;

  g_return_val_if_fail (ATK_IS_ACTION (obj), NULL);

  iface = ATK_ACTION_GET_IFACE (obj);

  if (iface->get_keybinding)
    return (iface->get_keybinding) (obj, i);
  else
    return NULL;
}

/**
 * atk_action_set_description:
 * @action: a #GObject instance that implements AtkActionIface
 * @i: the action index corresponding to the action to be performed 
 * @desc: the description to be assigned to this action
 *
 * Sets a description of the specified action of the object.
 *
 * Returns: a gboolean representing if the description was successfully set;
 **/
gboolean
atk_action_set_description (AtkAction   *obj,
                            gint        i,
                            const gchar *desc)
{
  AtkActionIface *iface;

  g_return_val_if_fail (ATK_IS_ACTION (obj), FALSE);

  iface = ATK_ACTION_GET_IFACE (obj);

  if (iface->set_description)
    return (iface->set_description) (obj, i, desc);
  else
    return FALSE;
}
