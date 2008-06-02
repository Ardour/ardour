// -*- c++ -*-
/* $Id: value_custom.cc 260 2006-04-12 08:12:12Z murrayc $ */

/* Copyright 2002 The gtkmm Development Team
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

#include <glibmm/value.h>
#include <glibmm/utility.h>
#include <glib.h>


namespace
{

static void warn_already_registered(const char* location, const std::string& full_name)
{
  g_warning("file %s: (%s): The type name `%s' has been registered already.\n"
            "This is not supposed to happen -- please send a mail with detailed "
            "information about your platform to gtkmm-list@gnome.org.  Thanks.\n",
            __FILE__, location, full_name.c_str());
}

} // anonymous namespace


namespace Glib
{

GType custom_boxed_type_register(const char*   type_name,
                                 ValueInitFunc init_func,
                                 ValueFreeFunc free_func,
                                 ValueCopyFunc copy_func)
{
  std::string full_name ("glibmm__CustomBoxed_");
  Glib::append_canonical_typename(full_name, type_name);

  // Templates of the same type _might_ be duplicated when instantiated in
  // multiple translation units -- I'm not sure whether this is true.  If the
  // static custom_type_ variable in Value<> is duplicated, then the type
  // would be registered more than once.
  //
  // Lookup the type name to see whether this scenario actually happens.
  // If this turns out to be common behaviour on some platform the warning
  // should be removed.

  if(const GType existing_type = g_type_from_name(full_name.c_str()))
  {
    warn_already_registered("Glib::custom_boxed_type_register", full_name);
    return existing_type;
  }

  // Via GTypeValueTable, we can teach GValue how to instantiate,
  // destroy, and copy arbitrary objects of the C++ type.

  const GTypeValueTable value_table =
  {
    init_func,
    free_func,
    copy_func,
    0, // value_peek_pointer
    0, // collect_format
    0, // collect_value
    0, // lcopy_format
    0, // lcopy_value
  };

  const GTypeInfo type_info =
  {
    0, // class_size
    0, // base_init
    0, // base_finalize
    0, // class_init_func
    0, // class_finalize
    0, // class_data
    0, // instance_size
    0, // n_preallocs
    0, // instance_init
    &value_table,
  };

  // Don't use g_boxed_type_register_static(), because that wouldn't allow
  // for a non-NULL default value.  The implementation of g_boxed_copy() will
  // use our custom GTypeValueTable automatically.

  return g_type_register_static(G_TYPE_BOXED, full_name.c_str(), &type_info, GTypeFlags(0));
}

GType custom_pointer_type_register(const char* type_name)
{
  std::string full_name ("glibmm__CustomPointer_");
  Glib::append_canonical_typename(full_name, type_name);

  // Templates of the same type _might_ be duplicated when instantiated in
  // multiple translation units -- I'm not sure whether this is true.  If the
  // static custom_type variable in Value<>::value_type_() is duplicated, then
  // the type would be registered more than once.
  //
  // Lookup the type name to see whether this scenario actually happens.
  // If this turns out to be common behaviour on some platform the warning
  // should be removed.

  if(const GType existing_type = g_type_from_name(full_name.c_str()))
  {
    warn_already_registered("Glib::custom_pointer_type_register", full_name);
    return existing_type;
  }

  const GTypeInfo type_info =
  {
    0, // class_size
    0, // base_init
    0, // base_finalize
    0, // class_init_func
    0, // class_finalize
    0, // class_data
    0, // instance_size
    0, // n_preallocs
    0, // instance_init
    0, // value_table
  };

  // We could probably use g_pointer_type_register_static(), but I want
  // to keep this function symmetric to custom_boxed_type_register().  Also,
  // g_pointer_type_register_static() would lookup the type name once again.

  return g_type_register_static(G_TYPE_POINTER, full_name.c_str(), &type_info, GTypeFlags(0));
}


} // namespace Glib

