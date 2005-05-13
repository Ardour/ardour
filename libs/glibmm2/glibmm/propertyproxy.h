// -*- c++ -*-
#ifndef _GLIBMM_PROPERTYPROXY_H
#define _GLIBMM_PROPERTYPROXY_H
/* $Id$ */

/* propertyproxy.h
 *
 * Copyright 2002 The gtkmm Development Team
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

#include <glibmm/propertyproxy_base.h>


namespace Glib
{

/** A PropertyProxy can be used to get and set the value of an object's property.
 * There are usually also get and set methods on the class itself, which you might find more convenient.
 * With the PropertyProxy, you may use either get_value() and set_value(), or operator=() and
 * operator PropertyType(), like so:
 * @code
 * int height = cellrenderer.property_height();
 * cellrenderer.property_editable() = true;
 * @endcode
 *
 * You may also receive notification when a property's value changes, by connecting to signal_changed().
 */
template <class T>
class PropertyProxy : public PropertyProxy_Base
{
public:
  typedef T PropertyType;

  PropertyProxy(ObjectBase* obj, const char* name)
    : PropertyProxy_Base(obj, name) {}

  /** Set the value of this property.
   * @param data The new value for the property.
   */
  void set_value(const PropertyType& data);

  /** Get the value of this property.
   * @result The current value of the property.
   */
  PropertyType get_value() const;

  /** Set the value of this property back to its default value
   */
  void reset_value()
    { reset_property_(); }

  PropertyProxy<T>& operator=(const PropertyType& data)
    { this->set_value(data); return *this; }

  operator PropertyType() const
    { return this->get_value(); }
};


/** See PropertyProxy().
 * This property can be written, but not read, so there is no get_value() method.
 */
template <class T>
class PropertyProxy_WriteOnly : public PropertyProxy_Base
{
public:
  typedef T PropertyType;

  PropertyProxy_WriteOnly(ObjectBase* obj, const char* name)
    : PropertyProxy_Base(obj, name) {}

  /** Set the value of this property.
   * @param data The new value for the property.
   */
  void set_value(const PropertyType& data)
    {
      PropertyProxy_Base& base = *this;
      // The downcast to PropertyProxy<T> is safe, and avoids code duplication.
      static_cast<PropertyProxy<T>&>(base).set_value(data);
    }

  /** Set the value of this property back to its default value
   */
  void reset_value()
    { reset_property_(); }

  PropertyProxy_WriteOnly<T>& operator=(const PropertyType& data)
    { this->set_value(data); return *this; }
};

/** See PropertyProxy().
 * This property can be read, but not written, so there is no set_value() method.
 */
template <class T>
class PropertyProxy_ReadOnly : public PropertyProxy_Base
{
public:
  typedef T PropertyType;

  //obj is const, because this should be returned by const accessors.
  PropertyProxy_ReadOnly(const ObjectBase* obj, const char* name)
    : PropertyProxy_Base(const_cast<ObjectBase*>(obj), name) {}

  /** Get the value of this property.
   * @result The current value of the property.
   */
  PropertyType get_value() const
    {
      const PropertyProxy_Base& base = *this;
      // The downcast to PropertyProxy<T> is safe, and avoids code duplication.
      return static_cast<const PropertyProxy<T>&>(base).get_value();
    }

  operator PropertyType() const
    { return this->get_value(); }
};


/**** Template Implementation **********************************************/

#ifndef DOXYGEN_SHOULD_SKIP_THIS

template <class T>
void PropertyProxy<T>::set_value(const T& data)
{
  Glib::Value<T> value;
  value.init(Glib::Value<T>::value_type());

  value.set(data);
  set_property_(value);
}

template <class T>
T PropertyProxy<T>::get_value() const
{
  Glib::Value<T> value;
  value.init(Glib::Value<T>::value_type());

  get_property_(value);
  return value.get();
}

#endif /* DOXYGEN_SHOULD_SKIP_THIS */

} // namespace Glib


#endif /* _GLIBMM_PROPERTYPROXY_H */

