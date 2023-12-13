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
#include "atksocket.h"

/**
 * SECTION:atksocket
 * @Short_description: Container for AtkPlug objects from other processes
 * @Title: AtkSocket
 * @See_also: #AtkPlug
 *
 * Together with #AtkPlug, #AtkSocket provides the ability to embed
 * accessibles from one process into another in a fashion that is
 * transparent to assistive technologies. #AtkSocket works as the
 * container of #AtkPlug, embedding it using the method
 * atk_socket_embed(). Any accessible contained in the #AtkPlug will
 * appear to the assistive technologies as being inside the
 * application that created the #AtkSocket.
 *
 * The communication between a #AtkSocket and a #AtkPlug is done by
 * the IPC layer of the accessibility framework, normally implemented
 * by the D-Bus based implementation of AT-SPI (at-spi2). If that is
 * the case, at-spi-atk2 is the responsible to implement the abstract
 * methods atk_plug_get_id() and atk_socket_embed(), so an ATK
 * implementor shouldn't reimplement them. The process that contains
 * the #AtkPlug is responsible to send the ID returned by
 * atk_plug_id() to the process that contains the #AtkSocket, so it
 * could call the method atk_socket_embed() in order to embed it.
 *
 * For the same reasons, an implementor doesn't need to implement
 * atk_object_get_n_accessible_children() and
 * atk_object_ref_accessible_child(). All the logic related to those
 * functions will be implemented by the IPC layer.
 */

static void atk_socket_class_init (AtkSocketClass *klass);
static void atk_socket_finalize   (GObject *obj);

static void atk_component_interface_init (AtkComponentIface *iface);

G_DEFINE_TYPE_WITH_CODE (AtkSocket, atk_socket, ATK_TYPE_OBJECT,
                         G_IMPLEMENT_INTERFACE (ATK_TYPE_COMPONENT, atk_component_interface_init))

static void
atk_socket_init (AtkSocket* obj)
{
  obj->embedded_plug_id = NULL;
}

static void
atk_socket_class_init (AtkSocketClass* klass)
{
  GObjectClass *obj_class = G_OBJECT_CLASS (klass);

  obj_class->finalize = atk_socket_finalize;

  klass->embed = NULL;
}

static void
atk_socket_finalize (GObject *_obj)
{
  AtkSocket *obj = ATK_SOCKET (_obj);

  g_free (obj->embedded_plug_id);
  obj->embedded_plug_id = NULL;

  G_OBJECT_CLASS (atk_socket_parent_class)->finalize (_obj);
}

static void atk_component_interface_init (AtkComponentIface *iface)
{
}

AtkObject*
atk_socket_new (void)
{
  AtkObject* accessible;
  
  accessible = g_object_new (ATK_TYPE_SOCKET, NULL);
  g_return_val_if_fail (accessible != NULL, NULL);

  accessible->role = ATK_ROLE_FILLER;
  accessible->layer = ATK_LAYER_WIDGET;
  
  return accessible;
}

/**
 * atk_socket_embed:
 * @obj: an #AtkSocket
 * @plug_id: the ID of an #AtkPlug
 *
 * Embeds the children of an #AtkPlug as the children of the
 * #AtkSocket. The plug may be in the same process or in a different
 * process.
 *
 * The class item used by this function should be filled in by the IPC
 * layer (usually at-spi2-atk). The implementor of the AtkSocket
 * should call this function and pass the id for the plug as returned
 * by atk_plug_get_id().  It is the responsibility of the application
 * to pass the plug id on to the process implementing the #AtkSocket
 * as needed.
 *
 * Since: 1.30
 **/
void
atk_socket_embed (AtkSocket* obj, gchar* plug_id)
{
  AtkSocketClass *klass;

  g_return_if_fail (plug_id != NULL);
  g_return_if_fail (ATK_IS_SOCKET (obj));

  klass = g_type_class_peek (ATK_TYPE_SOCKET);
  if (klass && klass->embed)
    {
      if (obj->embedded_plug_id)
        g_free (obj->embedded_plug_id);
      obj->embedded_plug_id = g_strdup (plug_id);
      (klass->embed) (obj, plug_id);
    }
}

/**
 * atk_socket_is_occupied:
 * @obj: an #AtkSocket
 *
 * Determines whether or not the socket has an embedded plug.
 *
 * Returns: TRUE if a plug is embedded in the socket
 *
 * Since: 1.30
 **/
gboolean
atk_socket_is_occupied (AtkSocket* obj)
{
  g_return_val_if_fail (ATK_IS_SOCKET (obj), FALSE);

  return (obj->embedded_plug_id != NULL);
}
