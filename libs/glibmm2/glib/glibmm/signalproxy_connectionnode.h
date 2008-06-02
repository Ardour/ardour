// -*- c++ -*-
#ifndef _GLIBMM_SIGNALPROXY_CONNECTIONNODE_H
#define _GLIBMM_SIGNALPROXY_CONNECTIONNODE_H

/* $Id: signalproxy_connectionnode.h 446 2007-09-26 15:10:44Z murrayc $ */

/* signalproxy_connectionnode.h
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

#include <sigc++/sigc++.h>
#include <glib/gtypes.h>

#ifndef DOXYGEN_SHOULD_SKIP_THIS
typedef struct _GObject GObject;
typedef struct _GClosure GClosure;
#endif //DOXYGEN_SHOULD_SKIP_THIS

namespace Glib
{

#ifndef DOXYGEN_SHOULD_SKIP_THIS

/** SignalProxyConnectionNode is a connection node for use with SignalProxy.
  * It lives between the layer of Gtk+ and libsigc++.
  * It is very much an internal class.
  */
class SignalProxyConnectionNode
{
public:

  /** @param slot The signal handler for the glib signal.
   *  @param gobject The GObject that might emit this glib signal
   */
  SignalProxyConnectionNode(const sigc::slot_base& slot, GObject* gobject);

  /** Callback that is executed when the slot becomes invalid.
   * This callback is registered in the slot.
   * @param data The SignalProxyConnectionNode object (@p this).
   */
  static void* notify(void* data);

  /** Callback that is executed when the glib closure is destroyed.
   * @param data The SignalProxyConnectionNode object (@p this).
   * @param closure The glib closure object.
   */
  static void destroy_notify_handler(gpointer data, GClosure* closure);

  gulong connection_id_;
  sigc::slot_base slot_;

protected:
  GObject* object_;
};

#endif //DOXYGEN_SHOULD_SKIP_THIS

} /* namespace Glib */


#endif /* _GLIBMM_SIGNALPROXY_CONNECTIONNODE_H */

