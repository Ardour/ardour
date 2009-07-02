// -*- c++ -*-
/* $Id: wrap.cc 749 2008-12-10 14:23:33Z jjongsma $ */

/* wrap.cc
 *
 * Copyright (C) 1998-2002 The gtkmm Development Team
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
#include <glib.h>

#include <vector>
#include <glibmm/object.h>
#include <glibmm/quark.h>
#include <glibmm/wrap.h>

#include <glibmmconfig.h>
GLIBMM_USING_STD(vector)


namespace
{

// Although the new g_type_set_qdata() interface is used now, we still need
// a table because we cannot assume that a function pointer fits into void*
// on any platform.  Nevertheless, indexing a vector costs almost nothing
// compared to a map lookup.

typedef std::vector<Glib::WrapNewFunction> WrapFuncTable;

static WrapFuncTable* wrap_func_table = 0;

} // anonymous namespace


namespace Glib
{

void wrap_register_init()
{
  g_type_init();

  if(!Glib::quark_)
  {
    Glib::quark_ = g_quark_from_static_string("glibmm__Glib::quark_");
    Glib::quark_cpp_wrapper_deleted_ = g_quark_from_static_string("glibmm__Glib::quark_cpp_wrapper_deleted_");
  }

  if(!wrap_func_table)
  {
    // Make the first element a dummy so we can detect unregistered types.
    // g_type_get_qdata() returns NULL if no data has been set up.
    wrap_func_table = new WrapFuncTable(1);
  }
}

void wrap_register_cleanup()
{
  if(wrap_func_table)
  {
    delete wrap_func_table;
    wrap_func_table = 0;
  }
}

// Register the unique wrap_new() function of a new C++ wrapper type.
// The GType argument specifies the parent C type to wrap from.
//
void wrap_register(GType type, WrapNewFunction func)
{
  const guint idx = wrap_func_table->size();
  wrap_func_table->push_back(func);

  // Store the table index in the type's static data.
  g_type_set_qdata(type, Glib::quark_, GUINT_TO_POINTER(idx));
}


static Glib::ObjectBase* wrap_create_new_wrapper(GObject* object)
{
  g_return_val_if_fail(wrap_func_table != 0, 0);

  const bool gtkmm_wrapper_already_deleted = (bool)g_object_get_qdata((GObject*)object, Glib::quark_cpp_wrapper_deleted_);
  if(gtkmm_wrapper_already_deleted)
  {
    g_warning("Glib::wrap_create_new_wrapper: Attempted to create a 2nd C++ wrapper for a C instance whose C++ wrapper has been deleted.");
    return 0;
  }

  // Traverse upwards through the inheritance hierarchy
  // to find the most-specialized wrap_new() for this GType.
  //
  for(GType type = G_OBJECT_TYPE(object); type != 0; type = g_type_parent(type))
  {
    // Look up the wrap table index stored in the type's static data.
    // If a wrap_new() has been registered for the type then call it.
    //
    if(const gpointer idx = g_type_get_qdata(type, Glib::quark_))
    {
      const Glib::WrapNewFunction func = (*wrap_func_table)[GPOINTER_TO_UINT(idx)];
      return (*func)(object);
    }
  }

  return 0;
}

static gboolean gtype_wraps_interface(GType implementer_type, GType interface_type)
{
  guint n_ifaces = 0;
  GType *ifaces = g_type_interfaces (implementer_type, &n_ifaces);

  gboolean found = FALSE;
  while (n_ifaces-- && !found)
  {
    found = (ifaces[n_ifaces] == interface_type);
  }
      
  g_free (ifaces);

  return found;
}

Glib::ObjectBase* wrap_create_new_wrapper_for_interface(GObject* object, GType interface_gtype)
{
  g_return_val_if_fail(wrap_func_table != 0, 0);

  const bool gtkmm_wrapper_already_deleted = (bool)g_object_get_qdata((GObject*)object, Glib::quark_cpp_wrapper_deleted_);
  if(gtkmm_wrapper_already_deleted)
  {
    g_warning("Glib::wrap_create_new_wrapper: Attempted to create a 2nd C++ wrapper for a C instance whose C++ wrapper has been deleted.");
    return 0;
  }

  // Traverse upwards through the inheritance hierarchy
  // to find the most-specialized wrap_new() for this GType.
  //
  for(GType type = G_OBJECT_TYPE(object); type != 0; type = g_type_parent(type))
  {
    // Look up the wrap table index stored in the type's static data.
    // If a wrap_new() has been registered for the type then call it.
    // But only if the type implements the interface, 
    // so that the C++ instance is likely to inherit from the appropriate class too.
    //
    const gpointer idx = g_type_get_qdata(type, Glib::quark_);
    if(idx && gtype_wraps_interface(type, interface_gtype))
    {
      const Glib::WrapNewFunction func = (*wrap_func_table)[GPOINTER_TO_UINT(idx)];
      return (*func)(object);
    }
  }

  return 0;
}


// This is a factory function that converts any type to
// its C++ wrapper instance by looking up a wrap_new() function in a map.
//
ObjectBase* wrap_auto(GObject* object, bool take_copy)
{
  if(!object)
    return 0;

  // Look up current C++ wrapper instance:
  ObjectBase* pCppObject = ObjectBase::_get_current_wrapper(object);

  if(!pCppObject)
  {
    // There's not already a wrapper: generate a new C++ instance.
    pCppObject = wrap_create_new_wrapper(object);

    if(!pCppObject)
    {
      g_warning("Failed to wrap object of type '%s'. Hint: this error is commonly caused by failing to call a library init() function.", G_OBJECT_TYPE_NAME(object));
      return 0;
    }
  }

  // take_copy=true is used where the GTK+ function doesn't do
  // an extra ref for us, and always for plain struct members.
  if(take_copy)
    pCppObject->reference();

  return pCppObject;
}

Glib::RefPtr<Object> wrap(GObject* object, bool take_copy /* = false */)
{
  return Glib::RefPtr<Object>(dynamic_cast<Object*>(wrap_auto(object, take_copy)));
}

} /* namespace Glib */

