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

define([MEMBER_FUNCTOR],[dnl
/** [$2]mem_functor$1 wraps $4 methods with $1 argument(s).
 * Use the convenience function mem_fun() to create an instance of [$2]mem_functor$1.
 *
 * The following template arguments are used:dnl
FOR(1,$1,[
 * - @e T_arg%1 Argument type used in the definition of operator()().])
 * - @e T_return The return type of operator()().
 * - @e T_obj The object type.
 *
 * @ingroup mem_fun
 */
template <LIST(class T_return, class T_obj, LOOP(class T_arg%1, $1))>
class [$2]mem_functor$1 : public functor_base
{
public:
  typedef T_return (T_obj::*function_type)(LOOP(T_arg%1, $1)) $4;
  typedef T_return result_type;

  /// Constructs an invalid functor.
  [$2]mem_functor$1() : func_ptr_(0) {}

  /** Constructs a [$2]mem_functor$1 object that wraps the passed method.
   * @param _A_func Pointer to method will be invoked from operator()().
   */
  explicit [$2]mem_functor$1(function_type _A_func) : func_ptr_(_A_func) {}

  /** Execute the wrapped method operating on the passed instance.
   * @param _A_obj Pointer to instance the method should operate on.dnl
FOR(1, $1,[
   * @param _A_a%1 Argument to be passed on to the method.])
   * @return The return value of the method invocation.
   */
  T_return operator()(LIST($3 T_obj* _A_obj, LOOP(typename type_trait<T_arg%1>::take _A_a%1, $1))) const
    { return (_A_obj->*(this->func_ptr_))(LOOP(_A_a%1, $1)); }

  /** Execute the wrapped method operating on the passed instance.
   * @param _A_obj Reference to instance the method should operate on.dnl
FOR(1, $1,[
   * @param _A_a%1 Argument to be passed on to the method.])
   * @return The return value of the method invocation.
   */
  T_return operator()(LIST($3 T_obj& _A_obj, LOOP(typename type_trait<T_arg%1>::take _A_a%1, $1))) const
    { return (_A_obj.*func_ptr_)(LOOP(_A_a%1, $1)); }

protected:
  function_type func_ptr_;
};

])
define([BOUND_MEMBER_FUNCTOR],[dnl

/** bound_[$2]mem_functor$1 encapsulates a $4 method with $1 arguments and an object instance.
 * Use the convenience function mem_fun() to create an instance of bound_[$2]mem_functor$1.
 *
 * The following template arguments are used:dnl
FOR(1,$1,[
 * - @e T_arg%1 Argument type used in the definition of operator()().])
 * - @e T_return The return type of operator()().
 * - @e T_obj The object type.
 *
 * @ingroup mem_fun
 */
template <LIST(class T_return, class T_obj, LOOP(class T_arg%1, $1))>
class bound_[$2]mem_functor$1
  : public [$2]mem_functor$1<LIST(T_return, T_obj, LOOP(T_arg%1, $1))>
{
  typedef [$2]mem_functor$1<LIST(T_return, T_obj, LOOP(T_arg%1, $1))> base_type_;
public:
  typedef typename base_type_::function_type function_type;

  /** Constructs a bound_[$2]mem_functor$1 object that wraps the passed method.
   * @param _A_obj Pointer to instance the method will operate on.
   * @param _A_func Pointer to method will be invoked from operator()().
   */
  bound_[$2]mem_functor$1($3 T_obj* _A_obj, function_type _A_func)
    : base_type_(_A_func),
      obj_(*_A_obj)
    {}

  /** Constructs a bound_[$2]mem_functor$1 object that wraps the passed method.
   * @param _A_obj Reference to instance the method will operate on.
   * @param _A_func Pointer to method will be invoked from operator()().
   */
  bound_[$2]mem_functor$1($3 T_obj& _A_obj, function_type _A_func)
    : base_type_(_A_func),
      obj_(_A_obj)
    {}

  /** Execute the wrapped method operating on the stored instance.dnl
FOR(1, $1,[
   * @param _A_a%1 Argument to be passed on to the method.])
   * @return The return value of the method invocation.
   */
  T_return operator()(LOOP(typename type_trait<T_arg%1>::take _A_a%1, $1)) const
    { return (obj_.invoke().*(this->func_ptr_))(LOOP(_A_a%1, $1)); }

//protected:
  // Reference to stored object instance.
  // This is the handler object, such as TheObject in void TheObject::signal_handler().
  [$2]limit_reference<T_obj> obj_;
};

//template specialization of visit_each<>(action, functor):
/** Performs a functor on each of the targets of a functor.
 * The function overload for sigc::bound_[$2]mem_functor performs a functor
 * on the object instance stored in the sigc::bound_[$2]mem_functor object.
 *
 * @ingroup mem_fun
 */
template <LIST(class T_action, class T_return, class T_obj, LOOP(class T_arg%1, $1))>
void visit_each(const T_action& _A_action,
                const bound_[$2]mem_functor$1<LIST(T_return, T_obj, LOOP(T_arg%1, $1))>& _A_target)
{
  sigc::visit_each(_A_action, _A_target.obj_);
}

])

