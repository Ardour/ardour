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

#include "atkobject.h"
#include "atknoopobject.h"
#include "atknoopobjectfactory.h"

/**
 * SECTION:atknoopobjectfactory
 * @Short_description: The AtkObjectFactory which creates an AtkNoOpObject.
 * @Title:AtkNoOpObjectFactory
 *
 * The AtkObjectFactory which creates an AtkNoOpObject. An instance of
 * this is created by an AtkRegistry if no factory type has not been
 * specified to create an accessible object of a particular type.
 */
static void atk_no_op_object_factory_class_init (
                              AtkNoOpObjectFactoryClass        *klass);

static AtkObject* atk_no_op_object_factory_create_accessible (
                              GObject                          *obj);
static GType      atk_no_op_object_factory_get_accessible_type (void);

static gpointer    parent_class = NULL;

GType
atk_no_op_object_factory_get_type (void)
{
  static GType type = 0;

  if (!type) 
  {
    static const GTypeInfo tinfo =
    {
      sizeof (AtkNoOpObjectFactoryClass),
      (GBaseInitFunc) NULL, /* base init */
      (GBaseFinalizeFunc) NULL, /* base finalize */
      (GClassInitFunc) atk_no_op_object_factory_class_init, /* class init */
      (GClassFinalizeFunc) NULL, /* class finalize */
      NULL, /* class data */
      sizeof (AtkNoOpObjectFactory), /* instance size */
      0, /* nb preallocs */
      (GInstanceInitFunc) NULL, /* instance init */
      NULL /* value table */
    };
    type = g_type_register_static (
                           ATK_TYPE_OBJECT_FACTORY, 
                           "AtkNoOpObjectFactory" , &tinfo, 0);
  }

  return type;
}

static void 
atk_no_op_object_factory_class_init (AtkNoOpObjectFactoryClass *klass)
{
  AtkObjectFactoryClass *class = ATK_OBJECT_FACTORY_CLASS (klass);

  parent_class = g_type_class_peek_parent (klass);

  class->create_accessible = atk_no_op_object_factory_create_accessible;
  class->get_accessible_type = atk_no_op_object_factory_get_accessible_type;
}

/**
 * atk_no_op_object_factory_new:
 *
 * Creates an instance of an #AtkObjectFactory which generates primitive
 * (non-functioning) #AtkObjects. 
 *
 * Returns: an instance of an #AtkObjectFactory
 **/
AtkObjectFactory* 
atk_no_op_object_factory_new (void)
{
  GObject *factory;

  factory = g_object_new (ATK_TYPE_NO_OP_OBJECT_FACTORY, NULL);

  g_return_val_if_fail (factory != NULL, NULL);
  return ATK_OBJECT_FACTORY (factory);
} 

static AtkObject* 
atk_no_op_object_factory_create_accessible (GObject   *obj)
{
  AtkObject     *accessible;

  accessible = atk_no_op_object_new (obj);

  return accessible;
}

static GType
atk_no_op_object_factory_get_accessible_type (void)
{
  return ATK_TYPE_NO_OP_OBJECT;
}
