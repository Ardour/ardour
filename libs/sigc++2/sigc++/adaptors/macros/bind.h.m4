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

define([DEDUCE_RESULT_TYPE_COUNT],[dnl
  template <LOOP(class T_arg%1, eval(CALL_SIZE))>
  struct deduce_result_type_internal<LIST($2, LOOP(T_arg%1,eval(CALL_SIZE)))>
    { typedef typename adaptor_type::template deduce_result_type<LIST(LOOP(_P_(T_arg%1), eval(CALL_SIZE-$2)), LOOP(_P_(typename unwrap_reference<T_type%1>::type), $1))>::type type; };
])
define([BIND_OPERATOR_LOCATION],[dnl
ifelse($2,1,,[dnl
  /** Invokes the wrapped functor passing on the arguments.
   * bound_ is passed as the $1[]th argument.dnl
FOR(1, eval($2-1),[
   * @param _A_arg%1 Argument to be passed on to the functor.])
   * @return The return value of the functor invocation.
   */
  template <LOOP([class T_arg%1], eval($2-1))>
  typename deduce_result_type<LOOP(T_arg%1,eval($2-1))>::type
  operator()(LOOP(T_arg%1 _A_arg%1,eval($2-1)))
    { return this->functor_.SIGC_WORKAROUND_OPERATOR_PARENTHESES<LIST(LOOP([_P_(T_arg%1)],eval($1-1)), _P_(typename unwrap_reference<T_bound>::type), FOR($1,eval($2-1),[_P_(T_arg%1),]))>
        (LIST(LOOP(_A_arg%1,eval($1-1)), bound_.invoke(), FOR($1,eval($2-1),[_A_arg%1,])));
    }

  #ifndef SIGC_TEMPLATE_SPECIALIZATION_OPERATOR_OVERLOAD
  template <LOOP([class T_arg%1], eval($2-1))>
  typename deduce_result_type<LOOP(T_arg%1,eval($2-1))>::type
  sun_forte_workaround(LOOP(T_arg%1 _A_arg%1,eval($2-1)))
    { return this->functor_.SIGC_WORKAROUND_OPERATOR_PARENTHESES<LIST(LOOP([_P_(T_arg%1)],eval($1-1)), _P_(typename unwrap_reference<T_bound>::type), FOR($1,eval($2-1),[_P_(T_arg%1),]))>
        (LIST(LOOP(_A_arg%1,eval($1-1)), bound_.invoke(), FOR($1,eval($2-1),[_A_arg%1,])));
    }
  #endif
    
])dnl
])
define([BIND_OPERATOR_COUNT],[dnl
  /** Invokes the wrapped functor passing on the arguments.
   * The last $1 argument(s) are fixed.dnl
FOR(1, eval($2-1),[
   * @param _A_arg%1 Argument to be passed on to the functor.])
   * @return The return value of the functor invocation.
   */
  template <LOOP([class T_arg%1], eval($2-1))>
  typename deduce_result_type<LOOP(T_arg%1,eval($2-1))>::type
  operator()(LOOP(T_arg%1 _A_arg%1, eval($2-1)))
    { return this->functor_.SIGC_WORKAROUND_OPERATOR_PARENTHESES<LIST(LOOP([_P_(T_arg%1)],eval($2-1)), LOOP(_P_(typename unwrap_reference<T_type%1>::type), $1))>
        (LIST(LOOP(_A_arg%1,eval($2-1)), LOOP(bound%1_.invoke(), $1)));
    }

  #ifndef SIGC_TEMPLATE_SPECIALIZATION_OPERATOR_OVERLOAD
  template <LOOP([class T_arg%1], eval($2-1))>
  typename deduce_result_type<LOOP(T_arg%1,eval($2-1))>::type
  sun_forte_workaround(LOOP(T_arg%1 _A_arg%1, eval($2-1)))
    { return this->functor_.SIGC_WORKAROUND_OPERATOR_PARENTHESES<LIST(LOOP([_P_(T_arg%1)],eval($2-1)), LOOP(_P_(typename unwrap_reference<T_type%1>::type), $1))>
        (LIST(LOOP(_A_arg%1,eval($2-1)), LOOP(bound%1_.invoke(), $1)));
    }
  #endif
    
])
define([BIND_FUNCTOR_LOCATION],[dnl
/** Adaptor that binds an argument to the wrapped functor.
 * This template specialization fixes the eval($1+1)[]th argument of the wrapped functor.
 *
 * @ingroup bind
 */
template <class T_functor, class T_bound>
struct bind_functor<$1, T_functor, T_bound, LIST(LOOP(nil, CALL_SIZE - 1))> : public adapts<T_functor>
{
  typedef typename adapts<T_functor>::adaptor_type adaptor_type;

  template <LOOP(class T_arg%1=void, eval(CALL_SIZE))>
  struct deduce_result_type
    { typedef typename adaptor_type::template deduce_result_type<LIST(LOOP(_P_(T_arg%1),eval($1)), _P_(typename unwrap_reference<T_bound>::type), FOR(eval($1+1),eval(CALL_SIZE-1),[_P_(T_arg%1),]))>::type type; };
  typedef typename adaptor_type::result_type  result_type;

  /** Invokes the wrapped functor passing on the bound argument only.
   * @return The return value of the functor invocation.
   */
  result_type
  operator()()
  {
    //Note: The AIX compiler sometimes gives linker errors if we do not define this in the class.
    return this->functor_.SIGC_WORKAROUND_OPERATOR_PARENTHESES<_P_(typename unwrap_reference<T_bound>::type)> (bound_.invoke());
  }

FOR(eval($1+1),CALL_SIZE,[[BIND_OPERATOR_LOCATION(eval($1+1),%1)]])dnl
  /** Constructs a bind_functor object that binds an argument to the passed functor.
   * @param _A_functor Functor to invoke from operator()().
   * @param _A_bound Argument to bind to the functor.
   */
  bind_functor(_R_(T_functor) _A_func, _R_(T_bound) _A_bound)
    : adapts<T_functor>(_A_func), bound_(_A_bound)
    {}

  /// The argument bound to the functor.
  bound_argument<T_bound> bound_;
};

])
define([BIND_FUNCTOR_COUNT],[dnl
/** Adaptor that binds $1 argument(s) to the wrapped functor.
 * This template specialization fixes the last $1 argument(s) of the wrapped functor.
 *
 * @ingroup bind
 */
template <LIST(class T_functor, LOOP(class T_type%1, $1))>
struct bind_functor<LIST(-1, T_functor, LIST(LOOP(T_type%1, $1), LOOP(nil, CALL_SIZE - $1)))> : public adapts<T_functor>
{
  typedef typename adapts<T_functor>::adaptor_type adaptor_type;

#ifndef DOXYGEN_SHOULD_SKIP_THIS
  template <LIST(int count, LOOP(class T_arg%1, eval(CALL_SIZE)))>
  struct deduce_result_type_internal
    { typedef typename adaptor_type::template deduce_result_type<LIST(LOOP(_P_(T_arg%1), eval(CALL_SIZE-$1)), LOOP(_P_(typename unwrap_reference<T_type%1>::type), $1))>::type type; };
FOR(eval($1+1),eval(CALL_SIZE-1),[[DEDUCE_RESULT_TYPE_COUNT($1,%1)]])dnl
#endif /*DOXYGEN_SHOULD_SKIP_THIS*/

  template <LOOP(class T_arg%1=void, eval(CALL_SIZE))>
  struct deduce_result_type {
    typedef typename deduce_result_type_internal<internal::count_void<LOOP(T_arg%1, eval(CALL_SIZE))>::value,
                                                 LOOP(T_arg%1, eval(CALL_SIZE))>::type type;
  };
  typedef typename adaptor_type::result_type  result_type;

  /** Invokes the wrapped functor passing on the bound argument only.
   * @return The return value of the functor invocation.
   */
  result_type
  operator()()
  {
    //Note: The AIX compiler sometimes gives linker errors if we do not define this in the class.
    return this->functor_.SIGC_WORKAROUND_OPERATOR_PARENTHESES<LOOP(_P_(typename unwrap_reference<T_type%1>::type), $1)> (LOOP(bound%1_.invoke(), $1));
  }

FOR(2,eval(CALL_SIZE-$1+1),[[BIND_OPERATOR_COUNT($1,%1)]])dnl
  /** Constructs a bind_functor object that binds an argument to the passed functor.
   * @param _A_functor Functor to invoke from operator()().
   * @param _A_bound Argument to bind to the functor.
   */
  bind_functor(_R_(T_functor) _A_func, LOOP(_R_(T_type%1) _A_bound%1, $1))
    : adapts<T_functor>(_A_func), LOOP(bound%1_(_A_bound%1), $1)
    {}

  /// The argument bound to the functor.dnl
FOR(1,$1,[
  bound_argument<T_type%1> bound%1_;])
};


//template specialization of visit_each<>(action, functor):
/** Performs a functor on each of the targets of a functor.
 * The function overload for sigc::bind_functor performs a functor on the
 * functor and on the object instances stored in the sigc::bind_functor object.
 *
 * @ingroup bind
 */
template <class T_action, class T_functor, LOOP(class T_type%1, $1)>
void visit_each(const T_action& _A_action,
                const bind_functor<-1, T_functor, LOOP(T_type%1, $1)>& _A_target)
{
  visit_each(_A_action, _A_target.functor_);dnl
FOR(1,$1,[
  visit_each(_A_action, _A_target.bound%1_);])
}

])
define([BIND_COUNT],[dnl
/** Creates an adaptor of type sigc::bind_functor which fixes the last $1 argument(s) of the passed functor.
 * This function overload fixes the last $1 argument(s) of @e _A_func.
 *
 * @param _A_func Functor that should be wrapped.dnl
FOR(1,$1,[
 * @param _A_b%1 Argument to bind to @e _A_func.])
 * @return Adaptor that executes _A_func with the bound argument on invokation.
 *
 * @ingroup bind
 */
template <LIST(LOOP(class T_type%1, $1), class T_functor)>
inline bind_functor<-1, T_functor,dnl
FOR(1,eval($1-1),[
                    T_type%1,])
                    T_type$1>
bind(const T_functor& _A_func, LOOP(T_type%1 _A_b%1, $1))
{ return bind_functor<-1, T_functor,dnl
FOR(1,eval($1-1),[
                    T_type%1,])
                    T_type$1>
                    (_A_func, LOOP(_A_b%1, $1));
}

])

