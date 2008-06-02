dnl Copyright 2002, The libsigc++ Development Team 
dnl 
dnl This library is free software; you can redistribute it and/or 
dnl modify it under the terms of the GNU Lesser General Public 
dnl License as published by the Free Software Foundation; either 
dnl version 2.1 of the License, or (at your option) any later version. 
dnl 
dnl This library is distributed in the hope that it will be useful, 
dnl but WITHOUT ANY WARRANTY; without even the implied warranty of 
dnl MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU 
dnl Lesser General Public License for more details. 
dnl 
dnl You should have received a copy of the GNU Lesser General Public 
dnl License along with this library; if not, write to the Free Software 
dnl Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA 
dnl
divert(-1)

include(template.macros.m4)

define([POINTER_FUNCTOR],[dnl
/** pointer_functor$1 wraps existing non-member functions with $1 argument(s).
 * Use the convenience function ptr_fun() to create an instance of pointer_functor$1.
 *
 * The following template arguments are used:dnl
FOR(1,$1,[
 * - @e T_arg%1 Argument type used in the definition of operator()().])
 * - @e T_return The return type of operator()().
 *
 * @ingroup ptr_fun
 */
template <LIST(LOOP(class T_arg%1, $1), class T_return)>
class pointer_functor$1 : public functor_base
{
  typedef T_return (*function_type)(LOOP(T_arg%1, $1));
protected: 
  function_type func_ptr_;
public:
  typedef T_return result_type;

  /// Constructs an invalid functor.
  pointer_functor$1() {}

  /** Constructs a pointer_functor$1 object that wraps an existing function.
   * @param _A_func Pointer to function that will be invoked from operator()().
   */
  explicit pointer_functor$1(function_type _A_func): func_ptr_(_A_func) {}

  /** Execute the wrapped function.dnl
FOR(1, $1,[
   * @param _A_a%1 Argument to be passed on to the function.])
   * @return The return value of the function invocation.
   */
  T_return operator()(LOOP(typename type_trait<T_arg%1>::take _A_a%1, $1)) const 
    { return func_ptr_(LOOP(_A_a%1, $1)); }
};

])

define([PTR_FUN],[dnl
/** Creates a functor of type sigc::pointer_functor$1 which wraps an existing non-member function.
 * @param _A_func Pointer to function that should be wrapped.
 * @return Functor that executes @e _A_func on invokation.
 *
 * @ingroup ptr_fun
 */
template <LIST(LOOP(class T_arg%1, $1), class T_return)>
inline pointer_functor$1<LIST(LOOP(T_arg%1, $1), T_return)> 
ptr_fun[]ifelse($2,, $1)(T_return (*_A_func)(LOOP(T_arg%1,$1)))
{ return pointer_functor$1<LIST(LOOP(T_arg%1, $1), T_return)>(_A_func); }

])

divert(0)
__FIREWALL__
#include <sigc++/type_traits.h>
#include <sigc++/functors/functor_trait.h>

namespace sigc {

/** @defgroup ptr_fun ptr_fun()
 * ptr_fun() is used to convert a pointer to a function to a functor.
 * If the function pointer is to an overloaded type, you must specify
 * the types using template arguments starting with the first argument.
 * It is not necessary to supply the return type.
 *
 * @par Example:
 *   @code
 *   void foo(int) {}
 *   sigc::slot<void, int> sl = sigc::ptr_fun(&foo);
 *   @endcode
 *
 * Use ptr_fun#() if there is an abiguity as to the number of arguments.
 *
 * @par Example:
 *   @code
 *   void foo(int) {}  // choose this one
 *   void foo(float) {}
 *   void foo(int, int) {}
 *   sigc::slot<void, long> sl = sigc::ptr_fun1<int>(&foo);
 *   @endcode
 *
 * ptr_fun() can also be used to convert a pointer to a static member
 * function to a functor, like so:
 *
 * @par Example:
 *   @code
 *   struct foo
 *   {
 *     static void bar(int) {}
 *   };
 *   sigc::slot<void, int> sl = sigc::ptr_fun(&foo::bar);
 *   @endcode
 *
 * @ingroup functors
 */

FOR(0,CALL_SIZE,[[POINTER_FUNCTOR(%1)]])dnl

// numbered ptr_fun
FOR(0,CALL_SIZE,[[PTR_FUN(%1)]])dnl

// unnumbered ptr_fun
FOR(0,CALL_SIZE,[[PTR_FUN(%1,1)]])dnl

} /* namespace sigc */
