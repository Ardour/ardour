// -*- c++ -*-
/* Do not edit! -- generated file */

#ifndef _SIGC_FUNCTORS_MACROS_PTR_FUNHM4_
#define _SIGC_FUNCTORS_MACROS_PTR_FUNHM4_
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

/** pointer_functor0 wraps existing non-member functions with 0 argument(s).
 * Use the convenience function ptr_fun() to create an instance of pointer_functor0.
 *
 * The following template arguments are used:
 * - @e T_return The return type of operator()().
 *
 * @ingroup ptr_fun
 */
template <class T_return>
class pointer_functor0 : public functor_base
{
  typedef T_return (*function_type)();
protected: 
  function_type func_ptr_;
public:
  typedef T_return result_type;

  /// Constructs an invalid functor.
  pointer_functor0() {}

  /** Constructs a pointer_functor0 object that wraps an existing function.
   * @param _A_func Pointer to function that will be invoked from operator()().
   */
  explicit pointer_functor0(function_type _A_func): func_ptr_(_A_func) {}

  /** Execute the wrapped function.
   * @return The return value of the function invocation.
   */
  T_return operator()() const 
    { return func_ptr_(); }
};

/** pointer_functor1 wraps existing non-member functions with 1 argument(s).
 * Use the convenience function ptr_fun() to create an instance of pointer_functor1.
 *
 * The following template arguments are used:
 * - @e T_arg1 Argument type used in the definition of operator()().
 * - @e T_return The return type of operator()().
 *
 * @ingroup ptr_fun
 */
template <class T_arg1, class T_return>
class pointer_functor1 : public functor_base
{
  typedef T_return (*function_type)(T_arg1);
protected: 
  function_type func_ptr_;
public:
  typedef T_return result_type;

  /// Constructs an invalid functor.
  pointer_functor1() {}

  /** Constructs a pointer_functor1 object that wraps an existing function.
   * @param _A_func Pointer to function that will be invoked from operator()().
   */
  explicit pointer_functor1(function_type _A_func): func_ptr_(_A_func) {}

  /** Execute the wrapped function.
   * @param _A_a1 Argument to be passed on to the function.
   * @return The return value of the function invocation.
   */
  T_return operator()(typename type_trait<T_arg1>::take _A_a1) const 
    { return func_ptr_(_A_a1); }
};

/** pointer_functor2 wraps existing non-member functions with 2 argument(s).
 * Use the convenience function ptr_fun() to create an instance of pointer_functor2.
 *
 * The following template arguments are used:
 * - @e T_arg1 Argument type used in the definition of operator()().
 * - @e T_arg2 Argument type used in the definition of operator()().
 * - @e T_return The return type of operator()().
 *
 * @ingroup ptr_fun
 */
template <class T_arg1,class T_arg2, class T_return>
class pointer_functor2 : public functor_base
{
  typedef T_return (*function_type)(T_arg1,T_arg2);
protected: 
  function_type func_ptr_;
public:
  typedef T_return result_type;

  /// Constructs an invalid functor.
  pointer_functor2() {}

  /** Constructs a pointer_functor2 object that wraps an existing function.
   * @param _A_func Pointer to function that will be invoked from operator()().
   */
  explicit pointer_functor2(function_type _A_func): func_ptr_(_A_func) {}

  /** Execute the wrapped function.
   * @param _A_a1 Argument to be passed on to the function.
   * @param _A_a2 Argument to be passed on to the function.
   * @return The return value of the function invocation.
   */
  T_return operator()(typename type_trait<T_arg1>::take _A_a1,typename type_trait<T_arg2>::take _A_a2) const 
    { return func_ptr_(_A_a1,_A_a2); }
};

/** pointer_functor3 wraps existing non-member functions with 3 argument(s).
 * Use the convenience function ptr_fun() to create an instance of pointer_functor3.
 *
 * The following template arguments are used:
 * - @e T_arg1 Argument type used in the definition of operator()().
 * - @e T_arg2 Argument type used in the definition of operator()().
 * - @e T_arg3 Argument type used in the definition of operator()().
 * - @e T_return The return type of operator()().
 *
 * @ingroup ptr_fun
 */
