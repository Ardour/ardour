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

#include "atk.h"

/**
 * SECTION:atkrelationset
 * @Short_description: A set of AtkRelations, normally the set of
 *  AtkRelations which an AtkObject has.
 * @Title:AtkRelationSet
 *
 * The AtkRelationSet held by an object establishes its relationships
 * with objects beyond the normal "parent/child" hierarchical
 * relationships that all user interface objects have.
 * AtkRelationSets establish whether objects are labelled or
 * controlled by other components, share group membership with other
 * components (for instance within a radio-button group), or share
 * content which "flows" between them, among other types of possible
 * relationships.
 */

static gpointer parent_class = NULL;

static void atk_relation_set_class_init (AtkRelationSetClass  *klass);
static void atk_relation_set_finalize   (GObject              *object);

GType
atk_relation_set_get_type (void)
{
  static GType type = 0;

  if (!type)
    {
      static const GTypeInfo typeInfo =
      {
        sizeof (AtkRelationSetClass),
        (GBaseInitFunc) NULL,
        (GBaseFinalizeFunc) NULL,
        (GClassInitFunc) atk_relation_set_class_init,
        (GClassFinalizeFunc) NULL,
        NULL,
        sizeof (AtkRelationSet),
        0,
        (GInstanceInitFunc) NULL,
      } ;
      type = g_type_register_static (G_TYPE_OBJECT, "AtkRelationSet", &typeInfo, 0) ;
    }
  return type;
}

static void
atk_relation_set_class_init (AtkRelationSetClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

  parent_class = g_type_class_peek_parent (klass);

  gobject_class->finalize = atk_relation_set_finalize;
}

/**
 * atk_relation_set_new:
 * 
 * Creates a new empty relation set.
 * 
 * Returns: a new #AtkRelationSet 
 **/
AtkRelationSet*
atk_relation_set_new (void)
{
  AtkRelationSet *relation_set;

  relation_set = g_object_new (ATK_TYPE_RELATION_SET, NULL);
  return relation_set;
}

/**
 * atk_relation_set_contains:
 * @set: an #AtkRelationSet
 * @relationship: an #AtkRelationType
 *
 * Determines whether the relation set contains a relation that matches the
 * specified type.
 *
 * Returns: %TRUE if @relationship is the relationship type of a relation
 * in @set, %FALSE otherwise
 **/
gboolean
atk_relation_set_contains (AtkRelationSet   *set,
                           AtkRelationType  relationship)
{
  GPtrArray *array_item;
  AtkRelation *item;
  gint  i;

  g_return_val_if_fail (ATK_IS_RELATION_SET (set), FALSE);

  array_item = set->relations;
  if (array_item == NULL)
    return FALSE;
  for (i = 0; i < array_item->len; i++)
  {
    item = g_ptr_array_index (array_item, i);
    if (item->relationship == relationship)
      return TRUE;
  }
  return FALSE;
}

/**
 * atk_relation_set_remove:
 * @set: an #AtkRelationSet
 * @relation: an #AtkRelation
 *
 * Removes a relation from the relation set.
 * This function unref's the #AtkRelation so it will be deleted unless there
 * is another reference to it.
 **/
void
atk_relation_set_remove (AtkRelationSet *set,
                         AtkRelation    *relation)
{
  GPtrArray *array_item;
  AtkRelationType relationship;

  g_return_if_fail (ATK_IS_RELATION_SET (set));

  array_item = set->relations;
  if (array_item == NULL)
    return;

  if (g_ptr_array_remove (array_item, relation))
  {
    g_object_unref (relation);
  }
  else
  {
    relationship = atk_relation_get_relation_type (relation);
    if (atk_relation_set_contains (set, relationship))
    {
      AtkRelation *exist_relation;
      gint i;
      exist_relation = atk_relation_set_get_relation_by_type (set, relationship);
      for (i = 0; i < relation->target->len; i++)
      {
        AtkObject *target = g_ptr_array_index(relation->target, i);
        atk_relation_remove_target (exist_relation, target);
      }
    }
  }
}

/**
 * atk_relation_set_add:
 * @set: an #AtkRelationSet
 * @relation: an #AtkRelation
 *
 * Add a new relation to the current relation set if it is not already
 * present.
 * This function ref's the AtkRelation so the caller of this function
 * should unref it to ensure that it will be destroyed when the AtkRelationSet
 * is destroyed.
 **/
void
atk_relation_set_add (AtkRelationSet *set,
                      AtkRelation    *relation)
{
  AtkRelationType relationship;

  g_return_if_fail (ATK_IS_RELATION_SET (set));
  g_return_if_fail (relation != NULL);

  if (set->relations == NULL)
  {
    set->relations = g_ptr_array_new ();
  }

  relationship = atk_relation_get_relation_type (relation);
  if (!atk_relation_set_contains (set, relationship))
  {
    g_ptr_array_add (set->relations, relation);
    g_object_ref (relation);
  }
  else
  {
    AtkRelation *exist_relation;
    gint i;
    exist_relation = atk_relation_set_get_relation_by_type (set, relationship);
    for (i = 0; i < relation->target->len; i++)
    {
      AtkObject *target = g_ptr_array_index(relation->target, i);
      atk_relation_add_target (exist_relation, target); 
    }
  }
}

