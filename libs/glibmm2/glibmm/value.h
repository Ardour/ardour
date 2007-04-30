// -*- c++ -*-
#ifndef _GLIBMM_VALUE_H
#define _GLIBMM_VALUE_H
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

#include <glib-object.h>
#include <glibmm/refptr.h>
#include <glibmm/ustring.h>


namespace Glib
{

class ObjectBase;
class Object;

/** @defgroup glibmmValue Generic Values
 *
 * Glib::Value<> is specialized for almost any type used within
 * the glibmm and gtkmm libraries.
 *
 * - Basic types like <tt>int</tt>, <tt>char</tt>, <tt>bool</tt>, etc., also <tt>void*</tt>.
 * - Glib::ustring and std::string.
 * - Pointers to classes derived from Glib::Object.
 * - Glib::RefPtr<> pointer types, which are assumed to be Glib::Object pointers.
 * - All flags and enum types used within the gtkmm libraries.
 *
 * If a type doesn't fit into any of these categories, then a generic
 * implementation for custom types will be used.  The requirements imposed
 * on custom types are described in the Glib::Value class documentation.
 */

/**
 * @ingroup glibmmValue
 */
class ValueBase
{
public:
  /** Initializes the GValue, but without a type.  You have to
   * call init() before using the set(), get(), or reset() methods.
   */
  ValueBase();

  ValueBase(const ValueBase& other);
  ValueBase& operator=(const ValueBase& other);

  ~ValueBase();

  /** Setup the GValue for storing the specified @a type.
   * The contents will be initialized to the default value for this type.
   * Note that init() should never be called twice.
   *
   * init() is not implemented as constructor, to avoid the necessity
   * to implement a forward constructor in each derived class.
   *
   * @param type The type that the Value should hold.
   */
  void init(GType type);

  /** Setup the GValue storing the type and value of the specified @a value.
   * Note that init() should never be called twice.
   *
   * init() is not implemented as constructor, to avoid the necessity
   * to implement a forward constructor in each derived class.
   *
   * @param value The existing GValue.
   */
  void init(const GValue* value);

  /** Reset contents to the default value of its type.
   */
  void reset();

  GValue*       gobj()       { return &gobject_; }
  const GValue* gobj() const { return &gobject_; }

protected:
  GValue gobject_;
};

/**
 * @ingroup glibmmValue
 */
class ValueBase_Boxed : public ValueBase
{
public:
  static GType value_type() G_GNUC_CONST;

#ifndef DOXYGEN_SHOULD_SKIP_THIS
  GParamSpec* create_param_spec(const Glib::ustring& name) const;
#endif

protected:
  void  set_boxed(const void* data);
  void* get_boxed() const; // doesn't copy  
};


/**
 * @ingroup glibmmValue
 */
class ValueBase_Object : public ValueBase
{
public:
  static GType value_type() G_GNUC_CONST;

#ifndef DOXYGEN_SHOULD_SKIP_THIS
  GParamSpec* create_param_spec(const Glib::ustring& name) const;
#endif

protected:
  void set_object(Glib::ObjectBase* data);
  Glib::ObjectBase* get_object() const;
  Glib::RefPtr<Glib::ObjectBase> get_object_copy() const;
};


/**
 * @ingroup glibmmValue
 */
class ValueBase_Enum : public ValueBase
{
public:
  typedef gint CType;
  static GType value_type() G_GNUC_CONST;

#ifndef DOXYGEN_SHOULD_SKIP_THIS
  GParamSpec* create_param_spec(const Glib::ustring& name) const;
#endif

protected:
  void set_enum(int data);
  int  get_enum() const;
};


/**
 * @ingroup glibmmValue
 */
class ValueBase_Flags : public ValueBase
{
public:
  typedef guint CType;
  static GType value_type() G_GNUC_CONST;

#ifndef DOXYGEN_SHOULD_SKIP_THIS
  GParamSpec* create_param_spec(const Glib::ustring& name) const;
#endif

protected:
  void set_flags(unsigned int data);
  unsigned int get_flags() const;
};


/**
 * @ingroup glibmmValue
 */
class ValueBase_String : public ValueBase
{
public:
  typedef const gchar* CType;
  static GType value_type() G_GNUC_CONST;

#ifndef DOXYGEN_SHOULD_SKIP_THIS
  GParamSpec* create_param_spec(const Glib::ustring& name) const;
#endif

protected:
  void set_cstring(const char* data);
  const char* get_cstring() const; // never returns 0
};

} // namespace Glib