template <class T_arg1,class T_arg2,class T_arg3, class T_return>
class pointer_functor3 : public functor_base
{
  typedef T_return (*function_type)(T_arg1,T_arg2,T_arg3);
protected: 
  function_type func_ptr_;
public:
  typedef T_return result_type;

  /// Constructs an invalid functor.
  pointer_functor3() {}

  /** Constructs a pointer_functor3 object that wraps an existing function.
   * @param _A_func Pointer to function that will be invoked from operator()().
   */
  explicit pointer_functor3(function_type _A_func): func_ptr_(_A_func) {}

  /** Execute the wrapped function.
   * @param _A_a1 Argument to be passed on to the function.
   * @param _A_a2 Argument to be passed on to the function.
   * @param _A_a3 Argument to be passed on to the function.
   * @return The return value of the function invocation.
   */
  T_return operator()(typename type_trait<T_arg1>::take _A_a1,typename type_trait<T_arg2>::take _A_a2,typename type_trait<T_arg3>::take _A_a3) const 
    { return func_ptr_(_A_a1,_A_a2,_A_a3); }
};

/** pointer_functor4 wraps existing non-member functions with 4 argument(s).
 * Use the convenience function ptr_fun() to create an instance of pointer_functor4.
 *
 * The following template arguments are used:
 * - @e T_arg1 Argument type used in the definition of operator()().
 * - @e T_arg2 Argument type used in the definition of operator()().
 * - @e T_arg3 Argument type used in the definition of operator()().
 * - @e T_arg4 Argument type used in the definition of operator()().
 * - @e T_return The return type of operator()().
 *
 * @ingroup ptr_fun
 */
template <class T_arg1,class T_arg2,class T_arg3,class T_arg4, class T_return>
class pointer_functor4 : public functor_base
{
  typedef T_return (*function_type)(T_arg1,T_arg2,T_arg3,T_arg4);
protected: 
  function_type func_ptr_;
public:
  typedef T_return result_type;

  /// Constructs an invalid functor.
  pointer_functor4() {}

  /** Constructs a pointer_functor4 object that wraps an existing function.
   * @param _A_func Pointer to function that will be invoked from operator()().
   */
  explicit pointer_functor4(function_type _A_func): func_ptr_(_A_func) {}

  /** Execute the wrapped function.
   * @param _A_a1 Argument to be passed on to the function.
   * @param _A_a2 Argument to be passed on to the function.
   * @param _A_a3 Argument to be passed on to the function.
   * @param _A_a4 Argument to be passed on to the function.
   * @return The return value of the function invocation.
   */
  T_return operator()(typename type_trait<T_arg1>::take _A_a1,typename type_trait<T_arg2>::take _A_a2,typename type_trait<T_arg3>::take _A_a3,typename type_trait<T_arg4>::take _A_a4) const 
    { return func_ptr_(_A_a1,_A_a2,_A_a3,_A_a4); }
};

/** pointer_functor5 wraps existing non-member functions with 5 argument(s).
 * Use the convenience function ptr_fun() to create an instance of pointer_functor5.
 *
 * The following template arguments are used:
 * - @e T_arg1 Argument type used in the definition of operator()().
 * - @e T_arg2 Argument type used in the definition of operator()().
 * - @e T_arg3 Argument type used in the definition of operator()().
 * - @e T_arg4 Argument type used in the definition of operator()().
 * - @e T_arg5 Argument type used in the definition of operator()().
 * - @e T_return The return type of operator()().
 *
 * @ingroup ptr_fun
 */
template <class T_arg1,class T_arg2,class T_arg3,class T_arg4,class T_arg5, class T_return>
class pointer_functor5 : public functor_base
{
  typedef T_return (*function_type)(T_arg1,T_arg2,T_arg3,T_arg4,T_arg5);
protected: 
  function_type func_ptr_;
public:
  typedef T_return result_type;

