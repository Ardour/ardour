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

#include <string.h>
#include <glib-object.h>
#include "atk.h"

/**
 * SECTION:atkrelation
 * @Short_description: An object used to describe a relation between a
 *  object and one or more other objects.
 * @Title:AtkRelation
 *
 * An AtkRelation describes a relation between an object and one or
 * more other objects. The actual relations that an object has with
 * other objects are defined as an AtkRelationSet, which is a set of
 * AtkRelations.
 */
enum {
  PROP_0,

  PROP_RELATION_TYPE,
  PROP_TARGET,
  PROP_LAST
};

static GPtrArray *extra_names = NULL;

static gpointer parent_class = NULL;
  
static void atk_relation_class_init   (AtkRelationClass *klass);
static void atk_relation_finalize     (GObject          *object);
static void atk_relation_set_property (GObject          *object,
                                       guint            prop_id,
                                       const GValue     *value,
                                       GParamSpec       *pspec);
static void atk_relation_get_property (GObject          *object,
                                       guint            prop_id,
                                       GValue           *value,
                                       GParamSpec       *pspec);

static GPtrArray* atk_relation_get_ptr_array_from_value_array (GValueArray *array);
static GValueArray* atk_relation_get_value_array_from_ptr_array (GPtrArray *array);

GType
atk_relation_get_type (void)
{
  static GType type = 0;

  if (!type)
    {
      static const GTypeInfo typeInfo =
      {
        sizeof (AtkRelationClass),
        (GBaseInitFunc) NULL,
        (GBaseFinalizeFunc) NULL,
        (GClassInitFunc) atk_relation_class_init,
        (GClassFinalizeFunc) NULL,
        NULL,
        sizeof (AtkRelation),
        0,
        (GInstanceInitFunc) NULL,
      } ;
      type = g_type_register_static (G_TYPE_OBJECT, "AtkRelation", &typeInfo, 0) ;
    }
  return type;
}

static void
atk_relation_class_init (AtkRelationClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

  parent_class = g_type_class_peek_parent (klass);
  
  gobject_class->finalize = atk_relation_finalize;
  gobject_class->set_property = atk_relation_set_property;
  gobject_class->get_property = atk_relation_get_property;

  g_object_class_install_property (gobject_class,
                                   PROP_RELATION_TYPE,
                                   g_param_spec_enum ("relation_type",
                                                      "Relation Type",
                                                      "The type of the relation",
                                                      ATK_TYPE_RELATION_TYPE,
                                                      ATK_RELATION_NULL,
                                                      G_PARAM_READWRITE));
  g_object_class_install_property (gobject_class,
                                   PROP_TARGET,
                                   g_param_spec_value_array ("target",
                                                             "Target",
                                                             "An array of the targets for the relation",
                                                             NULL,

                                                             G_PARAM_READWRITE));
}

/**
 * atk_relation_type_register:
 * @name: a name string
 *
 * Associate @name with a new #AtkRelationType
 
 * Returns: an #AtkRelationType associated with @name
 **/
AtkRelationType
atk_relation_type_register (const gchar *name)
{
  g_return_val_if_fail (name, ATK_RELATION_NULL);

  if (!extra_names)
    extra_names = g_ptr_array_new ();

  g_ptr_array_add (extra_names, g_strdup (name));
  return extra_names->len + ATK_RELATION_LAST_DEFINED;
}

/**
 * atk_relation_type_get_name:
 * @type: The #AtkRelationType whose name is required
 *
 * Gets the description string describing the #AtkRelationType @type.
 *
 * Returns: the string describing the AtkRelationType
 */
const gchar*
atk_relation_type_get_name (AtkRelationType type)
{
  GTypeClass *type_class;
  GEnumValue *value;
  const gchar *name = NULL;

  type_class = g_type_class_ref (ATK_TYPE_RELATION_TYPE);
  g_return_val_if_fail (G_IS_ENUM_CLASS (type_class), NULL);

  value = g_enum_get_value (G_ENUM_CLASS (type_class), type);

  if (value)
    {
      name = value->value_nick;
    }
  else
    {
      if (extra_names)
        {
          gint n = type;

          n -= ATK_RELATION_LAST_DEFINED + 1;

          if (n < extra_names->len)
            name = g_ptr_array_index (extra_names, n);
        }
    }
  g_type_class_unref (type_class);
  return name;
}

