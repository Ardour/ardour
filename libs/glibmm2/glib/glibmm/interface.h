// -*- c++ -*-
#ifndef _GLIBMM_INTERFACE_H
#define _GLIBMM_INTERFACE_H

/* $Id: interface.h 779 2009-01-19 17:58:50Z murrayc $ */

/* Copyright 2002 The gtkmm Development Team
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free
 * Software Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#include <glibmm/object.h>


namespace Glib
{

#ifndef DOXYGEN_SHOULD_SKIP_THIS
class Interface_Class;
#endif

// There is no base GInterface struct in Glib, though there is G_TYPE_INTERFACE enum value.
class Interface : virtual public Glib::ObjectBase
{
public:
#ifndef DOXYGEN_SHOULD_SKIP_THIS
  typedef Interface       CppObjectType;
  typedef Interface_Class CppClassType;
  typedef GTypeInterface  BaseClassType;
#endif /* DOXYGEN_SHOULD_SKIP_THIS */

  /** Called by constructors of derived classes. Provide the result of 
   * the Class object's init() function to ensure that it is properly 
   * initialized.
   * 
   * @param interface_class The Class object for the derived type.
   */
  explicit Interface(const Glib::Interface_Class& interface_class);

  /** Called by constructors of derived classes.
   * 
   * @param cast_item A C instance that will be wrapped by the new 
   * C++ instance. This does not take a reference, so call reference() 
   * if necessary.
   */
  explicit Interface(GObject* castitem);
  virtual ~Interface();

  //void add_interface(GType gtype_implementer);

  // Hook for translating API
  //static Glib::Interface* wrap_new(GTypeInterface*);

#ifndef DOXYGEN_SHOULD_SKIP_THIS
  static GType get_type()      G_GNUC_CONST;
  static GType get_base_type() G_GNUC_CONST;
#endif

  inline GObject* gobj()             { return gobject_; }
  inline const GObject* gobj() const { return gobject_; }

private:
  // noncopyable
  Interface(const Interface&);
  Interface& operator=(const Interface&);
};

RefPtr<ObjectBase> wrap_interface(GObject* object, bool take_copy = false);

} // namespace Glib

#endif /* _GLIBMM_INTERFACE_H */

