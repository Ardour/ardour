// -*- c++ -*-
/* Do not edit! -- generated file */


#ifndef _SIGC_MACROS_SLOTHM4_
#define _SIGC_MACROS_SLOTHM4_

#include <sigc++/functors/slot.h>

#ifndef LIBSIGC_DISABLE_DEPRECATED

namespace SigC {

// SlotN
/** Converts an arbitrary functor to a unified type which is opaque.
 * Slot0 itself is a functor or to be more precise a closure. It contains
 * a single, arbitrary functor (or closure) that is executed in operator()().
 *
 * The template arguments determine the function signature of operator()():
 * - @e T_return The return type of operator()().
 *
 * To use simply assign the slot to the desired functor. If the functor
 * is not compatible with the parameter list defined with the template
 * arguments compiler errors are triggered. When called the slot
 * will invoke the functor with minimal copies.
 * block() and unblock() can be used to block the functor's invocation
 * from operator()() temporarily.
 *
 * @par Example:
 *   @code
 *   #include <sigc++/slot.h>
 *   void foo(int) {}
 *   SigC::Slot1<void, long> s = SigC::slot(&foo);
 *   s(19);
 *   @endcode
 *
 * @deprecated Use the unnumbered template sigc::slot instead.
 * @ingroup compat
 */
template <class T_return>
class Slot0
  : public ::sigc::slot<T_return>
{
public:
  typedef ::sigc::slot<T_return> parent_type;

  /// Constructs an empty slot.
  Slot0() {}

  /** Constructs a slot from an arbitrary functor.
   * @param _A_func The desired functor the new slot should be assigned to.
   */
  template <class T_functor>
  Slot0(const T_functor& _A_func)
    : ::sigc::slot<T_return>(_A_func) {}

  /** Constructs a slot, copying an existing one.
   * @param src The existing slot to copy.
   */
  Slot0(const parent_type& src)
    : parent_type(src) {}

  /** Overrides this slot making a copy from another slot.
   * @param src The slot from which to make a copy.
   * @return @p this.
   */
  Slot0& operator=(const parent_type& src)
    { parent_type::operator=(src); return *this; }
};

/** Converts an arbitrary functor to a unified type which is opaque.
 * Slot1 itself is a functor or to be more precise a closure. It contains
 * a single, arbitrary functor (or closure) that is executed in operator()().
 *
 * The template arguments determine the function signature of operator()():
 * - @e T_return The return type of operator()().
 * - @e T_arg1 Argument type used in the definition of operator()().
 *
 * To use simply assign the slot to the desired functor. If the functor
 * is not compatible with the parameter list defined with the template
 * arguments compiler errors are triggered. When called the slot
 * will invoke the functor with minimal copies.
 * block() and unblock() can be used to block the functor's invocation
 * from operator()() temporarily.
 *
 * @par Example:
 *   @code
 *   #include <sigc++/slot.h>
 *   void foo(int) {}
 *   SigC::Slot1<void, long> s = SigC::slot(&foo);
 *   s(19);
 *   @endcode
 *
 * @deprecated Use the unnumbered template sigc::slot instead.
 * @ingroup compat
 */
template <class T_return, class T_arg1>
class Slot1
  : public ::sigc::slot<T_return, T_arg1>
{
public:
  typedef ::sigc::slot<T_return, T_arg1> parent_type;

  /// Constructs an empty slot.
  Slot1() {}

  /** Constructs a slot from an arbitrary functor.
   * @param _A_func The desired functor the new slot should be assigned to.
   */
  template <class T_functor>
  Slot1(const T_functor& _A_func)
    : ::sigc::slot<T_return, T_arg1>(_A_func) {}

  /** Constructs a slot, copying an existing one.
   * @param src The existing slot to copy.
   */
  Slot1(const parent_type& src)
    : parent_type(src) {}

  /** Overrides this slot making a copy from another slot.
   * @param src The slot from which to make a copy.
   * @return @p this.
   */
  Slot1& operator=(const parent_type& src)
    { parent_type::operator=(src); return *this; }
};

/** Converts an arbitrary functor to a unified type which is opaque.
 * Slot2 itself is a functor or to be more precise a closure. It contains
 * a single, arbitrary functor (or closure) that is executed in operator()().
 *
 * The template arguments determine the function signature of operator()():
 * - @e T_return The return type of operator()().
 * - @e T_arg1 Argument type used in the definition of operator()().
 * - @e T_arg2 Argument type used in the definition of operator()().
 *
 * To use simply assign the slot to the desired functor. If the functor
 * is not compatible with the parameter list defined with the template
 * arguments compiler errors are triggered. When called the slot
 * will invoke the functor with minimal copies.
 * block() and unblock() can be used to block the functor's invocation
 * from operator()() temporarily.
 *
 * @par Example:
 *   @code
 *   #include <sigc++/slot.h>
 *   void foo(int) {}
 *   SigC::Slot1<void, long> s = SigC::slot(&foo);
 *   s(19);
 *   @endcode
 *
 * @deprecated Use the unnumbered template sigc::slot instead.
 * @ingroup compat
 */
template <class T_return, class T_arg1,class T_arg2>
class Slot2
  : public ::sigc::slot<T_return, T_arg1,T_arg2>
{
public:
  typedef ::sigc::slot<T_return, T_arg1,T_arg2> parent_type;

  /// Constructs an empty slot.
  Slot2() {}

  /** Constructs a slot from an arbitrary functor.
   * @param _A_func The desired functor the new slot should be assigned to.
   */
  template <class T_functor>
  Slot2(const T_functor& _A_func)
    : ::sigc::slot<T_return, T_arg1,T_arg2>(_A_func) {}

  /** Constructs a slot, copying an existing one.
   * @param src The existing slot to copy.
   */
  Slot2(const parent_type& src)
    : parent_type(src) {}

  /** Overrides this slot making a copy from another slot.
   * @param src The slot from which to make a copy.
   * @return @p this.
   */
  Slot2& operator=(const parent_type& src)
    { parent_type::operator=(src); return *this; }
};

/** Converts an arbitrary functor to a unified type which is opaque.
 * Slot3 itself is a functor or to be more precise a closure. It contains
 * a single, arbitrary functor (or closure) that is executed in operator()().
 *
 * The template arguments determine the function signature of operator()():
 * - @e T_return The return type of operator()().
 * - @e T_arg1 Argument type used in the definition of operator()().
 * - @e T_arg2 Argument type used in the definition of operator()().
 * - @e T_arg3 Argument type used in the definition of operator()().
 *
 * To use simply assign the slot to the desired functor. If the functor
 * is not compatible with the parameter list defined with the template
 * arguments compiler errors are triggered. When called the slot
 * will invoke the functor with minimal copies.
 * block() and unblock() can be used to block the functor's invocation
 * from operator()() temporarily.
 *
 * @par Example:
 *   @code
 *   #include <sigc++/slot.h>
 *   void foo(int) {}
 *   SigC::Slot1<void, long> s = SigC::slot(&foo);
 *   s(19);
 *   @endcode
 *
 * @deprecated Use the unnumbered template sigc::slot instead.
 * @ingroup compat
 */
template <class T_return, class T_arg1,class T_arg2,class T_arg3>
class Slot3
  : public ::sigc::slot<T_return, T_arg1,T_arg2,T_arg3>
{
public:
  typedef ::sigc::slot<T_return, T_arg1,T_arg2,T_arg3> parent_type;

  /// Constructs an empty slot.
  Slot3() {}

  /** Constructs a slot from an arbitrary functor.
   * @param _A_func The desired functor the new slot should be assigned to.
   */
  template <class T_functor>
  Slot3(const T_functor& _A_func)
    : ::sigc::slot<T_return, T_arg1,T_arg2,T_arg3>(_A_func) {}

  /** Constructs a slot, copying an existing one.
   * @param src The existing slot to copy.
   */
  Slot3(const parent_type& src)
    : parent_type(src) {}

  /** Overrides this slot making a copy from another slot.
   * @param src The slot from which to make a copy.
   * @return @p this.
   */
  Slot3& operator=(const parent_type& src)
    { parent_type::operator=(src); return *this; }
};

/** Converts an arbitrary functor to a unified type which is opaque.
 * Slot4 itself is a functor or to be more precise a closure. It contains
 * a single, arbitrary functor (or closure) that is executed in operator()().
 *
 * The template arguments determine the function signature of operator()():
 * - @e T_return The return type of operator()().
 * - @e T_arg1 Argument type used in the definition of operator()().
 * - @e T_arg2 Argument type used in the definition of operator()().
 * - @e T_arg3 Argument type used in the definition of operator()().
 * - @e T_arg4 Argument type used in the definition of operator()().
 *
 * To use simply assign the slot to the desired functor. If the functor
 * is not compatible with the parameter list defined with the template
 * arguments compiler errors are triggered. When called the slot
 * will invoke the functor with minimal copies.
 * block() and unblock() can be used to block the functor's invocation
 * from operator()() temporarily.
 *
 * @par Example:
 *   @code
 *   #include <sigc++/slot.h>
 *   void foo(int) {}
 *   SigC::Slot1<void, long> s = SigC::slot(&foo);
 *   s(19);
 *   @endcode
 *
 * @deprecated Use the unnumbered template sigc::slot instead.
 * @ingroup compat
 */
template <class T_return, class T_arg1,class T_arg2,class T_arg3,class T_arg4>
class Slot4
  : public ::sigc::slot<T_return, T_arg1,T_arg2,T_arg3,T_arg4>
{
public:
  typedef ::sigc::slot<T_return, T_arg1,T_arg2,T_arg3,T_arg4> parent_type;

  /// Constructs an empty slot.
  Slot4() {}

  /** Constructs a slot from an arbitrary functor.
   * @param _A_func The desired functor the new slot should be assigned to.
   */
  template <class T_functor>
  Slot4(const T_functor& _A_func)
    : ::sigc::slot<T_return, T_arg1,T_arg2,T_arg3,T_arg4>(_A_func) {}

  /** Constructs a slot, copying an existing one.
   * @param src The existing slot to copy.
   */
  Slot4(const parent_type& src)
    : parent_type(src) {}

  /** Overrides this slot making a copy from another slot.
   * @param src The slot from which to make a copy.
   * @return @p this.
   */
  Slot4& operator=(const parent_type& src)
    { parent_type::operator=(src); return *this; }
};

/** Converts an arbitrary functor to a unified type which is opaque.
 * Slot5 itself is a functor or to be more precise a closure. It contains
 * a single, arbitrary functor (or closure) that is executed in operator()().
 *
 * The template arguments determine the function signature of operator()():
 * - @e T_return The return type of operator()().
 * - @e T_arg1 Argument type used in the definition of operator()().
 * - @e T_arg2 Argument type used in the definition of operator()().
 * - @e T_arg3 Argument type used in the definition of operator()().
 * - @e T_arg4 Argument type used in the definition of operator()().
 * - @e T_arg5 Argument type used in the definition of operator()().
 *
 * To use simply assign the slot to the desired functor. If the functor
 * is not compatible with the parameter list defined with the template
 * arguments compiler errors are triggered. When called the slot
 * will invoke the functor with minimal copies.
 * block() and unblock() can be used to block the functor's invocation
 * from operator()() temporarily.
 *
 * @par Example:
 *   @code
 *   #include <sigc++/slot.h>
 *   void foo(int) {}
 *   SigC::Slot1<void, long> s = SigC::slot(&foo);
 *   s(19);
 *   @endcode
 *
 * @deprecated Use the unnumbered template sigc::slot instead.
 * @ingroup compat
 */
template <class T_return, class T_arg1,class T_arg2,class T_arg3,class T_arg4,class T_arg5>
class Slot5
  : public ::sigc::slot<T_return, T_arg1,T_arg2,T_arg3,T_arg4,T_arg5>
{
public:
  typedef ::sigc::slot<T_return, T_arg1,T_arg2,T_arg3,T_arg4,T_arg5> parent_type;

  /// Constructs an empty slot.
  Slot5() {}

  /** Constructs a slot from an arbitrary functor.
   * @param _A_func The desired functor the new slot should be assigned to.
   */
  template <class T_functor>
  Slot5(const T_functor& _A_func)
    : ::sigc::slot<T_return, T_arg1,T_arg2,T_arg3,T_arg4,T_arg5>(_A_func) {}

  /** Constructs a slot, copying an existing one.
   * @param src The existing slot to copy.
   */
  Slot5(const parent_type& src)
    : parent_type(src) {}

  /** Overrides this slot making a copy from another slot.
   * @param src The slot from which to make a copy.
   * @return @p this.
   */
  Slot5& operator=(const parent_type& src)
    { parent_type::operator=(src); return *this; }
};

/** Converts an arbitrary functor to a unified type which is opaque.
 * Slot6 itself is a functor or to be more precise a closure. It contains
 * a single, arbitrary functor (or closure) that is executed in operator()().
 *
 * The template arguments determine the function signature of operator()():
 * - @e T_return The return type of operator()().
 * - @e T_arg1 Argument type used in the definition of operator()().
 * - @e T_arg2 Argument type used in the definition of operator()().
 * - @e T_arg3 Argument type used in the definition of operator()().
 * - @e T_arg4 Argument type used in the definition of operator()().
 * - @e T_arg5 Argument type used in the definition of operator()().
 * - @e T_arg6 Argument type used in the definition of operator()().
 *
 * To use simply assign the slot to the desired functor. If the functor
 * is not compatible with the parameter list defined with the template
 * arguments compiler errors are triggered. When called the slot
 * will invoke the functor with minimal copies.
 * block() and unblock() can be used to block the functor's invocation
 * from operator()() temporarily.
 *
 * @par Example:
 *   @code
 *   #include <sigc++/slot.h>
 *   void foo(int) {}
 *   SigC::Slot1<void, long> s = SigC::slot(&foo);
 *   s(19);
 *   @endcode
 *
 * @deprecated Use the unnumbered template sigc::slot instead.
 * @ingroup compat
 */
template <class T_return, class T_arg1,class T_arg2,class T_arg3,class T_arg4,class T_arg5,class T_arg6>
class Slot6
  : public ::sigc::slot<T_return, T_arg1,T_arg2,T_arg3,T_arg4,T_arg5,T_arg6>
{
public:
  typedef ::sigc::slot<T_return, T_arg1,T_arg2,T_arg3,T_arg4,T_arg5,T_arg6> parent_type;

  /// Constructs an empty slot.
  Slot6() {}

  /** Constructs a slot from an arbitrary functor.
   * @param _A_func The desired functor the new slot should be assigned to.
   */
  template <class T_functor>
  Slot6(const T_functor& _A_func)
    : ::sigc::slot<T_return, T_arg1,T_arg2,T_arg3,T_arg4,T_arg5,T_arg6>(_A_func) {}

  /** Constructs a slot, copying an existing one.
   * @param src The existing slot to copy.
   */
  Slot6(const parent_type& src)
    : parent_type(src) {}

  /** Overrides this slot making a copy from another slot.
   * @param src The slot from which to make a copy.
   * @return @p this.
   */
  Slot6& operator=(const parent_type& src)
    { parent_type::operator=(src); return *this; }
};

/** Converts an arbitrary functor to a unified type which is opaque.
 * Slot7 itself is a functor or to be more precise a closure. It contains
 * a single, arbitrary functor (or closure) that is executed in operator()().
 *
 * The template arguments determine the function signature of operator()():
 * - @e T_return The return type of operator()().
 * - @e T_arg1 Argument type used in the definition of operator()().
 * - @e T_arg2 Argument type used in the definition of operator()().
 * - @e T_arg3 Argument type used in the definition of operator()().
 * - @e T_arg4 Argument type used in the definition of operator()().
 * - @e T_arg5 Argument type used in the definition of operator()().
 * - @e T_arg6 Argument type used in the definition of operator()().
 * - @e T_arg7 Argument type used in the definition of operator()().
 *
 * To use simply assign the slot to the desired functor. If the functor
 * is not compatible with the parameter list defined with the template
 * arguments compiler errors are triggered. When called the slot
 * will invoke the functor with minimal copies.
 * block() and unblock() can be used to block the functor's invocation
 * from operator()() temporarily.
 *
 * @par Example:
 *   @code
 *   #include <sigc++/slot.h>
 *   void foo(int) {}
 *   SigC::Slot1<void, long> s = SigC::slot(&foo);
 *   s(19);
 *   @endcode
 *
 * @deprecated Use the unnumbered template sigc::slot instead.
 * @ingroup compat
 */
template <class T_return, class T_arg1,class T_arg2,class T_arg3,class T_arg4,class T_arg5,class T_arg6,class T_arg7>
class Slot7
  : public ::sigc::slot<T_return, T_arg1,T_arg2,T_arg3,T_arg4,T_arg5,T_arg6,T_arg7>
{
public:
  typedef ::sigc::slot<T_return, T_arg1,T_arg2,T_arg3,T_arg4,T_arg5,T_arg6,T_arg7> parent_type;

  /// Constructs an empty slot.
  Slot7() {}

  /** Constructs a slot from an arbitrary functor.
   * @param _A_func The desired functor the new slot should be assigned to.
   */
  template <class T_functor>
  Slot7(const T_functor& _A_func)
    : ::sigc::slot<T_return, T_arg1,T_arg2,T_arg3,T_arg4,T_arg5,T_arg6,T_arg7>(_A_func) {}

  /** Constructs a slot, copying an existing one.
   * @param src The existing slot to copy.
   */
  Slot7(const parent_type& src)
    : parent_type(src) {}

  /** Overrides this slot making a copy from another slot.
   * @param src The slot from which to make a copy.
   * @return @p this.
   */
  Slot7& operator=(const parent_type& src)
    { parent_type::operator=(src); return *this; }
};



#ifndef DOXYGEN_SHOULD_SKIP_THIS
/* gcc 3.2 reports a strange conflict between SigC::slot() and sigc::slot<>
 * when "using namespace SigC" and later using a slot(obj,func) overload
 * without the prefix "SigC::". Probably a compiler bug. I will investigate.
 *
 * This ugly hack avoids the error:
 */
// #define slot(...) make_slot(__VA_ARGS__) /* only works for gcc */
#endif


// slot()
/** Creates a functor of type SigC::Slot0 that wraps an existing non-member function.
 *
 * @param _A_func Pointer to function that should be wrapped.
 * @return Functor that executes _A_func on invokation.
 *
 * @deprecated Use sigc::ptr_fun() instead.
 * @ingroup compat
 */
template <class T_return>
inline Slot0<T_return>
slot(T_return (*_A_func)())
{ return Slot0<T_return>(_A_func); }

/** Creates a functor of type SigC::Slot1 that wraps an existing non-member function.
 *
 * @param _A_func Pointer to function that should be wrapped.
 * @return Functor that executes _A_func on invokation.
 *
 * @deprecated Use sigc::ptr_fun() instead.
 * @ingroup compat
 */
template <class T_return, class T_arg1>
inline Slot1<T_return, T_arg1>
slot(T_return (*_A_func)(T_arg1))
{ return Slot1<T_return, T_arg1>(_A_func); }

/** Creates a functor of type SigC::Slot2 that wraps an existing non-member function.
 *
 * @param _A_func Pointer to function that should be wrapped.
 * @return Functor that executes _A_func on invokation.
 *
 * @deprecated Use sigc::ptr_fun() instead.
 * @ingroup compat
 */
template <class T_return, class T_arg1,class T_arg2>
inline Slot2<T_return, T_arg1,T_arg2>
slot(T_return (*_A_func)(T_arg1,T_arg2))
{ return Slot2<T_return, T_arg1,T_arg2>(_A_func); }

/** Creates a functor of type SigC::Slot3 that wraps an existing non-member function.
 *
 * @param _A_func Pointer to function that should be wrapped.
 * @return Functor that executes _A_func on invokation.
 *
 * @deprecated Use sigc::ptr_fun() instead.
 * @ingroup compat
 */
template <class T_return, class T_arg1,class T_arg2,class T_arg3>
inline Slot3<T_return, T_arg1,T_arg2,T_arg3>
slot(T_return (*_A_func)(T_arg1,T_arg2,T_arg3))
{ return Slot3<T_return, T_arg1,T_arg2,T_arg3>(_A_func); }

/** Creates a functor of type SigC::Slot4 that wraps an existing non-member function.
 *
 * @param _A_func Pointer to function that should be wrapped.
 * @return Functor that executes _A_func on invokation.
 *
 * @deprecated Use sigc::ptr_fun() instead.
 * @ingroup compat
 */
template <class T_return, class T_arg1,class T_arg2,class T_arg3,class T_arg4>
inline Slot4<T_return, T_arg1,T_arg2,T_arg3,T_arg4>
slot(T_return (*_A_func)(T_arg1,T_arg2,T_arg3,T_arg4))
{ return Slot4<T_return, T_arg1,T_arg2,T_arg3,T_arg4>(_A_func); }

/** Creates a functor of type SigC::Slot5 that wraps an existing non-member function.
 *
 * @param _A_func Pointer to function that should be wrapped.
 * @return Functor that executes _A_func on invokation.
 *
 * @deprecated Use sigc::ptr_fun() instead.
 * @ingroup compat
 */
template <class T_return, class T_arg1,class T_arg2,class T_arg3,class T_arg4,class T_arg5>
inline Slot5<T_return, T_arg1,T_arg2,T_arg3,T_arg4,T_arg5>
slot(T_return (*_A_func)(T_arg1,T_arg2,T_arg3,T_arg4,T_arg5))
{ return Slot5<T_return, T_arg1,T_arg2,T_arg3,T_arg4,T_arg5>(_A_func); }

/** Creates a functor of type SigC::Slot6 that wraps an existing non-member function.
 *
 * @param _A_func Pointer to function that should be wrapped.
 * @return Functor that executes _A_func on invokation.
 *
 * @deprecated Use sigc::ptr_fun() instead.
 * @ingroup compat
 */
template <class T_return, class T_arg1,class T_arg2,class T_arg3,class T_arg4,class T_arg5,class T_arg6>
inline Slot6<T_return, T_arg1,T_arg2,T_arg3,T_arg4,T_arg5,T_arg6>
slot(T_return (*_A_func)(T_arg1,T_arg2,T_arg3,T_arg4,T_arg5,T_arg6))
{ return Slot6<T_return, T_arg1,T_arg2,T_arg3,T_arg4,T_arg5,T_arg6>(_A_func); }

/** Creates a functor of type SigC::Slot7 that wraps an existing non-member function.
 *
 * @param _A_func Pointer to function that should be wrapped.
 * @return Functor that executes _A_func on invokation.
 *
 * @deprecated Use sigc::ptr_fun() instead.
 * @ingroup compat
 */
template <class T_return, class T_arg1,class T_arg2,class T_arg3,class T_arg4,class T_arg5,class T_arg6,class T_arg7>
inline Slot7<T_return, T_arg1,T_arg2,T_arg3,T_arg4,T_arg5,T_arg6,T_arg7>
slot(T_return (*_A_func)(T_arg1,T_arg2,T_arg3,T_arg4,T_arg5,T_arg6,T_arg7))
{ return Slot7<T_return, T_arg1,T_arg2,T_arg3,T_arg4,T_arg5,T_arg6,T_arg7>(_A_func); }



}

#endif
#endif /* _SIGC_MACROS_SLOTHM4_ */