  /// Constructs an invalid functor.
  pointer_functor5() {}

  /** Constructs a pointer_functor5 object that wraps an existing function.
   * @param _A_func Pointer to function that will be invoked from operator()().
   */
  explicit pointer_functor5(function_type _A_func): func_ptr_(_A_func) {}

  /** Execute the wrapped function.
   * @param _A_a1 Argument to be passed on to the function.
   * @param _A_a2 Argument to be passed on to the function.
   * @param _A_a3 Argument to be passed on to the function.
   * @param _A_a4 Argument to be passed on to the function.
   * @param _A_a5 Argument to be passed on to the function.
   * @return The return value of the function invocation.
   */
  T_return operator()(typename type_trait<T_arg1>::take _A_a1,typename type_trait<T_arg2>::take _A_a2,typename type_trait<T_arg3>::take _A_a3,typename type_trait<T_arg4>::take _A_a4,typename type_trait<T_arg5>::take _A_a5) const 
    { return func_ptr_(_A_a1,_A_a2,_A_a3,_A_a4,_A_a5); }
};

/** pointer_functor6 wraps existing non-member functions with 6 argument(s).
 * Use the convenience function ptr_fun() to create an instance of pointer_functor6.
 *
 * The following template arguments are used:
 * - @e T_arg1 Argument type used in the definition of operator()().
 * - @e T_arg2 Argument type used in the definition of operator()().
 * - @e T_arg3 Argument type used in the definition of operator()().
 * - @e T_arg4 Argument type used in the definition of operator()().
 * - @e T_arg5 Argument type used in the definition of operator()().
 * - @e T_arg6 Argument type used in the definition of operator()().
 * - @e T_return The return type of operator()().
 *
 * @ingroup ptr_fun
 */
template <class T_arg1,class T_arg2,class T_arg3,class T_arg4,class T_arg5,class T_arg6, class T_return>
class pointer_functor6 : public functor_base
{
  typedef T_return (*function_type)(T_arg1,T_arg2,T_arg3,T_arg4,T_arg5,T_arg6);
protected: 
  function_type func_ptr_;
public:
  typedef T_return result_type;

  /// Constructs an invalid functor.
  pointer_functor6() {}

  /** Constructs a pointer_functor6 object that wraps an existing function.
   * @param _A_func Pointer to function that will be invoked from operator()().
   */
  explicit pointer_functor6(function_type _A_func): func_ptr_(_A_func) {}

  /** Execute the wrapped function.
   * @param _A_a1 Argument to be passed on to the function.
   * @param _A_a2 Argument to be passed on to the function.
   * @param _A_a3 Argument to be passed on to the function.
   * @param _A_a4 Argument to be passed on to the function.
   * @param _A_a5 Argument to be passed on to the function.
   * @param _A_a6 Argument to be passed on to the function.
   * @return The return value of the function invocation.
   */
  T_return operator()(typename type_trait<T_arg1>::take _A_a1,typename type_trait<T_arg2>::take _A_a2,typename type_trait<T_arg3>::take _A_a3,typename type_trait<T_arg4>::take _A_a4,typename type_trait<T_arg5>::take _A_a5,typename type_trait<T_arg6>::take _A_a6) const 
    { return func_ptr_(_A_a1,_A_a2,_A_a3,_A_a4,_A_a5,_A_a6); }
};

/** pointer_functor7 wraps existing non-member functions with 7 argument(s).
 * Use the convenience function ptr_fun() to create an instance of pointer_functor7.
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
 *
 * @ingroup ptr_fun
 */
template <class T_arg1,class T_arg2,class T_arg3,class T_arg4,class T_arg5,class T_arg6,class T_arg7, class T_return>
class pointer_functor7 : public functor_base
{
  typedef T_return (*function_type)(T_arg1,T_arg2,T_arg3,T_arg4,T_arg5,T_arg6,T_arg7);
protected: 
  function_type func_ptr_;
public:
  typedef T_return result_type;

