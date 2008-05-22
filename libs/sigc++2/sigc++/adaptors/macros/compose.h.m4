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

define([COMPOSE1_OPERATOR],[dnl
  template <LOOP(class T_arg%1, $1)>
  typename deduce_result_type<LOOP(T_arg%1, $1)>::type
  operator()(LOOP(T_arg%1 _A_a%1, $1))
    { return this->functor_.SIGC_WORKAROUND_OPERATOR_PARENTHESES<typename sigc::deduce_result_type<LIST(T_getter, LOOP(T_arg%1,$1))>::type>
        (get_(LOOP(_A_a%1, $1)));
    }

])

define([COMPOSE2_OPERATOR],[dnl
  template <LOOP(class T_arg%1, $1)>
  typename deduce_result_type<LOOP(T_arg%1, $1)>::type
  operator()(LOOP(T_arg%1 _A_a%1, $1))
    { return this->functor_.SIGC_WORKAROUND_OPERATOR_PARENTHESES<typename sigc::deduce_result_type<LIST(T_getter1, LOOP(T_arg%1,$1))>::type,
                                                         typename sigc::deduce_result_type<LIST(T_getter2, LOOP(T_arg%1,$1))>::type>
        (get1_(LOOP(_A_a%1, $1)), get2_(LOOP(_A_a%1,$1)));
    }

])

divert(0)
__FIREWALL__
#include <sigc++/adaptors/adaptor_trait.h>

namespace sigc {

/** @defgroup compose compose()
 * sigc::compose() combines two or three arbitrary functors.
 * On invokation parameters are passed on to one or two getter functor(s).
 * The return value(s) are then passed on to the setter function.
 *
 * @par Examples:
 *   @code
 *   float square_root(float a)  { return sqrtf(a); }
 *   float sum(float a, float b) { return a+b; }
 *   std::cout << sigc::compose(&square_root, &sum)(9, 16); // calls square_root(sum(3,6))
 *   std::cout << sigc::compose(&sum, &square_root, &square_root)(9); // calls sum(square_root(9), square_root(9))
 *   @endcode
 *
 * The functor sigc::compose() returns can be passed into
 * sigc::signal::connect() directly.
 *
 * @par Example:
 *   @code
 *   sigc::signal<float,float,float> some_signal;
 *   some_signal.connect(sigc::compose(&square_root, &sum));
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

/** Adaptor that combines two functors.
 * Use the convenience function sigc::compose() to create an instance of sigc::compose1_functor.
 *
 * The following template arguments are used:
 * - @e T_setter Type of the setter functor to wrap.
 * - @e T_getter Type of the getter functor to wrap.
 *
 * @ingroup compose
 */
template <class T_setter, class T_getter>
struct compose1_functor : public adapts<T_setter>
{
  typedef typename adapts<T_setter>::adaptor_type adaptor_type;
  typedef T_setter setter_type;
  typedef T_getter getter_type;

  template <LOOP(class T_arg%1 = void, CALL_SIZE)>
  struct deduce_result_type
    { typedef typename adaptor_type::template deduce_result_type<
        typename sigc::deduce_result_type<LIST(T_getter, LOOP(T_arg%1,CALL_SIZE))>::type
          >::type type; };
  typedef typename adaptor_type::result_type  result_type;

  result_type
  operator()();

FOR(1,CALL_SIZE, [[COMPOSE1_OPERATOR(%1)]])dnl

  /** Constructs a compose1_functor object that combines the passed functors.
   * @param _A_setter Functor that receives the return values of the invokation of @e _A_getter1 and @e _A_getter2.
   * @param _A_getter1 Functor to invoke from operator()().
   * @param _A_getter2 Functor to invoke from operator()().
   */
  compose1_functor(const T_setter& _A_setter, const T_getter& _A_getter)
    : adapts<T_setter>(_A_setter), get_(_A_getter)
    {}

  getter_type get_; // public, so that visit_each() can access it
};

template <class T_setter, class T_getter>
typename compose1_functor<T_setter, T_getter>::result_type
compose1_functor<T_setter, T_getter>::operator()()
  { return this->functor_(get_()); }

/** Adaptor that combines three functors.
 * Use the convenience function sigc::compose() to create an instance of sigc::compose2_functor.
 *
 * The following template arguments are used:
 * - @e T_setter Type of the setter functor to wrap.
 * - @e T_getter1 Type of the first getter functor to wrap.
 * - @e T_getter2 Type of the second getter functor to wrap.
 *
 * @ingroup compose
 */
template <class T_setter, class T_getter1, class T_getter2>
struct compose2_functor : public adapts<T_setter>
{
  typedef typename adapts<T_setter>::adaptor_type adaptor_type;
  typedef T_setter setter_type;
  typedef T_getter1 getter1_type;
  typedef T_getter2 getter2_type;
  
