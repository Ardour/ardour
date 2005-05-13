// -*- c++ -*-

/* $Id$ */

/* signalproxy_connectionnode.cc
 *
 * Copyright (C) 2002 The gtkmm Development Team
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
 * License along with this library; if not, write to the Free
 * Software Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#include <glibmm/signalproxy_connectionnode.h>
#include <glibmm/object.h>


namespace Glib
{

SignalProxyConnectionNode::SignalProxyConnectionNode(const sigc::slot_base& slot, GObject* gobject)
:
  connection_id_ (0),
  slot_          (slot),
  object_        (gobject)
{
  //The cleanup callback will be called when the connection is disconnected.
  slot_.set_parent(this, &SignalProxyConnectionNode::notify /* cleanup callback */);
}

// notify is a message coming up from the slot to be passed back to Gtk+
// disconnect is a message coming up from the Gtk+ to be passed down to SigC++
//static
void* SignalProxyConnectionNode::notify(void* data)
{
  // notification from libsigc++.
  SignalProxyConnectionNode* conn = static_cast<SignalProxyConnectionNode*>(data);

  // If there is no object, this call was triggered from destroy_notify_handler(),
  // because we set conn->object to 0 there:
  if(conn && conn->object_)
  {
    GObject* o = conn->object_;
    conn->object_ = 0;
  
    if(g_signal_handler_is_connected(o, conn->connection_id_)) //We check first, because during destruction, GTK+ sometimes seems to disconnect them for us, before we expect it to.  See bug #87912
    {
      // Disconnecting triggers execution of destroy_notify_handler(), eiter immediately or later:
      //   When the signal handler is currently running. (for instance, if the callback disconnects its own connection)
      //   In that case, destroy_notify_handler() will be called after this whole function has returned.
      // Anyway. destroy_notify_handler() will always be called, so we leave that to do the deletion.

      
      //Forget the connection:
      gulong connection_id = conn->connection_id_;
      conn->connection_id_ = 0;
      
      g_signal_handler_disconnect(o, connection_id);
    }
  }

  return 0; // apparently unused in libsigc++
}

//static
void SignalProxyConnectionNode::destroy_notify_handler(gpointer data, GClosure*)
{
  //glib calls this when it has finished with a glib signal connection,
  //either when the emitting object dies, or when the connection has been disconnected.
  
  // notification from gtk+.
  SignalProxyConnectionNode* conn = static_cast<SignalProxyConnectionNode*>(data);

  if(conn)
  {
    // the object has already lost track of this object.
    conn->object_ = 0;

    delete conn; // if there are connection objects referring to slot_ they are notified during destruction of slot_
  }
}

} /* namespace Glib */

