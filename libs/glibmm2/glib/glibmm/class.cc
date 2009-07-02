// -*- c++ -*-
/* $Id: class.cc 336 2006-10-04 12:06:14Z murrayc $ */

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

#include <glibmm/class.h>
#include <glibmm/property.h>
#include <glibmm/ustring.h>
#include <glibmm/utility.h>


namespace Glib
{

void Class::register_derived_type(GType base_type)
{
  if(gtype_)
    return; // already initialized

  GTypeQuery base_query = { 0, 0, 0, 0, };
  g_type_query(base_type, &base_query);

  const GTypeInfo derived_info =
  {
    base_query.class_size,
    0, // base_init
    0, // base_finalize
    class_init_func_,
    0, // class_finalize
    0, // class_data
    base_query.instance_size,
    0, // n_preallocs
    0, // instance_init
    0, // value_table
  };

  Glib::ustring derived_name = "gtkmm__";
  derived_name += base_query.type_name;

  gtype_ = g_type_register_static(base_type, derived_name.c_str(), &derived_info, GTypeFlags(0));
}

GType Class::clone_custom_type(const char* custom_type_name) const
{
  std::string full_name ("gtkmm__CustomObject_");
  Glib::append_canonical_typename(full_name, custom_type_name);

  GType custom_type = g_type_from_name(full_name.c_str());

  if(!custom_type)
  {
    g_return_val_if_fail(gtype_ != 0, 0);

    // Cloned custom types derive from the wrapper's parent type,
    // so that g_type_class_peek_parent() works correctly.
    const GType base_type = g_type_parent(gtype_);

    GTypeQuery base_query = { 0, 0, 0, 0, };
    g_type_query(base_type, &base_query);

    const GTypeInfo derived_info =
    {
      base_query.class_size,
      0, // base_init
      0, // base_finalize
      &Class::custom_class_init_function,
      0, // class_finalize
      this, // class_data
      base_query.instance_size,
      0, // n_preallocs
      0, // instance_init
      0, // value_table
    };

    custom_type = g_type_register_static(
        base_type, full_name.c_str(), &derived_info, GTypeFlags(0));
  }

  return custom_type;
}

// static
void Class::custom_class_init_function(void* g_class, void* class_data)
{
  // The class_data pointer is set to 'this' by clone_custom_type().
  const Class *const self = static_cast<Class*>(class_data);

  g_return_if_fail(self->class_init_func_ != 0);

  // Call the wrapper's class_init_function() to redirect
  // the vfunc and default signal handler callbacks.
  (*self->class_init_func_)(g_class, 0);

#ifdef GLIBMM_PROPERTIES_ENABLED
  GObjectClass *const gobject_class = static_cast<GObjectClass*>(g_class);
  gobject_class->get_property = &Glib::custom_get_property_callback;
  gobject_class->set_property = &Glib::custom_set_property_callback;
#endif //GLIBMM_PROPERTIES_ENABLED
}

} // namespace Glib

