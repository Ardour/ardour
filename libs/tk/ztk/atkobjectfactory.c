/* ATK - Accessibility Toolkit
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

#include "atkobjectfactory.h"
#include "atknoopobjectfactory.h"

/**
 * SECTION:atkobjectfactory
 * @Short_description: The base object class for a factory used to
 *  create accessible objects for objects of a specific GType.
 * @Title:AtkObjectFactory
 *
 * This class is the base object class for a factory used to create an
 * accessible object for a specific GType. The function
 * atk_registry_set_factory_type() is normally called to store in the
 * registry the factory type to be used to create an accessible of a
 * particular GType.
 */

static void atk_object_factory_class_init   (AtkObjectFactoryClass        *klass);

static gpointer    parent_class = NULL;

GType
atk_object_factory_get_type (void)
{
  static GType type = 0;

  if (!type) {
    GTypeInfo tinfo =
    {
      sizeof (AtkObjectFactoryClass),
      (GBaseInitFunc) NULL, /* base init */
      (GBaseFinalizeFunc) NULL, /* base finalize */
      (GClassInitFunc) atk_object_factory_class_init, /* class init */
      (GClassFinalizeFunc) NULL, /* class finalize */
      NULL, /* class data */
      sizeof (AtkObjectFactory), /* instance size */
      0, /* nb preallocs */
      (GInstanceInitFunc) NULL, /* instance init */
      NULL /* value table */
    };

    type = g_type_register_static (G_TYPE_OBJECT, "AtkObjectFactory", &tinfo, 0);
  }
  return type;
}

static void 
atk_object_factory_class_init (AtkObjectFactoryClass *klass)
{
  parent_class = g_type_class_peek_parent (klass);

}

/**
 * atk_object_factory_create_accessible:
 * @factory: The #AtkObjectFactory associated with @obj's
 * object type
 * @obj: a #GObject 
 * 
 * Provides an #AtkObject that implements an accessibility interface 
 * on behalf of @obj
 *
 * Returns: (transfer full): an #AtkObject that implements an accessibility
 * interface on behalf of @obj
 **/
AtkObject* 
atk_object_factory_create_accessible (AtkObjectFactory *factory,
                                      GObject          *obj)
{
  AtkObjectFactoryClass *klass;
  AtkObject *accessible = NULL;

  g_return_val_if_fail (ATK_IS_OBJECT_FACTORY (factory), NULL);
  g_return_val_if_fail (G_IS_OBJECT (obj), NULL);

  klass = ATK_OBJECT_FACTORY_GET_CLASS (factory);

  if (klass->create_accessible)
  {
      accessible = klass->create_accessible (obj);
  }
  return accessible;
} 

/**
 * atk_object_factory_invalidate:
 * @factory: an #AtkObjectFactory to invalidate
 *
 * Inform @factory that it is no longer being used to create
 * accessibles. When called, @factory may need to inform
 * #AtkObjects which it has created that they need to be re-instantiated.
 * Note: primarily used for runtime replacement of #AtkObjectFactorys
 * in object registries.
 **/
void 
atk_object_factory_invalidate (AtkObjectFactory *factory)
{
  AtkObjectFactoryClass *klass;

  g_return_if_fail (ATK_OBJECT_FACTORY (factory));

  klass = ATK_OBJECT_FACTORY_GET_CLASS (factory);
  if (klass->invalidate)
     (klass->invalidate) (factory);
}

/**
 * atk_object_factory_get_accessible_type:
 * @factory: an #AtkObjectFactory 
 *
 * Gets the GType of the accessible which is created by the factory. 
 * Returns: the type of the accessible which is created by the @factory.
 * The value G_TYPE_INVALID is returned if no type if found.
 **/
GType
atk_object_factory_get_accessible_type (AtkObjectFactory *factory)
{
  AtkObjectFactoryClass *klass;

  g_return_val_if_fail (ATK_OBJECT_FACTORY (factory), G_TYPE_INVALID);

  klass = ATK_OBJECT_FACTORY_GET_CLASS (factory);
  if (klass->get_accessible_type)
     return (klass->get_accessible_type) ();
  else
     return G_TYPE_INVALID;
}
