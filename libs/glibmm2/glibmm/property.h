// -*- c++ -*-
#ifndef _GLIBMM_PROPERTY_H
#define _GLIBMM_PROPERTY_H
/* $Id$ */

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

#include <glibmm/propertyproxy.h>

#ifdef GLIBMM_PROPERTIES_ENABLED

#include <glibmm/value.h>

namespace Glib
{

#ifndef DOXYGEN_SHOULD_SKIP_THIS

#ifdef GLIBMM_CXX_CAN_USE_NAMESPACES_INSIDE_EXTERNC
//For the AIX xlC compiler, I can not find a way to do this without putting the functions in the global namespace. murrayc
extern "C"
{
#endif //GLIBMM_CXX_CAN_USE_NAMESPACES_INSIDE_EXTERNC

void custom_get_property_callback(GObject* object, unsigned int property_id,
                                  GValue* value, GParamSpec* param_spec);

void custom_set_property_callback(GObject* object, unsigned int property_id,
                                  const GValue* value, GParamSpec* param_spec);

#ifdef GLIBMM_CXX_CAN_USE_NAMESPACES_INSIDE_EXTERNC
} //extern "C"
#endif //GLIBMM_CXX_CAN_USE_NAMESPACES_INSIDE_EXTERNC

#endif /* DOXYGEN_SHOULD_SKIP_THIS */


class PropertyBase
{
public:
  Glib::ustring get_name() const;
  void notify();

protected:
  Glib::Object*   object_;
  Glib::ValueBase value_;
  GParamSpec*     param_spec_;

  PropertyBase(Glib::Object& object, GType value_type);
  ~PropertyBase();

  bool lookup_property(const Glib::ustring& name);
  void install_property(GParamSpec* param_spec);

  const char* get_name_internal() const;

private:
  // noncopyable
  PropertyBase(const PropertyBase&);
  PropertyBase& operator=(const PropertyBase&);

#ifndef DOXYGEN_SHOULD_SKIP_THIS

  friend void Glib::custom_get_property_callback(GObject* object, unsigned int property_id,
                                                 GValue* value, GParamSpec* param_spec);

  friend void Glib::custom_set_property_callback(GObject* object, unsigned int property_id,
                                                 const GValue* value, GParamSpec* param_spec);

#endif /* DOXYGEN_SHOULD_SKIP_THIS */
};


template <class T>
class Property : public PropertyBase
{
public:
  typedef T PropertyType;
  typedef Glib::Value<T> ValueType;

  Property(Glib::Object& object, const Glib::ustring& name);
  Property(Glib::Object& object, const Glib::ustring& name, const PropertyType& default_value);

  inline void set_value(const PropertyType& data);
  inline PropertyType get_value() const;

  inline Property<T>& operator=(const PropertyType& data);
  inline operator PropertyType() const;

  inline Glib::PropertyProxy<T> get_proxy();
};


#ifndef DOXYGEN_SHOULD_SKIP_THIS

/**** Glib::Property<T> ****************************************************/

template <class T>
Property<T>::Property(Glib::Object& object, const Glib::ustring& name)
:
  PropertyBase(object, ValueType::value_type())
{
  if(!lookup_property(name))
    install_property(static_cast<ValueType&>(value_).create_param_spec(name));
}

template <class T>
Property<T>::Property(Glib::Object& object, const Glib::ustring& name,
                      const typename Property<T>::PropertyType& default_value)
:
  PropertyBase(object, ValueType::value_type())
{
  static_cast<ValueType&>(value_).set(default_value);

  if(!lookup_property(name))
    install_property(static_cast<ValueType&>(value_).create_param_spec(name));
}

template <class T> inline
void Property<T>::set_value(const typename Property<T>::PropertyType& data)
{
  static_cast<ValueType&>(value_).set(data);
  this->notify();
}

template <class T> inline
typename Property<T>::PropertyType Property<T>::get_value() const
{
  return static_cast<const ValueType&>(value_).get();
}

template <class T> inline
Property<T>& Property<T>::operator=(const typename Property<T>::PropertyType& data)
{
  static_cast<ValueType&>(value_).set(data);
  this->notify();
  return *this;
}

template <class T> inline
Property<T>::operator T() const
{
  return static_cast<const ValueType&>(value_).get();
}

template <class T> inline
Glib::PropertyProxy<T> Property<T>::get_proxy()
{
  return Glib::PropertyProxy<T>(object_, get_name_internal());
}

#endif /* DOXYGEN_SHOULD_SKIP_THIS */

} // namespace Glib

#endif //GLIBMM_PROPERTIES_ENABLED

#endif /* _GLIBMM_PROPERTY_H */