divert(0)dnl
__FIREWALL__
#include <sigc++/adaptors/adaptor_trait.h>
#include <sigc++/adaptors/bound_argument.h>

namespace sigc { 

#ifndef DOXYGEN_SHOULD_SKIP_THIS

namespace internal {

template <class T_arg1,class T_arg2,class T_arg3,class T_arg4,class T_arg5,class T_arg6,class T_arg7>
struct count_void
  { static const int value=0; };
template <class T_arg1,class T_arg2,class T_arg3,class T_arg4,class T_arg5,class T_arg6>
struct count_void<T_arg1,T_arg2,T_arg3,T_arg4,T_arg5,T_arg6,void>
  { static const int value=1; };
template <class T_arg1,class T_arg2,class T_arg3,class T_arg4,class T_arg5>
struct count_void<T_arg1,T_arg2,T_arg3,T_arg4,T_arg5,void,void>
  { static const int value=2; };
template <class T_arg1,class T_arg2,class T_arg3,class T_arg4>
struct count_void<T_arg1,T_arg2,T_arg3,T_arg4,void,void,void>
  { static const int value=3; };
template <class T_arg1,class T_arg2,class T_arg3>
struct count_void<T_arg1,T_arg2,T_arg3,void,void,void,void>
  { static const int value=4; };
template <class T_arg1,class T_arg2>
struct count_void<T_arg1,T_arg2,void,void,void,void,void>
  { static const int value=5; };
template <class T_arg1>
struct count_void<T_arg1,void,void,void,void,void,void>
  { static const int value=6; };
template <>
struct count_void<void,void,void,void,void,void,void>
  { static const int value=7; };

} /* namespace internal */

#endif /*DOXYGEN_SHOULD_SKIP_THIS*/


/** @defgroup bind bind(), bind_return()
 * sigc::bind() alters an arbitrary functor by fixing arguments to certain values.
 * Up to CALL_SIZE arguments can be bound at a time.
 * For single argument binding overloads of sigc::bind() are provided that let you
 * specify the zero-based position of the argument to fix with the first template parameter.
 * (A value of @p -1 fixes the last argument so sigc::bind<-1>() gives the same result as sigc::bind().)
 * The types of the arguments can optionally be specified if not deduced.
 *
 * @par Examples:
 *   @code
 *   void foo(int, int, int);
 *   // single argument binding ...
 *   sigc::bind(&foo,1)(2,3);     //fixes the last (third) argument and calls foo(2,3,1)
 *   sigc::bind<-1>(&foo,1)(2,3); //same as bind(&foo,1)(2,3) (calls foo(2,3,1))
 *   sigc::bind<0>(&foo,1)(2,3);  //fixes the first argument and calls foo(1,2,3)
 *   sigc::bind<1>(&foo,1)(2,3);  //fixes the second argument and calls foo(2,1,3)
 *   sigc::bind<2>(&foo,1)(2,3);  //fixes the third argument and calls foo(2,3,1)
 *   // multi argument binding ...
 *   sigc::bind(&foo,1,2)(3);     //fixes the last two arguments and calls foo(3,1,2)
 *   sigc::bind(&foo,1,2,3)();    //fixes all three arguments and calls foo(1,2,3)
 *   @endcode
 *
 * The functor sigc::bind() returns can be passed into
 * sigc::signal::connect() directly.
 *
 * @par Example:
 *   @code
 *   sigc::signal<void> some_signal;
 *   void foo(int);
 *   some_signal.connect(sigc::bind(&foo,1));
 *   @endcode
 *
 * sigc::bind_return() alters an arbitrary functor by
 * fixing its return value to a certain value.
 *
 * @par Example:
 *   @code
 *   void foo();
 *   std::cout << sigc::bind_return(&foo, 5)(); // calls foo() and returns 5
 *   @endcode
 *
 * You can bind references to functors by passing the objects through
 * the sigc::ref() helper function.
 *
 * @par Example:
 *   @code
 *   int some_int;
 *   sigc::signal<void> some_signal;
 *   void foo(int&);
 *   some_signal.connect(sigc::bind(&foo,sigc::ref(some_int)));
 *   @endcode
 *
 * If you bind an object of a sigc::trackable derived type to a functor
 * by reference, a slot assigned to the bind adaptor is cleared automatically
 * when the object goes out of scope.
 *
 * @par Example:
 *   @code
 *   struct bar : public sigc::trackable {} some_bar;
 *   sigc::signal<void> some_signal;
 *   void foo(bar&);
 *   some_signal.connect(sigc::bind(&foo,sigc::ref(some_bar)));
 *     // disconnected automatically if some_bar goes out of scope
 *   @endcode
 *
 * For a more powerful version of this functionality see the lambda
 * library adaptor sigc::group() which can bind, hide and reorder
 * arguments arbitrarily.  Although sigc::group() is more flexible,
 * sigc::bind() provides a means of binding parameters when then total
 * number of parameters called is variable.
 *
 * @ingroup adaptors
 */

/** Adaptor that binds an argument to the wrapped functor.
 * Use the convenience function sigc::bind() to create an instance of sigc::bind_functor.
 *
 * The following template arguments are used:
 * - @e I_location Zero-based position of the argument to fix (@p -1 for the last argument).
FOR(1, CALL_SIZE,[
 * - @e T_type%1 Type of the %1st bound argument.])
 * - @e T_functor Type of the functor to wrap.
 *
 * @ingroup bind
 */
template <LIST(int I_location, class T_functor, LOOP(class T_type%1=nil, CALL_SIZE))>
struct bind_functor;

FOR(0,eval(CALL_SIZE-1),[[BIND_FUNCTOR_LOCATION(%1)]])dnl

//template specialization of visit_each<>(action, functor):
/** Performs a functor on each of the targets of a functor.
 * The function overload for sigc::bind_functor performs a functor on the
 * functor and on the object instances stored in the sigc::bind_functor object.
 *
 * @ingroup bind
 */
template <class T_action, int T_loc, class T_functor, class T_bound>
void visit_each(const T_action& _A_action,
                const bind_functor<T_loc, T_functor, T_bound>& _A_target)
{
  visit_each(_A_action, _A_target.functor_);
  visit_each(_A_action, _A_target.bound_);
}

FOR(1,CALL_SIZE,[[BIND_FUNCTOR_COUNT(%1)]])dnl

/** Creates an adaptor of type sigc::bind_functor which binds the passed argument to the passed functor.
 * The optional template argument @e I_location specifies the zero-based
 * position of the argument to be fixed (@p -1 stands for the last argument).
 *
 * @param _A_func Functor that should be wrapped.
 * @param _A_b1 Argument to bind to @e _A_func.
 * @return Adaptor that executes @e _A_func with the bound argument on invokation.
 *
 * @ingroup bind
 */
template <int I_location, class T_bound1, class T_functor>
inline bind_functor<I_location, T_functor, T_bound1>
bind(const T_functor& _A_func, T_bound1 _A_b1)
{ 
  return bind_functor<I_location, T_functor, T_bound1>
           (_A_func, _A_b1);
}

FOR(1,CALL_SIZE,[[BIND_COUNT(%1)]])dnl

} /* namespace sigc */
