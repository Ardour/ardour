// -*- c++ -*-
#ifndef _GLIBMM_OBJECT_H
#define _GLIBMM_OBJECT_H
/* $Id: object.h,v 1.14 2006/06/19 20:43:42 murrayc Exp $ */

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

//X11 defines DestroyNotify and some other non-prefixed stuff, and it's too late to change that now,
//so let's give people a clue about the compilation errors that they will see:
#ifdef DestroyNotify
  #error "X11/Xlib.h seems to have been included before this header. Due to some commonly-named macros in X11/Xlib.h, it may only be included after any glibmm, gdkmm, or gtkmm headers."
#endif //DestroyNotify

#include <glibmm/objectbase.h>
#include <glibmm/wrap.h>
#include <glibmm/quark.h>
#include <glibmm/refptr.h>
#include <glibmm/utility.h> /* Could be private, but that would be tedious. */
#include <glibmm/containerhandle_shared.h> //Because its specializations may be here.
#include <glibmm/value.h>

#include <glibmmconfig.h>

#ifndef DOXYGEN_SHOULD_SKIP_THIS
extern "C"
{
typedef struct _GObject GObject;
typedef struct _GObjectClass GObjectClass;
}
#endif /* DOXYGEN_SHOULD_SKIP_THIS */


namespace Glib
{

#ifndef DOXYGEN_SHOULD_SKIP_THIS

class Class;
class Object_Class;
class GSigConnectionNode;

/* ConstructParams::ConstructParams() takes a varargs list of properties
 * and values, like g_object_new() does.  This list will then be converted
 * to a GParameter array, for use with g_object_newv().  No overhead is
 * involved, since g_object_new() is just a wrapper around g_object_newv()
 * as well.
 *
 * The advantage of an auxilary ConstructParams object over g_object_new()
 * is that the actual construction is always done in the Glib::Object ctor.
 * This allows for neat tricks like easy creation of derived custom types,
 * without adding special support to each ctor of every class.
 *
 * The comments in object.cc and objectbase.cc should explain in detail
 * how this works.
 */
class ConstructParams
{
public:
  const Glib::Class&  glibmm_class;
  unsigned int        n_parameters;
  GParameter*         parameters;

  explicit ConstructParams(const Glib::Class& glibmm_class_);
  ConstructParams(const Glib::Class& glibmm_class_, const char* first_property_name, ...);
  ~ConstructParams();

  // This is only used by the C++ compiler (since g++ 3.4) to create temporary instances.
  // Apparently the compiler will actually optimize away the use of this.
  // See bug #132300.
  ConstructParams(const ConstructParams& other);

private:
  // noncopyable 
  ConstructParams& operator=(const ConstructParams&);
};

#endif /* DOXYGEN_SHOULD_SKIP_THIS */


class GLIBMM_API Object : virtual public ObjectBase
{
public:
#ifndef DOXYGEN_SHOULD_SKIP_THIS
  typedef Object       CppObjectType;
  typedef Object_Class CppClassType;
  typedef GObject      BaseObjectType;
  typedef GObjectClass BaseClassType;
#endif /* DOXYGEN_SHOULD_SKIP_THIS */

protected:
  Object(); //For use by C++-only sub-types.
  explicit Object(const Glib::ConstructParams& construct_params);
  explicit Object(GObject* castitem);
  virtual ~Object(); //It should only be deleted by the callback.

public:
  //static RefPtr<Object> create(); //You must reimplement this in each derived class.

#ifndef DOXYGEN_SHOULD_SKIP_THIS
  static GType get_type()      G_GNUC_CONST;
  static GType get_base_type() G_GNUC_CONST;
#endif

  //GObject* gobj_copy(); //Give a ref-ed copy to someone. Use for direct struct access.

  // Glib::Objects contain a list<Quark, pair<void*, DestroyNotify> >
  // to store run time data added to the object at run time.
  //TODO: Use slots instead:
  void* get_data(const QueryQuark &key);
  void set_data(const Quark &key, void* data);
  typedef void (*DestroyNotify) (gpointer data);
  void set_data(const Quark &key, void* data, DestroyNotify notify);
  void remove_data(const QueryQuark& quark);
  // same as remove without notifying
  void* steal_data(const QueryQuark& quark);

  // convenience functions
  //template <class T>
  //void set_data_typed(const Quark& quark, const T& data)
  //  { set_data(quark, new T(data), delete_typed<T>); }

