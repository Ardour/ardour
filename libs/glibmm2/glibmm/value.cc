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

#include <glibmm/value.h>
#include <glibmm/objectbase.h>
#include <glibmm/utility.h>
#include <glibmm/wrap.h>


namespace Glib
{

/**** Glib::ValueBase ******************************************************/

ValueBase::ValueBase()
{
  GLIBMM_INITIALIZE_STRUCT(gobject_, GValue);
}

void ValueBase::init(GType type)
{
  g_value_init(&gobject_, type);
}

void ValueBase::init(const GValue* value)
{
  g_value_init(&gobject_, G_VALUE_TYPE(value));

  if(value)
    g_value_copy(value, &gobject_);
}

ValueBase::ValueBase(const ValueBase& other)
{
  GLIBMM_INITIALIZE_STRUCT(gobject_, GValue);

  g_value_init(&gobject_, G_VALUE_TYPE(&other.gobject_));
  g_value_copy(&other.gobject_, &gobject_);
}

ValueBase& ValueBase::operator=(const ValueBase& other)
{
  // g_value_copy() prevents self-assignment and deletes the destination.
  g_value_copy(&other.gobject_, &gobject_);
  return *this;
}

ValueBase::~ValueBase()
{
  g_value_unset(&gobject_);
}

void ValueBase::reset()
{
  g_value_reset(&gobject_);
}


/**** Glib::ValueBase_Boxed ************************************************/

// static
GType ValueBase_Boxed::value_type()
{
  return G_TYPE_BOXED;
}

void ValueBase_Boxed::set_boxed(const void* data)
{
  g_value_set_boxed(&gobject_, data);
}

void* ValueBase_Boxed::get_boxed() const
{
  return g_value_get_boxed(&gobject_);
}

GParamSpec* ValueBase_Boxed::create_param_spec(const Glib::ustring& name) const
{
  return g_param_spec_boxed(
      name.c_str(), 0, 0, G_VALUE_TYPE(&gobject_),
      GParamFlags(G_PARAM_READABLE | G_PARAM_WRITABLE));
}


/**** Glib::ValueBase_Object ***********************************************/

// static
GType ValueBase_Object::value_type()
{
  return G_TYPE_OBJECT;
}

void ValueBase_Object::set_object(Glib::ObjectBase* data)
{
  g_value_set_object(&gobject_, (data) ? data->gobj() : 0);
}

Glib::ObjectBase* ValueBase_Object::get_object() const
{
  GObject *const data = static_cast<GObject*>(g_value_get_object(&gobject_));
  return Glib::wrap_auto(data, false);
}

Glib::RefPtr<Glib::ObjectBase> ValueBase_Object::get_object_copy() const
{
  GObject *const data = static_cast<GObject*>(g_value_get_object(&gobject_));
  return Glib::RefPtr<Glib::ObjectBase>(Glib::wrap_auto(data, true));
}

GParamSpec* ValueBase_Object::create_param_spec(const Glib::ustring& name) const
{
  // Glib::Value_Pointer<> derives from Glib::ValueBase_Object, because
  // we don't know beforehand whether a certain type is derived from
  // Glib::Object or not.  To keep create_param_spec() out of the template
  // struggle, we dispatch here at runtime.

  if(G_VALUE_HOLDS_OBJECT(&gobject_))
  {
    return g_param_spec_object(
        name.c_str(), 0, 0, G_VALUE_TYPE(&gobject_),
        GParamFlags(G_PARAM_READABLE | G_PARAM_WRITABLE));
  }
  else
  {
    g_return_val_if_fail(G_VALUE_HOLDS_POINTER(&gobject_), 0);

    return g_param_spec_pointer(
        name.c_str(), 0, 0,
        GParamFlags(G_PARAM_READABLE | G_PARAM_WRITABLE));
  }
}


/**** Glib::ValueBase_Enum *************************************************/

// static
GType ValueBase_Enum::value_type()
{
  return G_TYPE_ENUM;
}

void ValueBase_Enum::set_enum(int data)
{
  g_value_set_enum(&gobject_, data);
}

int ValueBase_Enum::get_enum() const
{
  return g_value_get_enum(&gobject_);
}

GParamSpec* ValueBase_Enum::create_param_spec(const Glib::ustring& name) const
{
  return g_param_spec_enum(
      name.c_str(), 0, 0,
      G_VALUE_TYPE(&gobject_), g_value_get_enum(&gobject_),
      GParamFlags(G_PARAM_READABLE | G_PARAM_WRITABLE));
}


/**** Glib::ValueBase_Flags ************************************************/

// static
GType ValueBase_Flags::value_type()
{
  return G_TYPE_FLAGS;
}

void ValueBase_Flags::set_flags(unsigned int data)
{
  g_value_set_flags(&gobject_, data);
}

unsigned int ValueBase_Flags::get_flags() const
{
  return g_value_get_flags(&gobject_);
}

GParamSpec* ValueBase_Flags::create_param_spec(const Glib::ustring& name) const
{
  return g_param_spec_flags(
      name.c_str(), 0, 0,
      G_VALUE_TYPE(&gobject_), g_value_get_flags(&gobject_),
      GParamFlags(G_PARAM_READABLE | G_PARAM_WRITABLE));
}


/**** Glib::ValueBase_String ***********************************************/

// static
GType ValueBase_String::value_type()
{
  return G_TYPE_STRING;
}

void ValueBase_String::set_cstring(const char* data)
{
  g_value_set_string(&gobject_, data);
}

const char* ValueBase_String::get_cstring() const
{
  if(const char *const data = g_value_get_string(&gobject_))
    return data;
  else
    return "";
}

GParamSpec* ValueBase_String::create_param_spec(const Glib::ustring& name) const
{
  return g_param_spec_string(
      name.c_str(), 0, 0, get_cstring(),
      GParamFlags(G_PARAM_READABLE | G_PARAM_WRITABLE));
}


/**** Glib::Value<std::string> *********************************************/

void Value<std::string>::set(const std::string& data)
{
  g_value_set_string(&gobject_, data.c_str());
}


/**** Glib::Value<Glib::ustring> *******************************************/

void Value<Glib::ustring>::set(const Glib::ustring& data)
{
  g_value_set_string(&gobject_, data.c_str());
}

} // namespace Glib

