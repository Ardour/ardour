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

#ifndef DOXYGEN_SHOULD_SKIP_THIS
#ifndef _GLIBMM_VALUE_H_INCLUDE_VALUE_CUSTOM_H
#error "glibmm/value_custom.h cannot be included directly"
#endif
#endif

#include <new>
#include <typeinfo>
#include <glibmmconfig.h>

GLIBMM_USING_STD(nothrow)


namespace Glib
{

#ifndef DOXYGEN_SHOULD_SKIP_THIS

extern "C"
{
  typedef void (* ValueInitFunc) (GValue*);
  typedef void (* ValueFreeFunc) (GValue*);
  typedef void (* ValueCopyFunc) (const GValue*, GValue*);
}

/* When using Glib::Value<T> with custom types, each T will be registered
 * as subtype of G_TYPE_BOXED, via this function.  The type_name argument
 * should be the C++ RTTI name.
 */
GType custom_boxed_type_register(const char*   type_name,
                                 ValueInitFunc init_func,
                                 ValueFreeFunc free_func,
                                 ValueCopyFunc copy_func);

/* When using Glib::Value<T*> or Glib::Value<const T*> with custom types,
 * each T* or const T* will be registered as a subtype of G_TYPE_POINTER,
 * via this function.  The type_name argument should be the C++ RTTI name.
 */
GType custom_pointer_type_register(const char* type_name);

#endif /* DOXYGEN_SHOULD_SKIP_THIS */


/**
 * @ingroup glibmmValue
 */
template <class T, class PtrT>
class Value_Pointer : public ValueBase_Object
{
public:
  typedef PtrT  CppType;
  typedef void* CType;

  static inline GType value_type() G_GNUC_CONST;

  inline void set(CppType data);
  inline CppType get() const;

private:
  inline
  static GType value_type_(Glib::Object*);
  static GType value_type_(void*);

  inline void set_(CppType data, Glib::Object*);
  inline void set_(CppType data, void*);

  inline CppType get_(Glib::Object*) const;
  inline CppType get_(void*) const;
};

  
/** Generic value implementation for custom types.
 * @ingroup glibmmValue
 * Any type to be used with this template must implement:
 * - default constructor
 * - copy constructor
 * - assignment operator
 * - destructor
 *
 * Compiler-generated implementations are OK, provided they do the
 * right thing for the type.  In other words, any type that works with
 * <tt>std::vector</tt> will work with Glib::Value<>.
 *
 * @note None of the operations listed above are allowed to throw.  If you
 * cannot ensure that no exceptions will be thrown, consider using either
 * a normal pointer or a smart pointer to hold your objects indirectly.
 */
template <class T>
class Value : public ValueBase_Boxed
{
public:
  typedef T  CppType;
  typedef T* CType;

  static GType value_type() G_GNUC_CONST;

  inline void set(const CppType& data);
  inline CppType get() const;

private:
  static GType custom_type_;

