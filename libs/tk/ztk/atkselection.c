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

#include "atkselection.h"

/**
 * SECTION:atkselection
 * @Short_description: The ATK interface implemented by container
 *  objects whose #AtkObject children can be selected.
 * @Title:AtkSelection
 *
 * #AtkSelection should be implemented by UI components with children
 * which are exposed by #atk_object_ref_child and
 * #atk_object_get_n_children, if the use of the parent UI component
 * ordinarily involves selection of one or more of the objects
 * corresponding to those #AtkObject children - for example,
 * selectable lists.
 *
 * Note that other types of "selection" (for instance text selection)
 * are accomplished a other ATK interfaces - #AtkSelection is limited
 * to the selection/deselection of children.
 */


enum {
  SELECTION_CHANGED,
  LAST_SIGNAL
};

static void atk_selection_base_init (gpointer *g_class);

static guint atk_selection_signals[LAST_SIGNAL] = { 0 };

GType
atk_selection_get_type (void)
{
  static GType type = 0;

  if (!type) {
    GTypeInfo tinfo =
    {
      sizeof (AtkSelectionIface),
      (GBaseInitFunc)atk_selection_base_init,
      (GBaseFinalizeFunc) NULL,

    };

    type = g_type_register_static (G_TYPE_INTERFACE, "AtkSelection", &tinfo, 0);
  }

  return type;
}

static void
atk_selection_base_init (gpointer *g_class)
{
  static gboolean initialized = FALSE;

  if (! initialized)
    {
      /**
       * AtkSelection::selection-changed:
       * @atkselection: the object which received the signal.
       *
       * The "selection-changed" signal is emitted by an object which
       * implements AtkSelection interface when the selection changes.
       */
      atk_selection_signals[SELECTION_CHANGED] =
        g_signal_new ("selection_changed",
                      ATK_TYPE_SELECTION,
                      G_SIGNAL_RUN_LAST,
                      G_STRUCT_OFFSET (AtkSelectionIface, selection_changed),
                      (GSignalAccumulator) NULL, NULL,
                      g_cclosure_marshal_VOID__VOID,
                      G_TYPE_NONE, 0);


      initialized = TRUE;
    }
}

/**
 * atk_selection_add_selection:
 * @selection: a #GObject instance that implements AtkSelectionIface
 * @i: a #gint specifying the child index.
 *
 * Adds the specified accessible child of the object to the
 * object's selection.
 *
 * Returns: TRUE if success, FALSE otherwise.
 **/
gboolean
atk_selection_add_selection (AtkSelection *obj,
                             gint         i)
{
  AtkSelectionIface *iface;

  g_return_val_if_fail (ATK_IS_SELECTION (obj), FALSE);

  iface = ATK_SELECTION_GET_IFACE (obj);

  if (iface->add_selection)
    return (iface->add_selection) (obj, i);
  else
    return FALSE;
}

/**
 * atk_selection_clear_selection:
 * @selection: a #GObject instance that implements AtkSelectionIface
 *
 * Clears the selection in the object so that no children in the object
 * are selected.
 *
 * Returns: TRUE if success, FALSE otherwise.
 **/
gboolean
atk_selection_clear_selection (AtkSelection *obj)
{
  AtkSelectionIface *iface;

  g_return_val_if_fail (ATK_IS_SELECTION (obj), FALSE);

  iface = ATK_SELECTION_GET_IFACE (obj);

  if (iface->clear_selection)
    return (iface->clear_selection) (obj);
  else
    return FALSE;
}

/**
 * atk_selection_ref_selection:
 * @selection: a #GObject instance that implements AtkSelectionIface
 * @i: a #gint specifying the index in the selection set.  (e.g. the
 * ith selection as opposed to the ith child).
 *
 * Gets a reference to the accessible object representing the specified 
 * selected child of the object.
 * Note: callers should not rely on %NULL or on a zero value for
 * indication of whether AtkSelectionIface is implemented, they should
 * use type checking/interface checking macros or the
 * atk_get_accessible_value() convenience method.
 *
 * Returns: (nullable) (transfer full): an #AtkObject representing the
 * selected accessible, or %NULL if @selection does not implement this
 * interface.
 **/
