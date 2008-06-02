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

dnl
dnl  How to call the darn thing!
define([LAMBDA_GROUP_FACTORY],[dnl
template <class T_functor, LOOP(class T_type%1, $1)>
lambda<lambda_group$1<T_functor, LOOP(typename unwrap_reference<T_type%1>::type, $1)> >
group(const T_functor& _A_func, LOOP(T_type%1 _A_%1, $1))
{
  typedef lambda_group$1<T_functor, LOOP(typename unwrap_reference<T_type%1>::type, $1)> T_lambda;
  return lambda<T_lambda>(T_lambda(_A_func, LOOP(_A_%1, $1)));
}

])
dnl
dnl  How to call the darn thing!
define([LAMBDA_GROUP_DO],[dnl
define([_L_],[LOOP(_A_%1, $2)])dnl
define([_T_],[LOOP(T_arg%1, $2)])dnl
dnl Please someone get a gun!
  template <LOOP(class T_arg%1, $2)>
  typename deduce_result_type<LOOP(T_arg%1,$2)>::type
  operator() (LOOP(T_arg%1 _A_%1, $2)) const
    { return this->func_.SIGC_WORKAROUND_OPERATOR_PARENTHESES<LOOP([
          typename value%1_type::template deduce_result_type<LOOP(T_arg%1,$2)>::type],$1)>(LOOP([
        this->value%1_.SIGC_WORKAROUND_OPERATOR_PARENTHESES<LOOP([
          _P_(T_arg%1)],$2)>(_L_)],$1)); }

  #ifndef SIGC_TEMPLATE_SPECIALIZATION_OPERATOR_OVERLOAD
  template <LOOP(class T_arg%1, $2)>
  typename deduce_result_type<LOOP(T_arg%1,$2)>::type
  sun_forte_workaround (LOOP(T_arg%1 _A_%1, $2)) const
    { return this->func_.SIGC_WORKAROUND_OPERATOR_PARENTHESES<LOOP([
          typename value%1_type::template deduce_result_type<LOOP(T_arg%1,$2)>::type],$1)>(LOOP([
        this->value%1_.SIGC_WORKAROUND_OPERATOR_PARENTHESES<LOOP([
          _P_(T_arg%1)],$2)>(_L_)],$1)); }
  #endif //SIGC_TEMPLATE_SPECIALIZATION_OPERATOR_OVERLOAD

])
dnl
dnl This really doesn't have much to do with lambda other than
dnl holding lambdas with in itself.
define([LAMBDA_GROUP],[dnl
template <class T_functor, LOOP(class T_type%1, $1)>
struct lambda_group$1 : public lambda_base
{
  typedef typename functor_trait<T_functor>::result_type result_type;dnl
FOR(1, $1,[
  typedef typename lambda<T_type%1>::lambda_type   value%1_type;])
  typedef typename adaptor_trait<T_functor>::adaptor_type functor_type;

  template <LOOP(class T_arg%1=void,$2)>
  struct deduce_result_type
    { typedef typename functor_type::template deduce_result_type<LOOP([
          typename value%1_type::template deduce_result_type<LOOP([
            _P_(T_arg%1)],$2)>::type],$1)
        >::type type; };

  result_type
  operator ()() const;

FOR(1,CALL_SIZE,[[LAMBDA_GROUP_DO($1,%1)]])dnl
  lambda_group$1(typename type_trait<T_functor>::take _A_func, LOOP(typename type_trait<T_type%1>::take _A_%1, $1))
    : LOOP(value%1_(_A_%1), $1), func_(_A_func) {}dnl

FOR(1, $1,[
  value%1_type value%1_;])
  mutable functor_type func_;
};

template <class T_functor, LOOP(class T_type%1, $1)>
typename lambda_group$1<T_functor, LOOP(T_type%1, $1)>::result_type
lambda_group$1<T_functor, LOOP(T_type%1, $1)>::operator ()() const
  { return func_(LOOP(value%1_(), $1)); }


//template specialization of visit_each<>(action, functor):
template <class T_action, class T_functor, LOOP(class T_type%1, $1)>
void visit_each(const T_action& _A_action,
                const lambda_group$1<T_functor, LOOP(T_type%1, $1)>& _A_target)
{dnl
FOR(1, $1,[
  visit_each(_A_action, _A_target.value%1_);])
  visit_each(_A_action, _A_target.func_);
}


])
divert(0)dnl
__FIREWALL__
#include <sigc++/adaptors/lambda/base.h>

/** @defgroup group_ group()
 * sigc::group() alters an arbitrary functor by rebuilding its arguments from one or more lambda expressions.
 * For each parameter that should be passed to the wrapped functor one lambda expression
 * has to be passed into group(). Lambda selectors can be used as placeholders for the
 * arguments passed into the new functor. Arguments that don't have a placeholder in one
 * of the lambda expressions are dropped.
 *
 * @par Examples:
 *   @code
 *   void foo(int, int);
 *   int bar(int);
 *   // argument binding ...
 *   sigc::group(&foo,10,sigc::_1)(20); //fixes the first argument and calls foo(10,20)
 *   sigc::group(&foo,sigc::_1,30)(40); //fixes the second argument and calls foo(40,30)
 *   // argument reordering ...
 *   sigc::group(&foo,sigc::_2,sigc::_1)(1,2); //calls foo(2,1)
 *   // argument hiding ...
 *   sigc::group(&foo,sigc::_1,sigc::_2)(1,2,3); //calls foo(1,2)
 *   // functor composition ...
 *   sigc::group(&foo,sigc::_1,sigc::group(&bar,sigc::_2))(1,2); //calls foo(1,bar(2))
 *   // algebraic expressions ...
 *   sigc::group(&foo,sigc::_1*sigc::_2,sigc::_1/sigc::_2)(6,3); //calls foo(6*3,6/3)
 *   @endcode
 *
 * The functor sigc::group() returns can be passed into
 * sigc::signal::connect() directly.
 *
 * @par Example:
 *   @code
 *   sigc::signal<void,int,int> some_signal;
 *   void foo(int);
 *   some_signal.connect(sigc::group(&foo,sigc::_2));
 *   @endcode
 *
 * Like in sigc::bind() you can bind references to functors by passing the objects
 * through the sigc::ref() helper function.
 *
 * @par Example:
 *   @code
 *   int some_int;
 *   sigc::signal<void> some_signal;
 *   void foo(int&);
 *   some_signal.connect(sigc::group(&foo,sigc::ref(some_int)));
 *   @endcode
 *
 * If you bind an object of a sigc::trackable derived type to a functor
 * by reference, a slot assigned to the group adaptor is cleared automatically
 * when the object goes out of scope.
 *
 * @par Example:
 *   @code
 *   struct bar : public sigc::trackable {} some_bar;
 *   sigc::signal<void> some_signal;
 *   void foo(bar&);
 *   some_signal.connect(sigc::group(&foo,sigc::ref(some_bar)));
 *     // disconnected automatically if some_bar goes out of scope
 *   @endcode
 *
 * @ingroup adaptors, lambdas
 */

namespace sigc {

FOR(1,3,[[LAMBDA_GROUP(%1, CALL_SIZE)]])
FOR(1,3,[[LAMBDA_GROUP_FACTORY(%1)]])

} /* namespace sigc */
