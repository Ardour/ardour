/* ATK -  Accessibility Toolkit
 * Copyright (C) 2009 Novell, Inc.
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
#include "atkplug.h"

/**
 * SECTION:atkplug
 * @Short_description: Toplevel for embedding into other processes
 * @Title: AtkPlug
 * @See_also: #AtkPlug
 *
 * See #AtkSocket
 *
 */

static void atk_component_interface_init (AtkComponentIface *iface);

static void atk_plug_class_init (AtkPlugClass *klass);

G_DEFINE_TYPE_WITH_CODE (AtkPlug, atk_plug, ATK_TYPE_OBJECT,
                         G_IMPLEMENT_INTERFACE (ATK_TYPE_COMPONENT, atk_component_interface_init))

static void
atk_plug_init (AtkPlug* obj)
{
}

static void
atk_plug_class_init (AtkPlugClass* klass)
{
  klass->get_object_id = NULL;
}

static void
atk_component_interface_init (AtkComponentIface *iface)
{
}

AtkObject*
atk_plug_new (void)
{
  AtkObject* accessible;
  
  accessible = g_object_new (ATK_TYPE_PLUG, NULL);
  g_return_val_if_fail (accessible != NULL, NULL);

  accessible->role = ATK_ROLE_FILLER;
  accessible->layer = ATK_LAYER_WIDGET;
  
  return accessible;
}

/**
 * atk_plug_get_id:
 * @plug: an #AtkPlug
 *
 * Gets the unique ID of an #AtkPlug object, which can be used to
 * embed inside of an #AtkSocket using atk_socket_embed().
 *
 * Internally, this calls a class function that should be registered
 * by the IPC layer (usually at-spi2-atk). The implementor of an
 * #AtkPlug object should call this function (after atk-bridge is
 * loaded) and pass the value to the process implementing the
 * #AtkSocket, so it could embed the plug.
 *
 * Returns: the unique ID for the plug
 *
 * Since: 1.30
 **/
gchar*
atk_plug_get_id (AtkPlug* plug)
{
  AtkPlugClass *klass;

  g_return_val_if_fail (ATK_IS_PLUG (plug), NULL);

  klass = g_type_class_peek (ATK_TYPE_PLUG);

  if (klass && klass->get_object_id)
    return (klass->get_object_id) (plug);
  else
    return NULL;
}