  /// Constructs an invalid functor.
  pointer_functor7() {}

  /** Constructs a pointer_functor7 object that wraps an existing function.
   * @param _A_func Pointer to function that will be invoked from operator()().
   */
  explicit pointer_functor7(function_type _A_func): func_ptr_(_A_func) {}

  /** Execute the wrapped function.
   * @param _A_a1 Argument to be passed on to the function.
   * @param _A_a2 Argument to be passed on to the function.
   * @param _A_a3 Argument to be passed on to the function.
   * @param _A_a4 Argument to be passed on to the function.
   * @param _A_a5 Argument to be passed on to the function.
   * @param _A_a6 Argument to be passed on to the function.
   * @param _A_a7 Argument to be passed on to the function.
   * @return The return value of the function invocation.
   */
  T_return operator()(typename type_trait<T_arg1>::take _A_a1,typename type_trait<T_arg2>::take _A_a2,typename type_trait<T_arg3>::take _A_a3,typename type_trait<T_arg4>::take _A_a4,typename type_trait<T_arg5>::take _A_a5,typename type_trait<T_arg6>::take _A_a6,typename type_trait<T_arg7>::take _A_a7) const 
    { return func_ptr_(_A_a1,_A_a2,_A_a3,_A_a4,_A_a5,_A_a6,_A_a7); }
};


// numbered ptr_fun
/** Creates a functor of type sigc::pointer_functor0 which wraps an existing non-member function.
 * @param _A_func Pointer to function that should be wrapped.
 * @return Functor that executes @e _A_func on invokation.
 *
 * @ingroup ptr_fun
 */
template <class T_return>
inline pointer_functor0<T_return> 
ptr_fun0(T_return (*_A_func)())
{ return pointer_functor0<T_return>(_A_func); }

/** Creates a functor of type sigc::pointer_functor1 which wraps an existing non-member function.
 * @param _A_func Pointer to function that should be wrapped.
 * @return Functor that executes @e _A_func on invokation.
 *
 * @ingroup ptr_fun
 */
template <class T_arg1, class T_return>
inline pointer_functor1<T_arg1, T_return> 
ptr_fun1(T_return (*_A_func)(T_arg1))
{ return pointer_functor1<T_arg1, T_return>(_A_func); }

/** Creates a functor of type sigc::pointer_functor2 which wraps an existing non-member function.
 * @param _A_func Pointer to function that should be wrapped.
 * @return Functor that executes @e _A_func on invokation.
 *
 * @ingroup ptr_fun
 */
template <class T_arg1,class T_arg2, class T_return>
inline pointer_functor2<T_arg1,T_arg2, T_return> 
ptr_fun2(T_return (*_A_func)(T_arg1,T_arg2))
{ return pointer_functor2<T_arg1,T_arg2, T_return>(_A_func); }

/** Creates a functor of type sigc::pointer_functor3 which wraps an existing non-member function.
 * @param _A_func Pointer to function that should be wrapped.
 * @return Functor that executes @e _A_func on invokation.
 *
 * @ingroup ptr_fun
 */
template <class T_arg1,class T_arg2,class T_arg3, class T_return>
inline pointer_functor3<T_arg1,T_arg2,T_arg3, T_return> 
ptr_fun3(T_return (*_A_func)(T_arg1,T_arg2,T_arg3))
{ return pointer_functor3<T_arg1,T_arg2,T_arg3, T_return>(_A_func); }

/** Creates a functor of type sigc::pointer_functor4 which wraps an existing non-member function.
 * @param _A_func Pointer to function that should be wrapped.
 * @return Functor that executes @e _A_func on invokation.
 *
 * @ingroup ptr_fun
 */