/**
 * atk_relation_set_get_n_relations:
 * @set: an #AtkRelationSet
 *
 * Determines the number of relations in a relation set.
 *
 * Returns: an integer representing the number of relations in the set.
 **/
gint
atk_relation_set_get_n_relations (AtkRelationSet *set)
{
  g_return_val_if_fail (ATK_IS_RELATION_SET (set), 0);

  if (set->relations == NULL)
    return 0;

  return set->relations->len;
}

/**
 * atk_relation_set_get_relation:
 * @set: an #AtkRelationSet
 * @i: a gint representing a position in the set, starting from 0.
 *
 * Determines the relation at the specified position in the relation set.
 *
 * Returns: (transfer none): a #AtkRelation, which is the relation at
 * position i in the set.
 **/
AtkRelation*
atk_relation_set_get_relation (AtkRelationSet *set,
                               gint           i)
{
  GPtrArray *array_item;
  AtkRelation* item;

  g_return_val_if_fail (ATK_IS_RELATION_SET (set), NULL);
  g_return_val_if_fail (i >= 0, NULL);

  array_item = set->relations;
  if (array_item == NULL)
    return NULL;
  item = g_ptr_array_index (array_item, i);
  if (item == NULL)
    return NULL;

  return item;
}

/**
 * atk_relation_set_get_relation_by_type:
 * @set: an #AtkRelationSet
 * @relationship: an #AtkRelationType
 *
 * Finds a relation that matches the specified type.
 *
 * Returns: (transfer none): an #AtkRelation, which is a relation matching the
 * specified type.
 **/
AtkRelation*
atk_relation_set_get_relation_by_type (AtkRelationSet  *set,
                                       AtkRelationType relationship)
{
  GPtrArray *array_item;
  AtkRelation *item;
  gint i;

  g_return_val_if_fail (ATK_IS_RELATION_SET (set), NULL);

  array_item = set->relations;
  if (array_item == NULL)
    return NULL;
  for (i = 0; i < array_item->len; i++)
  {
    item = g_ptr_array_index (array_item, i);
    if (item->relationship == relationship)
      return item;
  }
  return NULL;
}

static void
atk_relation_set_finalize (GObject *object)
{
  AtkRelationSet     *relation_set;
  GPtrArray             *array;
  gint               i;

  g_return_if_fail (ATK_IS_RELATION_SET (object));

  relation_set = ATK_RELATION_SET (object);
  array = relation_set->relations;

  if (array)
  {
    for (i = 0; i < array->len; i++)
    {
      g_object_unref (g_ptr_array_index (array, i));
    }
    g_ptr_array_free (array, TRUE);
  }

  G_OBJECT_CLASS (parent_class)->finalize (object);
}

/**
 * atk_relation_set_add_relation_by_type:
 * @set: an #AtkRelationSet
 * @relationship: an #AtkRelationType
 * @target: an #AtkObject
 *
 * Add a new relation of the specified type with the specified target to 
 * the current relation set if the relation set does not contain a relation
 * of that type. If it is does contain a relation of that typea the target
 * is added to the relation.
 *
 * Since: 1.9
 **/
void
atk_relation_set_add_relation_by_type (AtkRelationSet  *set,
                                       AtkRelationType relationship,
                                       AtkObject       *target)
{
  AtkRelation *relation;

  g_return_if_fail (ATK_IS_RELATION_SET (set));
  g_return_if_fail (ATK_IS_OBJECT (target));

  relation = atk_relation_set_get_relation_by_type (set,
                                                    relationship);
  if (relation)
    {
      atk_relation_add_target (relation, target);
    } 
  else 
    {
      /* the relation hasn't been created yet ... */
      relation = atk_relation_new (&target, 1, relationship);
      atk_relation_set_add (set, relation);
      g_object_unref(relation);
    }
}

/**
 * atk_relation_set_contains_target:
 * @set: an #AtkRelationSet
 * @relationship: an #AtkRelationType
 * @target: an #AtkObject
 *
 * Determines whether the relation set contains a relation that
 * matches the specified pair formed by type @relationship and object
 * @target.
 *
 * Returns: %TRUE if @set contains a relation with the relationship
 * type @relationship with an object @target, %FALSE otherwise
 **/

gboolean
atk_relation_set_contains_target (AtkRelationSet  *set,
                                  AtkRelationType relationship,
                                  AtkObject       *target)
{
  GPtrArray *array_relations;
  GPtrArray *array_target;
  AtkObject *current_target;
  AtkRelation *relation;
  gint i;
  gint c;

  g_return_val_if_fail (ATK_IS_RELATION_SET (set), FALSE);
  g_return_val_if_fail (ATK_IS_OBJECT (target), FALSE);

  array_relations = set->relations;
  if (array_relations == NULL)
    return FALSE;

  for (i = 0; i < array_relations->len; i++)
  {
    relation = g_ptr_array_index (array_relations, i);
    if (relation->relationship == relationship)
      {
        array_target = atk_relation_get_target (relation);
        for (c = 0; c < array_target->len; c++)
          {
            current_target = g_ptr_array_index (array_target, c);
            if (target == current_target)
              return TRUE;
          }
      }
  }

  return FALSE;
}
