// -*- c++ -*-
#ifndef _GLIBMM_CLASS_H
#define _GLIBMM_CLASS_H

/* $Id$ */

/* Copyright 2001 Free Software Foundation
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
#include <glibmmconfig.h> //Include this here so that the /private/*.h classes have access to GLIBMM_VFUNCS_ENABLED


#ifndef DOXYGEN_SHOULD_SKIP_THIS

namespace Glib
{

class Class
{
public:
  /* No constructor/destructor:
   * Glib::Class objects are used only as static data, which would cause
   * lots of ugly global constructor invocations.  These are avoidable,
   * because the C/C++ standard explicitly specifies that all _static_ data
   * is zero-initialized at program start.
   */
  //Class();
  //~Class();

  //static void class_init_function(BaseClassType *p);
  //static void object_init_function(BaseObjectType *o);
  //GType get_type() = 0; //Creates the GType when this is first called.

  // Hook for translating API
  //static Glib::Object* wrap_new(GObject*);

  inline GType get_type() const;
  GType clone_custom_type(const char* custom_type_name) const;

protected:
  GType           gtype_;
  GClassInitFunc  class_init_func_;

  void register_derived_type(GType base_type);

private:
  static void custom_class_init_function(void* g_class, void* class_data);
};

inline
GType Class::get_type() const
{
  return gtype_;
}

} // namespace Glib

#endif /* DOXYGEN_SHOULD_SKIP_THIS */

#endif /* _GLIBMM_CLASS_H */

