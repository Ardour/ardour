/* ATK -  Accessibility Toolkit
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

#include "atk.h"
#include "atknoopobject.h"

/**
 * SECTION:atknoopobject
 * @Short_description: An AtkObject which purports to implement all ATK interfaces.
 * @Title:AtkNoOpObject
 *
 * An AtkNoOpObject is an AtkObject which purports to implement all
 * ATK interfaces. It is the type of AtkObject which is created if an
 * accessible object is requested for an object type for which no
 * factory type is specified.
 *
 */


static void atk_no_op_object_class_init (AtkNoOpObjectClass *klass);

static gpointer parent_class = NULL;


GType
atk_no_op_object_get_type (void)
{
  static GType type = 0;

  if (!type)
  {
    static const GTypeInfo tinfo =
    {
      sizeof (AtkObjectClass),
      (GBaseInitFunc) NULL, /* base init */
      (GBaseFinalizeFunc) NULL, /* base finalize */
      (GClassInitFunc) atk_no_op_object_class_init, /* class init */
      (GClassFinalizeFunc) NULL, /* class finalize */
      NULL, /* class data */
      sizeof (AtkNoOpObject), /* instance size */
      0, /* nb preallocs */
      (GInstanceInitFunc) NULL, /* instance init */
      NULL /* value table */
    };

    static const GInterfaceInfo atk_component_info =
    {
        (GInterfaceInitFunc) NULL,
        (GInterfaceFinalizeFunc) NULL,
        NULL
    };

    static const GInterfaceInfo atk_action_info =
    {
        (GInterfaceInitFunc) NULL,
        (GInterfaceFinalizeFunc) NULL,
        NULL
    };

    static const GInterfaceInfo atk_editable_text_info =
    {
        (GInterfaceInitFunc) NULL,
        (GInterfaceFinalizeFunc) NULL,
        NULL
    };

    static const GInterfaceInfo atk_image_info =
    {
        (GInterfaceInitFunc) NULL,
        (GInterfaceFinalizeFunc) NULL,
        NULL
    };

    static const GInterfaceInfo atk_selection_info =
    {
        (GInterfaceInitFunc) NULL,
        (GInterfaceFinalizeFunc) NULL,
        NULL
    };

    static const GInterfaceInfo atk_table_info =
    {
        (GInterfaceInitFunc) NULL,
        (GInterfaceFinalizeFunc) NULL,
        NULL
    };

    static const GInterfaceInfo atk_table_cell_info =
    {
        (GInterfaceInitFunc) NULL,
        (GInterfaceFinalizeFunc) NULL,
        NULL
    };

    static const GInterfaceInfo atk_text_info =
    {
        (GInterfaceInitFunc) NULL,
        (GInterfaceFinalizeFunc) NULL,
        NULL
    };

    static const GInterfaceInfo atk_hypertext_info =
    {
        (GInterfaceInitFunc) NULL,
        (GInterfaceFinalizeFunc) NULL,
        NULL
    };

    static const GInterfaceInfo atk_value_info =
    {
        (GInterfaceInitFunc) NULL,
        (GInterfaceFinalizeFunc) NULL,
        NULL
    };

    static const GInterfaceInfo atk_document_info =
    {
        (GInterfaceInitFunc) NULL,
        (GInterfaceFinalizeFunc) NULL,
        NULL
    };

    static const GInterfaceInfo atk_window_info =
    {
        (GInterfaceInitFunc) NULL,
        (GInterfaceFinalizeFunc) NULL,
        NULL
    };

    type = g_type_register_static (ATK_TYPE_OBJECT,
                                    "AtkNoOpObject", &tinfo, 0);
    g_type_add_interface_static (type, ATK_TYPE_COMPONENT,
                                 &atk_component_info);
    g_type_add_interface_static (type, ATK_TYPE_ACTION,
                                 &atk_action_info);
    g_type_add_interface_static (type, ATK_TYPE_EDITABLE_TEXT,
                                 &atk_editable_text_info);
    g_type_add_interface_static (type, ATK_TYPE_IMAGE,
                                 &atk_image_info);
    g_type_add_interface_static (type, ATK_TYPE_SELECTION,
                                 &atk_selection_info);
    g_type_add_interface_static (type, ATK_TYPE_TABLE,
                                 &atk_table_info);
    g_type_add_interface_static (type, ATK_TYPE_TABLE_CELL,
                                 &atk_table_cell_info);
    g_type_add_interface_static (type, ATK_TYPE_TEXT,
                                 &atk_text_info);
    g_type_add_interface_static (type, ATK_TYPE_HYPERTEXT,
                                 &atk_hypertext_info);
    g_type_add_interface_static (type, ATK_TYPE_VALUE,
                                 &atk_value_info);
    g_type_add_interface_static (type, ATK_TYPE_DOCUMENT,
                                 &atk_document_info);
    g_type_add_interface_static (type, ATK_TYPE_WINDOW,
                                 &atk_window_info);
  }
  return type;
}

static void
atk_no_op_object_class_init (AtkNoOpObjectClass *klass)
{
  parent_class = g_type_class_peek_parent (klass);
}

/**
 * atk_no_op_object_new:
 * @obj: a #GObject
 *
 * Provides a default (non-functioning stub) #AtkObject.
 * Application maintainers should not use this method. 
 *
 * Returns: a default (non-functioning stub) #AtkObject
 **/
AtkObject*
atk_no_op_object_new (GObject *obj)
{
  AtkObject *accessible;

  g_return_val_if_fail (obj != NULL, NULL);

  accessible = g_object_new (ATK_TYPE_NO_OP_OBJECT, NULL);
  g_return_val_if_fail (accessible != NULL, NULL);

  accessible->role = ATK_ROLE_INVALID;
  accessible->layer = ATK_LAYER_INVALID;

  return accessible;
}

