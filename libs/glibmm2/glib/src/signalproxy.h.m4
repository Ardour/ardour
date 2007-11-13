// -*- c++ -*-
dnl 
dnl Glib SignalProxy Templates
dnl 
dnl  Copyright 2001 Free Software Foundation
dnl  Copyright 1999 Karl Nelson <kenelson@ece.ucdavis.edu>
dnl 
dnl  This library is free software; you can redistribute it and/or
dnl  modify it under the terms of the GNU Library General Public
dnl  License as published by the Free Software Foundation; either
dnl  version 2 of the License, or (at your option) any later version.
dnl 
dnl  This library is distributed in the hope that it will be useful,
dnl  but WITHOUT ANY WARRANTY; without even the implied warranty of
dnl  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
dnl  Library General Public License for more details.
dnl 
dnl  You should have received a copy of the GNU Library General Public
dnl  License along with this library; if not, write to the Free
dnl  Software Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
dnl 
dnl Ignore the next line
/* This is a generated file, do not edit.  Generated from __file__ */
include(template.macros.m4)
#ifndef __header__
#define __header__

extern "C"
{
  typedef void (*GCallback) (void);
  typedef struct _GObject GObject;
}

#include <sigc++/sigc++.h>
#include <glibmm/signalproxy_connectionnode.h>


namespace Glib
{

// Forward declarations
class ObjectBase;

#ifndef DOXYGEN_SHOULD_SKIP_THIS

struct SignalProxyInfo
{
  const char* signal_name;
  GCallback   callback;
  GCallback   notify_callback;
};

#endif //DOXYGEN_SHOULD_SKIP_THIS

// This base class is used by SignalProxyNormal and SignalProxyProperty.
class SignalProxyBase
{
public:
  SignalProxyBase(Glib::ObjectBase* obj);

#ifndef DOXYGEN_SHOULD_SKIP_THIS
  static inline sigc::slot_base* data_to_slot(void* data)
  {
    SignalProxyConnectionNode *const pConnectionNode = static_cast<SignalProxyConnectionNode*>(data);

    // Return 0 if the connection is blocked.
    return (!pConnectionNode->slot_.blocked()) ? &pConnectionNode->slot_ : 0;
  }
#endif /* DOXYGEN_SHOULD_SKIP_THIS */

protected:
  ObjectBase* obj_;

private:
  SignalProxyBase& operator=(const SignalProxyBase&); // not implemented
};


// shared portion of a Signal
/** The SignalProxy provides an API similar to sigc::signal that can be used to
 * connect sigc::slots to glib signals.
 *
 * This holds the name of the glib signal and the object
 * which might emit it. Actually, proxies are controlled by
 * the template derivatives, which serve as gatekeepers for the
 * types allowed on a particular signal.
 *
 */
class SignalProxyNormal : public SignalProxyBase
{
public:
  ~SignalProxyNormal();

  /// stops the current signal emmision (not in libsigc++)
  void emission_stop();

#ifndef DOXYGEN_SHOULD_SKIP_THIS
  // This callback for SignalProxy0<void>
  // is defined here to avoid code duplication.
  static void slot0_void_callback(GObject*, void* data);
#endif

protected:

  /** Create a proxy for a signal that can be emitted by @a obj.
   * @param obj The object that can emit the signal.
   * @param info Information about the signal, including its name, and the C callbacks that should be called by glib.
   */
  SignalProxyNormal(Glib::ObjectBase* obj, const SignalProxyInfo* info);

  /** Connects a signal to a generic signal handler. This is called by connect() in derived SignalProxy classes.
   *
   * @param slot The signal handler, usually created with sigc::mem_fun(), or sigc::ptr_fun().
   * @param after Whether this signal handler should be called before or after the default signal handler.
   */
  sigc::slot_base& connect_(const sigc::slot_base& slot, bool after);

  /** Connects a signal to a signal handler without a return value.
   * This is called by connect() in derived SignalProxy classes.
   *
   * By default, the signal handler will be called before the default signal handler,
   * in which case any return value would be replaced anyway by that of the later signal handler.
   *
   * @param slot The signal handler, which should have a void return type, usually created with sigc::mem_fun(), or sigc::ptr_fun().
   * @param after Whether this signal handler should be called before or after the default signal handler.
   */
  sigc::slot_base& connect_notify_(const sigc::slot_base& slot, bool after);

private:
  const SignalProxyInfo* info_;

  //TODO: We could maybe replace both connect_ and connect_notify_ with this in future, because they don't do anything extra.
  /** This is called by connect_ and connect_impl_.
   */
  sigc::slot_base& connect_impl_(GCallback callback, const sigc::slot_base& slot, bool after);

  // no copy assignment
  SignalProxyNormal& operator=(const SignalProxyNormal&);
};


dnl
dnl GLIB_SIGNAL_PROXY([P1, P2, ...],return type)
dnl
define([GLIB_SIGNAL_PROXY],[dnl
LINE(]__line__[)dnl

/**** Glib::[SignalProxy]NUM($1) ***************************************************/

/** Proxy for signals with NUM($1) arguments.
 * Use the connect() method, with sigc::mem_fun() or sigc::ptr_fun() to connect signals to signal handlers.
 */
template <LIST(class R,ARG_CLASS($1))>
class [SignalProxy]NUM($1) : public SignalProxyNormal
{
public:
  typedef sigc::slot<LIST(R,ARG_TYPE($1))>    SlotType;
  typedef sigc::slot<LIST(void,ARG_TYPE($1))> VoidSlotType;

  [SignalProxy]NUM($1)(ObjectBase* obj, const SignalProxyInfo* info)
    : SignalProxyNormal(obj, info) {}

  /** Connects a signal to a signal handler.
   * For instance, connect( sigc::mem_fun(*this, &TheClass::on_something) );
   *
   * @param slot The signal handler, usually created with sigc::mem_fun(), or sigc::ptr_fun().
   * @param after Whether this signal handler should be called before or after the default signal handler.
   */
  sigc::connection connect(const SlotType& slot, bool after = true)
    { return sigc::connection(connect_(slot, after)); }

  /** Connects a signal to a signal handler without a return value.
   * By default, the signal handler will be called before the default signal handler,
   * in which case any return value would be replaced anyway by that of the later signal handler.
   *
   * For instance, connect( sigc::mem_fun(*this, &TheClass::on_something) );
   *
   * @param slot The signal handler, which should have a void return type, usually created with sigc::mem_fun(), or sigc::ptr_fun().
   * @param after Whether this signal handler should be called before or after the default signal handler.
   */
  sigc::connection connect_notify(const VoidSlotType& slot, bool after = false)
    { return sigc::connection(connect_notify_(slot, after)); }
};
])dnl

dnl Template forms of SignalProxy

GLIB_SIGNAL_PROXY(ARGS(P,0))
GLIB_SIGNAL_PROXY(ARGS(P,1))
GLIB_SIGNAL_PROXY(ARGS(P,2))
GLIB_SIGNAL_PROXY(ARGS(P,3))
GLIB_SIGNAL_PROXY(ARGS(P,4))
GLIB_SIGNAL_PROXY(ARGS(P,5))
GLIB_SIGNAL_PROXY(ARGS(P,6))

} // namespace Glib


#endif /* __header__ */

