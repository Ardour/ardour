// -*- c++ -*-
/* Do not edit! -- generated file */


#ifndef _SIGC_MACROS_OBJECT_SLOTHM4_
#define _SIGC_MACROS_OBJECT_SLOTHM4_

#include <sigc++/slot.h>
#include <sigc++/object.h>
#include <sigc++/functors/mem_fun.h>

#ifndef LIBSIGC_DISABLE_DEPRECATED

namespace SigC {

/** Creates a functor of type SigC::Slot0 that encapsulates a  method and an object instance.
 * @e _A_obj must be of a type that inherits from SigC::Object.
 *
 * @param _A_obj Reference to object instance the functor should operate on.
 * @param _A_func Pointer to method that should be wrapped.
 * @return Functor that executes _A_func on invokation.
 *
 * @deprecated Use sigc::mem_fun() instead.
 * @ingroup compat
 */
template <class T_return, class T_obj1, class T_obj2>
inline Slot0<T_return>
slot( T_obj1& _A_obj, T_return (T_obj2::*_A_func)() )
{ (void)dynamic_cast< Object&>(_A_obj); // trigger compiler error if T_obj1 does not derive from SigC::Object
  return ::sigc::bound_mem_functor0<T_return, T_obj2>(_A_obj, _A_func); }

/** Creates a functor of type SigC::Slot1 that encapsulates a  method and an object instance.
 * @e _A_obj must be of a type that inherits from SigC::Object.
 *
 * @param _A_obj Reference to object instance the functor should operate on.
 * @param _A_func Pointer to method that should be wrapped.
 * @return Functor that executes _A_func on invokation.
 *
 * @deprecated Use sigc::mem_fun() instead.
 * @ingroup compat
 */
template <class T_return, class T_arg1, class T_obj1, class T_obj2>
inline Slot1<T_return, T_arg1>
slot( T_obj1& _A_obj, T_return (T_obj2::*_A_func)(T_arg1) )
{ (void)dynamic_cast< Object&>(_A_obj); // trigger compiler error if T_obj1 does not derive from SigC::Object
  return ::sigc::bound_mem_functor1<T_return, T_obj2, T_arg1>(_A_obj, _A_func); }

/** Creates a functor of type SigC::Slot2 that encapsulates a  method and an object instance.
 * @e _A_obj must be of a type that inherits from SigC::Object.
 *
 * @param _A_obj Reference to object instance the functor should operate on.
 * @param _A_func Pointer to method that should be wrapped.
 * @return Functor that executes _A_func on invokation.
 *
 * @deprecated Use sigc::mem_fun() instead.
 * @ingroup compat
 */
template <class T_return, class T_arg1,class T_arg2, class T_obj1, class T_obj2>
inline Slot2<T_return, T_arg1,T_arg2>
slot( T_obj1& _A_obj, T_return (T_obj2::*_A_func)(T_arg1,T_arg2) )
{ (void)dynamic_cast< Object&>(_A_obj); // trigger compiler error if T_obj1 does not derive from SigC::Object
  return ::sigc::bound_mem_functor2<T_return, T_obj2, T_arg1,T_arg2>(_A_obj, _A_func); }

/** Creates a functor of type SigC::Slot3 that encapsulates a  method and an object instance.
 * @e _A_obj must be of a type that inherits from SigC::Object.
 *
 * @param _A_obj Reference to object instance the functor should operate on.
 * @param _A_func Pointer to method that should be wrapped.
 * @return Functor that executes _A_func on invokation.
 *
 * @deprecated Use sigc::mem_fun() instead.
 * @ingroup compat
 */
template <class T_return, class T_arg1,class T_arg2,class T_arg3, class T_obj1, class T_obj2>
inline Slot3<T_return, T_arg1,T_arg2,T_arg3>
slot( T_obj1& _A_obj, T_return (T_obj2::*_A_func)(T_arg1,T_arg2,T_arg3) )
{ (void)dynamic_cast< Object&>(_A_obj); // trigger compiler error if T_obj1 does not derive from SigC::Object
  return ::sigc::bound_mem_functor3<T_return, T_obj2, T_arg1,T_arg2,T_arg3>(_A_obj, _A_func); }

/** Creates a functor of type SigC::Slot4 that encapsulates a  method and an object instance.
 * @e _A_obj must be of a type that inherits from SigC::Object.
 *
 * @param _A_obj Reference to object instance the functor should operate on.
 * @param _A_func Pointer to method that should be wrapped.
 * @return Functor that executes _A_func on invokation.
 *
 * @deprecated Use sigc::mem_fun() instead.
 * @ingroup compat
 */
template <class T_return, class T_arg1,class T_arg2,class T_arg3,class T_arg4, class T_obj1, class T_obj2>
inline Slot4<T_return, T_arg1,T_arg2,T_arg3,T_arg4>
slot( T_obj1& _A_obj, T_return (T_obj2::*_A_func)(T_arg1,T_arg2,T_arg3,T_arg4) )
{ (void)dynamic_cast< Object&>(_A_obj); // trigger compiler error if T_obj1 does not derive from SigC::Object
  return ::sigc::bound_mem_functor4<T_return, T_obj2, T_arg1,T_arg2,T_arg3,T_arg4>(_A_obj, _A_func); }

/** Creates a functor of type SigC::Slot5 that encapsulates a  method and an object instance.
 * @e _A_obj must be of a type that inherits from SigC::Object.
 *
 * @param _A_obj Reference to object instance the functor should operate on.
 * @param _A_func Pointer to method that should be wrapped.
 * @return Functor that executes _A_func on invokation.
 *
 * @deprecated Use sigc::mem_fun() instead.
 * @ingroup compat
 */
template <class T_return, class T_arg1,class T_arg2,class T_arg3,class T_arg4,class T_arg5, class T_obj1, class T_obj2>
inline Slot5<T_return, T_arg1,T_arg2,T_arg3,T_arg4,T_arg5>
slot( T_obj1& _A_obj, T_return (T_obj2::*_A_func)(T_arg1,T_arg2,T_arg3,T_arg4,T_arg5) )
{ (void)dynamic_cast< Object&>(_A_obj); // trigger compiler error if T_obj1 does not derive from SigC::Object
  return ::sigc::bound_mem_functor5<T_return, T_obj2, T_arg1,T_arg2,T_arg3,T_arg4,T_arg5>(_A_obj, _A_func); }

/** Creates a functor of type SigC::Slot6 that encapsulates a  method and an object instance.
 * @e _A_obj must be of a type that inherits from SigC::Object.
 *
 * @param _A_obj Reference to object instance the functor should operate on.
 * @param _A_func Pointer to method that should be wrapped.
 * @return Functor that executes _A_func on invokation.
 *
 * @deprecated Use sigc::mem_fun() instead.
 * @ingroup compat
 */
template <class T_return, class T_arg1,class T_arg2,class T_arg3,class T_arg4,class T_arg5,class T_arg6, class T_obj1, class T_obj2>
inline Slot6<T_return, T_arg1,T_arg2,T_arg3,T_arg4,T_arg5,T_arg6>
slot( T_obj1& _A_obj, T_return (T_obj2::*_A_func)(T_arg1,T_arg2,T_arg3,T_arg4,T_arg5,T_arg6) )
{ (void)dynamic_cast< Object&>(_A_obj); // trigger compiler error if T_obj1 does not derive from SigC::Object
  return ::sigc::bound_mem_functor6<T_return, T_obj2, T_arg1,T_arg2,T_arg3,T_arg4,T_arg5,T_arg6>(_A_obj, _A_func); }

/** Creates a functor of type SigC::Slot7 that encapsulates a  method and an object instance.
 * @e _A_obj must be of a type that inherits from SigC::Object.
 *
 * @param _A_obj Reference to object instance the functor should operate on.
 * @param _A_func Pointer to method that should be wrapped.
 * @return Functor that executes _A_func on invokation.
 *
 * @deprecated Use sigc::mem_fun() instead.
 * @ingroup compat
 */
template <class T_return, class T_arg1,class T_arg2,class T_arg3,class T_arg4,class T_arg5,class T_arg6,class T_arg7, class T_obj1, class T_obj2>
inline Slot7<T_return, T_arg1,T_arg2,T_arg3,T_arg4,T_arg5,T_arg6,T_arg7>
slot( T_obj1& _A_obj, T_return (T_obj2::*_A_func)(T_arg1,T_arg2,T_arg3,T_arg4,T_arg5,T_arg6,T_arg7) )
{ (void)dynamic_cast< Object&>(_A_obj); // trigger compiler error if T_obj1 does not derive from SigC::Object
  return ::sigc::bound_mem_functor7<T_return, T_obj2, T_arg1,T_arg2,T_arg3,T_arg4,T_arg5,T_arg6,T_arg7>(_A_obj, _A_func); }


/** Creates a functor of type SigC::Slot0 that encapsulates a const method and an object instance.
 * @e _A_obj must be of a type that inherits from SigC::Object.
 *
 * @param _A_obj Reference to object instance the functor should operate on.
 * @param _A_func Pointer to method that should be wrapped.
 * @return Functor that executes _A_func on invokation.
 *
 * @deprecated Use sigc::mem_fun() instead.
 * @ingroup compat
 */
template <class T_return, class T_obj1, class T_obj2>
inline Slot0<T_return>
slot(const T_obj1& _A_obj, T_return (T_obj2::*_A_func)() const)
{ (void)dynamic_cast<const Object&>(_A_obj); // trigger compiler error if T_obj1 does not derive from SigC::Object
  return ::sigc::bound_const_mem_functor0<T_return, T_obj2>(_A_obj, _A_func); }

/** Creates a functor of type SigC::Slot1 that encapsulates a const method and an object instance.
 * @e _A_obj must be of a type that inherits from SigC::Object.
 *
 * @param _A_obj Reference to object instance the functor should operate on.
 * @param _A_func Pointer to method that should be wrapped.
 * @return Functor that executes _A_func on invokation.
 *
 * @deprecated Use sigc::mem_fun() instead.
 * @ingroup compat
 */
template <class T_return, class T_arg1, class T_obj1, class T_obj2>
inline Slot1<T_return, T_arg1>
slot(const T_obj1& _A_obj, T_return (T_obj2::*_A_func)(T_arg1) const)
{ (void)dynamic_cast<const Object&>(_A_obj); // trigger compiler error if T_obj1 does not derive from SigC::Object
  return ::sigc::bound_const_mem_functor1<T_return, T_obj2, T_arg1>(_A_obj, _A_func); }

/** Creates a functor of type SigC::Slot2 that encapsulates a const method and an object instance.
 * @e _A_obj must be of a type that inherits from SigC::Object.
 *
 * @param _A_obj Reference to object instance the functor should operate on.
 * @param _A_func Pointer to method that should be wrapped.
 * @return Functor that executes _A_func on invokation.
 *
 * @deprecated Use sigc::mem_fun() instead.
 * @ingroup compat
 */
template <class T_return, class T_arg1,class T_arg2, class T_obj1, class T_obj2>
inline Slot2<T_return, T_arg1,T_arg2>
slot(const T_obj1& _A_obj, T_return (T_obj2::*_A_func)(T_arg1,T_arg2) const)
{ (void)dynamic_cast<const Object&>(_A_obj); // trigger compiler error if T_obj1 does not derive from SigC::Object
  return ::sigc::bound_const_mem_functor2<T_return, T_obj2, T_arg1,T_arg2>(_A_obj, _A_func); }

/** Creates a functor of type SigC::Slot3 that encapsulates a const method and an object instance.
 * @e _A_obj must be of a type that inherits from SigC::Object.
 *
 * @param _A_obj Reference to object instance the functor should operate on.
 * @param _A_func Pointer to method that should be wrapped.
 * @return Functor that executes _A_func on invokation.
 *
 * @deprecated Use sigc::mem_fun() instead.
 * @ingroup compat
 */
template <class T_return, class T_arg1,class T_arg2,class T_arg3, class T_obj1, class T_obj2>
inline Slot3<T_return, T_arg1,T_arg2,T_arg3>
slot(const T_obj1& _A_obj, T_return (T_obj2::*_A_func)(T_arg1,T_arg2,T_arg3) const)
{ (void)dynamic_cast<const Object&>(_A_obj); // trigger compiler error if T_obj1 does not derive from SigC::Object
  return ::sigc::bound_const_mem_functor3<T_return, T_obj2, T_arg1,T_arg2,T_arg3>(_A_obj, _A_func); }

/** Creates a functor of type SigC::Slot4 that encapsulates a const method and an object instance.
 * @e _A_obj must be of a type that inherits from SigC::Object.
 *
 * @param _A_obj Reference to object instance the functor should operate on.
 * @param _A_func Pointer to method that should be wrapped.
 * @return Functor that executes _A_func on invokation.
 *
 * @deprecated Use sigc::mem_fun() instead.
 * @ingroup compat
 */
template <class T_return, class T_arg1,class T_arg2,class T_arg3,class T_arg4, class T_obj1, class T_obj2>
inline Slot4<T_return, T_arg1,T_arg2,T_arg3,T_arg4>
slot(const T_obj1& _A_obj, T_return (T_obj2::*_A_func)(T_arg1,T_arg2,T_arg3,T_arg4) const)
{ (void)dynamic_cast<const Object&>(_A_obj); // trigger compiler error if T_obj1 does not derive from SigC::Object
  return ::sigc::bound_const_mem_functor4<T_return, T_obj2, T_arg1,T_arg2,T_arg3,T_arg4>(_A_obj, _A_func); }

/** Creates a functor of type SigC::Slot5 that encapsulates a const method and an object instance.
 * @e _A_obj must be of a type that inherits from SigC::Object.
 *
 * @param _A_obj Reference to object instance the functor should operate on.
 * @param _A_func Pointer to method that should be wrapped.
 * @return Functor that executes _A_func on invokation.
 *
 * @deprecated Use sigc::mem_fun() instead.
 * @ingroup compat
 */
template <class T_return, class T_arg1,class T_arg2,class T_arg3,class T_arg4,class T_arg5, class T_obj1, class T_obj2>
inline Slot5<T_return, T_arg1,T_arg2,T_arg3,T_arg4,T_arg5>
slot(const T_obj1& _A_obj, T_return (T_obj2::*_A_func)(T_arg1,T_arg2,T_arg3,T_arg4,T_arg5) const)
{ (void)dynamic_cast<const Object&>(_A_obj); // trigger compiler error if T_obj1 does not derive from SigC::Object
  return ::sigc::bound_const_mem_functor5<T_return, T_obj2, T_arg1,T_arg2,T_arg3,T_arg4,T_arg5>(_A_obj, _A_func); }

/** Creates a functor of type SigC::Slot6 that encapsulates a const method and an object instance.
 * @e _A_obj must be of a type that inherits from SigC::Object.
 *
 * @param _A_obj Reference to object instance the functor should operate on.
 * @param _A_func Pointer to method that should be wrapped.
 * @return Functor that executes _A_func on invokation.
 *
 * @deprecated Use sigc::mem_fun() instead.
 * @ingroup compat
 */
template <class T_return, class T_arg1,class T_arg2,class T_arg3,class T_arg4,class T_arg5,class T_arg6, class T_obj1, class T_obj2>
inline Slot6<T_return, T_arg1,T_arg2,T_arg3,T_arg4,T_arg5,T_arg6>
slot(const T_obj1& _A_obj, T_return (T_obj2::*_A_func)(T_arg1,T_arg2,T_arg3,T_arg4,T_arg5,T_arg6) const)
{ (void)dynamic_cast<const Object&>(_A_obj); // trigger compiler error if T_obj1 does not derive from SigC::Object
  return ::sigc::bound_const_mem_functor6<T_return, T_obj2, T_arg1,T_arg2,T_arg3,T_arg4,T_arg5,T_arg6>(_A_obj, _A_func); }

/** Creates a functor of type SigC::Slot7 that encapsulates a const method and an object instance.
 * @e _A_obj must be of a type that inherits from SigC::Object.
 *
 * @param _A_obj Reference to object instance the functor should operate on.
 * @param _A_func Pointer to method that should be wrapped.
 * @return Functor that executes _A_func on invokation.
 *
 * @deprecated Use sigc::mem_fun() instead.
 * @ingroup compat
 */
template <class T_return, class T_arg1,class T_arg2,class T_arg3,class T_arg4,class T_arg5,class T_arg6,class T_arg7, class T_obj1, class T_obj2>
inline Slot7<T_return, T_arg1,T_arg2,T_arg3,T_arg4,T_arg5,T_arg6,T_arg7>
slot(const T_obj1& _A_obj, T_return (T_obj2::*_A_func)(T_arg1,T_arg2,T_arg3,T_arg4,T_arg5,T_arg6,T_arg7) const)
{ (void)dynamic_cast<const Object&>(_A_obj); // trigger compiler error if T_obj1 does not derive from SigC::Object
  return ::sigc::bound_const_mem_functor7<T_return, T_obj2, T_arg1,T_arg2,T_arg3,T_arg4,T_arg5,T_arg6,T_arg7>(_A_obj, _A_func); }


/** Creates a functor of type SigC::Slot0 that encapsulates a volatile method and an object instance.
 * @e _A_obj must be of a type that inherits from SigC::Object.
 *
 * @param _A_obj Reference to object instance the functor should operate on.
 * @param _A_func Pointer to method that should be wrapped.
 * @return Functor that executes _A_func on invokation.
 *
 * @deprecated Use sigc::mem_fun() instead.
 * @ingroup compat
 */
template <class T_return, class T_obj1, class T_obj2>
inline Slot0<T_return>
slot( T_obj1& _A_obj, T_return (T_obj2::*_A_func)() volatile)
{ (void)dynamic_cast< Object&>(_A_obj); // trigger compiler error if T_obj1 does not derive from SigC::Object
  return ::sigc::bound_volatile_mem_functor0<T_return, T_obj2>(_A_obj, _A_func); }

/** Creates a functor of type SigC::Slot1 that encapsulates a volatile method and an object instance.
 * @e _A_obj must be of a type that inherits from SigC::Object.
 *
 * @param _A_obj Reference to object instance the functor should operate on.
 * @param _A_func Pointer to method that should be wrapped.
 * @return Functor that executes _A_func on invokation.
 *
 * @deprecated Use sigc::mem_fun() instead.
 * @ingroup compat
 */
template <class T_return, class T_arg1, class T_obj1, class T_obj2>
inline Slot1<T_return, T_arg1>
slot( T_obj1& _A_obj, T_return (T_obj2::*_A_func)(T_arg1) volatile)
{ (void)dynamic_cast< Object&>(_A_obj); // trigger compiler error if T_obj1 does not derive from SigC::Object
  return ::sigc::bound_volatile_mem_functor1<T_return, T_obj2, T_arg1>(_A_obj, _A_func); }

/** Creates a functor of type SigC::Slot2 that encapsulates a volatile method and an object instance.
 * @e _A_obj must be of a type that inherits from SigC::Object.
 *
 * @param _A_obj Reference to object instance the functor should operate on.
 * @param _A_func Pointer to method that should be wrapped.
 * @return Functor that executes _A_func on invokation.
 *
 * @deprecated Use sigc::mem_fun() instead.
 * @ingroup compat
 */
template <class T_return, class T_arg1,class T_arg2, class T_obj1, class T_obj2>
inline Slot2<T_return, T_arg1,T_arg2>
slot( T_obj1& _A_obj, T_return (T_obj2::*_A_func)(T_arg1,T_arg2) volatile)
{ (void)dynamic_cast< Object&>(_A_obj); // trigger compiler error if T_obj1 does not derive from SigC::Object
  return ::sigc::bound_volatile_mem_functor2<T_return, T_obj2, T_arg1,T_arg2>(_A_obj, _A_func); }

/** Creates a functor of type SigC::Slot3 that encapsulates a volatile method and an object instance.
 * @e _A_obj must be of a type that inherits from SigC::Object.
 *
 * @param _A_obj Reference to object instance the functor should operate on.
 * @param _A_func Pointer to method that should be wrapped.
 * @return Functor that executes _A_func on invokation.
 *
 * @deprecated Use sigc::mem_fun() instead.
 * @ingroup compat
 */
template <class T_return, class T_arg1,class T_arg2,class T_arg3, class T_obj1, class T_obj2>
inline Slot3<T_return, T_arg1,T_arg2,T_arg3>
slot( T_obj1& _A_obj, T_return (T_obj2::*_A_func)(T_arg1,T_arg2,T_arg3) volatile)
{ (void)dynamic_cast< Object&>(_A_obj); // trigger compiler error if T_obj1 does not derive from SigC::Object
  return ::sigc::bound_volatile_mem_functor3<T_return, T_obj2, T_arg1,T_arg2,T_arg3>(_A_obj, _A_func); }

/** Creates a functor of type SigC::Slot4 that encapsulates a volatile method and an object instance.
 * @e _A_obj must be of a type that inherits from SigC::Object.
 *
 * @param _A_obj Reference to object instance the functor should operate on.
 * @param _A_func Pointer to method that should be wrapped.
 * @return Functor that executes _A_func on invokation.
 *
 * @deprecated Use sigc::mem_fun() instead.
 * @ingroup compat
 */
template <class T_return, class T_arg1,class T_arg2,class T_arg3,class T_arg4, class T_obj1, class T_obj2>
inline Slot4<T_return, T_arg1,T_arg2,T_arg3,T_arg4>
slot( T_obj1& _A_obj, T_return (T_obj2::*_A_func)(T_arg1,T_arg2,T_arg3,T_arg4) volatile)
{ (void)dynamic_cast< Object&>(_A_obj); // trigger compiler error if T_obj1 does not derive from SigC::Object
  return ::sigc::bound_volatile_mem_functor4<T_return, T_obj2, T_arg1,T_arg2,T_arg3,T_arg4>(_A_obj, _A_func); }

/** Creates a functor of type SigC::Slot5 that encapsulates a volatile method and an object instance.
 * @e _A_obj must be of a type that inherits from SigC::Object.
 *
 * @param _A_obj Reference to object instance the functor should operate on.
 * @param _A_func Pointer to method that should be wrapped.
 * @return Functor that executes _A_func on invokation.
 *
 * @deprecated Use sigc::mem_fun() instead.
 * @ingroup compat
 */
template <class T_return, class T_arg1,class T_arg2,class T_arg3,class T_arg4,class T_arg5, class T_obj1, class T_obj2>
inline Slot5<T_return, T_arg1,T_arg2,T_arg3,T_arg4,T_arg5>
slot( T_obj1& _A_obj, T_return (T_obj2::*_A_func)(T_arg1,T_arg2,T_arg3,T_arg4,T_arg5) volatile)
{ (void)dynamic_cast< Object&>(_A_obj); // trigger compiler error if T_obj1 does not derive from SigC::Object
  return ::sigc::bound_volatile_mem_functor5<T_return, T_obj2, T_arg1,T_arg2,T_arg3,T_arg4,T_arg5>(_A_obj, _A_func); }

/** Creates a functor of type SigC::Slot6 that encapsulates a volatile method and an object instance.
 * @e _A_obj must be of a type that inherits from SigC::Object.
 *
 * @param _A_obj Reference to object instance the functor should operate on.
 * @param _A_func Pointer to method that should be wrapped.
 * @return Functor that executes _A_func on invokation.
 *
 * @deprecated Use sigc::mem_fun() instead.
 * @ingroup compat
 */
template <class T_return, class T_arg1,class T_arg2,class T_arg3,class T_arg4,class T_arg5,class T_arg6, class T_obj1, class T_obj2>
inline Slot6<T_return, T_arg1,T_arg2,T_arg3,T_arg4,T_arg5,T_arg6>
slot( T_obj1& _A_obj, T_return (T_obj2::*_A_func)(T_arg1,T_arg2,T_arg3,T_arg4,T_arg5,T_arg6) volatile)
{ (void)dynamic_cast< Object&>(_A_obj); // trigger compiler error if T_obj1 does not derive from SigC::Object
  return ::sigc::bound_volatile_mem_functor6<T_return, T_obj2, T_arg1,T_arg2,T_arg3,T_arg4,T_arg5,T_arg6>(_A_obj, _A_func); }

/** Creates a functor of type SigC::Slot7 that encapsulates a volatile method and an object instance.
 * @e _A_obj must be of a type that inherits from SigC::Object.
 *
 * @param _A_obj Reference to object instance the functor should operate on.
 * @param _A_func Pointer to method that should be wrapped.
 * @return Functor that executes _A_func on invokation.
 *
 * @deprecated Use sigc::mem_fun() instead.
 * @ingroup compat
 */
template <class T_return, class T_arg1,class T_arg2,class T_arg3,class T_arg4,class T_arg5,class T_arg6,class T_arg7, class T_obj1, class T_obj2>
inline Slot7<T_return, T_arg1,T_arg2,T_arg3,T_arg4,T_arg5,T_arg6,T_arg7>
slot( T_obj1& _A_obj, T_return (T_obj2::*_A_func)(T_arg1,T_arg2,T_arg3,T_arg4,T_arg5,T_arg6,T_arg7) volatile)
{ (void)dynamic_cast< Object&>(_A_obj); // trigger compiler error if T_obj1 does not derive from SigC::Object
  return ::sigc::bound_volatile_mem_functor7<T_return, T_obj2, T_arg1,T_arg2,T_arg3,T_arg4,T_arg5,T_arg6,T_arg7>(_A_obj, _A_func); }


/** Creates a functor of type SigC::Slot0 that encapsulates a const volatile method and an object instance.
 * @e _A_obj must be of a type that inherits from SigC::Object.
 *
 * @param _A_obj Reference to object instance the functor should operate on.
 * @param _A_func Pointer to method that should be wrapped.
 * @return Functor that executes _A_func on invokation.
 *
 * @deprecated Use sigc::mem_fun() instead.
 * @ingroup compat
 */
template <class T_return, class T_obj1, class T_obj2>
inline Slot0<T_return>
slot(const T_obj1& _A_obj, T_return (T_obj2::*_A_func)() const volatile)
{ (void)dynamic_cast<const Object&>(_A_obj); // trigger compiler error if T_obj1 does not derive from SigC::Object
  return ::sigc::bound_const_volatile_mem_functor0<T_return, T_obj2>(_A_obj, _A_func); }

/** Creates a functor of type SigC::Slot1 that encapsulates a const volatile method and an object instance.
 * @e _A_obj must be of a type that inherits from SigC::Object.
 *
 * @param _A_obj Reference to object instance the functor should operate on.
 * @param _A_func Pointer to method that should be wrapped.
 * @return Functor that executes _A_func on invokation.
 *
 * @deprecated Use sigc::mem_fun() instead.
 * @ingroup compat
 */
template <class T_return, class T_arg1, class T_obj1, class T_obj2>
inline Slot1<T_return, T_arg1>
slot(const T_obj1& _A_obj, T_return (T_obj2::*_A_func)(T_arg1) const volatile)
{ (void)dynamic_cast<const Object&>(_A_obj); // trigger compiler error if T_obj1 does not derive from SigC::Object
  return ::sigc::bound_const_volatile_mem_functor1<T_return, T_obj2, T_arg1>(_A_obj, _A_func); }

/** Creates a functor of type SigC::Slot2 that encapsulates a const volatile method and an object instance.
 * @e _A_obj must be of a type that inherits from SigC::Object.
 *
 * @param _A_obj Reference to object instance the functor should operate on.
 * @param _A_func Pointer to method that should be wrapped.
 * @return Functor that executes _A_func on invokation.
 *
 * @deprecated Use sigc::mem_fun() instead.
 * @ingroup compat
 */
template <class T_return, class T_arg1,class T_arg2, class T_obj1, class T_obj2>
inline Slot2<T_return, T_arg1,T_arg2>
slot(const T_obj1& _A_obj, T_return (T_obj2::*_A_func)(T_arg1,T_arg2) const volatile)
{ (void)dynamic_cast<const Object&>(_A_obj); // trigger compiler error if T_obj1 does not derive from SigC::Object
  return ::sigc::bound_const_volatile_mem_functor2<T_return, T_obj2, T_arg1,T_arg2>(_A_obj, _A_func); }

/** Creates a functor of type SigC::Slot3 that encapsulates a const volatile method and an object instance.
 * @e _A_obj must be of a type that inherits from SigC::Object.
 *
 * @param _A_obj Reference to object instance the functor should operate on.
 * @param _A_func Pointer to method that should be wrapped.
 * @return Functor that executes _A_func on invokation.
 *
 * @deprecated Use sigc::mem_fun() instead.
 * @ingroup compat
 */
template <class T_return, class T_arg1,class T_arg2,class T_arg3, class T_obj1, class T_obj2>
inline Slot3<T_return, T_arg1,T_arg2,T_arg3>
slot(const T_obj1& _A_obj, T_return (T_obj2::*_A_func)(T_arg1,T_arg2,T_arg3) const volatile)
{ (void)dynamic_cast<const Object&>(_A_obj); // trigger compiler error if T_obj1 does not derive from SigC::Object
  return ::sigc::bound_const_volatile_mem_functor3<T_return, T_obj2, T_arg1,T_arg2,T_arg3>(_A_obj, _A_func); }

/** Creates a functor of type SigC::Slot4 that encapsulates a const volatile method and an object instance.
 * @e _A_obj must be of a type that inherits from SigC::Object.
 *
 * @param _A_obj Reference to object instance the functor should operate on.
 * @param _A_func Pointer to method that should be wrapped.
 * @return Functor that executes _A_func on invokation.
 *
 * @deprecated Use sigc::mem_fun() instead.
 * @ingroup compat
 */
template <class T_return, class T_arg1,class T_arg2,class T_arg3,class T_arg4, class T_obj1, class T_obj2>
inline Slot4<T_return, T_arg1,T_arg2,T_arg3,T_arg4>
slot(const T_obj1& _A_obj, T_return (T_obj2::*_A_func)(T_arg1,T_arg2,T_arg3,T_arg4) const volatile)
{ (void)dynamic_cast<const Object&>(_A_obj); // trigger compiler error if T_obj1 does not derive from SigC::Object
  return ::sigc::bound_const_volatile_mem_functor4<T_return, T_obj2, T_arg1,T_arg2,T_arg3,T_arg4>(_A_obj, _A_func); }

/** Creates a functor of type SigC::Slot5 that encapsulates a const volatile method and an object instance.
 * @e _A_obj must be of a type that inherits from SigC::Object.
 *
 * @param _A_obj Reference to object instance the functor should operate on.
 * @param _A_func Pointer to method that should be wrapped.
 * @return Functor that executes _A_func on invokation.
 *
 * @deprecated Use sigc::mem_fun() instead.
 * @ingroup compat
 */
template <class T_return, class T_arg1,class T_arg2,class T_arg3,class T_arg4,class T_arg5, class T_obj1, class T_obj2>
inline Slot5<T_return, T_arg1,T_arg2,T_arg3,T_arg4,T_arg5>
slot(const T_obj1& _A_obj, T_return (T_obj2::*_A_func)(T_arg1,T_arg2,T_arg3,T_arg4,T_arg5) const volatile)
{ (void)dynamic_cast<const Object&>(_A_obj); // trigger compiler error if T_obj1 does not derive from SigC::Object
  return ::sigc::bound_const_volatile_mem_functor5<T_return, T_obj2, T_arg1,T_arg2,T_arg3,T_arg4,T_arg5>(_A_obj, _A_func); }

/** Creates a functor of type SigC::Slot6 that encapsulates a const volatile method and an object instance.
 * @e _A_obj must be of a type that inherits from SigC::Object.
 *
 * @param _A_obj Reference to object instance the functor should operate on.
 * @param _A_func Pointer to method that should be wrapped.
 * @return Functor that executes _A_func on invokation.
 *
 * @deprecated Use sigc::mem_fun() instead.
 * @ingroup compat
 */
template <class T_return, class T_arg1,class T_arg2,class T_arg3,class T_arg4,class T_arg5,class T_arg6, class T_obj1, class T_obj2>
inline Slot6<T_return, T_arg1,T_arg2,T_arg3,T_arg4,T_arg5,T_arg6>
slot(const T_obj1& _A_obj, T_return (T_obj2::*_A_func)(T_arg1,T_arg2,T_arg3,T_arg4,T_arg5,T_arg6) const volatile)
{ (void)dynamic_cast<const Object&>(_A_obj); // trigger compiler error if T_obj1 does not derive from SigC::Object
  return ::sigc::bound_const_volatile_mem_functor6<T_return, T_obj2, T_arg1,T_arg2,T_arg3,T_arg4,T_arg5,T_arg6>(_A_obj, _A_func); }

/** Creates a functor of type SigC::Slot7 that encapsulates a const volatile method and an object instance.
 * @e _A_obj must be of a type that inherits from SigC::Object.
 *
 * @param _A_obj Reference to object instance the functor should operate on.
 * @param _A_func Pointer to method that should be wrapped.
 * @return Functor that executes _A_func on invokation.
 *
 * @deprecated Use sigc::mem_fun() instead.
 * @ingroup compat
 */
template <class T_return, class T_arg1,class T_arg2,class T_arg3,class T_arg4,class T_arg5,class T_arg6,class T_arg7, class T_obj1, class T_obj2>
inline Slot7<T_return, T_arg1,T_arg2,T_arg3,T_arg4,T_arg5,T_arg6,T_arg7>
slot(const T_obj1& _A_obj, T_return (T_obj2::*_A_func)(T_arg1,T_arg2,T_arg3,T_arg4,T_arg5,T_arg6,T_arg7) const volatile)
{ (void)dynamic_cast<const Object&>(_A_obj); // trigger compiler error if T_obj1 does not derive from SigC::Object
  return ::sigc::bound_const_volatile_mem_functor7<T_return, T_obj2, T_arg1,T_arg2,T_arg3,T_arg4,T_arg5,T_arg6,T_arg7>(_A_obj, _A_func); }



}

#endif
#endif /* _SIGC_MACROS_OBJECT_SLOTHM4_ */