  template <LOOP(class T_arg%1=void, CALL_SIZE)>
  struct deduce_result_type
    { typedef typename adaptor_type::template deduce_result_type<
        typename sigc::deduce_result_type<LIST(T_getter1, LOOP(T_arg%1,CALL_SIZE))>::type,
        typename sigc::deduce_result_type<LIST(T_getter2, LOOP(T_arg%1,CALL_SIZE))>::type
          >::type result_type; };
  typedef typename adaptor_type::result_type  result_type;

  result_type
  operator()();

FOR(1,CALL_SIZE,[[COMPOSE2_OPERATOR(%1)]])dnl

  /** Constructs a compose2_functor object that combines the passed functors.
   * @param _A_setter Functor that receives the return values of the invokation of @e _A_getter1 and @e _A_getter2.
   * @param _A_getter1 Functor to invoke from operator()().
   * @param _A_getter2 Functor to invoke from operator()().
   */
  compose2_functor(const T_setter& _A_setter,
                   const T_getter1& _A_getter1,
                   const T_getter2& _A_getter2)
    : adapts<T_setter>(_A_setter), get1_(_A_getter1), get2_(_A_getter2)
    {}

  getter1_type get1_; // public, so that visit_each() can access it
  getter2_type get2_; // public, so that visit_each() can access it
};

template <class T_setter, class T_getter1, class T_getter2>
typename compose2_functor<T_setter, T_getter1, T_getter2>::result_type
compose2_functor<T_setter, T_getter1, T_getter2>::operator()()
  { return this->functor_(get1_(), get2_()); }

//template specialization of visit_each<>(action, functor):
/** Performs a functor on each of the targets of a functor.
 * The function overload for sigc::compose1_functor performs a functor on the
 * functors stored in the sigc::compose1_functor object.
 *
 * @ingroup compose
 */
template <class T_action, class T_setter, class T_getter>
void visit_each(const T_action& _A_action,
                const compose1_functor<T_setter, T_getter>& _A_target)
{
  typedef compose1_functor<T_setter, T_getter> type_functor;
  
  //Note that the AIX compiler needs the actual template types of visit_each to be specified:
  typedef typename type_functor::setter_type type_functor1;
  visit_each<T_action, type_functor1>(_A_action, _A_target.functor_);
  
  typedef typename type_functor::getter_type type_functor_getter;
  visit_each<T_action, type_functor_getter>(_A_action, _A_target.get_);
}

//template specialization of visit_each<>(action, functor):
/** Performs a functor on each of the targets of a functor.
 * The function overload for sigc::compose2_functor performs a functor on the
 * functors stored in the sigc::compose2_functor object.
 *
 * @ingroup compose
 */
template <class T_action, class T_setter, class T_getter1, class T_getter2>
void visit_each(const T_action& _A_action,
                const compose2_functor<T_setter, T_getter1, T_getter2>& _A_target)
{
  typedef compose2_functor<T_setter, T_getter1, T_getter2> type_functor;
  
  //Note that the AIX compiler needs the actual template types of visit_each to be specified:
  typedef typename type_functor::setter_type type_functor1;
  visit_each<T_action, type_functor1>(_A_action, _A_target.functor_);
  
  typedef typename type_functor::getter1_type type_functor_getter1;
  visit_each<T_action, type_functor_getter1>(_A_action, _A_target.get1_);
  
  typedef typename type_functor::getter2_type type_functor_getter2;
  visit_each<T_action, type_functor_getter2>(_A_action, _A_target.get2_);
}


/** Creates an adaptor of type sigc::compose1_functor which combines two functors.
 *
 * @param _A_setter Functor that receives the return value of the invokation of @e _A_getter.
 * @param _A_getter Functor to invoke from operator()().
 * @return Adaptor that executes @e _A_setter with the value returned from invokation of @e _A_getter.
 *
 * @ingroup compose
 */
template <class T_setter, class T_getter>
inline compose1_functor<T_setter, T_getter>
compose(const T_setter& _A_setter, const T_getter& _A_getter)
  { return compose1_functor<T_setter, T_getter>(_A_setter, _A_getter); }

/** Creates an adaptor of type sigc::compose2_functor which combines three functors.
 *
 * @param _A_setter Functor that receives the return values of the invokation of @e _A_getter1 and @e _A_getter2.
 * @param _A_getter1 Functor to invoke from operator()().
 * @param _A_getter2 Functor to invoke from operator()().
 * @return Adaptor that executes @e _A_setter with the values return from invokation of @e _A_getter1 and @e _A_getter2.
 *
 * @ingroup compose
 */
template <class T_setter, class T_getter1, class T_getter2>
inline compose2_functor<T_setter, T_getter1, T_getter2>
compose(const T_setter& _A_setter, const T_getter1& _A_getter1, const T_getter2& _A_getter2)
  { return compose2_functor<T_setter, T_getter1, T_getter2>(_A_setter, _A_getter1, _A_getter2); }

} /* namespace sigc */