/* Include generic Glib::Value<> template, before any specializations:
 */
#define _GLIBMM_VALUE_H_INCLUDE_VALUE_CUSTOM_H
#include <glibmm/value_custom.h>
#undef _GLIBMM_VALUE_H_INCLUDE_VALUE_CUSTOM_H


namespace Glib
{

/**
 * @ingroup glibmmValue
 */
template <class T>
class Value_Boxed : public ValueBase_Boxed
{
public:
  typedef T                           CppType;
  typedef typename T::BaseObjectType* CType;

  static GType value_type() { return T::get_type(); }

  void set(const CppType& data) { set_boxed(data.gobj()); }
  CppType get() const           { return CppType(static_cast<CType>(get_boxed())); }
};

//More spec-compliant compilers (such as Tru64) need this to be near Glib::Object instead.
#ifdef GLIBMM_CAN_USE_DYNAMIC_CAST_IN_UNUSED_TEMPLATE_WITHOUT_DEFINITION

/** Partial specialization for RefPtr<> to Glib::Object.
 * @ingroup glibmmValue
 */
template <class T>
class Value< Glib::RefPtr<T> > : public ValueBase_Object
{
public:
  typedef Glib::RefPtr<T>             CppType;
  typedef typename T::BaseObjectType* CType;

  static GType value_type() { return T::get_base_type(); }

  void set(const CppType& data) { set_object(data.operator->()); }
  CppType get() const           { return Glib::RefPtr<T>::cast_dynamic(get_object_copy()); }
};

//The SUN Forte Compiler has a problem with this: 
#ifdef GLIBMM_HAVE_DISAMBIGUOUS_CONST_TEMPLATE_SPECIALIZATIONS

/** Partial specialization for RefPtr<> to const Glib::Object.
 * @ingroup glibmmValue
 */
template <class T>
class Value< Glib::RefPtr<const T> > : public ValueBase_Object
{
public:
  typedef Glib::RefPtr<const T>       CppType;
  typedef typename T::BaseObjectType* CType;

  static GType value_type() { return T::get_base_type(); }

  void set(const CppType& data) { set_object(const_cast<T*>(data.operator->())); }
  CppType get() const           { return Glib::RefPtr<T>::cast_dynamic(get_object_copy()); }
};
#endif //GLIBMM_HAVE_DISAMBIGUOUS_CONST_TEMPLATE_SPECIALIZATIONS

#endif //GLIBMM_CAN_USE_DYNAMIC_CAST_IN_UNUSED_TEMPLATE_WITHOUT_DEFINITION

} // namespace Glib


/* Include generated specializations of Glib::Value<> for fundamental types:
 */
#define _GLIBMM_VALUE_H_INCLUDE_VALUE_BASICTYPES_H
#include <glibmm/value_basictypes.h>
#undef _GLIBMM_VALUE_H_INCLUDE_VALUE_BASICTYPES_H


namespace Glib
{

/** Specialization for strings.
 * @ingroup glibmmValue
 */
template <>
class Value<std::string> : public ValueBase_String
{
public:
  typedef std::string CppType;

  void set(const std::string& data);
  std::string get() const { return get_cstring(); }
};

/** Specialization for UTF-8 strings.
 * @ingroup glibmmValue
 */
template <>
class Value<Glib::ustring> : public ValueBase_String
{
public:
  typedef Glib::ustring CppType;

  void set(const Glib::ustring& data);
  Glib::ustring get() const { return get_cstring(); }
};


/** Base class of Glib::Value<T> specializations for enum types.
 * @ingroup glibmmValue
 */
template <class T>
class Value_Enum : public ValueBase_Enum
{
public:
  typedef T CppType;

  void set(CppType data) { set_enum(data); }
  CppType get() const    { return CppType(get_enum()); }
};

/** Base class of Glib::Value<T> specializations for flags types.
 * @ingroup glibmmValue
 */
template <class T>
class Value_Flags : public ValueBase_Flags
{
public:
  typedef T CppType;

  void set(CppType data) { set_flags(data); }
  CppType get() const    { return CppType(get_flags()); }
};

} // namespace Glib


#endif /* _GLIBMM_VALUE_H */