template <class T_arg1,class T_arg2,class T_arg3,class T_arg4, class T_return>
inline pointer_functor4<T_arg1,T_arg2,T_arg3,T_arg4, T_return> 
ptr_fun4(T_return (*_A_func)(T_arg1,T_arg2,T_arg3,T_arg4))
{ return pointer_functor4<T_arg1,T_arg2,T_arg3,T_arg4, T_return>(_A_func); }

/** Creates a functor of type sigc::pointer_functor5 which wraps an existing non-member function.
 * @param _A_func Pointer to function that should be wrapped.
 * @return Functor that executes @e _A_func on invokation.
 *
 * @ingroup ptr_fun
 */
template <class T_arg1,class T_arg2,class T_arg3,class T_arg4,class T_arg5, class T_return>
inline pointer_functor5<T_arg1,T_arg2,T_arg3,T_arg4,T_arg5, T_return> 
ptr_fun5(T_return (*_A_func)(T_arg1,T_arg2,T_arg3,T_arg4,T_arg5))
{ return pointer_functor5<T_arg1,T_arg2,T_arg3,T_arg4,T_arg5, T_return>(_A_func); }

/** Creates a functor of type sigc::pointer_functor6 which wraps an existing non-member function.
 * @param _A_func Pointer to function that should be wrapped.
 * @return Functor that executes @e _A_func on invokation.
 *
 * @ingroup ptr_fun
 */
template <class T_arg1,class T_arg2,class T_arg3,class T_arg4,class T_arg5,class T_arg6, class T_return>
inline pointer_functor6<T_arg1,T_arg2,T_arg3,T_arg4,T_arg5,T_arg6, T_return> 
ptr_fun6(T_return (*_A_func)(T_arg1,T_arg2,T_arg3,T_arg4,T_arg5,T_arg6))
{ return pointer_functor6<T_arg1,T_arg2,T_arg3,T_arg4,T_arg5,T_arg6, T_return>(_A_func); }

/** Creates a functor of type sigc::pointer_functor7 which wraps an existing non-member function.
 * @param _A_func Pointer to function that should be wrapped.
 * @return Functor that executes @e _A_func on invokation.
 *
 * @ingroup ptr_fun
 */
template <class T_arg1,class T_arg2,class T_arg3,class T_arg4,class T_arg5,class T_arg6,class T_arg7, class T_return>
inline pointer_functor7<T_arg1,T_arg2,T_arg3,T_arg4,T_arg5,T_arg6,T_arg7, T_return> 
ptr_fun7(T_return (*_A_func)(T_arg1,T_arg2,T_arg3,T_arg4,T_arg5,T_arg6,T_arg7))
{ return pointer_functor7<T_arg1,T_arg2,T_arg3,T_arg4,T_arg5,T_arg6,T_arg7, T_return>(_A_func); }


// unnumbered ptr_fun
/** Creates a functor of type sigc::pointer_functor0 which wraps an existing non-member function.
 * @param _A_func Pointer to function that should be wrapped.
 * @return Functor that executes @e _A_func on invokation.
 *
 * @ingroup ptr_fun
 */
template <class T_return>
inline pointer_functor0<T_return> 
ptr_fun(T_return (*_A_func)())
{ return pointer_functor0<T_return>(_A_func); }

/** Creates a functor of type sigc::pointer_functor1 which wraps an existing non-member function.
 * @param _A_func Pointer to function that should be wrapped.
 * @return Functor that executes @e _A_func on invokation.
 *
 * @ingroup ptr_fun
 */
template <class T_arg1, class T_return>
inline pointer_functor1<T_arg1, T_return> 
ptr_fun(T_return (*_A_func)(T_arg1))
{ return pointer_functor1<T_arg1, T_return>(_A_func); }

/** Creates a functor of type sigc::pointer_functor2 which wraps an existing non-member function.
 * @param _A_func Pointer to function that should be wrapped.
 * @return Functor that executes @e _A_func on invokation.
 *
 * @ingroup ptr_fun
 */
