// -*- c++ -*-
/* Do not edit! -- generated file */


#ifndef _SIGC_MACROS_METHOD_SLOTHM4_
#define _SIGC_MACROS_METHOD_SLOTHM4_

#include <sigc++/slot.h>
#include <sigc++/functors/mem_fun.h>

#ifndef LIBSIGC_DISABLE_DEPRECATED

namespace SigC {

/** Creates a functor of type Sigc::Slot1 that wraps a  method.
 *
 * @param _A_func Pointer to method that should be wrapped.
 * @return Functor that executes _A_func on invokation.
 *
 * @deprecated Use sigc::mem_fun() instead.
 * @ingroup compat
 */
template <class T_return, class T_obj>
inline Slot1<T_return, T_obj&>
slot(T_return (T_obj::*_A_func)() )
{ return ::sigc::mem_functor0<T_return, T_obj>(_A_func); }

/** Creates a functor of type Sigc::Slot2 that wraps a  method.
 *
 * @param _A_func Pointer to method that should be wrapped.
 * @return Functor that executes _A_func on invokation.
 *
 * @deprecated Use sigc::mem_fun() instead.
 * @ingroup compat
 */
template <class T_return, class T_arg1, class T_obj>
inline Slot2<T_return, T_obj&, T_arg1>
slot(T_return (T_obj::*_A_func)(T_arg1) )
{ return ::sigc::mem_functor1<T_return, T_obj, T_arg1>(_A_func); }

/** Creates a functor of type Sigc::Slot3 that wraps a  method.
 *
 * @param _A_func Pointer to method that should be wrapped.
 * @return Functor that executes _A_func on invokation.
 *
 * @deprecated Use sigc::mem_fun() instead.
 * @ingroup compat
 */
template <class T_return, class T_arg1,class T_arg2, class T_obj>
inline Slot3<T_return, T_obj&, T_arg1,T_arg2>
slot(T_return (T_obj::*_A_func)(T_arg1,T_arg2) )
{ return ::sigc::mem_functor2<T_return, T_obj, T_arg1,T_arg2>(_A_func); }

/** Creates a functor of type Sigc::Slot4 that wraps a  method.
 *
 * @param _A_func Pointer to method that should be wrapped.
 * @return Functor that executes _A_func on invokation.
 *
 * @deprecated Use sigc::mem_fun() instead.
 * @ingroup compat
 */
template <class T_return, class T_arg1,class T_arg2,class T_arg3, class T_obj>
inline Slot4<T_return, T_obj&, T_arg1,T_arg2,T_arg3>
slot(T_return (T_obj::*_A_func)(T_arg1,T_arg2,T_arg3) )
{ return ::sigc::mem_functor3<T_return, T_obj, T_arg1,T_arg2,T_arg3>(_A_func); }

/** Creates a functor of type Sigc::Slot5 that wraps a  method.
 *
 * @param _A_func Pointer to method that should be wrapped.
 * @return Functor that executes _A_func on invokation.
 *
 * @deprecated Use sigc::mem_fun() instead.
 * @ingroup compat
 */
template <class T_return, class T_arg1,class T_arg2,class T_arg3,class T_arg4, class T_obj>
inline Slot5<T_return, T_obj&, T_arg1,T_arg2,T_arg3,T_arg4>
slot(T_return (T_obj::*_A_func)(T_arg1,T_arg2,T_arg3,T_arg4) )
{ return ::sigc::mem_functor4<T_return, T_obj, T_arg1,T_arg2,T_arg3,T_arg4>(_A_func); }

/** Creates a functor of type Sigc::Slot6 that wraps a  method.
 *
 * @param _A_func Pointer to method that should be wrapped.
 * @return Functor that executes _A_func on invokation.
 *
 * @deprecated Use sigc::mem_fun() instead.
 * @ingroup compat
 */
template <class T_return, class T_arg1,class T_arg2,class T_arg3,class T_arg4,class T_arg5, class T_obj>
inline Slot6<T_return, T_obj&, T_arg1,T_arg2,T_arg3,T_arg4,T_arg5>
slot(T_return (T_obj::*_A_func)(T_arg1,T_arg2,T_arg3,T_arg4,T_arg5) )
{ return ::sigc::mem_functor5<T_return, T_obj, T_arg1,T_arg2,T_arg3,T_arg4,T_arg5>(_A_func); }

/** Creates a functor of type Sigc::Slot7 that wraps a  method.
 *
 * @param _A_func Pointer to method that should be wrapped.
 * @return Functor that executes _A_func on invokation.
 *
 * @deprecated Use sigc::mem_fun() instead.
 * @ingroup compat
 */
template <class T_return, class T_arg1,class T_arg2,class T_arg3,class T_arg4,class T_arg5,class T_arg6, class T_obj>
inline Slot7<T_return, T_obj&, T_arg1,T_arg2,T_arg3,T_arg4,T_arg5,T_arg6>
slot(T_return (T_obj::*_A_func)(T_arg1,T_arg2,T_arg3,T_arg4,T_arg5,T_arg6) )
{ return ::sigc::mem_functor6<T_return, T_obj, T_arg1,T_arg2,T_arg3,T_arg4,T_arg5,T_arg6>(_A_func); }


/** Creates a functor of type Sigc::Slot1 that wraps a const method.
 *
 * @param _A_func Pointer to method that should be wrapped.
 * @return Functor that executes _A_func on invokation.
 *
 * @deprecated Use sigc::mem_fun() instead.
 * @ingroup compat
 */
template <class T_return, class T_obj>
inline Slot1<T_return, T_obj&>
slot(T_return (T_obj::*_A_func)() const)
{ return ::sigc::const_mem_functor0<T_return, T_obj>(_A_func); }

/** Creates a functor of type Sigc::Slot2 that wraps a const method.
 *
 * @param _A_func Pointer to method that should be wrapped.
 * @return Functor that executes _A_func on invokation.
 *
 * @deprecated Use sigc::mem_fun() instead.
 * @ingroup compat
 */
template <class T_return, class T_arg1, class T_obj>
inline Slot2<T_return, T_obj&, T_arg1>
slot(T_return (T_obj::*_A_func)(T_arg1) const)
{ return ::sigc::const_mem_functor1<T_return, T_obj, T_arg1>(_A_func); }

/** Creates a functor of type Sigc::Slot3 that wraps a const method.
 *
 * @param _A_func Pointer to method that should be wrapped.
 * @return Functor that executes _A_func on invokation.
 *
 * @deprecated Use sigc::mem_fun() instead.
 * @ingroup compat
 */
template <class T_return, class T_arg1,class T_arg2, class T_obj>
inline Slot3<T_return, T_obj&, T_arg1,T_arg2>
slot(T_return (T_obj::*_A_func)(T_arg1,T_arg2) const)
{ return ::sigc::const_mem_functor2<T_return, T_obj, T_arg1,T_arg2>(_A_func); }

/** Creates a functor of type Sigc::Slot4 that wraps a const method.
 *
 * @param _A_func Pointer to method that should be wrapped.
 * @return Functor that executes _A_func on invokation.
 *
 * @deprecated Use sigc::mem_fun() instead.
 * @ingroup compat
 */
template <class T_return, class T_arg1,class T_arg2,class T_arg3, class T_obj>
inline Slot4<T_return, T_obj&, T_arg1,T_arg2,T_arg3>
slot(T_return (T_obj::*_A_func)(T_arg1,T_arg2,T_arg3) const)
{ return ::sigc::const_mem_functor3<T_return, T_obj, T_arg1,T_arg2,T_arg3>(_A_func); }

/** Creates a functor of type Sigc::Slot5 that wraps a const method.
 *
 * @param _A_func Pointer to method that should be wrapped.
 * @return Functor that executes _A_func on invokation.
 *
 * @deprecated Use sigc::mem_fun() instead.
 * @ingroup compat
 */
template <class T_return, class T_arg1,class T_arg2,class T_arg3,class T_arg4, class T_obj>
inline Slot5<T_return, T_obj&, T_arg1,T_arg2,T_arg3,T_arg4>
slot(T_return (T_obj::*_A_func)(T_arg1,T_arg2,T_arg3,T_arg4) const)
{ return ::sigc::const_mem_functor4<T_return, T_obj, T_arg1,T_arg2,T_arg3,T_arg4>(_A_func); }

/** Creates a functor of type Sigc::Slot6 that wraps a const method.
 *
 * @param _A_func Pointer to method that should be wrapped.
 * @return Functor that executes _A_func on invokation.
 *
 * @deprecated Use sigc::mem_fun() instead.
 * @ingroup compat
 */
template <class T_return, class T_arg1,class T_arg2,class T_arg3,class T_arg4,class T_arg5, class T_obj>
inline Slot6<T_return, T_obj&, T_arg1,T_arg2,T_arg3,T_arg4,T_arg5>
slot(T_return (T_obj::*_A_func)(T_arg1,T_arg2,T_arg3,T_arg4,T_arg5) const)
{ return ::sigc::const_mem_functor5<T_return, T_obj, T_arg1,T_arg2,T_arg3,T_arg4,T_arg5>(_A_func); }

/** Creates a functor of type Sigc::Slot7 that wraps a const method.
 *
 * @param _A_func Pointer to method that should be wrapped.
 * @return Functor that executes _A_func on invokation.
 *
 * @deprecated Use sigc::mem_fun() instead.
 * @ingroup compat
 */
template <class T_return, class T_arg1,class T_arg2,class T_arg3,class T_arg4,class T_arg5,class T_arg6, class T_obj>
inline Slot7<T_return, T_obj&, T_arg1,T_arg2,T_arg3,T_arg4,T_arg5,T_arg6>
slot(T_return (T_obj::*_A_func)(T_arg1,T_arg2,T_arg3,T_arg4,T_arg5,T_arg6) const)
{ return ::sigc::const_mem_functor6<T_return, T_obj, T_arg1,T_arg2,T_arg3,T_arg4,T_arg5,T_arg6>(_A_func); }


/** Creates a functor of type Sigc::Slot1 that wraps a volatile method.
 *
 * @param _A_func Pointer to method that should be wrapped.
 * @return Functor that executes _A_func on invokation.
 *
 * @deprecated Use sigc::mem_fun() instead.
 * @ingroup compat
 */
template <class T_return, class T_obj>
inline Slot1<T_return, T_obj&>
slot(T_return (T_obj::*_A_func)() volatile)
{ return ::sigc::volatile_mem_functor0<T_return, T_obj>(_A_func); }

/** Creates a functor of type Sigc::Slot2 that wraps a volatile method.
 *
 * @param _A_func Pointer to method that should be wrapped.
 * @return Functor that executes _A_func on invokation.
 *
 * @deprecated Use sigc::mem_fun() instead.
 * @ingroup compat
 */
template <class T_return, class T_arg1, class T_obj>
inline Slot2<T_return, T_obj&, T_arg1>
slot(T_return (T_obj::*_A_func)(T_arg1) volatile)
{ return ::sigc::volatile_mem_functor1<T_return, T_obj, T_arg1>(_A_func); }

/** Creates a functor of type Sigc::Slot3 that wraps a volatile method.
 *
 * @param _A_func Pointer to method that should be wrapped.
 * @return Functor that executes _A_func on invokation.
 *
 * @deprecated Use sigc::mem_fun() instead.
 * @ingroup compat
 */
template <class T_return, class T_arg1,class T_arg2, class T_obj>
inline Slot3<T_return, T_obj&, T_arg1,T_arg2>
slot(T_return (T_obj::*_A_func)(T_arg1,T_arg2) volatile)
{ return ::sigc::volatile_mem_functor2<T_return, T_obj, T_arg1,T_arg2>(_A_func); }

/** Creates a functor of type Sigc::Slot4 that wraps a volatile method.
 *
 * @param _A_func Pointer to method that should be wrapped.
 * @return Functor that executes _A_func on invokation.
 *
 * @deprecated Use sigc::mem_fun() instead.
 * @ingroup compat
 */
template <class T_return, class T_arg1,class T_arg2,class T_arg3, class T_obj>
inline Slot4<T_return, T_obj&, T_arg1,T_arg2,T_arg3>
slot(T_return (T_obj::*_A_func)(T_arg1,T_arg2,T_arg3) volatile)
{ return ::sigc::volatile_mem_functor3<T_return, T_obj, T_arg1,T_arg2,T_arg3>(_A_func); }

/** Creates a functor of type Sigc::Slot5 that wraps a volatile method.
 *
 * @param _A_func Pointer to method that should be wrapped.
 * @return Functor that executes _A_func on invokation.
 *
 * @deprecated Use sigc::mem_fun() instead.
 * @ingroup compat
 */
template <class T_return, class T_arg1,class T_arg2,class T_arg3,class T_arg4, class T_obj>
inline Slot5<T_return, T_obj&, T_arg1,T_arg2,T_arg3,T_arg4>
slot(T_return (T_obj::*_A_func)(T_arg1,T_arg2,T_arg3,T_arg4) volatile)
{ return ::sigc::volatile_mem_functor4<T_return, T_obj, T_arg1,T_arg2,T_arg3,T_arg4>(_A_func); }

/** Creates a functor of type Sigc::Slot6 that wraps a volatile method.
 *
 * @param _A_func Pointer to method that should be wrapped.
 * @return Functor that executes _A_func on invokation.
 *
 * @deprecated Use sigc::mem_fun() instead.
 * @ingroup compat
 */
template <class T_return, class T_arg1,class T_arg2,class T_arg3,class T_arg4,class T_arg5, class T_obj>
inline Slot6<T_return, T_obj&, T_arg1,T_arg2,T_arg3,T_arg4,T_arg5>
slot(T_return (T_obj::*_A_func)(T_arg1,T_arg2,T_arg3,T_arg4,T_arg5) volatile)
{ return ::sigc::volatile_mem_functor5<T_return, T_obj, T_arg1,T_arg2,T_arg3,T_arg4,T_arg5>(_A_func); }

/** Creates a functor of type Sigc::Slot7 that wraps a volatile method.
 *
 * @param _A_func Pointer to method that should be wrapped.
 * @return Functor that executes _A_func on invokation.
 *
 * @deprecated Use sigc::mem_fun() instead.
 * @ingroup compat
 */
template <class T_return, class T_arg1,class T_arg2,class T_arg3,class T_arg4,class T_arg5,class T_arg6, class T_obj>
inline Slot7<T_return, T_obj&, T_arg1,T_arg2,T_arg3,T_arg4,T_arg5,T_arg6>
slot(T_return (T_obj::*_A_func)(T_arg1,T_arg2,T_arg3,T_arg4,T_arg5,T_arg6) volatile)
{ return ::sigc::volatile_mem_functor6<T_return, T_obj, T_arg1,T_arg2,T_arg3,T_arg4,T_arg5,T_arg6>(_A_func); }


/** Creates a functor of type Sigc::Slot1 that wraps a const volatile method.
 *
 * @param _A_func Pointer to method that should be wrapped.
 * @return Functor that executes _A_func on invokation.
 *
 * @deprecated Use sigc::mem_fun() instead.
 * @ingroup compat
 */
template <class T_return, class T_obj>
inline Slot1<T_return, T_obj&>
slot(T_return (T_obj::*_A_func)() const volatile)
{ return ::sigc::const_volatile_mem_functor0<T_return, T_obj>(_A_func); }

/** Creates a functor of type Sigc::Slot2 that wraps a const volatile method.
 *
 * @param _A_func Pointer to method that should be wrapped.
 * @return Functor that executes _A_func on invokation.
 *
 * @deprecated Use sigc::mem_fun() instead.
 * @ingroup compat
 */
template <class T_return, class T_arg1, class T_obj>
inline Slot2<T_return, T_obj&, T_arg1>
slot(T_return (T_obj::*_A_func)(T_arg1) const volatile)
{ return ::sigc::const_volatile_mem_functor1<T_return, T_obj, T_arg1>(_A_func); }

/** Creates a functor of type Sigc::Slot3 that wraps a const volatile method.
 *
 * @param _A_func Pointer to method that should be wrapped.
 * @return Functor that executes _A_func on invokation.
 *
 * @deprecated Use sigc::mem_fun() instead.
 * @ingroup compat
 */
template <class T_return, class T_arg1,class T_arg2, class T_obj>
inline Slot3<T_return, T_obj&, T_arg1,T_arg2>
slot(T_return (T_obj::*_A_func)(T_arg1,T_arg2) const volatile)
{ return ::sigc::const_volatile_mem_functor2<T_return, T_obj, T_arg1,T_arg2>(_A_func); }

/** Creates a functor of type Sigc::Slot4 that wraps a const volatile method.
 *
 * @param _A_func Pointer to method that should be wrapped.
 * @return Functor that executes _A_func on invokation.
 *
 * @deprecated Use sigc::mem_fun() instead.
 * @ingroup compat
 */
template <class T_return, class T_arg1,class T_arg2,class T_arg3, class T_obj>
inline Slot4<T_return, T_obj&, T_arg1,T_arg2,T_arg3>
slot(T_return (T_obj::*_A_func)(T_arg1,T_arg2,T_arg3) const volatile)
{ return ::sigc::const_volatile_mem_functor3<T_return, T_obj, T_arg1,T_arg2,T_arg3>(_A_func); }

/** Creates a functor of type Sigc::Slot5 that wraps a const volatile method.
 *
 * @param _A_func Pointer to method that should be wrapped.
 * @return Functor that executes _A_func on invokation.
 *
 * @deprecated Use sigc::mem_fun() instead.
 * @ingroup compat
 */
template <class T_return, class T_arg1,class T_arg2,class T_arg3,class T_arg4, class T_obj>
inline Slot5<T_return, T_obj&, T_arg1,T_arg2,T_arg3,T_arg4>
slot(T_return (T_obj::*_A_func)(T_arg1,T_arg2,T_arg3,T_arg4) const volatile)
{ return ::sigc::const_volatile_mem_functor4<T_return, T_obj, T_arg1,T_arg2,T_arg3,T_arg4>(_A_func); }

/** Creates a functor of type Sigc::Slot6 that wraps a const volatile method.
 *
 * @param _A_func Pointer to method that should be wrapped.
 * @return Functor that executes _A_func on invokation.
 *
 * @deprecated Use sigc::mem_fun() instead.
 * @ingroup compat
 */
template <class T_return, class T_arg1,class T_arg2,class T_arg3,class T_arg4,class T_arg5, class T_obj>
inline Slot6<T_return, T_obj&, T_arg1,T_arg2,T_arg3,T_arg4,T_arg5>
slot(T_return (T_obj::*_A_func)(T_arg1,T_arg2,T_arg3,T_arg4,T_arg5) const volatile)
{ return ::sigc::const_volatile_mem_functor5<T_return, T_obj, T_arg1,T_arg2,T_arg3,T_arg4,T_arg5>(_A_func); }

/** Creates a functor of type Sigc::Slot7 that wraps a const volatile method.
 *
 * @param _A_func Pointer to method that should be wrapped.
 * @return Functor that executes _A_func on invokation.
 *
 * @deprecated Use sigc::mem_fun() instead.
 * @ingroup compat
 */
template <class T_return, class T_arg1,class T_arg2,class T_arg3,class T_arg4,class T_arg5,class T_arg6, class T_obj>
inline Slot7<T_return, T_obj&, T_arg1,T_arg2,T_arg3,T_arg4,T_arg5,T_arg6>
slot(T_return (T_obj::*_A_func)(T_arg1,T_arg2,T_arg3,T_arg4,T_arg5,T_arg6) const volatile)
{ return ::sigc::const_volatile_mem_functor6<T_return, T_obj, T_arg1,T_arg2,T_arg3,T_arg4,T_arg5,T_arg6>(_A_func); }



}

#endif
#endif /* _SIGC_MACROS_METHOD_SLOTHM4_ */