/**
 * atk_relation_type_for_name:
 * @name: a string which is the (non-localized) name of an ATK relation type.
 *
 * Get the #AtkRelationType type corresponding to a relation name.
 *
 * Returns: the #AtkRelationType enumerated type corresponding to the specified name,
 *          or #ATK_RELATION_NULL if no matching relation type is found.
 **/
AtkRelationType
atk_relation_type_for_name (const gchar *name)
{
  GTypeClass *type_class;
  GEnumValue *value;
  AtkRelationType type = ATK_RELATION_NULL;

  g_return_val_if_fail (name, ATK_RELATION_NULL);

  type_class = g_type_class_ref (ATK_TYPE_RELATION_TYPE);
  g_return_val_if_fail (G_IS_ENUM_CLASS (type_class), ATK_RELATION_NULL);

  value = g_enum_get_value_by_nick (G_ENUM_CLASS (type_class), name);

  if (value)
    {
      type = value->value;
    }
  else
    {
      gint i;

      if (extra_names)
        {
          for (i = 0; i < extra_names->len; i++)
            {
              gchar *extra_name = (gchar *)g_ptr_array_index (extra_names, i);

              g_return_val_if_fail (extra_name, ATK_RELATION_NULL);
         
              if (strcmp (name, extra_name) == 0)
                {
                  type = i + 1 + ATK_RELATION_LAST_DEFINED;
                  break;
                }
            }
        }
    }
  g_type_class_unref (type_class);
 
  return type;
}


/**
 * atk_relation_new:
 * @targets: (array length=n_targets): an array of pointers to
 *  #AtkObjects
 * @n_targets: number of #AtkObjects pointed to by @targets
 * @relationship: an #AtkRelationType with which to create the new
 *  #AtkRelation
 *
 * Create a new relation for the specified key and the specified list
 * of targets.  See also atk_object_add_relationship().
 *
 * Returns: a pointer to a new #AtkRelation
 **/
AtkRelation*
atk_relation_new (AtkObject       **targets,
                  gint            n_targets,
                  AtkRelationType relationship)
{
  AtkRelation *relation;
  int         i;
  GValueArray *array;
  GValue      *value;

  g_return_val_if_fail (targets != NULL, NULL);

  array = g_value_array_new (n_targets);
  for (i = 0; i < n_targets; i++)
  {
    value = g_new0 (GValue, 1);
    g_value_init (value, ATK_TYPE_OBJECT);
    g_value_set_object (value, targets[i]);
    array = g_value_array_append (array, value);
    g_value_unset (value);
    g_free (value);
  }
  
  relation =  g_object_new (ATK_TYPE_RELATION, 
                            "relation_type", relationship,
                            "target", array,
                            NULL);

  g_value_array_free (array);

  return relation;
}

/**
 * atk_relation_get_relation_type:
 * @relation: an #AtkRelation 
 *
 * Gets the type of @relation
 *
 * Returns: the type of @relation
 **/
AtkRelationType
atk_relation_get_relation_type (AtkRelation *relation)
{
  g_return_val_if_fail (ATK_IS_RELATION (relation), 0);
  
  return relation->relationship;
}

/**
 * atk_relation_get_target:
 * @relation: an #AtkRelation
 *
 * Gets the target list of @relation
 *
 * Returns: (transfer none) (element-type Atk.Object): the target list of @relation
 **/
GPtrArray*
atk_relation_get_target (AtkRelation *relation)
{
  g_return_val_if_fail (ATK_IS_RELATION (relation), NULL);

  return relation->target;
}

static void
delete_object_while_in_relation (gpointer callback_data,
                                 GObject *where_the_object_was)
{
  GPtrArray *array;

  g_assert (callback_data != NULL);

  array = callback_data;
  g_ptr_array_remove (array, where_the_object_was);
}

/**
 * atk_relation_add_target:
 * @relation: an #AtkRelation
 * @target: an #AtkObject
 *
 * Adds the specified AtkObject to the target for the relation, if it is
 * not already present.  See also atk_object_add_relationship().
 *
 *
 * Since: 1.9
 **/