template <class T_arg1,class T_arg2, class T_return>
inline pointer_functor2<T_arg1,T_arg2, T_return> 
ptr_fun(T_return (*_A_func)(T_arg1,T_arg2))
{ return pointer_functor2<T_arg1,T_arg2, T_return>(_A_func); }

/** Creates a functor of type sigc::pointer_functor3 which wraps an existing non-member function.
 * @param _A_func Pointer to function that should be wrapped.
 * @return Functor that executes @e _A_func on invokation.
 *
 * @ingroup ptr_fun
 */
template <class T_arg1,class T_arg2,class T_arg3, class T_return>
inline pointer_functor3<T_arg1,T_arg2,T_arg3, T_return> 
ptr_fun(T_return (*_A_func)(T_arg1,T_arg2,T_arg3))
{ return pointer_functor3<T_arg1,T_arg2,T_arg3, T_return>(_A_func); }

/** Creates a functor of type sigc::pointer_functor4 which wraps an existing non-member function.
 * @param _A_func Pointer to function that should be wrapped.
 * @return Functor that executes @e _A_func on invokation.
 *
 * @ingroup ptr_fun
 */
template <class T_arg1,class T_arg2,class T_arg3,class T_arg4, class T_return>
inline pointer_functor4<T_arg1,T_arg2,T_arg3,T_arg4, T_return> 
ptr_fun(T_return (*_A_func)(T_arg1,T_arg2,T_arg3,T_arg4))
{ return pointer_functor4<T_arg1,T_arg2,T_arg3,T_arg4, T_return>(_A_func); }

/** Creates a functor of type sigc::pointer_functor5 which wraps an existing non-member function.
 * @param _A_func Pointer to function that should be wrapped.
 * @return Functor that executes @e _A_func on invokation.
 *
 * @ingroup ptr_fun
 */
template <class T_arg1,class T_arg2,class T_arg3,class T_arg4,class T_arg5, class T_return>
inline pointer_functor5<T_arg1,T_arg2,T_arg3,T_arg4,T_arg5, T_return> 
ptr_fun(T_return (*_A_func)(T_arg1,T_arg2,T_arg3,T_arg4,T_arg5))
{ return pointer_functor5<T_arg1,T_arg2,T_arg3,T_arg4,T_arg5, T_return>(_A_func); }

/** Creates a functor of type sigc::pointer_functor6 which wraps an existing non-member function.
 * @param _A_func Pointer to function that should be wrapped.
 * @return Functor that executes @e _A_func on invokation.
 *
 * @ingroup ptr_fun
 */
template <class T_arg1,class T_arg2,class T_arg3,class T_arg4,class T_arg5,class T_arg6, class T_return>
inline pointer_functor6<T_arg1,T_arg2,T_arg3,T_arg4,T_arg5,T_arg6, T_return> 
ptr_fun(T_return (*_A_func)(T_arg1,T_arg2,T_arg3,T_arg4,T_arg5,T_arg6))
{ return pointer_functor6<T_arg1,T_arg2,T_arg3,T_arg4,T_arg5,T_arg6, T_return>(_A_func); }

/** Creates a functor of type sigc::pointer_functor7 which wraps an existing non-member function.
 * @param _A_func Pointer to function that should be wrapped.
 * @return Functor that executes @e _A_func on invokation.
 *
 * @ingroup ptr_fun
 */
template <class T_arg1,class T_arg2,class T_arg3,class T_arg4,class T_arg5,class T_arg6,class T_arg7, class T_return>
inline pointer_functor7<T_arg1,T_arg2,T_arg3,T_arg4,T_arg5,T_arg6,T_arg7, T_return> 
ptr_fun(T_return (*_A_func)(T_arg1,T_arg2,T_arg3,T_arg4,T_arg5,T_arg6,T_arg7))
{ return pointer_functor7<T_arg1,T_arg2,T_arg3,T_arg4,T_arg5,T_arg6,T_arg7, T_return>(_A_func); }


} /* namespace sigc */
#endif /* _SIGC_FUNCTORS_MACROS_PTR_FUNHM4_ */
