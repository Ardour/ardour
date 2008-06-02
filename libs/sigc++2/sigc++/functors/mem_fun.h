// -*- c++ -*-
/* Do not edit! -- generated file */


// implementation notes:  
//  - we do not use bind here, because it would introduce
//    an extra copy and complicate the header include order if bind is
//    to have automatic conversion for member pointers.
#ifndef _SIGC_FUNCTORS_MACROS_MEM_FUNHM4_
#define _SIGC_FUNCTORS_MACROS_MEM_FUNHM4_
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

/** mem_functor0 wraps  methods with 0 argument(s).
 * Use the convenience function mem_fun() to create an instance of mem_functor0.
 *
 * The following template arguments are used:
 * - @e T_return The return type of operator()().
 * - @e T_obj The object type.
 *
 * @ingroup mem_fun
 */
template <class T_return, class T_obj>
class mem_functor0 : public functor_base
{
public:
  typedef T_return (T_obj::*function_type)() ;
  typedef T_return result_type;

  /// Constructs an invalid functor.
  mem_functor0() : func_ptr_(0) {}

  /** Constructs a mem_functor0 object that wraps the passed method.
   * @param _A_func Pointer to method will be invoked from operator()().
   */
  explicit mem_functor0(function_type _A_func) : func_ptr_(_A_func) {}

  /** Execute the wrapped method operating on the passed instance.
   * @param _A_obj Pointer to instance the method should operate on.
   * @return The return value of the method invocation.
   */
  T_return operator()(T_obj* _A_obj) const
    { return (_A_obj->*(this->func_ptr_))(); }

  /** Execute the wrapped method operating on the passed instance.
   * @param _A_obj Reference to instance the method should operate on.
   * @return The return value of the method invocation.
   */
  T_return operator()(T_obj& _A_obj) const
    { return (_A_obj.*func_ptr_)(); }

protected:
  function_type func_ptr_;
};

/** mem_functor1 wraps  methods with 1 argument(s).
 * Use the convenience function mem_fun() to create an instance of mem_functor1.
 *
 * The following template arguments are used:
 * - @e T_arg1 Argument type used in the definition of operator()().
 * - @e T_return The return type of operator()().
 * - @e T_obj The object type.
 *
 * @ingroup mem_fun
 */
template <class T_return, class T_obj, class T_arg1>
class mem_functor1 : public functor_base
{
public:
  typedef T_return (T_obj::*function_type)(T_arg1) ;
  typedef T_return result_type;

  /// Constructs an invalid functor.
  mem_functor1() : func_ptr_(0) {}

  /** Constructs a mem_functor1 object that wraps the passed method.
   * @param _A_func Pointer to method will be invoked from operator()().
   */
  explicit mem_functor1(function_type _A_func) : func_ptr_(_A_func) {}

  /** Execute the wrapped method operating on the passed instance.
   * @param _A_obj Pointer to instance the method should operate on.
   * @param _A_a1 Argument to be passed on to the method.
   * @return The return value of the method invocation.
   */
  T_return operator()(T_obj* _A_obj, typename type_trait<T_arg1>::take _A_a1) const
    { return (_A_obj->*(this->func_ptr_))(_A_a1); }

  /** Execute the wrapped method operating on the passed instance.
   * @param _A_obj Reference to instance the method should operate on.
   * @param _A_a1 Argument to be passed on to the method.
   * @return The return value of the method invocation.
   */
  T_return operator()(T_obj& _A_obj, typename type_trait<T_arg1>::take _A_a1) const
    { return (_A_obj.*func_ptr_)(_A_a1); }

protected:
  function_type func_ptr_;
};

/** mem_functor2 wraps  methods with 2 argument(s).
 * Use the convenience function mem_fun() to create an instance of mem_functor2.
 *
 * The following template arguments are used:
 * - @e T_arg1 Argument type used in the definition of operator()().
 * - @e T_arg2 Argument type used in the definition of operator()().
 * - @e T_return The return type of operator()().
 * - @e T_obj The object type.
 *
 * @ingroup mem_fun
 */
template <class T_return, class T_obj, class T_arg1,class T_arg2>
class mem_functor2 : public functor_base
{
public:
  typedef T_return (T_obj::*function_type)(T_arg1,T_arg2) ;
  typedef T_return result_type;

  /// Constructs an invalid functor.
  mem_functor2() : func_ptr_(0) {}

  /** Constructs a mem_functor2 object that wraps the passed method.
   * @param _A_func Pointer to method will be invoked from operator()().
   */
  explicit mem_functor2(function_type _A_func) : func_ptr_(_A_func) {}

  /** Execute the wrapped method operating on the passed instance.
   * @param _A_obj Pointer to instance the method should operate on.
   * @param _A_a1 Argument to be passed on to the method.
   * @param _A_a2 Argument to be passed on to the method.
   * @return The return value of the method invocation.
   */
  T_return operator()(T_obj* _A_obj, typename type_trait<T_arg1>::take _A_a1,typename type_trait<T_arg2>::take _A_a2) const
    { return (_A_obj->*(this->func_ptr_))(_A_a1,_A_a2); }

  /** Execute the wrapped method operating on the passed instance.
   * @param _A_obj Reference to instance the method should operate on.
   * @param _A_a1 Argument to be passed on to the method.
   * @param _A_a2 Argument to be passed on to the method.
   * @return The return value of the method invocation.
   */
  T_return operator()(T_obj& _A_obj, typename type_trait<T_arg1>::take _A_a1,typename type_trait<T_arg2>::take _A_a2) const
    { return (_A_obj.*func_ptr_)(_A_a1,_A_a2); }

protected:
  function_type func_ptr_;
};

/** mem_functor3 wraps  methods with 3 argument(s).
 * Use the convenience function mem_fun() to create an instance of mem_functor3.
 *
 * The following template arguments are used:
 * - @e T_arg1 Argument type used in the definition of operator()().
 * - @e T_arg2 Argument type used in the definition of operator()().
 * - @e T_arg3 Argument type used in the definition of operator()().
 * - @e T_return The return type of operator()().
 * - @e T_obj The object type.
 *
 * @ingroup mem_fun
 */
template <class T_return, class T_obj, class T_arg1,class T_arg2,class T_arg3>
class mem_functor3 : public functor_base
{
public:
  typedef T_return (T_obj::*function_type)(T_arg1,T_arg2,T_arg3) ;
  typedef T_return result_type;

  /// Constructs an invalid functor.
  mem_functor3() : func_ptr_(0) {}

  /** Constructs a mem_functor3 object that wraps the passed method.
   * @param _A_func Pointer to method will be invoked from operator()().
   */
  explicit mem_functor3(function_type _A_func) : func_ptr_(_A_func) {}

  /** Execute the wrapped method operating on the passed instance.
   * @param _A_obj Pointer to instance the method should operate on.
   * @param _A_a1 Argument to be passed on to the method.
   * @param _A_a2 Argument to be passed on to the method.
   * @param _A_a3 Argument to be passed on to the method.
   * @return The return value of the method invocation.
   */
  T_return operator()(T_obj* _A_obj, typename type_trait<T_arg1>::take _A_a1,typename type_trait<T_arg2>::take _A_a2,typename type_trait<T_arg3>::take _A_a3) const
    { return (_A_obj->*(this->func_ptr_))(_A_a1,_A_a2,_A_a3); }

  /** Execute the wrapped method operating on the passed instance.
   * @param _A_obj Reference to instance the method should operate on.
   * @param _A_a1 Argument to be passed on to the method.
   * @param _A_a2 Argument to be passed on to the method.
   * @param _A_a3 Argument to be passed on to the method.
   * @return The return value of the method invocation.
   */
  T_return operator()(T_obj& _A_obj, typename type_trait<T_arg1>::take _A_a1,typename type_trait<T_arg2>::take _A_a2,typename type_trait<T_arg3>::take _A_a3) const
    { return (_A_obj.*func_ptr_)(_A_a1,_A_a2,_A_a3); }

protected:
  function_type func_ptr_;
};

/** mem_functor4 wraps  methods with 4 argument(s).
 * Use the convenience function mem_fun() to create an instance of mem_functor4.
 *
 * The following template arguments are used:
 * - @e T_arg1 Argument type used in the definition of operator()().
 * - @e T_arg2 Argument type used in the definition of operator()().
 * - @e T_arg3 Argument type used in the definition of operator()().
 * - @e T_arg4 Argument type used in the definition of operator()().
 * - @e T_return The return type of operator()().
 * - @e T_obj The object type.
 *
 * @ingroup mem_fun
 */
template <class T_return, class T_obj, class T_arg1,class T_arg2,class T_arg3,class T_arg4>
class mem_functor4 : public functor_base
{
public:
  typedef T_return (T_obj::*function_type)(T_arg1,T_arg2,T_arg3,T_arg4) ;
  typedef T_return result_type;

  /// Constructs an invalid functor.
  mem_functor4() : func_ptr_(0) {}

  /** Constructs a mem_functor4 object that wraps the passed method.
   * @param _A_func Pointer to method will be invoked from operator()().
   */
  explicit mem_functor4(function_type _A_func) : func_ptr_(_A_func) {}

  /** Execute the wrapped method operating on the passed instance.
   * @param _A_obj Pointer to instance the method should operate on.
   * @param _A_a1 Argument to be passed on to the method.
   * @param _A_a2 Argument to be passed on to the method.
   * @param _A_a3 Argument to be passed on to the method.
   * @param _A_a4 Argument to be passed on to the method.
   * @return The return value of the method invocation.
   */
  T_return operator()(T_obj* _A_obj, typename type_trait<T_arg1>::take _A_a1,typename type_trait<T_arg2>::take _A_a2,typename type_trait<T_arg3>::take _A_a3,typename type_trait<T_arg4>::take _A_a4) const
    { return (_A_obj->*(this->func_ptr_))(_A_a1,_A_a2,_A_a3,_A_a4); }

  /** Execute the wrapped method operating on the passed instance.
   * @param _A_obj Reference to instance the method should operate on.
   * @param _A_a1 Argument to be passed on to the method.
   * @param _A_a2 Argument to be passed on to the method.
   * @param _A_a3 Argument to be passed on to the method.
   * @param _A_a4 Argument to be passed on to the method.
   * @return The return value of the method invocation.
   */
  T_return operator()(T_obj& _A_obj, typename type_trait<T_arg1>::take _A_a1,typename type_trait<T_arg2>::take _A_a2,typename type_trait<T_arg3>::take _A_a3,typename type_trait<T_arg4>::take _A_a4) const
    { return (_A_obj.*func_ptr_)(_A_a1,_A_a2,_A_a3,_A_a4); }

protected:
  function_type func_ptr_;
};

/** mem_functor5 wraps  methods with 5 argument(s).
 * Use the convenience function mem_fun() to create an instance of mem_functor5.
 *
 * The following template arguments are used:
 * - @e T_arg1 Argument type used in the definition of operator()().
 * - @e T_arg2 Argument type used in the definition of operator()().
 * - @e T_arg3 Argument type used in the definition of operator()().
 * - @e T_arg4 Argument type used in the definition of operator()().
 * - @e T_arg5 Argument type used in the definition of operator()().
 * - @e T_return The return type of operator()().
 * - @e T_obj The object type.
 *
 * @ingroup mem_fun
 */
template <class T_return, class T_obj, class T_arg1,class T_arg2,class T_arg3,class T_arg4,class T_arg5>
class mem_functor5 : public functor_base
{
public:
  typedef T_return (T_obj::*function_type)(T_arg1,T_arg2,T_arg3,T_arg4,T_arg5) ;
  typedef T_return result_type;

  /// Constructs an invalid functor.
  mem_functor5() : func_ptr_(0) {}

  /** Constructs a mem_functor5 object that wraps the passed method.
   * @param _A_func Pointer to method will be invoked from operator()().
   */
  explicit mem_functor5(function_type _A_func) : func_ptr_(_A_func) {}

  /** Execute the wrapped method operating on the passed instance.
   * @param _A_obj Pointer to instance the method should operate on.
   * @param _A_a1 Argument to be passed on to the method.
   * @param _A_a2 Argument to be passed on to the method.
   * @param _A_a3 Argument to be passed on to the method.
   * @param _A_a4 Argument to be passed on to the method.
   * @param _A_a5 Argument to be passed on to the method.
   * @return The return value of the method invocation.
   */
  T_return operator()(T_obj* _A_obj, typename type_trait<T_arg1>::take _A_a1,typename type_trait<T_arg2>::take _A_a2,typename type_trait<T_arg3>::take _A_a3,typename type_trait<T_arg4>::take _A_a4,typename type_trait<T_arg5>::take _A_a5) const
    { return (_A_obj->*(this->func_ptr_))(_A_a1,_A_a2,_A_a3,_A_a4,_A_a5); }

  /** Execute the wrapped method operating on the passed instance.
   * @param _A_obj Reference to instance the method should operate on.
   * @param _A_a1 Argument to be passed on to the method.
   * @param _A_a2 Argument to be passed on to the method.
   * @param _A_a3 Argument to be passed on to the method.
   * @param _A_a4 Argument to be passed on to the method.
   * @param _A_a5 Argument to be passed on to the method.
   * @return The return value of the method invocation.
   */
  T_return operator()(T_obj& _A_obj, typename type_trait<T_arg1>::take _A_a1,typename type_trait<T_arg2>::take _A_a2,typename type_trait<T_arg3>::take _A_a3,typename type_trait<T_arg4>::take _A_a4,typename type_trait<T_arg5>::take _A_a5) const
    { return (_A_obj.*func_ptr_)(_A_a1,_A_a2,_A_a3,_A_a4,_A_a5); }

protected:
  function_type func_ptr_;
};

/** mem_functor6 wraps  methods with 6 argument(s).
 * Use the convenience function mem_fun() to create an instance of mem_functor6.
 *
 * The following template arguments are used:
 * - @e T_arg1 Argument type used in the definition of operator()().
 * - @e T_arg2 Argument type used in the definition of operator()().
 * - @e T_arg3 Argument type used in the definition of operator()().
 * - @e T_arg4 Argument type used in the definition of operator()().
 * - @e T_arg5 Argument type used in the definition of operator()().
 * - @e T_arg6 Argument type used in the definition of operator()().
 * - @e T_return The return type of operator()().
 * - @e T_obj The object type.
 *
 * @ingroup mem_fun
 */
template <class T_return, class T_obj, class T_arg1,class T_arg2,class T_arg3,class T_arg4,class T_arg5,class T_arg6>
class mem_functor6 : public functor_base
{
public:
  typedef T_return (T_obj::*function_type)(T_arg1,T_arg2,T_arg3,T_arg4,T_arg5,T_arg6) ;
  typedef T_return result_type;

  /// Constructs an invalid functor.
  mem_functor6() : func_ptr_(0) {}

  /** Constructs a mem_functor6 object that wraps the passed method.
   * @param _A_func Pointer to method will be invoked from operator()().
   */
  explicit mem_functor6(function_type _A_func) : func_ptr_(_A_func) {}

  /** Execute the wrapped method operating on the passed instance.
   * @param _A_obj Pointer to instance the method should operate on.
   * @param _A_a1 Argument to be passed on to the method.
   * @param _A_a2 Argument to be passed on to the method.
   * @param _A_a3 Argument to be passed on to the method.
   * @param _A_a4 Argument to be passed on to the method.
   * @param _A_a5 Argument to be passed on to the method.
   * @param _A_a6 Argument to be passed on to the method.
   * @return The return value of the method invocation.
   */
  T_return operator()(T_obj* _A_obj, typename type_trait<T_arg1>::take _A_a1,typename type_trait<T_arg2>::take _A_a2,typename type_trait<T_arg3>::take _A_a3,typename type_trait<T_arg4>::take _A_a4,typename type_trait<T_arg5>::take _A_a5,typename type_trait<T_arg6>::take _A_a6) const
    { return (_A_obj->*(this->func_ptr_))(_A_a1,_A_a2,_A_a3,_A_a4,_A_a5,_A_a6); }

  /** Execute the wrapped method operating on the passed instance.
   * @param _A_obj Reference to instance the method should operate on.
   * @param _A_a1 Argument to be passed on to the method.
   * @param _A_a2 Argument to be passed on to the method.
   * @param _A_a3 Argument to be passed on to the method.
   * @param _A_a4 Argument to be passed on to the method.
   * @param _A_a5 Argument to be passed on to the method.
   * @param _A_a6 Argument to be passed on to the method.
   * @return The return value of the method invocation.
   */
  T_return operator()(T_obj& _A_obj, typename type_trait<T_arg1>::take _A_a1,typename type_trait<T_arg2>::take _A_a2,typename type_trait<T_arg3>::take _A_a3,typename type_trait<T_arg4>::take _A_a4,typename type_trait<T_arg5>::take _A_a5,typename type_trait<T_arg6>::take _A_a6) const
    { return (_A_obj.*func_ptr_)(_A_a1,_A_a2,_A_a3,_A_a4,_A_a5,_A_a6); }

protected:
  function_type func_ptr_;
};

/** mem_functor7 wraps  methods with 7 argument(s).
 * Use the convenience function mem_fun() to create an instance of mem_functor7.
 *
 * The following template arguments are used:
 * - @e T_arg1 Argument type used in the definition of operator()().
 * - @e T_arg2 Argument type used in the definition of operator()().
 * - @e T_arg3 Argument type used in the definition of operator()().
 * - @e T_arg4 Argument type used in the definition of operator()().
 * - @e T_arg5 Argument type used in the definition of operator()().
 * - @e T_arg6 Argument type used in the definition of operator()().
 * - @e T_arg7 Argument type used in the definition of operator()().
 * - @e T_return The return type of operator()().
 * - @e T_obj The object type.
 *
 * @ingroup mem_fun
 */
template <class T_return, class T_obj, class T_arg1,class T_arg2,class T_arg3,class T_arg4,class T_arg5,class T_arg6,class T_arg7>
class mem_functor7 : public functor_base
{
public:
  typedef T_return (T_obj::*function_type)(T_arg1,T_arg2,T_arg3,T_arg4,T_arg5,T_arg6,T_arg7) ;
  typedef T_return result_type;

  /// Constructs an invalid functor.
  mem_functor7() : func_ptr_(0) {}

  /** Constructs a mem_functor7 object that wraps the passed method.
   * @param _A_func Pointer to method will be invoked from operator()().
   */
  explicit mem_functor7(function_type _A_func) : func_ptr_(_A_func) {}

  /** Execute the wrapped method operating on the passed instance.
   * @param _A_obj Pointer to instance the method should operate on.
   * @param _A_a1 Argument to be passed on to the method.
   * @param _A_a2 Argument to be passed on to the method.
   * @param _A_a3 Argument to be passed on to the method.
   * @param _A_a4 Argument to be passed on to the method.
   * @param _A_a5 Argument to be passed on to the method.
   * @param _A_a6 Argument to be passed on to the method.
   * @param _A_a7 Argument to be passed on to the method.
   * @return The return value of the method invocation.
   */
  T_return operator()(T_obj* _A_obj, typename type_trait<T_arg1>::take _A_a1,typename type_trait<T_arg2>::take _A_a2,typename type_trait<T_arg3>::take _A_a3,typename type_trait<T_arg4>::take _A_a4,typename type_trait<T_arg5>::take _A_a5,typename type_trait<T_arg6>::take _A_a6,typename type_trait<T_arg7>::take _A_a7) const
    { return (_A_obj->*(this->func_ptr_))(_A_a1,_A_a2,_A_a3,_A_a4,_A_a5,_A_a6,_A_a7); }

  /** Execute the wrapped method operating on the passed instance.
   * @param _A_obj Reference to instance the method should operate on.
   * @param _A_a1 Argument to be passed on to the method.
   * @param _A_a2 Argument to be passed on to the method.
   * @param _A_a3 Argument to be passed on to the method.
   * @param _A_a4 Argument to be passed on to the method.
   * @param _A_a5 Argument to be passed on to the method.
   * @param _A_a6 Argument to be passed on to the method.
   * @param _A_a7 Argument to be passed on to the method.
   * @return The return value of the method invocation.
   */
  T_return operator()(T_obj& _A_obj, typename type_trait<T_arg1>::take _A_a1,typename type_trait<T_arg2>::take _A_a2,typename type_trait<T_arg3>::take _A_a3,typename type_trait<T_arg4>::take _A_a4,typename type_trait<T_arg5>::take _A_a5,typename type_trait<T_arg6>::take _A_a6,typename type_trait<T_arg7>::take _A_a7) const
    { return (_A_obj.*func_ptr_)(_A_a1,_A_a2,_A_a3,_A_a4,_A_a5,_A_a6,_A_a7); }

protected:
  function_type func_ptr_;
};

/** const_mem_functor0 wraps const methods with 0 argument(s).
 * Use the convenience function mem_fun() to create an instance of const_mem_functor0.
 *
 * The following template arguments are used:
 * - @e T_return The return type of operator()().
 * - @e T_obj The object type.
 *
 * @ingroup mem_fun
 */
template <class T_return, class T_obj>
class const_mem_functor0 : public functor_base
{
public:
  typedef T_return (T_obj::*function_type)() const;
  typedef T_return result_type;

  /// Constructs an invalid functor.
  const_mem_functor0() : func_ptr_(0) {}

  /** Constructs a const_mem_functor0 object that wraps the passed method.
   * @param _A_func Pointer to method will be invoked from operator()().
   */
  explicit const_mem_functor0(function_type _A_func) : func_ptr_(_A_func) {}

  /** Execute the wrapped method operating on the passed instance.
   * @param _A_obj Pointer to instance the method should operate on.
   * @return The return value of the method invocation.
   */
  T_return operator()(const T_obj* _A_obj) const
    { return (_A_obj->*(this->func_ptr_))(); }

  /** Execute the wrapped method operating on the passed instance.
   * @param _A_obj Reference to instance the method should operate on.
   * @return The return value of the method invocation.
   */
  T_return operator()(const T_obj& _A_obj) const
    { return (_A_obj.*func_ptr_)(); }

protected:
  function_type func_ptr_;
};

/** const_mem_functor1 wraps const methods with 1 argument(s).
 * Use the convenience function mem_fun() to create an instance of const_mem_functor1.
 *
 * The following template arguments are used:
 * - @e T_arg1 Argument type used in the definition of operator()().
 * - @e T_return The return type of operator()().
 * - @e T_obj The object type.
 *
 * @ingroup mem_fun
 */
template <class T_return, class T_obj, class T_arg1>
class const_mem_functor1 : public functor_base
{
public:
  typedef T_return (T_obj::*function_type)(T_arg1) const;
  typedef T_return result_type;

  /// Constructs an invalid functor.
  const_mem_functor1() : func_ptr_(0) {}

  /** Constructs a const_mem_functor1 object that wraps the passed method.
   * @param _A_func Pointer to method will be invoked from operator()().
   */
  explicit const_mem_functor1(function_type _A_func) : func_ptr_(_A_func) {}

  /** Execute the wrapped method operating on the passed instance.
   * @param _A_obj Pointer to instance the method should operate on.
   * @param _A_a1 Argument to be passed on to the method.
   * @return The return value of the method invocation.
   */
  T_return operator()(const T_obj* _A_obj, typename type_trait<T_arg1>::take _A_a1) const
    { return (_A_obj->*(this->func_ptr_))(_A_a1); }

  /** Execute the wrapped method operating on the passed instance.
   * @param _A_obj Reference to instance the method should operate on.
   * @param _A_a1 Argument to be passed on to the method.
   * @return The return value of the method invocation.
   */
  T_return operator()(const T_obj& _A_obj, typename type_trait<T_arg1>::take _A_a1) const
    { return (_A_obj.*func_ptr_)(_A_a1); }

protected:
  function_type func_ptr_;
};

/** const_mem_functor2 wraps const methods with 2 argument(s).
 * Use the convenience function mem_fun() to create an instance of const_mem_functor2.
 *
 * The following template arguments are used:
 * - @e T_arg1 Argument type used in the definition of operator()().
 * - @e T_arg2 Argument type used in the definition of operator()().
 * - @e T_return The return type of operator()().
 * - @e T_obj The object type.
 *
 * @ingroup mem_fun
 */
template <class T_return, class T_obj, class T_arg1,class T_arg2>
class const_mem_functor2 : public functor_base
{
public:
  typedef T_return (T_obj::*function_type)(T_arg1,T_arg2) const;
  typedef T_return result_type;

  /// Constructs an invalid functor.
  const_mem_functor2() : func_ptr_(0) {}

  /** Constructs a const_mem_functor2 object that wraps the passed method.
   * @param _A_func Pointer to method will be invoked from operator()().
   */
  explicit const_mem_functor2(function_type _A_func) : func_ptr_(_A_func) {}

  /** Execute the wrapped method operating on the passed instance.
   * @param _A_obj Pointer to instance the method should operate on.
   * @param _A_a1 Argument to be passed on to the method.
   * @param _A_a2 Argument to be passed on to the method.
   * @return The return value of the method invocation.
   */
  T_return operator()(const T_obj* _A_obj, typename type_trait<T_arg1>::take _A_a1,typename type_trait<T_arg2>::take _A_a2) const
    { return (_A_obj->*(this->func_ptr_))(_A_a1,_A_a2); }

  /** Execute the wrapped method operating on the passed instance.
   * @param _A_obj Reference to instance the method should operate on.
   * @param _A_a1 Argument to be passed on to the method.
   * @param _A_a2 Argument to be passed on to the method.
   * @return The return value of the method invocation.
   */
  T_return operator()(const T_obj& _A_obj, typename type_trait<T_arg1>::take _A_a1,typename type_trait<T_arg2>::take _A_a2) const
    { return (_A_obj.*func_ptr_)(_A_a1,_A_a2); }

protected:
  function_type func_ptr_;
};

/** const_mem_functor3 wraps const methods with 3 argument(s).
 * Use the convenience function mem_fun() to create an instance of const_mem_functor3.
 *
 * The following template arguments are used:
 * - @e T_arg1 Argument type used in the definition of operator()().
 * - @e T_arg2 Argument type used in the definition of operator()().
 * - @e T_arg3 Argument type used in the definition of operator()().
 * - @e T_return The return type of operator()().
 * - @e T_obj The object type.
 *
 * @ingroup mem_fun
 */
template <class T_return, class T_obj, class T_arg1,class T_arg2,class T_arg3>
class const_mem_functor3 : public functor_base
{
public:
  typedef T_return (T_obj::*function_type)(T_arg1,T_arg2,T_arg3) const;
  typedef T_return result_type;

  /// Constructs an invalid functor.
  const_mem_functor3() : func_ptr_(0) {}

  /** Constructs a const_mem_functor3 object that wraps the passed method.
   * @param _A_func Pointer to method will be invoked from operator()().
   */
  explicit const_mem_functor3(function_type _A_func) : func_ptr_(_A_func) {}

  /** Execute the wrapped method operating on the passed instance.
   * @param _A_obj Pointer to instance the method should operate on.
   * @param _A_a1 Argument to be passed on to the method.
   * @param _A_a2 Argument to be passed on to the method.
   * @param _A_a3 Argument to be passed on to the method.
   * @return The return value of the method invocation.
   */
  T_return operator()(const T_obj* _A_obj, typename type_trait<T_arg1>::take _A_a1,typename type_trait<T_arg2>::take _A_a2,typename type_trait<T_arg3>::take _A_a3) const
    { return (_A_obj->*(this->func_ptr_))(_A_a1,_A_a2,_A_a3); }

  /** Execute the wrapped method operating on the passed instance.
   * @param _A_obj Reference to instance the method should operate on.
   * @param _A_a1 Argument to be passed on to the method.
   * @param _A_a2 Argument to be passed on to the method.
   * @param _A_a3 Argument to be passed on to the method.
   * @return The return value of the method invocation.
   */
  T_return operator()(const T_obj& _A_obj, typename type_trait<T_arg1>::take _A_a1,typename type_trait<T_arg2>::take _A_a2,typename type_trait<T_arg3>::take _A_a3) const
    { return (_A_obj.*func_ptr_)(_A_a1,_A_a2,_A_a3); }

protected:
  function_type func_ptr_;
};

/** const_mem_functor4 wraps const methods with 4 argument(s).
 * Use the convenience function mem_fun() to create an instance of const_mem_functor4.
 *
 * The following template arguments are used:
 * - @e T_arg1 Argument type used in the definition of operator()().
 * - @e T_arg2 Argument type used in the definition of operator()().
 * - @e T_arg3 Argument type used in the definition of operator()().
 * - @e T_arg4 Argument type used in the definition of operator()().
 * - @e T_return The return type of operator()().
 * - @e T_obj The object type.
 *
 * @ingroup mem_fun
 */
template <class T_return, class T_obj, class T_arg1,class T_arg2,class T_arg3,class T_arg4>
class const_mem_functor4 : public functor_base
{
public:
  typedef T_return (T_obj::*function_type)(T_arg1,T_arg2,T_arg3,T_arg4) const;
  typedef T_return result_type;

  /// Constructs an invalid functor.
  const_mem_functor4() : func_ptr_(0) {}

  /** Constructs a const_mem_functor4 object that wraps the passed method.
   * @param _A_func Pointer to method will be invoked from operator()().
   */
  explicit const_mem_functor4(function_type _A_func) : func_ptr_(_A_func) {}

  /** Execute the wrapped method operating on the passed instance.
   * @param _A_obj Pointer to instance the method should operate on.
   * @param _A_a1 Argument to be passed on to the method.
   * @param _A_a2 Argument to be passed on to the method.
   * @param _A_a3 Argument to be passed on to the method.
   * @param _A_a4 Argument to be passed on to the method.
   * @return The return value of the method invocation.
   */
  T_return operator()(const T_obj* _A_obj, typename type_trait<T_arg1>::take _A_a1,typename type_trait<T_arg2>::take _A_a2,typename type_trait<T_arg3>::take _A_a3,typename type_trait<T_arg4>::take _A_a4) const
    { return (_A_obj->*(this->func_ptr_))(_A_a1,_A_a2,_A_a3,_A_a4); }

  /** Execute the wrapped method operating on the passed instance.
   * @param _A_obj Reference to instance the method should operate on.
   * @param _A_a1 Argument to be passed on to the method.
   * @param _A_a2 Argument to be passed on to the method.
   * @param _A_a3 Argument to be passed on to the method.
   * @param _A_a4 Argument to be passed on to the method.
   * @return The return value of the method invocation.
   */
  T_return operator()(const T_obj& _A_obj, typename type_trait<T_arg1>::take _A_a1,typename type_trait<T_arg2>::take _A_a2,typename type_trait<T_arg3>::take _A_a3,typename type_trait<T_arg4>::take _A_a4) const
    { return (_A_obj.*func_ptr_)(_A_a1,_A_a2,_A_a3,_A_a4); }

protected:
  function_type func_ptr_;
};

/** const_mem_functor5 wraps const methods with 5 argument(s).
 * Use the convenience function mem_fun() to create an instance of const_mem_functor5.
 *
 * The following template arguments are used:
 * - @e T_arg1 Argument type used in the definition of operator()().
 * - @e T_arg2 Argument type used in the definition of operator()().
 * - @e T_arg3 Argument type used in the definition of operator()().
 * - @e T_arg4 Argument type used in the definition of operator()().
 * - @e T_arg5 Argument type used in the definition of operator()().
 * - @e T_return The return type of operator()().
 * - @e T_obj The object type.
 *
 * @ingroup mem_fun
 */
template <class T_return, class T_obj, class T_arg1,class T_arg2,class T_arg3,class T_arg4,class T_arg5>
class const_mem_functor5 : public functor_base
{
public:
  typedef T_return (T_obj::*function_type)(T_arg1,T_arg2,T_arg3,T_arg4,T_arg5) const;
  typedef T_return result_type;

  /// Constructs an invalid functor.
  const_mem_functor5() : func_ptr_(0) {}

  /** Constructs a const_mem_functor5 object that wraps the passed method.
   * @param _A_func Pointer to method will be invoked from operator()().
   */
  explicit const_mem_functor5(function_type _A_func) : func_ptr_(_A_func) {}

  /** Execute the wrapped method operating on the passed instance.
   * @param _A_obj Pointer to instance the method should operate on.
   * @param _A_a1 Argument to be passed on to the method.
   * @param _A_a2 Argument to be passed on to the method.
   * @param _A_a3 Argument to be passed on to the method.
   * @param _A_a4 Argument to be passed on to the method.
   * @param _A_a5 Argument to be passed on to the method.
   * @return The return value of the method invocation.
   */
  T_return operator()(const T_obj* _A_obj, typename type_trait<T_arg1>::take _A_a1,typename type_trait<T_arg2>::take _A_a2,typename type_trait<T_arg3>::take _A_a3,typename type_trait<T_arg4>::take _A_a4,typename type_trait<T_arg5>::take _A_a5) const
    { return (_A_obj->*(this->func_ptr_))(_A_a1,_A_a2,_A_a3,_A_a4,_A_a5); }

  /** Execute the wrapped method operating on the passed instance.
   * @param _A_obj Reference to instance the method should operate on.
   * @param _A_a1 Argument to be passed on to the method.
   * @param _A_a2 Argument to be passed on to the method.
   * @param _A_a3 Argument to be passed on to the method.
   * @param _A_a4 Argument to be passed on to the method.
   * @param _A_a5 Argument to be passed on to the method.
   * @return The return value of the method invocation.
   */
  T_return operator()(const T_obj& _A_obj, typename type_trait<T_arg1>::take _A_a1,typename type_trait<T_arg2>::take _A_a2,typename type_trait<T_arg3>::take _A_a3,typename type_trait<T_arg4>::take _A_a4,typename type_trait<T_arg5>::take _A_a5) const
    { return (_A_obj.*func_ptr_)(_A_a1,_A_a2,_A_a3,_A_a4,_A_a5); }

protected:
  function_type func_ptr_;
};

/** const_mem_functor6 wraps const methods with 6 argument(s).
 * Use the convenience function mem_fun() to create an instance of const_mem_functor6.
 *
 * The following template arguments are used:
 * - @e T_arg1 Argument type used in the definition of operator()().
 * - @e T_arg2 Argument type used in the definition of operator()().
 * - @e T_arg3 Argument type used in the definition of operator()().
 * - @e T_arg4 Argument type used in the definition of operator()().
 * - @e T_arg5 Argument type used in the definition of operator()().
 * - @e T_arg6 Argument type used in the definition of operator()().
 * - @e T_return The return type of operator()().
 * - @e T_obj The object type.
 *
 * @ingroup mem_fun
 */
template <class T_return, class T_obj, class T_arg1,class T_arg2,class T_arg3,class T_arg4,class T_arg5,class T_arg6>
class const_mem_functor6 : public functor_base
{
public:
  typedef T_return (T_obj::*function_type)(T_arg1,T_arg2,T_arg3,T_arg4,T_arg5,T_arg6) const;
  typedef T_return result_type;

  /// Constructs an invalid functor.
  const_mem_functor6() : func_ptr_(0) {}

  /** Constructs a const_mem_functor6 object that wraps the passed method.
   * @param _A_func Pointer to method will be invoked from operator()().
   */
  explicit const_mem_functor6(function_type _A_func) : func_ptr_(_A_func) {}

  /** Execute the wrapped method operating on the passed instance.
   * @param _A_obj Pointer to instance the method should operate on.
   * @param _A_a1 Argument to be passed on to the method.
   * @param _A_a2 Argument to be passed on to the method.
   * @param _A_a3 Argument to be passed on to the method.
   * @param _A_a4 Argument to be passed on to the method.
   * @param _A_a5 Argument to be passed on to the method.
   * @param _A_a6 Argument to be passed on to the method.
   * @return The return value of the method invocation.
   */
  T_return operator()(const T_obj* _A_obj, typename type_trait<T_arg1>::take _A_a1,typename type_trait<T_arg2>::take _A_a2,typename type_trait<T_arg3>::take _A_a3,typename type_trait<T_arg4>::take _A_a4,typename type_trait<T_arg5>::take _A_a5,typename type_trait<T_arg6>::take _A_a6) const
    { return (_A_obj->*(this->func_ptr_))(_A_a1,_A_a2,_A_a3,_A_a4,_A_a5,_A_a6); }

  /** Execute the wrapped method operating on the passed instance.
   * @param _A_obj Reference to instance the method should operate on.
   * @param _A_a1 Argument to be passed on to the method.
   * @param _A_a2 Argument to be passed on to the method.
   * @param _A_a3 Argument to be passed on to the method.
   * @param _A_a4 Argument to be passed on to the method.
   * @param _A_a5 Argument to be passed on to the method.
   * @param _A_a6 Argument to be passed on to the method.
   * @return The return value of the method invocation.
   */
  T_return operator()(const T_obj& _A_obj, typename type_trait<T_arg1>::take _A_a1,typename type_trait<T_arg2>::take _A_a2,typename type_trait<T_arg3>::take _A_a3,typename type_trait<T_arg4>::take _A_a4,typename type_trait<T_arg5>::take _A_a5,typename type_trait<T_arg6>::take _A_a6) const
    { return (_A_obj.*func_ptr_)(_A_a1,_A_a2,_A_a3,_A_a4,_A_a5,_A_a6); }

protected:
  function_type func_ptr_;
};

/** const_mem_functor7 wraps const methods with 7 argument(s).
 * Use the convenience function mem_fun() to create an instance of const_mem_functor7.
 *
 * The following template arguments are used:
 * - @e T_arg1 Argument type used in the definition of operator()().
 * - @e T_arg2 Argument type used in the definition of operator()().
 * - @e T_arg3 Argument type used in the definition of operator()().
 * - @e T_arg4 Argument type used in the definition of operator()().
 * - @e T_arg5 Argument type used in the definition of operator()().
 * - @e T_arg6 Argument type used in the definition of operator()().
 * - @e T_arg7 Argument type used in the definition of operator()().
 * - @e T_return The return type of operator()().
 * - @e T_obj The object type.
 *
 * @ingroup mem_fun
 */
template <class T_return, class T_obj, class T_arg1,class T_arg2,class T_arg3,class T_arg4,class T_arg5,class T_arg6,class T_arg7>
class const_mem_functor7 : public functor_base
{
public:
  typedef T_return (T_obj::*function_type)(T_arg1,T_arg2,T_arg3,T_arg4,T_arg5,T_arg6,T_arg7) const;
  typedef T_return result_type;

  /// Constructs an invalid functor.
  const_mem_functor7() : func_ptr_(0) {}

  /** Constructs a const_mem_functor7 object that wraps the passed method.
   * @param _A_func Pointer to method will be invoked from operator()().
   */
  explicit const_mem_functor7(function_type _A_func) : func_ptr_(_A_func) {}

  /** Execute the wrapped method operating on the passed instance.
   * @param _A_obj Pointer to instance the method should operate on.
   * @param _A_a1 Argument to be passed on to the method.
   * @param _A_a2 Argument to be passed on to the method.
   * @param _A_a3 Argument to be passed on to the method.
   * @param _A_a4 Argument to be passed on to the method.
   * @param _A_a5 Argument to be passed on to the method.
   * @param _A_a6 Argument to be passed on to the method.
   * @param _A_a7 Argument to be passed on to the method.
   * @return The return value of the method invocation.
   */
  T_return operator()(const T_obj* _A_obj, typename type_trait<T_arg1>::take _A_a1,typename type_trait<T_arg2>::take _A_a2,typename type_trait<T_arg3>::take _A_a3,typename type_trait<T_arg4>::take _A_a4,typename type_trait<T_arg5>::take _A_a5,typename type_trait<T_arg6>::take _A_a6,typename type_trait<T_arg7>::take _A_a7) const
    { return (_A_obj->*(this->func_ptr_))(_A_a1,_A_a2,_A_a3,_A_a4,_A_a5,_A_a6,_A_a7); }

  /** Execute the wrapped method operating on the passed instance.
   * @param _A_obj Reference to instance the method should operate on.
   * @param _A_a1 Argument to be passed on to the method.
   * @param _A_a2 Argument to be passed on to the method.
   * @param _A_a3 Argument to be passed on to the method.
   * @param _A_a4 Argument to be passed on to the method.
   * @param _A_a5 Argument to be passed on to the method.
   * @param _A_a6 Argument to be passed on to the method.
   * @param _A_a7 Argument to be passed on to the method.
   * @return The return value of the method invocation.
   */
  T_return operator()(const T_obj& _A_obj, typename type_trait<T_arg1>::take _A_a1,typename type_trait<T_arg2>::take _A_a2,typename type_trait<T_arg3>::take _A_a3,typename type_trait<T_arg4>::take _A_a4,typename type_trait<T_arg5>::take _A_a5,typename type_trait<T_arg6>::take _A_a6,typename type_trait<T_arg7>::take _A_a7) const
    { return (_A_obj.*func_ptr_)(_A_a1,_A_a2,_A_a3,_A_a4,_A_a5,_A_a6,_A_a7); }

protected:
  function_type func_ptr_;
};

/** volatile_mem_functor0 wraps volatile methods with 0 argument(s).
 * Use the convenience function mem_fun() to create an instance of volatile_mem_functor0.
 *
 * The following template arguments are used:
 * - @e T_return The return type of operator()().
 * - @e T_obj The object type.
 *
 * @ingroup mem_fun
 */
template <class T_return, class T_obj>
class volatile_mem_functor0 : public functor_base
{
public:
  typedef T_return (T_obj::*function_type)() volatile;
  typedef T_return result_type;

  /// Constructs an invalid functor.
  volatile_mem_functor0() : func_ptr_(0) {}

  /** Constructs a volatile_mem_functor0 object that wraps the passed method.
   * @param _A_func Pointer to method will be invoked from operator()().
   */
  explicit volatile_mem_functor0(function_type _A_func) : func_ptr_(_A_func) {}

  /** Execute the wrapped method operating on the passed instance.
   * @param _A_obj Pointer to instance the method should operate on.
   * @return The return value of the method invocation.
   */
  T_return operator()(T_obj* _A_obj) const
    { return (_A_obj->*(this->func_ptr_))(); }

  /** Execute the wrapped method operating on the passed instance.
   * @param _A_obj Reference to instance the method should operate on.
   * @return The return value of the method invocation.
   */
  T_return operator()(T_obj& _A_obj) const
    { return (_A_obj.*func_ptr_)(); }

protected:
  function_type func_ptr_;
};

/** volatile_mem_functor1 wraps volatile methods with 1 argument(s).
 * Use the convenience function mem_fun() to create an instance of volatile_mem_functor1.
 *
 * The following template arguments are used:
 * - @e T_arg1 Argument type used in the definition of operator()().
 * - @e T_return The return type of operator()().
 * - @e T_obj The object type.
 *
 * @ingroup mem_fun
 */
template <class T_return, class T_obj, class T_arg1>
class volatile_mem_functor1 : public functor_base
{
public:
  typedef T_return (T_obj::*function_type)(T_arg1) volatile;
  typedef T_return result_type;

  /// Constructs an invalid functor.
  volatile_mem_functor1() : func_ptr_(0) {}

  /** Constructs a volatile_mem_functor1 object that wraps the passed method.
   * @param _A_func Pointer to method will be invoked from operator()().
   */
  explicit volatile_mem_functor1(function_type _A_func) : func_ptr_(_A_func) {}

  /** Execute the wrapped method operating on the passed instance.
   * @param _A_obj Pointer to instance the method should operate on.
   * @param _A_a1 Argument to be passed on to the method.
   * @return The return value of the method invocation.
   */
  T_return operator()(T_obj* _A_obj, typename type_trait<T_arg1>::take _A_a1) const
    { return (_A_obj->*(this->func_ptr_))(_A_a1); }

  /** Execute the wrapped method operating on the passed instance.
   * @param _A_obj Reference to instance the method should operate on.
   * @param _A_a1 Argument to be passed on to the method.
   * @return The return value of the method invocation.
   */
  T_return operator()(T_obj& _A_obj, typename type_trait<T_arg1>::take _A_a1) const
    { return (_A_obj.*func_ptr_)(_A_a1); }

protected:
  function_type func_ptr_;
};

/** volatile_mem_functor2 wraps volatile methods with 2 argument(s).
 * Use the convenience function mem_fun() to create an instance of volatile_mem_functor2.
 *
 * The following template arguments are used:
 * - @e T_arg1 Argument type used in the definition of operator()().
 * - @e T_arg2 Argument type used in the definition of operator()().
 * - @e T_return The return type of operator()().
 * - @e T_obj The object type.
 *
 * @ingroup mem_fun
 */
template <class T_return, class T_obj, class T_arg1,class T_arg2>
class volatile_mem_functor2 : public functor_base
{
public:
  typedef T_return (T_obj::*function_type)(T_arg1,T_arg2) volatile;
  typedef T_return result_type;

  /// Constructs an invalid functor.
  volatile_mem_functor2() : func_ptr_(0) {}

  /** Constructs a volatile_mem_functor2 object that wraps the passed method.
   * @param _A_func Pointer to method will be invoked from operator()().
   */
  explicit volatile_mem_functor2(function_type _A_func) : func_ptr_(_A_func) {}

  /** Execute the wrapped method operating on the passed instance.
   * @param _A_obj Pointer to instance the method should operate on.
   * @param _A_a1 Argument to be passed on to the method.
   * @param _A_a2 Argument to be passed on to the method.
   * @return The return value of the method invocation.
   */
  T_return operator()(T_obj* _A_obj, typename type_trait<T_arg1>::take _A_a1,typename type_trait<T_arg2>::take _A_a2) const
    { return (_A_obj->*(this->func_ptr_))(_A_a1,_A_a2); }

  /** Execute the wrapped method operating on the passed instance.
   * @param _A_obj Reference to instance the method should operate on.
   * @param _A_a1 Argument to be passed on to the method.
   * @param _A_a2 Argument to be passed on to the method.
   * @return The return value of the method invocation.
   */
  T_return operator()(T_obj& _A_obj, typename type_trait<T_arg1>::take _A_a1,typename type_trait<T_arg2>::take _A_a2) const
    { return (_A_obj.*func_ptr_)(_A_a1,_A_a2); }

protected:
  function_type func_ptr_;
};

/** volatile_mem_functor3 wraps volatile methods with 3 argument(s).
 * Use the convenience function mem_fun() to create an instance of volatile_mem_functor3.
 *
 * The following template arguments are used:
 * - @e T_arg1 Argument type used in the definition of operator()().
 * - @e T_arg2 Argument type used in the definition of operator()().
 * - @e T_arg3 Argument type used in the definition of operator()().
 * - @e T_return The return type of operator()().
 * - @e T_obj The object type.
 *
 * @ingroup mem_fun
 */
template <class T_return, class T_obj, class T_arg1,class T_arg2,class T_arg3>
class volatile_mem_functor3 : public functor_base
{
public:
  typedef T_return (T_obj::*function_type)(T_arg1,T_arg2,T_arg3) volatile;
  typedef T_return result_type;

  /// Constructs an invalid functor.
  volatile_mem_functor3() : func_ptr_(0) {}

  /** Constructs a volatile_mem_functor3 object that wraps the passed method.
   * @param _A_func Pointer to method will be invoked from operator()().
   */
  explicit volatile_mem_functor3(function_type _A_func) : func_ptr_(_A_func) {}

  /** Execute the wrapped method operating on the passed instance.
   * @param _A_obj Pointer to instance the method should operate on.
   * @param _A_a1 Argument to be passed on to the method.
   * @param _A_a2 Argument to be passed on to the method.
   * @param _A_a3 Argument to be passed on to the method.
   * @return The return value of the method invocation.
   */
  T_return operator()(T_obj* _A_obj, typename type_trait<T_arg1>::take _A_a1,typename type_trait<T_arg2>::take _A_a2,typename type_trait<T_arg3>::take _A_a3) const
    { return (_A_obj->*(this->func_ptr_))(_A_a1,_A_a2,_A_a3); }

  /** Execute the wrapped method operating on the passed instance.
   * @param _A_obj Reference to instance the method should operate on.
   * @param _A_a1 Argument to be passed on to the method.
   * @param _A_a2 Argument to be passed on to the method.
   * @param _A_a3 Argument to be passed on to the method.
   * @return The return value of the method invocation.
   */
  T_return operator()(T_obj& _A_obj, typename type_trait<T_arg1>::take _A_a1,typename type_trait<T_arg2>::take _A_a2,typename type_trait<T_arg3>::take _A_a3) const
    { return (_A_obj.*func_ptr_)(_A_a1,_A_a2,_A_a3); }

protected:
  function_type func_ptr_;
};

/** volatile_mem_functor4 wraps volatile methods with 4 argument(s).
 * Use the convenience function mem_fun() to create an instance of volatile_mem_functor4.
 *
 * The following template arguments are used:
 * - @e T_arg1 Argument type used in the definition of operator()().
 * - @e T_arg2 Argument type used in the definition of operator()().
 * - @e T_arg3 Argument type used in the definition of operator()().
 * - @e T_arg4 Argument type used in the definition of operator()().
 * - @e T_return The return type of operator()().
 * - @e T_obj The object type.
 *
 * @ingroup mem_fun
 */
template <class T_return, class T_obj, class T_arg1,class T_arg2,class T_arg3,class T_arg4>
class volatile_mem_functor4 : public functor_base
{
public:
  typedef T_return (T_obj::*function_type)(T_arg1,T_arg2,T_arg3,T_arg4) volatile;
  typedef T_return result_type;

  /// Constructs an invalid functor.
  volatile_mem_functor4() : func_ptr_(0) {}

  /** Constructs a volatile_mem_functor4 object that wraps the passed method.
   * @param _A_func Pointer to method will be invoked from operator()().
   */
  explicit volatile_mem_functor4(function_type _A_func) : func_ptr_(_A_func) {}

  /** Execute the wrapped method operating on the passed instance.
   * @param _A_obj Pointer to instance the method should operate on.
   * @param _A_a1 Argument to be passed on to the method.
   * @param _A_a2 Argument to be passed on to the method.
   * @param _A_a3 Argument to be passed on to the method.
   * @param _A_a4 Argument to be passed on to the method.
   * @return The return value of the method invocation.
   */
  T_return operator()(T_obj* _A_obj, typename type_trait<T_arg1>::take _A_a1,typename type_trait<T_arg2>::take _A_a2,typename type_trait<T_arg3>::take _A_a3,typename type_trait<T_arg4>::take _A_a4) const
    { return (_A_obj->*(this->func_ptr_))(_A_a1,_A_a2,_A_a3,_A_a4); }

  /** Execute the wrapped method operating on the passed instance.
   * @param _A_obj Reference to instance the method should operate on.
   * @param _A_a1 Argument to be passed on to the method.
   * @param _A_a2 Argument to be passed on to the method.
   * @param _A_a3 Argument to be passed on to the method.
   * @param _A_a4 Argument to be passed on to the method.
   * @return The return value of the method invocation.
   */
  T_return operator()(T_obj& _A_obj, typename type_trait<T_arg1>::take _A_a1,typename type_trait<T_arg2>::take _A_a2,typename type_trait<T_arg3>::take _A_a3,typename type_trait<T_arg4>::take _A_a4) const
    { return (_A_obj.*func_ptr_)(_A_a1,_A_a2,_A_a3,_A_a4); }

protected:
  function_type func_ptr_;
};

/** volatile_mem_functor5 wraps volatile methods with 5 argument(s).
 * Use the convenience function mem_fun() to create an instance of volatile_mem_functor5.
 *
 * The following template arguments are used:
 * - @e T_arg1 Argument type used in the definition of operator()().
 * - @e T_arg2 Argument type used in the definition of operator()().
 * - @e T_arg3 Argument type used in the definition of operator()().
 * - @e T_arg4 Argument type used in the definition of operator()().
 * - @e T_arg5 Argument type used in the definition of operator()().
 * - @e T_return The return type of operator()().
 * - @e T_obj The object type.
 *
 * @ingroup mem_fun
 */
template <class T_return, class T_obj, class T_arg1,class T_arg2,class T_arg3,class T_arg4,class T_arg5>
class volatile_mem_functor5 : public functor_base
{
public:
  typedef T_return (T_obj::*function_type)(T_arg1,T_arg2,T_arg3,T_arg4,T_arg5) volatile;
  typedef T_return result_type;

  /// Constructs an invalid functor.
  volatile_mem_functor5() : func_ptr_(0) {}

  /** Constructs a volatile_mem_functor5 object that wraps the passed method.
   * @param _A_func Pointer to method will be invoked from operator()().
   */
  explicit volatile_mem_functor5(function_type _A_func) : func_ptr_(_A_func) {}

  /** Execute the wrapped method operating on the passed instance.
   * @param _A_obj Pointer to instance the method should operate on.
   * @param _A_a1 Argument to be passed on to the method.
   * @param _A_a2 Argument to be passed on to the method.
   * @param _A_a3 Argument to be passed on to the method.
   * @param _A_a4 Argument to be passed on to the method.
   * @param _A_a5 Argument to be passed on to the method.
   * @return The return value of the method invocation.
   */
  T_return operator()(T_obj* _A_obj, typename type_trait<T_arg1>::take _A_a1,typename type_trait<T_arg2>::take _A_a2,typename type_trait<T_arg3>::take _A_a3,typename type_trait<T_arg4>::take _A_a4,typename type_trait<T_arg5>::take _A_a5) const
    { return (_A_obj->*(this->func_ptr_))(_A_a1,_A_a2,_A_a3,_A_a4,_A_a5); }

  /** Execute the wrapped method operating on the passed instance.
   * @param _A_obj Reference to instance the method should operate on.
   * @param _A_a1 Argument to be passed on to the method.
   * @param _A_a2 Argument to be passed on to the method.
   * @param _A_a3 Argument to be passed on to the method.
   * @param _A_a4 Argument to be passed on to the method.
   * @param _A_a5 Argument to be passed on to the method.
   * @return The return value of the method invocation.
   */
  T_return operator()(T_obj& _A_obj, typename type_trait<T_arg1>::take _A_a1,typename type_trait<T_arg2>::take _A_a2,typename type_trait<T_arg3>::take _A_a3,typename type_trait<T_arg4>::take _A_a4,typename type_trait<T_arg5>::take _A_a5) const
    { return (_A_obj.*func_ptr_)(_A_a1,_A_a2,_A_a3,_A_a4,_A_a5); }

protected:
  function_type func_ptr_;
};

/** volatile_mem_functor6 wraps volatile methods with 6 argument(s).
 * Use the convenience function mem_fun() to create an instance of volatile_mem_functor6.
 *
 * The following template arguments are used:
 * - @e T_arg1 Argument type used in the definition of operator()().
 * - @e T_arg2 Argument type used in the definition of operator()().
 * - @e T_arg3 Argument type used in the definition of operator()().
 * - @e T_arg4 Argument type used in the definition of operator()().
 * - @e T_arg5 Argument type used in the definition of operator()().
 * - @e T_arg6 Argument type used in the definition of operator()().
 * - @e T_return The return type of operator()().
 * - @e T_obj The object type.
 *
 * @ingroup mem_fun
 */
template <class T_return, class T_obj, class T_arg1,class T_arg2,class T_arg3,class T_arg4,class T_arg5,class T_arg6>
class volatile_mem_functor6 : public functor_base
{
public:
  typedef T_return (T_obj::*function_type)(T_arg1,T_arg2,T_arg3,T_arg4,T_arg5,T_arg6) volatile;
  typedef T_return result_type;

  /// Constructs an invalid functor.
  volatile_mem_functor6() : func_ptr_(0) {}

  /** Constructs a volatile_mem_functor6 object that wraps the passed method.
   * @param _A_func Pointer to method will be invoked from operator()().
   */
  explicit volatile_mem_functor6(function_type _A_func) : func_ptr_(_A_func) {}

  /** Execute the wrapped method operating on the passed instance.
   * @param _A_obj Pointer to instance the method should operate on.
   * @param _A_a1 Argument to be passed on to the method.
   * @param _A_a2 Argument to be passed on to the method.
   * @param _A_a3 Argument to be passed on to the method.
   * @param _A_a4 Argument to be passed on to the method.
   * @param _A_a5 Argument to be passed on to the method.
   * @param _A_a6 Argument to be passed on to the method.
   * @return The return value of the method invocation.
   */
  T_return operator()(T_obj* _A_obj, typename type_trait<T_arg1>::take _A_a1,typename type_trait<T_arg2>::take _A_a2,typename type_trait<T_arg3>::take _A_a3,typename type_trait<T_arg4>::take _A_a4,typename type_trait<T_arg5>::take _A_a5,typename type_trait<T_arg6>::take _A_a6) const
    { return (_A_obj->*(this->func_ptr_))(_A_a1,_A_a2,_A_a3,_A_a4,_A_a5,_A_a6); }

  /** Execute the wrapped method operating on the passed instance.
   * @param _A_obj Reference to instance the method should operate on.
   * @param _A_a1 Argument to be passed on to the method.
   * @param _A_a2 Argument to be passed on to the method.
   * @param _A_a3 Argument to be passed on to the method.
   * @param _A_a4 Argument to be passed on to the method.
   * @param _A_a5 Argument to be passed on to the method.
   * @param _A_a6 Argument to be passed on to the method.
   * @return The return value of the method invocation.
   */
  T_return operator()(T_obj& _A_obj, typename type_trait<T_arg1>::take _A_a1,typename type_trait<T_arg2>::take _A_a2,typename type_trait<T_arg3>::take _A_a3,typename type_trait<T_arg4>::take _A_a4,typename type_trait<T_arg5>::take _A_a5,typename type_trait<T_arg6>::take _A_a6) const
    { return (_A_obj.*func_ptr_)(_A_a1,_A_a2,_A_a3,_A_a4,_A_a5,_A_a6); }

protected:
  function_type func_ptr_;
};

/** volatile_mem_functor7 wraps volatile methods with 7 argument(s).
 * Use the convenience function mem_fun() to create an instance of volatile_mem_functor7.
 *
 * The following template arguments are used:
 * - @e T_arg1 Argument type used in the definition of operator()().
 * - @e T_arg2 Argument type used in the definition of operator()().
 * - @e T_arg3 Argument type used in the definition of operator()().
 * - @e T_arg4 Argument type used in the definition of operator()().
 * - @e T_arg5 Argument type used in the definition of operator()().
 * - @e T_arg6 Argument type used in the definition of operator()().
 * - @e T_arg7 Argument type used in the definition of operator()().
 * - @e T_return The return type of operator()().
 * - @e T_obj The object type.
 *
 * @ingroup mem_fun
 */
template <class T_return, class T_obj, class T_arg1,class T_arg2,class T_arg3,class T_arg4,class T_arg5,class T_arg6,class T_arg7>
class volatile_mem_functor7 : public functor_base
{
public:
  typedef T_return (T_obj::*function_type)(T_arg1,T_arg2,T_arg3,T_arg4,T_arg5,T_arg6,T_arg7) volatile;
  typedef T_return result_type;

  /// Constructs an invalid functor.
  volatile_mem_functor7() : func_ptr_(0) {}

  /** Constructs a volatile_mem_functor7 object that wraps the passed method.
   * @param _A_func Pointer to method will be invoked from operator()().
   */
  explicit volatile_mem_functor7(function_type _A_func) : func_ptr_(_A_func) {}

  /** Execute the wrapped method operating on the passed instance.
   * @param _A_obj Pointer to instance the method should operate on.
   * @param _A_a1 Argument to be passed on to the method.
   * @param _A_a2 Argument to be passed on to the method.
   * @param _A_a3 Argument to be passed on to the method.
   * @param _A_a4 Argument to be passed on to the method.
   * @param _A_a5 Argument to be passed on to the method.
   * @param _A_a6 Argument to be passed on to the method.
   * @param _A_a7 Argument to be passed on to the method.
   * @return The return value of the method invocation.
   */
  T_return operator()(T_obj* _A_obj, typename type_trait<T_arg1>::take _A_a1,typename type_trait<T_arg2>::take _A_a2,typename type_trait<T_arg3>::take _A_a3,typename type_trait<T_arg4>::take _A_a4,typename type_trait<T_arg5>::take _A_a5,typename type_trait<T_arg6>::take _A_a6,typename type_trait<T_arg7>::take _A_a7) const
    { return (_A_obj->*(this->func_ptr_))(_A_a1,_A_a2,_A_a3,_A_a4,_A_a5,_A_a6,_A_a7); }

  /** Execute the wrapped method operating on the passed instance.
   * @param _A_obj Reference to instance the method should operate on.
   * @param _A_a1 Argument to be passed on to the method.
   * @param _A_a2 Argument to be passed on to the method.
   * @param _A_a3 Argument to be passed on to the method.
   * @param _A_a4 Argument to be passed on to the method.
   * @param _A_a5 Argument to be passed on to the method.
   * @param _A_a6 Argument to be passed on to the method.
   * @param _A_a7 Argument to be passed on to the method.
   * @return The return value of the method invocation.
   */
  T_return operator()(T_obj& _A_obj, typename type_trait<T_arg1>::take _A_a1,typename type_trait<T_arg2>::take _A_a2,typename type_trait<T_arg3>::take _A_a3,typename type_trait<T_arg4>::take _A_a4,typename type_trait<T_arg5>::take _A_a5,typename type_trait<T_arg6>::take _A_a6,typename type_trait<T_arg7>::take _A_a7) const
    { return (_A_obj.*func_ptr_)(_A_a1,_A_a2,_A_a3,_A_a4,_A_a5,_A_a6,_A_a7); }

protected:
  function_type func_ptr_;
};

/** const_volatile_mem_functor0 wraps const volatile methods with 0 argument(s).
 * Use the convenience function mem_fun() to create an instance of const_volatile_mem_functor0.
 *
 * The following template arguments are used:
 * - @e T_return The return type of operator()().
 * - @e T_obj The object type.
 *
 * @ingroup mem_fun
 */
template <class T_return, class T_obj>
class const_volatile_mem_functor0 : public functor_base
{
public:
  typedef T_return (T_obj::*function_type)() const volatile;
  typedef T_return result_type;

  /// Constructs an invalid functor.
  const_volatile_mem_functor0() : func_ptr_(0) {}

  /** Constructs a const_volatile_mem_functor0 object that wraps the passed method.
   * @param _A_func Pointer to method will be invoked from operator()().
   */
  explicit const_volatile_mem_functor0(function_type _A_func) : func_ptr_(_A_func) {}

  /** Execute the wrapped method operating on the passed instance.
   * @param _A_obj Pointer to instance the method should operate on.
   * @return The return value of the method invocation.
   */
  T_return operator()(const T_obj* _A_obj) const
    { return (_A_obj->*(this->func_ptr_))(); }

  /** Execute the wrapped method operating on the passed instance.
   * @param _A_obj Reference to instance the method should operate on.
   * @return The return value of the method invocation.
   */
  T_return operator()(const T_obj& _A_obj) const
    { return (_A_obj.*func_ptr_)(); }

protected:
  function_type func_ptr_;
};

/** const_volatile_mem_functor1 wraps const volatile methods with 1 argument(s).
 * Use the convenience function mem_fun() to create an instance of const_volatile_mem_functor1.
 *
 * The following template arguments are used:
 * - @e T_arg1 Argument type used in the definition of operator()().
 * - @e T_return The return type of operator()().
 * - @e T_obj The object type.
 *
 * @ingroup mem_fun
 */
template <class T_return, class T_obj, class T_arg1>
class const_volatile_mem_functor1 : public functor_base
{
public:
  typedef T_return (T_obj::*function_type)(T_arg1) const volatile;
  typedef T_return result_type;

  /// Constructs an invalid functor.
  const_volatile_mem_functor1() : func_ptr_(0) {}

  /** Constructs a const_volatile_mem_functor1 object that wraps the passed method.
   * @param _A_func Pointer to method will be invoked from operator()().
   */
  explicit const_volatile_mem_functor1(function_type _A_func) : func_ptr_(_A_func) {}

  /** Execute the wrapped method operating on the passed instance.
   * @param _A_obj Pointer to instance the method should operate on.
   * @param _A_a1 Argument to be passed on to the method.
   * @return The return value of the method invocation.
   */
  T_return operator()(const T_obj* _A_obj, typename type_trait<T_arg1>::take _A_a1) const
    { return (_A_obj->*(this->func_ptr_))(_A_a1); }

  /** Execute the wrapped method operating on the passed instance.
   * @param _A_obj Reference to instance the method should operate on.
   * @param _A_a1 Argument to be passed on to the method.
   * @return The return value of the method invocation.
   */
  T_return operator()(const T_obj& _A_obj, typename type_trait<T_arg1>::take _A_a1) const
    { return (_A_obj.*func_ptr_)(_A_a1); }

protected:
  function_type func_ptr_;
};

/** const_volatile_mem_functor2 wraps const volatile methods with 2 argument(s).
 * Use the convenience function mem_fun() to create an instance of const_volatile_mem_functor2.
 *
 * The following template arguments are used:
 * - @e T_arg1 Argument type used in the definition of operator()().
 * - @e T_arg2 Argument type used in the definition of operator()().
 * - @e T_return The return type of operator()().
 * - @e T_obj The object type.
 *
 * @ingroup mem_fun
 */
template <class T_return, class T_obj, class T_arg1,class T_arg2>
class const_volatile_mem_functor2 : public functor_base
{
public:
  typedef T_return (T_obj::*function_type)(T_arg1,T_arg2) const volatile;
  typedef T_return result_type;

  /// Constructs an invalid functor.
  const_volatile_mem_functor2() : func_ptr_(0) {}

  /** Constructs a const_volatile_mem_functor2 object that wraps the passed method.
   * @param _A_func Pointer to method will be invoked from operator()().
   */
  explicit const_volatile_mem_functor2(function_type _A_func) : func_ptr_(_A_func) {}

  /** Execute the wrapped method operating on the passed instance.
   * @param _A_obj Pointer to instance the method should operate on.
   * @param _A_a1 Argument to be passed on to the method.
   * @param _A_a2 Argument to be passed on to the method.
   * @return The return value of the method invocation.
   */
  T_return operator()(const T_obj* _A_obj, typename type_trait<T_arg1>::take _A_a1,typename type_trait<T_arg2>::take _A_a2) const
    { return (_A_obj->*(this->func_ptr_))(_A_a1,_A_a2); }

  /** Execute the wrapped method operating on the passed instance.
   * @param _A_obj Reference to instance the method should operate on.
   * @param _A_a1 Argument to be passed on to the method.
   * @param _A_a2 Argument to be passed on to the method.
   * @return The return value of the method invocation.
   */
  T_return operator()(const T_obj& _A_obj, typename type_trait<T_arg1>::take _A_a1,typename type_trait<T_arg2>::take _A_a2) const
    { return (_A_obj.*func_ptr_)(_A_a1,_A_a2); }

protected:
  function_type func_ptr_;
};

/** const_volatile_mem_functor3 wraps const volatile methods with 3 argument(s).
 * Use the convenience function mem_fun() to create an instance of const_volatile_mem_functor3.
 *
 * The following template arguments are used:
 * - @e T_arg1 Argument type used in the definition of operator()().
 * - @e T_arg2 Argument type used in the definition of operator()().
 * - @e T_arg3 Argument type used in the definition of operator()().
 * - @e T_return The return type of operator()().
 * - @e T_obj The object type.
 *
 * @ingroup mem_fun
 */
template <class T_return, class T_obj, class T_arg1,class T_arg2,class T_arg3>
class const_volatile_mem_functor3 : public functor_base
{
public:
  typedef T_return (T_obj::*function_type)(T_arg1,T_arg2,T_arg3) const volatile;
  typedef T_return result_type;

  /// Constructs an invalid functor.
  const_volatile_mem_functor3() : func_ptr_(0) {}

  /** Constructs a const_volatile_mem_functor3 object that wraps the passed method.
   * @param _A_func Pointer to method will be invoked from operator()().
   */
  explicit const_volatile_mem_functor3(function_type _A_func) : func_ptr_(_A_func) {}

  /** Execute the wrapped method operating on the passed instance.
   * @param _A_obj Pointer to instance the method should operate on.
   * @param _A_a1 Argument to be passed on to the method.
   * @param _A_a2 Argument to be passed on to the method.
   * @param _A_a3 Argument to be passed on to the method.
   * @return The return value of the method invocation.
   */
  T_return operator()(const T_obj* _A_obj, typename type_trait<T_arg1>::take _A_a1,typename type_trait<T_arg2>::take _A_a2,typename type_trait<T_arg3>::take _A_a3) const
    { return (_A_obj->*(this->func_ptr_))(_A_a1,_A_a2,_A_a3); }

  /** Execute the wrapped method operating on the passed instance.
   * @param _A_obj Reference to instance the method should operate on.
   * @param _A_a1 Argument to be passed on to the method.
   * @param _A_a2 Argument to be passed on to the method.
   * @param _A_a3 Argument to be passed on to the method.
   * @return The return value of the method invocation.
   */
  T_return operator()(const T_obj& _A_obj, typename type_trait<T_arg1>::take _A_a1,typename type_trait<T_arg2>::take _A_a2,typename type_trait<T_arg3>::take _A_a3) const
    { return (_A_obj.*func_ptr_)(_A_a1,_A_a2,_A_a3); }

protected:
  function_type func_ptr_;
};

/** const_volatile_mem_functor4 wraps const volatile methods with 4 argument(s).
 * Use the convenience function mem_fun() to create an instance of const_volatile_mem_functor4.
 *
 * The following template arguments are used:
 * - @e T_arg1 Argument type used in the definition of operator()().
 * - @e T_arg2 Argument type used in the definition of operator()().
 * - @e T_arg3 Argument type used in the definition of operator()().
 * - @e T_arg4 Argument type used in the definition of operator()().
 * - @e T_return The return type of operator()().
 * - @e T_obj The object type.
 *
 * @ingroup mem_fun
 */
template <class T_return, class T_obj, class T_arg1,class T_arg2,class T_arg3,class T_arg4>
class const_volatile_mem_functor4 : public functor_base
{
public:
  typedef T_return (T_obj::*function_type)(T_arg1,T_arg2,T_arg3,T_arg4) const volatile;
  typedef T_return result_type;

  /// Constructs an invalid functor.
  const_volatile_mem_functor4() : func_ptr_(0) {}

  /** Constructs a const_volatile_mem_functor4 object that wraps the passed method.
   * @param _A_func Pointer to method will be invoked from operator()().
   */
  explicit const_volatile_mem_functor4(function_type _A_func) : func_ptr_(_A_func) {}

  /** Execute the wrapped method operating on the passed instance.
   * @param _A_obj Pointer to instance the method should operate on.
   * @param _A_a1 Argument to be passed on to the method.
   * @param _A_a2 Argument to be passed on to the method.
   * @param _A_a3 Argument to be passed on to the method.
   * @param _A_a4 Argument to be passed on to the method.
   * @return The return value of the method invocation.
   */
  T_return operator()(const T_obj* _A_obj, typename type_trait<T_arg1>::take _A_a1,typename type_trait<T_arg2>::take _A_a2,typename type_trait<T_arg3>::take _A_a3,typename type_trait<T_arg4>::take _A_a4) const
    { return (_A_obj->*(this->func_ptr_))(_A_a1,_A_a2,_A_a3,_A_a4); }

  /** Execute the wrapped method operating on the passed instance.
   * @param _A_obj Reference to instance the method should operate on.
   * @param _A_a1 Argument to be passed on to the method.
   * @param _A_a2 Argument to be passed on to the method.
   * @param _A_a3 Argument to be passed on to the method.
   * @param _A_a4 Argument to be passed on to the method.
   * @return The return value of the method invocation.
   */
  T_return operator()(const T_obj& _A_obj, typename type_trait<T_arg1>::take _A_a1,typename type_trait<T_arg2>::take _A_a2,typename type_trait<T_arg3>::take _A_a3,typename type_trait<T_arg4>::take _A_a4) const
    { return (_A_obj.*func_ptr_)(_A_a1,_A_a2,_A_a3,_A_a4); }

protected:
  function_type func_ptr_;
};

/** const_volatile_mem_functor5 wraps const volatile methods with 5 argument(s).
 * Use the convenience function mem_fun() to create an instance of const_volatile_mem_functor5.
 *
 * The following template arguments are used:
 * - @e T_arg1 Argument type used in the definition of operator()().
 * - @e T_arg2 Argument type used in the definition of operator()().
 * - @e T_arg3 Argument type used in the definition of operator()().
 * - @e T_arg4 Argument type used in the definition of operator()().
 * - @e T_arg5 Argument type used in the definition of operator()().
 * - @e T_return The return type of operator()().
 * - @e T_obj The object type.
 *
 * @ingroup mem_fun
 */
template <class T_return, class T_obj, class T_arg1,class T_arg2,class T_arg3,class T_arg4,class T_arg5>
class const_volatile_mem_functor5 : public functor_base
{
public:
  typedef T_return (T_obj::*function_type)(T_arg1,T_arg2,T_arg3,T_arg4,T_arg5) const volatile;
  typedef T_return result_type;

  /// Constructs an invalid functor.
  const_volatile_mem_functor5() : func_ptr_(0) {}

  /** Constructs a const_volatile_mem_functor5 object that wraps the passed method.
   * @param _A_func Pointer to method will be invoked from operator()().
   */
  explicit const_volatile_mem_functor5(function_type _A_func) : func_ptr_(_A_func) {}

  /** Execute the wrapped method operating on the passed instance.
   * @param _A_obj Pointer to instance the method should operate on.
   * @param _A_a1 Argument to be passed on to the method.
   * @param _A_a2 Argument to be passed on to the method.
   * @param _A_a3 Argument to be passed on to the method.
   * @param _A_a4 Argument to be passed on to the method.
   * @param _A_a5 Argument to be passed on to the method.
   * @return The return value of the method invocation.
   */
  T_return operator()(const T_obj* _A_obj, typename type_trait<T_arg1>::take _A_a1,typename type_trait<T_arg2>::take _A_a2,typename type_trait<T_arg3>::take _A_a3,typename type_trait<T_arg4>::take _A_a4,typename type_trait<T_arg5>::take _A_a5) const
    { return (_A_obj->*(this->func_ptr_))(_A_a1,_A_a2,_A_a3,_A_a4,_A_a5); }

  /** Execute the wrapped method operating on the passed instance.
   * @param _A_obj Reference to instance the method should operate on.
   * @param _A_a1 Argument to be passed on to the method.
   * @param _A_a2 Argument to be passed on to the method.
   * @param _A_a3 Argument to be passed on to the method.
   * @param _A_a4 Argument to be passed on to the method.
   * @param _A_a5 Argument to be passed on to the method.
   * @return The return value of the method invocation.
   */
  T_return operator()(const T_obj& _A_obj, typename type_trait<T_arg1>::take _A_a1,typename type_trait<T_arg2>::take _A_a2,typename type_trait<T_arg3>::take _A_a3,typename type_trait<T_arg4>::take _A_a4,typename type_trait<T_arg5>::take _A_a5) const
    { return (_A_obj.*func_ptr_)(_A_a1,_A_a2,_A_a3,_A_a4,_A_a5); }

protected:
  function_type func_ptr_;
};

/** const_volatile_mem_functor6 wraps const volatile methods with 6 argument(s).
 * Use the convenience function mem_fun() to create an instance of const_volatile_mem_functor6.
 *
 * The following template arguments are used:
 * - @e T_arg1 Argument type used in the definition of operator()().
 * - @e T_arg2 Argument type used in the definition of operator()().
 * - @e T_arg3 Argument type used in the definition of operator()().
 * - @e T_arg4 Argument type used in the definition of operator()().
 * - @e T_arg5 Argument type used in the definition of operator()().
 * - @e T_arg6 Argument type used in the definition of operator()().
 * - @e T_return The return type of operator()().
 * - @e T_obj The object type.
 *
 * @ingroup mem_fun
 */
template <class T_return, class T_obj, class T_arg1,class T_arg2,class T_arg3,class T_arg4,class T_arg5,class T_arg6>
class const_volatile_mem_functor6 : public functor_base
{
public:
  typedef T_return (T_obj::*function_type)(T_arg1,T_arg2,T_arg3,T_arg4,T_arg5,T_arg6) const volatile;
  typedef T_return result_type;

  /// Constructs an invalid functor.
  const_volatile_mem_functor6() : func_ptr_(0) {}

  /** Constructs a const_volatile_mem_functor6 object that wraps the passed method.
   * @param _A_func Pointer to method will be invoked from operator()().
   */
  explicit const_volatile_mem_functor6(function_type _A_func) : func_ptr_(_A_func) {}

  /** Execute the wrapped method operating on the passed instance.
   * @param _A_obj Pointer to instance the method should operate on.
   * @param _A_a1 Argument to be passed on to the method.
   * @param _A_a2 Argument to be passed on to the method.
   * @param _A_a3 Argument to be passed on to the method.
   * @param _A_a4 Argument to be passed on to the method.
   * @param _A_a5 Argument to be passed on to the method.
   * @param _A_a6 Argument to be passed on to the method.
   * @return The return value of the method invocation.
   */
  T_return operator()(const T_obj* _A_obj, typename type_trait<T_arg1>::take _A_a1,typename type_trait<T_arg2>::take _A_a2,typename type_trait<T_arg3>::take _A_a3,typename type_trait<T_arg4>::take _A_a4,typename type_trait<T_arg5>::take _A_a5,typename type_trait<T_arg6>::take _A_a6) const
    { return (_A_obj->*(this->func_ptr_))(_A_a1,_A_a2,_A_a3,_A_a4,_A_a5,_A_a6); }

  /** Execute the wrapped method operating on the passed instance.
   * @param _A_obj Reference to instance the method should operate on.
   * @param _A_a1 Argument to be passed on to the method.
   * @param _A_a2 Argument to be passed on to the method.
   * @param _A_a3 Argument to be passed on to the method.
   * @param _A_a4 Argument to be passed on to the method.
   * @param _A_a5 Argument to be passed on to the method.
   * @param _A_a6 Argument to be passed on to the method.
   * @return The return value of the method invocation.
   */
  T_return operator()(const T_obj& _A_obj, typename type_trait<T_arg1>::take _A_a1,typename type_trait<T_arg2>::take _A_a2,typename type_trait<T_arg3>::take _A_a3,typename type_trait<T_arg4>::take _A_a4,typename type_trait<T_arg5>::take _A_a5,typename type_trait<T_arg6>::take _A_a6) const
    { return (_A_obj.*func_ptr_)(_A_a1,_A_a2,_A_a3,_A_a4,_A_a5,_A_a6); }

protected:
  function_type func_ptr_;
};

/** const_volatile_mem_functor7 wraps const volatile methods with 7 argument(s).
 * Use the convenience function mem_fun() to create an instance of const_volatile_mem_functor7.
 *
 * The following template arguments are used:
 * - @e T_arg1 Argument type used in the definition of operator()().
 * - @e T_arg2 Argument type used in the definition of operator()().
 * - @e T_arg3 Argument type used in the definition of operator()().
 * - @e T_arg4 Argument type used in the definition of operator()().
 * - @e T_arg5 Argument type used in the definition of operator()().
 * - @e T_arg6 Argument type used in the definition of operator()().
 * - @e T_arg7 Argument type used in the definition of operator()().
 * - @e T_return The return type of operator()().
 * - @e T_obj The object type.
 *
 * @ingroup mem_fun
 */
template <class T_return, class T_obj, class T_arg1,class T_arg2,class T_arg3,class T_arg4,class T_arg5,class T_arg6,class T_arg7>
class const_volatile_mem_functor7 : public functor_base
{
public:
  typedef T_return (T_obj::*function_type)(T_arg1,T_arg2,T_arg3,T_arg4,T_arg5,T_arg6,T_arg7) const volatile;
  typedef T_return result_type;

  /// Constructs an invalid functor.
  const_volatile_mem_functor7() : func_ptr_(0) {}

  /** Constructs a const_volatile_mem_functor7 object that wraps the passed method.
   * @param _A_func Pointer to method will be invoked from operator()().
   */
  explicit const_volatile_mem_functor7(function_type _A_func) : func_ptr_(_A_func) {}

  /** Execute the wrapped method operating on the passed instance.
   * @param _A_obj Pointer to instance the method should operate on.
   * @param _A_a1 Argument to be passed on to the method.
   * @param _A_a2 Argument to be passed on to the method.
   * @param _A_a3 Argument to be passed on to the method.
   * @param _A_a4 Argument to be passed on to the method.
   * @param _A_a5 Argument to be passed on to the method.
   * @param _A_a6 Argument to be passed on to the method.
   * @param _A_a7 Argument to be passed on to the method.
   * @return The return value of the method invocation.
   */
  T_return operator()(const T_obj* _A_obj, typename type_trait<T_arg1>::take _A_a1,typename type_trait<T_arg2>::take _A_a2,typename type_trait<T_arg3>::take _A_a3,typename type_trait<T_arg4>::take _A_a4,typename type_trait<T_arg5>::take _A_a5,typename type_trait<T_arg6>::take _A_a6,typename type_trait<T_arg7>::take _A_a7) const
    { return (_A_obj->*(this->func_ptr_))(_A_a1,_A_a2,_A_a3,_A_a4,_A_a5,_A_a6,_A_a7); }

  /** Execute the wrapped method operating on the passed instance.
   * @param _A_obj Reference to instance the method should operate on.
   * @param _A_a1 Argument to be passed on to the method.
   * @param _A_a2 Argument to be passed on to the method.
   * @param _A_a3 Argument to be passed on to the method.
   * @param _A_a4 Argument to be passed on to the method.
   * @param _A_a5 Argument to be passed on to the method.
   * @param _A_a6 Argument to be passed on to the method.
   * @param _A_a7 Argument to be passed on to the method.
   * @return The return value of the method invocation.
   */
  T_return operator()(const T_obj& _A_obj, typename type_trait<T_arg1>::take _A_a1,typename type_trait<T_arg2>::take _A_a2,typename type_trait<T_arg3>::take _A_a3,typename type_trait<T_arg4>::take _A_a4,typename type_trait<T_arg5>::take _A_a5,typename type_trait<T_arg6>::take _A_a6,typename type_trait<T_arg7>::take _A_a7) const
    { return (_A_obj.*func_ptr_)(_A_a1,_A_a2,_A_a3,_A_a4,_A_a5,_A_a6,_A_a7); }

protected:
  function_type func_ptr_;
};


/** bound_mem_functor0 encapsulates a  method with 0 arguments and an object instance.
 * Use the convenience function mem_fun() to create an instance of bound_mem_functor0.
 *
 * The following template arguments are used:
 * - @e T_return The return type of operator()().
 * - @e T_obj The object type.
 *
 * @ingroup mem_fun
 */
template <class T_return, class T_obj>
class bound_mem_functor0
  : public mem_functor0<T_return, T_obj>
{
  typedef mem_functor0<T_return, T_obj> base_type_;
public:
  typedef typename base_type_::function_type function_type;

  /** Constructs a bound_mem_functor0 object that wraps the passed method.
   * @param _A_obj Pointer to instance the method will operate on.
   * @param _A_func Pointer to method will be invoked from operator()().
   */
  bound_mem_functor0( T_obj* _A_obj, function_type _A_func)
    : base_type_(_A_func),
      obj_(*_A_obj)
    {}

  /** Constructs a bound_mem_functor0 object that wraps the passed method.
   * @param _A_obj Reference to instance the method will operate on.
   * @param _A_func Pointer to method will be invoked from operator()().
   */
  bound_mem_functor0( T_obj& _A_obj, function_type _A_func)
    : base_type_(_A_func),
      obj_(_A_obj)
    {}

  /** Execute the wrapped method operating on the stored instance.
   * @return The return value of the method invocation.
   */
  T_return operator()() const
    { return (obj_.invoke().*(this->func_ptr_))(); }

//protected:
  // Reference to stored object instance.
  // This is the handler object, such as TheObject in void TheObject::signal_handler().
  limit_reference<T_obj> obj_;
};

//template specialization of visit_each<>(action, functor):
/** Performs a functor on each of the targets of a functor.
 * The function overload for sigc::bound_mem_functor performs a functor
 * on the object instance stored in the sigc::bound_mem_functor object.
 *
 * @ingroup mem_fun
 */
template <class T_action, class T_return, class T_obj>
void visit_each(const T_action& _A_action,
                const bound_mem_functor0<T_return, T_obj>& _A_target)
{
  sigc::visit_each(_A_action, _A_target.obj_);
}


/** bound_mem_functor1 encapsulates a  method with 1 arguments and an object instance.
 * Use the convenience function mem_fun() to create an instance of bound_mem_functor1.
 *
 * The following template arguments are used:
 * - @e T_arg1 Argument type used in the definition of operator()().
 * - @e T_return The return type of operator()().
 * - @e T_obj The object type.
 *
 * @ingroup mem_fun
 */
template <class T_return, class T_obj, class T_arg1>
class bound_mem_functor1
  : public mem_functor1<T_return, T_obj, T_arg1>
{
  typedef mem_functor1<T_return, T_obj, T_arg1> base_type_;
public:
  typedef typename base_type_::function_type function_type;

  /** Constructs a bound_mem_functor1 object that wraps the passed method.
   * @param _A_obj Pointer to instance the method will operate on.
   * @param _A_func Pointer to method will be invoked from operator()().
   */
  bound_mem_functor1( T_obj* _A_obj, function_type _A_func)
    : base_type_(_A_func),
      obj_(*_A_obj)
    {}

  /** Constructs a bound_mem_functor1 object that wraps the passed method.
   * @param _A_obj Reference to instance the method will operate on.
   * @param _A_func Pointer to method will be invoked from operator()().
   */
  bound_mem_functor1( T_obj& _A_obj, function_type _A_func)
    : base_type_(_A_func),
      obj_(_A_obj)
    {}

  /** Execute the wrapped method operating on the stored instance.
   * @param _A_a1 Argument to be passed on to the method.
   * @return The return value of the method invocation.
   */
  T_return operator()(typename type_trait<T_arg1>::take _A_a1) const
    { return (obj_.invoke().*(this->func_ptr_))(_A_a1); }

//protected:
  // Reference to stored object instance.
  // This is the handler object, such as TheObject in void TheObject::signal_handler().
  limit_reference<T_obj> obj_;
};

//template specialization of visit_each<>(action, functor):
/** Performs a functor on each of the targets of a functor.
 * The function overload for sigc::bound_mem_functor performs a functor
 * on the object instance stored in the sigc::bound_mem_functor object.
 *
 * @ingroup mem_fun
 */
template <class T_action, class T_return, class T_obj, class T_arg1>
void visit_each(const T_action& _A_action,
                const bound_mem_functor1<T_return, T_obj, T_arg1>& _A_target)
{
  sigc::visit_each(_A_action, _A_target.obj_);
}


/** bound_mem_functor2 encapsulates a  method with 2 arguments and an object instance.
 * Use the convenience function mem_fun() to create an instance of bound_mem_functor2.
 *
 * The following template arguments are used:
 * - @e T_arg1 Argument type used in the definition of operator()().
 * - @e T_arg2 Argument type used in the definition of operator()().
 * - @e T_return The return type of operator()().
 * - @e T_obj The object type.
 *
 * @ingroup mem_fun
 */
template <class T_return, class T_obj, class T_arg1,class T_arg2>
class bound_mem_functor2
  : public mem_functor2<T_return, T_obj, T_arg1,T_arg2>
{
  typedef mem_functor2<T_return, T_obj, T_arg1,T_arg2> base_type_;
public:
  typedef typename base_type_::function_type function_type;

  /** Constructs a bound_mem_functor2 object that wraps the passed method.
   * @param _A_obj Pointer to instance the method will operate on.
   * @param _A_func Pointer to method will be invoked from operator()().
   */
  bound_mem_functor2( T_obj* _A_obj, function_type _A_func)
    : base_type_(_A_func),
      obj_(*_A_obj)
    {}

  /** Constructs a bound_mem_functor2 object that wraps the passed method.
   * @param _A_obj Reference to instance the method will operate on.
   * @param _A_func Pointer to method will be invoked from operator()().
   */
  bound_mem_functor2( T_obj& _A_obj, function_type _A_func)
    : base_type_(_A_func),
      obj_(_A_obj)
    {}

  /** Execute the wrapped method operating on the stored instance.
   * @param _A_a1 Argument to be passed on to the method.
   * @param _A_a2 Argument to be passed on to the method.
   * @return The return value of the method invocation.
   */
  T_return operator()(typename type_trait<T_arg1>::take _A_a1,typename type_trait<T_arg2>::take _A_a2) const
    { return (obj_.invoke().*(this->func_ptr_))(_A_a1,_A_a2); }

//protected:
  // Reference to stored object instance.
  // This is the handler object, such as TheObject in void TheObject::signal_handler().
  limit_reference<T_obj> obj_;
};

//template specialization of visit_each<>(action, functor):
/** Performs a functor on each of the targets of a functor.
 * The function overload for sigc::bound_mem_functor performs a functor
 * on the object instance stored in the sigc::bound_mem_functor object.
 *
 * @ingroup mem_fun
 */
template <class T_action, class T_return, class T_obj, class T_arg1,class T_arg2>
void visit_each(const T_action& _A_action,
                const bound_mem_functor2<T_return, T_obj, T_arg1,T_arg2>& _A_target)
{
  sigc::visit_each(_A_action, _A_target.obj_);
}


/** bound_mem_functor3 encapsulates a  method with 3 arguments and an object instance.
 * Use the convenience function mem_fun() to create an instance of bound_mem_functor3.
 *
 * The following template arguments are used:
 * - @e T_arg1 Argument type used in the definition of operator()().
 * - @e T_arg2 Argument type used in the definition of operator()().
 * - @e T_arg3 Argument type used in the definition of operator()().
 * - @e T_return The return type of operator()().
 * - @e T_obj The object type.
 *
 * @ingroup mem_fun
 */
template <class T_return, class T_obj, class T_arg1,class T_arg2,class T_arg3>
class bound_mem_functor3
  : public mem_functor3<T_return, T_obj, T_arg1,T_arg2,T_arg3>
{
  typedef mem_functor3<T_return, T_obj, T_arg1,T_arg2,T_arg3> base_type_;
public:
  typedef typename base_type_::function_type function_type;

  /** Constructs a bound_mem_functor3 object that wraps the passed method.
   * @param _A_obj Pointer to instance the method will operate on.
   * @param _A_func Pointer to method will be invoked from operator()().
   */
  bound_mem_functor3( T_obj* _A_obj, function_type _A_func)
    : base_type_(_A_func),
      obj_(*_A_obj)
    {}

  /** Constructs a bound_mem_functor3 object that wraps the passed method.
   * @param _A_obj Reference to instance the method will operate on.
   * @param _A_func Pointer to method will be invoked from operator()().
   */
  bound_mem_functor3( T_obj& _A_obj, function_type _A_func)
    : base_type_(_A_func),
      obj_(_A_obj)
    {}

  /** Execute the wrapped method operating on the stored instance.
   * @param _A_a1 Argument to be passed on to the method.
   * @param _A_a2 Argument to be passed on to the method.
   * @param _A_a3 Argument to be passed on to the method.
   * @return The return value of the method invocation.
   */
  T_return operator()(typename type_trait<T_arg1>::take _A_a1,typename type_trait<T_arg2>::take _A_a2,typename type_trait<T_arg3>::take _A_a3) const
    { return (obj_.invoke().*(this->func_ptr_))(_A_a1,_A_a2,_A_a3); }

//protected:
  // Reference to stored object instance.
  // This is the handler object, such as TheObject in void TheObject::signal_handler().
  limit_reference<T_obj> obj_;
};

//template specialization of visit_each<>(action, functor):
/** Performs a functor on each of the targets of a functor.
 * The function overload for sigc::bound_mem_functor performs a functor
 * on the object instance stored in the sigc::bound_mem_functor object.
 *
 * @ingroup mem_fun
 */
template <class T_action, class T_return, class T_obj, class T_arg1,class T_arg2,class T_arg3>
void visit_each(const T_action& _A_action,
                const bound_mem_functor3<T_return, T_obj, T_arg1,T_arg2,T_arg3>& _A_target)
{
  sigc::visit_each(_A_action, _A_target.obj_);
}


/** bound_mem_functor4 encapsulates a  method with 4 arguments and an object instance.
 * Use the convenience function mem_fun() to create an instance of bound_mem_functor4.
 *
 * The following template arguments are used:
 * - @e T_arg1 Argument type used in the definition of operator()().
 * - @e T_arg2 Argument type used in the definition of operator()().
 * - @e T_arg3 Argument type used in the definition of operator()().
 * - @e T_arg4 Argument type used in the definition of operator()().
 * - @e T_return The return type of operator()().
 * - @e T_obj The object type.
 *
 * @ingroup mem_fun
 */
template <class T_return, class T_obj, class T_arg1,class T_arg2,class T_arg3,class T_arg4>
class bound_mem_functor4
  : public mem_functor4<T_return, T_obj, T_arg1,T_arg2,T_arg3,T_arg4>
{
  typedef mem_functor4<T_return, T_obj, T_arg1,T_arg2,T_arg3,T_arg4> base_type_;
public:
  typedef typename base_type_::function_type function_type;

  /** Constructs a bound_mem_functor4 object that wraps the passed method.
   * @param _A_obj Pointer to instance the method will operate on.
   * @param _A_func Pointer to method will be invoked from operator()().
   */
  bound_mem_functor4( T_obj* _A_obj, function_type _A_func)
    : base_type_(_A_func),
      obj_(*_A_obj)
    {}

  /** Constructs a bound_mem_functor4 object that wraps the passed method.
   * @param _A_obj Reference to instance the method will operate on.
   * @param _A_func Pointer to method will be invoked from operator()().
   */
  bound_mem_functor4( T_obj& _A_obj, function_type _A_func)
    : base_type_(_A_func),
      obj_(_A_obj)
    {}

  /** Execute the wrapped method operating on the stored instance.
   * @param _A_a1 Argument to be passed on to the method.
   * @param _A_a2 Argument to be passed on to the method.
   * @param _A_a3 Argument to be passed on to the method.
   * @param _A_a4 Argument to be passed on to the method.
   * @return The return value of the method invocation.
   */
  T_return operator()(typename type_trait<T_arg1>::take _A_a1,typename type_trait<T_arg2>::take _A_a2,typename type_trait<T_arg3>::take _A_a3,typename type_trait<T_arg4>::take _A_a4) const
    { return (obj_.invoke().*(this->func_ptr_))(_A_a1,_A_a2,_A_a3,_A_a4); }

//protected:
  // Reference to stored object instance.
  // This is the handler object, such as TheObject in void TheObject::signal_handler().
  limit_reference<T_obj> obj_;
};

//template specialization of visit_each<>(action, functor):
/** Performs a functor on each of the targets of a functor.
 * The function overload for sigc::bound_mem_functor performs a functor
 * on the object instance stored in the sigc::bound_mem_functor object.
 *
 * @ingroup mem_fun
 */
template <class T_action, class T_return, class T_obj, class T_arg1,class T_arg2,class T_arg3,class T_arg4>
void visit_each(const T_action& _A_action,
                const bound_mem_functor4<T_return, T_obj, T_arg1,T_arg2,T_arg3,T_arg4>& _A_target)
{
  sigc::visit_each(_A_action, _A_target.obj_);
}


/** bound_mem_functor5 encapsulates a  method with 5 arguments and an object instance.
 * Use the convenience function mem_fun() to create an instance of bound_mem_functor5.
 *
 * The following template arguments are used:
 * - @e T_arg1 Argument type used in the definition of operator()().
 * - @e T_arg2 Argument type used in the definition of operator()().
 * - @e T_arg3 Argument type used in the definition of operator()().
 * - @e T_arg4 Argument type used in the definition of operator()().
 * - @e T_arg5 Argument type used in the definition of operator()().
 * - @e T_return The return type of operator()().
 * - @e T_obj The object type.
 *
 * @ingroup mem_fun
 */
template <class T_return, class T_obj, class T_arg1,class T_arg2,class T_arg3,class T_arg4,class T_arg5>
class bound_mem_functor5
  : public mem_functor5<T_return, T_obj, T_arg1,T_arg2,T_arg3,T_arg4,T_arg5>
{
  typedef mem_functor5<T_return, T_obj, T_arg1,T_arg2,T_arg3,T_arg4,T_arg5> base_type_;
public:
  typedef typename base_type_::function_type function_type;

  /** Constructs a bound_mem_functor5 object that wraps the passed method.
   * @param _A_obj Pointer to instance the method will operate on.
   * @param _A_func Pointer to method will be invoked from operator()().
   */
  bound_mem_functor5( T_obj* _A_obj, function_type _A_func)
    : base_type_(_A_func),
      obj_(*_A_obj)
    {}

  /** Constructs a bound_mem_functor5 object that wraps the passed method.
   * @param _A_obj Reference to instance the method will operate on.
   * @param _A_func Pointer to method will be invoked from operator()().
   */
  bound_mem_functor5( T_obj& _A_obj, function_type _A_func)
    : base_type_(_A_func),
      obj_(_A_obj)
    {}

  /** Execute the wrapped method operating on the stored instance.
   * @param _A_a1 Argument to be passed on to the method.
   * @param _A_a2 Argument to be passed on to the method.
   * @param _A_a3 Argument to be passed on to the method.
   * @param _A_a4 Argument to be passed on to the method.
   * @param _A_a5 Argument to be passed on to the method.
   * @return The return value of the method invocation.
   */
  T_return operator()(typename type_trait<T_arg1>::take _A_a1,typename type_trait<T_arg2>::take _A_a2,typename type_trait<T_arg3>::take _A_a3,typename type_trait<T_arg4>::take _A_a4,typename type_trait<T_arg5>::take _A_a5) const
    { return (obj_.invoke().*(this->func_ptr_))(_A_a1,_A_a2,_A_a3,_A_a4,_A_a5); }

//protected:
  // Reference to stored object instance.
  // This is the handler object, such as TheObject in void TheObject::signal_handler().
  limit_reference<T_obj> obj_;
};

//template specialization of visit_each<>(action, functor):
/** Performs a functor on each of the targets of a functor.
 * The function overload for sigc::bound_mem_functor performs a functor
 * on the object instance stored in the sigc::bound_mem_functor object.
 *
 * @ingroup mem_fun
 */
template <class T_action, class T_return, class T_obj, class T_arg1,class T_arg2,class T_arg3,class T_arg4,class T_arg5>
void visit_each(const T_action& _A_action,
                const bound_mem_functor5<T_return, T_obj, T_arg1,T_arg2,T_arg3,T_arg4,T_arg5>& _A_target)
{
  sigc::visit_each(_A_action, _A_target.obj_);
}


/** bound_mem_functor6 encapsulates a  method with 6 arguments and an object instance.
 * Use the convenience function mem_fun() to create an instance of bound_mem_functor6.
 *
 * The following template arguments are used:
 * - @e T_arg1 Argument type used in the definition of operator()().
 * - @e T_arg2 Argument type used in the definition of operator()().
 * - @e T_arg3 Argument type used in the definition of operator()().
 * - @e T_arg4 Argument type used in the definition of operator()().
 * - @e T_arg5 Argument type used in the definition of operator()().
 * - @e T_arg6 Argument type used in the definition of operator()().
 * - @e T_return The return type of operator()().
 * - @e T_obj The object type.
 *
 * @ingroup mem_fun
 */
template <class T_return, class T_obj, class T_arg1,class T_arg2,class T_arg3,class T_arg4,class T_arg5,class T_arg6>
class bound_mem_functor6
  : public mem_functor6<T_return, T_obj, T_arg1,T_arg2,T_arg3,T_arg4,T_arg5,T_arg6>
{
  typedef mem_functor6<T_return, T_obj, T_arg1,T_arg2,T_arg3,T_arg4,T_arg5,T_arg6> base_type_;
public:
  typedef typename base_type_::function_type function_type;

  /** Constructs a bound_mem_functor6 object that wraps the passed method.
   * @param _A_obj Pointer to instance the method will operate on.
   * @param _A_func Pointer to method will be invoked from operator()().
   */
  bound_mem_functor6( T_obj* _A_obj, function_type _A_func)
    : base_type_(_A_func),
      obj_(*_A_obj)
    {}

  /** Constructs a bound_mem_functor6 object that wraps the passed method.
   * @param _A_obj Reference to instance the method will operate on.
   * @param _A_func Pointer to method will be invoked from operator()().
   */
  bound_mem_functor6( T_obj& _A_obj, function_type _A_func)
    : base_type_(_A_func),
      obj_(_A_obj)
    {}

  /** Execute the wrapped method operating on the stored instance.
   * @param _A_a1 Argument to be passed on to the method.
   * @param _A_a2 Argument to be passed on to the method.
   * @param _A_a3 Argument to be passed on to the method.
   * @param _A_a4 Argument to be passed on to the method.
   * @param _A_a5 Argument to be passed on to the method.
   * @param _A_a6 Argument to be passed on to the method.
   * @return The return value of the method invocation.
   */
  T_return operator()(typename type_trait<T_arg1>::take _A_a1,typename type_trait<T_arg2>::take _A_a2,typename type_trait<T_arg3>::take _A_a3,typename type_trait<T_arg4>::take _A_a4,typename type_trait<T_arg5>::take _A_a5,typename type_trait<T_arg6>::take _A_a6) const
    { return (obj_.invoke().*(this->func_ptr_))(_A_a1,_A_a2,_A_a3,_A_a4,_A_a5,_A_a6); }

//protected:
  // Reference to stored object instance.
  // This is the handler object, such as TheObject in void TheObject::signal_handler().
  limit_reference<T_obj> obj_;
};

//template specialization of visit_each<>(action, functor):
/** Performs a functor on each of the targets of a functor.
 * The function overload for sigc::bound_mem_functor performs a functor
 * on the object instance stored in the sigc::bound_mem_functor object.
 *
 * @ingroup mem_fun
 */
template <class T_action, class T_return, class T_obj, class T_arg1,class T_arg2,class T_arg3,class T_arg4,class T_arg5,class T_arg6>
void visit_each(const T_action& _A_action,
                const bound_mem_functor6<T_return, T_obj, T_arg1,T_arg2,T_arg3,T_arg4,T_arg5,T_arg6>& _A_target)
{
  sigc::visit_each(_A_action, _A_target.obj_);
}


/** bound_mem_functor7 encapsulates a  method with 7 arguments and an object instance.
 * Use the convenience function mem_fun() to create an instance of bound_mem_functor7.
 *
 * The following template arguments are used:
 * - @e T_arg1 Argument type used in the definition of operator()().
 * - @e T_arg2 Argument type used in the definition of operator()().
 * - @e T_arg3 Argument type used in the definition of operator()().
 * - @e T_arg4 Argument type used in the definition of operator()().
 * - @e T_arg5 Argument type used in the definition of operator()().
 * - @e T_arg6 Argument type used in the definition of operator()().
 * - @e T_arg7 Argument type used in the definition of operator()().
 * - @e T_return The return type of operator()().
 * - @e T_obj The object type.
 *
 * @ingroup mem_fun
 */
template <class T_return, class T_obj, class T_arg1,class T_arg2,class T_arg3,class T_arg4,class T_arg5,class T_arg6,class T_arg7>
class bound_mem_functor7
  : public mem_functor7<T_return, T_obj, T_arg1,T_arg2,T_arg3,T_arg4,T_arg5,T_arg6,T_arg7>
{
  typedef mem_functor7<T_return, T_obj, T_arg1,T_arg2,T_arg3,T_arg4,T_arg5,T_arg6,T_arg7> base_type_;
public:
  typedef typename base_type_::function_type function_type;

  /** Constructs a bound_mem_functor7 object that wraps the passed method.
   * @param _A_obj Pointer to instance the method will operate on.
   * @param _A_func Pointer to method will be invoked from operator()().
   */
  bound_mem_functor7( T_obj* _A_obj, function_type _A_func)
    : base_type_(_A_func),
      obj_(*_A_obj)
    {}

  /** Constructs a bound_mem_functor7 object that wraps the passed method.
   * @param _A_obj Reference to instance the method will operate on.
   * @param _A_func Pointer to method will be invoked from operator()().
   */
  bound_mem_functor7( T_obj& _A_obj, function_type _A_func)
    : base_type_(_A_func),
      obj_(_A_obj)
    {}

  /** Execute the wrapped method operating on the stored instance.
   * @param _A_a1 Argument to be passed on to the method.
   * @param _A_a2 Argument to be passed on to the method.
   * @param _A_a3 Argument to be passed on to the method.
   * @param _A_a4 Argument to be passed on to the method.
   * @param _A_a5 Argument to be passed on to the method.
   * @param _A_a6 Argument to be passed on to the method.
   * @param _A_a7 Argument to be passed on to the method.
   * @return The return value of the method invocation.
   */
  T_return operator()(typename type_trait<T_arg1>::take _A_a1,typename type_trait<T_arg2>::take _A_a2,typename type_trait<T_arg3>::take _A_a3,typename type_trait<T_arg4>::take _A_a4,typename type_trait<T_arg5>::take _A_a5,typename type_trait<T_arg6>::take _A_a6,typename type_trait<T_arg7>::take _A_a7) const
    { return (obj_.invoke().*(this->func_ptr_))(_A_a1,_A_a2,_A_a3,_A_a4,_A_a5,_A_a6,_A_a7); }

//protected:
  // Reference to stored object instance.
  // This is the handler object, such as TheObject in void TheObject::signal_handler().
  limit_reference<T_obj> obj_;
};

//template specialization of visit_each<>(action, functor):
/** Performs a functor on each of the targets of a functor.
 * The function overload for sigc::bound_mem_functor performs a functor
 * on the object instance stored in the sigc::bound_mem_functor object.
 *
 * @ingroup mem_fun
 */
template <class T_action, class T_return, class T_obj, class T_arg1,class T_arg2,class T_arg3,class T_arg4,class T_arg5,class T_arg6,class T_arg7>
void visit_each(const T_action& _A_action,
                const bound_mem_functor7<T_return, T_obj, T_arg1,T_arg2,T_arg3,T_arg4,T_arg5,T_arg6,T_arg7>& _A_target)
{
  sigc::visit_each(_A_action, _A_target.obj_);
}


/** bound_const_mem_functor0 encapsulates a const method with 0 arguments and an object instance.
 * Use the convenience function mem_fun() to create an instance of bound_const_mem_functor0.
 *
 * The following template arguments are used:
 * - @e T_return The return type of operator()().
 * - @e T_obj The object type.
 *
 * @ingroup mem_fun
 */
template <class T_return, class T_obj>
class bound_const_mem_functor0
  : public const_mem_functor0<T_return, T_obj>
{
  typedef const_mem_functor0<T_return, T_obj> base_type_;
public:
  typedef typename base_type_::function_type function_type;

  /** Constructs a bound_const_mem_functor0 object that wraps the passed method.
   * @param _A_obj Pointer to instance the method will operate on.
   * @param _A_func Pointer to method will be invoked from operator()().
   */
  bound_const_mem_functor0(const T_obj* _A_obj, function_type _A_func)
    : base_type_(_A_func),
      obj_(*_A_obj)
    {}

  /** Constructs a bound_const_mem_functor0 object that wraps the passed method.
   * @param _A_obj Reference to instance the method will operate on.
   * @param _A_func Pointer to method will be invoked from operator()().
   */
  bound_const_mem_functor0(const T_obj& _A_obj, function_type _A_func)
    : base_type_(_A_func),
      obj_(_A_obj)
    {}

  /** Execute the wrapped method operating on the stored instance.
   * @return The return value of the method invocation.
   */
  T_return operator()() const
    { return (obj_.invoke().*(this->func_ptr_))(); }

//protected:
  // Reference to stored object instance.
  // This is the handler object, such as TheObject in void TheObject::signal_handler().
  const_limit_reference<T_obj> obj_;
};

//template specialization of visit_each<>(action, functor):
/** Performs a functor on each of the targets of a functor.
 * The function overload for sigc::bound_const_mem_functor performs a functor
 * on the object instance stored in the sigc::bound_const_mem_functor object.
 *
 * @ingroup mem_fun
 */
template <class T_action, class T_return, class T_obj>
void visit_each(const T_action& _A_action,
                const bound_const_mem_functor0<T_return, T_obj>& _A_target)
{
  sigc::visit_each(_A_action, _A_target.obj_);
}


/** bound_const_mem_functor1 encapsulates a const method with 1 arguments and an object instance.
 * Use the convenience function mem_fun() to create an instance of bound_const_mem_functor1.
 *
 * The following template arguments are used:
 * - @e T_arg1 Argument type used in the definition of operator()().
 * - @e T_return The return type of operator()().
 * - @e T_obj The object type.
 *
 * @ingroup mem_fun
 */
template <class T_return, class T_obj, class T_arg1>
class bound_const_mem_functor1
  : public const_mem_functor1<T_return, T_obj, T_arg1>
{
  typedef const_mem_functor1<T_return, T_obj, T_arg1> base_type_;
public:
  typedef typename base_type_::function_type function_type;

  /** Constructs a bound_const_mem_functor1 object that wraps the passed method.
   * @param _A_obj Pointer to instance the method will operate on.
   * @param _A_func Pointer to method will be invoked from operator()().
   */
  bound_const_mem_functor1(const T_obj* _A_obj, function_type _A_func)
    : base_type_(_A_func),
      obj_(*_A_obj)
    {}

  /** Constructs a bound_const_mem_functor1 object that wraps the passed method.
   * @param _A_obj Reference to instance the method will operate on.
   * @param _A_func Pointer to method will be invoked from operator()().
   */
  bound_const_mem_functor1(const T_obj& _A_obj, function_type _A_func)
    : base_type_(_A_func),
      obj_(_A_obj)
    {}

  /** Execute the wrapped method operating on the stored instance.
   * @param _A_a1 Argument to be passed on to the method.
   * @return The return value of the method invocation.
   */
  T_return operator()(typename type_trait<T_arg1>::take _A_a1) const
    { return (obj_.invoke().*(this->func_ptr_))(_A_a1); }

//protected:
  // Reference to stored object instance.
  // This is the handler object, such as TheObject in void TheObject::signal_handler().
  const_limit_reference<T_obj> obj_;
};

//template specialization of visit_each<>(action, functor):
/** Performs a functor on each of the targets of a functor.
 * The function overload for sigc::bound_const_mem_functor performs a functor
 * on the object instance stored in the sigc::bound_const_mem_functor object.
 *
 * @ingroup mem_fun
 */
template <class T_action, class T_return, class T_obj, class T_arg1>
void visit_each(const T_action& _A_action,
                const bound_const_mem_functor1<T_return, T_obj, T_arg1>& _A_target)
{
  sigc::visit_each(_A_action, _A_target.obj_);
}


/** bound_const_mem_functor2 encapsulates a const method with 2 arguments and an object instance.
 * Use the convenience function mem_fun() to create an instance of bound_const_mem_functor2.
 *
 * The following template arguments are used:
 * - @e T_arg1 Argument type used in the definition of operator()().
 * - @e T_arg2 Argument type used in the definition of operator()().
 * - @e T_return The return type of operator()().
 * - @e T_obj The object type.
 *
 * @ingroup mem_fun
 */
template <class T_return, class T_obj, class T_arg1,class T_arg2>
class bound_const_mem_functor2
  : public const_mem_functor2<T_return, T_obj, T_arg1,T_arg2>
{
  typedef const_mem_functor2<T_return, T_obj, T_arg1,T_arg2> base_type_;
public:
  typedef typename base_type_::function_type function_type;

  /** Constructs a bound_const_mem_functor2 object that wraps the passed method.
   * @param _A_obj Pointer to instance the method will operate on.
   * @param _A_func Pointer to method will be invoked from operator()().
   */
  bound_const_mem_functor2(const T_obj* _A_obj, function_type _A_func)
    : base_type_(_A_func),
      obj_(*_A_obj)
    {}

  /** Constructs a bound_const_mem_functor2 object that wraps the passed method.
   * @param _A_obj Reference to instance the method will operate on.
   * @param _A_func Pointer to method will be invoked from operator()().
   */
  bound_const_mem_functor2(const T_obj& _A_obj, function_type _A_func)
    : base_type_(_A_func),
      obj_(_A_obj)
    {}

  /** Execute the wrapped method operating on the stored instance.
   * @param _A_a1 Argument to be passed on to the method.
   * @param _A_a2 Argument to be passed on to the method.
   * @return The return value of the method invocation.
   */
  T_return operator()(typename type_trait<T_arg1>::take _A_a1,typename type_trait<T_arg2>::take _A_a2) const
    { return (obj_.invoke().*(this->func_ptr_))(_A_a1,_A_a2); }

//protected:
  // Reference to stored object instance.
  // This is the handler object, such as TheObject in void TheObject::signal_handler().
  const_limit_reference<T_obj> obj_;
};

//template specialization of visit_each<>(action, functor):
/** Performs a functor on each of the targets of a functor.
 * The function overload for sigc::bound_const_mem_functor performs a functor
 * on the object instance stored in the sigc::bound_const_mem_functor object.
 *
 * @ingroup mem_fun
 */
template <class T_action, class T_return, class T_obj, class T_arg1,class T_arg2>
void visit_each(const T_action& _A_action,
                const bound_const_mem_functor2<T_return, T_obj, T_arg1,T_arg2>& _A_target)
{
  sigc::visit_each(_A_action, _A_target.obj_);
}


/** bound_const_mem_functor3 encapsulates a const method with 3 arguments and an object instance.
 * Use the convenience function mem_fun() to create an instance of bound_const_mem_functor3.
 *
 * The following template arguments are used:
 * - @e T_arg1 Argument type used in the definition of operator()().
 * - @e T_arg2 Argument type used in the definition of operator()().
 * - @e T_arg3 Argument type used in the definition of operator()().
 * - @e T_return The return type of operator()().
 * - @e T_obj The object type.
 *
 * @ingroup mem_fun
 */
template <class T_return, class T_obj, class T_arg1,class T_arg2,class T_arg3>
class bound_const_mem_functor3
  : public const_mem_functor3<T_return, T_obj, T_arg1,T_arg2,T_arg3>
{
  typedef const_mem_functor3<T_return, T_obj, T_arg1,T_arg2,T_arg3> base_type_;
public:
  typedef typename base_type_::function_type function_type;

  /** Constructs a bound_const_mem_functor3 object that wraps the passed method.
   * @param _A_obj Pointer to instance the method will operate on.
   * @param _A_func Pointer to method will be invoked from operator()().
   */
  bound_const_mem_functor3(const T_obj* _A_obj, function_type _A_func)
    : base_type_(_A_func),
      obj_(*_A_obj)
    {}

  /** Constructs a bound_const_mem_functor3 object that wraps the passed method.
   * @param _A_obj Reference to instance the method will operate on.
   * @param _A_func Pointer to method will be invoked from operator()().
   */
  bound_const_mem_functor3(const T_obj& _A_obj, function_type _A_func)
    : base_type_(_A_func),
      obj_(_A_obj)
    {}

  /** Execute the wrapped method operating on the stored instance.
   * @param _A_a1 Argument to be passed on to the method.
   * @param _A_a2 Argument to be passed on to the method.
   * @param _A_a3 Argument to be passed on to the method.
   * @return The return value of the method invocation.
   */
  T_return operator()(typename type_trait<T_arg1>::take _A_a1,typename type_trait<T_arg2>::take _A_a2,typename type_trait<T_arg3>::take _A_a3) const
    { return (obj_.invoke().*(this->func_ptr_))(_A_a1,_A_a2,_A_a3); }

//protected:
  // Reference to stored object instance.
  // This is the handler object, such as TheObject in void TheObject::signal_handler().
  const_limit_reference<T_obj> obj_;
};

//template specialization of visit_each<>(action, functor):
/** Performs a functor on each of the targets of a functor.
 * The function overload for sigc::bound_const_mem_functor performs a functor
 * on the object instance stored in the sigc::bound_const_mem_functor object.
 *
 * @ingroup mem_fun
 */
template <class T_action, class T_return, class T_obj, class T_arg1,class T_arg2,class T_arg3>
void visit_each(const T_action& _A_action,
                const bound_const_mem_functor3<T_return, T_obj, T_arg1,T_arg2,T_arg3>& _A_target)
{
  sigc::visit_each(_A_action, _A_target.obj_);
}


/** bound_const_mem_functor4 encapsulates a const method with 4 arguments and an object instance.
 * Use the convenience function mem_fun() to create an instance of bound_const_mem_functor4.
 *
 * The following template arguments are used:
 * - @e T_arg1 Argument type used in the definition of operator()().
 * - @e T_arg2 Argument type used in the definition of operator()().
 * - @e T_arg3 Argument type used in the definition of operator()().
 * - @e T_arg4 Argument type used in the definition of operator()().
 * - @e T_return The return type of operator()().
 * - @e T_obj The object type.
 *
 * @ingroup mem_fun
 */
template <class T_return, class T_obj, class T_arg1,class T_arg2,class T_arg3,class T_arg4>
class bound_const_mem_functor4
  : public const_mem_functor4<T_return, T_obj, T_arg1,T_arg2,T_arg3,T_arg4>
{
  typedef const_mem_functor4<T_return, T_obj, T_arg1,T_arg2,T_arg3,T_arg4> base_type_;
public:
  typedef typename base_type_::function_type function_type;

  /** Constructs a bound_const_mem_functor4 object that wraps the passed method.
   * @param _A_obj Pointer to instance the method will operate on.
   * @param _A_func Pointer to method will be invoked from operator()().
   */
  bound_const_mem_functor4(const T_obj* _A_obj, function_type _A_func)
    : base_type_(_A_func),
      obj_(*_A_obj)
    {}

  /** Constructs a bound_const_mem_functor4 object that wraps the passed method.
   * @param _A_obj Reference to instance the method will operate on.
   * @param _A_func Pointer to method will be invoked from operator()().
   */
  bound_const_mem_functor4(const T_obj& _A_obj, function_type _A_func)
    : base_type_(_A_func),
      obj_(_A_obj)
    {}

  /** Execute the wrapped method operating on the stored instance.
   * @param _A_a1 Argument to be passed on to the method.
   * @param _A_a2 Argument to be passed on to the method.
   * @param _A_a3 Argument to be passed on to the method.
   * @param _A_a4 Argument to be passed on to the method.
   * @return The return value of the method invocation.
   */
  T_return operator()(typename type_trait<T_arg1>::take _A_a1,typename type_trait<T_arg2>::take _A_a2,typename type_trait<T_arg3>::take _A_a3,typename type_trait<T_arg4>::take _A_a4) const
    { return (obj_.invoke().*(this->func_ptr_))(_A_a1,_A_a2,_A_a3,_A_a4); }

//protected:
  // Reference to stored object instance.
  // This is the handler object, such as TheObject in void TheObject::signal_handler().
  const_limit_reference<T_obj> obj_;
};

//template specialization of visit_each<>(action, functor):
/** Performs a functor on each of the targets of a functor.
 * The function overload for sigc::bound_const_mem_functor performs a functor
 * on the object instance stored in the sigc::bound_const_mem_functor object.
 *
 * @ingroup mem_fun
 */
template <class T_action, class T_return, class T_obj, class T_arg1,class T_arg2,class T_arg3,class T_arg4>
void visit_each(const T_action& _A_action,
                const bound_const_mem_functor4<T_return, T_obj, T_arg1,T_arg2,T_arg3,T_arg4>& _A_target)
{
  sigc::visit_each(_A_action, _A_target.obj_);
}


/** bound_const_mem_functor5 encapsulates a const method with 5 arguments and an object instance.
 * Use the convenience function mem_fun() to create an instance of bound_const_mem_functor5.
 *
 * The following template arguments are used:
 * - @e T_arg1 Argument type used in the definition of operator()().
 * - @e T_arg2 Argument type used in the definition of operator()().
 * - @e T_arg3 Argument type used in the definition of operator()().
 * - @e T_arg4 Argument type used in the definition of operator()().
 * - @e T_arg5 Argument type used in the definition of operator()().
 * - @e T_return The return type of operator()().
 * - @e T_obj The object type.
 *
 * @ingroup mem_fun
 */
template <class T_return, class T_obj, class T_arg1,class T_arg2,class T_arg3,class T_arg4,class T_arg5>
class bound_const_mem_functor5
  : public const_mem_functor5<T_return, T_obj, T_arg1,T_arg2,T_arg3,T_arg4,T_arg5>
{
  typedef const_mem_functor5<T_return, T_obj, T_arg1,T_arg2,T_arg3,T_arg4,T_arg5> base_type_;
public:
  typedef typename base_type_::function_type function_type;

  /** Constructs a bound_const_mem_functor5 object that wraps the passed method.
   * @param _A_obj Pointer to instance the method will operate on.
   * @param _A_func Pointer to method will be invoked from operator()().
   */
  bound_const_mem_functor5(const T_obj* _A_obj, function_type _A_func)
    : base_type_(_A_func),
      obj_(*_A_obj)
    {}

  /** Constructs a bound_const_mem_functor5 object that wraps the passed method.
   * @param _A_obj Reference to instance the method will operate on.
   * @param _A_func Pointer to method will be invoked from operator()().
   */
  bound_const_mem_functor5(const T_obj& _A_obj, function_type _A_func)
    : base_type_(_A_func),
      obj_(_A_obj)
    {}

  /** Execute the wrapped method operating on the stored instance.
   * @param _A_a1 Argument to be passed on to the method.
   * @param _A_a2 Argument to be passed on to the method.
   * @param _A_a3 Argument to be passed on to the method.
   * @param _A_a4 Argument to be passed on to the method.
   * @param _A_a5 Argument to be passed on to the method.
   * @return The return value of the method invocation.
   */
  T_return operator()(typename type_trait<T_arg1>::take _A_a1,typename type_trait<T_arg2>::take _A_a2,typename type_trait<T_arg3>::take _A_a3,typename type_trait<T_arg4>::take _A_a4,typename type_trait<T_arg5>::take _A_a5) const
    { return (obj_.invoke().*(this->func_ptr_))(_A_a1,_A_a2,_A_a3,_A_a4,_A_a5); }

//protected:
  // Reference to stored object instance.
  // This is the handler object, such as TheObject in void TheObject::signal_handler().
  const_limit_reference<T_obj> obj_;
};

//template specialization of visit_each<>(action, functor):
/** Performs a functor on each of the targets of a functor.
 * The function overload for sigc::bound_const_mem_functor performs a functor
 * on the object instance stored in the sigc::bound_const_mem_functor object.
 *
 * @ingroup mem_fun
 */
template <class T_action, class T_return, class T_obj, class T_arg1,class T_arg2,class T_arg3,class T_arg4,class T_arg5>
void visit_each(const T_action& _A_action,
                const bound_const_mem_functor5<T_return, T_obj, T_arg1,T_arg2,T_arg3,T_arg4,T_arg5>& _A_target)
{
  sigc::visit_each(_A_action, _A_target.obj_);
}


/** bound_const_mem_functor6 encapsulates a const method with 6 arguments and an object instance.
 * Use the convenience function mem_fun() to create an instance of bound_const_mem_functor6.
 *
 * The following template arguments are used:
 * - @e T_arg1 Argument type used in the definition of operator()().
 * - @e T_arg2 Argument type used in the definition of operator()().
 * - @e T_arg3 Argument type used in the definition of operator()().
 * - @e T_arg4 Argument type used in the definition of operator()().
 * - @e T_arg5 Argument type used in the definition of operator()().
 * - @e T_arg6 Argument type used in the definition of operator()().
 * - @e T_return The return type of operator()().
 * - @e T_obj The object type.
 *
 * @ingroup mem_fun
 */
template <class T_return, class T_obj, class T_arg1,class T_arg2,class T_arg3,class T_arg4,class T_arg5,class T_arg6>
class bound_const_mem_functor6
  : public const_mem_functor6<T_return, T_obj, T_arg1,T_arg2,T_arg3,T_arg4,T_arg5,T_arg6>
{
  typedef const_mem_functor6<T_return, T_obj, T_arg1,T_arg2,T_arg3,T_arg4,T_arg5,T_arg6> base_type_;
public:
  typedef typename base_type_::function_type function_type;

  /** Constructs a bound_const_mem_functor6 object that wraps the passed method.
   * @param _A_obj Pointer to instance the method will operate on.
   * @param _A_func Pointer to method will be invoked from operator()().
   */
  bound_const_mem_functor6(const T_obj* _A_obj, function_type _A_func)
    : base_type_(_A_func),
      obj_(*_A_obj)
    {}

  /** Constructs a bound_const_mem_functor6 object that wraps the passed method.
   * @param _A_obj Reference to instance the method will operate on.
   * @param _A_func Pointer to method will be invoked from operator()().
   */
  bound_const_mem_functor6(const T_obj& _A_obj, function_type _A_func)
    : base_type_(_A_func),
      obj_(_A_obj)
    {}

  /** Execute the wrapped method operating on the stored instance.
   * @param _A_a1 Argument to be passed on to the method.
   * @param _A_a2 Argument to be passed on to the method.
   * @param _A_a3 Argument to be passed on to the method.
   * @param _A_a4 Argument to be passed on to the method.
   * @param _A_a5 Argument to be passed on to the method.
   * @param _A_a6 Argument to be passed on to the method.
   * @return The return value of the method invocation.
   */
  T_return operator()(typename type_trait<T_arg1>::take _A_a1,typename type_trait<T_arg2>::take _A_a2,typename type_trait<T_arg3>::take _A_a3,typename type_trait<T_arg4>::take _A_a4,typename type_trait<T_arg5>::take _A_a5,typename type_trait<T_arg6>::take _A_a6) const
    { return (obj_.invoke().*(this->func_ptr_))(_A_a1,_A_a2,_A_a3,_A_a4,_A_a5,_A_a6); }

//protected:
  // Reference to stored object instance.
  // This is the handler object, such as TheObject in void TheObject::signal_handler().
  const_limit_reference<T_obj> obj_;
};

//template specialization of visit_each<>(action, functor):
/** Performs a functor on each of the targets of a functor.
 * The function overload for sigc::bound_const_mem_functor performs a functor
 * on the object instance stored in the sigc::bound_const_mem_functor object.
 *
 * @ingroup mem_fun
 */
template <class T_action, class T_return, class T_obj, class T_arg1,class T_arg2,class T_arg3,class T_arg4,class T_arg5,class T_arg6>
void visit_each(const T_action& _A_action,
                const bound_const_mem_functor6<T_return, T_obj, T_arg1,T_arg2,T_arg3,T_arg4,T_arg5,T_arg6>& _A_target)
{
  sigc::visit_each(_A_action, _A_target.obj_);
}


/** bound_const_mem_functor7 encapsulates a const method with 7 arguments and an object instance.
 * Use the convenience function mem_fun() to create an instance of bound_const_mem_functor7.
 *
 * The following template arguments are used:
 * - @e T_arg1 Argument type used in the definition of operator()().
 * - @e T_arg2 Argument type used in the definition of operator()().
 * - @e T_arg3 Argument type used in the definition of operator()().
 * - @e T_arg4 Argument type used in the definition of operator()().
 * - @e T_arg5 Argument type used in the definition of operator()().
 * - @e T_arg6 Argument type used in the definition of operator()().
 * - @e T_arg7 Argument type used in the definition of operator()().
 * - @e T_return The return type of operator()().
 * - @e T_obj The object type.
 *
 * @ingroup mem_fun
 */
template <class T_return, class T_obj, class T_arg1,class T_arg2,class T_arg3,class T_arg4,class T_arg5,class T_arg6,class T_arg7>
class bound_const_mem_functor7
  : public const_mem_functor7<T_return, T_obj, T_arg1,T_arg2,T_arg3,T_arg4,T_arg5,T_arg6,T_arg7>
{
  typedef const_mem_functor7<T_return, T_obj, T_arg1,T_arg2,T_arg3,T_arg4,T_arg5,T_arg6,T_arg7> base_type_;
public:
  typedef typename base_type_::function_type function_type;

  /** Constructs a bound_const_mem_functor7 object that wraps the passed method.
   * @param _A_obj Pointer to instance the method will operate on.
   * @param _A_func Pointer to method will be invoked from operator()().
   */
  bound_const_mem_functor7(const T_obj* _A_obj, function_type _A_func)
    : base_type_(_A_func),
      obj_(*_A_obj)
    {}

  /** Constructs a bound_const_mem_functor7 object that wraps the passed method.
   * @param _A_obj Reference to instance the method will operate on.
   * @param _A_func Pointer to method will be invoked from operator()().
   */
  bound_const_mem_functor7(const T_obj& _A_obj, function_type _A_func)
    : base_type_(_A_func),
      obj_(_A_obj)
    {}

  /** Execute the wrapped method operating on the stored instance.
   * @param _A_a1 Argument to be passed on to the method.
   * @param _A_a2 Argument to be passed on to the method.
   * @param _A_a3 Argument to be passed on to the method.
   * @param _A_a4 Argument to be passed on to the method.
   * @param _A_a5 Argument to be passed on to the method.
   * @param _A_a6 Argument to be passed on to the method.
   * @param _A_a7 Argument to be passed on to the method.
   * @return The return value of the method invocation.
   */
  T_return operator()(typename type_trait<T_arg1>::take _A_a1,typename type_trait<T_arg2>::take _A_a2,typename type_trait<T_arg3>::take _A_a3,typename type_trait<T_arg4>::take _A_a4,typename type_trait<T_arg5>::take _A_a5,typename type_trait<T_arg6>::take _A_a6,typename type_trait<T_arg7>::take _A_a7) const
    { return (obj_.invoke().*(this->func_ptr_))(_A_a1,_A_a2,_A_a3,_A_a4,_A_a5,_A_a6,_A_a7); }

//protected:
  // Reference to stored object instance.
  // This is the handler object, such as TheObject in void TheObject::signal_handler().
  const_limit_reference<T_obj> obj_;
};

//template specialization of visit_each<>(action, functor):
/** Performs a functor on each of the targets of a functor.
 * The function overload for sigc::bound_const_mem_functor performs a functor
 * on the object instance stored in the sigc::bound_const_mem_functor object.
 *
 * @ingroup mem_fun
 */
template <class T_action, class T_return, class T_obj, class T_arg1,class T_arg2,class T_arg3,class T_arg4,class T_arg5,class T_arg6,class T_arg7>
void visit_each(const T_action& _A_action,
                const bound_const_mem_functor7<T_return, T_obj, T_arg1,T_arg2,T_arg3,T_arg4,T_arg5,T_arg6,T_arg7>& _A_target)
{
  sigc::visit_each(_A_action, _A_target.obj_);
}


/** bound_volatile_mem_functor0 encapsulates a volatile method with 0 arguments and an object instance.
 * Use the convenience function mem_fun() to create an instance of bound_volatile_mem_functor0.
 *
 * The following template arguments are used:
 * - @e T_return The return type of operator()().
 * - @e T_obj The object type.
 *
 * @ingroup mem_fun
 */
template <class T_return, class T_obj>
class bound_volatile_mem_functor0
  : public volatile_mem_functor0<T_return, T_obj>
{
  typedef volatile_mem_functor0<T_return, T_obj> base_type_;
public:
  typedef typename base_type_::function_type function_type;

  /** Constructs a bound_volatile_mem_functor0 object that wraps the passed method.
   * @param _A_obj Pointer to instance the method will operate on.
   * @param _A_func Pointer to method will be invoked from operator()().
   */
  bound_volatile_mem_functor0( T_obj* _A_obj, function_type _A_func)
    : base_type_(_A_func),
      obj_(*_A_obj)
    {}

  /** Constructs a bound_volatile_mem_functor0 object that wraps the passed method.
   * @param _A_obj Reference to instance the method will operate on.
   * @param _A_func Pointer to method will be invoked from operator()().
   */
  bound_volatile_mem_functor0( T_obj& _A_obj, function_type _A_func)
    : base_type_(_A_func),
      obj_(_A_obj)
    {}

  /** Execute the wrapped method operating on the stored instance.
   * @return The return value of the method invocation.
   */
  T_return operator()() const
    { return (obj_.invoke().*(this->func_ptr_))(); }

//protected:
  // Reference to stored object instance.
  // This is the handler object, such as TheObject in void TheObject::signal_handler().
  volatile_limit_reference<T_obj> obj_;
};

//template specialization of visit_each<>(action, functor):
/** Performs a functor on each of the targets of a functor.
 * The function overload for sigc::bound_volatile_mem_functor performs a functor
 * on the object instance stored in the sigc::bound_volatile_mem_functor object.
 *
 * @ingroup mem_fun
 */
template <class T_action, class T_return, class T_obj>
void visit_each(const T_action& _A_action,
                const bound_volatile_mem_functor0<T_return, T_obj>& _A_target)
{
  sigc::visit_each(_A_action, _A_target.obj_);
}


/** bound_volatile_mem_functor1 encapsulates a volatile method with 1 arguments and an object instance.
 * Use the convenience function mem_fun() to create an instance of bound_volatile_mem_functor1.
 *
 * The following template arguments are used:
 * - @e T_arg1 Argument type used in the definition of operator()().
 * - @e T_return The return type of operator()().
 * - @e T_obj The object type.
 *
 * @ingroup mem_fun
 */
template <class T_return, class T_obj, class T_arg1>
class bound_volatile_mem_functor1
  : public volatile_mem_functor1<T_return, T_obj, T_arg1>
{
  typedef volatile_mem_functor1<T_return, T_obj, T_arg1> base_type_;
public:
  typedef typename base_type_::function_type function_type;

  /** Constructs a bound_volatile_mem_functor1 object that wraps the passed method.
   * @param _A_obj Pointer to instance the method will operate on.
   * @param _A_func Pointer to method will be invoked from operator()().
   */
  bound_volatile_mem_functor1( T_obj* _A_obj, function_type _A_func)
    : base_type_(_A_func),
      obj_(*_A_obj)
    {}

  /** Constructs a bound_volatile_mem_functor1 object that wraps the passed method.
   * @param _A_obj Reference to instance the method will operate on.
   * @param _A_func Pointer to method will be invoked from operator()().
   */
  bound_volatile_mem_functor1( T_obj& _A_obj, function_type _A_func)
    : base_type_(_A_func),
      obj_(_A_obj)
    {}

  /** Execute the wrapped method operating on the stored instance.
   * @param _A_a1 Argument to be passed on to the method.
   * @return The return value of the method invocation.
   */
  T_return operator()(typename type_trait<T_arg1>::take _A_a1) const
    { return (obj_.invoke().*(this->func_ptr_))(_A_a1); }

//protected:
  // Reference to stored object instance.
  // This is the handler object, such as TheObject in void TheObject::signal_handler().
  volatile_limit_reference<T_obj> obj_;
};

//template specialization of visit_each<>(action, functor):
/** Performs a functor on each of the targets of a functor.
 * The function overload for sigc::bound_volatile_mem_functor performs a functor
 * on the object instance stored in the sigc::bound_volatile_mem_functor object.
 *
 * @ingroup mem_fun
 */
template <class T_action, class T_return, class T_obj, class T_arg1>
void visit_each(const T_action& _A_action,
                const bound_volatile_mem_functor1<T_return, T_obj, T_arg1>& _A_target)
{
  sigc::visit_each(_A_action, _A_target.obj_);
}


/** bound_volatile_mem_functor2 encapsulates a volatile method with 2 arguments and an object instance.
 * Use the convenience function mem_fun() to create an instance of bound_volatile_mem_functor2.
 *
 * The following template arguments are used:
 * - @e T_arg1 Argument type used in the definition of operator()().
 * - @e T_arg2 Argument type used in the definition of operator()().
 * - @e T_return The return type of operator()().
 * - @e T_obj The object type.
 *
 * @ingroup mem_fun
 */
template <class T_return, class T_obj, class T_arg1,class T_arg2>
class bound_volatile_mem_functor2
  : public volatile_mem_functor2<T_return, T_obj, T_arg1,T_arg2>
{
  typedef volatile_mem_functor2<T_return, T_obj, T_arg1,T_arg2> base_type_;
public:
  typedef typename base_type_::function_type function_type;

  /** Constructs a bound_volatile_mem_functor2 object that wraps the passed method.
   * @param _A_obj Pointer to instance the method will operate on.
   * @param _A_func Pointer to method will be invoked from operator()().
   */
  bound_volatile_mem_functor2( T_obj* _A_obj, function_type _A_func)
    : base_type_(_A_func),
      obj_(*_A_obj)
    {}

  /** Constructs a bound_volatile_mem_functor2 object that wraps the passed method.
   * @param _A_obj Reference to instance the method will operate on.
   * @param _A_func Pointer to method will be invoked from operator()().
   */
  bound_volatile_mem_functor2( T_obj& _A_obj, function_type _A_func)
    : base_type_(_A_func),
      obj_(_A_obj)
    {}

  /** Execute the wrapped method operating on the stored instance.
   * @param _A_a1 Argument to be passed on to the method.
   * @param _A_a2 Argument to be passed on to the method.
   * @return The return value of the method invocation.
   */
  T_return operator()(typename type_trait<T_arg1>::take _A_a1,typename type_trait<T_arg2>::take _A_a2) const
    { return (obj_.invoke().*(this->func_ptr_))(_A_a1,_A_a2); }

//protected:
  // Reference to stored object instance.
  // This is the handler object, such as TheObject in void TheObject::signal_handler().
  volatile_limit_reference<T_obj> obj_;
};

//template specialization of visit_each<>(action, functor):
/** Performs a functor on each of the targets of a functor.
 * The function overload for sigc::bound_volatile_mem_functor performs a functor
 * on the object instance stored in the sigc::bound_volatile_mem_functor object.
 *
 * @ingroup mem_fun
 */
template <class T_action, class T_return, class T_obj, class T_arg1,class T_arg2>
void visit_each(const T_action& _A_action,
                const bound_volatile_mem_functor2<T_return, T_obj, T_arg1,T_arg2>& _A_target)
{
  sigc::visit_each(_A_action, _A_target.obj_);
}


/** bound_volatile_mem_functor3 encapsulates a volatile method with 3 arguments and an object instance.
 * Use the convenience function mem_fun() to create an instance of bound_volatile_mem_functor3.
 *
 * The following template arguments are used:
 * - @e T_arg1 Argument type used in the definition of operator()().
 * - @e T_arg2 Argument type used in the definition of operator()().
 * - @e T_arg3 Argument type used in the definition of operator()().
 * - @e T_return The return type of operator()().
 * - @e T_obj The object type.
 *
 * @ingroup mem_fun
 */
template <class T_return, class T_obj, class T_arg1,class T_arg2,class T_arg3>
class bound_volatile_mem_functor3
  : public volatile_mem_functor3<T_return, T_obj, T_arg1,T_arg2,T_arg3>
{
  typedef volatile_mem_functor3<T_return, T_obj, T_arg1,T_arg2,T_arg3> base_type_;
public:
  typedef typename base_type_::function_type function_type;

  /** Constructs a bound_volatile_mem_functor3 object that wraps the passed method.
   * @param _A_obj Pointer to instance the method will operate on.
   * @param _A_func Pointer to method will be invoked from operator()().
   */
  bound_volatile_mem_functor3( T_obj* _A_obj, function_type _A_func)
    : base_type_(_A_func),
      obj_(*_A_obj)
    {}

  /** Constructs a bound_volatile_mem_functor3 object that wraps the passed method.
   * @param _A_obj Reference to instance the method will operate on.
   * @param _A_func Pointer to method will be invoked from operator()().
   */
  bound_volatile_mem_functor3( T_obj& _A_obj, function_type _A_func)
    : base_type_(_A_func),
      obj_(_A_obj)
    {}

  /** Execute the wrapped method operating on the stored instance.
   * @param _A_a1 Argument to be passed on to the method.
   * @param _A_a2 Argument to be passed on to the method.
   * @param _A_a3 Argument to be passed on to the method.
   * @return The return value of the method invocation.
   */
  T_return operator()(typename type_trait<T_arg1>::take _A_a1,typename type_trait<T_arg2>::take _A_a2,typename type_trait<T_arg3>::take _A_a3) const
    { return (obj_.invoke().*(this->func_ptr_))(_A_a1,_A_a2,_A_a3); }

//protected:
  // Reference to stored object instance.
  // This is the handler object, such as TheObject in void TheObject::signal_handler().
  volatile_limit_reference<T_obj> obj_;
};

//template specialization of visit_each<>(action, functor):
/** Performs a functor on each of the targets of a functor.
 * The function overload for sigc::bound_volatile_mem_functor performs a functor
 * on the object instance stored in the sigc::bound_volatile_mem_functor object.
 *
 * @ingroup mem_fun
 */
template <class T_action, class T_return, class T_obj, class T_arg1,class T_arg2,class T_arg3>
void visit_each(const T_action& _A_action,
                const bound_volatile_mem_functor3<T_return, T_obj, T_arg1,T_arg2,T_arg3>& _A_target)
{
  sigc::visit_each(_A_action, _A_target.obj_);
}


/** bound_volatile_mem_functor4 encapsulates a volatile method with 4 arguments and an object instance.
 * Use the convenience function mem_fun() to create an instance of bound_volatile_mem_functor4.
 *
 * The following template arguments are used:
 * - @e T_arg1 Argument type used in the definition of operator()().
 * - @e T_arg2 Argument type used in the definition of operator()().
 * - @e T_arg3 Argument type used in the definition of operator()().
 * - @e T_arg4 Argument type used in the definition of operator()().
 * - @e T_return The return type of operator()().
 * - @e T_obj The object type.
 *
 * @ingroup mem_fun
 */
template <class T_return, class T_obj, class T_arg1,class T_arg2,class T_arg3,class T_arg4>
class bound_volatile_mem_functor4
  : public volatile_mem_functor4<T_return, T_obj, T_arg1,T_arg2,T_arg3,T_arg4>
{
  typedef volatile_mem_functor4<T_return, T_obj, T_arg1,T_arg2,T_arg3,T_arg4> base_type_;
public:
  typedef typename base_type_::function_type function_type;

  /** Constructs a bound_volatile_mem_functor4 object that wraps the passed method.
   * @param _A_obj Pointer to instance the method will operate on.
   * @param _A_func Pointer to method will be invoked from operator()().
   */
  bound_volatile_mem_functor4( T_obj* _A_obj, function_type _A_func)
    : base_type_(_A_func),
      obj_(*_A_obj)
    {}

  /** Constructs a bound_volatile_mem_functor4 object that wraps the passed method.
   * @param _A_obj Reference to instance the method will operate on.
   * @param _A_func Pointer to method will be invoked from operator()().
   */
  bound_volatile_mem_functor4( T_obj& _A_obj, function_type _A_func)
    : base_type_(_A_func),
      obj_(_A_obj)
    {}

  /** Execute the wrapped method operating on the stored instance.
   * @param _A_a1 Argument to be passed on to the method.
   * @param _A_a2 Argument to be passed on to the method.
   * @param _A_a3 Argument to be passed on to the method.
   * @param _A_a4 Argument to be passed on to the method.
   * @return The return value of the method invocation.
   */
  T_return operator()(typename type_trait<T_arg1>::take _A_a1,typename type_trait<T_arg2>::take _A_a2,typename type_trait<T_arg3>::take _A_a3,typename type_trait<T_arg4>::take _A_a4) const
    { return (obj_.invoke().*(this->func_ptr_))(_A_a1,_A_a2,_A_a3,_A_a4); }

//protected:
  // Reference to stored object instance.
  // This is the handler object, such as TheObject in void TheObject::signal_handler().
  volatile_limit_reference<T_obj> obj_;
};

//template specialization of visit_each<>(action, functor):
/** Performs a functor on each of the targets of a functor.
 * The function overload for sigc::bound_volatile_mem_functor performs a functor
 * on the object instance stored in the sigc::bound_volatile_mem_functor object.
 *
 * @ingroup mem_fun
 */
template <class T_action, class T_return, class T_obj, class T_arg1,class T_arg2,class T_arg3,class T_arg4>
void visit_each(const T_action& _A_action,
                const bound_volatile_mem_functor4<T_return, T_obj, T_arg1,T_arg2,T_arg3,T_arg4>& _A_target)
{
  sigc::visit_each(_A_action, _A_target.obj_);
}


/** bound_volatile_mem_functor5 encapsulates a volatile method with 5 arguments and an object instance.
 * Use the convenience function mem_fun() to create an instance of bound_volatile_mem_functor5.
 *
 * The following template arguments are used:
 * - @e T_arg1 Argument type used in the definition of operator()().
 * - @e T_arg2 Argument type used in the definition of operator()().
 * - @e T_arg3 Argument type used in the definition of operator()().
 * - @e T_arg4 Argument type used in the definition of operator()().
 * - @e T_arg5 Argument type used in the definition of operator()().
 * - @e T_return The return type of operator()().
 * - @e T_obj The object type.
 *
 * @ingroup mem_fun
 */
template <class T_return, class T_obj, class T_arg1,class T_arg2,class T_arg3,class T_arg4,class T_arg5>
class bound_volatile_mem_functor5
  : public volatile_mem_functor5<T_return, T_obj, T_arg1,T_arg2,T_arg3,T_arg4,T_arg5>
{
  typedef volatile_mem_functor5<T_return, T_obj, T_arg1,T_arg2,T_arg3,T_arg4,T_arg5> base_type_;
public:
  typedef typename base_type_::function_type function_type;

  /** Constructs a bound_volatile_mem_functor5 object that wraps the passed method.
   * @param _A_obj Pointer to instance the method will operate on.
   * @param _A_func Pointer to method will be invoked from operator()().
   */
  bound_volatile_mem_functor5( T_obj* _A_obj, function_type _A_func)
    : base_type_(_A_func),
      obj_(*_A_obj)
    {}

  /** Constructs a bound_volatile_mem_functor5 object that wraps the passed method.
   * @param _A_obj Reference to instance the method will operate on.
   * @param _A_func Pointer to method will be invoked from operator()().
   */
  bound_volatile_mem_functor5( T_obj& _A_obj, function_type _A_func)
    : base_type_(_A_func),
      obj_(_A_obj)
    {}

  /** Execute the wrapped method operating on the stored instance.
   * @param _A_a1 Argument to be passed on to the method.
   * @param _A_a2 Argument to be passed on to the method.
   * @param _A_a3 Argument to be passed on to the method.
   * @param _A_a4 Argument to be passed on to the method.
   * @param _A_a5 Argument to be passed on to the method.
   * @return The return value of the method invocation.
   */
  T_return operator()(typename type_trait<T_arg1>::take _A_a1,typename type_trait<T_arg2>::take _A_a2,typename type_trait<T_arg3>::take _A_a3,typename type_trait<T_arg4>::take _A_a4,typename type_trait<T_arg5>::take _A_a5) const
    { return (obj_.invoke().*(this->func_ptr_))(_A_a1,_A_a2,_A_a3,_A_a4,_A_a5); }

//protected:
  // Reference to stored object instance.
  // This is the handler object, such as TheObject in void TheObject::signal_handler().
  volatile_limit_reference<T_obj> obj_;
};

//template specialization of visit_each<>(action, functor):
/** Performs a functor on each of the targets of a functor.
 * The function overload for sigc::bound_volatile_mem_functor performs a functor
 * on the object instance stored in the sigc::bound_volatile_mem_functor object.
 *
 * @ingroup mem_fun
 */
template <class T_action, class T_return, class T_obj, class T_arg1,class T_arg2,class T_arg3,class T_arg4,class T_arg5>
void visit_each(const T_action& _A_action,
                const bound_volatile_mem_functor5<T_return, T_obj, T_arg1,T_arg2,T_arg3,T_arg4,T_arg5>& _A_target)
{
  sigc::visit_each(_A_action, _A_target.obj_);
}


/** bound_volatile_mem_functor6 encapsulates a volatile method with 6 arguments and an object instance.
 * Use the convenience function mem_fun() to create an instance of bound_volatile_mem_functor6.
 *
 * The following template arguments are used:
 * - @e T_arg1 Argument type used in the definition of operator()().
 * - @e T_arg2 Argument type used in the definition of operator()().
 * - @e T_arg3 Argument type used in the definition of operator()().
 * - @e T_arg4 Argument type used in the definition of operator()().
 * - @e T_arg5 Argument type used in the definition of operator()().
 * - @e T_arg6 Argument type used in the definition of operator()().
 * - @e T_return The return type of operator()().
 * - @e T_obj The object type.
 *
 * @ingroup mem_fun
 */
template <class T_return, class T_obj, class T_arg1,class T_arg2,class T_arg3,class T_arg4,class T_arg5,class T_arg6>
class bound_volatile_mem_functor6
  : public volatile_mem_functor6<T_return, T_obj, T_arg1,T_arg2,T_arg3,T_arg4,T_arg5,T_arg6>
{
  typedef volatile_mem_functor6<T_return, T_obj, T_arg1,T_arg2,T_arg3,T_arg4,T_arg5,T_arg6> base_type_;
public:
  typedef typename base_type_::function_type function_type;

  /** Constructs a bound_volatile_mem_functor6 object that wraps the passed method.
   * @param _A_obj Pointer to instance the method will operate on.
   * @param _A_func Pointer to method will be invoked from operator()().
   */
  bound_volatile_mem_functor6( T_obj* _A_obj, function_type _A_func)
    : base_type_(_A_func),
      obj_(*_A_obj)
    {}

  /** Constructs a bound_volatile_mem_functor6 object that wraps the passed method.
   * @param _A_obj Reference to instance the method will operate on.
   * @param _A_func Pointer to method will be invoked from operator()().
   */
  bound_volatile_mem_functor6( T_obj& _A_obj, function_type _A_func)
    : base_type_(_A_func),
      obj_(_A_obj)
    {}

  /** Execute the wrapped method operating on the stored instance.
   * @param _A_a1 Argument to be passed on to the method.
   * @param _A_a2 Argument to be passed on to the method.
   * @param _A_a3 Argument to be passed on to the method.
   * @param _A_a4 Argument to be passed on to the method.
   * @param _A_a5 Argument to be passed on to the method.
   * @param _A_a6 Argument to be passed on to the method.
   * @return The return value of the method invocation.
   */
  T_return operator()(typename type_trait<T_arg1>::take _A_a1,typename type_trait<T_arg2>::take _A_a2,typename type_trait<T_arg3>::take _A_a3,typename type_trait<T_arg4>::take _A_a4,typename type_trait<T_arg5>::take _A_a5,typename type_trait<T_arg6>::take _A_a6) const
    { return (obj_.invoke().*(this->func_ptr_))(_A_a1,_A_a2,_A_a3,_A_a4,_A_a5,_A_a6); }

//protected:
  // Reference to stored object instance.
  // This is the handler object, such as TheObject in void TheObject::signal_handler().
  volatile_limit_reference<T_obj> obj_;
};

//template specialization of visit_each<>(action, functor):
/** Performs a functor on each of the targets of a functor.
 * The function overload for sigc::bound_volatile_mem_functor performs a functor
 * on the object instance stored in the sigc::bound_volatile_mem_functor object.
 *
 * @ingroup mem_fun
 */
template <class T_action, class T_return, class T_obj, class T_arg1,class T_arg2,class T_arg3,class T_arg4,class T_arg5,class T_arg6>
void visit_each(const T_action& _A_action,
                const bound_volatile_mem_functor6<T_return, T_obj, T_arg1,T_arg2,T_arg3,T_arg4,T_arg5,T_arg6>& _A_target)
{
  sigc::visit_each(_A_action, _A_target.obj_);
}


/** bound_volatile_mem_functor7 encapsulates a volatile method with 7 arguments and an object instance.
 * Use the convenience function mem_fun() to create an instance of bound_volatile_mem_functor7.
 *
 * The following template arguments are used:
 * - @e T_arg1 Argument type used in the definition of operator()().
 * - @e T_arg2 Argument type used in the definition of operator()().
 * - @e T_arg3 Argument type used in the definition of operator()().
 * - @e T_arg4 Argument type used in the definition of operator()().
 * - @e T_arg5 Argument type used in the definition of operator()().
 * - @e T_arg6 Argument type used in the definition of operator()().
 * - @e T_arg7 Argument type used in the definition of operator()().
 * - @e T_return The return type of operator()().
 * - @e T_obj The object type.
 *
 * @ingroup mem_fun
 */
template <class T_return, class T_obj, class T_arg1,class T_arg2,class T_arg3,class T_arg4,class T_arg5,class T_arg6,class T_arg7>
class bound_volatile_mem_functor7
  : public volatile_mem_functor7<T_return, T_obj, T_arg1,T_arg2,T_arg3,T_arg4,T_arg5,T_arg6,T_arg7>
{
  typedef volatile_mem_functor7<T_return, T_obj, T_arg1,T_arg2,T_arg3,T_arg4,T_arg5,T_arg6,T_arg7> base_type_;
public:
  typedef typename base_type_::function_type function_type;

  /** Constructs a bound_volatile_mem_functor7 object that wraps the passed method.
   * @param _A_obj Pointer to instance the method will operate on.
   * @param _A_func Pointer to method will be invoked from operator()().
   */
  bound_volatile_mem_functor7( T_obj* _A_obj, function_type _A_func)
    : base_type_(_A_func),
      obj_(*_A_obj)
    {}

  /** Constructs a bound_volatile_mem_functor7 object that wraps the passed method.
   * @param _A_obj Reference to instance the method will operate on.
   * @param _A_func Pointer to method will be invoked from operator()().
   */
  bound_volatile_mem_functor7( T_obj& _A_obj, function_type _A_func)
    : base_type_(_A_func),
      obj_(_A_obj)
    {}

  /** Execute the wrapped method operating on the stored instance.
   * @param _A_a1 Argument to be passed on to the method.
   * @param _A_a2 Argument to be passed on to the method.
   * @param _A_a3 Argument to be passed on to the method.
   * @param _A_a4 Argument to be passed on to the method.
   * @param _A_a5 Argument to be passed on to the method.
   * @param _A_a6 Argument to be passed on to the method.
   * @param _A_a7 Argument to be passed on to the method.
   * @return The return value of the method invocation.
   */
  T_return operator()(typename type_trait<T_arg1>::take _A_a1,typename type_trait<T_arg2>::take _A_a2,typename type_trait<T_arg3>::take _A_a3,typename type_trait<T_arg4>::take _A_a4,typename type_trait<T_arg5>::take _A_a5,typename type_trait<T_arg6>::take _A_a6,typename type_trait<T_arg7>::take _A_a7) const
    { return (obj_.invoke().*(this->func_ptr_))(_A_a1,_A_a2,_A_a3,_A_a4,_A_a5,_A_a6,_A_a7); }

//protected:
  // Reference to stored object instance.
  // This is the handler object, such as TheObject in void TheObject::signal_handler().
  volatile_limit_reference<T_obj> obj_;
};

//template specialization of visit_each<>(action, functor):
/** Performs a functor on each of the targets of a functor.
 * The function overload for sigc::bound_volatile_mem_functor performs a functor
 * on the object instance stored in the sigc::bound_volatile_mem_functor object.
 *
 * @ingroup mem_fun
 */
template <class T_action, class T_return, class T_obj, class T_arg1,class T_arg2,class T_arg3,class T_arg4,class T_arg5,class T_arg6,class T_arg7>
void visit_each(const T_action& _A_action,
                const bound_volatile_mem_functor7<T_return, T_obj, T_arg1,T_arg2,T_arg3,T_arg4,T_arg5,T_arg6,T_arg7>& _A_target)
{
  sigc::visit_each(_A_action, _A_target.obj_);
}


/** bound_const_volatile_mem_functor0 encapsulates a const volatile method with 0 arguments and an object instance.
 * Use the convenience function mem_fun() to create an instance of bound_const_volatile_mem_functor0.
 *
 * The following template arguments are used:
 * - @e T_return The return type of operator()().
 * - @e T_obj The object type.
 *
 * @ingroup mem_fun
 */
template <class T_return, class T_obj>
class bound_const_volatile_mem_functor0
  : public const_volatile_mem_functor0<T_return, T_obj>
{
  typedef const_volatile_mem_functor0<T_return, T_obj> base_type_;
public:
  typedef typename base_type_::function_type function_type;

  /** Constructs a bound_const_volatile_mem_functor0 object that wraps the passed method.
   * @param _A_obj Pointer to instance the method will operate on.
   * @param _A_func Pointer to method will be invoked from operator()().
   */
  bound_const_volatile_mem_functor0(const T_obj* _A_obj, function_type _A_func)
    : base_type_(_A_func),
      obj_(*_A_obj)
    {}

  /** Constructs a bound_const_volatile_mem_functor0 object that wraps the passed method.
   * @param _A_obj Reference to instance the method will operate on.
   * @param _A_func Pointer to method will be invoked from operator()().
   */
  bound_const_volatile_mem_functor0(const T_obj& _A_obj, function_type _A_func)
    : base_type_(_A_func),
      obj_(_A_obj)
    {}

  /** Execute the wrapped method operating on the stored instance.
   * @return The return value of the method invocation.
   */
  T_return operator()() const
    { return (obj_.invoke().*(this->func_ptr_))(); }

//protected:
  // Reference to stored object instance.
  // This is the handler object, such as TheObject in void TheObject::signal_handler().
  const_volatile_limit_reference<T_obj> obj_;
};

//template specialization of visit_each<>(action, functor):
/** Performs a functor on each of the targets of a functor.
 * The function overload for sigc::bound_const_volatile_mem_functor performs a functor
 * on the object instance stored in the sigc::bound_const_volatile_mem_functor object.
 *
 * @ingroup mem_fun
 */
template <class T_action, class T_return, class T_obj>
void visit_each(const T_action& _A_action,
                const bound_const_volatile_mem_functor0<T_return, T_obj>& _A_target)
{
  sigc::visit_each(_A_action, _A_target.obj_);
}


/** bound_const_volatile_mem_functor1 encapsulates a const volatile method with 1 arguments and an object instance.
 * Use the convenience function mem_fun() to create an instance of bound_const_volatile_mem_functor1.
 *
 * The following template arguments are used:
 * - @e T_arg1 Argument type used in the definition of operator()().
 * - @e T_return The return type of operator()().
 * - @e T_obj The object type.
 *
 * @ingroup mem_fun
 */
template <class T_return, class T_obj, class T_arg1>
class bound_const_volatile_mem_functor1
  : public const_volatile_mem_functor1<T_return, T_obj, T_arg1>
{
  typedef const_volatile_mem_functor1<T_return, T_obj, T_arg1> base_type_;
public:
  typedef typename base_type_::function_type function_type;

  /** Constructs a bound_const_volatile_mem_functor1 object that wraps the passed method.
   * @param _A_obj Pointer to instance the method will operate on.
   * @param _A_func Pointer to method will be invoked from operator()().
   */
  bound_const_volatile_mem_functor1(const T_obj* _A_obj, function_type _A_func)
    : base_type_(_A_func),
      obj_(*_A_obj)
    {}

  /** Constructs a bound_const_volatile_mem_functor1 object that wraps the passed method.
   * @param _A_obj Reference to instance the method will operate on.
   * @param _A_func Pointer to method will be invoked from operator()().
   */
  bound_const_volatile_mem_functor1(const T_obj& _A_obj, function_type _A_func)
    : base_type_(_A_func),
      obj_(_A_obj)
    {}

  /** Execute the wrapped method operating on the stored instance.
   * @param _A_a1 Argument to be passed on to the method.
   * @return The return value of the method invocation.
   */
  T_return operator()(typename type_trait<T_arg1>::take _A_a1) const
    { return (obj_.invoke().*(this->func_ptr_))(_A_a1); }

//protected:
  // Reference to stored object instance.
  // This is the handler object, such as TheObject in void TheObject::signal_handler().
  const_volatile_limit_reference<T_obj> obj_;
};

//template specialization of visit_each<>(action, functor):
/** Performs a functor on each of the targets of a functor.
 * The function overload for sigc::bound_const_volatile_mem_functor performs a functor
 * on the object instance stored in the sigc::bound_const_volatile_mem_functor object.
 *
 * @ingroup mem_fun
 */
template <class T_action, class T_return, class T_obj, class T_arg1>
void visit_each(const T_action& _A_action,
                const bound_const_volatile_mem_functor1<T_return, T_obj, T_arg1>& _A_target)
{
  sigc::visit_each(_A_action, _A_target.obj_);
}


/** bound_const_volatile_mem_functor2 encapsulates a const volatile method with 2 arguments and an object instance.
 * Use the convenience function mem_fun() to create an instance of bound_const_volatile_mem_functor2.
 *
 * The following template arguments are used:
 * - @e T_arg1 Argument type used in the definition of operator()().
 * - @e T_arg2 Argument type used in the definition of operator()().
 * - @e T_return The return type of operator()().
 * - @e T_obj The object type.
 *
 * @ingroup mem_fun
 */
template <class T_return, class T_obj, class T_arg1,class T_arg2>
class bound_const_volatile_mem_functor2
  : public const_volatile_mem_functor2<T_return, T_obj, T_arg1,T_arg2>
{
  typedef const_volatile_mem_functor2<T_return, T_obj, T_arg1,T_arg2> base_type_;
public:
  typedef typename base_type_::function_type function_type;

  /** Constructs a bound_const_volatile_mem_functor2 object that wraps the passed method.
   * @param _A_obj Pointer to instance the method will operate on.
   * @param _A_func Pointer to method will be invoked from operator()().
   */
  bound_const_volatile_mem_functor2(const T_obj* _A_obj, function_type _A_func)
    : base_type_(_A_func),
      obj_(*_A_obj)
    {}

  /** Constructs a bound_const_volatile_mem_functor2 object that wraps the passed method.
   * @param _A_obj Reference to instance the method will operate on.
   * @param _A_func Pointer to method will be invoked from operator()().
   */
  bound_const_volatile_mem_functor2(const T_obj& _A_obj, function_type _A_func)
    : base_type_(_A_func),
      obj_(_A_obj)
    {}

  /** Execute the wrapped method operating on the stored instance.
   * @param _A_a1 Argument to be passed on to the method.
   * @param _A_a2 Argument to be passed on to the method.
   * @return The return value of the method invocation.
   */
  T_return operator()(typename type_trait<T_arg1>::take _A_a1,typename type_trait<T_arg2>::take _A_a2) const
    { return (obj_.invoke().*(this->func_ptr_))(_A_a1,_A_a2); }

//protected:
  // Reference to stored object instance.
  // This is the handler object, such as TheObject in void TheObject::signal_handler().
  const_volatile_limit_reference<T_obj> obj_;
};

//template specialization of visit_each<>(action, functor):
/** Performs a functor on each of the targets of a functor.
 * The function overload for sigc::bound_const_volatile_mem_functor performs a functor
 * on the object instance stored in the sigc::bound_const_volatile_mem_functor object.
 *
 * @ingroup mem_fun
 */
template <class T_action, class T_return, class T_obj, class T_arg1,class T_arg2>
void visit_each(const T_action& _A_action,
                const bound_const_volatile_mem_functor2<T_return, T_obj, T_arg1,T_arg2>& _A_target)
{
  sigc::visit_each(_A_action, _A_target.obj_);
}


/** bound_const_volatile_mem_functor3 encapsulates a const volatile method with 3 arguments and an object instance.
 * Use the convenience function mem_fun() to create an instance of bound_const_volatile_mem_functor3.
 *
 * The following template arguments are used:
 * - @e T_arg1 Argument type used in the definition of operator()().
 * - @e T_arg2 Argument type used in the definition of operator()().
 * - @e T_arg3 Argument type used in the definition of operator()().
 * - @e T_return The return type of operator()().
 * - @e T_obj The object type.
 *
 * @ingroup mem_fun
 */
template <class T_return, class T_obj, class T_arg1,class T_arg2,class T_arg3>
class bound_const_volatile_mem_functor3
  : public const_volatile_mem_functor3<T_return, T_obj, T_arg1,T_arg2,T_arg3>
{
  typedef const_volatile_mem_functor3<T_return, T_obj, T_arg1,T_arg2,T_arg3> base_type_;
public:
  typedef typename base_type_::function_type function_type;

  /** Constructs a bound_const_volatile_mem_functor3 object that wraps the passed method.
   * @param _A_obj Pointer to instance the method will operate on.
   * @param _A_func Pointer to method will be invoked from operator()().
   */
  bound_const_volatile_mem_functor3(const T_obj* _A_obj, function_type _A_func)
    : base_type_(_A_func),
      obj_(*_A_obj)
    {}

  /** Constructs a bound_const_volatile_mem_functor3 object that wraps the passed method.
   * @param _A_obj Reference to instance the method will operate on.
   * @param _A_func Pointer to method will be invoked from operator()().
   */
  bound_const_volatile_mem_functor3(const T_obj& _A_obj, function_type _A_func)
    : base_type_(_A_func),
      obj_(_A_obj)
    {}

  /** Execute the wrapped method operating on the stored instance.
   * @param _A_a1 Argument to be passed on to the method.
   * @param _A_a2 Argument to be passed on to the method.
   * @param _A_a3 Argument to be passed on to the method.
   * @return The return value of the method invocation.
   */
  T_return operator()(typename type_trait<T_arg1>::take _A_a1,typename type_trait<T_arg2>::take _A_a2,typename type_trait<T_arg3>::take _A_a3) const
    { return (obj_.invoke().*(this->func_ptr_))(_A_a1,_A_a2,_A_a3); }

//protected:
  // Reference to stored object instance.
  // This is the handler object, such as TheObject in void TheObject::signal_handler().
  const_volatile_limit_reference<T_obj> obj_;
};

//template specialization of visit_each<>(action, functor):
/** Performs a functor on each of the targets of a functor.
 * The function overload for sigc::bound_const_volatile_mem_functor performs a functor
 * on the object instance stored in the sigc::bound_const_volatile_mem_functor object.
 *
 * @ingroup mem_fun
 */
template <class T_action, class T_return, class T_obj, class T_arg1,class T_arg2,class T_arg3>
void visit_each(const T_action& _A_action,
                const bound_const_volatile_mem_functor3<T_return, T_obj, T_arg1,T_arg2,T_arg3>& _A_target)
{
  sigc::visit_each(_A_action, _A_target.obj_);
}


/** bound_const_volatile_mem_functor4 encapsulates a const volatile method with 4 arguments and an object instance.
 * Use the convenience function mem_fun() to create an instance of bound_const_volatile_mem_functor4.
 *
 * The following template arguments are used:
 * - @e T_arg1 Argument type used in the definition of operator()().
 * - @e T_arg2 Argument type used in the definition of operator()().
 * - @e T_arg3 Argument type used in the definition of operator()().
 * - @e T_arg4 Argument type used in the definition of operator()().
 * - @e T_return The return type of operator()().
 * - @e T_obj The object type.
 *
 * @ingroup mem_fun
 */
template <class T_return, class T_obj, class T_arg1,class T_arg2,class T_arg3,class T_arg4>
class bound_const_volatile_mem_functor4
  : public const_volatile_mem_functor4<T_return, T_obj, T_arg1,T_arg2,T_arg3,T_arg4>
{
  typedef const_volatile_mem_functor4<T_return, T_obj, T_arg1,T_arg2,T_arg3,T_arg4> base_type_;
public:
  typedef typename base_type_::function_type function_type;

  /** Constructs a bound_const_volatile_mem_functor4 object that wraps the passed method.
   * @param _A_obj Pointer to instance the method will operate on.
   * @param _A_func Pointer to method will be invoked from operator()().
   */
  bound_const_volatile_mem_functor4(const T_obj* _A_obj, function_type _A_func)
    : base_type_(_A_func),
      obj_(*_A_obj)
    {}

  /** Constructs a bound_const_volatile_mem_functor4 object that wraps the passed method.
   * @param _A_obj Reference to instance the method will operate on.
   * @param _A_func Pointer to method will be invoked from operator()().
   */
  bound_const_volatile_mem_functor4(const T_obj& _A_obj, function_type _A_func)
    : base_type_(_A_func),
      obj_(_A_obj)
    {}

  /** Execute the wrapped method operating on the stored instance.
   * @param _A_a1 Argument to be passed on to the method.
   * @param _A_a2 Argument to be passed on to the method.
   * @param _A_a3 Argument to be passed on to the method.
   * @param _A_a4 Argument to be passed on to the method.
   * @return The return value of the method invocation.
   */
  T_return operator()(typename type_trait<T_arg1>::take _A_a1,typename type_trait<T_arg2>::take _A_a2,typename type_trait<T_arg3>::take _A_a3,typename type_trait<T_arg4>::take _A_a4) const
    { return (obj_.invoke().*(this->func_ptr_))(_A_a1,_A_a2,_A_a3,_A_a4); }

//protected:
  // Reference to stored object instance.
  // This is the handler object, such as TheObject in void TheObject::signal_handler().
  const_volatile_limit_reference<T_obj> obj_;
};

//template specialization of visit_each<>(action, functor):
/** Performs a functor on each of the targets of a functor.
 * The function overload for sigc::bound_const_volatile_mem_functor performs a functor
 * on the object instance stored in the sigc::bound_const_volatile_mem_functor object.
 *
 * @ingroup mem_fun
 */
template <class T_action, class T_return, class T_obj, class T_arg1,class T_arg2,class T_arg3,class T_arg4>
void visit_each(const T_action& _A_action,
                const bound_const_volatile_mem_functor4<T_return, T_obj, T_arg1,T_arg2,T_arg3,T_arg4>& _A_target)
{
  sigc::visit_each(_A_action, _A_target.obj_);
}


/** bound_const_volatile_mem_functor5 encapsulates a const volatile method with 5 arguments and an object instance.
 * Use the convenience function mem_fun() to create an instance of bound_const_volatile_mem_functor5.
 *
 * The following template arguments are used:
 * - @e T_arg1 Argument type used in the definition of operator()().
 * - @e T_arg2 Argument type used in the definition of operator()().
 * - @e T_arg3 Argument type used in the definition of operator()().
 * - @e T_arg4 Argument type used in the definition of operator()().
 * - @e T_arg5 Argument type used in the definition of operator()().
 * - @e T_return The return type of operator()().
 * - @e T_obj The object type.
 *
 * @ingroup mem_fun
 */
template <class T_return, class T_obj, class T_arg1,class T_arg2,class T_arg3,class T_arg4,class T_arg5>
class bound_const_volatile_mem_functor5
  : public const_volatile_mem_functor5<T_return, T_obj, T_arg1,T_arg2,T_arg3,T_arg4,T_arg5>
{
  typedef const_volatile_mem_functor5<T_return, T_obj, T_arg1,T_arg2,T_arg3,T_arg4,T_arg5> base_type_;
public:
  typedef typename base_type_::function_type function_type;

  /** Constructs a bound_const_volatile_mem_functor5 object that wraps the passed method.
   * @param _A_obj Pointer to instance the method will operate on.
   * @param _A_func Pointer to method will be invoked from operator()().
   */
  bound_const_volatile_mem_functor5(const T_obj* _A_obj, function_type _A_func)
    : base_type_(_A_func),
      obj_(*_A_obj)
    {}

  /** Constructs a bound_const_volatile_mem_functor5 object that wraps the passed method.
   * @param _A_obj Reference to instance the method will operate on.
   * @param _A_func Pointer to method will be invoked from operator()().
   */
  bound_const_volatile_mem_functor5(const T_obj& _A_obj, function_type _A_func)
    : base_type_(_A_func),
      obj_(_A_obj)
    {}

  /** Execute the wrapped method operating on the stored instance.
   * @param _A_a1 Argument to be passed on to the method.
   * @param _A_a2 Argument to be passed on to the method.
   * @param _A_a3 Argument to be passed on to the method.
   * @param _A_a4 Argument to be passed on to the method.
   * @param _A_a5 Argument to be passed on to the method.
   * @return The return value of the method invocation.
   */
  T_return operator()(typename type_trait<T_arg1>::take _A_a1,typename type_trait<T_arg2>::take _A_a2,typename type_trait<T_arg3>::take _A_a3,typename type_trait<T_arg4>::take _A_a4,typename type_trait<T_arg5>::take _A_a5) const
    { return (obj_.invoke().*(this->func_ptr_))(_A_a1,_A_a2,_A_a3,_A_a4,_A_a5); }

//protected:
  // Reference to stored object instance.
  // This is the handler object, such as TheObject in void TheObject::signal_handler().
  const_volatile_limit_reference<T_obj> obj_;
};

//template specialization of visit_each<>(action, functor):
/** Performs a functor on each of the targets of a functor.
 * The function overload for sigc::bound_const_volatile_mem_functor performs a functor
 * on the object instance stored in the sigc::bound_const_volatile_mem_functor object.
 *
 * @ingroup mem_fun
 */
template <class T_action, class T_return, class T_obj, class T_arg1,class T_arg2,class T_arg3,class T_arg4,class T_arg5>
void visit_each(const T_action& _A_action,
                const bound_const_volatile_mem_functor5<T_return, T_obj, T_arg1,T_arg2,T_arg3,T_arg4,T_arg5>& _A_target)
{
  sigc::visit_each(_A_action, _A_target.obj_);
}


/** bound_const_volatile_mem_functor6 encapsulates a const volatile method with 6 arguments and an object instance.
 * Use the convenience function mem_fun() to create an instance of bound_const_volatile_mem_functor6.
 *
 * The following template arguments are used:
 * - @e T_arg1 Argument type used in the definition of operator()().
 * - @e T_arg2 Argument type used in the definition of operator()().
 * - @e T_arg3 Argument type used in the definition of operator()().
 * - @e T_arg4 Argument type used in the definition of operator()().
 * - @e T_arg5 Argument type used in the definition of operator()().
 * - @e T_arg6 Argument type used in the definition of operator()().
 * - @e T_return The return type of operator()().
 * - @e T_obj The object type.
 *
 * @ingroup mem_fun
 */
template <class T_return, class T_obj, class T_arg1,class T_arg2,class T_arg3,class T_arg4,class T_arg5,class T_arg6>
class bound_const_volatile_mem_functor6
  : public const_volatile_mem_functor6<T_return, T_obj, T_arg1,T_arg2,T_arg3,T_arg4,T_arg5,T_arg6>
{
  typedef const_volatile_mem_functor6<T_return, T_obj, T_arg1,T_arg2,T_arg3,T_arg4,T_arg5,T_arg6> base_type_;
public:
  typedef typename base_type_::function_type function_type;

  /** Constructs a bound_const_volatile_mem_functor6 object that wraps the passed method.
   * @param _A_obj Pointer to instance the method will operate on.
   * @param _A_func Pointer to method will be invoked from operator()().
   */
  bound_const_volatile_mem_functor6(const T_obj* _A_obj, function_type _A_func)
    : base_type_(_A_func),
      obj_(*_A_obj)
    {}

  /** Constructs a bound_const_volatile_mem_functor6 object that wraps the passed method.
   * @param _A_obj Reference to instance the method will operate on.
   * @param _A_func Pointer to method will be invoked from operator()().
   */
  bound_const_volatile_mem_functor6(const T_obj& _A_obj, function_type _A_func)
    : base_type_(_A_func),
      obj_(_A_obj)
    {}

  /** Execute the wrapped method operating on the stored instance.
   * @param _A_a1 Argument to be passed on to the method.
   * @param _A_a2 Argument to be passed on to the method.
   * @param _A_a3 Argument to be passed on to the method.
   * @param _A_a4 Argument to be passed on to the method.
   * @param _A_a5 Argument to be passed on to the method.
   * @param _A_a6 Argument to be passed on to the method.
   * @return The return value of the method invocation.
   */
  T_return operator()(typename type_trait<T_arg1>::take _A_a1,typename type_trait<T_arg2>::take _A_a2,typename type_trait<T_arg3>::take _A_a3,typename type_trait<T_arg4>::take _A_a4,typename type_trait<T_arg5>::take _A_a5,typename type_trait<T_arg6>::take _A_a6) const
    { return (obj_.invoke().*(this->func_ptr_))(_A_a1,_A_a2,_A_a3,_A_a4,_A_a5,_A_a6); }

//protected:
  // Reference to stored object instance.
  // This is the handler object, such as TheObject in void TheObject::signal_handler().
  const_volatile_limit_reference<T_obj> obj_;
};

//template specialization of visit_each<>(action, functor):
/** Performs a functor on each of the targets of a functor.
 * The function overload for sigc::bound_const_volatile_mem_functor performs a functor
 * on the object instance stored in the sigc::bound_const_volatile_mem_functor object.
 *
 * @ingroup mem_fun
 */
template <class T_action, class T_return, class T_obj, class T_arg1,class T_arg2,class T_arg3,class T_arg4,class T_arg5,class T_arg6>
void visit_each(const T_action& _A_action,
                const bound_const_volatile_mem_functor6<T_return, T_obj, T_arg1,T_arg2,T_arg3,T_arg4,T_arg5,T_arg6>& _A_target)
{
  sigc::visit_each(_A_action, _A_target.obj_);
}


/** bound_const_volatile_mem_functor7 encapsulates a const volatile method with 7 arguments and an object instance.
 * Use the convenience function mem_fun() to create an instance of bound_const_volatile_mem_functor7.
 *
 * The following template arguments are used:
 * - @e T_arg1 Argument type used in the definition of operator()().
 * - @e T_arg2 Argument type used in the definition of operator()().
 * - @e T_arg3 Argument type used in the definition of operator()().
 * - @e T_arg4 Argument type used in the definition of operator()().
 * - @e T_arg5 Argument type used in the definition of operator()().
 * - @e T_arg6 Argument type used in the definition of operator()().
 * - @e T_arg7 Argument type used in the definition of operator()().
 * - @e T_return The return type of operator()().
 * - @e T_obj The object type.
 *
 * @ingroup mem_fun
 */
template <class T_return, class T_obj, class T_arg1,class T_arg2,class T_arg3,class T_arg4,class T_arg5,class T_arg6,class T_arg7>
class bound_const_volatile_mem_functor7
  : public const_volatile_mem_functor7<T_return, T_obj, T_arg1,T_arg2,T_arg3,T_arg4,T_arg5,T_arg6,T_arg7>
{
  typedef const_volatile_mem_functor7<T_return, T_obj, T_arg1,T_arg2,T_arg3,T_arg4,T_arg5,T_arg6,T_arg7> base_type_;
public:
  typedef typename base_type_::function_type function_type;

  /** Constructs a bound_const_volatile_mem_functor7 object that wraps the passed method.
   * @param _A_obj Pointer to instance the method will operate on.
   * @param _A_func Pointer to method will be invoked from operator()().
   */
  bound_const_volatile_mem_functor7(const T_obj* _A_obj, function_type _A_func)
    : base_type_(_A_func),
      obj_(*_A_obj)
    {}

  /** Constructs a bound_const_volatile_mem_functor7 object that wraps the passed method.
   * @param _A_obj Reference to instance the method will operate on.
   * @param _A_func Pointer to method will be invoked from operator()().
   */
  bound_const_volatile_mem_functor7(const T_obj& _A_obj, function_type _A_func)
    : base_type_(_A_func),
      obj_(_A_obj)
    {}

  /** Execute the wrapped method operating on the stored instance.
   * @param _A_a1 Argument to be passed on to the method.
   * @param _A_a2 Argument to be passed on to the method.
   * @param _A_a3 Argument to be passed on to the method.
   * @param _A_a4 Argument to be passed on to the method.
   * @param _A_a5 Argument to be passed on to the method.
   * @param _A_a6 Argument to be passed on to the method.
   * @param _A_a7 Argument to be passed on to the method.
   * @return The return value of the method invocation.
   */
  T_return operator()(typename type_trait<T_arg1>::take _A_a1,typename type_trait<T_arg2>::take _A_a2,typename type_trait<T_arg3>::take _A_a3,typename type_trait<T_arg4>::take _A_a4,typename type_trait<T_arg5>::take _A_a5,typename type_trait<T_arg6>::take _A_a6,typename type_trait<T_arg7>::take _A_a7) const
    { return (obj_.invoke().*(this->func_ptr_))(_A_a1,_A_a2,_A_a3,_A_a4,_A_a5,_A_a6,_A_a7); }

//protected:
  // Reference to stored object instance.
  // This is the handler object, such as TheObject in void TheObject::signal_handler().
  const_volatile_limit_reference<T_obj> obj_;
};

//template specialization of visit_each<>(action, functor):
/** Performs a functor on each of the targets of a functor.
 * The function overload for sigc::bound_const_volatile_mem_functor performs a functor
 * on the object instance stored in the sigc::bound_const_volatile_mem_functor object.
 *
 * @ingroup mem_fun
 */
template <class T_action, class T_return, class T_obj, class T_arg1,class T_arg2,class T_arg3,class T_arg4,class T_arg5,class T_arg6,class T_arg7>
void visit_each(const T_action& _A_action,
                const bound_const_volatile_mem_functor7<T_return, T_obj, T_arg1,T_arg2,T_arg3,T_arg4,T_arg5,T_arg6,T_arg7>& _A_target)
{
  sigc::visit_each(_A_action, _A_target.obj_);
}


// numbered
/** Creates a functor of type sigc::mem_functor0 which wraps a  method.
 * @param _A_func Pointer to method that should be wrapped.
 * @return Functor that executes _A_func on invokation.
 *
 * @ingroup mem_fun
 */
template <class T_return, class T_obj>
inline mem_functor0<T_return, T_obj>
mem_fun0(T_return (T_obj::*_A_func)() )
{ return mem_functor0<T_return, T_obj>(_A_func); }

/** Creates a functor of type sigc::mem_functor1 which wraps a  method.
 * @param _A_func Pointer to method that should be wrapped.
 * @return Functor that executes _A_func on invokation.
 *
 * @ingroup mem_fun
 */
template <class T_arg1, class T_return, class T_obj>
inline mem_functor1<T_return, T_obj, T_arg1>
mem_fun1(T_return (T_obj::*_A_func)(T_arg1) )
{ return mem_functor1<T_return, T_obj, T_arg1>(_A_func); }

/** Creates a functor of type sigc::mem_functor2 which wraps a  method.
 * @param _A_func Pointer to method that should be wrapped.
 * @return Functor that executes _A_func on invokation.
 *
 * @ingroup mem_fun
 */
template <class T_arg1,class T_arg2, class T_return, class T_obj>
inline mem_functor2<T_return, T_obj, T_arg1,T_arg2>
mem_fun2(T_return (T_obj::*_A_func)(T_arg1,T_arg2) )
{ return mem_functor2<T_return, T_obj, T_arg1,T_arg2>(_A_func); }

/** Creates a functor of type sigc::mem_functor3 which wraps a  method.
 * @param _A_func Pointer to method that should be wrapped.
 * @return Functor that executes _A_func on invokation.
 *
 * @ingroup mem_fun
 */
template <class T_arg1,class T_arg2,class T_arg3, class T_return, class T_obj>
inline mem_functor3<T_return, T_obj, T_arg1,T_arg2,T_arg3>
mem_fun3(T_return (T_obj::*_A_func)(T_arg1,T_arg2,T_arg3) )
{ return mem_functor3<T_return, T_obj, T_arg1,T_arg2,T_arg3>(_A_func); }

/** Creates a functor of type sigc::mem_functor4 which wraps a  method.
 * @param _A_func Pointer to method that should be wrapped.
 * @return Functor that executes _A_func on invokation.
 *
 * @ingroup mem_fun
 */
template <class T_arg1,class T_arg2,class T_arg3,class T_arg4, class T_return, class T_obj>
inline mem_functor4<T_return, T_obj, T_arg1,T_arg2,T_arg3,T_arg4>
mem_fun4(T_return (T_obj::*_A_func)(T_arg1,T_arg2,T_arg3,T_arg4) )
{ return mem_functor4<T_return, T_obj, T_arg1,T_arg2,T_arg3,T_arg4>(_A_func); }

/** Creates a functor of type sigc::mem_functor5 which wraps a  method.
 * @param _A_func Pointer to method that should be wrapped.
 * @return Functor that executes _A_func on invokation.
 *
 * @ingroup mem_fun
 */
template <class T_arg1,class T_arg2,class T_arg3,class T_arg4,class T_arg5, class T_return, class T_obj>
inline mem_functor5<T_return, T_obj, T_arg1,T_arg2,T_arg3,T_arg4,T_arg5>
mem_fun5(T_return (T_obj::*_A_func)(T_arg1,T_arg2,T_arg3,T_arg4,T_arg5) )
{ return mem_functor5<T_return, T_obj, T_arg1,T_arg2,T_arg3,T_arg4,T_arg5>(_A_func); }

/** Creates a functor of type sigc::mem_functor6 which wraps a  method.
 * @param _A_func Pointer to method that should be wrapped.
 * @return Functor that executes _A_func on invokation.
 *
 * @ingroup mem_fun
 */
template <class T_arg1,class T_arg2,class T_arg3,class T_arg4,class T_arg5,class T_arg6, class T_return, class T_obj>
inline mem_functor6<T_return, T_obj, T_arg1,T_arg2,T_arg3,T_arg4,T_arg5,T_arg6>
mem_fun6(T_return (T_obj::*_A_func)(T_arg1,T_arg2,T_arg3,T_arg4,T_arg5,T_arg6) )
{ return mem_functor6<T_return, T_obj, T_arg1,T_arg2,T_arg3,T_arg4,T_arg5,T_arg6>(_A_func); }

/** Creates a functor of type sigc::mem_functor7 which wraps a  method.
 * @param _A_func Pointer to method that should be wrapped.
 * @return Functor that executes _A_func on invokation.
 *
 * @ingroup mem_fun
 */
template <class T_arg1,class T_arg2,class T_arg3,class T_arg4,class T_arg5,class T_arg6,class T_arg7, class T_return, class T_obj>
inline mem_functor7<T_return, T_obj, T_arg1,T_arg2,T_arg3,T_arg4,T_arg5,T_arg6,T_arg7>
mem_fun7(T_return (T_obj::*_A_func)(T_arg1,T_arg2,T_arg3,T_arg4,T_arg5,T_arg6,T_arg7) )
{ return mem_functor7<T_return, T_obj, T_arg1,T_arg2,T_arg3,T_arg4,T_arg5,T_arg6,T_arg7>(_A_func); }

/** Creates a functor of type sigc::const_mem_functor0 which wraps a const method.
 * @param _A_func Pointer to method that should be wrapped.
 * @return Functor that executes _A_func on invokation.
 *
 * @ingroup mem_fun
 */
template <class T_return, class T_obj>
inline const_mem_functor0<T_return, T_obj>
mem_fun0(T_return (T_obj::*_A_func)() const)
{ return const_mem_functor0<T_return, T_obj>(_A_func); }

/** Creates a functor of type sigc::const_mem_functor1 which wraps a const method.
 * @param _A_func Pointer to method that should be wrapped.
 * @return Functor that executes _A_func on invokation.
 *
 * @ingroup mem_fun
 */
template <class T_arg1, class T_return, class T_obj>
inline const_mem_functor1<T_return, T_obj, T_arg1>
mem_fun1(T_return (T_obj::*_A_func)(T_arg1) const)
{ return const_mem_functor1<T_return, T_obj, T_arg1>(_A_func); }

/** Creates a functor of type sigc::const_mem_functor2 which wraps a const method.
 * @param _A_func Pointer to method that should be wrapped.
 * @return Functor that executes _A_func on invokation.
 *
 * @ingroup mem_fun
 */
template <class T_arg1,class T_arg2, class T_return, class T_obj>
inline const_mem_functor2<T_return, T_obj, T_arg1,T_arg2>
mem_fun2(T_return (T_obj::*_A_func)(T_arg1,T_arg2) const)
{ return const_mem_functor2<T_return, T_obj, T_arg1,T_arg2>(_A_func); }

/** Creates a functor of type sigc::const_mem_functor3 which wraps a const method.
 * @param _A_func Pointer to method that should be wrapped.
 * @return Functor that executes _A_func on invokation.
 *
 * @ingroup mem_fun
 */
template <class T_arg1,class T_arg2,class T_arg3, class T_return, class T_obj>
inline const_mem_functor3<T_return, T_obj, T_arg1,T_arg2,T_arg3>
mem_fun3(T_return (T_obj::*_A_func)(T_arg1,T_arg2,T_arg3) const)
{ return const_mem_functor3<T_return, T_obj, T_arg1,T_arg2,T_arg3>(_A_func); }

/** Creates a functor of type sigc::const_mem_functor4 which wraps a const method.
 * @param _A_func Pointer to method that should be wrapped.
 * @return Functor that executes _A_func on invokation.
 *
 * @ingroup mem_fun
 */
template <class T_arg1,class T_arg2,class T_arg3,class T_arg4, class T_return, class T_obj>
inline const_mem_functor4<T_return, T_obj, T_arg1,T_arg2,T_arg3,T_arg4>
mem_fun4(T_return (T_obj::*_A_func)(T_arg1,T_arg2,T_arg3,T_arg4) const)
{ return const_mem_functor4<T_return, T_obj, T_arg1,T_arg2,T_arg3,T_arg4>(_A_func); }

/** Creates a functor of type sigc::const_mem_functor5 which wraps a const method.
 * @param _A_func Pointer to method that should be wrapped.
 * @return Functor that executes _A_func on invokation.
 *
 * @ingroup mem_fun
 */
template <class T_arg1,class T_arg2,class T_arg3,class T_arg4,class T_arg5, class T_return, class T_obj>
inline const_mem_functor5<T_return, T_obj, T_arg1,T_arg2,T_arg3,T_arg4,T_arg5>
mem_fun5(T_return (T_obj::*_A_func)(T_arg1,T_arg2,T_arg3,T_arg4,T_arg5) const)
{ return const_mem_functor5<T_return, T_obj, T_arg1,T_arg2,T_arg3,T_arg4,T_arg5>(_A_func); }

/** Creates a functor of type sigc::const_mem_functor6 which wraps a const method.
 * @param _A_func Pointer to method that should be wrapped.
 * @return Functor that executes _A_func on invokation.
 *
 * @ingroup mem_fun
 */
template <class T_arg1,class T_arg2,class T_arg3,class T_arg4,class T_arg5,class T_arg6, class T_return, class T_obj>
inline const_mem_functor6<T_return, T_obj, T_arg1,T_arg2,T_arg3,T_arg4,T_arg5,T_arg6>
mem_fun6(T_return (T_obj::*_A_func)(T_arg1,T_arg2,T_arg3,T_arg4,T_arg5,T_arg6) const)
{ return const_mem_functor6<T_return, T_obj, T_arg1,T_arg2,T_arg3,T_arg4,T_arg5,T_arg6>(_A_func); }

/** Creates a functor of type sigc::const_mem_functor7 which wraps a const method.
 * @param _A_func Pointer to method that should be wrapped.
 * @return Functor that executes _A_func on invokation.
 *
 * @ingroup mem_fun
 */
template <class T_arg1,class T_arg2,class T_arg3,class T_arg4,class T_arg5,class T_arg6,class T_arg7, class T_return, class T_obj>
inline const_mem_functor7<T_return, T_obj, T_arg1,T_arg2,T_arg3,T_arg4,T_arg5,T_arg6,T_arg7>
mem_fun7(T_return (T_obj::*_A_func)(T_arg1,T_arg2,T_arg3,T_arg4,T_arg5,T_arg6,T_arg7) const)
{ return const_mem_functor7<T_return, T_obj, T_arg1,T_arg2,T_arg3,T_arg4,T_arg5,T_arg6,T_arg7>(_A_func); }

/** Creates a functor of type sigc::volatile_mem_functor0 which wraps a volatile method.
 * @param _A_func Pointer to method that should be wrapped.
 * @return Functor that executes _A_func on invokation.
 *
 * @ingroup mem_fun
 */
template <class T_return, class T_obj>
inline volatile_mem_functor0<T_return, T_obj>
mem_fun0(T_return (T_obj::*_A_func)() volatile)
{ return volatile_mem_functor0<T_return, T_obj>(_A_func); }

/** Creates a functor of type sigc::volatile_mem_functor1 which wraps a volatile method.
 * @param _A_func Pointer to method that should be wrapped.
 * @return Functor that executes _A_func on invokation.
 *
 * @ingroup mem_fun
 */
template <class T_arg1, class T_return, class T_obj>
inline volatile_mem_functor1<T_return, T_obj, T_arg1>
mem_fun1(T_return (T_obj::*_A_func)(T_arg1) volatile)
{ return volatile_mem_functor1<T_return, T_obj, T_arg1>(_A_func); }

/** Creates a functor of type sigc::volatile_mem_functor2 which wraps a volatile method.
 * @param _A_func Pointer to method that should be wrapped.
 * @return Functor that executes _A_func on invokation.
 *
 * @ingroup mem_fun
 */
template <class T_arg1,class T_arg2, class T_return, class T_obj>
inline volatile_mem_functor2<T_return, T_obj, T_arg1,T_arg2>
mem_fun2(T_return (T_obj::*_A_func)(T_arg1,T_arg2) volatile)
{ return volatile_mem_functor2<T_return, T_obj, T_arg1,T_arg2>(_A_func); }

/** Creates a functor of type sigc::volatile_mem_functor3 which wraps a volatile method.
 * @param _A_func Pointer to method that should be wrapped.
 * @return Functor that executes _A_func on invokation.
 *
 * @ingroup mem_fun
 */
template <class T_arg1,class T_arg2,class T_arg3, class T_return, class T_obj>
inline volatile_mem_functor3<T_return, T_obj, T_arg1,T_arg2,T_arg3>
mem_fun3(T_return (T_obj::*_A_func)(T_arg1,T_arg2,T_arg3) volatile)
{ return volatile_mem_functor3<T_return, T_obj, T_arg1,T_arg2,T_arg3>(_A_func); }

/** Creates a functor of type sigc::volatile_mem_functor4 which wraps a volatile method.
 * @param _A_func Pointer to method that should be wrapped.
 * @return Functor that executes _A_func on invokation.
 *
 * @ingroup mem_fun
 */
template <class T_arg1,class T_arg2,class T_arg3,class T_arg4, class T_return, class T_obj>
inline volatile_mem_functor4<T_return, T_obj, T_arg1,T_arg2,T_arg3,T_arg4>
mem_fun4(T_return (T_obj::*_A_func)(T_arg1,T_arg2,T_arg3,T_arg4) volatile)
{ return volatile_mem_functor4<T_return, T_obj, T_arg1,T_arg2,T_arg3,T_arg4>(_A_func); }

/** Creates a functor of type sigc::volatile_mem_functor5 which wraps a volatile method.
 * @param _A_func Pointer to method that should be wrapped.
 * @return Functor that executes _A_func on invokation.
 *
 * @ingroup mem_fun
 */
template <class T_arg1,class T_arg2,class T_arg3,class T_arg4,class T_arg5, class T_return, class T_obj>
inline volatile_mem_functor5<T_return, T_obj, T_arg1,T_arg2,T_arg3,T_arg4,T_arg5>
mem_fun5(T_return (T_obj::*_A_func)(T_arg1,T_arg2,T_arg3,T_arg4,T_arg5) volatile)
{ return volatile_mem_functor5<T_return, T_obj, T_arg1,T_arg2,T_arg3,T_arg4,T_arg5>(_A_func); }

/** Creates a functor of type sigc::volatile_mem_functor6 which wraps a volatile method.
 * @param _A_func Pointer to method that should be wrapped.
 * @return Functor that executes _A_func on invokation.
 *
 * @ingroup mem_fun
 */
template <class T_arg1,class T_arg2,class T_arg3,class T_arg4,class T_arg5,class T_arg6, class T_return, class T_obj>
inline volatile_mem_functor6<T_return, T_obj, T_arg1,T_arg2,T_arg3,T_arg4,T_arg5,T_arg6>
mem_fun6(T_return (T_obj::*_A_func)(T_arg1,T_arg2,T_arg3,T_arg4,T_arg5,T_arg6) volatile)
{ return volatile_mem_functor6<T_return, T_obj, T_arg1,T_arg2,T_arg3,T_arg4,T_arg5,T_arg6>(_A_func); }

/** Creates a functor of type sigc::volatile_mem_functor7 which wraps a volatile method.
 * @param _A_func Pointer to method that should be wrapped.
 * @return Functor that executes _A_func on invokation.
 *
 * @ingroup mem_fun
 */
template <class T_arg1,class T_arg2,class T_arg3,class T_arg4,class T_arg5,class T_arg6,class T_arg7, class T_return, class T_obj>
inline volatile_mem_functor7<T_return, T_obj, T_arg1,T_arg2,T_arg3,T_arg4,T_arg5,T_arg6,T_arg7>
mem_fun7(T_return (T_obj::*_A_func)(T_arg1,T_arg2,T_arg3,T_arg4,T_arg5,T_arg6,T_arg7) volatile)
{ return volatile_mem_functor7<T_return, T_obj, T_arg1,T_arg2,T_arg3,T_arg4,T_arg5,T_arg6,T_arg7>(_A_func); }

/** Creates a functor of type sigc::const_volatile_mem_functor0 which wraps a const volatile method.
 * @param _A_func Pointer to method that should be wrapped.
 * @return Functor that executes _A_func on invokation.
 *
 * @ingroup mem_fun
 */
template <class T_return, class T_obj>
inline const_volatile_mem_functor0<T_return, T_obj>
mem_fun0(T_return (T_obj::*_A_func)() const volatile)
{ return const_volatile_mem_functor0<T_return, T_obj>(_A_func); }

/** Creates a functor of type sigc::const_volatile_mem_functor1 which wraps a const volatile method.
 * @param _A_func Pointer to method that should be wrapped.
 * @return Functor that executes _A_func on invokation.
 *
 * @ingroup mem_fun
 */
template <class T_arg1, class T_return, class T_obj>
inline const_volatile_mem_functor1<T_return, T_obj, T_arg1>
mem_fun1(T_return (T_obj::*_A_func)(T_arg1) const volatile)
{ return const_volatile_mem_functor1<T_return, T_obj, T_arg1>(_A_func); }

/** Creates a functor of type sigc::const_volatile_mem_functor2 which wraps a const volatile method.
 * @param _A_func Pointer to method that should be wrapped.
 * @return Functor that executes _A_func on invokation.
 *
 * @ingroup mem_fun
 */
template <class T_arg1,class T_arg2, class T_return, class T_obj>
inline const_volatile_mem_functor2<T_return, T_obj, T_arg1,T_arg2>
mem_fun2(T_return (T_obj::*_A_func)(T_arg1,T_arg2) const volatile)
{ return const_volatile_mem_functor2<T_return, T_obj, T_arg1,T_arg2>(_A_func); }

/** Creates a functor of type sigc::const_volatile_mem_functor3 which wraps a const volatile method.
 * @param _A_func Pointer to method that should be wrapped.
 * @return Functor that executes _A_func on invokation.
 *
 * @ingroup mem_fun
 */
template <class T_arg1,class T_arg2,class T_arg3, class T_return, class T_obj>
inline const_volatile_mem_functor3<T_return, T_obj, T_arg1,T_arg2,T_arg3>
mem_fun3(T_return (T_obj::*_A_func)(T_arg1,T_arg2,T_arg3) const volatile)
{ return const_volatile_mem_functor3<T_return, T_obj, T_arg1,T_arg2,T_arg3>(_A_func); }

/** Creates a functor of type sigc::const_volatile_mem_functor4 which wraps a const volatile method.
 * @param _A_func Pointer to method that should be wrapped.
 * @return Functor that executes _A_func on invokation.
 *
 * @ingroup mem_fun
 */
template <class T_arg1,class T_arg2,class T_arg3,class T_arg4, class T_return, class T_obj>
inline const_volatile_mem_functor4<T_return, T_obj, T_arg1,T_arg2,T_arg3,T_arg4>
mem_fun4(T_return (T_obj::*_A_func)(T_arg1,T_arg2,T_arg3,T_arg4) const volatile)
{ return const_volatile_mem_functor4<T_return, T_obj, T_arg1,T_arg2,T_arg3,T_arg4>(_A_func); }

/** Creates a functor of type sigc::const_volatile_mem_functor5 which wraps a const volatile method.
 * @param _A_func Pointer to method that should be wrapped.
 * @return Functor that executes _A_func on invokation.
 *
 * @ingroup mem_fun
 */
template <class T_arg1,class T_arg2,class T_arg3,class T_arg4,class T_arg5, class T_return, class T_obj>
inline const_volatile_mem_functor5<T_return, T_obj, T_arg1,T_arg2,T_arg3,T_arg4,T_arg5>
mem_fun5(T_return (T_obj::*_A_func)(T_arg1,T_arg2,T_arg3,T_arg4,T_arg5) const volatile)
{ return const_volatile_mem_functor5<T_return, T_obj, T_arg1,T_arg2,T_arg3,T_arg4,T_arg5>(_A_func); }

/** Creates a functor of type sigc::const_volatile_mem_functor6 which wraps a const volatile method.
 * @param _A_func Pointer to method that should be wrapped.
 * @return Functor that executes _A_func on invokation.
 *
 * @ingroup mem_fun
 */
template <class T_arg1,class T_arg2,class T_arg3,class T_arg4,class T_arg5,class T_arg6, class T_return, class T_obj>
inline const_volatile_mem_functor6<T_return, T_obj, T_arg1,T_arg2,T_arg3,T_arg4,T_arg5,T_arg6>
mem_fun6(T_return (T_obj::*_A_func)(T_arg1,T_arg2,T_arg3,T_arg4,T_arg5,T_arg6) const volatile)
{ return const_volatile_mem_functor6<T_return, T_obj, T_arg1,T_arg2,T_arg3,T_arg4,T_arg5,T_arg6>(_A_func); }

/** Creates a functor of type sigc::const_volatile_mem_functor7 which wraps a const volatile method.
 * @param _A_func Pointer to method that should be wrapped.
 * @return Functor that executes _A_func on invokation.
 *
 * @ingroup mem_fun
 */
template <class T_arg1,class T_arg2,class T_arg3,class T_arg4,class T_arg5,class T_arg6,class T_arg7, class T_return, class T_obj>
inline const_volatile_mem_functor7<T_return, T_obj, T_arg1,T_arg2,T_arg3,T_arg4,T_arg5,T_arg6,T_arg7>
mem_fun7(T_return (T_obj::*_A_func)(T_arg1,T_arg2,T_arg3,T_arg4,T_arg5,T_arg6,T_arg7) const volatile)
{ return const_volatile_mem_functor7<T_return, T_obj, T_arg1,T_arg2,T_arg3,T_arg4,T_arg5,T_arg6,T_arg7>(_A_func); }

/** Creates a functor of type sigc::bound_mem_functor0 which encapsulates a method and an object instance.
 * @param _A_obj Pointer to object instance the functor should operate on.
 * @param _A_func Pointer to method that should be wrapped.
 * @return Functor that executes @e _A_func on invokation.
 *
 * @ingroup mem_fun
 */
template <class T_return, class T_obj, class T_obj2>
inline bound_mem_functor0<T_return, T_obj>
mem_fun0(/**/ T_obj* _A_obj, T_return (T_obj2::*_A_func)() )
{ return bound_mem_functor0<T_return, T_obj>(_A_obj, _A_func); }

/** Creates a functor of type sigc::bound_mem_functor0 which encapsulates a method and an object instance.
 * @param _A_obj Reference to object instance the functor should operate on.
 * @param _A_func Pointer to method that should be wrapped.
 * @return Functor that executes @e _A_func on invokation.
 *
 * @ingroup mem_fun
 */
template <class T_return, class T_obj, class T_obj2>
inline bound_mem_functor0<T_return, T_obj>
mem_fun0(/**/ T_obj& _A_obj, T_return (T_obj2::*_A_func)() )
{ return bound_mem_functor0<T_return, T_obj>(_A_obj, _A_func); }

/** Creates a functor of type sigc::bound_mem_functor1 which encapsulates a method and an object instance.
 * @param _A_obj Pointer to object instance the functor should operate on.
 * @param _A_func Pointer to method that should be wrapped.
 * @return Functor that executes @e _A_func on invokation.
 *
 * @ingroup mem_fun
 */
template <class T_arg1, class T_return, class T_obj, class T_obj2>
inline bound_mem_functor1<T_return, T_obj, T_arg1>
mem_fun1(/**/ T_obj* _A_obj, T_return (T_obj2::*_A_func)(T_arg1) )
{ return bound_mem_functor1<T_return, T_obj, T_arg1>(_A_obj, _A_func); }

/** Creates a functor of type sigc::bound_mem_functor1 which encapsulates a method and an object instance.
 * @param _A_obj Reference to object instance the functor should operate on.
 * @param _A_func Pointer to method that should be wrapped.
 * @return Functor that executes @e _A_func on invokation.
 *
 * @ingroup mem_fun
 */
template <class T_arg1, class T_return, class T_obj, class T_obj2>
inline bound_mem_functor1<T_return, T_obj, T_arg1>
mem_fun1(/**/ T_obj& _A_obj, T_return (T_obj2::*_A_func)(T_arg1) )
{ return bound_mem_functor1<T_return, T_obj, T_arg1>(_A_obj, _A_func); }

/** Creates a functor of type sigc::bound_mem_functor2 which encapsulates a method and an object instance.
 * @param _A_obj Pointer to object instance the functor should operate on.
 * @param _A_func Pointer to method that should be wrapped.
 * @return Functor that executes @e _A_func on invokation.
 *
 * @ingroup mem_fun
 */
template <class T_arg1,class T_arg2, class T_return, class T_obj, class T_obj2>
inline bound_mem_functor2<T_return, T_obj, T_arg1,T_arg2>
mem_fun2(/**/ T_obj* _A_obj, T_return (T_obj2::*_A_func)(T_arg1,T_arg2) )
{ return bound_mem_functor2<T_return, T_obj, T_arg1,T_arg2>(_A_obj, _A_func); }

/** Creates a functor of type sigc::bound_mem_functor2 which encapsulates a method and an object instance.
 * @param _A_obj Reference to object instance the functor should operate on.
 * @param _A_func Pointer to method that should be wrapped.
 * @return Functor that executes @e _A_func on invokation.
 *
 * @ingroup mem_fun
 */
template <class T_arg1,class T_arg2, class T_return, class T_obj, class T_obj2>
inline bound_mem_functor2<T_return, T_obj, T_arg1,T_arg2>
mem_fun2(/**/ T_obj& _A_obj, T_return (T_obj2::*_A_func)(T_arg1,T_arg2) )
{ return bound_mem_functor2<T_return, T_obj, T_arg1,T_arg2>(_A_obj, _A_func); }

/** Creates a functor of type sigc::bound_mem_functor3 which encapsulates a method and an object instance.
 * @param _A_obj Pointer to object instance the functor should operate on.
 * @param _A_func Pointer to method that should be wrapped.
 * @return Functor that executes @e _A_func on invokation.
 *
 * @ingroup mem_fun
 */
template <class T_arg1,class T_arg2,class T_arg3, class T_return, class T_obj, class T_obj2>
inline bound_mem_functor3<T_return, T_obj, T_arg1,T_arg2,T_arg3>
mem_fun3(/**/ T_obj* _A_obj, T_return (T_obj2::*_A_func)(T_arg1,T_arg2,T_arg3) )
{ return bound_mem_functor3<T_return, T_obj, T_arg1,T_arg2,T_arg3>(_A_obj, _A_func); }

/** Creates a functor of type sigc::bound_mem_functor3 which encapsulates a method and an object instance.
 * @param _A_obj Reference to object instance the functor should operate on.
 * @param _A_func Pointer to method that should be wrapped.
 * @return Functor that executes @e _A_func on invokation.
 *
 * @ingroup mem_fun
 */
template <class T_arg1,class T_arg2,class T_arg3, class T_return, class T_obj, class T_obj2>
inline bound_mem_functor3<T_return, T_obj, T_arg1,T_arg2,T_arg3>
mem_fun3(/**/ T_obj& _A_obj, T_return (T_obj2::*_A_func)(T_arg1,T_arg2,T_arg3) )
{ return bound_mem_functor3<T_return, T_obj, T_arg1,T_arg2,T_arg3>(_A_obj, _A_func); }

/** Creates a functor of type sigc::bound_mem_functor4 which encapsulates a method and an object instance.
 * @param _A_obj Pointer to object instance the functor should operate on.
 * @param _A_func Pointer to method that should be wrapped.
 * @return Functor that executes @e _A_func on invokation.
 *
 * @ingroup mem_fun
 */
template <class T_arg1,class T_arg2,class T_arg3,class T_arg4, class T_return, class T_obj, class T_obj2>
inline bound_mem_functor4<T_return, T_obj, T_arg1,T_arg2,T_arg3,T_arg4>
mem_fun4(/**/ T_obj* _A_obj, T_return (T_obj2::*_A_func)(T_arg1,T_arg2,T_arg3,T_arg4) )
{ return bound_mem_functor4<T_return, T_obj, T_arg1,T_arg2,T_arg3,T_arg4>(_A_obj, _A_func); }

/** Creates a functor of type sigc::bound_mem_functor4 which encapsulates a method and an object instance.
 * @param _A_obj Reference to object instance the functor should operate on.
 * @param _A_func Pointer to method that should be wrapped.
 * @return Functor that executes @e _A_func on invokation.
 *
 * @ingroup mem_fun
 */
template <class T_arg1,class T_arg2,class T_arg3,class T_arg4, class T_return, class T_obj, class T_obj2>
inline bound_mem_functor4<T_return, T_obj, T_arg1,T_arg2,T_arg3,T_arg4>
mem_fun4(/**/ T_obj& _A_obj, T_return (T_obj2::*_A_func)(T_arg1,T_arg2,T_arg3,T_arg4) )
{ return bound_mem_functor4<T_return, T_obj, T_arg1,T_arg2,T_arg3,T_arg4>(_A_obj, _A_func); }

/** Creates a functor of type sigc::bound_mem_functor5 which encapsulates a method and an object instance.
 * @param _A_obj Pointer to object instance the functor should operate on.
 * @param _A_func Pointer to method that should be wrapped.
 * @return Functor that executes @e _A_func on invokation.
 *
 * @ingroup mem_fun
 */
template <class T_arg1,class T_arg2,class T_arg3,class T_arg4,class T_arg5, class T_return, class T_obj, class T_obj2>
inline bound_mem_functor5<T_return, T_obj, T_arg1,T_arg2,T_arg3,T_arg4,T_arg5>
mem_fun5(/**/ T_obj* _A_obj, T_return (T_obj2::*_A_func)(T_arg1,T_arg2,T_arg3,T_arg4,T_arg5) )
{ return bound_mem_functor5<T_return, T_obj, T_arg1,T_arg2,T_arg3,T_arg4,T_arg5>(_A_obj, _A_func); }

/** Creates a functor of type sigc::bound_mem_functor5 which encapsulates a method and an object instance.
 * @param _A_obj Reference to object instance the functor should operate on.
 * @param _A_func Pointer to method that should be wrapped.
 * @return Functor that executes @e _A_func on invokation.
 *
 * @ingroup mem_fun
 */
template <class T_arg1,class T_arg2,class T_arg3,class T_arg4,class T_arg5, class T_return, class T_obj, class T_obj2>
inline bound_mem_functor5<T_return, T_obj, T_arg1,T_arg2,T_arg3,T_arg4,T_arg5>
mem_fun5(/**/ T_obj& _A_obj, T_return (T_obj2::*_A_func)(T_arg1,T_arg2,T_arg3,T_arg4,T_arg5) )
{ return bound_mem_functor5<T_return, T_obj, T_arg1,T_arg2,T_arg3,T_arg4,T_arg5>(_A_obj, _A_func); }

/** Creates a functor of type sigc::bound_mem_functor6 which encapsulates a method and an object instance.
 * @param _A_obj Pointer to object instance the functor should operate on.
 * @param _A_func Pointer to method that should be wrapped.
 * @return Functor that executes @e _A_func on invokation.
 *
 * @ingroup mem_fun
 */
template <class T_arg1,class T_arg2,class T_arg3,class T_arg4,class T_arg5,class T_arg6, class T_return, class T_obj, class T_obj2>
inline bound_mem_functor6<T_return, T_obj, T_arg1,T_arg2,T_arg3,T_arg4,T_arg5,T_arg6>
mem_fun6(/**/ T_obj* _A_obj, T_return (T_obj2::*_A_func)(T_arg1,T_arg2,T_arg3,T_arg4,T_arg5,T_arg6) )
{ return bound_mem_functor6<T_return, T_obj, T_arg1,T_arg2,T_arg3,T_arg4,T_arg5,T_arg6>(_A_obj, _A_func); }

/** Creates a functor of type sigc::bound_mem_functor6 which encapsulates a method and an object instance.
 * @param _A_obj Reference to object instance the functor should operate on.
 * @param _A_func Pointer to method that should be wrapped.
 * @return Functor that executes @e _A_func on invokation.
 *
 * @ingroup mem_fun
 */
template <class T_arg1,class T_arg2,class T_arg3,class T_arg4,class T_arg5,class T_arg6, class T_return, class T_obj, class T_obj2>
inline bound_mem_functor6<T_return, T_obj, T_arg1,T_arg2,T_arg3,T_arg4,T_arg5,T_arg6>
mem_fun6(/**/ T_obj& _A_obj, T_return (T_obj2::*_A_func)(T_arg1,T_arg2,T_arg3,T_arg4,T_arg5,T_arg6) )
{ return bound_mem_functor6<T_return, T_obj, T_arg1,T_arg2,T_arg3,T_arg4,T_arg5,T_arg6>(_A_obj, _A_func); }

/** Creates a functor of type sigc::bound_mem_functor7 which encapsulates a method and an object instance.
 * @param _A_obj Pointer to object instance the functor should operate on.
 * @param _A_func Pointer to method that should be wrapped.
 * @return Functor that executes @e _A_func on invokation.
 *
 * @ingroup mem_fun
 */
template <class T_arg1,class T_arg2,class T_arg3,class T_arg4,class T_arg5,class T_arg6,class T_arg7, class T_return, class T_obj, class T_obj2>
inline bound_mem_functor7<T_return, T_obj, T_arg1,T_arg2,T_arg3,T_arg4,T_arg5,T_arg6,T_arg7>
mem_fun7(/**/ T_obj* _A_obj, T_return (T_obj2::*_A_func)(T_arg1,T_arg2,T_arg3,T_arg4,T_arg5,T_arg6,T_arg7) )
{ return bound_mem_functor7<T_return, T_obj, T_arg1,T_arg2,T_arg3,T_arg4,T_arg5,T_arg6,T_arg7>(_A_obj, _A_func); }

/** Creates a functor of type sigc::bound_mem_functor7 which encapsulates a method and an object instance.
 * @param _A_obj Reference to object instance the functor should operate on.
 * @param _A_func Pointer to method that should be wrapped.
 * @return Functor that executes @e _A_func on invokation.
 *
 * @ingroup mem_fun
 */
template <class T_arg1,class T_arg2,class T_arg3,class T_arg4,class T_arg5,class T_arg6,class T_arg7, class T_return, class T_obj, class T_obj2>
inline bound_mem_functor7<T_return, T_obj, T_arg1,T_arg2,T_arg3,T_arg4,T_arg5,T_arg6,T_arg7>
mem_fun7(/**/ T_obj& _A_obj, T_return (T_obj2::*_A_func)(T_arg1,T_arg2,T_arg3,T_arg4,T_arg5,T_arg6,T_arg7) )
{ return bound_mem_functor7<T_return, T_obj, T_arg1,T_arg2,T_arg3,T_arg4,T_arg5,T_arg6,T_arg7>(_A_obj, _A_func); }

/** Creates a functor of type sigc::bound_const_mem_functor0 which encapsulates a method and an object instance.
 * @param _A_obj Pointer to object instance the functor should operate on.
 * @param _A_func Pointer to method that should be wrapped.
 * @return Functor that executes @e _A_func on invokation.
 *
 * @ingroup mem_fun
 */
template <class T_return, class T_obj, class T_obj2>
inline bound_const_mem_functor0<T_return, T_obj>
mem_fun0(/*const*/ T_obj* _A_obj, T_return (T_obj2::*_A_func)() const)
{ return bound_const_mem_functor0<T_return, T_obj>(_A_obj, _A_func); }

/** Creates a functor of type sigc::bound_const_mem_functor0 which encapsulates a method and an object instance.
 * @param _A_obj Reference to object instance the functor should operate on.
 * @param _A_func Pointer to method that should be wrapped.
 * @return Functor that executes @e _A_func on invokation.
 *
 * @ingroup mem_fun
 */
template <class T_return, class T_obj, class T_obj2>
inline bound_const_mem_functor0<T_return, T_obj>
mem_fun0(/*const*/ T_obj& _A_obj, T_return (T_obj2::*_A_func)() const)
{ return bound_const_mem_functor0<T_return, T_obj>(_A_obj, _A_func); }

/** Creates a functor of type sigc::bound_const_mem_functor1 which encapsulates a method and an object instance.
 * @param _A_obj Pointer to object instance the functor should operate on.
 * @param _A_func Pointer to method that should be wrapped.
 * @return Functor that executes @e _A_func on invokation.
 *
 * @ingroup mem_fun
 */
template <class T_arg1, class T_return, class T_obj, class T_obj2>
inline bound_const_mem_functor1<T_return, T_obj, T_arg1>
mem_fun1(/*const*/ T_obj* _A_obj, T_return (T_obj2::*_A_func)(T_arg1) const)
{ return bound_const_mem_functor1<T_return, T_obj, T_arg1>(_A_obj, _A_func); }

/** Creates a functor of type sigc::bound_const_mem_functor1 which encapsulates a method and an object instance.
 * @param _A_obj Reference to object instance the functor should operate on.
 * @param _A_func Pointer to method that should be wrapped.
 * @return Functor that executes @e _A_func on invokation.
 *
 * @ingroup mem_fun
 */
template <class T_arg1, class T_return, class T_obj, class T_obj2>
inline bound_const_mem_functor1<T_return, T_obj, T_arg1>
mem_fun1(/*const*/ T_obj& _A_obj, T_return (T_obj2::*_A_func)(T_arg1) const)
{ return bound_const_mem_functor1<T_return, T_obj, T_arg1>(_A_obj, _A_func); }

/** Creates a functor of type sigc::bound_const_mem_functor2 which encapsulates a method and an object instance.
 * @param _A_obj Pointer to object instance the functor should operate on.
 * @param _A_func Pointer to method that should be wrapped.
 * @return Functor that executes @e _A_func on invokation.
 *
 * @ingroup mem_fun
 */
template <class T_arg1,class T_arg2, class T_return, class T_obj, class T_obj2>
inline bound_const_mem_functor2<T_return, T_obj, T_arg1,T_arg2>
mem_fun2(/*const*/ T_obj* _A_obj, T_return (T_obj2::*_A_func)(T_arg1,T_arg2) const)
{ return bound_const_mem_functor2<T_return, T_obj, T_arg1,T_arg2>(_A_obj, _A_func); }

/** Creates a functor of type sigc::bound_const_mem_functor2 which encapsulates a method and an object instance.
 * @param _A_obj Reference to object instance the functor should operate on.
 * @param _A_func Pointer to method that should be wrapped.
 * @return Functor that executes @e _A_func on invokation.
 *
 * @ingroup mem_fun
 */
template <class T_arg1,class T_arg2, class T_return, class T_obj, class T_obj2>
inline bound_const_mem_functor2<T_return, T_obj, T_arg1,T_arg2>
mem_fun2(/*const*/ T_obj& _A_obj, T_return (T_obj2::*_A_func)(T_arg1,T_arg2) const)
{ return bound_const_mem_functor2<T_return, T_obj, T_arg1,T_arg2>(_A_obj, _A_func); }

/** Creates a functor of type sigc::bound_const_mem_functor3 which encapsulates a method and an object instance.
 * @param _A_obj Pointer to object instance the functor should operate on.
 * @param _A_func Pointer to method that should be wrapped.
 * @return Functor that executes @e _A_func on invokation.
 *
 * @ingroup mem_fun
 */
template <class T_arg1,class T_arg2,class T_arg3, class T_return, class T_obj, class T_obj2>
inline bound_const_mem_functor3<T_return, T_obj, T_arg1,T_arg2,T_arg3>
mem_fun3(/*const*/ T_obj* _A_obj, T_return (T_obj2::*_A_func)(T_arg1,T_arg2,T_arg3) const)
{ return bound_const_mem_functor3<T_return, T_obj, T_arg1,T_arg2,T_arg3>(_A_obj, _A_func); }

/** Creates a functor of type sigc::bound_const_mem_functor3 which encapsulates a method and an object instance.
 * @param _A_obj Reference to object instance the functor should operate on.
 * @param _A_func Pointer to method that should be wrapped.
 * @return Functor that executes @e _A_func on invokation.
 *
 * @ingroup mem_fun
 */
template <class T_arg1,class T_arg2,class T_arg3, class T_return, class T_obj, class T_obj2>
inline bound_const_mem_functor3<T_return, T_obj, T_arg1,T_arg2,T_arg3>
mem_fun3(/*const*/ T_obj& _A_obj, T_return (T_obj2::*_A_func)(T_arg1,T_arg2,T_arg3) const)
{ return bound_const_mem_functor3<T_return, T_obj, T_arg1,T_arg2,T_arg3>(_A_obj, _A_func); }

/** Creates a functor of type sigc::bound_const_mem_functor4 which encapsulates a method and an object instance.
 * @param _A_obj Pointer to object instance the functor should operate on.
 * @param _A_func Pointer to method that should be wrapped.
 * @return Functor that executes @e _A_func on invokation.
 *
 * @ingroup mem_fun
 */
template <class T_arg1,class T_arg2,class T_arg3,class T_arg4, class T_return, class T_obj, class T_obj2>
inline bound_const_mem_functor4<T_return, T_obj, T_arg1,T_arg2,T_arg3,T_arg4>
mem_fun4(/*const*/ T_obj* _A_obj, T_return (T_obj2::*_A_func)(T_arg1,T_arg2,T_arg3,T_arg4) const)
{ return bound_const_mem_functor4<T_return, T_obj, T_arg1,T_arg2,T_arg3,T_arg4>(_A_obj, _A_func); }

/** Creates a functor of type sigc::bound_const_mem_functor4 which encapsulates a method and an object instance.
 * @param _A_obj Reference to object instance the functor should operate on.
 * @param _A_func Pointer to method that should be wrapped.
 * @return Functor that executes @e _A_func on invokation.
 *
 * @ingroup mem_fun
 */
template <class T_arg1,class T_arg2,class T_arg3,class T_arg4, class T_return, class T_obj, class T_obj2>
inline bound_const_mem_functor4<T_return, T_obj, T_arg1,T_arg2,T_arg3,T_arg4>
mem_fun4(/*const*/ T_obj& _A_obj, T_return (T_obj2::*_A_func)(T_arg1,T_arg2,T_arg3,T_arg4) const)
{ return bound_const_mem_functor4<T_return, T_obj, T_arg1,T_arg2,T_arg3,T_arg4>(_A_obj, _A_func); }

/** Creates a functor of type sigc::bound_const_mem_functor5 which encapsulates a method and an object instance.
 * @param _A_obj Pointer to object instance the functor should operate on.
 * @param _A_func Pointer to method that should be wrapped.
 * @return Functor that executes @e _A_func on invokation.
 *
 * @ingroup mem_fun
 */
template <class T_arg1,class T_arg2,class T_arg3,class T_arg4,class T_arg5, class T_return, class T_obj, class T_obj2>
inline bound_const_mem_functor5<T_return, T_obj, T_arg1,T_arg2,T_arg3,T_arg4,T_arg5>
mem_fun5(/*const*/ T_obj* _A_obj, T_return (T_obj2::*_A_func)(T_arg1,T_arg2,T_arg3,T_arg4,T_arg5) const)
{ return bound_const_mem_functor5<T_return, T_obj, T_arg1,T_arg2,T_arg3,T_arg4,T_arg5>(_A_obj, _A_func); }

/** Creates a functor of type sigc::bound_const_mem_functor5 which encapsulates a method and an object instance.
 * @param _A_obj Reference to object instance the functor should operate on.
 * @param _A_func Pointer to method that should be wrapped.
 * @return Functor that executes @e _A_func on invokation.
 *
 * @ingroup mem_fun
 */
template <class T_arg1,class T_arg2,class T_arg3,class T_arg4,class T_arg5, class T_return, class T_obj, class T_obj2>
inline bound_const_mem_functor5<T_return, T_obj, T_arg1,T_arg2,T_arg3,T_arg4,T_arg5>
mem_fun5(/*const*/ T_obj& _A_obj, T_return (T_obj2::*_A_func)(T_arg1,T_arg2,T_arg3,T_arg4,T_arg5) const)
{ return bound_const_mem_functor5<T_return, T_obj, T_arg1,T_arg2,T_arg3,T_arg4,T_arg5>(_A_obj, _A_func); }

/** Creates a functor of type sigc::bound_const_mem_functor6 which encapsulates a method and an object instance.
 * @param _A_obj Pointer to object instance the functor should operate on.
 * @param _A_func Pointer to method that should be wrapped.
 * @return Functor that executes @e _A_func on invokation.
 *
 * @ingroup mem_fun
 */
template <class T_arg1,class T_arg2,class T_arg3,class T_arg4,class T_arg5,class T_arg6, class T_return, class T_obj, class T_obj2>
inline bound_const_mem_functor6<T_return, T_obj, T_arg1,T_arg2,T_arg3,T_arg4,T_arg5,T_arg6>
mem_fun6(/*const*/ T_obj* _A_obj, T_return (T_obj2::*_A_func)(T_arg1,T_arg2,T_arg3,T_arg4,T_arg5,T_arg6) const)
{ return bound_const_mem_functor6<T_return, T_obj, T_arg1,T_arg2,T_arg3,T_arg4,T_arg5,T_arg6>(_A_obj, _A_func); }

/** Creates a functor of type sigc::bound_const_mem_functor6 which encapsulates a method and an object instance.
 * @param _A_obj Reference to object instance the functor should operate on.
 * @param _A_func Pointer to method that should be wrapped.
 * @return Functor that executes @e _A_func on invokation.
 *
 * @ingroup mem_fun
 */
template <class T_arg1,class T_arg2,class T_arg3,class T_arg4,class T_arg5,class T_arg6, class T_return, class T_obj, class T_obj2>
inline bound_const_mem_functor6<T_return, T_obj, T_arg1,T_arg2,T_arg3,T_arg4,T_arg5,T_arg6>
mem_fun6(/*const*/ T_obj& _A_obj, T_return (T_obj2::*_A_func)(T_arg1,T_arg2,T_arg3,T_arg4,T_arg5,T_arg6) const)
{ return bound_const_mem_functor6<T_return, T_obj, T_arg1,T_arg2,T_arg3,T_arg4,T_arg5,T_arg6>(_A_obj, _A_func); }

/** Creates a functor of type sigc::bound_const_mem_functor7 which encapsulates a method and an object instance.
 * @param _A_obj Pointer to object instance the functor should operate on.
 * @param _A_func Pointer to method that should be wrapped.
 * @return Functor that executes @e _A_func on invokation.
 *
 * @ingroup mem_fun
 */
template <class T_arg1,class T_arg2,class T_arg3,class T_arg4,class T_arg5,class T_arg6,class T_arg7, class T_return, class T_obj, class T_obj2>
inline bound_const_mem_functor7<T_return, T_obj, T_arg1,T_arg2,T_arg3,T_arg4,T_arg5,T_arg6,T_arg7>
mem_fun7(/*const*/ T_obj* _A_obj, T_return (T_obj2::*_A_func)(T_arg1,T_arg2,T_arg3,T_arg4,T_arg5,T_arg6,T_arg7) const)
{ return bound_const_mem_functor7<T_return, T_obj, T_arg1,T_arg2,T_arg3,T_arg4,T_arg5,T_arg6,T_arg7>(_A_obj, _A_func); }

/** Creates a functor of type sigc::bound_const_mem_functor7 which encapsulates a method and an object instance.
 * @param _A_obj Reference to object instance the functor should operate on.
 * @param _A_func Pointer to method that should be wrapped.
 * @return Functor that executes @e _A_func on invokation.
 *
 * @ingroup mem_fun
 */
template <class T_arg1,class T_arg2,class T_arg3,class T_arg4,class T_arg5,class T_arg6,class T_arg7, class T_return, class T_obj, class T_obj2>
inline bound_const_mem_functor7<T_return, T_obj, T_arg1,T_arg2,T_arg3,T_arg4,T_arg5,T_arg6,T_arg7>
mem_fun7(/*const*/ T_obj& _A_obj, T_return (T_obj2::*_A_func)(T_arg1,T_arg2,T_arg3,T_arg4,T_arg5,T_arg6,T_arg7) const)
{ return bound_const_mem_functor7<T_return, T_obj, T_arg1,T_arg2,T_arg3,T_arg4,T_arg5,T_arg6,T_arg7>(_A_obj, _A_func); }

/** Creates a functor of type sigc::bound_volatile_mem_functor0 which encapsulates a method and an object instance.
 * @param _A_obj Pointer to object instance the functor should operate on.
 * @param _A_func Pointer to method that should be wrapped.
 * @return Functor that executes @e _A_func on invokation.
 *
 * @ingroup mem_fun
 */
template <class T_return, class T_obj, class T_obj2>
inline bound_volatile_mem_functor0<T_return, T_obj>
mem_fun0(/**/ T_obj* _A_obj, T_return (T_obj2::*_A_func)() volatile)
{ return bound_volatile_mem_functor0<T_return, T_obj>(_A_obj, _A_func); }

/** Creates a functor of type sigc::bound_volatile_mem_functor0 which encapsulates a method and an object instance.
 * @param _A_obj Reference to object instance the functor should operate on.
 * @param _A_func Pointer to method that should be wrapped.
 * @return Functor that executes @e _A_func on invokation.
 *
 * @ingroup mem_fun
 */
template <class T_return, class T_obj, class T_obj2>
inline bound_volatile_mem_functor0<T_return, T_obj>
mem_fun0(/**/ T_obj& _A_obj, T_return (T_obj2::*_A_func)() volatile)
{ return bound_volatile_mem_functor0<T_return, T_obj>(_A_obj, _A_func); }

/** Creates a functor of type sigc::bound_volatile_mem_functor1 which encapsulates a method and an object instance.
 * @param _A_obj Pointer to object instance the functor should operate on.
 * @param _A_func Pointer to method that should be wrapped.
 * @return Functor that executes @e _A_func on invokation.
 *
 * @ingroup mem_fun
 */
template <class T_arg1, class T_return, class T_obj, class T_obj2>
inline bound_volatile_mem_functor1<T_return, T_obj, T_arg1>
mem_fun1(/**/ T_obj* _A_obj, T_return (T_obj2::*_A_func)(T_arg1) volatile)
{ return bound_volatile_mem_functor1<T_return, T_obj, T_arg1>(_A_obj, _A_func); }

/** Creates a functor of type sigc::bound_volatile_mem_functor1 which encapsulates a method and an object instance.
 * @param _A_obj Reference to object instance the functor should operate on.
 * @param _A_func Pointer to method that should be wrapped.
 * @return Functor that executes @e _A_func on invokation.
 *
 * @ingroup mem_fun
 */
template <class T_arg1, class T_return, class T_obj, class T_obj2>
inline bound_volatile_mem_functor1<T_return, T_obj, T_arg1>
mem_fun1(/**/ T_obj& _A_obj, T_return (T_obj2::*_A_func)(T_arg1) volatile)
{ return bound_volatile_mem_functor1<T_return, T_obj, T_arg1>(_A_obj, _A_func); }

/** Creates a functor of type sigc::bound_volatile_mem_functor2 which encapsulates a method and an object instance.
 * @param _A_obj Pointer to object instance the functor should operate on.
 * @param _A_func Pointer to method that should be wrapped.
 * @return Functor that executes @e _A_func on invokation.
 *
 * @ingroup mem_fun
 */
template <class T_arg1,class T_arg2, class T_return, class T_obj, class T_obj2>
inline bound_volatile_mem_functor2<T_return, T_obj, T_arg1,T_arg2>
mem_fun2(/**/ T_obj* _A_obj, T_return (T_obj2::*_A_func)(T_arg1,T_arg2) volatile)
{ return bound_volatile_mem_functor2<T_return, T_obj, T_arg1,T_arg2>(_A_obj, _A_func); }

/** Creates a functor of type sigc::bound_volatile_mem_functor2 which encapsulates a method and an object instance.
 * @param _A_obj Reference to object instance the functor should operate on.
 * @param _A_func Pointer to method that should be wrapped.
 * @return Functor that executes @e _A_func on invokation.
 *
 * @ingroup mem_fun
 */
template <class T_arg1,class T_arg2, class T_return, class T_obj, class T_obj2>
inline bound_volatile_mem_functor2<T_return, T_obj, T_arg1,T_arg2>
mem_fun2(/**/ T_obj& _A_obj, T_return (T_obj2::*_A_func)(T_arg1,T_arg2) volatile)
{ return bound_volatile_mem_functor2<T_return, T_obj, T_arg1,T_arg2>(_A_obj, _A_func); }

/** Creates a functor of type sigc::bound_volatile_mem_functor3 which encapsulates a method and an object instance.
 * @param _A_obj Pointer to object instance the functor should operate on.
 * @param _A_func Pointer to method that should be wrapped.
 * @return Functor that executes @e _A_func on invokation.
 *
 * @ingroup mem_fun
 */
template <class T_arg1,class T_arg2,class T_arg3, class T_return, class T_obj, class T_obj2>
inline bound_volatile_mem_functor3<T_return, T_obj, T_arg1,T_arg2,T_arg3>
mem_fun3(/**/ T_obj* _A_obj, T_return (T_obj2::*_A_func)(T_arg1,T_arg2,T_arg3) volatile)
{ return bound_volatile_mem_functor3<T_return, T_obj, T_arg1,T_arg2,T_arg3>(_A_obj, _A_func); }

/** Creates a functor of type sigc::bound_volatile_mem_functor3 which encapsulates a method and an object instance.
 * @param _A_obj Reference to object instance the functor should operate on.
 * @param _A_func Pointer to method that should be wrapped.
 * @return Functor that executes @e _A_func on invokation.
 *
 * @ingroup mem_fun
 */
template <class T_arg1,class T_arg2,class T_arg3, class T_return, class T_obj, class T_obj2>
inline bound_volatile_mem_functor3<T_return, T_obj, T_arg1,T_arg2,T_arg3>
mem_fun3(/**/ T_obj& _A_obj, T_return (T_obj2::*_A_func)(T_arg1,T_arg2,T_arg3) volatile)
{ return bound_volatile_mem_functor3<T_return, T_obj, T_arg1,T_arg2,T_arg3>(_A_obj, _A_func); }

/** Creates a functor of type sigc::bound_volatile_mem_functor4 which encapsulates a method and an object instance.
 * @param _A_obj Pointer to object instance the functor should operate on.
 * @param _A_func Pointer to method that should be wrapped.
 * @return Functor that executes @e _A_func on invokation.
 *
 * @ingroup mem_fun
 */
template <class T_arg1,class T_arg2,class T_arg3,class T_arg4, class T_return, class T_obj, class T_obj2>
inline bound_volatile_mem_functor4<T_return, T_obj, T_arg1,T_arg2,T_arg3,T_arg4>
mem_fun4(/**/ T_obj* _A_obj, T_return (T_obj2::*_A_func)(T_arg1,T_arg2,T_arg3,T_arg4) volatile)
{ return bound_volatile_mem_functor4<T_return, T_obj, T_arg1,T_arg2,T_arg3,T_arg4>(_A_obj, _A_func); }

/** Creates a functor of type sigc::bound_volatile_mem_functor4 which encapsulates a method and an object instance.
 * @param _A_obj Reference to object instance the functor should operate on.
 * @param _A_func Pointer to method that should be wrapped.
 * @return Functor that executes @e _A_func on invokation.
 *
 * @ingroup mem_fun
 */
template <class T_arg1,class T_arg2,class T_arg3,class T_arg4, class T_return, class T_obj, class T_obj2>
inline bound_volatile_mem_functor4<T_return, T_obj, T_arg1,T_arg2,T_arg3,T_arg4>
mem_fun4(/**/ T_obj& _A_obj, T_return (T_obj2::*_A_func)(T_arg1,T_arg2,T_arg3,T_arg4) volatile)
{ return bound_volatile_mem_functor4<T_return, T_obj, T_arg1,T_arg2,T_arg3,T_arg4>(_A_obj, _A_func); }

/** Creates a functor of type sigc::bound_volatile_mem_functor5 which encapsulates a method and an object instance.
 * @param _A_obj Pointer to object instance the functor should operate on.
 * @param _A_func Pointer to method that should be wrapped.
 * @return Functor that executes @e _A_func on invokation.
 *
 * @ingroup mem_fun
 */
template <class T_arg1,class T_arg2,class T_arg3,class T_arg4,class T_arg5, class T_return, class T_obj, class T_obj2>
inline bound_volatile_mem_functor5<T_return, T_obj, T_arg1,T_arg2,T_arg3,T_arg4,T_arg5>
mem_fun5(/**/ T_obj* _A_obj, T_return (T_obj2::*_A_func)(T_arg1,T_arg2,T_arg3,T_arg4,T_arg5) volatile)
{ return bound_volatile_mem_functor5<T_return, T_obj, T_arg1,T_arg2,T_arg3,T_arg4,T_arg5>(_A_obj, _A_func); }

/** Creates a functor of type sigc::bound_volatile_mem_functor5 which encapsulates a method and an object instance.
 * @param _A_obj Reference to object instance the functor should operate on.
 * @param _A_func Pointer to method that should be wrapped.
 * @return Functor that executes @e _A_func on invokation.
 *
 * @ingroup mem_fun
 */
template <class T_arg1,class T_arg2,class T_arg3,class T_arg4,class T_arg5, class T_return, class T_obj, class T_obj2>
inline bound_volatile_mem_functor5<T_return, T_obj, T_arg1,T_arg2,T_arg3,T_arg4,T_arg5>
mem_fun5(/**/ T_obj& _A_obj, T_return (T_obj2::*_A_func)(T_arg1,T_arg2,T_arg3,T_arg4,T_arg5) volatile)
{ return bound_volatile_mem_functor5<T_return, T_obj, T_arg1,T_arg2,T_arg3,T_arg4,T_arg5>(_A_obj, _A_func); }

/** Creates a functor of type sigc::bound_volatile_mem_functor6 which encapsulates a method and an object instance.
 * @param _A_obj Pointer to object instance the functor should operate on.
 * @param _A_func Pointer to method that should be wrapped.
 * @return Functor that executes @e _A_func on invokation.
 *
 * @ingroup mem_fun
 */
template <class T_arg1,class T_arg2,class T_arg3,class T_arg4,class T_arg5,class T_arg6, class T_return, class T_obj, class T_obj2>
inline bound_volatile_mem_functor6<T_return, T_obj, T_arg1,T_arg2,T_arg3,T_arg4,T_arg5,T_arg6>
mem_fun6(/**/ T_obj* _A_obj, T_return (T_obj2::*_A_func)(T_arg1,T_arg2,T_arg3,T_arg4,T_arg5,T_arg6) volatile)
{ return bound_volatile_mem_functor6<T_return, T_obj, T_arg1,T_arg2,T_arg3,T_arg4,T_arg5,T_arg6>(_A_obj, _A_func); }

/** Creates a functor of type sigc::bound_volatile_mem_functor6 which encapsulates a method and an object instance.
 * @param _A_obj Reference to object instance the functor should operate on.
 * @param _A_func Pointer to method that should be wrapped.
 * @return Functor that executes @e _A_func on invokation.
 *
 * @ingroup mem_fun
 */
template <class T_arg1,class T_arg2,class T_arg3,class T_arg4,class T_arg5,class T_arg6, class T_return, class T_obj, class T_obj2>
inline bound_volatile_mem_functor6<T_return, T_obj, T_arg1,T_arg2,T_arg3,T_arg4,T_arg5,T_arg6>
mem_fun6(/**/ T_obj& _A_obj, T_return (T_obj2::*_A_func)(T_arg1,T_arg2,T_arg3,T_arg4,T_arg5,T_arg6) volatile)
{ return bound_volatile_mem_functor6<T_return, T_obj, T_arg1,T_arg2,T_arg3,T_arg4,T_arg5,T_arg6>(_A_obj, _A_func); }

/** Creates a functor of type sigc::bound_volatile_mem_functor7 which encapsulates a method and an object instance.
 * @param _A_obj Pointer to object instance the functor should operate on.
 * @param _A_func Pointer to method that should be wrapped.
 * @return Functor that executes @e _A_func on invokation.
 *
 * @ingroup mem_fun
 */
template <class T_arg1,class T_arg2,class T_arg3,class T_arg4,class T_arg5,class T_arg6,class T_arg7, class T_return, class T_obj, class T_obj2>
inline bound_volatile_mem_functor7<T_return, T_obj, T_arg1,T_arg2,T_arg3,T_arg4,T_arg5,T_arg6,T_arg7>
mem_fun7(/**/ T_obj* _A_obj, T_return (T_obj2::*_A_func)(T_arg1,T_arg2,T_arg3,T_arg4,T_arg5,T_arg6,T_arg7) volatile)
{ return bound_volatile_mem_functor7<T_return, T_obj, T_arg1,T_arg2,T_arg3,T_arg4,T_arg5,T_arg6,T_arg7>(_A_obj, _A_func); }

/** Creates a functor of type sigc::bound_volatile_mem_functor7 which encapsulates a method and an object instance.
 * @param _A_obj Reference to object instance the functor should operate on.
 * @param _A_func Pointer to method that should be wrapped.
 * @return Functor that executes @e _A_func on invokation.
 *
 * @ingroup mem_fun
 */
template <class T_arg1,class T_arg2,class T_arg3,class T_arg4,class T_arg5,class T_arg6,class T_arg7, class T_return, class T_obj, class T_obj2>
inline bound_volatile_mem_functor7<T_return, T_obj, T_arg1,T_arg2,T_arg3,T_arg4,T_arg5,T_arg6,T_arg7>
mem_fun7(/**/ T_obj& _A_obj, T_return (T_obj2::*_A_func)(T_arg1,T_arg2,T_arg3,T_arg4,T_arg5,T_arg6,T_arg7) volatile)
{ return bound_volatile_mem_functor7<T_return, T_obj, T_arg1,T_arg2,T_arg3,T_arg4,T_arg5,T_arg6,T_arg7>(_A_obj, _A_func); }

/** Creates a functor of type sigc::bound_const_volatile_mem_functor0 which encapsulates a method and an object instance.
 * @param _A_obj Pointer to object instance the functor should operate on.
 * @param _A_func Pointer to method that should be wrapped.
 * @return Functor that executes @e _A_func on invokation.
 *
 * @ingroup mem_fun
 */
template <class T_return, class T_obj, class T_obj2>
inline bound_const_volatile_mem_functor0<T_return, T_obj>
mem_fun0(/*const*/ T_obj* _A_obj, T_return (T_obj2::*_A_func)() const volatile)
{ return bound_const_volatile_mem_functor0<T_return, T_obj>(_A_obj, _A_func); }

/** Creates a functor of type sigc::bound_const_volatile_mem_functor0 which encapsulates a method and an object instance.
 * @param _A_obj Reference to object instance the functor should operate on.
 * @param _A_func Pointer to method that should be wrapped.
 * @return Functor that executes @e _A_func on invokation.
 *
 * @ingroup mem_fun
 */
template <class T_return, class T_obj, class T_obj2>
inline bound_const_volatile_mem_functor0<T_return, T_obj>
mem_fun0(/*const*/ T_obj& _A_obj, T_return (T_obj2::*_A_func)() const volatile)
{ return bound_const_volatile_mem_functor0<T_return, T_obj>(_A_obj, _A_func); }

/** Creates a functor of type sigc::bound_const_volatile_mem_functor1 which encapsulates a method and an object instance.
 * @param _A_obj Pointer to object instance the functor should operate on.
 * @param _A_func Pointer to method that should be wrapped.
 * @return Functor that executes @e _A_func on invokation.
 *
 * @ingroup mem_fun
 */
template <class T_arg1, class T_return, class T_obj, class T_obj2>
inline bound_const_volatile_mem_functor1<T_return, T_obj, T_arg1>
mem_fun1(/*const*/ T_obj* _A_obj, T_return (T_obj2::*_A_func)(T_arg1) const volatile)
{ return bound_const_volatile_mem_functor1<T_return, T_obj, T_arg1>(_A_obj, _A_func); }

/** Creates a functor of type sigc::bound_const_volatile_mem_functor1 which encapsulates a method and an object instance.
 * @param _A_obj Reference to object instance the functor should operate on.
 * @param _A_func Pointer to method that should be wrapped.
 * @return Functor that executes @e _A_func on invokation.
 *
 * @ingroup mem_fun
 */
template <class T_arg1, class T_return, class T_obj, class T_obj2>
inline bound_const_volatile_mem_functor1<T_return, T_obj, T_arg1>
mem_fun1(/*const*/ T_obj& _A_obj, T_return (T_obj2::*_A_func)(T_arg1) const volatile)
{ return bound_const_volatile_mem_functor1<T_return, T_obj, T_arg1>(_A_obj, _A_func); }

/** Creates a functor of type sigc::bound_const_volatile_mem_functor2 which encapsulates a method and an object instance.
 * @param _A_obj Pointer to object instance the functor should operate on.
 * @param _A_func Pointer to method that should be wrapped.
 * @return Functor that executes @e _A_func on invokation.
 *
 * @ingroup mem_fun
 */
template <class T_arg1,class T_arg2, class T_return, class T_obj, class T_obj2>
inline bound_const_volatile_mem_functor2<T_return, T_obj, T_arg1,T_arg2>
mem_fun2(/*const*/ T_obj* _A_obj, T_return (T_obj2::*_A_func)(T_arg1,T_arg2) const volatile)
{ return bound_const_volatile_mem_functor2<T_return, T_obj, T_arg1,T_arg2>(_A_obj, _A_func); }

/** Creates a functor of type sigc::bound_const_volatile_mem_functor2 which encapsulates a method and an object instance.
 * @param _A_obj Reference to object instance the functor should operate on.
 * @param _A_func Pointer to method that should be wrapped.
 * @return Functor that executes @e _A_func on invokation.
 *
 * @ingroup mem_fun
 */
template <class T_arg1,class T_arg2, class T_return, class T_obj, class T_obj2>
inline bound_const_volatile_mem_functor2<T_return, T_obj, T_arg1,T_arg2>
mem_fun2(/*const*/ T_obj& _A_obj, T_return (T_obj2::*_A_func)(T_arg1,T_arg2) const volatile)
{ return bound_const_volatile_mem_functor2<T_return, T_obj, T_arg1,T_arg2>(_A_obj, _A_func); }

/** Creates a functor of type sigc::bound_const_volatile_mem_functor3 which encapsulates a method and an object instance.
 * @param _A_obj Pointer to object instance the functor should operate on.
 * @param _A_func Pointer to method that should be wrapped.
 * @return Functor that executes @e _A_func on invokation.
 *
 * @ingroup mem_fun
 */
template <class T_arg1,class T_arg2,class T_arg3, class T_return, class T_obj, class T_obj2>
inline bound_const_volatile_mem_functor3<T_return, T_obj, T_arg1,T_arg2,T_arg3>
mem_fun3(/*const*/ T_obj* _A_obj, T_return (T_obj2::*_A_func)(T_arg1,T_arg2,T_arg3) const volatile)
{ return bound_const_volatile_mem_functor3<T_return, T_obj, T_arg1,T_arg2,T_arg3>(_A_obj, _A_func); }

/** Creates a functor of type sigc::bound_const_volatile_mem_functor3 which encapsulates a method and an object instance.
 * @param _A_obj Reference to object instance the functor should operate on.
 * @param _A_func Pointer to method that should be wrapped.
 * @return Functor that executes @e _A_func on invokation.
 *
 * @ingroup mem_fun
 */
template <class T_arg1,class T_arg2,class T_arg3, class T_return, class T_obj, class T_obj2>
inline bound_const_volatile_mem_functor3<T_return, T_obj, T_arg1,T_arg2,T_arg3>
mem_fun3(/*const*/ T_obj& _A_obj, T_return (T_obj2::*_A_func)(T_arg1,T_arg2,T_arg3) const volatile)
{ return bound_const_volatile_mem_functor3<T_return, T_obj, T_arg1,T_arg2,T_arg3>(_A_obj, _A_func); }

/** Creates a functor of type sigc::bound_const_volatile_mem_functor4 which encapsulates a method and an object instance.
 * @param _A_obj Pointer to object instance the functor should operate on.
 * @param _A_func Pointer to method that should be wrapped.
 * @return Functor that executes @e _A_func on invokation.
 *
 * @ingroup mem_fun
 */
template <class T_arg1,class T_arg2,class T_arg3,class T_arg4, class T_return, class T_obj, class T_obj2>
inline bound_const_volatile_mem_functor4<T_return, T_obj, T_arg1,T_arg2,T_arg3,T_arg4>
mem_fun4(/*const*/ T_obj* _A_obj, T_return (T_obj2::*_A_func)(T_arg1,T_arg2,T_arg3,T_arg4) const volatile)
{ return bound_const_volatile_mem_functor4<T_return, T_obj, T_arg1,T_arg2,T_arg3,T_arg4>(_A_obj, _A_func); }

/** Creates a functor of type sigc::bound_const_volatile_mem_functor4 which encapsulates a method and an object instance.
 * @param _A_obj Reference to object instance the functor should operate on.
 * @param _A_func Pointer to method that should be wrapped.
 * @return Functor that executes @e _A_func on invokation.
 *
 * @ingroup mem_fun
 */
template <class T_arg1,class T_arg2,class T_arg3,class T_arg4, class T_return, class T_obj, class T_obj2>
inline bound_const_volatile_mem_functor4<T_return, T_obj, T_arg1,T_arg2,T_arg3,T_arg4>
mem_fun4(/*const*/ T_obj& _A_obj, T_return (T_obj2::*_A_func)(T_arg1,T_arg2,T_arg3,T_arg4) const volatile)
{ return bound_const_volatile_mem_functor4<T_return, T_obj, T_arg1,T_arg2,T_arg3,T_arg4>(_A_obj, _A_func); }

/** Creates a functor of type sigc::bound_const_volatile_mem_functor5 which encapsulates a method and an object instance.
 * @param _A_obj Pointer to object instance the functor should operate on.
 * @param _A_func Pointer to method that should be wrapped.
 * @return Functor that executes @e _A_func on invokation.
 *
 * @ingroup mem_fun
 */
template <class T_arg1,class T_arg2,class T_arg3,class T_arg4,class T_arg5, class T_return, class T_obj, class T_obj2>
inline bound_const_volatile_mem_functor5<T_return, T_obj, T_arg1,T_arg2,T_arg3,T_arg4,T_arg5>
mem_fun5(/*const*/ T_obj* _A_obj, T_return (T_obj2::*_A_func)(T_arg1,T_arg2,T_arg3,T_arg4,T_arg5) const volatile)
{ return bound_const_volatile_mem_functor5<T_return, T_obj, T_arg1,T_arg2,T_arg3,T_arg4,T_arg5>(_A_obj, _A_func); }

/** Creates a functor of type sigc::bound_const_volatile_mem_functor5 which encapsulates a method and an object instance.
 * @param _A_obj Reference to object instance the functor should operate on.
 * @param _A_func Pointer to method that should be wrapped.
 * @return Functor that executes @e _A_func on invokation.
 *
 * @ingroup mem_fun
 */
template <class T_arg1,class T_arg2,class T_arg3,class T_arg4,class T_arg5, class T_return, class T_obj, class T_obj2>
inline bound_const_volatile_mem_functor5<T_return, T_obj, T_arg1,T_arg2,T_arg3,T_arg4,T_arg5>
mem_fun5(/*const*/ T_obj& _A_obj, T_return (T_obj2::*_A_func)(T_arg1,T_arg2,T_arg3,T_arg4,T_arg5) const volatile)
{ return bound_const_volatile_mem_functor5<T_return, T_obj, T_arg1,T_arg2,T_arg3,T_arg4,T_arg5>(_A_obj, _A_func); }

/** Creates a functor of type sigc::bound_const_volatile_mem_functor6 which encapsulates a method and an object instance.
 * @param _A_obj Pointer to object instance the functor should operate on.
 * @param _A_func Pointer to method that should be wrapped.
 * @return Functor that executes @e _A_func on invokation.
 *
 * @ingroup mem_fun
 */
template <class T_arg1,class T_arg2,class T_arg3,class T_arg4,class T_arg5,class T_arg6, class T_return, class T_obj, class T_obj2>
inline bound_const_volatile_mem_functor6<T_return, T_obj, T_arg1,T_arg2,T_arg3,T_arg4,T_arg5,T_arg6>
mem_fun6(/*const*/ T_obj* _A_obj, T_return (T_obj2::*_A_func)(T_arg1,T_arg2,T_arg3,T_arg4,T_arg5,T_arg6) const volatile)
{ return bound_const_volatile_mem_functor6<T_return, T_obj, T_arg1,T_arg2,T_arg3,T_arg4,T_arg5,T_arg6>(_A_obj, _A_func); }

/** Creates a functor of type sigc::bound_const_volatile_mem_functor6 which encapsulates a method and an object instance.
 * @param _A_obj Reference to object instance the functor should operate on.
 * @param _A_func Pointer to method that should be wrapped.
 * @return Functor that executes @e _A_func on invokation.
 *
 * @ingroup mem_fun
 */
template <class T_arg1,class T_arg2,class T_arg3,class T_arg4,class T_arg5,class T_arg6, class T_return, class T_obj, class T_obj2>
inline bound_const_volatile_mem_functor6<T_return, T_obj, T_arg1,T_arg2,T_arg3,T_arg4,T_arg5,T_arg6>
mem_fun6(/*const*/ T_obj& _A_obj, T_return (T_obj2::*_A_func)(T_arg1,T_arg2,T_arg3,T_arg4,T_arg5,T_arg6) const volatile)
{ return bound_const_volatile_mem_functor6<T_return, T_obj, T_arg1,T_arg2,T_arg3,T_arg4,T_arg5,T_arg6>(_A_obj, _A_func); }

/** Creates a functor of type sigc::bound_const_volatile_mem_functor7 which encapsulates a method and an object instance.
 * @param _A_obj Pointer to object instance the functor should operate on.
 * @param _A_func Pointer to method that should be wrapped.
 * @return Functor that executes @e _A_func on invokation.
 *
 * @ingroup mem_fun
 */
template <class T_arg1,class T_arg2,class T_arg3,class T_arg4,class T_arg5,class T_arg6,class T_arg7, class T_return, class T_obj, class T_obj2>
inline bound_const_volatile_mem_functor7<T_return, T_obj, T_arg1,T_arg2,T_arg3,T_arg4,T_arg5,T_arg6,T_arg7>
mem_fun7(/*const*/ T_obj* _A_obj, T_return (T_obj2::*_A_func)(T_arg1,T_arg2,T_arg3,T_arg4,T_arg5,T_arg6,T_arg7) const volatile)
{ return bound_const_volatile_mem_functor7<T_return, T_obj, T_arg1,T_arg2,T_arg3,T_arg4,T_arg5,T_arg6,T_arg7>(_A_obj, _A_func); }

/** Creates a functor of type sigc::bound_const_volatile_mem_functor7 which encapsulates a method and an object instance.
 * @param _A_obj Reference to object instance the functor should operate on.
 * @param _A_func Pointer to method that should be wrapped.
 * @return Functor that executes @e _A_func on invokation.
 *
 * @ingroup mem_fun
 */
template <class T_arg1,class T_arg2,class T_arg3,class T_arg4,class T_arg5,class T_arg6,class T_arg7, class T_return, class T_obj, class T_obj2>
inline bound_const_volatile_mem_functor7<T_return, T_obj, T_arg1,T_arg2,T_arg3,T_arg4,T_arg5,T_arg6,T_arg7>
mem_fun7(/*const*/ T_obj& _A_obj, T_return (T_obj2::*_A_func)(T_arg1,T_arg2,T_arg3,T_arg4,T_arg5,T_arg6,T_arg7) const volatile)
{ return bound_const_volatile_mem_functor7<T_return, T_obj, T_arg1,T_arg2,T_arg3,T_arg4,T_arg5,T_arg6,T_arg7>(_A_obj, _A_func); }


// unnumbered
/** Creates a functor of type sigc::mem_functor0 which wraps a  method.
 * @param _A_func Pointer to method that should be wrapped.
 * @return Functor that executes _A_func on invokation.
 *
 * @ingroup mem_fun
 */
template <class T_return, class T_obj>
inline mem_functor0<T_return, T_obj>
mem_fun(T_return (T_obj::*_A_func)() )
{ return mem_functor0<T_return, T_obj>(_A_func); }

/** Creates a functor of type sigc::mem_functor1 which wraps a  method.
 * @param _A_func Pointer to method that should be wrapped.
 * @return Functor that executes _A_func on invokation.
 *
 * @ingroup mem_fun
 */
template <class T_arg1, class T_return, class T_obj>
inline mem_functor1<T_return, T_obj, T_arg1>
mem_fun(T_return (T_obj::*_A_func)(T_arg1) )
{ return mem_functor1<T_return, T_obj, T_arg1>(_A_func); }

/** Creates a functor of type sigc::mem_functor2 which wraps a  method.
 * @param _A_func Pointer to method that should be wrapped.
 * @return Functor that executes _A_func on invokation.
 *
 * @ingroup mem_fun
 */
template <class T_arg1,class T_arg2, class T_return, class T_obj>
inline mem_functor2<T_return, T_obj, T_arg1,T_arg2>
mem_fun(T_return (T_obj::*_A_func)(T_arg1,T_arg2) )
{ return mem_functor2<T_return, T_obj, T_arg1,T_arg2>(_A_func); }

/** Creates a functor of type sigc::mem_functor3 which wraps a  method.
 * @param _A_func Pointer to method that should be wrapped.
 * @return Functor that executes _A_func on invokation.
 *
 * @ingroup mem_fun
 */
template <class T_arg1,class T_arg2,class T_arg3, class T_return, class T_obj>
inline mem_functor3<T_return, T_obj, T_arg1,T_arg2,T_arg3>
mem_fun(T_return (T_obj::*_A_func)(T_arg1,T_arg2,T_arg3) )
{ return mem_functor3<T_return, T_obj, T_arg1,T_arg2,T_arg3>(_A_func); }

/** Creates a functor of type sigc::mem_functor4 which wraps a  method.
 * @param _A_func Pointer to method that should be wrapped.
 * @return Functor that executes _A_func on invokation.
 *
 * @ingroup mem_fun
 */
template <class T_arg1,class T_arg2,class T_arg3,class T_arg4, class T_return, class T_obj>
inline mem_functor4<T_return, T_obj, T_arg1,T_arg2,T_arg3,T_arg4>
mem_fun(T_return (T_obj::*_A_func)(T_arg1,T_arg2,T_arg3,T_arg4) )
{ return mem_functor4<T_return, T_obj, T_arg1,T_arg2,T_arg3,T_arg4>(_A_func); }

/** Creates a functor of type sigc::mem_functor5 which wraps a  method.
 * @param _A_func Pointer to method that should be wrapped.
 * @return Functor that executes _A_func on invokation.
 *
 * @ingroup mem_fun
 */
template <class T_arg1,class T_arg2,class T_arg3,class T_arg4,class T_arg5, class T_return, class T_obj>
inline mem_functor5<T_return, T_obj, T_arg1,T_arg2,T_arg3,T_arg4,T_arg5>
mem_fun(T_return (T_obj::*_A_func)(T_arg1,T_arg2,T_arg3,T_arg4,T_arg5) )
{ return mem_functor5<T_return, T_obj, T_arg1,T_arg2,T_arg3,T_arg4,T_arg5>(_A_func); }

/** Creates a functor of type sigc::mem_functor6 which wraps a  method.
 * @param _A_func Pointer to method that should be wrapped.
 * @return Functor that executes _A_func on invokation.
 *
 * @ingroup mem_fun
 */
template <class T_arg1,class T_arg2,class T_arg3,class T_arg4,class T_arg5,class T_arg6, class T_return, class T_obj>
inline mem_functor6<T_return, T_obj, T_arg1,T_arg2,T_arg3,T_arg4,T_arg5,T_arg6>
mem_fun(T_return (T_obj::*_A_func)(T_arg1,T_arg2,T_arg3,T_arg4,T_arg5,T_arg6) )
{ return mem_functor6<T_return, T_obj, T_arg1,T_arg2,T_arg3,T_arg4,T_arg5,T_arg6>(_A_func); }

/** Creates a functor of type sigc::mem_functor7 which wraps a  method.
 * @param _A_func Pointer to method that should be wrapped.
 * @return Functor that executes _A_func on invokation.
 *
 * @ingroup mem_fun
 */
template <class T_arg1,class T_arg2,class T_arg3,class T_arg4,class T_arg5,class T_arg6,class T_arg7, class T_return, class T_obj>
inline mem_functor7<T_return, T_obj, T_arg1,T_arg2,T_arg3,T_arg4,T_arg5,T_arg6,T_arg7>
mem_fun(T_return (T_obj::*_A_func)(T_arg1,T_arg2,T_arg3,T_arg4,T_arg5,T_arg6,T_arg7) )
{ return mem_functor7<T_return, T_obj, T_arg1,T_arg2,T_arg3,T_arg4,T_arg5,T_arg6,T_arg7>(_A_func); }

/** Creates a functor of type sigc::const_mem_functor0 which wraps a const method.
 * @param _A_func Pointer to method that should be wrapped.
 * @return Functor that executes _A_func on invokation.
 *
 * @ingroup mem_fun
 */
template <class T_return, class T_obj>
inline const_mem_functor0<T_return, T_obj>
mem_fun(T_return (T_obj::*_A_func)() const)
{ return const_mem_functor0<T_return, T_obj>(_A_func); }

/** Creates a functor of type sigc::const_mem_functor1 which wraps a const method.
 * @param _A_func Pointer to method that should be wrapped.
 * @return Functor that executes _A_func on invokation.
 *
 * @ingroup mem_fun
 */
template <class T_arg1, class T_return, class T_obj>
inline const_mem_functor1<T_return, T_obj, T_arg1>
mem_fun(T_return (T_obj::*_A_func)(T_arg1) const)
{ return const_mem_functor1<T_return, T_obj, T_arg1>(_A_func); }

/** Creates a functor of type sigc::const_mem_functor2 which wraps a const method.
 * @param _A_func Pointer to method that should be wrapped.
 * @return Functor that executes _A_func on invokation.
 *
 * @ingroup mem_fun
 */
template <class T_arg1,class T_arg2, class T_return, class T_obj>
inline const_mem_functor2<T_return, T_obj, T_arg1,T_arg2>
mem_fun(T_return (T_obj::*_A_func)(T_arg1,T_arg2) const)
{ return const_mem_functor2<T_return, T_obj, T_arg1,T_arg2>(_A_func); }

/** Creates a functor of type sigc::const_mem_functor3 which wraps a const method.
 * @param _A_func Pointer to method that should be wrapped.
 * @return Functor that executes _A_func on invokation.
 *
 * @ingroup mem_fun
 */
template <class T_arg1,class T_arg2,class T_arg3, class T_return, class T_obj>
inline const_mem_functor3<T_return, T_obj, T_arg1,T_arg2,T_arg3>
mem_fun(T_return (T_obj::*_A_func)(T_arg1,T_arg2,T_arg3) const)
{ return const_mem_functor3<T_return, T_obj, T_arg1,T_arg2,T_arg3>(_A_func); }

/** Creates a functor of type sigc::const_mem_functor4 which wraps a const method.
 * @param _A_func Pointer to method that should be wrapped.
 * @return Functor that executes _A_func on invokation.
 *
 * @ingroup mem_fun
 */
template <class T_arg1,class T_arg2,class T_arg3,class T_arg4, class T_return, class T_obj>
inline const_mem_functor4<T_return, T_obj, T_arg1,T_arg2,T_arg3,T_arg4>
mem_fun(T_return (T_obj::*_A_func)(T_arg1,T_arg2,T_arg3,T_arg4) const)
{ return const_mem_functor4<T_return, T_obj, T_arg1,T_arg2,T_arg3,T_arg4>(_A_func); }

/** Creates a functor of type sigc::const_mem_functor5 which wraps a const method.
 * @param _A_func Pointer to method that should be wrapped.
 * @return Functor that executes _A_func on invokation.
 *
 * @ingroup mem_fun
 */
template <class T_arg1,class T_arg2,class T_arg3,class T_arg4,class T_arg5, class T_return, class T_obj>
inline const_mem_functor5<T_return, T_obj, T_arg1,T_arg2,T_arg3,T_arg4,T_arg5>
mem_fun(T_return (T_obj::*_A_func)(T_arg1,T_arg2,T_arg3,T_arg4,T_arg5) const)
{ return const_mem_functor5<T_return, T_obj, T_arg1,T_arg2,T_arg3,T_arg4,T_arg5>(_A_func); }

/** Creates a functor of type sigc::const_mem_functor6 which wraps a const method.
 * @param _A_func Pointer to method that should be wrapped.
 * @return Functor that executes _A_func on invokation.
 *
 * @ingroup mem_fun
 */
template <class T_arg1,class T_arg2,class T_arg3,class T_arg4,class T_arg5,class T_arg6, class T_return, class T_obj>
inline const_mem_functor6<T_return, T_obj, T_arg1,T_arg2,T_arg3,T_arg4,T_arg5,T_arg6>
mem_fun(T_return (T_obj::*_A_func)(T_arg1,T_arg2,T_arg3,T_arg4,T_arg5,T_arg6) const)
{ return const_mem_functor6<T_return, T_obj, T_arg1,T_arg2,T_arg3,T_arg4,T_arg5,T_arg6>(_A_func); }

/** Creates a functor of type sigc::const_mem_functor7 which wraps a const method.
 * @param _A_func Pointer to method that should be wrapped.
 * @return Functor that executes _A_func on invokation.
 *
 * @ingroup mem_fun
 */
template <class T_arg1,class T_arg2,class T_arg3,class T_arg4,class T_arg5,class T_arg6,class T_arg7, class T_return, class T_obj>
inline const_mem_functor7<T_return, T_obj, T_arg1,T_arg2,T_arg3,T_arg4,T_arg5,T_arg6,T_arg7>
mem_fun(T_return (T_obj::*_A_func)(T_arg1,T_arg2,T_arg3,T_arg4,T_arg5,T_arg6,T_arg7) const)
{ return const_mem_functor7<T_return, T_obj, T_arg1,T_arg2,T_arg3,T_arg4,T_arg5,T_arg6,T_arg7>(_A_func); }

/** Creates a functor of type sigc::volatile_mem_functor0 which wraps a volatile method.
 * @param _A_func Pointer to method that should be wrapped.
 * @return Functor that executes _A_func on invokation.
 *
 * @ingroup mem_fun
 */
template <class T_return, class T_obj>
inline volatile_mem_functor0<T_return, T_obj>
mem_fun(T_return (T_obj::*_A_func)() volatile)
{ return volatile_mem_functor0<T_return, T_obj>(_A_func); }

/** Creates a functor of type sigc::volatile_mem_functor1 which wraps a volatile method.
 * @param _A_func Pointer to method that should be wrapped.
 * @return Functor that executes _A_func on invokation.
 *
 * @ingroup mem_fun
 */
template <class T_arg1, class T_return, class T_obj>
inline volatile_mem_functor1<T_return, T_obj, T_arg1>
mem_fun(T_return (T_obj::*_A_func)(T_arg1) volatile)
{ return volatile_mem_functor1<T_return, T_obj, T_arg1>(_A_func); }

/** Creates a functor of type sigc::volatile_mem_functor2 which wraps a volatile method.
 * @param _A_func Pointer to method that should be wrapped.
 * @return Functor that executes _A_func on invokation.
 *
 * @ingroup mem_fun
 */
template <class T_arg1,class T_arg2, class T_return, class T_obj>
inline volatile_mem_functor2<T_return, T_obj, T_arg1,T_arg2>
mem_fun(T_return (T_obj::*_A_func)(T_arg1,T_arg2) volatile)
{ return volatile_mem_functor2<T_return, T_obj, T_arg1,T_arg2>(_A_func); }

/** Creates a functor of type sigc::volatile_mem_functor3 which wraps a volatile method.
 * @param _A_func Pointer to method that should be wrapped.
 * @return Functor that executes _A_func on invokation.
 *
 * @ingroup mem_fun
 */
template <class T_arg1,class T_arg2,class T_arg3, class T_return, class T_obj>
inline volatile_mem_functor3<T_return, T_obj, T_arg1,T_arg2,T_arg3>
mem_fun(T_return (T_obj::*_A_func)(T_arg1,T_arg2,T_arg3) volatile)
{ return volatile_mem_functor3<T_return, T_obj, T_arg1,T_arg2,T_arg3>(_A_func); }

/** Creates a functor of type sigc::volatile_mem_functor4 which wraps a volatile method.
 * @param _A_func Pointer to method that should be wrapped.
 * @return Functor that executes _A_func on invokation.
 *
 * @ingroup mem_fun
 */
template <class T_arg1,class T_arg2,class T_arg3,class T_arg4, class T_return, class T_obj>
inline volatile_mem_functor4<T_return, T_obj, T_arg1,T_arg2,T_arg3,T_arg4>
mem_fun(T_return (T_obj::*_A_func)(T_arg1,T_arg2,T_arg3,T_arg4) volatile)
{ return volatile_mem_functor4<T_return, T_obj, T_arg1,T_arg2,T_arg3,T_arg4>(_A_func); }

/** Creates a functor of type sigc::volatile_mem_functor5 which wraps a volatile method.
 * @param _A_func Pointer to method that should be wrapped.
 * @return Functor that executes _A_func on invokation.
 *
 * @ingroup mem_fun
 */
template <class T_arg1,class T_arg2,class T_arg3,class T_arg4,class T_arg5, class T_return, class T_obj>
inline volatile_mem_functor5<T_return, T_obj, T_arg1,T_arg2,T_arg3,T_arg4,T_arg5>
mem_fun(T_return (T_obj::*_A_func)(T_arg1,T_arg2,T_arg3,T_arg4,T_arg5) volatile)
{ return volatile_mem_functor5<T_return, T_obj, T_arg1,T_arg2,T_arg3,T_arg4,T_arg5>(_A_func); }

/** Creates a functor of type sigc::volatile_mem_functor6 which wraps a volatile method.
 * @param _A_func Pointer to method that should be wrapped.
 * @return Functor that executes _A_func on invokation.
 *
 * @ingroup mem_fun
 */
template <class T_arg1,class T_arg2,class T_arg3,class T_arg4,class T_arg5,class T_arg6, class T_return, class T_obj>
inline volatile_mem_functor6<T_return, T_obj, T_arg1,T_arg2,T_arg3,T_arg4,T_arg5,T_arg6>
mem_fun(T_return (T_obj::*_A_func)(T_arg1,T_arg2,T_arg3,T_arg4,T_arg5,T_arg6) volatile)
{ return volatile_mem_functor6<T_return, T_obj, T_arg1,T_arg2,T_arg3,T_arg4,T_arg5,T_arg6>(_A_func); }

/** Creates a functor of type sigc::volatile_mem_functor7 which wraps a volatile method.
 * @param _A_func Pointer to method that should be wrapped.
 * @return Functor that executes _A_func on invokation.
 *
 * @ingroup mem_fun
 */
template <class T_arg1,class T_arg2,class T_arg3,class T_arg4,class T_arg5,class T_arg6,class T_arg7, class T_return, class T_obj>
inline volatile_mem_functor7<T_return, T_obj, T_arg1,T_arg2,T_arg3,T_arg4,T_arg5,T_arg6,T_arg7>
mem_fun(T_return (T_obj::*_A_func)(T_arg1,T_arg2,T_arg3,T_arg4,T_arg5,T_arg6,T_arg7) volatile)
{ return volatile_mem_functor7<T_return, T_obj, T_arg1,T_arg2,T_arg3,T_arg4,T_arg5,T_arg6,T_arg7>(_A_func); }

/** Creates a functor of type sigc::const_volatile_mem_functor0 which wraps a const volatile method.
 * @param _A_func Pointer to method that should be wrapped.
 * @return Functor that executes _A_func on invokation.
 *
 * @ingroup mem_fun
 */
template <class T_return, class T_obj>
inline const_volatile_mem_functor0<T_return, T_obj>
mem_fun(T_return (T_obj::*_A_func)() const volatile)
{ return const_volatile_mem_functor0<T_return, T_obj>(_A_func); }

/** Creates a functor of type sigc::const_volatile_mem_functor1 which wraps a const volatile method.
 * @param _A_func Pointer to method that should be wrapped.
 * @return Functor that executes _A_func on invokation.
 *
 * @ingroup mem_fun
 */
template <class T_arg1, class T_return, class T_obj>
inline const_volatile_mem_functor1<T_return, T_obj, T_arg1>
mem_fun(T_return (T_obj::*_A_func)(T_arg1) const volatile)
{ return const_volatile_mem_functor1<T_return, T_obj, T_arg1>(_A_func); }

/** Creates a functor of type sigc::const_volatile_mem_functor2 which wraps a const volatile method.
 * @param _A_func Pointer to method that should be wrapped.
 * @return Functor that executes _A_func on invokation.
 *
 * @ingroup mem_fun
 */
template <class T_arg1,class T_arg2, class T_return, class T_obj>
inline const_volatile_mem_functor2<T_return, T_obj, T_arg1,T_arg2>
mem_fun(T_return (T_obj::*_A_func)(T_arg1,T_arg2) const volatile)
{ return const_volatile_mem_functor2<T_return, T_obj, T_arg1,T_arg2>(_A_func); }

/** Creates a functor of type sigc::const_volatile_mem_functor3 which wraps a const volatile method.
 * @param _A_func Pointer to method that should be wrapped.
 * @return Functor that executes _A_func on invokation.
 *
 * @ingroup mem_fun
 */
template <class T_arg1,class T_arg2,class T_arg3, class T_return, class T_obj>
inline const_volatile_mem_functor3<T_return, T_obj, T_arg1,T_arg2,T_arg3>
mem_fun(T_return (T_obj::*_A_func)(T_arg1,T_arg2,T_arg3) const volatile)
{ return const_volatile_mem_functor3<T_return, T_obj, T_arg1,T_arg2,T_arg3>(_A_func); }

/** Creates a functor of type sigc::const_volatile_mem_functor4 which wraps a const volatile method.
 * @param _A_func Pointer to method that should be wrapped.
 * @return Functor that executes _A_func on invokation.
 *
 * @ingroup mem_fun
 */
template <class T_arg1,class T_arg2,class T_arg3,class T_arg4, class T_return, class T_obj>
inline const_volatile_mem_functor4<T_return, T_obj, T_arg1,T_arg2,T_arg3,T_arg4>
mem_fun(T_return (T_obj::*_A_func)(T_arg1,T_arg2,T_arg3,T_arg4) const volatile)
{ return const_volatile_mem_functor4<T_return, T_obj, T_arg1,T_arg2,T_arg3,T_arg4>(_A_func); }

/** Creates a functor of type sigc::const_volatile_mem_functor5 which wraps a const volatile method.
 * @param _A_func Pointer to method that should be wrapped.
 * @return Functor that executes _A_func on invokation.
 *
 * @ingroup mem_fun
 */
template <class T_arg1,class T_arg2,class T_arg3,class T_arg4,class T_arg5, class T_return, class T_obj>
inline const_volatile_mem_functor5<T_return, T_obj, T_arg1,T_arg2,T_arg3,T_arg4,T_arg5>
mem_fun(T_return (T_obj::*_A_func)(T_arg1,T_arg2,T_arg3,T_arg4,T_arg5) const volatile)
{ return const_volatile_mem_functor5<T_return, T_obj, T_arg1,T_arg2,T_arg3,T_arg4,T_arg5>(_A_func); }

/** Creates a functor of type sigc::const_volatile_mem_functor6 which wraps a const volatile method.
 * @param _A_func Pointer to method that should be wrapped.
 * @return Functor that executes _A_func on invokation.
 *
 * @ingroup mem_fun
 */
template <class T_arg1,class T_arg2,class T_arg3,class T_arg4,class T_arg5,class T_arg6, class T_return, class T_obj>
inline const_volatile_mem_functor6<T_return, T_obj, T_arg1,T_arg2,T_arg3,T_arg4,T_arg5,T_arg6>
mem_fun(T_return (T_obj::*_A_func)(T_arg1,T_arg2,T_arg3,T_arg4,T_arg5,T_arg6) const volatile)
{ return const_volatile_mem_functor6<T_return, T_obj, T_arg1,T_arg2,T_arg3,T_arg4,T_arg5,T_arg6>(_A_func); }

/** Creates a functor of type sigc::const_volatile_mem_functor7 which wraps a const volatile method.
 * @param _A_func Pointer to method that should be wrapped.
 * @return Functor that executes _A_func on invokation.
 *
 * @ingroup mem_fun
 */
template <class T_arg1,class T_arg2,class T_arg3,class T_arg4,class T_arg5,class T_arg6,class T_arg7, class T_return, class T_obj>
inline const_volatile_mem_functor7<T_return, T_obj, T_arg1,T_arg2,T_arg3,T_arg4,T_arg5,T_arg6,T_arg7>
mem_fun(T_return (T_obj::*_A_func)(T_arg1,T_arg2,T_arg3,T_arg4,T_arg5,T_arg6,T_arg7) const volatile)
{ return const_volatile_mem_functor7<T_return, T_obj, T_arg1,T_arg2,T_arg3,T_arg4,T_arg5,T_arg6,T_arg7>(_A_func); }

/** Creates a functor of type sigc::bound_mem_functor0 which encapsulates a method and an object instance.
 * @param _A_obj Pointer to object instance the functor should operate on.
 * @param _A_func Pointer to method that should be wrapped.
 * @return Functor that executes @e _A_func on invokation.
 *
 * @ingroup mem_fun
 */
template <class T_return, class T_obj, class T_obj2>
inline bound_mem_functor0<T_return, T_obj>
mem_fun(/**/ T_obj* _A_obj, T_return (T_obj2::*_A_func)() )
{ return bound_mem_functor0<T_return, T_obj>(_A_obj, _A_func); }

/** Creates a functor of type sigc::bound_mem_functor0 which encapsulates a method and an object instance.
 * @param _A_obj Reference to object instance the functor should operate on.
 * @param _A_func Pointer to method that should be wrapped.
 * @return Functor that executes @e _A_func on invokation.
 *
 * @ingroup mem_fun
 */
template <class T_return, class T_obj, class T_obj2>
inline bound_mem_functor0<T_return, T_obj>
mem_fun(/**/ T_obj& _A_obj, T_return (T_obj2::*_A_func)() )
{ return bound_mem_functor0<T_return, T_obj>(_A_obj, _A_func); }

/** Creates a functor of type sigc::bound_mem_functor1 which encapsulates a method and an object instance.
 * @param _A_obj Pointer to object instance the functor should operate on.
 * @param _A_func Pointer to method that should be wrapped.
 * @return Functor that executes @e _A_func on invokation.
 *
 * @ingroup mem_fun
 */
template <class T_arg1, class T_return, class T_obj, class T_obj2>
inline bound_mem_functor1<T_return, T_obj, T_arg1>
mem_fun(/**/ T_obj* _A_obj, T_return (T_obj2::*_A_func)(T_arg1) )
{ return bound_mem_functor1<T_return, T_obj, T_arg1>(_A_obj, _A_func); }

/** Creates a functor of type sigc::bound_mem_functor1 which encapsulates a method and an object instance.
 * @param _A_obj Reference to object instance the functor should operate on.
 * @param _A_func Pointer to method that should be wrapped.
 * @return Functor that executes @e _A_func on invokation.
 *
 * @ingroup mem_fun
 */
template <class T_arg1, class T_return, class T_obj, class T_obj2>
inline bound_mem_functor1<T_return, T_obj, T_arg1>
mem_fun(/**/ T_obj& _A_obj, T_return (T_obj2::*_A_func)(T_arg1) )
{ return bound_mem_functor1<T_return, T_obj, T_arg1>(_A_obj, _A_func); }

/** Creates a functor of type sigc::bound_mem_functor2 which encapsulates a method and an object instance.
 * @param _A_obj Pointer to object instance the functor should operate on.
 * @param _A_func Pointer to method that should be wrapped.
 * @return Functor that executes @e _A_func on invokation.
 *
 * @ingroup mem_fun
 */
template <class T_arg1,class T_arg2, class T_return, class T_obj, class T_obj2>
inline bound_mem_functor2<T_return, T_obj, T_arg1,T_arg2>
mem_fun(/**/ T_obj* _A_obj, T_return (T_obj2::*_A_func)(T_arg1,T_arg2) )
{ return bound_mem_functor2<T_return, T_obj, T_arg1,T_arg2>(_A_obj, _A_func); }

/** Creates a functor of type sigc::bound_mem_functor2 which encapsulates a method and an object instance.
 * @param _A_obj Reference to object instance the functor should operate on.
 * @param _A_func Pointer to method that should be wrapped.
 * @return Functor that executes @e _A_func on invokation.
 *
 * @ingroup mem_fun
 */
template <class T_arg1,class T_arg2, class T_return, class T_obj, class T_obj2>
inline bound_mem_functor2<T_return, T_obj, T_arg1,T_arg2>
mem_fun(/**/ T_obj& _A_obj, T_return (T_obj2::*_A_func)(T_arg1,T_arg2) )
{ return bound_mem_functor2<T_return, T_obj, T_arg1,T_arg2>(_A_obj, _A_func); }

/** Creates a functor of type sigc::bound_mem_functor3 which encapsulates a method and an object instance.
 * @param _A_obj Pointer to object instance the functor should operate on.
 * @param _A_func Pointer to method that should be wrapped.
 * @return Functor that executes @e _A_func on invokation.
 *
 * @ingroup mem_fun
 */
template <class T_arg1,class T_arg2,class T_arg3, class T_return, class T_obj, class T_obj2>
inline bound_mem_functor3<T_return, T_obj, T_arg1,T_arg2,T_arg3>
mem_fun(/**/ T_obj* _A_obj, T_return (T_obj2::*_A_func)(T_arg1,T_arg2,T_arg3) )
{ return bound_mem_functor3<T_return, T_obj, T_arg1,T_arg2,T_arg3>(_A_obj, _A_func); }

/** Creates a functor of type sigc::bound_mem_functor3 which encapsulates a method and an object instance.
 * @param _A_obj Reference to object instance the functor should operate on.
 * @param _A_func Pointer to method that should be wrapped.
 * @return Functor that executes @e _A_func on invokation.
 *
 * @ingroup mem_fun
 */
template <class T_arg1,class T_arg2,class T_arg3, class T_return, class T_obj, class T_obj2>
inline bound_mem_functor3<T_return, T_obj, T_arg1,T_arg2,T_arg3>
mem_fun(/**/ T_obj& _A_obj, T_return (T_obj2::*_A_func)(T_arg1,T_arg2,T_arg3) )
{ return bound_mem_functor3<T_return, T_obj, T_arg1,T_arg2,T_arg3>(_A_obj, _A_func); }

/** Creates a functor of type sigc::bound_mem_functor4 which encapsulates a method and an object instance.
 * @param _A_obj Pointer to object instance the functor should operate on.
 * @param _A_func Pointer to method that should be wrapped.
 * @return Functor that executes @e _A_func on invokation.
 *
 * @ingroup mem_fun
 */
template <class T_arg1,class T_arg2,class T_arg3,class T_arg4, class T_return, class T_obj, class T_obj2>
inline bound_mem_functor4<T_return, T_obj, T_arg1,T_arg2,T_arg3,T_arg4>
mem_fun(/**/ T_obj* _A_obj, T_return (T_obj2::*_A_func)(T_arg1,T_arg2,T_arg3,T_arg4) )
{ return bound_mem_functor4<T_return, T_obj, T_arg1,T_arg2,T_arg3,T_arg4>(_A_obj, _A_func); }

/** Creates a functor of type sigc::bound_mem_functor4 which encapsulates a method and an object instance.
 * @param _A_obj Reference to object instance the functor should operate on.
 * @param _A_func Pointer to method that should be wrapped.
 * @return Functor that executes @e _A_func on invokation.
 *
 * @ingroup mem_fun
 */
template <class T_arg1,class T_arg2,class T_arg3,class T_arg4, class T_return, class T_obj, class T_obj2>
inline bound_mem_functor4<T_return, T_obj, T_arg1,T_arg2,T_arg3,T_arg4>
mem_fun(/**/ T_obj& _A_obj, T_return (T_obj2::*_A_func)(T_arg1,T_arg2,T_arg3,T_arg4) )
{ return bound_mem_functor4<T_return, T_obj, T_arg1,T_arg2,T_arg3,T_arg4>(_A_obj, _A_func); }

/** Creates a functor of type sigc::bound_mem_functor5 which encapsulates a method and an object instance.
 * @param _A_obj Pointer to object instance the functor should operate on.
 * @param _A_func Pointer to method that should be wrapped.
 * @return Functor that executes @e _A_func on invokation.
 *
 * @ingroup mem_fun
 */
template <class T_arg1,class T_arg2,class T_arg3,class T_arg4,class T_arg5, class T_return, class T_obj, class T_obj2>
inline bound_mem_functor5<T_return, T_obj, T_arg1,T_arg2,T_arg3,T_arg4,T_arg5>
mem_fun(/**/ T_obj* _A_obj, T_return (T_obj2::*_A_func)(T_arg1,T_arg2,T_arg3,T_arg4,T_arg5) )
{ return bound_mem_functor5<T_return, T_obj, T_arg1,T_arg2,T_arg3,T_arg4,T_arg5>(_A_obj, _A_func); }

/** Creates a functor of type sigc::bound_mem_functor5 which encapsulates a method and an object instance.
 * @param _A_obj Reference to object instance the functor should operate on.
 * @param _A_func Pointer to method that should be wrapped.
 * @return Functor that executes @e _A_func on invokation.
 *
 * @ingroup mem_fun
 */
template <class T_arg1,class T_arg2,class T_arg3,class T_arg4,class T_arg5, class T_return, class T_obj, class T_obj2>
inline bound_mem_functor5<T_return, T_obj, T_arg1,T_arg2,T_arg3,T_arg4,T_arg5>
mem_fun(/**/ T_obj& _A_obj, T_return (T_obj2::*_A_func)(T_arg1,T_arg2,T_arg3,T_arg4,T_arg5) )
{ return bound_mem_functor5<T_return, T_obj, T_arg1,T_arg2,T_arg3,T_arg4,T_arg5>(_A_obj, _A_func); }

/** Creates a functor of type sigc::bound_mem_functor6 which encapsulates a method and an object instance.
 * @param _A_obj Pointer to object instance the functor should operate on.
 * @param _A_func Pointer to method that should be wrapped.
 * @return Functor that executes @e _A_func on invokation.
 *
 * @ingroup mem_fun
 */
template <class T_arg1,class T_arg2,class T_arg3,class T_arg4,class T_arg5,class T_arg6, class T_return, class T_obj, class T_obj2>
inline bound_mem_functor6<T_return, T_obj, T_arg1,T_arg2,T_arg3,T_arg4,T_arg5,T_arg6>
mem_fun(/**/ T_obj* _A_obj, T_return (T_obj2::*_A_func)(T_arg1,T_arg2,T_arg3,T_arg4,T_arg5,T_arg6) )
{ return bound_mem_functor6<T_return, T_obj, T_arg1,T_arg2,T_arg3,T_arg4,T_arg5,T_arg6>(_A_obj, _A_func); }

/** Creates a functor of type sigc::bound_mem_functor6 which encapsulates a method and an object instance.
 * @param _A_obj Reference to object instance the functor should operate on.
 * @param _A_func Pointer to method that should be wrapped.
 * @return Functor that executes @e _A_func on invokation.
 *
 * @ingroup mem_fun
 */
template <class T_arg1,class T_arg2,class T_arg3,class T_arg4,class T_arg5,class T_arg6, class T_return, class T_obj, class T_obj2>
inline bound_mem_functor6<T_return, T_obj, T_arg1,T_arg2,T_arg3,T_arg4,T_arg5,T_arg6>
mem_fun(/**/ T_obj& _A_obj, T_return (T_obj2::*_A_func)(T_arg1,T_arg2,T_arg3,T_arg4,T_arg5,T_arg6) )
{ return bound_mem_functor6<T_return, T_obj, T_arg1,T_arg2,T_arg3,T_arg4,T_arg5,T_arg6>(_A_obj, _A_func); }

/** Creates a functor of type sigc::bound_mem_functor7 which encapsulates a method and an object instance.
 * @param _A_obj Pointer to object instance the functor should operate on.
 * @param _A_func Pointer to method that should be wrapped.
 * @return Functor that executes @e _A_func on invokation.
 *
 * @ingroup mem_fun
 */
template <class T_arg1,class T_arg2,class T_arg3,class T_arg4,class T_arg5,class T_arg6,class T_arg7, class T_return, class T_obj, class T_obj2>
inline bound_mem_functor7<T_return, T_obj, T_arg1,T_arg2,T_arg3,T_arg4,T_arg5,T_arg6,T_arg7>
mem_fun(/**/ T_obj* _A_obj, T_return (T_obj2::*_A_func)(T_arg1,T_arg2,T_arg3,T_arg4,T_arg5,T_arg6,T_arg7) )
{ return bound_mem_functor7<T_return, T_obj, T_arg1,T_arg2,T_arg3,T_arg4,T_arg5,T_arg6,T_arg7>(_A_obj, _A_func); }

/** Creates a functor of type sigc::bound_mem_functor7 which encapsulates a method and an object instance.
 * @param _A_obj Reference to object instance the functor should operate on.
 * @param _A_func Pointer to method that should be wrapped.
 * @return Functor that executes @e _A_func on invokation.
 *
 * @ingroup mem_fun
 */
template <class T_arg1,class T_arg2,class T_arg3,class T_arg4,class T_arg5,class T_arg6,class T_arg7, class T_return, class T_obj, class T_obj2>
inline bound_mem_functor7<T_return, T_obj, T_arg1,T_arg2,T_arg3,T_arg4,T_arg5,T_arg6,T_arg7>
mem_fun(/**/ T_obj& _A_obj, T_return (T_obj2::*_A_func)(T_arg1,T_arg2,T_arg3,T_arg4,T_arg5,T_arg6,T_arg7) )
{ return bound_mem_functor7<T_return, T_obj, T_arg1,T_arg2,T_arg3,T_arg4,T_arg5,T_arg6,T_arg7>(_A_obj, _A_func); }

/** Creates a functor of type sigc::bound_const_mem_functor0 which encapsulates a method and an object instance.
 * @param _A_obj Pointer to object instance the functor should operate on.
 * @param _A_func Pointer to method that should be wrapped.
 * @return Functor that executes @e _A_func on invokation.
 *
 * @ingroup mem_fun
 */
template <class T_return, class T_obj, class T_obj2>
inline bound_const_mem_functor0<T_return, T_obj>
mem_fun(/*const*/ T_obj* _A_obj, T_return (T_obj2::*_A_func)() const)
{ return bound_const_mem_functor0<T_return, T_obj>(_A_obj, _A_func); }

/** Creates a functor of type sigc::bound_const_mem_functor0 which encapsulates a method and an object instance.
 * @param _A_obj Reference to object instance the functor should operate on.
 * @param _A_func Pointer to method that should be wrapped.
 * @return Functor that executes @e _A_func on invokation.
 *
 * @ingroup mem_fun
 */
template <class T_return, class T_obj, class T_obj2>
inline bound_const_mem_functor0<T_return, T_obj>
mem_fun(/*const*/ T_obj& _A_obj, T_return (T_obj2::*_A_func)() const)
{ return bound_const_mem_functor0<T_return, T_obj>(_A_obj, _A_func); }

/** Creates a functor of type sigc::bound_const_mem_functor1 which encapsulates a method and an object instance.
 * @param _A_obj Pointer to object instance the functor should operate on.
 * @param _A_func Pointer to method that should be wrapped.
 * @return Functor that executes @e _A_func on invokation.
 *
 * @ingroup mem_fun
 */
template <class T_arg1, class T_return, class T_obj, class T_obj2>
inline bound_const_mem_functor1<T_return, T_obj, T_arg1>
mem_fun(/*const*/ T_obj* _A_obj, T_return (T_obj2::*_A_func)(T_arg1) const)
{ return bound_const_mem_functor1<T_return, T_obj, T_arg1>(_A_obj, _A_func); }

/** Creates a functor of type sigc::bound_const_mem_functor1 which encapsulates a method and an object instance.
 * @param _A_obj Reference to object instance the functor should operate on.
 * @param _A_func Pointer to method that should be wrapped.
 * @return Functor that executes @e _A_func on invokation.
 *
 * @ingroup mem_fun
 */
template <class T_arg1, class T_return, class T_obj, class T_obj2>
inline bound_const_mem_functor1<T_return, T_obj, T_arg1>
mem_fun(/*const*/ T_obj& _A_obj, T_return (T_obj2::*_A_func)(T_arg1) const)
{ return bound_const_mem_functor1<T_return, T_obj, T_arg1>(_A_obj, _A_func); }

/** Creates a functor of type sigc::bound_const_mem_functor2 which encapsulates a method and an object instance.
 * @param _A_obj Pointer to object instance the functor should operate on.
 * @param _A_func Pointer to method that should be wrapped.
 * @return Functor that executes @e _A_func on invokation.
 *
 * @ingroup mem_fun
 */
template <class T_arg1,class T_arg2, class T_return, class T_obj, class T_obj2>
inline bound_const_mem_functor2<T_return, T_obj, T_arg1,T_arg2>
mem_fun(/*const*/ T_obj* _A_obj, T_return (T_obj2::*_A_func)(T_arg1,T_arg2) const)
{ return bound_const_mem_functor2<T_return, T_obj, T_arg1,T_arg2>(_A_obj, _A_func); }

/** Creates a functor of type sigc::bound_const_mem_functor2 which encapsulates a method and an object instance.
 * @param _A_obj Reference to object instance the functor should operate on.
 * @param _A_func Pointer to method that should be wrapped.
 * @return Functor that executes @e _A_func on invokation.
 *
 * @ingroup mem_fun
 */
template <class T_arg1,class T_arg2, class T_return, class T_obj, class T_obj2>
inline bound_const_mem_functor2<T_return, T_obj, T_arg1,T_arg2>
mem_fun(/*const*/ T_obj& _A_obj, T_return (T_obj2::*_A_func)(T_arg1,T_arg2) const)
{ return bound_const_mem_functor2<T_return, T_obj, T_arg1,T_arg2>(_A_obj, _A_func); }

/** Creates a functor of type sigc::bound_const_mem_functor3 which encapsulates a method and an object instance.
 * @param _A_obj Pointer to object instance the functor should operate on.
 * @param _A_func Pointer to method that should be wrapped.
 * @return Functor that executes @e _A_func on invokation.
 *
 * @ingroup mem_fun
 */
template <class T_arg1,class T_arg2,class T_arg3, class T_return, class T_obj, class T_obj2>
inline bound_const_mem_functor3<T_return, T_obj, T_arg1,T_arg2,T_arg3>
mem_fun(/*const*/ T_obj* _A_obj, T_return (T_obj2::*_A_func)(T_arg1,T_arg2,T_arg3) const)
{ return bound_const_mem_functor3<T_return, T_obj, T_arg1,T_arg2,T_arg3>(_A_obj, _A_func); }

/** Creates a functor of type sigc::bound_const_mem_functor3 which encapsulates a method and an object instance.
 * @param _A_obj Reference to object instance the functor should operate on.
 * @param _A_func Pointer to method that should be wrapped.
 * @return Functor that executes @e _A_func on invokation.
 *
 * @ingroup mem_fun
 */
template <class T_arg1,class T_arg2,class T_arg3, class T_return, class T_obj, class T_obj2>
inline bound_const_mem_functor3<T_return, T_obj, T_arg1,T_arg2,T_arg3>
mem_fun(/*const*/ T_obj& _A_obj, T_return (T_obj2::*_A_func)(T_arg1,T_arg2,T_arg3) const)
{ return bound_const_mem_functor3<T_return, T_obj, T_arg1,T_arg2,T_arg3>(_A_obj, _A_func); }

/** Creates a functor of type sigc::bound_const_mem_functor4 which encapsulates a method and an object instance.
 * @param _A_obj Pointer to object instance the functor should operate on.
 * @param _A_func Pointer to method that should be wrapped.
 * @return Functor that executes @e _A_func on invokation.
 *
 * @ingroup mem_fun
 */
template <class T_arg1,class T_arg2,class T_arg3,class T_arg4, class T_return, class T_obj, class T_obj2>
inline bound_const_mem_functor4<T_return, T_obj, T_arg1,T_arg2,T_arg3,T_arg4>
mem_fun(/*const*/ T_obj* _A_obj, T_return (T_obj2::*_A_func)(T_arg1,T_arg2,T_arg3,T_arg4) const)
{ return bound_const_mem_functor4<T_return, T_obj, T_arg1,T_arg2,T_arg3,T_arg4>(_A_obj, _A_func); }

/** Creates a functor of type sigc::bound_const_mem_functor4 which encapsulates a method and an object instance.
 * @param _A_obj Reference to object instance the functor should operate on.
 * @param _A_func Pointer to method that should be wrapped.
 * @return Functor that executes @e _A_func on invokation.
 *
 * @ingroup mem_fun
 */
template <class T_arg1,class T_arg2,class T_arg3,class T_arg4, class T_return, class T_obj, class T_obj2>
inline bound_const_mem_functor4<T_return, T_obj, T_arg1,T_arg2,T_arg3,T_arg4>
mem_fun(/*const*/ T_obj& _A_obj, T_return (T_obj2::*_A_func)(T_arg1,T_arg2,T_arg3,T_arg4) const)
{ return bound_const_mem_functor4<T_return, T_obj, T_arg1,T_arg2,T_arg3,T_arg4>(_A_obj, _A_func); }

/** Creates a functor of type sigc::bound_const_mem_functor5 which encapsulates a method and an object instance.
 * @param _A_obj Pointer to object instance the functor should operate on.
 * @param _A_func Pointer to method that should be wrapped.
 * @return Functor that executes @e _A_func on invokation.
 *
 * @ingroup mem_fun
 */
template <class T_arg1,class T_arg2,class T_arg3,class T_arg4,class T_arg5, class T_return, class T_obj, class T_obj2>
inline bound_const_mem_functor5<T_return, T_obj, T_arg1,T_arg2,T_arg3,T_arg4,T_arg5>
mem_fun(/*const*/ T_obj* _A_obj, T_return (T_obj2::*_A_func)(T_arg1,T_arg2,T_arg3,T_arg4,T_arg5) const)
{ return bound_const_mem_functor5<T_return, T_obj, T_arg1,T_arg2,T_arg3,T_arg4,T_arg5>(_A_obj, _A_func); }

/** Creates a functor of type sigc::bound_const_mem_functor5 which encapsulates a method and an object instance.
 * @param _A_obj Reference to object instance the functor should operate on.
 * @param _A_func Pointer to method that should be wrapped.
 * @return Functor that executes @e _A_func on invokation.
 *
 * @ingroup mem_fun
 */
template <class T_arg1,class T_arg2,class T_arg3,class T_arg4,class T_arg5, class T_return, class T_obj, class T_obj2>
inline bound_const_mem_functor5<T_return, T_obj, T_arg1,T_arg2,T_arg3,T_arg4,T_arg5>
mem_fun(/*const*/ T_obj& _A_obj, T_return (T_obj2::*_A_func)(T_arg1,T_arg2,T_arg3,T_arg4,T_arg5) const)
{ return bound_const_mem_functor5<T_return, T_obj, T_arg1,T_arg2,T_arg3,T_arg4,T_arg5>(_A_obj, _A_func); }

/** Creates a functor of type sigc::bound_const_mem_functor6 which encapsulates a method and an object instance.
 * @param _A_obj Pointer to object instance the functor should operate on.
 * @param _A_func Pointer to method that should be wrapped.
 * @return Functor that executes @e _A_func on invokation.
 *
 * @ingroup mem_fun
 */
template <class T_arg1,class T_arg2,class T_arg3,class T_arg4,class T_arg5,class T_arg6, class T_return, class T_obj, class T_obj2>
inline bound_const_mem_functor6<T_return, T_obj, T_arg1,T_arg2,T_arg3,T_arg4,T_arg5,T_arg6>
mem_fun(/*const*/ T_obj* _A_obj, T_return (T_obj2::*_A_func)(T_arg1,T_arg2,T_arg3,T_arg4,T_arg5,T_arg6) const)
{ return bound_const_mem_functor6<T_return, T_obj, T_arg1,T_arg2,T_arg3,T_arg4,T_arg5,T_arg6>(_A_obj, _A_func); }

/** Creates a functor of type sigc::bound_const_mem_functor6 which encapsulates a method and an object instance.
 * @param _A_obj Reference to object instance the functor should operate on.
 * @param _A_func Pointer to method that should be wrapped.
 * @return Functor that executes @e _A_func on invokation.
 *
 * @ingroup mem_fun
 */
template <class T_arg1,class T_arg2,class T_arg3,class T_arg4,class T_arg5,class T_arg6, class T_return, class T_obj, class T_obj2>
inline bound_const_mem_functor6<T_return, T_obj, T_arg1,T_arg2,T_arg3,T_arg4,T_arg5,T_arg6>
mem_fun(/*const*/ T_obj& _A_obj, T_return (T_obj2::*_A_func)(T_arg1,T_arg2,T_arg3,T_arg4,T_arg5,T_arg6) const)
{ return bound_const_mem_functor6<T_return, T_obj, T_arg1,T_arg2,T_arg3,T_arg4,T_arg5,T_arg6>(_A_obj, _A_func); }

/** Creates a functor of type sigc::bound_const_mem_functor7 which encapsulates a method and an object instance.
 * @param _A_obj Pointer to object instance the functor should operate on.
 * @param _A_func Pointer to method that should be wrapped.
 * @return Functor that executes @e _A_func on invokation.
 *
 * @ingroup mem_fun
 */
template <class T_arg1,class T_arg2,class T_arg3,class T_arg4,class T_arg5,class T_arg6,class T_arg7, class T_return, class T_obj, class T_obj2>
inline bound_const_mem_functor7<T_return, T_obj, T_arg1,T_arg2,T_arg3,T_arg4,T_arg5,T_arg6,T_arg7>
mem_fun(/*const*/ T_obj* _A_obj, T_return (T_obj2::*_A_func)(T_arg1,T_arg2,T_arg3,T_arg4,T_arg5,T_arg6,T_arg7) const)
{ return bound_const_mem_functor7<T_return, T_obj, T_arg1,T_arg2,T_arg3,T_arg4,T_arg5,T_arg6,T_arg7>(_A_obj, _A_func); }

/** Creates a functor of type sigc::bound_const_mem_functor7 which encapsulates a method and an object instance.
 * @param _A_obj Reference to object instance the functor should operate on.
 * @param _A_func Pointer to method that should be wrapped.
 * @return Functor that executes @e _A_func on invokation.
 *
 * @ingroup mem_fun
 */
template <class T_arg1,class T_arg2,class T_arg3,class T_arg4,class T_arg5,class T_arg6,class T_arg7, class T_return, class T_obj, class T_obj2>
inline bound_const_mem_functor7<T_return, T_obj, T_arg1,T_arg2,T_arg3,T_arg4,T_arg5,T_arg6,T_arg7>
mem_fun(/*const*/ T_obj& _A_obj, T_return (T_obj2::*_A_func)(T_arg1,T_arg2,T_arg3,T_arg4,T_arg5,T_arg6,T_arg7) const)
{ return bound_const_mem_functor7<T_return, T_obj, T_arg1,T_arg2,T_arg3,T_arg4,T_arg5,T_arg6,T_arg7>(_A_obj, _A_func); }

/** Creates a functor of type sigc::bound_volatile_mem_functor0 which encapsulates a method and an object instance.
 * @param _A_obj Pointer to object instance the functor should operate on.
 * @param _A_func Pointer to method that should be wrapped.
 * @return Functor that executes @e _A_func on invokation.
 *
 * @ingroup mem_fun
 */
template <class T_return, class T_obj, class T_obj2>
inline bound_volatile_mem_functor0<T_return, T_obj>
mem_fun(/**/ T_obj* _A_obj, T_return (T_obj2::*_A_func)() volatile)
{ return bound_volatile_mem_functor0<T_return, T_obj>(_A_obj, _A_func); }

/** Creates a functor of type sigc::bound_volatile_mem_functor0 which encapsulates a method and an object instance.
 * @param _A_obj Reference to object instance the functor should operate on.
 * @param _A_func Pointer to method that should be wrapped.
 * @return Functor that executes @e _A_func on invokation.
 *
 * @ingroup mem_fun
 */
template <class T_return, class T_obj, class T_obj2>
inline bound_volatile_mem_functor0<T_return, T_obj>
mem_fun(/**/ T_obj& _A_obj, T_return (T_obj2::*_A_func)() volatile)
{ return bound_volatile_mem_functor0<T_return, T_obj>(_A_obj, _A_func); }

/** Creates a functor of type sigc::bound_volatile_mem_functor1 which encapsulates a method and an object instance.
 * @param _A_obj Pointer to object instance the functor should operate on.
 * @param _A_func Pointer to method that should be wrapped.
 * @return Functor that executes @e _A_func on invokation.
 *
 * @ingroup mem_fun
 */
template <class T_arg1, class T_return, class T_obj, class T_obj2>
inline bound_volatile_mem_functor1<T_return, T_obj, T_arg1>
mem_fun(/**/ T_obj* _A_obj, T_return (T_obj2::*_A_func)(T_arg1) volatile)
{ return bound_volatile_mem_functor1<T_return, T_obj, T_arg1>(_A_obj, _A_func); }

/** Creates a functor of type sigc::bound_volatile_mem_functor1 which encapsulates a method and an object instance.
 * @param _A_obj Reference to object instance the functor should operate on.
 * @param _A_func Pointer to method that should be wrapped.
 * @return Functor that executes @e _A_func on invokation.
 *
 * @ingroup mem_fun
 */
template <class T_arg1, class T_return, class T_obj, class T_obj2>
inline bound_volatile_mem_functor1<T_return, T_obj, T_arg1>
mem_fun(/**/ T_obj& _A_obj, T_return (T_obj2::*_A_func)(T_arg1) volatile)
{ return bound_volatile_mem_functor1<T_return, T_obj, T_arg1>(_A_obj, _A_func); }

/** Creates a functor of type sigc::bound_volatile_mem_functor2 which encapsulates a method and an object instance.
 * @param _A_obj Pointer to object instance the functor should operate on.
 * @param _A_func Pointer to method that should be wrapped.
 * @return Functor that executes @e _A_func on invokation.
 *
 * @ingroup mem_fun
 */
template <class T_arg1,class T_arg2, class T_return, class T_obj, class T_obj2>
inline bound_volatile_mem_functor2<T_return, T_obj, T_arg1,T_arg2>
mem_fun(/**/ T_obj* _A_obj, T_return (T_obj2::*_A_func)(T_arg1,T_arg2) volatile)
{ return bound_volatile_mem_functor2<T_return, T_obj, T_arg1,T_arg2>(_A_obj, _A_func); }

/** Creates a functor of type sigc::bound_volatile_mem_functor2 which encapsulates a method and an object instance.
 * @param _A_obj Reference to object instance the functor should operate on.
 * @param _A_func Pointer to method that should be wrapped.
 * @return Functor that executes @e _A_func on invokation.
 *
 * @ingroup mem_fun
 */
template <class T_arg1,class T_arg2, class T_return, class T_obj, class T_obj2>
inline bound_volatile_mem_functor2<T_return, T_obj, T_arg1,T_arg2>
mem_fun(/**/ T_obj& _A_obj, T_return (T_obj2::*_A_func)(T_arg1,T_arg2) volatile)
{ return bound_volatile_mem_functor2<T_return, T_obj, T_arg1,T_arg2>(_A_obj, _A_func); }

/** Creates a functor of type sigc::bound_volatile_mem_functor3 which encapsulates a method and an object instance.
 * @param _A_obj Pointer to object instance the functor should operate on.
 * @param _A_func Pointer to method that should be wrapped.
 * @return Functor that executes @e _A_func on invokation.
 *
 * @ingroup mem_fun
 */
template <class T_arg1,class T_arg2,class T_arg3, class T_return, class T_obj, class T_obj2>
inline bound_volatile_mem_functor3<T_return, T_obj, T_arg1,T_arg2,T_arg3>
mem_fun(/**/ T_obj* _A_obj, T_return (T_obj2::*_A_func)(T_arg1,T_arg2,T_arg3) volatile)
{ return bound_volatile_mem_functor3<T_return, T_obj, T_arg1,T_arg2,T_arg3>(_A_obj, _A_func); }

/** Creates a functor of type sigc::bound_volatile_mem_functor3 which encapsulates a method and an object instance.
 * @param _A_obj Reference to object instance the functor should operate on.
 * @param _A_func Pointer to method that should be wrapped.
 * @return Functor that executes @e _A_func on invokation.
 *
 * @ingroup mem_fun
 */
template <class T_arg1,class T_arg2,class T_arg3, class T_return, class T_obj, class T_obj2>
inline bound_volatile_mem_functor3<T_return, T_obj, T_arg1,T_arg2,T_arg3>
mem_fun(/**/ T_obj& _A_obj, T_return (T_obj2::*_A_func)(T_arg1,T_arg2,T_arg3) volatile)
{ return bound_volatile_mem_functor3<T_return, T_obj, T_arg1,T_arg2,T_arg3>(_A_obj, _A_func); }

/** Creates a functor of type sigc::bound_volatile_mem_functor4 which encapsulates a method and an object instance.
 * @param _A_obj Pointer to object instance the functor should operate on.
 * @param _A_func Pointer to method that should be wrapped.
 * @return Functor that executes @e _A_func on invokation.
 *
 * @ingroup mem_fun
 */
template <class T_arg1,class T_arg2,class T_arg3,class T_arg4, class T_return, class T_obj, class T_obj2>
inline bound_volatile_mem_functor4<T_return, T_obj, T_arg1,T_arg2,T_arg3,T_arg4>
mem_fun(/**/ T_obj* _A_obj, T_return (T_obj2::*_A_func)(T_arg1,T_arg2,T_arg3,T_arg4) volatile)
{ return bound_volatile_mem_functor4<T_return, T_obj, T_arg1,T_arg2,T_arg3,T_arg4>(_A_obj, _A_func); }

/** Creates a functor of type sigc::bound_volatile_mem_functor4 which encapsulates a method and an object instance.
 * @param _A_obj Reference to object instance the functor should operate on.
 * @param _A_func Pointer to method that should be wrapped.
 * @return Functor that executes @e _A_func on invokation.
 *
 * @ingroup mem_fun
 */
template <class T_arg1,class T_arg2,class T_arg3,class T_arg4, class T_return, class T_obj, class T_obj2>
inline bound_volatile_mem_functor4<T_return, T_obj, T_arg1,T_arg2,T_arg3,T_arg4>
mem_fun(/**/ T_obj& _A_obj, T_return (T_obj2::*_A_func)(T_arg1,T_arg2,T_arg3,T_arg4) volatile)
{ return bound_volatile_mem_functor4<T_return, T_obj, T_arg1,T_arg2,T_arg3,T_arg4>(_A_obj, _A_func); }

/** Creates a functor of type sigc::bound_volatile_mem_functor5 which encapsulates a method and an object instance.
 * @param _A_obj Pointer to object instance the functor should operate on.
 * @param _A_func Pointer to method that should be wrapped.
 * @return Functor that executes @e _A_func on invokation.
 *
 * @ingroup mem_fun
 */
template <class T_arg1,class T_arg2,class T_arg3,class T_arg4,class T_arg5, class T_return, class T_obj, class T_obj2>
inline bound_volatile_mem_functor5<T_return, T_obj, T_arg1,T_arg2,T_arg3,T_arg4,T_arg5>
mem_fun(/**/ T_obj* _A_obj, T_return (T_obj2::*_A_func)(T_arg1,T_arg2,T_arg3,T_arg4,T_arg5) volatile)
{ return bound_volatile_mem_functor5<T_return, T_obj, T_arg1,T_arg2,T_arg3,T_arg4,T_arg5>(_A_obj, _A_func); }

/** Creates a functor of type sigc::bound_volatile_mem_functor5 which encapsulates a method and an object instance.
 * @param _A_obj Reference to object instance the functor should operate on.
 * @param _A_func Pointer to method that should be wrapped.
 * @return Functor that executes @e _A_func on invokation.
 *
 * @ingroup mem_fun
 */
template <class T_arg1,class T_arg2,class T_arg3,class T_arg4,class T_arg5, class T_return, class T_obj, class T_obj2>
inline bound_volatile_mem_functor5<T_return, T_obj, T_arg1,T_arg2,T_arg3,T_arg4,T_arg5>
mem_fun(/**/ T_obj& _A_obj, T_return (T_obj2::*_A_func)(T_arg1,T_arg2,T_arg3,T_arg4,T_arg5) volatile)
{ return bound_volatile_mem_functor5<T_return, T_obj, T_arg1,T_arg2,T_arg3,T_arg4,T_arg5>(_A_obj, _A_func); }

/** Creates a functor of type sigc::bound_volatile_mem_functor6 which encapsulates a method and an object instance.
 * @param _A_obj Pointer to object instance the functor should operate on.
 * @param _A_func Pointer to method that should be wrapped.
 * @return Functor that executes @e _A_func on invokation.
 *
 * @ingroup mem_fun
 */
template <class T_arg1,class T_arg2,class T_arg3,class T_arg4,class T_arg5,class T_arg6, class T_return, class T_obj, class T_obj2>
inline bound_volatile_mem_functor6<T_return, T_obj, T_arg1,T_arg2,T_arg3,T_arg4,T_arg5,T_arg6>
mem_fun(/**/ T_obj* _A_obj, T_return (T_obj2::*_A_func)(T_arg1,T_arg2,T_arg3,T_arg4,T_arg5,T_arg6) volatile)
{ return bound_volatile_mem_functor6<T_return, T_obj, T_arg1,T_arg2,T_arg3,T_arg4,T_arg5,T_arg6>(_A_obj, _A_func); }

/** Creates a functor of type sigc::bound_volatile_mem_functor6 which encapsulates a method and an object instance.
 * @param _A_obj Reference to object instance the functor should operate on.
 * @param _A_func Pointer to method that should be wrapped.
 * @return Functor that executes @e _A_func on invokation.
 *
 * @ingroup mem_fun
 */
template <class T_arg1,class T_arg2,class T_arg3,class T_arg4,class T_arg5,class T_arg6, class T_return, class T_obj, class T_obj2>
inline bound_volatile_mem_functor6<T_return, T_obj, T_arg1,T_arg2,T_arg3,T_arg4,T_arg5,T_arg6>
mem_fun(/**/ T_obj& _A_obj, T_return (T_obj2::*_A_func)(T_arg1,T_arg2,T_arg3,T_arg4,T_arg5,T_arg6) volatile)
{ return bound_volatile_mem_functor6<T_return, T_obj, T_arg1,T_arg2,T_arg3,T_arg4,T_arg5,T_arg6>(_A_obj, _A_func); }

/** Creates a functor of type sigc::bound_volatile_mem_functor7 which encapsulates a method and an object instance.
 * @param _A_obj Pointer to object instance the functor should operate on.
 * @param _A_func Pointer to method that should be wrapped.
 * @return Functor that executes @e _A_func on invokation.
 *
 * @ingroup mem_fun
 */
template <class T_arg1,class T_arg2,class T_arg3,class T_arg4,class T_arg5,class T_arg6,class T_arg7, class T_return, class T_obj, class T_obj2>
inline bound_volatile_mem_functor7<T_return, T_obj, T_arg1,T_arg2,T_arg3,T_arg4,T_arg5,T_arg6,T_arg7>
mem_fun(/**/ T_obj* _A_obj, T_return (T_obj2::*_A_func)(T_arg1,T_arg2,T_arg3,T_arg4,T_arg5,T_arg6,T_arg7) volatile)
{ return bound_volatile_mem_functor7<T_return, T_obj, T_arg1,T_arg2,T_arg3,T_arg4,T_arg5,T_arg6,T_arg7>(_A_obj, _A_func); }

/** Creates a functor of type sigc::bound_volatile_mem_functor7 which encapsulates a method and an object instance.
 * @param _A_obj Reference to object instance the functor should operate on.
 * @param _A_func Pointer to method that should be wrapped.
 * @return Functor that executes @e _A_func on invokation.
 *
 * @ingroup mem_fun
 */
template <class T_arg1,class T_arg2,class T_arg3,class T_arg4,class T_arg5,class T_arg6,class T_arg7, class T_return, class T_obj, class T_obj2>
inline bound_volatile_mem_functor7<T_return, T_obj, T_arg1,T_arg2,T_arg3,T_arg4,T_arg5,T_arg6,T_arg7>
mem_fun(/**/ T_obj& _A_obj, T_return (T_obj2::*_A_func)(T_arg1,T_arg2,T_arg3,T_arg4,T_arg5,T_arg6,T_arg7) volatile)
{ return bound_volatile_mem_functor7<T_return, T_obj, T_arg1,T_arg2,T_arg3,T_arg4,T_arg5,T_arg6,T_arg7>(_A_obj, _A_func); }

/** Creates a functor of type sigc::bound_const_volatile_mem_functor0 which encapsulates a method and an object instance.
 * @param _A_obj Pointer to object instance the functor should operate on.
 * @param _A_func Pointer to method that should be wrapped.
 * @return Functor that executes @e _A_func on invokation.
 *
 * @ingroup mem_fun
 */
template <class T_return, class T_obj, class T_obj2>
inline bound_const_volatile_mem_functor0<T_return, T_obj>
mem_fun(/*const*/ T_obj* _A_obj, T_return (T_obj2::*_A_func)() const volatile)
{ return bound_const_volatile_mem_functor0<T_return, T_obj>(_A_obj, _A_func); }

/** Creates a functor of type sigc::bound_const_volatile_mem_functor0 which encapsulates a method and an object instance.
 * @param _A_obj Reference to object instance the functor should operate on.
 * @param _A_func Pointer to method that should be wrapped.
 * @return Functor that executes @e _A_func on invokation.
 *
 * @ingroup mem_fun
 */
template <class T_return, class T_obj, class T_obj2>
inline bound_const_volatile_mem_functor0<T_return, T_obj>
mem_fun(/*const*/ T_obj& _A_obj, T_return (T_obj2::*_A_func)() const volatile)
{ return bound_const_volatile_mem_functor0<T_return, T_obj>(_A_obj, _A_func); }

/** Creates a functor of type sigc::bound_const_volatile_mem_functor1 which encapsulates a method and an object instance.
 * @param _A_obj Pointer to object instance the functor should operate on.
 * @param _A_func Pointer to method that should be wrapped.
 * @return Functor that executes @e _A_func on invokation.
 *
 * @ingroup mem_fun
 */
template <class T_arg1, class T_return, class T_obj, class T_obj2>
inline bound_const_volatile_mem_functor1<T_return, T_obj, T_arg1>
mem_fun(/*const*/ T_obj* _A_obj, T_return (T_obj2::*_A_func)(T_arg1) const volatile)
{ return bound_const_volatile_mem_functor1<T_return, T_obj, T_arg1>(_A_obj, _A_func); }

/** Creates a functor of type sigc::bound_const_volatile_mem_functor1 which encapsulates a method and an object instance.
 * @param _A_obj Reference to object instance the functor should operate on.
 * @param _A_func Pointer to method that should be wrapped.
 * @return Functor that executes @e _A_func on invokation.
 *
 * @ingroup mem_fun
 */
template <class T_arg1, class T_return, class T_obj, class T_obj2>
inline bound_const_volatile_mem_functor1<T_return, T_obj, T_arg1>
mem_fun(/*const*/ T_obj& _A_obj, T_return (T_obj2::*_A_func)(T_arg1) const volatile)
{ return bound_const_volatile_mem_functor1<T_return, T_obj, T_arg1>(_A_obj, _A_func); }

/** Creates a functor of type sigc::bound_const_volatile_mem_functor2 which encapsulates a method and an object instance.
 * @param _A_obj Pointer to object instance the functor should operate on.
 * @param _A_func Pointer to method that should be wrapped.
 * @return Functor that executes @e _A_func on invokation.
 *
 * @ingroup mem_fun
 */
template <class T_arg1,class T_arg2, class T_return, class T_obj, class T_obj2>
inline bound_const_volatile_mem_functor2<T_return, T_obj, T_arg1,T_arg2>
mem_fun(/*const*/ T_obj* _A_obj, T_return (T_obj2::*_A_func)(T_arg1,T_arg2) const volatile)
{ return bound_const_volatile_mem_functor2<T_return, T_obj, T_arg1,T_arg2>(_A_obj, _A_func); }

/** Creates a functor of type sigc::bound_const_volatile_mem_functor2 which encapsulates a method and an object instance.
 * @param _A_obj Reference to object instance the functor should operate on.
 * @param _A_func Pointer to method that should be wrapped.
 * @return Functor that executes @e _A_func on invokation.
 *
 * @ingroup mem_fun
 */
template <class T_arg1,class T_arg2, class T_return, class T_obj, class T_obj2>
inline bound_const_volatile_mem_functor2<T_return, T_obj, T_arg1,T_arg2>
mem_fun(/*const*/ T_obj& _A_obj, T_return (T_obj2::*_A_func)(T_arg1,T_arg2) const volatile)
{ return bound_const_volatile_mem_functor2<T_return, T_obj, T_arg1,T_arg2>(_A_obj, _A_func); }

/** Creates a functor of type sigc::bound_const_volatile_mem_functor3 which encapsulates a method and an object instance.
 * @param _A_obj Pointer to object instance the functor should operate on.
 * @param _A_func Pointer to method that should be wrapped.
 * @return Functor that executes @e _A_func on invokation.
 *
 * @ingroup mem_fun
 */
template <class T_arg1,class T_arg2,class T_arg3, class T_return, class T_obj, class T_obj2>
inline bound_const_volatile_mem_functor3<T_return, T_obj, T_arg1,T_arg2,T_arg3>
mem_fun(/*const*/ T_obj* _A_obj, T_return (T_obj2::*_A_func)(T_arg1,T_arg2,T_arg3) const volatile)
{ return bound_const_volatile_mem_functor3<T_return, T_obj, T_arg1,T_arg2,T_arg3>(_A_obj, _A_func); }

/** Creates a functor of type sigc::bound_const_volatile_mem_functor3 which encapsulates a method and an object instance.
 * @param _A_obj Reference to object instance the functor should operate on.
 * @param _A_func Pointer to method that should be wrapped.
 * @return Functor that executes @e _A_func on invokation.
 *
 * @ingroup mem_fun
 */
template <class T_arg1,class T_arg2,class T_arg3, class T_return, class T_obj, class T_obj2>
inline bound_const_volatile_mem_functor3<T_return, T_obj, T_arg1,T_arg2,T_arg3>
mem_fun(/*const*/ T_obj& _A_obj, T_return (T_obj2::*_A_func)(T_arg1,T_arg2,T_arg3) const volatile)
{ return bound_const_volatile_mem_functor3<T_return, T_obj, T_arg1,T_arg2,T_arg3>(_A_obj, _A_func); }

/** Creates a functor of type sigc::bound_const_volatile_mem_functor4 which encapsulates a method and an object instance.
 * @param _A_obj Pointer to object instance the functor should operate on.
 * @param _A_func Pointer to method that should be wrapped.
 * @return Functor that executes @e _A_func on invokation.
 *
 * @ingroup mem_fun
 */
template <class T_arg1,class T_arg2,class T_arg3,class T_arg4, class T_return, class T_obj, class T_obj2>
inline bound_const_volatile_mem_functor4<T_return, T_obj, T_arg1,T_arg2,T_arg3,T_arg4>
mem_fun(/*const*/ T_obj* _A_obj, T_return (T_obj2::*_A_func)(T_arg1,T_arg2,T_arg3,T_arg4) const volatile)
{ return bound_const_volatile_mem_functor4<T_return, T_obj, T_arg1,T_arg2,T_arg3,T_arg4>(_A_obj, _A_func); }

/** Creates a functor of type sigc::bound_const_volatile_mem_functor4 which encapsulates a method and an object instance.
 * @param _A_obj Reference to object instance the functor should operate on.
 * @param _A_func Pointer to method that should be wrapped.
 * @return Functor that executes @e _A_func on invokation.
 *
 * @ingroup mem_fun
 */
template <class T_arg1,class T_arg2,class T_arg3,class T_arg4, class T_return, class T_obj, class T_obj2>
inline bound_const_volatile_mem_functor4<T_return, T_obj, T_arg1,T_arg2,T_arg3,T_arg4>
mem_fun(/*const*/ T_obj& _A_obj, T_return (T_obj2::*_A_func)(T_arg1,T_arg2,T_arg3,T_arg4) const volatile)
{ return bound_const_volatile_mem_functor4<T_return, T_obj, T_arg1,T_arg2,T_arg3,T_arg4>(_A_obj, _A_func); }

/** Creates a functor of type sigc::bound_const_volatile_mem_functor5 which encapsulates a method and an object instance.
 * @param _A_obj Pointer to object instance the functor should operate on.
 * @param _A_func Pointer to method that should be wrapped.
 * @return Functor that executes @e _A_func on invokation.
 *
 * @ingroup mem_fun
 */
template <class T_arg1,class T_arg2,class T_arg3,class T_arg4,class T_arg5, class T_return, class T_obj, class T_obj2>
inline bound_const_volatile_mem_functor5<T_return, T_obj, T_arg1,T_arg2,T_arg3,T_arg4,T_arg5>
mem_fun(/*const*/ T_obj* _A_obj, T_return (T_obj2::*_A_func)(T_arg1,T_arg2,T_arg3,T_arg4,T_arg5) const volatile)
{ return bound_const_volatile_mem_functor5<T_return, T_obj, T_arg1,T_arg2,T_arg3,T_arg4,T_arg5>(_A_obj, _A_func); }

/** Creates a functor of type sigc::bound_const_volatile_mem_functor5 which encapsulates a method and an object instance.
 * @param _A_obj Reference to object instance the functor should operate on.
 * @param _A_func Pointer to method that should be wrapped.
 * @return Functor that executes @e _A_func on invokation.
 *
 * @ingroup mem_fun
 */
template <class T_arg1,class T_arg2,class T_arg3,class T_arg4,class T_arg5, class T_return, class T_obj, class T_obj2>
inline bound_const_volatile_mem_functor5<T_return, T_obj, T_arg1,T_arg2,T_arg3,T_arg4,T_arg5>
mem_fun(/*const*/ T_obj& _A_obj, T_return (T_obj2::*_A_func)(T_arg1,T_arg2,T_arg3,T_arg4,T_arg5) const volatile)
{ return bound_const_volatile_mem_functor5<T_return, T_obj, T_arg1,T_arg2,T_arg3,T_arg4,T_arg5>(_A_obj, _A_func); }

/** Creates a functor of type sigc::bound_const_volatile_mem_functor6 which encapsulates a method and an object instance.
 * @param _A_obj Pointer to object instance the functor should operate on.
 * @param _A_func Pointer to method that should be wrapped.
 * @return Functor that executes @e _A_func on invokation.
 *
 * @ingroup mem_fun
 */
template <class T_arg1,class T_arg2,class T_arg3,class T_arg4,class T_arg5,class T_arg6, class T_return, class T_obj, class T_obj2>
inline bound_const_volatile_mem_functor6<T_return, T_obj, T_arg1,T_arg2,T_arg3,T_arg4,T_arg5,T_arg6>
mem_fun(/*const*/ T_obj* _A_obj, T_return (T_obj2::*_A_func)(T_arg1,T_arg2,T_arg3,T_arg4,T_arg5,T_arg6) const volatile)
{ return bound_const_volatile_mem_functor6<T_return, T_obj, T_arg1,T_arg2,T_arg3,T_arg4,T_arg5,T_arg6>(_A_obj, _A_func); }

/** Creates a functor of type sigc::bound_const_volatile_mem_functor6 which encapsulates a method and an object instance.
 * @param _A_obj Reference to object instance the functor should operate on.
 * @param _A_func Pointer to method that should be wrapped.
 * @return Functor that executes @e _A_func on invokation.
 *
 * @ingroup mem_fun
 */
template <class T_arg1,class T_arg2,class T_arg3,class T_arg4,class T_arg5,class T_arg6, class T_return, class T_obj, class T_obj2>
inline bound_const_volatile_mem_functor6<T_return, T_obj, T_arg1,T_arg2,T_arg3,T_arg4,T_arg5,T_arg6>
mem_fun(/*const*/ T_obj& _A_obj, T_return (T_obj2::*_A_func)(T_arg1,T_arg2,T_arg3,T_arg4,T_arg5,T_arg6) const volatile)
{ return bound_const_volatile_mem_functor6<T_return, T_obj, T_arg1,T_arg2,T_arg3,T_arg4,T_arg5,T_arg6>(_A_obj, _A_func); }

/** Creates a functor of type sigc::bound_const_volatile_mem_functor7 which encapsulates a method and an object instance.
 * @param _A_obj Pointer to object instance the functor should operate on.
 * @param _A_func Pointer to method that should be wrapped.
 * @return Functor that executes @e _A_func on invokation.
 *
 * @ingroup mem_fun
 */
template <class T_arg1,class T_arg2,class T_arg3,class T_arg4,class T_arg5,class T_arg6,class T_arg7, class T_return, class T_obj, class T_obj2>
inline bound_const_volatile_mem_functor7<T_return, T_obj, T_arg1,T_arg2,T_arg3,T_arg4,T_arg5,T_arg6,T_arg7>
mem_fun(/*const*/ T_obj* _A_obj, T_return (T_obj2::*_A_func)(T_arg1,T_arg2,T_arg3,T_arg4,T_arg5,T_arg6,T_arg7) const volatile)
{ return bound_const_volatile_mem_functor7<T_return, T_obj, T_arg1,T_arg2,T_arg3,T_arg4,T_arg5,T_arg6,T_arg7>(_A_obj, _A_func); }

/** Creates a functor of type sigc::bound_const_volatile_mem_functor7 which encapsulates a method and an object instance.
 * @param _A_obj Reference to object instance the functor should operate on.
 * @param _A_func Pointer to method that should be wrapped.
 * @return Functor that executes @e _A_func on invokation.
 *
 * @ingroup mem_fun
 */
template <class T_arg1,class T_arg2,class T_arg3,class T_arg4,class T_arg5,class T_arg6,class T_arg7, class T_return, class T_obj, class T_obj2>
inline bound_const_volatile_mem_functor7<T_return, T_obj, T_arg1,T_arg2,T_arg3,T_arg4,T_arg5,T_arg6,T_arg7>
mem_fun(/*const*/ T_obj& _A_obj, T_return (T_obj2::*_A_func)(T_arg1,T_arg2,T_arg3,T_arg4,T_arg5,T_arg6,T_arg7) const volatile)
{ return bound_const_volatile_mem_functor7<T_return, T_obj, T_arg1,T_arg2,T_arg3,T_arg4,T_arg5,T_arg6,T_arg7>(_A_obj, _A_func); }


} /* namespace sigc */
#endif /* _SIGC_FUNCTORS_MACROS_MEM_FUNHM4_ */
