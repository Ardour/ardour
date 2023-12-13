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

#include "atk.h"

#include <string.h>

/**
 * SECTION:atkstate
 * @Short_description: An AtkState describes a component's particular state.
 * @Title:AtkState
 *
 * An AtkState describes a component's particular state. The actual
 * state of an component is described by its AtkStateSet, which is a
 * set of AtkStates.
 */

static guint last_type = ATK_STATE_LAST_DEFINED;

#define NUM_POSSIBLE_STATES               (sizeof(AtkState)*8)

static gchar* state_names[NUM_POSSIBLE_STATES];

/**
 * atk_state_type_register:
 * @name: a character string describing the new state.
 *
 * Register a new object state.
 *
 * Returns: an #AtkState value for the new state.
 **/
AtkStateType
atk_state_type_register (const gchar *name)
{
  g_return_val_if_fail (name, ATK_STATE_INVALID);

  if (last_type < NUM_POSSIBLE_STATES -1)
    {
      state_names[++last_type] = g_strdup (name); 
      return (last_type);
    }
  return ATK_STATE_INVALID; /* caller needs to check */
}

/**
 * atk_state_type_get_name:
 * @type: The #AtkStateType whose name is required
 *
 * Gets the description string describing the #AtkStateType @type.
 *
 * Returns: the string describing the AtkStateType
 */
const gchar*
atk_state_type_get_name (AtkStateType type)
{
  GTypeClass *type_class;
  GEnumValue *value;
  const gchar *name = NULL;

  type_class = g_type_class_ref (ATK_TYPE_STATE_TYPE);
  g_return_val_if_fail (G_IS_ENUM_CLASS (type_class), NULL);

  value = g_enum_get_value (G_ENUM_CLASS (type_class), type);

  if (value)
    {
      name = value->value_nick;
    }
  else
    {
      if (type <= last_type)
        {
          if (type >= 0)
            name = state_names[type];
        }
    }

  return name;
}

/**
 * atk_state_type_for_name:
 * @name: a character string state name
 *
 * Gets the #AtkStateType corresponding to the description string @name.
 *
 * Returns: an #AtkStateType corresponding to @name 
 */
AtkStateType
atk_state_type_for_name (const gchar *name)
{
  GTypeClass *type_class;
  GEnumValue *value;
  AtkStateType type = ATK_STATE_INVALID;

  g_return_val_if_fail (name, ATK_STATE_INVALID);

  type_class = g_type_class_ref (ATK_TYPE_STATE_TYPE);
  g_return_val_if_fail (G_IS_ENUM_CLASS (type_class), ATK_STATE_INVALID);

  value = g_enum_get_value_by_nick (G_ENUM_CLASS (type_class), name);

  if (value)
    {
      type = value->value;
    }
  else
    {
      gint i;

      for (i = ATK_STATE_LAST_DEFINED + 1; i <= last_type; i++)
        {
          if (state_names[i] == NULL)
            continue; 
          if (!strcmp(name, state_names[i])) 
            {
              type = i;
              break;
            }
        }
    }
  return type;
}