void
atk_relation_add_target (AtkRelation *relation,
                         AtkObject   *target)
{
  guint i;

  g_return_if_fail (ATK_IS_RELATION (relation));
  g_return_if_fail (ATK_IS_OBJECT (target));

  /* first check if target occurs in array ... */
  for (i = 0; i < relation->target->len; i++)
    if (g_ptr_array_index(relation->target, i) == target)
      return;

  g_ptr_array_add (relation->target, target);
  g_object_weak_ref (G_OBJECT (target), (GWeakNotify) delete_object_while_in_relation, relation->target);
}

/**
 * atk_relation_remove_target:
 * @relation: an #AtkRelation
 * @target: an #AtkObject
 *
 * Remove the specified AtkObject from the target for the relation.
 *
 * Returns: TRUE if the removal is successful.
 **/

gboolean
atk_relation_remove_target (AtkRelation *relation,
                            AtkObject *target)
{
  gboolean ret = FALSE;
  GPtrArray *array;

  array = atk_relation_get_target (relation);

  if (array && g_ptr_array_remove (array, target))
    {
      g_object_weak_unref (G_OBJECT (target),
                           (GWeakNotify) delete_object_while_in_relation,
                           relation->target);
      ret = TRUE;
    }
  return ret;
}

static void
atk_relation_finalize (GObject *object)
{
  AtkRelation        *relation;

  g_return_if_fail (ATK_IS_RELATION (object));

  relation = ATK_RELATION (object);

  if (relation->target)
  {
    gint i;

    for (i = 0; i < relation->target->len; i++)
    {
      g_object_weak_unref (G_OBJECT (g_ptr_array_index (relation->target, i)),
                           (GWeakNotify) delete_object_while_in_relation, 
                           relation->target);
    }
    g_ptr_array_free (relation->target, TRUE);
  } 

  G_OBJECT_CLASS (parent_class)->finalize (object);
}

static void 
atk_relation_set_property (GObject       *object,
                           guint         prop_id,
                           const GValue  *value,
                           GParamSpec    *pspec)
{
  AtkRelation *relation;
  gpointer boxed;

  relation = ATK_RELATION (object);

  switch (prop_id)
    {
    case PROP_RELATION_TYPE:
      relation->relationship = g_value_get_enum (value);
      break; 
    case PROP_TARGET:
      if (relation->target)
      {
        gint i;

        for (i = 0; i < relation->target->len; i++)
        {
          g_object_weak_unref (G_OBJECT (g_ptr_array_index (relation->target, i)),
                               (GWeakNotify) delete_object_while_in_relation,
                               relation->target);
        }
        g_ptr_array_free (relation->target, TRUE);
      }
      boxed = g_value_get_boxed (value);
      relation->target = atk_relation_get_ptr_array_from_value_array ( (GValueArray *) boxed);
      break; 
    default:
      break;
    }  
}

static void
atk_relation_get_property (GObject    *object,
                           guint      prop_id,
                           GValue     *value,
                           GParamSpec *pspec)
{
  AtkRelation *relation;
  GValueArray *array;

  relation = ATK_RELATION (object);

  switch (prop_id)
    {
    case PROP_RELATION_TYPE:
      g_value_set_enum (value, relation->relationship);
      break;
    case PROP_TARGET:
      array = atk_relation_get_value_array_from_ptr_array (relation->target);
      g_value_set_boxed (value, array);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }  
}

static GPtrArray*
atk_relation_get_ptr_array_from_value_array (GValueArray *array)
{
  gint i;
  GPtrArray *return_array;
  GValue *value;
  GObject *obj;

  return_array = g_ptr_array_sized_new (array->n_values);
  for (i = 0; i < array->n_values; i++)
    {
      value = g_value_array_get_nth (array, i);
      obj = g_value_get_object (value);
      g_ptr_array_add (return_array, obj);
      g_object_weak_ref (obj, (GWeakNotify) delete_object_while_in_relation, return_array);
    }
      
  return return_array;
}

static GValueArray*
atk_relation_get_value_array_from_ptr_array (GPtrArray *array)
{
  int         i;
  GValueArray *return_array;
  GValue      *value;

  return_array = g_value_array_new (array->len);
  for (i = 0; i < array->len; i++)
    {
      value = g_new0 (GValue, 1);
      g_value_init (value, ATK_TYPE_OBJECT);
      g_value_set_object (value, g_ptr_array_index (array, i));
      return_array = g_value_array_append (return_array, value);
    }
  return return_array;
}
