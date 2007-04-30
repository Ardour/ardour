// -*- c++ -*-
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

#include <glibmm/property.h>

#ifdef GLIBMM_PROPERTIES_ENABLED

#include <glibmm/object.h>
#include <cstddef>

// Temporary hack till GLib gets fixed.
#undef  G_STRLOC
#define G_STRLOC __FILE__ ":" G_STRINGIFY(__LINE__)


namespace
{

// OK guys, please don't kill me for that.  Let me explain what happens here.
//
// The task:
// ---------
// a) Autogenerate a property ID number for each custom property.  This is an
//    unsigned integer, which doesn't have to be assigned continuously.  I.e.,
//    it can be everything but 0.
// b) If more than one object of the same class is instantiated, then of course
//    the already installed properties must be used.  That means, a property ID
//    must not be associated with a single Glib::Property<> instance.  Rather,
//    the ID has to be associated with the class somehow.
// c) With only a GObject pointer and a property ID (and perhaps GParamSpec*
//    if necessary), it must be possible to acquire a reference to the property
//    wrapper instance.
//
// The current solution:
// ---------------------
// a) Assign an ID to a Glib::PropertyBase by calculating its offset in bytes
//    relative to the beginning of the object's memory.  dynamic_cast<void*>
//    is used to retrieve a pointer to the very beginning of an instance.
// b) Recalculate a specific PropertyBase pointer by adding the property ID
//    (i.e. the byte offset) to the object start pointer.  The result is then
//    just casted to PropertyBase*.
//
// Drawbacks:
// ----------
// a) It's a low-level hack.  Should be portable, yes, but we can only do very
//    limited error checking.
// b) All Glib::Property<> instances are absolutely required to be direct data
//    members of the class that implements the property.  That seems a natural
//    thing to do, but it's questionable whether it should be a requirement.
//
// Advantages:
// -----------
// a) Although low-level, it's extremely easy to implement.  The nasty code is
//    concentrated in only two non-exposed utility functions, and it works
//    just fine.
// b) It's efficient, and the memory footprint is very small too.
// c) I actually tried other ways, too, but ran into dead-ends everywhere.
//    It's probably possible to implement this without calculating offsets,
//    but it'll be very complicated, and involve a lot of qdata pointers to
//    property tables andwhatnot.
//
// We can reimplement this later if necessary.

static unsigned int property_to_id(Glib::ObjectBase& object, Glib::PropertyBase& property)
{
  void *const base_ptr = dynamic_cast<void*>(&object);
  void *const prop_ptr = &property;

  const ptrdiff_t offset = static_cast<guint8*>(prop_ptr) - static_cast<guint8*>(base_ptr);

  g_return_val_if_fail(offset > 0 && offset < G_MAXINT, 0);

  return static_cast<unsigned int>(offset);
}

Glib::PropertyBase& property_from_id(Glib::ObjectBase& object, unsigned int property_id)
{
  void *const base_ptr = dynamic_cast<void*>(&object);
  void *const prop_ptr = static_cast<guint8*>(base_ptr) + property_id;

  return *static_cast<Glib::PropertyBase*>(prop_ptr);
}

} // anonymous namespace


namespace Glib
{

void custom_get_property_callback(GObject* object, unsigned int property_id,
                                  GValue* value, GParamSpec* param_spec)
{
  if(Glib::ObjectBase *const wrapper = Glib::ObjectBase::_get_current_wrapper(object))
  {
    PropertyBase& property = property_from_id(*wrapper, property_id);

    if((property.object_ == wrapper) && (property.param_spec_ == param_spec))
      g_value_copy(property.value_.gobj(), value);
    else
      G_OBJECT_WARN_INVALID_PROPERTY_ID(object, property_id, param_spec);
  }
}

void custom_set_property_callback(GObject* object, unsigned int property_id,
                                  const GValue* value, GParamSpec* param_spec)
{
  if(Glib::ObjectBase *const wrapper = Glib::ObjectBase::_get_current_wrapper(object))
  {
    PropertyBase& property = property_from_id(*wrapper, property_id);

    if((property.object_ == wrapper) && (property.param_spec_ == param_spec))
    {
      g_value_copy(value, property.value_.gobj());
      g_object_notify(object, g_param_spec_get_name(param_spec));
    }
    else
      G_OBJECT_WARN_INVALID_PROPERTY_ID(object, property_id, param_spec);
  }
}


/**** Glib::PropertyBase ***************************************************/

PropertyBase::PropertyBase(Glib::Object& object, GType value_type)
:
  object_     (&object),
  value_      (),
  param_spec_ (0)
{
  value_.init(value_type);
}

PropertyBase::~PropertyBase()
{
  if(param_spec_)
    g_param_spec_unref(param_spec_);
}

bool PropertyBase::lookup_property(const Glib::ustring& name)
{
  g_assert(param_spec_ == 0);

  param_spec_ = g_object_class_find_property(G_OBJECT_GET_CLASS(object_->gobj()), name.c_str());

  if(param_spec_)
  {
    g_assert(G_PARAM_SPEC_VALUE_TYPE(param_spec_) == G_VALUE_TYPE(value_.gobj()));
    g_param_spec_ref(param_spec_);
  }

  return (param_spec_ != 0);
}

void PropertyBase::install_property(GParamSpec* param_spec)
{
  g_return_if_fail(param_spec != 0);

  const unsigned int property_id = property_to_id(*object_, *this);

  g_object_class_install_property(G_OBJECT_GET_CLASS(object_->gobj()), property_id, param_spec);

  param_spec_ = param_spec;
  g_param_spec_ref(param_spec_);
}

const char* PropertyBase::get_name_internal() const
{
  const char *const name = g_param_spec_get_name(param_spec_);
  g_return_val_if_fail(name != 0, "");
  return name;
}

Glib::ustring PropertyBase::get_name() const
{
  return Glib::ustring(get_name_internal());
}

void PropertyBase::notify()
{
  g_object_notify(object_->gobj(), g_param_spec_get_name(param_spec_));
}

} // namespace Glib

#endif //GLIBMM_PROPERTIES_ENABLED

