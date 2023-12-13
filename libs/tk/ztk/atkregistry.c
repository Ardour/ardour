/* ATK - Accessibility Toolkit
 * Copyright 2001 Sun Microsystems Inc.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#include "config.h"

#include "atkregistry.h"
#include "atknoopobjectfactory.h"

/**
 * SECTION:atkregistry
 * @Short_description: An object used to store the GType of the
 * factories used to create an accessible object for an object of a
 * particular GType.
 * @Title:AtkRegistry
 *
 * The AtkRegistry is normally used to create appropriate ATK "peers"
 * for user interface components.  Application developers usually need
 * only interact with the AtkRegistry by associating appropriate ATK
 * implementation classes with GObject classes via the
 * atk_registry_set_factory_type call, passing the appropriate GType
 * for application custom widget classes.
 */

static AtkRegistry *default_registry = NULL;

static void              atk_registry_init           (AtkRegistry      *instance,
                                                      AtkRegistryClass *klass);
static void              atk_registry_finalize       (GObject          *instance);
static void              atk_registry_class_init     (AtkRegistryClass *klass);
static AtkRegistry*      atk_registry_new            (void);

static gpointer parent_class = NULL;

GType
atk_registry_get_type (void)
{
  static GType type = 0;

  if (!type)
    {
      static const GTypeInfo info =
      {
        sizeof (AtkRegistryClass),
        (GBaseInitFunc) NULL,                             /* base_init */
        (GBaseFinalizeFunc) NULL,                         /* base_finalize */
        (GClassInitFunc) atk_registry_class_init,         /* class_init */
        (GClassFinalizeFunc) NULL,                        /* class_finalize */
        NULL,                                             /* class_data */
        sizeof (AtkRegistry),                             /* instance size */
        0,                                                /* n_preallocs */
        (GInstanceInitFunc) atk_registry_init,            /* instance init */
        NULL                                              /* value table */
      };

      type = g_type_register_static (G_TYPE_OBJECT, "AtkRegistry", &info, 0);
    }

  return type;
}

static void
atk_registry_class_init (AtkRegistryClass *klass)
{
  GObjectClass *object_class = (GObjectClass *) klass;

  parent_class = g_type_class_peek_parent (klass);

  object_class->finalize = atk_registry_finalize;
}

#if 0
/*
 * Cannot define a class_finalize function when calling
 * g_type_register_static()
 */
static void
atk_registry_class_finalize (GObjectClass *klass)
{
  g_return_if_fail (ATK_IS_REGISTRY_CLASS (klass));

  g_object_unref (G_OBJECT (default_registry));
}
#endif

static void
atk_registry_init (AtkRegistry *instance, AtkRegistryClass *klass)
{
  instance->factory_type_registry = g_hash_table_new ((GHashFunc) NULL, 
                                                      (GEqualFunc) NULL);
  instance->factory_singleton_cache = g_hash_table_new ((GHashFunc) NULL, 
                                                        (GEqualFunc) NULL);
}

static AtkRegistry *
atk_registry_new (void)
{
  GObject *object;

  object = g_object_new (ATK_TYPE_REGISTRY, NULL);

  g_return_val_if_fail (ATK_IS_REGISTRY (object), NULL);

  return (AtkRegistry *) object;
}

static void
atk_registry_finalize (GObject *object)
{
  AtkRegistry *registry = ATK_REGISTRY (object);

  g_hash_table_destroy (registry->factory_type_registry);
  g_hash_table_destroy (registry->factory_singleton_cache);

  G_OBJECT_CLASS (parent_class)->finalize (object);
}

/**
 * atk_registry_set_factory_type:
 * @registry: the #AtkRegistry in which to register the type association
 * @type: an #AtkObject type 
 * @factory_type: an #AtkObjectFactory type to associate with @type.  Must
 * implement AtkObject appropriate for @type.
 *
 * Associate an #AtkObjectFactory subclass with a #GType. Note:
 * The associated @factory_type will thereafter be responsible for
 * the creation of new #AtkObject implementations for instances
 * appropriate for @type.
 **/