AtkObject*
atk_selection_ref_selection (AtkSelection *obj,
                             gint         i)
{
  AtkSelectionIface *iface;

  g_return_val_if_fail (ATK_IS_SELECTION (obj), NULL);

  iface = ATK_SELECTION_GET_IFACE (obj);

  if (iface->ref_selection)
    return (iface->ref_selection) (obj, i);
  else
    return NULL;
}

/**
 * atk_selection_get_selection_count:
 * @selection: a #GObject instance that implements AtkSelectionIface
 *
 * Gets the number of accessible children currently selected.
 * Note: callers should not rely on %NULL or on a zero value for
 * indication of whether AtkSelectionIface is implemented, they should
 * use type checking/interface checking macros or the
 * atk_get_accessible_value() convenience method.
 *
 * Returns: a gint representing the number of items selected, or 0
 * if @selection does not implement this interface.
 **/
gint
atk_selection_get_selection_count (AtkSelection *obj)
{
  AtkSelectionIface *iface;

  g_return_val_if_fail (ATK_IS_SELECTION (obj), 0);

  iface = ATK_SELECTION_GET_IFACE (obj);

  if (iface->get_selection_count)
    return (iface->get_selection_count) (obj);
  else
    return 0;
}

/**
 * atk_selection_is_child_selected:
 * @selection: a #GObject instance that implements AtkSelectionIface
 * @i: a #gint specifying the child index.
 *
 * Determines if the current child of this object is selected
 * Note: callers should not rely on %NULL or on a zero value for
 * indication of whether AtkSelectionIface is implemented, they should
 * use type checking/interface checking macros or the
 * atk_get_accessible_value() convenience method.
 *
 * Returns: a gboolean representing the specified child is selected, or 0
 * if @selection does not implement this interface.
 **/
gboolean
atk_selection_is_child_selected (AtkSelection *obj,
                                 gint         i)
{
  AtkSelectionIface *iface;

  g_return_val_if_fail (ATK_IS_SELECTION (obj), FALSE);

  iface = ATK_SELECTION_GET_IFACE (obj);

  if (iface->is_child_selected)
    return (iface->is_child_selected) (obj, i);
  else
    return FALSE;
}

/**
 * atk_selection_remove_selection:
 * @selection: a #GObject instance that implements AtkSelectionIface
 * @i: a #gint specifying the index in the selection set.  (e.g. the
 * ith selection as opposed to the ith child).
 *
 * Removes the specified child of the object from the object's selection.
 *
 * Returns: TRUE if success, FALSE otherwise.
 **/
gboolean
atk_selection_remove_selection (AtkSelection *obj,
                                gint         i)
{
  AtkSelectionIface *iface;

  g_return_val_if_fail (ATK_IS_SELECTION (obj), FALSE);

  iface = ATK_SELECTION_GET_IFACE (obj);

  if (iface->remove_selection)
    return (iface->remove_selection) (obj, i);
  else
    return FALSE;
}

/**
 * atk_selection_select_all_selection:
 * @selection: a #GObject instance that implements AtkSelectionIface
 *
 * Causes every child of the object to be selected if the object
 * supports multiple selections.
 *
 * Returns: TRUE if success, FALSE otherwise.
 **/
gboolean
atk_selection_select_all_selection (AtkSelection *obj)
{
  AtkSelectionIface *iface;

  g_return_val_if_fail (ATK_IS_SELECTION (obj), FALSE);

  iface = ATK_SELECTION_GET_IFACE (obj);

  if (iface->select_all_selection)
    return (iface->select_all_selection) (obj);
  else
    return FALSE;
}