  //template <class T>
  //T& get_data_typed(const QueryQuark& quark)
  //  { return *static_cast<T*>(get_data(quark)); }

#ifndef DOXYGEN_SHOULD_SKIP_THIS

private:
  friend class Glib::Object_Class;
  static CppClassType object_class_;

  // noncopyable
  Object(const Object&);
  Object& operator=(const Object&);

#endif /* DOXYGEN_SHOULD_SKIP_THIS */

  // Glib::Object can not be dynamic because it lacks a float state.
  //virtual void set_manage();
};


//For some (proably, more spec-compliant) compilers, these specializations must
//be next to the objects that they use.
#ifndef GLIBMM_CAN_USE_DYNAMIC_CAST_IN_UNUSED_TEMPLATE_WITHOUT_DEFINITION
#ifndef DOXYGEN_SHOULD_SKIP_THIS /* hide the specializations */

namespace Container_Helpers
{

/** Partial specialization for pointers to GObject instances.
 * @ingroup ContHelpers
 * The C++ type is always a Glib::RefPtr<>.
 */
template <class T>
struct TypeTraits< Glib::RefPtr<T> >
{
  typedef Glib::RefPtr<T>              CppType;
  typedef typename T::BaseObjectType * CType;
  typedef typename T::BaseObjectType * CTypeNonConst;

  static CType   to_c_type      (const CppType& ptr) { return Glib::unwrap(ptr);     }
  static CType   to_c_type      (CType          ptr) { return ptr;                   }
  static CppType to_cpp_type    (CType          ptr)
  {
    //return Glib::wrap(ptr, true);

    //We copy/paste the wrap() implementation here,
    //because we can not use a specific Glib::wrap(CType) overload here,
    //because that would be "dependent", and g++ 3.4 does not allow that.
    //The specific Glib::wrap() overloads don't do anything special anyway.
    GObject* cobj = (GObject*)const_cast<CTypeNonConst>(ptr);
    return Glib::RefPtr<T>( dynamic_cast<T*>(Glib::wrap_auto(cobj, true /* take_copy */)) );
    //We use dynamic_cast<> in case of multiple inheritance.
  }
  
  static void    release_c_type (CType          ptr)
  {
    GLIBMM_DEBUG_UNREFERENCE(0, ptr);
    g_object_unref(ptr);
  }
};

//This confuses the SUN Forte compiler, so we ifdef it out:
#ifdef GLIBMM_HAVE_DISAMBIGUOUS_CONST_TEMPLATE_SPECIALIZATIONS

/** Partial specialization for pointers to const GObject instances.
 * @ingroup ContHelpers
 * The C++ type is always a Glib::RefPtr<>.
 */
template <class T>
struct TypeTraits< Glib::RefPtr<const T> >
{
  typedef Glib::RefPtr<const T>              CppType;
  typedef const typename T::BaseObjectType * CType;
  typedef typename T::BaseObjectType *       CTypeNonConst;

  static CType   to_c_type      (const CppType& ptr) { return Glib::unwrap(ptr);     }
  static CType   to_c_type      (CType          ptr) { return ptr;                   }
  static CppType to_cpp_type    (CType          ptr)
  {
    //return Glib::wrap(ptr, true);

    //We copy/paste the wrap() implementation here,
    //because we can not use a specific Glib::wrap(CType) overload here,
    //because that would be "dependent", and g++ 3.4 does not allow that.
    //The specific Glib::wrap() overloads don't do anything special anyway.
    GObject* cobj = (GObject*)(ptr);
    return Glib::RefPtr<const T>( dynamic_cast<const T*>(Glib::wrap_auto(cobj, true /* take_copy */)) );
    //We use dynamic_cast<> in case of multiple inheritance.
  }
  
  static void    release_c_type (CType          ptr)
  {
    GLIBMM_DEBUG_UNREFERENCE(0, ptr);
    g_object_unref(const_cast<CTypeNonConst>(ptr));
  }
};

#endif //GLIBMM_HAVE_DISAMBIGUOUS_CONST_TEMPLATE_SPECIALIZATIONS

} //namespace Container_Helpers


template <class T, class PtrT> inline
PtrT Value_Pointer<T,PtrT>::get_(Glib::Object*) const
{
  return dynamic_cast<T*>(get_object());
}


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


#endif //DOXYGEN_SHOULD_SKIP_THIS
#endif //GLIBMM_CAN_USE_DYNAMIC_CAST_IN_UNUSED_TEMPLATE_WITHOUT_DEFINITION

} // namespace Glib

#endif /* _GLIBMM_OBJECT_H */