  static void value_init_func(GValue* value);
  static void value_free_func(GValue* value);
  static void value_copy_func(const GValue* src_value, GValue* dest_value);
};


/** Specialization for pointers to instances of any type.
 * @ingroup glibmmValue
 * No attempt is made to manage the memory associated with the
 * pointer, you must take care of that yourself.
 */
template <class T>
class Value<T*> : public Value_Pointer<T,T*>
{};

/** Specialization for pointers to const instances of any type.
 * @ingroup glibmmValue
 * No attempt is made to manage the memory associated with the
 * pointer, you must take care of that yourself.
 */
template <class T>
class Value<const T*> : public Value_Pointer<T,const T*>
{};


#ifndef DOXYGEN_SHOULD_SKIP_THIS

/**** Glib::Value_Pointer<T, PtrT> *****************************************/

/** Implementation for Glib::Object pointers **/

// static
template <class T, class PtrT> inline
GType Value_Pointer<T,PtrT>::value_type_(Glib::Object*)
{
  return T::get_base_type();
}

template <class T, class PtrT> inline
void Value_Pointer<T,PtrT>::set_(PtrT data, Glib::Object*)
{
  set_object(const_cast<T*>(data));
}

//More spec-compliant compilers (such as Tru64) need this to be near Glib::Object instead.
#ifdef GLIBMM_CAN_USE_DYNAMIC_CAST_IN_UNUSED_TEMPLATE_WITHOUT_DEFINITION
template <class T, class PtrT> inline
PtrT Value_Pointer<T,PtrT>::get_(Glib::Object*) const
{
  return dynamic_cast<T*>(get_object());
}
#endif //GLIBMM_CAN_USE_DYNAMIC_CAST_IN_UNUSED_TEMPLATE_WITHOUT_DEFINITION

/** Implementation for custom pointers **/

// static
template <class T, class PtrT>
GType Value_Pointer<T,PtrT>::value_type_(void*)
{
  static GType custom_type = 0;

  if(!custom_type)
    custom_type = Glib::custom_pointer_type_register(typeid(PtrT).name());

  return custom_type;
}

template <class T, class PtrT> inline
void Value_Pointer<T,PtrT>::set_(PtrT data, void*)
{
  gobject_.data[0].v_pointer = const_cast<T*>(data);
}

template <class T, class PtrT> inline
PtrT Value_Pointer<T,PtrT>::get_(void*) const
{
  return static_cast<T*>(gobject_.data[0].v_pointer);
}

/** Public forwarding interface **/

// static
template <class T, class PtrT> inline
GType Value_Pointer<T,PtrT>::value_type()
{
  // Dispatch to the specific value_type_() overload.
  return Value_Pointer<T,PtrT>::value_type_(static_cast<T*>(0));
}

template <class T, class PtrT> inline
void Value_Pointer<T,PtrT>::set(PtrT data)
{
  // Dispatch to the specific set_() overload.
  this->set_(data, static_cast<T*>(0));
}

template <class T, class PtrT> inline
PtrT Value_Pointer<T,PtrT>::get() const
{
  // Dispatch to the specific get_() overload.
  return this->get_(static_cast<T*>(0));
}


/**** Glib::Value<T> *******************************************************/

// Static data, specific to each template instantiation.
template <class T>
GType Value<T>::custom_type_ = 0;

template <class T> inline
void Value<T>::set(const typename Value<T>::CppType& data)
{
  // Assume the value is already default-initialized.  See value_init_func().
  *static_cast<T*>(gobject_.data[0].v_pointer) = data;
}

template <class T> inline
typename Value<T>::CppType Value<T>::get() const
{
  // Assume the pointer is not NULL.  See value_init_func().
  return *static_cast<T*>(gobject_.data[0].v_pointer);
}

// static
template <class T>
GType Value<T>::value_type()
{
  if(!custom_type_)
  {
    custom_type_ = Glib::custom_boxed_type_register(
        typeid(CppType).name(),
        &Value<T>::value_init_func,
        &Value<T>::value_free_func,
        &Value<T>::value_copy_func);
  }
  return custom_type_;
}

// static
template <class T>
void Value<T>::value_init_func(GValue* value)
{
  // Never store a NULL pointer (unless we're out of memory).
  value->data[0].v_pointer = new(std::nothrow) T();
}

// static
template <class T>
void Value<T>::value_free_func(GValue* value)
{
  delete static_cast<T*>(value->data[0].v_pointer);
}

// static
template <class T>
void Value<T>::value_copy_func(const GValue* src_value, GValue* dest_value)
{
  // Assume the source is not NULL.  See value_init_func().
  const T& source = *static_cast<T*>(src_value->data[0].v_pointer);
  dest_value->data[0].v_pointer = new(std::nothrow) T(source);
}

#endif /* DOXYGEN_SHOULD_SKIP_THIS */

} // namespace Glib