define([MEM_FUN],[dnl
/** Creates a functor of type sigc::[$3]mem_functor$1 which wraps a $5 method.
 * @param _A_func Pointer to method that should be wrapped.
 * @return Functor that executes _A_func on invokation.
 *
 * @ingroup mem_fun
 */
template <LIST(LOOP(class T_arg%1, $1), class T_return, class T_obj)>
inline [$3]mem_functor$1<LIST(T_return, T_obj, LOOP(T_arg%1, $1))>
mem_fun[]ifelse($2,, $1)(T_return (T_obj::*_A_func)(LOOP(T_arg%1,$1)) $5)
{ return [$3]mem_functor$1<LIST(T_return, T_obj, LOOP(T_arg%1, $1))>(_A_func); }

])
define([BOUND_MEM_FUN],[dnl
/** Creates a functor of type sigc::bound_[$3]mem_functor$1 which encapsulates a method and an object instance.
 * @param _A_obj Pointer to object instance the functor should operate on.
 * @param _A_func Pointer to method that should be wrapped.
 * @return Functor that executes @e _A_func on invokation.
 *
 * @ingroup mem_fun
 */
template <LIST(LOOP(class T_arg%1, $1), class T_return, class T_obj, class T_obj2)>
inline bound_[$3]mem_functor$1<LIST(T_return, T_obj, LOOP(T_arg%1, $1))>
mem_fun[]ifelse($2,, $1)(/*$4*/ T_obj* _A_obj, T_return (T_obj2::*_A_func)(LOOP(T_arg%1,$1)) $5)
{ return bound_[$3]mem_functor$1<LIST(T_return, T_obj, LOOP(T_arg%1, $1))>(_A_obj, _A_func); }

/** Creates a functor of type sigc::bound_[$3]mem_functor$1 which encapsulates a method and an object instance.
 * @param _A_obj Reference to object instance the functor should operate on.
 * @param _A_func Pointer to method that should be wrapped.
 * @return Functor that executes @e _A_func on invokation.
 *
 * @ingroup mem_fun
 */
template <LIST(LOOP(class T_arg%1, $1), class T_return, class T_obj, class T_obj2)>
inline bound_[$3]mem_functor$1<LIST(T_return, T_obj, LOOP(T_arg%1, $1))>
mem_fun[]ifelse($2,, $1)(/*$4*/ T_obj& _A_obj, T_return (T_obj2::*_A_func)(LOOP(T_arg%1,$1)) $5)
{ return bound_[$3]mem_functor$1<LIST(T_return, T_obj, LOOP(T_arg%1, $1))>(_A_obj, _A_func); }

])

divert(0)

// implementation notes:  
//  - we do not use bind here, because it would introduce
//    an extra copy and complicate the header include order if bind is
//    to have automatic conversion for member pointers.
__FIREWALL__
#include <sigc++/type_traits.h>
#include <sigc++/functors/functor_trait.h>
#include <sigc++/limit_reference.h>