void
atk_registry_set_factory_type (AtkRegistry *registry,
                               GType type,
                               GType factory_type)
{
  GType old_type;
  gpointer value;
  AtkObjectFactory *old_factory;

  g_return_if_fail (ATK_IS_REGISTRY (registry));

  value = g_hash_table_lookup (registry->factory_type_registry, 
                                  (gpointer) type);
  old_type = (GType) value;
  if (old_type && old_type != factory_type)
    {
      g_hash_table_remove (registry->factory_type_registry, 
                           (gpointer) type);
      /*
       * If the old factory was created, notify it that it has
       * been replaced, then free it.
       */
      old_factory = g_hash_table_lookup (registry->factory_singleton_cache, 
                                         (gpointer) old_type);
      if (old_factory)
        {
          atk_object_factory_invalidate (old_factory);
          g_type_free_instance ((GTypeInstance *) old_factory);
        }
    }
  g_hash_table_insert (registry->factory_type_registry, 
                       (gpointer) type, 
                       (gpointer) factory_type);
}

/**
 * atk_registry_get_factory_type:
 * @registry: an #AtkRegistry
 * @type: a #GType with which to look up the associated #AtkObjectFactory
 * subclass
 *
 * Provides a #GType indicating the #AtkObjectFactory subclass
 * associated with @type.
 *
 * Returns: a #GType associated with type @type
 **/
GType
atk_registry_get_factory_type (AtkRegistry *registry,
                               GType type)
{
  GType factory_type;
  gpointer value;

  /*
   * look up factory type in first hash;
   * if there isn't an explicitly registered factory type,
   * try inheriting one...
   */
  do {
    value =
        g_hash_table_lookup (registry->factory_type_registry, 
                             (gpointer) type);
    type = g_type_parent (type);
    if (type == G_TYPE_INVALID)
      {
        break;
      }
  } while (value == NULL);

  factory_type = (GType) value;
  return factory_type;
}

/**
 * atk_registry_get_factory:
 * @registry: an #AtkRegistry
 * @type: a #GType with which to look up the associated #AtkObjectFactory
 *
 * Gets an #AtkObjectFactory appropriate for creating #AtkObjects
 * appropriate for @type.
 *
 * Returns: (transfer none): an #AtkObjectFactory appropriate for creating
 * #AtkObjects appropriate for @type.
 **/
AtkObjectFactory*
atk_registry_get_factory (AtkRegistry *registry,
                          GType type)
{
  gpointer factory_pointer = NULL;
  GType factory_type;

  factory_type = atk_registry_get_factory_type (registry, type);

  if (factory_type == G_TYPE_INVALID)
  {
  /* Factory type has not been specified for this object type */
    static AtkObjectFactory* default_factory = NULL;

    if (!default_factory)
      default_factory = atk_no_op_object_factory_new ();

    return default_factory;
  }

  /* ask second hashtable for instance of factory type */
  factory_pointer =
        g_hash_table_lookup (registry->factory_singleton_cache, 
        (gpointer) factory_type);

  /* if there isn't one already, create one and save it */
  if (factory_pointer == NULL)
    {
      factory_pointer = g_type_create_instance (factory_type);
      g_hash_table_insert (registry->factory_singleton_cache,
                           (gpointer) factory_type,
                           factory_pointer);
    }

  return ATK_OBJECT_FACTORY (factory_pointer);
}

/**
 * atk_get_default_registry:
 *
 * Gets a default implementation of the #AtkObjectFactory/type
 * registry.
 * Note: For most toolkit maintainers, this will be the correct
 * registry for registering new #AtkObject factories. Following
 * a call to this function, maintainers may call atk_registry_set_factory_type()
 * to associate an #AtkObjectFactory subclass with the GType of objects
 * for whom accessibility information will be provided.
 *
 * Returns: (transfer full): a default implementation of the
 * #AtkObjectFactory/type registry
 **/
AtkRegistry*
atk_get_default_registry (void)
{
  if (!default_registry)
    {
      default_registry = atk_registry_new ();
    }
  return default_registry;
}
