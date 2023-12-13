/* ATK - Accessibility Toolkit
 * Copyright 2001, 2002, 2003 Sun Microsystems Inc.
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

#include <atk/atkgobjectaccessible.h>
#include <atk/atkregistry.h>
#include <atk/atkutil.h>

/**
 * SECTION:atkgobjectaccessible
 * @Short_description: This object class is derived from AtkObject and
 *  can be used as a basis implementing accessible objects.
 * @Title:AtkGObjectAccessible
 *
 * This object class is derived from AtkObject. It can be used as a
 * basis for implementing accessible objects for GObjects which are
 * not derived from GtkWidget. One example of its use is in providing
 * an accessible object for GnomeCanvasItem in the GAIL library.
 */
static void       atk_gobject_accessible_class_init       (AtkGObjectAccessibleClass   *klass);
static void       atk_real_gobject_accessible_initialize  (AtkObject         *atk_obj,
                                                           gpointer          data);
static void       atk_gobject_accessible_dispose          (gpointer          data);

static GQuark quark_accessible_object = 0;
static GQuark quark_object = 0;
static gpointer parent_class = NULL;

GType
atk_gobject_accessible_get_type (void)
{
  static GType type = 0;

  if (!type)
    {
      static const GTypeInfo tinfo =
      {
        sizeof (AtkGObjectAccessibleClass),
        (GBaseInitFunc) NULL, /* base init */
        (GBaseFinalizeFunc) NULL, /* base finalize */
        (GClassInitFunc) atk_gobject_accessible_class_init,
        (GClassFinalizeFunc) NULL, /* class finalize */
        NULL, /* class data */
        sizeof (AtkGObjectAccessible),
        0, /* nb preallocs */
        (GInstanceInitFunc) NULL, /* instance init */
        NULL /* value table */
      };

      type = g_type_register_static (ATK_TYPE_OBJECT,
                                     "AtkGObjectAccessible", &tinfo, 0);
    }

  return type;
}

/**
 * atk_gobject_accessible_for_object:
 * @obj: a #GObject
 *
 * Gets the accessible object for the specified @obj.
 *
 * Returns: (transfer none): a #AtkObject which is the accessible object for
 * the @obj
 **/
AtkObject*
atk_gobject_accessible_for_object (GObject *obj)
{
  AtkObject* accessible;

  g_return_val_if_fail (G_IS_OBJECT (obj), NULL);
  /* See if we have a cached accessible for this object */

  accessible = g_object_get_qdata (obj,
				   quark_accessible_object);

  if (!accessible)
    {
      AtkObjectFactory *factory;
      AtkRegistry *default_registry;

      default_registry = atk_get_default_registry ();
      factory = atk_registry_get_factory (default_registry, 
                                          G_OBJECT_TYPE (obj));
      accessible = atk_object_factory_create_accessible (factory,
                                                         obj);
      if (!ATK_IS_GOBJECT_ACCESSIBLE (accessible))
        {
          /*
           * The AtkObject which was created was not a AtkGObjectAccessible
           */
          g_object_weak_ref (obj,
                             (GWeakNotify) g_object_unref,
                             accessible); 
          if (!quark_accessible_object)
            quark_accessible_object = g_quark_from_static_string ("accessible-object");
        }
      g_object_set_qdata (obj, quark_accessible_object, accessible);
    }
  return accessible;
}

/**
 * atk_gobject_accessible_get_object:
 * @obj: a #AtkGObjectAccessible
 *
 * Gets the GObject for which @obj is the accessible object.
 *
 * Returns: (transfer none): a #GObject which is the object for which @obj is
 * the accessible object
 **/
GObject *
atk_gobject_accessible_get_object (AtkGObjectAccessible *obj)
{
  g_return_val_if_fail (ATK_IS_GOBJECT_ACCESSIBLE (obj), NULL);

  return g_object_get_qdata (G_OBJECT (obj), quark_object);
}
 
static void
atk_real_gobject_accessible_initialize (AtkObject  *atk_obj,
                                        gpointer   data)
{
  AtkGObjectAccessible *atk_gobj;

  atk_gobj = ATK_GOBJECT_ACCESSIBLE (atk_obj);

  g_object_set_qdata (G_OBJECT (atk_gobj), quark_object, data);
  atk_obj->layer = ATK_LAYER_WIDGET;

  g_object_weak_ref (data,
                     (GWeakNotify) atk_gobject_accessible_dispose,
                     atk_gobj);
}

static void
atk_gobject_accessible_dispose (gpointer  data)
{
  GObject *object;

  g_return_if_fail (ATK_IS_GOBJECT_ACCESSIBLE (data));

  object = atk_gobject_accessible_get_object (data);
  if (object)
      g_object_set_qdata (object, quark_accessible_object, NULL);

  g_object_set_qdata (G_OBJECT (data), quark_object, NULL);
  atk_object_notify_state_change (ATK_OBJECT (data), ATK_STATE_DEFUNCT,
                                  TRUE); 
  g_object_unref (data);
}

static void
atk_gobject_accessible_class_init (AtkGObjectAccessibleClass *klass)
{ 
  AtkObjectClass *class;

  class = ATK_OBJECT_CLASS (klass);

  parent_class = g_type_class_peek_parent (klass);

  class->initialize = atk_real_gobject_accessible_initialize;

  if (!quark_accessible_object)
    quark_accessible_object = g_quark_from_static_string ("accessible-object");
  quark_object = g_quark_from_static_string ("object-for-accessible");
}
