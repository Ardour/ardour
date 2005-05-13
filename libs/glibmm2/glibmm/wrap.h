// -*- c++ -*-
#ifndef _GLIBMM_WRAP_H
#define _GLIBMM_WRAP_H

/* $Id$ */

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


namespace Glib
{

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

