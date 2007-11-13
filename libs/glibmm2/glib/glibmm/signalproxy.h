// -*- c++ -*-
/* This is a generated file, do not edit.  Generated from signalproxy.h.m4 */

#ifndef _GLIBMM_SIGNALPROXY_H
#define _GLIBMM_SIGNALPROXY_H

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





/**** Glib::SignalProxy0 ***************************************************/

/** Proxy for signals with 0 arguments.
 * Use the connect() method, with sigc::mem_fun() or sigc::ptr_fun() to connect signals to signal handlers.
 */
template <class R>
class SignalProxy0 : public SignalProxyNormal
{
public:
  typedef sigc::slot<R>    SlotType;
  typedef sigc::slot<void> VoidSlotType;

  SignalProxy0(ObjectBase* obj, const SignalProxyInfo* info)
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


/**** Glib::SignalProxy1 ***************************************************/

/** Proxy for signals with 1 arguments.
 * Use the connect() method, with sigc::mem_fun() or sigc::ptr_fun() to connect signals to signal handlers.
 */
template <class R,class P1>
class SignalProxy1 : public SignalProxyNormal
{
public:
  typedef sigc::slot<R,P1>    SlotType;
  typedef sigc::slot<void,P1> VoidSlotType;

  SignalProxy1(ObjectBase* obj, const SignalProxyInfo* info)
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


/**** Glib::SignalProxy2 ***************************************************/

/** Proxy for signals with 2 arguments.
 * Use the connect() method, with sigc::mem_fun() or sigc::ptr_fun() to connect signals to signal handlers.
 */
template <class R,class P1,class P2>
class SignalProxy2 : public SignalProxyNormal
{
public:
  typedef sigc::slot<R,P1,P2>    SlotType;
  typedef sigc::slot<void,P1,P2> VoidSlotType;

  SignalProxy2(ObjectBase* obj, const SignalProxyInfo* info)
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


/**** Glib::SignalProxy3 ***************************************************/

/** Proxy for signals with 3 arguments.
 * Use the connect() method, with sigc::mem_fun() or sigc::ptr_fun() to connect signals to signal handlers.
 */
template <class R,class P1,class P2,class P3>
class SignalProxy3 : public SignalProxyNormal
{
public:
  typedef sigc::slot<R,P1,P2,P3>    SlotType;
  typedef sigc::slot<void,P1,P2,P3> VoidSlotType;

  SignalProxy3(ObjectBase* obj, const SignalProxyInfo* info)
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


/**** Glib::SignalProxy4 ***************************************************/

/** Proxy for signals with 4 arguments.
 * Use the connect() method, with sigc::mem_fun() or sigc::ptr_fun() to connect signals to signal handlers.
 */
template <class R,class P1,class P2,class P3,class P4>
class SignalProxy4 : public SignalProxyNormal
{
public:
  typedef sigc::slot<R,P1,P2,P3,P4>    SlotType;
  typedef sigc::slot<void,P1,P2,P3,P4> VoidSlotType;

  SignalProxy4(ObjectBase* obj, const SignalProxyInfo* info)
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


/**** Glib::SignalProxy5 ***************************************************/

/** Proxy for signals with 5 arguments.
 * Use the connect() method, with sigc::mem_fun() or sigc::ptr_fun() to connect signals to signal handlers.
 */
template <class R,class P1,class P2,class P3,class P4,class P5>
class SignalProxy5 : public SignalProxyNormal
{
public:
  typedef sigc::slot<R,P1,P2,P3,P4,P5>    SlotType;
  typedef sigc::slot<void,P1,P2,P3,P4,P5> VoidSlotType;

  SignalProxy5(ObjectBase* obj, const SignalProxyInfo* info)
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


/**** Glib::SignalProxy6 ***************************************************/

/** Proxy for signals with 6 arguments.
 * Use the connect() method, with sigc::mem_fun() or sigc::ptr_fun() to connect signals to signal handlers.
 */
template <class R,class P1,class P2,class P3,class P4,class P5,class P6>
class SignalProxy6 : public SignalProxyNormal
{
public:
  typedef sigc::slot<R,P1,P2,P3,P4,P5,P6>    SlotType;
  typedef sigc::slot<void,P1,P2,P3,P4,P5,P6> VoidSlotType;

  SignalProxy6(ObjectBase* obj, const SignalProxyInfo* info)
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


} // namespace Glib


#endif /* _GLIBMM_SIGNALPROXY_H */

