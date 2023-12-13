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

#include <glib-object.h>

#include "atkobject.h"
#include "atkstateset.h"

/**
 * SECTION:atkstateset
 * @Short_description: An AtkStateSet determines a component's state set.
 * @Title:AtkStateSet
 *
 * An AtkStateSet determines a component's state set. It is composed
 * of a set of AtkStates.
 */

#define ATK_STATE(state_enum)             ((AtkState)((guint64)1 << ((state_enum)%64)))

struct _AtkRealStateSet
{
  GObject parent;

  AtkState state;
};

typedef struct _AtkRealStateSet      AtkRealStateSet;

static void            atk_state_set_class_init       (AtkStateSetClass  *klass);

GType
atk_state_set_get_type (void)
{
  static GType type = 0;

  if (!type)
    {
      static const GTypeInfo typeInfo =
      {
        sizeof (AtkStateSetClass),
        (GBaseInitFunc) NULL,
        (GBaseFinalizeFunc) NULL,
        (GClassInitFunc) atk_state_set_class_init,
        (GClassFinalizeFunc) NULL,
        NULL,
        sizeof (AtkRealStateSet),
        0,
        (GInstanceInitFunc) NULL,
      } ;
      type = g_type_register_static (G_TYPE_OBJECT, "AtkStateSet", &typeInfo, 0) ;
    }
  return type;
}

static void
atk_state_set_class_init (AtkStateSetClass *klass)
{
}

/**
 * atk_state_set_new:
 * 
 * Creates a new empty state set.
 * 
 * Returns: a new #AtkStateSet 
 **/
AtkStateSet*
atk_state_set_new (void)
{
  return (AtkStateSet*) g_object_new (ATK_TYPE_STATE_SET, NULL);
}

/**
 * atk_state_set_is_empty:
 * @set: an #AtkStateType
 *
 * Checks whether the state set is empty, i.e. has no states set.
 *
 * Returns: %TRUE if @set has no states set, otherwise %FALSE
 **/
gboolean
atk_state_set_is_empty (AtkStateSet   *set)
{
  AtkRealStateSet *real_set;
  g_return_val_if_fail (ATK_IS_STATE_SET (set), FALSE);

  real_set = (AtkRealStateSet *)set;

  if (real_set->state)
    return FALSE;
  else
    return TRUE;
}

/**
 * atk_state_set_add_state:
 * @set: an #AtkStateSet
 * @type: an #AtkStateType
 *
 * Add a new state for the specified type to the current state set if
 * it is not already present.
 *
 * Returns: %TRUE if  the state for @type is not already in @set.
 **/
gboolean
atk_state_set_add_state (AtkStateSet   *set,
                         AtkStateType  type)
{
  AtkRealStateSet *real_set;
  g_return_val_if_fail (ATK_IS_STATE_SET (set), FALSE);

  real_set = (AtkRealStateSet *)set;

  if (real_set->state & ATK_STATE (type))
    return FALSE;
  else
  {
    real_set->state |= ATK_STATE (type);
    return TRUE;
  }
}
/**
 * atk_state_set_add_states:
 * @set: an #AtkStateSet
 * @types: (array length=n_types): an array of #AtkStateType
 * @n_types: The number of elements in the array
 *
 * Add the states for the specified types to the current state set.
 **/
void
atk_state_set_add_states (AtkStateSet   *set,
                          AtkStateType  *types,
                          gint          n_types)
{
  AtkRealStateSet *real_set;
  gint     i;
  g_return_if_fail (ATK_IS_STATE_SET (set));

  real_set = (AtkRealStateSet *)set;

  for (i = 0; i < n_types; i++)
  {
    real_set->state |= ATK_STATE (types[i]);
  }
}

/**
 * atk_state_set_clear_states:
 * @set: an #AtkStateSet
 *
 * Removes all states from the state set.
 **/
void
atk_state_set_clear_states (AtkStateSet   *set)
{
  AtkRealStateSet *real_set;
  g_return_if_fail (ATK_IS_STATE_SET (set));

  real_set = (AtkRealStateSet *)set;

  real_set->state = 0;
}

/**
 * atk_state_set_contains_state:
 * @set: an #AtkStateSet
 * @type: an #AtkStateType
 *
 * Checks whether the state for the specified type is in the specified set.
 *
 * Returns: %TRUE if @type is the state type is in @set.
 **/
gboolean
atk_state_set_contains_state (AtkStateSet   *set,
                              AtkStateType  type)
{
  AtkRealStateSet *real_set;
  g_return_val_if_fail (ATK_IS_STATE_SET (set), FALSE);

  real_set = (AtkRealStateSet *)set;

  if (real_set->state & ATK_STATE (type))
    return TRUE;
  else
    return FALSE;
}