namespace sigc {

/** @defgroup mem_fun mem_fun()
 * mem_fun() is used to convert a pointer to a method to a functor.
 *
 * Optionally a reference or pointer to an object can be bound to the functor.
 * Note that only if the object type inherits from sigc::trackable
 * the slot is cleared automatically when the object goes out of scope!
 *
 * If the member function pointer is to an overloaded type, you must specify
 * the types using template arguments starting with the first argument.
 * It is not necessary to supply the return type.
 *
 * @par Example:
 *   @code
 *   struct foo : public sigc::trackable
 *   {
 *     void bar(int) {}
 *   };
 *   foo my_foo;
 *   sigc::slot<void, int> sl = sigc::mem_fun(my_foo, &foo::bar);
 *   @endcode
 *
 * For const methods mem_fun() takes a const reference or pointer to an object.
 *
 * @par Example:
 *   @code
 *   struct foo : public sigc::trackable
 *   {
 *     void bar(int) const {}
 *   };
 *   const foo my_foo;
 *   sigc::slot<void, int> sl = sigc::mem_fun(my_foo, &foo::bar);
 *   @endcode
 *
 * Use mem_fun#() if there is an abiguity as to the number of arguments.
 *
 * @par Example:
 *   @code
 *   struct foo : public sigc::trackable
 *   {
 *     void bar(int) {}
 *     void bar(float) {}
 *     void bar(int, int) {}
 *   };
 *   foo my_foo;
 *   sigc::slot<void, int> sl = sigc::mem_fun1<int>(my_foo, &foo::bar);
 *   @endcode
 *
 * @ingroup functors
 */

FOR(0,CALL_SIZE,[[MEMBER_FUNCTOR(%1,[],[],[])]])dnl
FOR(0,CALL_SIZE,[[MEMBER_FUNCTOR(%1,[const_],[const],[const])]])dnl
FOR(0,CALL_SIZE,[[MEMBER_FUNCTOR(%1,[volatile_],[],[volatile])]])dnl
FOR(0,CALL_SIZE,[[MEMBER_FUNCTOR(%1,[const_volatile_],[const],[const volatile])]])dnl
FOR(0,CALL_SIZE,[[BOUND_MEMBER_FUNCTOR(%1,[],[],[])]])dnl
FOR(0,CALL_SIZE,[[BOUND_MEMBER_FUNCTOR(%1,[const_],[const],[const])]])dnl
FOR(0,CALL_SIZE,[[BOUND_MEMBER_FUNCTOR(%1,[volatile_],[],[volatile])]])dnl
FOR(0,CALL_SIZE,[[BOUND_MEMBER_FUNCTOR(%1,[const_volatile_],[const],[const volatile])]])dnl

// numbered
FOR(0,CALL_SIZE,[[MEM_FUN(%1,,[],[],[])]])dnl
FOR(0,CALL_SIZE,[[MEM_FUN(%1,,[const_],[const],[const])]])dnl
FOR(0,CALL_SIZE,[[MEM_FUN(%1,,[volatile_],[],[volatile])]])dnl
FOR(0,CALL_SIZE,[[MEM_FUN(%1,,[const_volatile_],[const],[const volatile])]])dnl
FOR(0,CALL_SIZE,[[BOUND_MEM_FUN(%1,,[],[],[])]])dnl
FOR(0,CALL_SIZE,[[BOUND_MEM_FUN(%1,,[const_],[const],[const])]])dnl
FOR(0,CALL_SIZE,[[BOUND_MEM_FUN(%1,,[volatile_],[],[volatile])]])dnl
FOR(0,CALL_SIZE,[[BOUND_MEM_FUN(%1,,[const_volatile_],[const],[const volatile])]])dnl

// unnumbered
FOR(0,CALL_SIZE,[[MEM_FUN(%1,1,[],[],[])]])dnl
FOR(0,CALL_SIZE,[[MEM_FUN(%1,1,[const_],[const],[const])]])dnl
FOR(0,CALL_SIZE,[[MEM_FUN(%1,1,[volatile_],[],[volatile])]])dnl
FOR(0,CALL_SIZE,[[MEM_FUN(%1,1,[const_volatile_],[const],[const volatile])]])dnl
FOR(0,CALL_SIZE,[[BOUND_MEM_FUN(%1,1,[],[],[])]])dnl
FOR(0,CALL_SIZE,[[BOUND_MEM_FUN(%1,1,[const_],[const],[const])]])dnl
FOR(0,CALL_SIZE,[[BOUND_MEM_FUN(%1,1,[volatile_],[],[volatile])]])dnl
FOR(0,CALL_SIZE,[[BOUND_MEM_FUN(%1,1,[const_volatile_],[const],[const volatile])]])dnl

} /* namespace sigc */
