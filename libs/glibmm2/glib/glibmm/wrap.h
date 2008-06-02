// -*- c++ -*-
#ifndef _GLIBMM_WRAP_H
#define _GLIBMM_WRAP_H

/* $Id: wrap.h 447 2007-10-03 09:51:41Z murrayc $ */

/* Copyright (C) 1998-2002 The gtkmm Development Team
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

#include <glib-object.h>
#include <glibmm/refptr.h>
#include <glibmm/objectbase.h>

namespace Glib
{

#ifndef DOXYGEN_SHOULD_SKIP_THIS

class ObjectBase;
class Object;

// Type of the per-class wrap_new() functions.
typedef Glib::ObjectBase* (*WrapNewFunction) (GObject*);

// Setup and free the structures used by wrap_register().
// Both functions might be called more than once.
void wrap_register_init();
void wrap_register_cleanup();

// Register a new type for auto allocation.
void wrap_register(GType type, WrapNewFunction func);

// Return the current C++ wrapper instance of the GObject,
// or automatically generate a new wrapper if there's none.
Glib::ObjectBase* wrap_auto(GObject* object, bool take_copy = false);

/** Create a C++ instance of a known C++ type that is mostly closely associated with the GType of the C object.
 * @param object The C object which should be placed in a new C++ instance.
 * @param interface_gtype The returned instance will implement this interface. Otherwise it will be NULL.
 */
Glib::ObjectBase* wrap_create_new_wrapper_for_interface(GObject* object, GType interface_gtype);

// Return the current C++ wrapper instance of the GObject,
// or automatically generate a new wrapper if there's none.
template<class TInterface>
TInterface* wrap_auto_interface(GObject* object, bool take_copy = false)
{
  if(!object)
    return 0;

  // Look up current C++ wrapper instance:
  ObjectBase* pCppObject = ObjectBase::_get_current_wrapper(object);

  if(!pCppObject)
  {
    // There's not already a wrapper: generate a new C++ instance.
    // We use exact_type_only=true avoid creating Glib::Object for interfaces of unknown implementation,
    // because we do not want a C++ object that does not dynamic_cast to the expected interface type.
    pCppObject = wrap_create_new_wrapper_for_interface(object, TInterface::get_base_type());
  }

  //If no exact wrapper was created, 
  //create an instance of the interface, 
  //so we at least get the expected type:
  TInterface* result = 0;
  if(pCppObject)
     result = dynamic_cast<TInterface*>(pCppObject);
  else
     result = new TInterface((typename TInterface::BaseObjectType*)object);

  // take_copy=true is used where the GTK+ function doesn't do
  // an extra ref for us, and always for plain struct members.
  if(take_copy && result)
    result->reference();

  return result;
}

#endif //DOXYGEN_SHOULD_SKIP_THIS

// Get a C++ instance that wraps the C instance.
// This always returns the same C++ instance for the same C instance.
// Each wrapper has it's own override of Glib::wrap().
// use take_copy = true when wrapping a struct member.
// TODO: move to object.h ?
/** @relates Glib::Object */
Glib::RefPtr<Glib::Object> wrap(GObject* object, bool take_copy = false);


/** Get the underlying C instance from the C++ instance.  This is just
 * like calling gobj(), but it does its own check for a NULL pointer.
 */
template <class T> inline
typename T::BaseObjectType* unwrap(T* ptr)
{
  return (ptr) ? ptr->gobj() : 0;
}

/** Get the underlying C instance from the C++ instance.  This is just
 * like calling gobj(), but it does its own check for a NULL pointer.
 */
template <class T> inline
const typename T::BaseObjectType* unwrap(const T* ptr)
{
  return (ptr) ? ptr->gobj() : 0;
}

/** Get the underlying C instance from the C++ instance.  This is just
 * like calling gobj(), but it does its own check for a NULL pointer.
 */
template <class T> inline
typename T::BaseObjectType* unwrap(const Glib::RefPtr<T>& ptr)
{
  return (ptr) ? ptr->gobj() : 0;
}

/** Get the underlying C instance from the C++ instance.  This is just
 * like calling gobj(), but it does its own check for a NULL pointer.
 */
template <class T> inline
const typename T::BaseObjectType* unwrap(const Glib::RefPtr<const T>& ptr)
{
  return (ptr) ? ptr->gobj() : 0;
}

/** Get the underlying C instance from the C++ instance and acquire a
 * reference.  This is just like calling gobj_copy(), but it does its own
 * check for a NULL pointer.
 */
template <class T> inline
typename T::BaseObjectType* unwrap_copy(const Glib::RefPtr<T>& ptr)
{
  return (ptr) ? ptr->gobj_copy() : 0;
}

/** Get the underlying C instance from the C++ instance and acquire a
 * reference.  This is just like calling gobj_copy(), but it does its own
 * check for a NULL pointer.
 */
template <class T> inline
const typename T::BaseObjectType* unwrap_copy(const Glib::RefPtr<const T>& ptr)
{
  return (ptr) ? ptr->gobj_copy() : 0;
}

} // namespace Glib


#endif /* _GLIBMM_WRAP_H */