/**
 * atk_state_set_contains_states:
 * @set: an #AtkStateSet
 * @types: (array length=n_types): an array of #AtkStateType
 * @n_types: The number of elements in the array
 *
 * Checks whether the states for all the specified types are in the 
 * specified set.
 *
 * Returns: %TRUE if all the states for @type are in @set.
 **/
gboolean
atk_state_set_contains_states (AtkStateSet   *set,
                               AtkStateType  *types,
                               gint          n_types)
{
  AtkRealStateSet *real_set;
  gint i;
  g_return_val_if_fail (ATK_IS_STATE_SET (set), FALSE);

  real_set = (AtkRealStateSet *)set;

  for (i = 0; i < n_types; i++)
  {
    if (!(real_set->state & ATK_STATE (types[i])))
      return FALSE;
  }
  return TRUE;
}

/**
 * atk_state_set_remove_state:
 * @set: an #AtkStateSet
 * @type: an #AtkType
 *
 * Removes the state for the specified type from the state set.
 *
 * Returns: %TRUE if @type was the state type is in @set.
 **/
gboolean
atk_state_set_remove_state (AtkStateSet  *set,
                            AtkStateType type)
{
  AtkRealStateSet *real_set;
  g_return_val_if_fail (ATK_IS_STATE_SET (set), FALSE);

  real_set = (AtkRealStateSet *)set;

  if (real_set->state & ATK_STATE (type))
  {
    real_set->state ^= ATK_STATE (type);
    return TRUE;
  }
  else
    return FALSE;
}

/**
 * atk_state_set_and_sets:
 * @set: an #AtkStateSet
 * @compare_set: another #AtkStateSet
 *
 * Constructs the intersection of the two sets, returning %NULL if the
 * intersection is empty.
 *
 * Returns: (transfer full): a new #AtkStateSet which is the intersection of
 * the two sets.
 **/
AtkStateSet*
atk_state_set_and_sets (AtkStateSet  *set,
                        AtkStateSet  *compare_set)
{
  AtkRealStateSet *real_set, *real_compare_set;
  AtkStateSet *return_set = NULL;
  AtkState state;

  g_return_val_if_fail (ATK_IS_STATE_SET (set), NULL);
  g_return_val_if_fail (ATK_IS_STATE_SET (compare_set), NULL);

  real_set = (AtkRealStateSet *)set;
  real_compare_set = (AtkRealStateSet *)compare_set;

  state = real_set->state & real_compare_set->state;
  if (state)
  {
    return_set = atk_state_set_new();
    ((AtkRealStateSet *) return_set)->state = state;
  }
  return return_set;
}

/**
 * atk_state_set_or_sets:
 * @set: an #AtkStateSet
 * @compare_set: another #AtkStateSet
 *
 * Constructs the union of the two sets.
 *
 * Returns: (nullable) (transfer full): a new #AtkStateSet which is
 * the union of the two sets, returning %NULL is empty.
 **/
AtkStateSet*
atk_state_set_or_sets (AtkStateSet  *set,
                       AtkStateSet  *compare_set)
{
  AtkRealStateSet *real_set, *real_compare_set;
  AtkStateSet *return_set = NULL;
  AtkState state;

  g_return_val_if_fail (ATK_IS_STATE_SET (set), NULL);
  g_return_val_if_fail (ATK_IS_STATE_SET (compare_set), NULL);

  real_set = (AtkRealStateSet *)set;
  real_compare_set = (AtkRealStateSet *)compare_set;

  state = real_set->state | real_compare_set->state;

  if (state)
  {
    return_set = atk_state_set_new();
    ((AtkRealStateSet *) return_set)->state = state;
  }

  return return_set;
}

/**
 * atk_state_set_xor_sets:
 * @set: an #AtkStateSet
 * @compare_set: another #AtkStateSet
 *
 * Constructs the exclusive-or of the two sets, returning %NULL is empty.
 * The set returned by this operation contains the states in exactly
 * one of the two sets.
 *
 * Returns: (transfer full): a new #AtkStateSet which contains the states
 * which are in exactly one of the two sets.
 **/
AtkStateSet*
atk_state_set_xor_sets (AtkStateSet  *set,
                        AtkStateSet  *compare_set)
{
  AtkRealStateSet *real_set, *real_compare_set;
  AtkStateSet *return_set = NULL;
  AtkState state, state1, state2;

  g_return_val_if_fail (ATK_IS_STATE_SET (set), NULL);
  g_return_val_if_fail (ATK_IS_STATE_SET (compare_set), NULL);

  real_set = (AtkRealStateSet *)set;
  real_compare_set = (AtkRealStateSet *)compare_set;

  state1 = real_set->state & (~real_compare_set->state);
  state2 = (~real_set->state) & real_compare_set->state;
  state = state1 | state2;

  if (state)
  {
    return_set = atk_state_set_new();
    ((AtkRealStateSet *) return_set)->state = state;
  }
  return return_set;
}
