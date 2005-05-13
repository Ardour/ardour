// -*- c++ -*-
/* Do not edit! -- generated file */


#ifndef _SIGC_MACROS_CLASS_SLOTHM4_
#define _SIGC_MACROS_CLASS_SLOTHM4_

#include <sigc++/slot.h>
#include <sigc++/functors/mem_fun.h>

#ifndef LIBSIGC_DISABLE_DEPRECATED

namespace SigC {

// slot_class()
/** Creates a functor of type SigC::Slot0 that encapsulates a  method and an object instance.
 *
 * This function is part of the compatibility module and therefore deprecated.
 * Use sigc::mem_fun() instead.
 *
 * @param _A_obj Reference to object instance the functor should operate on.
 * @param _A_func Pointer to method that should be wrapped.
 * @return Functor that executes _A_func on invokation.
 *
 * @deprecated Use sigc::mem_fun() instead.
 * @ingroup compat
 */
template <class T_return, class T_obj>
inline Slot0<T_return>
slot_class( T_obj& _A_obj, T_return (T_obj::*_A_func)() )
{ return ::sigc::bound_mem_functor0<T_return, T_obj>(_A_obj, _A_func); }

/** Creates a functor of type SigC::Slot1 that encapsulates a  method and an object instance.
 *
 * This function is part of the compatibility module and therefore deprecated.
 * Use sigc::mem_fun() instead.
 *
 * @param _A_obj Reference to object instance the functor should operate on.
 * @param _A_func Pointer to method that should be wrapped.
 * @return Functor that executes _A_func on invokation.
 *
 * @deprecated Use sigc::mem_fun() instead.
 * @ingroup compat
 */
template <class T_return, class T_arg1, class T_obj>
inline Slot1<T_return, T_arg1>
slot_class( T_obj& _A_obj, T_return (T_obj::*_A_func)(T_arg1) )
{ return ::sigc::bound_mem_functor1<T_return, T_obj, T_arg1>(_A_obj, _A_func); }

/** Creates a functor of type SigC::Slot2 that encapsulates a  method and an object instance.
 *
 * This function is part of the compatibility module and therefore deprecated.
 * Use sigc::mem_fun() instead.
 *
 * @param _A_obj Reference to object instance the functor should operate on.
 * @param _A_func Pointer to method that should be wrapped.
 * @return Functor that executes _A_func on invokation.
 *
 * @deprecated Use sigc::mem_fun() instead.
 * @ingroup compat
 */
template <class T_return, class T_arg1,class T_arg2, class T_obj>
inline Slot2<T_return, T_arg1,T_arg2>
slot_class( T_obj& _A_obj, T_return (T_obj::*_A_func)(T_arg1,T_arg2) )
{ return ::sigc::bound_mem_functor2<T_return, T_obj, T_arg1,T_arg2>(_A_obj, _A_func); }

/** Creates a functor of type SigC::Slot3 that encapsulates a  method and an object instance.
 *
 * This function is part of the compatibility module and therefore deprecated.
 * Use sigc::mem_fun() instead.
 *
 * @param _A_obj Reference to object instance the functor should operate on.
 * @param _A_func Pointer to method that should be wrapped.
 * @return Functor that executes _A_func on invokation.
 *
 * @deprecated Use sigc::mem_fun() instead.
 * @ingroup compat
 */
template <class T_return, class T_arg1,class T_arg2,class T_arg3, class T_obj>
inline Slot3<T_return, T_arg1,T_arg2,T_arg3>
slot_class( T_obj& _A_obj, T_return (T_obj::*_A_func)(T_arg1,T_arg2,T_arg3) )
{ return ::sigc::bound_mem_functor3<T_return, T_obj, T_arg1,T_arg2,T_arg3>(_A_obj, _A_func); }

/** Creates a functor of type SigC::Slot4 that encapsulates a  method and an object instance.
 *
 * This function is part of the compatibility module and therefore deprecated.
 * Use sigc::mem_fun() instead.
 *
 * @param _A_obj Reference to object instance the functor should operate on.
 * @param _A_func Pointer to method that should be wrapped.
 * @return Functor that executes _A_func on invokation.
 *
 * @deprecated Use sigc::mem_fun() instead.
 * @ingroup compat
 */
template <class T_return, class T_arg1,class T_arg2,class T_arg3,class T_arg4, class T_obj>
inline Slot4<T_return, T_arg1,T_arg2,T_arg3,T_arg4>
slot_class( T_obj& _A_obj, T_return (T_obj::*_A_func)(T_arg1,T_arg2,T_arg3,T_arg4) )
{ return ::sigc::bound_mem_functor4<T_return, T_obj, T_arg1,T_arg2,T_arg3,T_arg4>(_A_obj, _A_func); }

/** Creates a functor of type SigC::Slot5 that encapsulates a  method and an object instance.
 *
 * This function is part of the compatibility module and therefore deprecated.
 * Use sigc::mem_fun() instead.
 *
 * @param _A_obj Reference to object instance the functor should operate on.
 * @param _A_func Pointer to method that should be wrapped.
 * @return Functor that executes _A_func on invokation.
 *
 * @deprecated Use sigc::mem_fun() instead.
 * @ingroup compat
 */
template <class T_return, class T_arg1,class T_arg2,class T_arg3,class T_arg4,class T_arg5, class T_obj>
inline Slot5<T_return, T_arg1,T_arg2,T_arg3,T_arg4,T_arg5>
slot_class( T_obj& _A_obj, T_return (T_obj::*_A_func)(T_arg1,T_arg2,T_arg3,T_arg4,T_arg5) )
{ return ::sigc::bound_mem_functor5<T_return, T_obj, T_arg1,T_arg2,T_arg3,T_arg4,T_arg5>(_A_obj, _A_func); }

/** Creates a functor of type SigC::Slot6 that encapsulates a  method and an object instance.
 *
 * This function is part of the compatibility module and therefore deprecated.
 * Use sigc::mem_fun() instead.
 *
 * @param _A_obj Reference to object instance the functor should operate on.
 * @param _A_func Pointer to method that should be wrapped.
 * @return Functor that executes _A_func on invokation.
 *
 * @deprecated Use sigc::mem_fun() instead.
 * @ingroup compat
 */
template <class T_return, class T_arg1,class T_arg2,class T_arg3,class T_arg4,class T_arg5,class T_arg6, class T_obj>
inline Slot6<T_return, T_arg1,T_arg2,T_arg3,T_arg4,T_arg5,T_arg6>
slot_class( T_obj& _A_obj, T_return (T_obj::*_A_func)(T_arg1,T_arg2,T_arg3,T_arg4,T_arg5,T_arg6) )
{ return ::sigc::bound_mem_functor6<T_return, T_obj, T_arg1,T_arg2,T_arg3,T_arg4,T_arg5,T_arg6>(_A_obj, _A_func); }

/** Creates a functor of type SigC::Slot7 that encapsulates a  method and an object instance.
 *
 * This function is part of the compatibility module and therefore deprecated.
 * Use sigc::mem_fun() instead.
 *
 * @param _A_obj Reference to object instance the functor should operate on.
 * @param _A_func Pointer to method that should be wrapped.
 * @return Functor that executes _A_func on invokation.
 *
 * @deprecated Use sigc::mem_fun() instead.
 * @ingroup compat
 */
template <class T_return, class T_arg1,class T_arg2,class T_arg3,class T_arg4,class T_arg5,class T_arg6,class T_arg7, class T_obj>
inline Slot7<T_return, T_arg1,T_arg2,T_arg3,T_arg4,T_arg5,T_arg6,T_arg7>
slot_class( T_obj& _A_obj, T_return (T_obj::*_A_func)(T_arg1,T_arg2,T_arg3,T_arg4,T_arg5,T_arg6,T_arg7) )
{ return ::sigc::bound_mem_functor7<T_return, T_obj, T_arg1,T_arg2,T_arg3,T_arg4,T_arg5,T_arg6,T_arg7>(_A_obj, _A_func); }


/** Creates a functor of type SigC::Slot0 that encapsulates a const method and an object instance.
 *
 * This function is part of the compatibility module and therefore deprecated.
 * Use sigc::mem_fun() instead.
 *
 * @param _A_obj Reference to object instance the functor should operate on.
 * @param _A_func Pointer to method that should be wrapped.
 * @return Functor that executes _A_func on invokation.
 *
 * @deprecated Use sigc::mem_fun() instead.
 * @ingroup compat
 */
template <class T_return, class T_obj>
inline Slot0<T_return>
slot_class(const T_obj& _A_obj, T_return (T_obj::*_A_func)() const)
{ return ::sigc::bound_const_mem_functor0<T_return, T_obj>(_A_obj, _A_func); }

/** Creates a functor of type SigC::Slot1 that encapsulates a const method and an object instance.
 *
 * This function is part of the compatibility module and therefore deprecated.
 * Use sigc::mem_fun() instead.
 *
 * @param _A_obj Reference to object instance the functor should operate on.
 * @param _A_func Pointer to method that should be wrapped.
 * @return Functor that executes _A_func on invokation.
 *
 * @deprecated Use sigc::mem_fun() instead.
 * @ingroup compat
 */
template <class T_return, class T_arg1, class T_obj>
inline Slot1<T_return, T_arg1>
slot_class(const T_obj& _A_obj, T_return (T_obj::*_A_func)(T_arg1) const)
{ return ::sigc::bound_const_mem_functor1<T_return, T_obj, T_arg1>(_A_obj, _A_func); }

/** Creates a functor of type SigC::Slot2 that encapsulates a const method and an object instance.
 *
 * This function is part of the compatibility module and therefore deprecated.
 * Use sigc::mem_fun() instead.
 *
 * @param _A_obj Reference to object instance the functor should operate on.
 * @param _A_func Pointer to method that should be wrapped.
 * @return Functor that executes _A_func on invokation.
 *
 * @deprecated Use sigc::mem_fun() instead.
 * @ingroup compat
 */
template <class T_return, class T_arg1,class T_arg2, class T_obj>
inline Slot2<T_return, T_arg1,T_arg2>
slot_class(const T_obj& _A_obj, T_return (T_obj::*_A_func)(T_arg1,T_arg2) const)
{ return ::sigc::bound_const_mem_functor2<T_return, T_obj, T_arg1,T_arg2>(_A_obj, _A_func); }

/** Creates a functor of type SigC::Slot3 that encapsulates a const method and an object instance.
 *
 * This function is part of the compatibility module and therefore deprecated.
 * Use sigc::mem_fun() instead.
 *
 * @param _A_obj Reference to object instance the functor should operate on.
 * @param _A_func Pointer to method that should be wrapped.
 * @return Functor that executes _A_func on invokation.
 *
 * @deprecated Use sigc::mem_fun() instead.
 * @ingroup compat
 */
template <class T_return, class T_arg1,class T_arg2,class T_arg3, class T_obj>
inline Slot3<T_return, T_arg1,T_arg2,T_arg3>
slot_class(const T_obj& _A_obj, T_return (T_obj::*_A_func)(T_arg1,T_arg2,T_arg3) const)
{ return ::sigc::bound_const_mem_functor3<T_return, T_obj, T_arg1,T_arg2,T_arg3>(_A_obj, _A_func); }

/** Creates a functor of type SigC::Slot4 that encapsulates a const method and an object instance.
 *
 * This function is part of the compatibility module and therefore deprecated.
 * Use sigc::mem_fun() instead.
 *
 * @param _A_obj Reference to object instance the functor should operate on.
 * @param _A_func Pointer to method that should be wrapped.
 * @return Functor that executes _A_func on invokation.
 *
 * @deprecated Use sigc::mem_fun() instead.
 * @ingroup compat
 */
template <class T_return, class T_arg1,class T_arg2,class T_arg3,class T_arg4, class T_obj>
inline Slot4<T_return, T_arg1,T_arg2,T_arg3,T_arg4>
slot_class(const T_obj& _A_obj, T_return (T_obj::*_A_func)(T_arg1,T_arg2,T_arg3,T_arg4) const)
{ return ::sigc::bound_const_mem_functor4<T_return, T_obj, T_arg1,T_arg2,T_arg3,T_arg4>(_A_obj, _A_func); }

/** Creates a functor of type SigC::Slot5 that encapsulates a const method and an object instance.
 *
 * This function is part of the compatibility module and therefore deprecated.
 * Use sigc::mem_fun() instead.
 *
 * @param _A_obj Reference to object instance the functor should operate on.
 * @param _A_func Pointer to method that should be wrapped.
 * @return Functor that executes _A_func on invokation.
 *
 * @deprecated Use sigc::mem_fun() instead.
 * @ingroup compat
 */
template <class T_return, class T_arg1,class T_arg2,class T_arg3,class T_arg4,class T_arg5, class T_obj>
inline Slot5<T_return, T_arg1,T_arg2,T_arg3,T_arg4,T_arg5>
slot_class(const T_obj& _A_obj, T_return (T_obj::*_A_func)(T_arg1,T_arg2,T_arg3,T_arg4,T_arg5) const)
{ return ::sigc::bound_const_mem_functor5<T_return, T_obj, T_arg1,T_arg2,T_arg3,T_arg4,T_arg5>(_A_obj, _A_func); }

/** Creates a functor of type SigC::Slot6 that encapsulates a const method and an object instance.
 *
 * This function is part of the compatibility module and therefore deprecated.
 * Use sigc::mem_fun() instead.
 *
 * @param _A_obj Reference to object instance the functor should operate on.
 * @param _A_func Pointer to method that should be wrapped.
 * @return Functor that executes _A_func on invokation.
 *
 * @deprecated Use sigc::mem_fun() instead.
 * @ingroup compat
 */
template <class T_return, class T_arg1,class T_arg2,class T_arg3,class T_arg4,class T_arg5,class T_arg6, class T_obj>
inline Slot6<T_return, T_arg1,T_arg2,T_arg3,T_arg4,T_arg5,T_arg6>
slot_class(const T_obj& _A_obj, T_return (T_obj::*_A_func)(T_arg1,T_arg2,T_arg3,T_arg4,T_arg5,T_arg6) const)
{ return ::sigc::bound_const_mem_functor6<T_return, T_obj, T_arg1,T_arg2,T_arg3,T_arg4,T_arg5,T_arg6>(_A_obj, _A_func); }

/** Creates a functor of type SigC::Slot7 that encapsulates a const method and an object instance.
 *
 * This function is part of the compatibility module and therefore deprecated.
 * Use sigc::mem_fun() instead.
 *
 * @param _A_obj Reference to object instance the functor should operate on.
 * @param _A_func Pointer to method that should be wrapped.
 * @return Functor that executes _A_func on invokation.
 *
 * @deprecated Use sigc::mem_fun() instead.
 * @ingroup compat
 */
template <class T_return, class T_arg1,class T_arg2,class T_arg3,class T_arg4,class T_arg5,class T_arg6,class T_arg7, class T_obj>
inline Slot7<T_return, T_arg1,T_arg2,T_arg3,T_arg4,T_arg5,T_arg6,T_arg7>
slot_class(const T_obj& _A_obj, T_return (T_obj::*_A_func)(T_arg1,T_arg2,T_arg3,T_arg4,T_arg5,T_arg6,T_arg7) const)
{ return ::sigc::bound_const_mem_functor7<T_return, T_obj, T_arg1,T_arg2,T_arg3,T_arg4,T_arg5,T_arg6,T_arg7>(_A_obj, _A_func); }


/** Creates a functor of type SigC::Slot0 that encapsulates a volatile method and an object instance.
 *
 * This function is part of the compatibility module and therefore deprecated.
 * Use sigc::mem_fun() instead.
 *
 * @param _A_obj Reference to object instance the functor should operate on.
 * @param _A_func Pointer to method that should be wrapped.
 * @return Functor that executes _A_func on invokation.
 *
 * @deprecated Use sigc::mem_fun() instead.
 * @ingroup compat
 */
template <class T_return, class T_obj>
inline Slot0<T_return>
slot_class( T_obj& _A_obj, T_return (T_obj::*_A_func)() volatile)
{ return ::sigc::bound_volatile_mem_functor0<T_return, T_obj>(_A_obj, _A_func); }

/** Creates a functor of type SigC::Slot1 that encapsulates a volatile method and an object instance.
 *
 * This function is part of the compatibility module and therefore deprecated.
 * Use sigc::mem_fun() instead.
 *
 * @param _A_obj Reference to object instance the functor should operate on.
 * @param _A_func Pointer to method that should be wrapped.
 * @return Functor that executes _A_func on invokation.
 *
 * @deprecated Use sigc::mem_fun() instead.
 * @ingroup compat
 */
template <class T_return, class T_arg1, class T_obj>
inline Slot1<T_return, T_arg1>
slot_class( T_obj& _A_obj, T_return (T_obj::*_A_func)(T_arg1) volatile)
{ return ::sigc::bound_volatile_mem_functor1<T_return, T_obj, T_arg1>(_A_obj, _A_func); }

/** Creates a functor of type SigC::Slot2 that encapsulates a volatile method and an object instance.
 *
 * This function is part of the compatibility module and therefore deprecated.
 * Use sigc::mem_fun() instead.
 *
 * @param _A_obj Reference to object instance the functor should operate on.
 * @param _A_func Pointer to method that should be wrapped.
 * @return Functor that executes _A_func on invokation.
 *
 * @deprecated Use sigc::mem_fun() instead.
 * @ingroup compat
 */
template <class T_return, class T_arg1,class T_arg2, class T_obj>
inline Slot2<T_return, T_arg1,T_arg2>
slot_class( T_obj& _A_obj, T_return (T_obj::*_A_func)(T_arg1,T_arg2) volatile)
{ return ::sigc::bound_volatile_mem_functor2<T_return, T_obj, T_arg1,T_arg2>(_A_obj, _A_func); }

/** Creates a functor of type SigC::Slot3 that encapsulates a volatile method and an object instance.
 *
 * This function is part of the compatibility module and therefore deprecated.
 * Use sigc::mem_fun() instead.
 *
 * @param _A_obj Reference to object instance the functor should operate on.
 * @param _A_func Pointer to method that should be wrapped.
 * @return Functor that executes _A_func on invokation.
 *
 * @deprecated Use sigc::mem_fun() instead.
 * @ingroup compat
 */
template <class T_return, class T_arg1,class T_arg2,class T_arg3, class T_obj>
inline Slot3<T_return, T_arg1,T_arg2,T_arg3>
slot_class( T_obj& _A_obj, T_return (T_obj::*_A_func)(T_arg1,T_arg2,T_arg3) volatile)
{ return ::sigc::bound_volatile_mem_functor3<T_return, T_obj, T_arg1,T_arg2,T_arg3>(_A_obj, _A_func); }

/** Creates a functor of type SigC::Slot4 that encapsulates a volatile method and an object instance.
 *
 * This function is part of the compatibility module and therefore deprecated.
 * Use sigc::mem_fun() instead.
 *
 * @param _A_obj Reference to object instance the functor should operate on.
 * @param _A_func Pointer to method that should be wrapped.
 * @return Functor that executes _A_func on invokation.
 *
 * @deprecated Use sigc::mem_fun() instead.
 * @ingroup compat
 */
template <class T_return, class T_arg1,class T_arg2,class T_arg3,class T_arg4, class T_obj>
inline Slot4<T_return, T_arg1,T_arg2,T_arg3,T_arg4>
slot_class( T_obj& _A_obj, T_return (T_obj::*_A_func)(T_arg1,T_arg2,T_arg3,T_arg4) volatile)
{ return ::sigc::bound_volatile_mem_functor4<T_return, T_obj, T_arg1,T_arg2,T_arg3,T_arg4>(_A_obj, _A_func); }

/** Creates a functor of type SigC::Slot5 that encapsulates a volatile method and an object instance.
 *
 * This function is part of the compatibility module and therefore deprecated.
 * Use sigc::mem_fun() instead.
 *
 * @param _A_obj Reference to object instance the functor should operate on.
 * @param _A_func Pointer to method that should be wrapped.
 * @return Functor that executes _A_func on invokation.
 *
 * @deprecated Use sigc::mem_fun() instead.
 * @ingroup compat
 */
template <class T_return, class T_arg1,class T_arg2,class T_arg3,class T_arg4,class T_arg5, class T_obj>
inline Slot5<T_return, T_arg1,T_arg2,T_arg3,T_arg4,T_arg5>
slot_class( T_obj& _A_obj, T_return (T_obj::*_A_func)(T_arg1,T_arg2,T_arg3,T_arg4,T_arg5) volatile)
{ return ::sigc::bound_volatile_mem_functor5<T_return, T_obj, T_arg1,T_arg2,T_arg3,T_arg4,T_arg5>(_A_obj, _A_func); }

/** Creates a functor of type SigC::Slot6 that encapsulates a volatile method and an object instance.
 *
 * This function is part of the compatibility module and therefore deprecated.
 * Use sigc::mem_fun() instead.
 *
 * @param _A_obj Reference to object instance the functor should operate on.
 * @param _A_func Pointer to method that should be wrapped.
 * @return Functor that executes _A_func on invokation.
 *
 * @deprecated Use sigc::mem_fun() instead.
 * @ingroup compat
 */
template <class T_return, class T_arg1,class T_arg2,class T_arg3,class T_arg4,class T_arg5,class T_arg6, class T_obj>
inline Slot6<T_return, T_arg1,T_arg2,T_arg3,T_arg4,T_arg5,T_arg6>
slot_class( T_obj& _A_obj, T_return (T_obj::*_A_func)(T_arg1,T_arg2,T_arg3,T_arg4,T_arg5,T_arg6) volatile)
{ return ::sigc::bound_volatile_mem_functor6<T_return, T_obj, T_arg1,T_arg2,T_arg3,T_arg4,T_arg5,T_arg6>(_A_obj, _A_func); }

/** Creates a functor of type SigC::Slot7 that encapsulates a volatile method and an object instance.
 *
 * This function is part of the compatibility module and therefore deprecated.
 * Use sigc::mem_fun() instead.
 *
 * @param _A_obj Reference to object instance the functor should operate on.
 * @param _A_func Pointer to method that should be wrapped.
 * @return Functor that executes _A_func on invokation.
 *
 * @deprecated Use sigc::mem_fun() instead.
 * @ingroup compat
 */
template <class T_return, class T_arg1,class T_arg2,class T_arg3,class T_arg4,class T_arg5,class T_arg6,class T_arg7, class T_obj>
inline Slot7<T_return, T_arg1,T_arg2,T_arg3,T_arg4,T_arg5,T_arg6,T_arg7>
slot_class( T_obj& _A_obj, T_return (T_obj::*_A_func)(T_arg1,T_arg2,T_arg3,T_arg4,T_arg5,T_arg6,T_arg7) volatile)
{ return ::sigc::bound_volatile_mem_functor7<T_return, T_obj, T_arg1,T_arg2,T_arg3,T_arg4,T_arg5,T_arg6,T_arg7>(_A_obj, _A_func); }


/** Creates a functor of type SigC::Slot0 that encapsulates a const volatile method and an object instance.
 *
 * This function is part of the compatibility module and therefore deprecated.
 * Use sigc::mem_fun() instead.
 *
 * @param _A_obj Reference to object instance the functor should operate on.
 * @param _A_func Pointer to method that should be wrapped.
 * @return Functor that executes _A_func on invokation.
 *
 * @deprecated Use sigc::mem_fun() instead.
 * @ingroup compat
 */
template <class T_return, class T_obj>
inline Slot0<T_return>
slot_class(const T_obj& _A_obj, T_return (T_obj::*_A_func)() const volatile)
{ return ::sigc::bound_const_volatile_mem_functor0<T_return, T_obj>(_A_obj, _A_func); }

/** Creates a functor of type SigC::Slot1 that encapsulates a const volatile method and an object instance.
 *
 * This function is part of the compatibility module and therefore deprecated.
 * Use sigc::mem_fun() instead.
 *
 * @param _A_obj Reference to object instance the functor should operate on.
 * @param _A_func Pointer to method that should be wrapped.
 * @return Functor that executes _A_func on invokation.
 *
 * @deprecated Use sigc::mem_fun() instead.
 * @ingroup compat
 */
template <class T_return, class T_arg1, class T_obj>
inline Slot1<T_return, T_arg1>
slot_class(const T_obj& _A_obj, T_return (T_obj::*_A_func)(T_arg1) const volatile)
{ return ::sigc::bound_const_volatile_mem_functor1<T_return, T_obj, T_arg1>(_A_obj, _A_func); }

/** Creates a functor of type SigC::Slot2 that encapsulates a const volatile method and an object instance.
 *
 * This function is part of the compatibility module and therefore deprecated.
 * Use sigc::mem_fun() instead.
 *
 * @param _A_obj Reference to object instance the functor should operate on.
 * @param _A_func Pointer to method that should be wrapped.
 * @return Functor that executes _A_func on invokation.
 *
 * @deprecated Use sigc::mem_fun() instead.
 * @ingroup compat
 */
template <class T_return, class T_arg1,class T_arg2, class T_obj>
inline Slot2<T_return, T_arg1,T_arg2>
slot_class(const T_obj& _A_obj, T_return (T_obj::*_A_func)(T_arg1,T_arg2) const volatile)
{ return ::sigc::bound_const_volatile_mem_functor2<T_return, T_obj, T_arg1,T_arg2>(_A_obj, _A_func); }

/** Creates a functor of type SigC::Slot3 that encapsulates a const volatile method and an object instance.
 *
 * This function is part of the compatibility module and therefore deprecated.
 * Use sigc::mem_fun() instead.
 *
 * @param _A_obj Reference to object instance the functor should operate on.
 * @param _A_func Pointer to method that should be wrapped.
 * @return Functor that executes _A_func on invokation.
 *
 * @deprecated Use sigc::mem_fun() instead.
 * @ingroup compat
 */
template <class T_return, class T_arg1,class T_arg2,class T_arg3, class T_obj>
inline Slot3<T_return, T_arg1,T_arg2,T_arg3>
slot_class(const T_obj& _A_obj, T_return (T_obj::*_A_func)(T_arg1,T_arg2,T_arg3) const volatile)
{ return ::sigc::bound_const_volatile_mem_functor3<T_return, T_obj, T_arg1,T_arg2,T_arg3>(_A_obj, _A_func); }

/** Creates a functor of type SigC::Slot4 that encapsulates a const volatile method and an object instance.
 *
 * This function is part of the compatibility module and therefore deprecated.
 * Use sigc::mem_fun() instead.
 *
 * @param _A_obj Reference to object instance the functor should operate on.
 * @param _A_func Pointer to method that should be wrapped.
 * @return Functor that executes _A_func on invokation.
 *
 * @deprecated Use sigc::mem_fun() instead.
 * @ingroup compat
 */
template <class T_return, class T_arg1,class T_arg2,class T_arg3,class T_arg4, class T_obj>
inline Slot4<T_return, T_arg1,T_arg2,T_arg3,T_arg4>
slot_class(const T_obj& _A_obj, T_return (T_obj::*_A_func)(T_arg1,T_arg2,T_arg3,T_arg4) const volatile)
{ return ::sigc::bound_const_volatile_mem_functor4<T_return, T_obj, T_arg1,T_arg2,T_arg3,T_arg4>(_A_obj, _A_func); }

/** Creates a functor of type SigC::Slot5 that encapsulates a const volatile method and an object instance.
 *
 * This function is part of the compatibility module and therefore deprecated.
 * Use sigc::mem_fun() instead.
 *
 * @param _A_obj Reference to object instance the functor should operate on.
 * @param _A_func Pointer to method that should be wrapped.
 * @return Functor that executes _A_func on invokation.
 *
 * @deprecated Use sigc::mem_fun() instead.
 * @ingroup compat
 */
template <class T_return, class T_arg1,class T_arg2,class T_arg3,class T_arg4,class T_arg5, class T_obj>
inline Slot5<T_return, T_arg1,T_arg2,T_arg3,T_arg4,T_arg5>
slot_class(const T_obj& _A_obj, T_return (T_obj::*_A_func)(T_arg1,T_arg2,T_arg3,T_arg4,T_arg5) const volatile)
{ return ::sigc::bound_const_volatile_mem_functor5<T_return, T_obj, T_arg1,T_arg2,T_arg3,T_arg4,T_arg5>(_A_obj, _A_func); }

/** Creates a functor of type SigC::Slot6 that encapsulates a const volatile method and an object instance.
 *
 * This function is part of the compatibility module and therefore deprecated.
 * Use sigc::mem_fun() instead.
 *
 * @param _A_obj Reference to object instance the functor should operate on.
 * @param _A_func Pointer to method that should be wrapped.
 * @return Functor that executes _A_func on invokation.
 *
 * @deprecated Use sigc::mem_fun() instead.
 * @ingroup compat
 */
template <class T_return, class T_arg1,class T_arg2,class T_arg3,class T_arg4,class T_arg5,class T_arg6, class T_obj>
inline Slot6<T_return, T_arg1,T_arg2,T_arg3,T_arg4,T_arg5,T_arg6>
slot_class(const T_obj& _A_obj, T_return (T_obj::*_A_func)(T_arg1,T_arg2,T_arg3,T_arg4,T_arg5,T_arg6) const volatile)
{ return ::sigc::bound_const_volatile_mem_functor6<T_return, T_obj, T_arg1,T_arg2,T_arg3,T_arg4,T_arg5,T_arg6>(_A_obj, _A_func); }

/** Creates a functor of type SigC::Slot7 that encapsulates a const volatile method and an object instance.
 *
 * This function is part of the compatibility module and therefore deprecated.
 * Use sigc::mem_fun() instead.
 *
 * @param _A_obj Reference to object instance the functor should operate on.
 * @param _A_func Pointer to method that should be wrapped.
 * @return Functor that executes _A_func on invokation.
 *
 * @deprecated Use sigc::mem_fun() instead.
 * @ingroup compat
 */
template <class T_return, class T_arg1,class T_arg2,class T_arg3,class T_arg4,class T_arg5,class T_arg6,class T_arg7, class T_obj>
inline Slot7<T_return, T_arg1,T_arg2,T_arg3,T_arg4,T_arg5,T_arg6,T_arg7>
slot_class(const T_obj& _A_obj, T_return (T_obj::*_A_func)(T_arg1,T_arg2,T_arg3,T_arg4,T_arg5,T_arg6,T_arg7) const volatile)
{ return ::sigc::bound_const_volatile_mem_functor7<T_return, T_obj, T_arg1,T_arg2,T_arg3,T_arg4,T_arg5,T_arg6,T_arg7>(_A_obj, _A_func); }



}

#endif
#endif /* _SIGC_MACROS_CLASS_SLOTHM4_ */
