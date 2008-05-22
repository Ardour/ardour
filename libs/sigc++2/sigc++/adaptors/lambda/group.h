// -*- c++ -*-
/* Do not edit! -- generated file */
#ifndef _SIGC_ADAPTORS_LAMBDA_MACROS_GROUPHM4_
#define _SIGC_ADAPTORS_LAMBDA_MACROS_GROUPHM4_
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

template <class T_functor, class T_type1>
struct lambda_group1 : public lambda_base
{
  typedef typename functor_trait<T_functor>::result_type result_type;
  typedef typename lambda<T_type1>::lambda_type   value1_type;
  typedef typename adaptor_trait<T_functor>::adaptor_type functor_type;

  template <class T_arg1=void,class T_arg2=void,class T_arg3=void,class T_arg4=void,class T_arg5=void,class T_arg6=void,class T_arg7=void>
  struct deduce_result_type
    { typedef typename functor_type::template deduce_result_type<
          typename value1_type::template deduce_result_type<
            typename type_trait<T_arg1>::pass,typename type_trait<T_arg2>::pass,typename type_trait<T_arg3>::pass,typename type_trait<T_arg4>::pass,typename type_trait<T_arg5>::pass,typename type_trait<T_arg6>::pass,typename type_trait<T_arg7>::pass>::type
        >::type type; };

  result_type
  operator ()() const;

  template <class T_arg1>
  typename deduce_result_type<T_arg1>::type
  operator() (T_arg1 _A_1) const
    { return this->func_.SIGC_WORKAROUND_OPERATOR_PARENTHESES<
          typename value1_type::template deduce_result_type<T_arg1>::type>(
        this->value1_.SIGC_WORKAROUND_OPERATOR_PARENTHESES<
          typename type_trait<T_arg1>::pass>(_A_1)); }

  #ifndef SIGC_TEMPLATE_SPECIALIZATION_OPERATOR_OVERLOAD
  template <class T_arg1>
  typename deduce_result_type<T_arg1>::type
  sun_forte_workaround (T_arg1 _A_1) const
    { return this->func_.SIGC_WORKAROUND_OPERATOR_PARENTHESES<
          typename value1_type::template deduce_result_type<T_arg1>::type>(
        this->value1_.SIGC_WORKAROUND_OPERATOR_PARENTHESES<
          typename type_trait<T_arg1>::pass>(_A_1)); }
  #endif //SIGC_TEMPLATE_SPECIALIZATION_OPERATOR_OVERLOAD

  template <class T_arg1,class T_arg2>
  typename deduce_result_type<T_arg1,T_arg2>::type
  operator() (T_arg1 _A_1,T_arg2 _A_2) const
    { return this->func_.SIGC_WORKAROUND_OPERATOR_PARENTHESES<
          typename value1_type::template deduce_result_type<T_arg1,T_arg2>::type>(
        this->value1_.SIGC_WORKAROUND_OPERATOR_PARENTHESES<
          typename type_trait<T_arg1>::pass,typename type_trait<T_arg2>::pass>(_A_1,_A_2)); }

  #ifndef SIGC_TEMPLATE_SPECIALIZATION_OPERATOR_OVERLOAD
  template <class T_arg1,class T_arg2>
  typename deduce_result_type<T_arg1,T_arg2>::type
  sun_forte_workaround (T_arg1 _A_1,T_arg2 _A_2) const
    { return this->func_.SIGC_WORKAROUND_OPERATOR_PARENTHESES<
          typename value1_type::template deduce_result_type<T_arg1,T_arg2>::type>(
        this->value1_.SIGC_WORKAROUND_OPERATOR_PARENTHESES<
          typename type_trait<T_arg1>::pass,typename type_trait<T_arg2>::pass>(_A_1,_A_2)); }
  #endif //SIGC_TEMPLATE_SPECIALIZATION_OPERATOR_OVERLOAD

  template <class T_arg1,class T_arg2,class T_arg3>
  typename deduce_result_type<T_arg1,T_arg2,T_arg3>::type
  operator() (T_arg1 _A_1,T_arg2 _A_2,T_arg3 _A_3) const
    { return this->func_.SIGC_WORKAROUND_OPERATOR_PARENTHESES<
          typename value1_type::template deduce_result_type<T_arg1,T_arg2,T_arg3>::type>(
        this->value1_.SIGC_WORKAROUND_OPERATOR_PARENTHESES<
          typename type_trait<T_arg1>::pass,typename type_trait<T_arg2>::pass,typename type_trait<T_arg3>::pass>(_A_1,_A_2,_A_3)); }

  #ifndef SIGC_TEMPLATE_SPECIALIZATION_OPERATOR_OVERLOAD
  template <class T_arg1,class T_arg2,class T_arg3>
  typename deduce_result_type<T_arg1,T_arg2,T_arg3>::type
  sun_forte_workaround (T_arg1 _A_1,T_arg2 _A_2,T_arg3 _A_3) const
    { return this->func_.SIGC_WORKAROUND_OPERATOR_PARENTHESES<
          typename value1_type::template deduce_result_type<T_arg1,T_arg2,T_arg3>::type>(
        this->value1_.SIGC_WORKAROUND_OPERATOR_PARENTHESES<
          typename type_trait<T_arg1>::pass,typename type_trait<T_arg2>::pass,typename type_trait<T_arg3>::pass>(_A_1,_A_2,_A_3)); }
  #endif //SIGC_TEMPLATE_SPECIALIZATION_OPERATOR_OVERLOAD

  template <class T_arg1,class T_arg2,class T_arg3,class T_arg4>
  typename deduce_result_type<T_arg1,T_arg2,T_arg3,T_arg4>::type
  operator() (T_arg1 _A_1,T_arg2 _A_2,T_arg3 _A_3,T_arg4 _A_4) const
    { return this->func_.SIGC_WORKAROUND_OPERATOR_PARENTHESES<
          typename value1_type::template deduce_result_type<T_arg1,T_arg2,T_arg3,T_arg4>::type>(
        this->value1_.SIGC_WORKAROUND_OPERATOR_PARENTHESES<
          typename type_trait<T_arg1>::pass,typename type_trait<T_arg2>::pass,typename type_trait<T_arg3>::pass,typename type_trait<T_arg4>::pass>(_A_1,_A_2,_A_3,_A_4)); }

  #ifndef SIGC_TEMPLATE_SPECIALIZATION_OPERATOR_OVERLOAD
  template <class T_arg1,class T_arg2,class T_arg3,class T_arg4>
  typename deduce_result_type<T_arg1,T_arg2,T_arg3,T_arg4>::type
  sun_forte_workaround (T_arg1 _A_1,T_arg2 _A_2,T_arg3 _A_3,T_arg4 _A_4) const
    { return this->func_.SIGC_WORKAROUND_OPERATOR_PARENTHESES<
          typename value1_type::template deduce_result_type<T_arg1,T_arg2,T_arg3,T_arg4>::type>(
        this->value1_.SIGC_WORKAROUND_OPERATOR_PARENTHESES<
          typename type_trait<T_arg1>::pass,typename type_trait<T_arg2>::pass,typename type_trait<T_arg3>::pass,typename type_trait<T_arg4>::pass>(_A_1,_A_2,_A_3,_A_4)); }
  #endif //SIGC_TEMPLATE_SPECIALIZATION_OPERATOR_OVERLOAD

  template <class T_arg1,class T_arg2,class T_arg3,class T_arg4,class T_arg5>
  typename deduce_result_type<T_arg1,T_arg2,T_arg3,T_arg4,T_arg5>::type
  operator() (T_arg1 _A_1,T_arg2 _A_2,T_arg3 _A_3,T_arg4 _A_4,T_arg5 _A_5) const
    { return this->func_.SIGC_WORKAROUND_OPERATOR_PARENTHESES<
          typename value1_type::template deduce_result_type<T_arg1,T_arg2,T_arg3,T_arg4,T_arg5>::type>(
        this->value1_.SIGC_WORKAROUND_OPERATOR_PARENTHESES<
          typename type_trait<T_arg1>::pass,typename type_trait<T_arg2>::pass,typename type_trait<T_arg3>::pass,typename type_trait<T_arg4>::pass,typename type_trait<T_arg5>::pass>(_A_1,_A_2,_A_3,_A_4,_A_5)); }

  #ifndef SIGC_TEMPLATE_SPECIALIZATION_OPERATOR_OVERLOAD
  template <class T_arg1,class T_arg2,class T_arg3,class T_arg4,class T_arg5>
  typename deduce_result_type<T_arg1,T_arg2,T_arg3,T_arg4,T_arg5>::type
  sun_forte_workaround (T_arg1 _A_1,T_arg2 _A_2,T_arg3 _A_3,T_arg4 _A_4,T_arg5 _A_5) const
    { return this->func_.SIGC_WORKAROUND_OPERATOR_PARENTHESES<
          typename value1_type::template deduce_result_type<T_arg1,T_arg2,T_arg3,T_arg4,T_arg5>::type>(
        this->value1_.SIGC_WORKAROUND_OPERATOR_PARENTHESES<
          typename type_trait<T_arg1>::pass,typename type_trait<T_arg2>::pass,typename type_trait<T_arg3>::pass,typename type_trait<T_arg4>::pass,typename type_trait<T_arg5>::pass>(_A_1,_A_2,_A_3,_A_4,_A_5)); }
  #endif //SIGC_TEMPLATE_SPECIALIZATION_OPERATOR_OVERLOAD

  template <class T_arg1,class T_arg2,class T_arg3,class T_arg4,class T_arg5,class T_arg6>
  typename deduce_result_type<T_arg1,T_arg2,T_arg3,T_arg4,T_arg5,T_arg6>::type
  operator() (T_arg1 _A_1,T_arg2 _A_2,T_arg3 _A_3,T_arg4 _A_4,T_arg5 _A_5,T_arg6 _A_6) const
    { return this->func_.SIGC_WORKAROUND_OPERATOR_PARENTHESES<
          typename value1_type::template deduce_result_type<T_arg1,T_arg2,T_arg3,T_arg4,T_arg5,T_arg6>::type>(
        this->value1_.SIGC_WORKAROUND_OPERATOR_PARENTHESES<
          typename type_trait<T_arg1>::pass,typename type_trait<T_arg2>::pass,typename type_trait<T_arg3>::pass,typename type_trait<T_arg4>::pass,typename type_trait<T_arg5>::pass,typename type_trait<T_arg6>::pass>(_A_1,_A_2,_A_3,_A_4,_A_5,_A_6)); }

  #ifndef SIGC_TEMPLATE_SPECIALIZATION_OPERATOR_OVERLOAD
  template <class T_arg1,class T_arg2,class T_arg3,class T_arg4,class T_arg5,class T_arg6>
  typename deduce_result_type<T_arg1,T_arg2,T_arg3,T_arg4,T_arg5,T_arg6>::type
  sun_forte_workaround (T_arg1 _A_1,T_arg2 _A_2,T_arg3 _A_3,T_arg4 _A_4,T_arg5 _A_5,T_arg6 _A_6) const
    { return this->func_.SIGC_WORKAROUND_OPERATOR_PARENTHESES<
          typename value1_type::template deduce_result_type<T_arg1,T_arg2,T_arg3,T_arg4,T_arg5,T_arg6>::type>(
        this->value1_.SIGC_WORKAROUND_OPERATOR_PARENTHESES<
          typename type_trait<T_arg1>::pass,typename type_trait<T_arg2>::pass,typename type_trait<T_arg3>::pass,typename type_trait<T_arg4>::pass,typename type_trait<T_arg5>::pass,typename type_trait<T_arg6>::pass>(_A_1,_A_2,_A_3,_A_4,_A_5,_A_6)); }
  #endif //SIGC_TEMPLATE_SPECIALIZATION_OPERATOR_OVERLOAD

  template <class T_arg1,class T_arg2,class T_arg3,class T_arg4,class T_arg5,class T_arg6,class T_arg7>
  typename deduce_result_type<T_arg1,T_arg2,T_arg3,T_arg4,T_arg5,T_arg6,T_arg7>::type
  operator() (T_arg1 _A_1,T_arg2 _A_2,T_arg3 _A_3,T_arg4 _A_4,T_arg5 _A_5,T_arg6 _A_6,T_arg7 _A_7) const
    { return this->func_.SIGC_WORKAROUND_OPERATOR_PARENTHESES<
          typename value1_type::template deduce_result_type<T_arg1,T_arg2,T_arg3,T_arg4,T_arg5,T_arg6,T_arg7>::type>(
        this->value1_.SIGC_WORKAROUND_OPERATOR_PARENTHESES<
          typename type_trait<T_arg1>::pass,typename type_trait<T_arg2>::pass,typename type_trait<T_arg3>::pass,typename type_trait<T_arg4>::pass,typename type_trait<T_arg5>::pass,typename type_trait<T_arg6>::pass,typename type_trait<T_arg7>::pass>(_A_1,_A_2,_A_3,_A_4,_A_5,_A_6,_A_7)); }

  #ifndef SIGC_TEMPLATE_SPECIALIZATION_OPERATOR_OVERLOAD
  template <class T_arg1,class T_arg2,class T_arg3,class T_arg4,class T_arg5,class T_arg6,class T_arg7>
  typename deduce_result_type<T_arg1,T_arg2,T_arg3,T_arg4,T_arg5,T_arg6,T_arg7>::type
  sun_forte_workaround (T_arg1 _A_1,T_arg2 _A_2,T_arg3 _A_3,T_arg4 _A_4,T_arg5 _A_5,T_arg6 _A_6,T_arg7 _A_7) const
    { return this->func_.SIGC_WORKAROUND_OPERATOR_PARENTHESES<
          typename value1_type::template deduce_result_type<T_arg1,T_arg2,T_arg3,T_arg4,T_arg5,T_arg6,T_arg7>::type>(
        this->value1_.SIGC_WORKAROUND_OPERATOR_PARENTHESES<
          typename type_trait<T_arg1>::pass,typename type_trait<T_arg2>::pass,typename type_trait<T_arg3>::pass,typename type_trait<T_arg4>::pass,typename type_trait<T_arg5>::pass,typename type_trait<T_arg6>::pass,typename type_trait<T_arg7>::pass>(_A_1,_A_2,_A_3,_A_4,_A_5,_A_6,_A_7)); }
  #endif //SIGC_TEMPLATE_SPECIALIZATION_OPERATOR_OVERLOAD

  lambda_group1(typename type_trait<T_functor>::take _A_func, typename type_trait<T_type1>::take _A_1)
    : value1_(_A_1), func_(_A_func) {}

  value1_type value1_;
  mutable functor_type func_;
};

template <class T_functor, class T_type1>
typename lambda_group1<T_functor, T_type1>::result_type
lambda_group1<T_functor, T_type1>::operator ()() const
  { return func_(value1_()); }


//template specialization of visit_each<>(action, functor):
template <class T_action, class T_functor, class T_type1>
void visit_each(const T_action& _A_action,
                const lambda_group1<T_functor, T_type1>& _A_target)
{
  visit_each(_A_action, _A_target.value1_);
  visit_each(_A_action, _A_target.func_);
}


template <class T_functor, class T_type1,class T_type2>
struct lambda_group2 : public lambda_base
{
  typedef typename functor_trait<T_functor>::result_type result_type;
  typedef typename lambda<T_type1>::lambda_type   value1_type;
  typedef typename lambda<T_type2>::lambda_type   value2_type;
  typedef typename adaptor_trait<T_functor>::adaptor_type functor_type;

  template <class T_arg1=void,class T_arg2=void,class T_arg3=void,class T_arg4=void,class T_arg5=void,class T_arg6=void,class T_arg7=void>
  struct deduce_result_type
    { typedef typename functor_type::template deduce_result_type<
          typename value1_type::template deduce_result_type<
            typename type_trait<T_arg1>::pass,typename type_trait<T_arg2>::pass,typename type_trait<T_arg3>::pass,typename type_trait<T_arg4>::pass,typename type_trait<T_arg5>::pass,typename type_trait<T_arg6>::pass,typename type_trait<T_arg7>::pass>::type,
          typename value2_type::template deduce_result_type<
            typename type_trait<T_arg1>::pass,typename type_trait<T_arg2>::pass,typename type_trait<T_arg3>::pass,typename type_trait<T_arg4>::pass,typename type_trait<T_arg5>::pass,typename type_trait<T_arg6>::pass,typename type_trait<T_arg7>::pass>::type
        >::type type; };

  result_type
  operator ()() const;

  template <class T_arg1>
  typename deduce_result_type<T_arg1>::type
  operator() (T_arg1 _A_1) const
    { return this->func_.SIGC_WORKAROUND_OPERATOR_PARENTHESES<
          typename value1_type::template deduce_result_type<T_arg1>::type,
          typename value2_type::template deduce_result_type<T_arg1>::type>(
        this->value1_.SIGC_WORKAROUND_OPERATOR_PARENTHESES<
          typename type_trait<T_arg1>::pass>(_A_1),
        this->value2_.SIGC_WORKAROUND_OPERATOR_PARENTHESES<
          typename type_trait<T_arg1>::pass>(_A_1)); }

  #ifndef SIGC_TEMPLATE_SPECIALIZATION_OPERATOR_OVERLOAD
  template <class T_arg1>
  typename deduce_result_type<T_arg1>::type
  sun_forte_workaround (T_arg1 _A_1) const
    { return this->func_.SIGC_WORKAROUND_OPERATOR_PARENTHESES<
          typename value1_type::template deduce_result_type<T_arg1>::type,
          typename value2_type::template deduce_result_type<T_arg1>::type>(
        this->value1_.SIGC_WORKAROUND_OPERATOR_PARENTHESES<
          typename type_trait<T_arg1>::pass>(_A_1),
        this->value2_.SIGC_WORKAROUND_OPERATOR_PARENTHESES<
          typename type_trait<T_arg1>::pass>(_A_1)); }
  #endif //SIGC_TEMPLATE_SPECIALIZATION_OPERATOR_OVERLOAD

  template <class T_arg1,class T_arg2>
  typename deduce_result_type<T_arg1,T_arg2>::type
  operator() (T_arg1 _A_1,T_arg2 _A_2) const
    { return this->func_.SIGC_WORKAROUND_OPERATOR_PARENTHESES<
          typename value1_type::template deduce_result_type<T_arg1,T_arg2>::type,
          typename value2_type::template deduce_result_type<T_arg1,T_arg2>::type>(
        this->value1_.SIGC_WORKAROUND_OPERATOR_PARENTHESES<
          typename type_trait<T_arg1>::pass,typename type_trait<T_arg2>::pass>(_A_1,_A_2),
        this->value2_.SIGC_WORKAROUND_OPERATOR_PARENTHESES<
          typename type_trait<T_arg1>::pass,typename type_trait<T_arg2>::pass>(_A_1,_A_2)); }

  #ifndef SIGC_TEMPLATE_SPECIALIZATION_OPERATOR_OVERLOAD
  template <class T_arg1,class T_arg2>
  typename deduce_result_type<T_arg1,T_arg2>::type
  sun_forte_workaround (T_arg1 _A_1,T_arg2 _A_2) const
    { return this->func_.SIGC_WORKAROUND_OPERATOR_PARENTHESES<
          typename value1_type::template deduce_result_type<T_arg1,T_arg2>::type,
          typename value2_type::template deduce_result_type<T_arg1,T_arg2>::type>(
        this->value1_.SIGC_WORKAROUND_OPERATOR_PARENTHESES<
          typename type_trait<T_arg1>::pass,typename type_trait<T_arg2>::pass>(_A_1,_A_2),
        this->value2_.SIGC_WORKAROUND_OPERATOR_PARENTHESES<
          typename type_trait<T_arg1>::pass,typename type_trait<T_arg2>::pass>(_A_1,_A_2)); }
  #endif //SIGC_TEMPLATE_SPECIALIZATION_OPERATOR_OVERLOAD

  template <class T_arg1,class T_arg2,class T_arg3>
  typename deduce_result_type<T_arg1,T_arg2,T_arg3>::type
  operator() (T_arg1 _A_1,T_arg2 _A_2,T_arg3 _A_3) const
    { return this->func_.SIGC_WORKAROUND_OPERATOR_PARENTHESES<
          typename value1_type::template deduce_result_type<T_arg1,T_arg2,T_arg3>::type,
          typename value2_type::template deduce_result_type<T_arg1,T_arg2,T_arg3>::type>(
        this->value1_.SIGC_WORKAROUND_OPERATOR_PARENTHESES<
          typename type_trait<T_arg1>::pass,typename type_trait<T_arg2>::pass,typename type_trait<T_arg3>::pass>(_A_1,_A_2,_A_3),
        this->value2_.SIGC_WORKAROUND_OPERATOR_PARENTHESES<
          typename type_trait<T_arg1>::pass,typename type_trait<T_arg2>::pass,typename type_trait<T_arg3>::pass>(_A_1,_A_2,_A_3)); }

  #ifndef SIGC_TEMPLATE_SPECIALIZATION_OPERATOR_OVERLOAD
  template <class T_arg1,class T_arg2,class T_arg3>
  typename deduce_result_type<T_arg1,T_arg2,T_arg3>::type
  sun_forte_workaround (T_arg1 _A_1,T_arg2 _A_2,T_arg3 _A_3) const
    { return this->func_.SIGC_WORKAROUND_OPERATOR_PARENTHESES<
          typename value1_type::template deduce_result_type<T_arg1,T_arg2,T_arg3>::type,
          typename value2_type::template deduce_result_type<T_arg1,T_arg2,T_arg3>::type>(
        this->value1_.SIGC_WORKAROUND_OPERATOR_PARENTHESES<
          typename type_trait<T_arg1>::pass,typename type_trait<T_arg2>::pass,typename type_trait<T_arg3>::pass>(_A_1,_A_2,_A_3),
        this->value2_.SIGC_WORKAROUND_OPERATOR_PARENTHESES<
          typename type_trait<T_arg1>::pass,typename type_trait<T_arg2>::pass,typename type_trait<T_arg3>::pass>(_A_1,_A_2,_A_3)); }
  #endif //SIGC_TEMPLATE_SPECIALIZATION_OPERATOR_OVERLOAD

  template <class T_arg1,class T_arg2,class T_arg3,class T_arg4>
  typename deduce_result_type<T_arg1,T_arg2,T_arg3,T_arg4>::type
  operator() (T_arg1 _A_1,T_arg2 _A_2,T_arg3 _A_3,T_arg4 _A_4) const
    { return this->func_.SIGC_WORKAROUND_OPERATOR_PARENTHESES<
          typename value1_type::template deduce_result_type<T_arg1,T_arg2,T_arg3,T_arg4>::type,
          typename value2_type::template deduce_result_type<T_arg1,T_arg2,T_arg3,T_arg4>::type>(
        this->value1_.SIGC_WORKAROUND_OPERATOR_PARENTHESES<
          typename type_trait<T_arg1>::pass,typename type_trait<T_arg2>::pass,typename type_trait<T_arg3>::pass,typename type_trait<T_arg4>::pass>(_A_1,_A_2,_A_3,_A_4),
        this->value2_.SIGC_WORKAROUND_OPERATOR_PARENTHESES<
          typename type_trait<T_arg1>::pass,typename type_trait<T_arg2>::pass,typename type_trait<T_arg3>::pass,typename type_trait<T_arg4>::pass>(_A_1,_A_2,_A_3,_A_4)); }

  #ifndef SIGC_TEMPLATE_SPECIALIZATION_OPERATOR_OVERLOAD
  template <class T_arg1,class T_arg2,class T_arg3,class T_arg4>
  typename deduce_result_type<T_arg1,T_arg2,T_arg3,T_arg4>::type
  sun_forte_workaround (T_arg1 _A_1,T_arg2 _A_2,T_arg3 _A_3,T_arg4 _A_4) const
    { return this->func_.SIGC_WORKAROUND_OPERATOR_PARENTHESES<
          typename value1_type::template deduce_result_type<T_arg1,T_arg2,T_arg3,T_arg4>::type,
          typename value2_type::template deduce_result_type<T_arg1,T_arg2,T_arg3,T_arg4>::type>(
        this->value1_.SIGC_WORKAROUND_OPERATOR_PARENTHESES<
          typename type_trait<T_arg1>::pass,typename type_trait<T_arg2>::pass,typename type_trait<T_arg3>::pass,typename type_trait<T_arg4>::pass>(_A_1,_A_2,_A_3,_A_4),
        this->value2_.SIGC_WORKAROUND_OPERATOR_PARENTHESES<
          typename type_trait<T_arg1>::pass,typename type_trait<T_arg2>::pass,typename type_trait<T_arg3>::pass,typename type_trait<T_arg4>::pass>(_A_1,_A_2,_A_3,_A_4)); }
  #endif //SIGC_TEMPLATE_SPECIALIZATION_OPERATOR_OVERLOAD

  template <class T_arg1,class T_arg2,class T_arg3,class T_arg4,class T_arg5>
  typename deduce_result_type<T_arg1,T_arg2,T_arg3,T_arg4,T_arg5>::type
  operator() (T_arg1 _A_1,T_arg2 _A_2,T_arg3 _A_3,T_arg4 _A_4,T_arg5 _A_5) const
    { return this->func_.SIGC_WORKAROUND_OPERATOR_PARENTHESES<
          typename value1_type::template deduce_result_type<T_arg1,T_arg2,T_arg3,T_arg4,T_arg5>::type,
          typename value2_type::template deduce_result_type<T_arg1,T_arg2,T_arg3,T_arg4,T_arg5>::type>(
        this->value1_.SIGC_WORKAROUND_OPERATOR_PARENTHESES<
          typename type_trait<T_arg1>::pass,typename type_trait<T_arg2>::pass,typename type_trait<T_arg3>::pass,typename type_trait<T_arg4>::pass,typename type_trait<T_arg5>::pass>(_A_1,_A_2,_A_3,_A_4,_A_5),
        this->value2_.SIGC_WORKAROUND_OPERATOR_PARENTHESES<
          typename type_trait<T_arg1>::pass,typename type_trait<T_arg2>::pass,typename type_trait<T_arg3>::pass,typename type_trait<T_arg4>::pass,typename type_trait<T_arg5>::pass>(_A_1,_A_2,_A_3,_A_4,_A_5)); }

  #ifndef SIGC_TEMPLATE_SPECIALIZATION_OPERATOR_OVERLOAD
  template <class T_arg1,class T_arg2,class T_arg3,class T_arg4,class T_arg5>
  typename deduce_result_type<T_arg1,T_arg2,T_arg3,T_arg4,T_arg5>::type
  sun_forte_workaround (T_arg1 _A_1,T_arg2 _A_2,T_arg3 _A_3,T_arg4 _A_4,T_arg5 _A_5) const
    { return this->func_.SIGC_WORKAROUND_OPERATOR_PARENTHESES<
          typename value1_type::template deduce_result_type<T_arg1,T_arg2,T_arg3,T_arg4,T_arg5>::type,
          typename value2_type::template deduce_result_type<T_arg1,T_arg2,T_arg3,T_arg4,T_arg5>::type>(
        this->value1_.SIGC_WORKAROUND_OPERATOR_PARENTHESES<
          typename type_trait<T_arg1>::pass,typename type_trait<T_arg2>::pass,typename type_trait<T_arg3>::pass,typename type_trait<T_arg4>::pass,typename type_trait<T_arg5>::pass>(_A_1,_A_2,_A_3,_A_4,_A_5),
        this->value2_.SIGC_WORKAROUND_OPERATOR_PARENTHESES<
          typename type_trait<T_arg1>::pass,typename type_trait<T_arg2>::pass,typename type_trait<T_arg3>::pass,typename type_trait<T_arg4>::pass,typename type_trait<T_arg5>::pass>(_A_1,_A_2,_A_3,_A_4,_A_5)); }
  #endif //SIGC_TEMPLATE_SPECIALIZATION_OPERATOR_OVERLOAD

  template <class T_arg1,class T_arg2,class T_arg3,class T_arg4,class T_arg5,class T_arg6>
  typename deduce_result_type<T_arg1,T_arg2,T_arg3,T_arg4,T_arg5,T_arg6>::type
  operator() (T_arg1 _A_1,T_arg2 _A_2,T_arg3 _A_3,T_arg4 _A_4,T_arg5 _A_5,T_arg6 _A_6) const
    { return this->func_.SIGC_WORKAROUND_OPERATOR_PARENTHESES<
          typename value1_type::template deduce_result_type<T_arg1,T_arg2,T_arg3,T_arg4,T_arg5,T_arg6>::type,
          typename value2_type::template deduce_result_type<T_arg1,T_arg2,T_arg3,T_arg4,T_arg5,T_arg6>::type>(
        this->value1_.SIGC_WORKAROUND_OPERATOR_PARENTHESES<
          typename type_trait<T_arg1>::pass,typename type_trait<T_arg2>::pass,typename type_trait<T_arg3>::pass,typename type_trait<T_arg4>::pass,typename type_trait<T_arg5>::pass,typename type_trait<T_arg6>::pass>(_A_1,_A_2,_A_3,_A_4,_A_5,_A_6),
        this->value2_.SIGC_WORKAROUND_OPERATOR_PARENTHESES<
          typename type_trait<T_arg1>::pass,typename type_trait<T_arg2>::pass,typename type_trait<T_arg3>::pass,typename type_trait<T_arg4>::pass,typename type_trait<T_arg5>::pass,typename type_trait<T_arg6>::pass>(_A_1,_A_2,_A_3,_A_4,_A_5,_A_6)); }

  #ifndef SIGC_TEMPLATE_SPECIALIZATION_OPERATOR_OVERLOAD
  template <class T_arg1,class T_arg2,class T_arg3,class T_arg4,class T_arg5,class T_arg6>
  typename deduce_result_type<T_arg1,T_arg2,T_arg3,T_arg4,T_arg5,T_arg6>::type
  sun_forte_workaround (T_arg1 _A_1,T_arg2 _A_2,T_arg3 _A_3,T_arg4 _A_4,T_arg5 _A_5,T_arg6 _A_6) const
    { return this->func_.SIGC_WORKAROUND_OPERATOR_PARENTHESES<
          typename value1_type::template deduce_result_type<T_arg1,T_arg2,T_arg3,T_arg4,T_arg5,T_arg6>::type,
          typename value2_type::template deduce_result_type<T_arg1,T_arg2,T_arg3,T_arg4,T_arg5,T_arg6>::type>(
        this->value1_.SIGC_WORKAROUND_OPERATOR_PARENTHESES<
          typename type_trait<T_arg1>::pass,typename type_trait<T_arg2>::pass,typename type_trait<T_arg3>::pass,typename type_trait<T_arg4>::pass,typename type_trait<T_arg5>::pass,typename type_trait<T_arg6>::pass>(_A_1,_A_2,_A_3,_A_4,_A_5,_A_6),
        this->value2_.SIGC_WORKAROUND_OPERATOR_PARENTHESES<
          typename type_trait<T_arg1>::pass,typename type_trait<T_arg2>::pass,typename type_trait<T_arg3>::pass,typename type_trait<T_arg4>::pass,typename type_trait<T_arg5>::pass,typename type_trait<T_arg6>::pass>(_A_1,_A_2,_A_3,_A_4,_A_5,_A_6)); }
  #endif //SIGC_TEMPLATE_SPECIALIZATION_OPERATOR_OVERLOAD

  template <class T_arg1,class T_arg2,class T_arg3,class T_arg4,class T_arg5,class T_arg6,class T_arg7>
  typename deduce_result_type<T_arg1,T_arg2,T_arg3,T_arg4,T_arg5,T_arg6,T_arg7>::type
  operator() (T_arg1 _A_1,T_arg2 _A_2,T_arg3 _A_3,T_arg4 _A_4,T_arg5 _A_5,T_arg6 _A_6,T_arg7 _A_7) const
    { return this->func_.SIGC_WORKAROUND_OPERATOR_PARENTHESES<
          typename value1_type::template deduce_result_type<T_arg1,T_arg2,T_arg3,T_arg4,T_arg5,T_arg6,T_arg7>::type,
          typename value2_type::template deduce_result_type<T_arg1,T_arg2,T_arg3,T_arg4,T_arg5,T_arg6,T_arg7>::type>(
        this->value1_.SIGC_WORKAROUND_OPERATOR_PARENTHESES<
          typename type_trait<T_arg1>::pass,typename type_trait<T_arg2>::pass,typename type_trait<T_arg3>::pass,typename type_trait<T_arg4>::pass,typename type_trait<T_arg5>::pass,typename type_trait<T_arg6>::pass,typename type_trait<T_arg7>::pass>(_A_1,_A_2,_A_3,_A_4,_A_5,_A_6,_A_7),
        this->value2_.SIGC_WORKAROUND_OPERATOR_PARENTHESES<
          typename type_trait<T_arg1>::pass,typename type_trait<T_arg2>::pass,typename type_trait<T_arg3>::pass,typename type_trait<T_arg4>::pass,typename type_trait<T_arg5>::pass,typename type_trait<T_arg6>::pass,typename type_trait<T_arg7>::pass>(_A_1,_A_2,_A_3,_A_4,_A_5,_A_6,_A_7)); }

  #ifndef SIGC_TEMPLATE_SPECIALIZATION_OPERATOR_OVERLOAD
  template <class T_arg1,class T_arg2,class T_arg3,class T_arg4,class T_arg5,class T_arg6,class T_arg7>
  typename deduce_result_type<T_arg1,T_arg2,T_arg3,T_arg4,T_arg5,T_arg6,T_arg7>::type
  sun_forte_workaround (T_arg1 _A_1,T_arg2 _A_2,T_arg3 _A_3,T_arg4 _A_4,T_arg5 _A_5,T_arg6 _A_6,T_arg7 _A_7) const
    { return this->func_.SIGC_WORKAROUND_OPERATOR_PARENTHESES<
          typename value1_type::template deduce_result_type<T_arg1,T_arg2,T_arg3,T_arg4,T_arg5,T_arg6,T_arg7>::type,
          typename value2_type::template deduce_result_type<T_arg1,T_arg2,T_arg3,T_arg4,T_arg5,T_arg6,T_arg7>::type>(
        this->value1_.SIGC_WORKAROUND_OPERATOR_PARENTHESES<
          typename type_trait<T_arg1>::pass,typename type_trait<T_arg2>::pass,typename type_trait<T_arg3>::pass,typename type_trait<T_arg4>::pass,typename type_trait<T_arg5>::pass,typename type_trait<T_arg6>::pass,typename type_trait<T_arg7>::pass>(_A_1,_A_2,_A_3,_A_4,_A_5,_A_6,_A_7),
        this->value2_.SIGC_WORKAROUND_OPERATOR_PARENTHESES<
          typename type_trait<T_arg1>::pass,typename type_trait<T_arg2>::pass,typename type_trait<T_arg3>::pass,typename type_trait<T_arg4>::pass,typename type_trait<T_arg5>::pass,typename type_trait<T_arg6>::pass,typename type_trait<T_arg7>::pass>(_A_1,_A_2,_A_3,_A_4,_A_5,_A_6,_A_7)); }
  #endif //SIGC_TEMPLATE_SPECIALIZATION_OPERATOR_OVERLOAD

  lambda_group2(typename type_trait<T_functor>::take _A_func, typename type_trait<T_type1>::take _A_1,typename type_trait<T_type2>::take _A_2)
    : value1_(_A_1),value2_(_A_2), func_(_A_func) {}

  value1_type value1_;
  value2_type value2_;
  mutable functor_type func_;
};

template <class T_functor, class T_type1,class T_type2>
typename lambda_group2<T_functor, T_type1,T_type2>::result_type
lambda_group2<T_functor, T_type1,T_type2>::operator ()() const
  { return func_(value1_(),value2_()); }


//template specialization of visit_each<>(action, functor):
template <class T_action, class T_functor, class T_type1,class T_type2>
void visit_each(const T_action& _A_action,
                const lambda_group2<T_functor, T_type1,T_type2>& _A_target)
{
  visit_each(_A_action, _A_target.value1_);
  visit_each(_A_action, _A_target.value2_);
  visit_each(_A_action, _A_target.func_);
}


template <class T_functor, class T_type1,class T_type2,class T_type3>
struct lambda_group3 : public lambda_base
{
  typedef typename functor_trait<T_functor>::result_type result_type;
  typedef typename lambda<T_type1>::lambda_type   value1_type;
  typedef typename lambda<T_type2>::lambda_type   value2_type;
  typedef typename lambda<T_type3>::lambda_type   value3_type;
  typedef typename adaptor_trait<T_functor>::adaptor_type functor_type;

  template <class T_arg1=void,class T_arg2=void,class T_arg3=void,class T_arg4=void,class T_arg5=void,class T_arg6=void,class T_arg7=void>
  struct deduce_result_type
    { typedef typename functor_type::template deduce_result_type<
          typename value1_type::template deduce_result_type<
            typename type_trait<T_arg1>::pass,typename type_trait<T_arg2>::pass,typename type_trait<T_arg3>::pass,typename type_trait<T_arg4>::pass,typename type_trait<T_arg5>::pass,typename type_trait<T_arg6>::pass,typename type_trait<T_arg7>::pass>::type,
          typename value2_type::template deduce_result_type<
            typename type_trait<T_arg1>::pass,typename type_trait<T_arg2>::pass,typename type_trait<T_arg3>::pass,typename type_trait<T_arg4>::pass,typename type_trait<T_arg5>::pass,typename type_trait<T_arg6>::pass,typename type_trait<T_arg7>::pass>::type,
          typename value3_type::template deduce_result_type<
            typename type_trait<T_arg1>::pass,typename type_trait<T_arg2>::pass,typename type_trait<T_arg3>::pass,typename type_trait<T_arg4>::pass,typename type_trait<T_arg5>::pass,typename type_trait<T_arg6>::pass,typename type_trait<T_arg7>::pass>::type
        >::type type; };

  result_type
  operator ()() const;

  template <class T_arg1>
  typename deduce_result_type<T_arg1>::type
  operator() (T_arg1 _A_1) const
    { return this->func_.SIGC_WORKAROUND_OPERATOR_PARENTHESES<
          typename value1_type::template deduce_result_type<T_arg1>::type,
          typename value2_type::template deduce_result_type<T_arg1>::type,
          typename value3_type::template deduce_result_type<T_arg1>::type>(
        this->value1_.SIGC_WORKAROUND_OPERATOR_PARENTHESES<
          typename type_trait<T_arg1>::pass>(_A_1),
        this->value2_.SIGC_WORKAROUND_OPERATOR_PARENTHESES<
          typename type_trait<T_arg1>::pass>(_A_1),
        this->value3_.SIGC_WORKAROUND_OPERATOR_PARENTHESES<
          typename type_trait<T_arg1>::pass>(_A_1)); }

  #ifndef SIGC_TEMPLATE_SPECIALIZATION_OPERATOR_OVERLOAD
  template <class T_arg1>
  typename deduce_result_type<T_arg1>::type
  sun_forte_workaround (T_arg1 _A_1) const
    { return this->func_.SIGC_WORKAROUND_OPERATOR_PARENTHESES<
          typename value1_type::template deduce_result_type<T_arg1>::type,
          typename value2_type::template deduce_result_type<T_arg1>::type,
          typename value3_type::template deduce_result_type<T_arg1>::type>(
        this->value1_.SIGC_WORKAROUND_OPERATOR_PARENTHESES<
          typename type_trait<T_arg1>::pass>(_A_1),
        this->value2_.SIGC_WORKAROUND_OPERATOR_PARENTHESES<
          typename type_trait<T_arg1>::pass>(_A_1),
        this->value3_.SIGC_WORKAROUND_OPERATOR_PARENTHESES<
          typename type_trait<T_arg1>::pass>(_A_1)); }
  #endif //SIGC_TEMPLATE_SPECIALIZATION_OPERATOR_OVERLOAD

  template <class T_arg1,class T_arg2>
  typename deduce_result_type<T_arg1,T_arg2>::type
  operator() (T_arg1 _A_1,T_arg2 _A_2) const
    { return this->func_.SIGC_WORKAROUND_OPERATOR_PARENTHESES<
          typename value1_type::template deduce_result_type<T_arg1,T_arg2>::type,
          typename value2_type::template deduce_result_type<T_arg1,T_arg2>::type,
          typename value3_type::template deduce_result_type<T_arg1,T_arg2>::type>(
        this->value1_.SIGC_WORKAROUND_OPERATOR_PARENTHESES<
          typename type_trait<T_arg1>::pass,typename type_trait<T_arg2>::pass>(_A_1,_A_2),
        this->value2_.SIGC_WORKAROUND_OPERATOR_PARENTHESES<
          typename type_trait<T_arg1>::pass,typename type_trait<T_arg2>::pass>(_A_1,_A_2),
        this->value3_.SIGC_WORKAROUND_OPERATOR_PARENTHESES<
          typename type_trait<T_arg1>::pass,typename type_trait<T_arg2>::pass>(_A_1,_A_2)); }

  #ifndef SIGC_TEMPLATE_SPECIALIZATION_OPERATOR_OVERLOAD
  template <class T_arg1,class T_arg2>
  typename deduce_result_type<T_arg1,T_arg2>::type
  sun_forte_workaround (T_arg1 _A_1,T_arg2 _A_2) const
    { return this->func_.SIGC_WORKAROUND_OPERATOR_PARENTHESES<
          typename value1_type::template deduce_result_type<T_arg1,T_arg2>::type,
          typename value2_type::template deduce_result_type<T_arg1,T_arg2>::type,
          typename value3_type::template deduce_result_type<T_arg1,T_arg2>::type>(
        this->value1_.SIGC_WORKAROUND_OPERATOR_PARENTHESES<
          typename type_trait<T_arg1>::pass,typename type_trait<T_arg2>::pass>(_A_1,_A_2),
        this->value2_.SIGC_WORKAROUND_OPERATOR_PARENTHESES<
          typename type_trait<T_arg1>::pass,typename type_trait<T_arg2>::pass>(_A_1,_A_2),
        this->value3_.SIGC_WORKAROUND_OPERATOR_PARENTHESES<
          typename type_trait<T_arg1>::pass,typename type_trait<T_arg2>::pass>(_A_1,_A_2)); }
  #endif //SIGC_TEMPLATE_SPECIALIZATION_OPERATOR_OVERLOAD

  template <class T_arg1,class T_arg2,class T_arg3>
  typename deduce_result_type<T_arg1,T_arg2,T_arg3>::type
  operator() (T_arg1 _A_1,T_arg2 _A_2,T_arg3 _A_3) const
    { return this->func_.SIGC_WORKAROUND_OPERATOR_PARENTHESES<
          typename value1_type::template deduce_result_type<T_arg1,T_arg2,T_arg3>::type,
          typename value2_type::template deduce_result_type<T_arg1,T_arg2,T_arg3>::type,
          typename value3_type::template deduce_result_type<T_arg1,T_arg2,T_arg3>::type>(
        this->value1_.SIGC_WORKAROUND_OPERATOR_PARENTHESES<
          typename type_trait<T_arg1>::pass,typename type_trait<T_arg2>::pass,typename type_trait<T_arg3>::pass>(_A_1,_A_2,_A_3),
        this->value2_.SIGC_WORKAROUND_OPERATOR_PARENTHESES<
          typename type_trait<T_arg1>::pass,typename type_trait<T_arg2>::pass,typename type_trait<T_arg3>::pass>(_A_1,_A_2,_A_3),
        this->value3_.SIGC_WORKAROUND_OPERATOR_PARENTHESES<
          typename type_trait<T_arg1>::pass,typename type_trait<T_arg2>::pass,typename type_trait<T_arg3>::pass>(_A_1,_A_2,_A_3)); }

  #ifndef SIGC_TEMPLATE_SPECIALIZATION_OPERATOR_OVERLOAD
  template <class T_arg1,class T_arg2,class T_arg3>
  typename deduce_result_type<T_arg1,T_arg2,T_arg3>::type
  sun_forte_workaround (T_arg1 _A_1,T_arg2 _A_2,T_arg3 _A_3) const
    { return this->func_.SIGC_WORKAROUND_OPERATOR_PARENTHESES<
          typename value1_type::template deduce_result_type<T_arg1,T_arg2,T_arg3>::type,
          typename value2_type::template deduce_result_type<T_arg1,T_arg2,T_arg3>::type,
          typename value3_type::template deduce_result_type<T_arg1,T_arg2,T_arg3>::type>(
        this->value1_.SIGC_WORKAROUND_OPERATOR_PARENTHESES<
          typename type_trait<T_arg1>::pass,typename type_trait<T_arg2>::pass,typename type_trait<T_arg3>::pass>(_A_1,_A_2,_A_3),
        this->value2_.SIGC_WORKAROUND_OPERATOR_PARENTHESES<
          typename type_trait<T_arg1>::pass,typename type_trait<T_arg2>::pass,typename type_trait<T_arg3>::pass>(_A_1,_A_2,_A_3),
        this->value3_.SIGC_WORKAROUND_OPERATOR_PARENTHESES<
          typename type_trait<T_arg1>::pass,typename type_trait<T_arg2>::pass,typename type_trait<T_arg3>::pass>(_A_1,_A_2,_A_3)); }
  #endif //SIGC_TEMPLATE_SPECIALIZATION_OPERATOR_OVERLOAD

  template <class T_arg1,class T_arg2,class T_arg3,class T_arg4>
  typename deduce_result_type<T_arg1,T_arg2,T_arg3,T_arg4>::type
  operator() (T_arg1 _A_1,T_arg2 _A_2,T_arg3 _A_3,T_arg4 _A_4) const
    { return this->func_.SIGC_WORKAROUND_OPERATOR_PARENTHESES<
          typename value1_type::template deduce_result_type<T_arg1,T_arg2,T_arg3,T_arg4>::type,
          typename value2_type::template deduce_result_type<T_arg1,T_arg2,T_arg3,T_arg4>::type,
          typename value3_type::template deduce_result_type<T_arg1,T_arg2,T_arg3,T_arg4>::type>(
        this->value1_.SIGC_WORKAROUND_OPERATOR_PARENTHESES<
          typename type_trait<T_arg1>::pass,typename type_trait<T_arg2>::pass,typename type_trait<T_arg3>::pass,typename type_trait<T_arg4>::pass>(_A_1,_A_2,_A_3,_A_4),
        this->value2_.SIGC_WORKAROUND_OPERATOR_PARENTHESES<
          typename type_trait<T_arg1>::pass,typename type_trait<T_arg2>::pass,typename type_trait<T_arg3>::pass,typename type_trait<T_arg4>::pass>(_A_1,_A_2,_A_3,_A_4),
        this->value3_.SIGC_WORKAROUND_OPERATOR_PARENTHESES<
          typename type_trait<T_arg1>::pass,typename type_trait<T_arg2>::pass,typename type_trait<T_arg3>::pass,typename type_trait<T_arg4>::pass>(_A_1,_A_2,_A_3,_A_4)); }

  #ifndef SIGC_TEMPLATE_SPECIALIZATION_OPERATOR_OVERLOAD
  template <class T_arg1,class T_arg2,class T_arg3,class T_arg4>
  typename deduce_result_type<T_arg1,T_arg2,T_arg3,T_arg4>::type
  sun_forte_workaround (T_arg1 _A_1,T_arg2 _A_2,T_arg3 _A_3,T_arg4 _A_4) const
    { return this->func_.SIGC_WORKAROUND_OPERATOR_PARENTHESES<
          typename value1_type::template deduce_result_type<T_arg1,T_arg2,T_arg3,T_arg4>::type,
          typename value2_type::template deduce_result_type<T_arg1,T_arg2,T_arg3,T_arg4>::type,
          typename value3_type::template deduce_result_type<T_arg1,T_arg2,T_arg3,T_arg4>::type>(
        this->value1_.SIGC_WORKAROUND_OPERATOR_PARENTHESES<
          typename type_trait<T_arg1>::pass,typename type_trait<T_arg2>::pass,typename type_trait<T_arg3>::pass,typename type_trait<T_arg4>::pass>(_A_1,_A_2,_A_3,_A_4),
        this->value2_.SIGC_WORKAROUND_OPERATOR_PARENTHESES<
          typename type_trait<T_arg1>::pass,typename type_trait<T_arg2>::pass,typename type_trait<T_arg3>::pass,typename type_trait<T_arg4>::pass>(_A_1,_A_2,_A_3,_A_4),
        this->value3_.SIGC_WORKAROUND_OPERATOR_PARENTHESES<
          typename type_trait<T_arg1>::pass,typename type_trait<T_arg2>::pass,typename type_trait<T_arg3>::pass,typename type_trait<T_arg4>::pass>(_A_1,_A_2,_A_3,_A_4)); }
  #endif //SIGC_TEMPLATE_SPECIALIZATION_OPERATOR_OVERLOAD

  template <class T_arg1,class T_arg2,class T_arg3,class T_arg4,class T_arg5>
  typename deduce_result_type<T_arg1,T_arg2,T_arg3,T_arg4,T_arg5>::type
  operator() (T_arg1 _A_1,T_arg2 _A_2,T_arg3 _A_3,T_arg4 _A_4,T_arg5 _A_5) const
    { return this->func_.SIGC_WORKAROUND_OPERATOR_PARENTHESES<
          typename value1_type::template deduce_result_type<T_arg1,T_arg2,T_arg3,T_arg4,T_arg5>::type,
          typename value2_type::template deduce_result_type<T_arg1,T_arg2,T_arg3,T_arg4,T_arg5>::type,
          typename value3_type::template deduce_result_type<T_arg1,T_arg2,T_arg3,T_arg4,T_arg5>::type>(
        this->value1_.SIGC_WORKAROUND_OPERATOR_PARENTHESES<
          typename type_trait<T_arg1>::pass,typename type_trait<T_arg2>::pass,typename type_trait<T_arg3>::pass,typename type_trait<T_arg4>::pass,typename type_trait<T_arg5>::pass>(_A_1,_A_2,_A_3,_A_4,_A_5),
        this->value2_.SIGC_WORKAROUND_OPERATOR_PARENTHESES<
          typename type_trait<T_arg1>::pass,typename type_trait<T_arg2>::pass,typename type_trait<T_arg3>::pass,typename type_trait<T_arg4>::pass,typename type_trait<T_arg5>::pass>(_A_1,_A_2,_A_3,_A_4,_A_5),
        this->value3_.SIGC_WORKAROUND_OPERATOR_PARENTHESES<
          typename type_trait<T_arg1>::pass,typename type_trait<T_arg2>::pass,typename type_trait<T_arg3>::pass,typename type_trait<T_arg4>::pass,typename type_trait<T_arg5>::pass>(_A_1,_A_2,_A_3,_A_4,_A_5)); }

  #ifndef SIGC_TEMPLATE_SPECIALIZATION_OPERATOR_OVERLOAD
  template <class T_arg1,class T_arg2,class T_arg3,class T_arg4,class T_arg5>
  typename deduce_result_type<T_arg1,T_arg2,T_arg3,T_arg4,T_arg5>::type
  sun_forte_workaround (T_arg1 _A_1,T_arg2 _A_2,T_arg3 _A_3,T_arg4 _A_4,T_arg5 _A_5) const
    { return this->func_.SIGC_WORKAROUND_OPERATOR_PARENTHESES<
          typename value1_type::template deduce_result_type<T_arg1,T_arg2,T_arg3,T_arg4,T_arg5>::type,
          typename value2_type::template deduce_result_type<T_arg1,T_arg2,T_arg3,T_arg4,T_arg5>::type,
          typename value3_type::template deduce_result_type<T_arg1,T_arg2,T_arg3,T_arg4,T_arg5>::type>(
        this->value1_.SIGC_WORKAROUND_OPERATOR_PARENTHESES<
          typename type_trait<T_arg1>::pass,typename type_trait<T_arg2>::pass,typename type_trait<T_arg3>::pass,typename type_trait<T_arg4>::pass,typename type_trait<T_arg5>::pass>(_A_1,_A_2,_A_3,_A_4,_A_5),
        this->value2_.SIGC_WORKAROUND_OPERATOR_PARENTHESES<
          typename type_trait<T_arg1>::pass,typename type_trait<T_arg2>::pass,typename type_trait<T_arg3>::pass,typename type_trait<T_arg4>::pass,typename type_trait<T_arg5>::pass>(_A_1,_A_2,_A_3,_A_4,_A_5),
        this->value3_.SIGC_WORKAROUND_OPERATOR_PARENTHESES<
          typename type_trait<T_arg1>::pass,typename type_trait<T_arg2>::pass,typename type_trait<T_arg3>::pass,typename type_trait<T_arg4>::pass,typename type_trait<T_arg5>::pass>(_A_1,_A_2,_A_3,_A_4,_A_5)); }
  #endif //SIGC_TEMPLATE_SPECIALIZATION_OPERATOR_OVERLOAD

  template <class T_arg1,class T_arg2,class T_arg3,class T_arg4,class T_arg5,class T_arg6>
  typename deduce_result_type<T_arg1,T_arg2,T_arg3,T_arg4,T_arg5,T_arg6>::type
  operator() (T_arg1 _A_1,T_arg2 _A_2,T_arg3 _A_3,T_arg4 _A_4,T_arg5 _A_5,T_arg6 _A_6) const
    { return this->func_.SIGC_WORKAROUND_OPERATOR_PARENTHESES<
          typename value1_type::template deduce_result_type<T_arg1,T_arg2,T_arg3,T_arg4,T_arg5,T_arg6>::type,
          typename value2_type::template deduce_result_type<T_arg1,T_arg2,T_arg3,T_arg4,T_arg5,T_arg6>::type,
          typename value3_type::template deduce_result_type<T_arg1,T_arg2,T_arg3,T_arg4,T_arg5,T_arg6>::type>(
        this->value1_.SIGC_WORKAROUND_OPERATOR_PARENTHESES<
          typename type_trait<T_arg1>::pass,typename type_trait<T_arg2>::pass,typename type_trait<T_arg3>::pass,typename type_trait<T_arg4>::pass,typename type_trait<T_arg5>::pass,typename type_trait<T_arg6>::pass>(_A_1,_A_2,_A_3,_A_4,_A_5,_A_6),
        this->value2_.SIGC_WORKAROUND_OPERATOR_PARENTHESES<
          typename type_trait<T_arg1>::pass,typename type_trait<T_arg2>::pass,typename type_trait<T_arg3>::pass,typename type_trait<T_arg4>::pass,typename type_trait<T_arg5>::pass,typename type_trait<T_arg6>::pass>(_A_1,_A_2,_A_3,_A_4,_A_5,_A_6),
        this->value3_.SIGC_WORKAROUND_OPERATOR_PARENTHESES<
          typename type_trait<T_arg1>::pass,typename type_trait<T_arg2>::pass,typename type_trait<T_arg3>::pass,typename type_trait<T_arg4>::pass,typename type_trait<T_arg5>::pass,typename type_trait<T_arg6>::pass>(_A_1,_A_2,_A_3,_A_4,_A_5,_A_6)); }

  #ifndef SIGC_TEMPLATE_SPECIALIZATION_OPERATOR_OVERLOAD
  template <class T_arg1,class T_arg2,class T_arg3,class T_arg4,class T_arg5,class T_arg6>
  typename deduce_result_type<T_arg1,T_arg2,T_arg3,T_arg4,T_arg5,T_arg6>::type
  sun_forte_workaround (T_arg1 _A_1,T_arg2 _A_2,T_arg3 _A_3,T_arg4 _A_4,T_arg5 _A_5,T_arg6 _A_6) const
    { return this->func_.SIGC_WORKAROUND_OPERATOR_PARENTHESES<
          typename value1_type::template deduce_result_type<T_arg1,T_arg2,T_arg3,T_arg4,T_arg5,T_arg6>::type,
          typename value2_type::template deduce_result_type<T_arg1,T_arg2,T_arg3,T_arg4,T_arg5,T_arg6>::type,
          typename value3_type::template deduce_result_type<T_arg1,T_arg2,T_arg3,T_arg4,T_arg5,T_arg6>::type>(
        this->value1_.SIGC_WORKAROUND_OPERATOR_PARENTHESES<
          typename type_trait<T_arg1>::pass,typename type_trait<T_arg2>::pass,typename type_trait<T_arg3>::pass,typename type_trait<T_arg4>::pass,typename type_trait<T_arg5>::pass,typename type_trait<T_arg6>::pass>(_A_1,_A_2,_A_3,_A_4,_A_5,_A_6),
        this->value2_.SIGC_WORKAROUND_OPERATOR_PARENTHESES<
          typename type_trait<T_arg1>::pass,typename type_trait<T_arg2>::pass,typename type_trait<T_arg3>::pass,typename type_trait<T_arg4>::pass,typename type_trait<T_arg5>::pass,typename type_trait<T_arg6>::pass>(_A_1,_A_2,_A_3,_A_4,_A_5,_A_6),
        this->value3_.SIGC_WORKAROUND_OPERATOR_PARENTHESES<
          typename type_trait<T_arg1>::pass,typename type_trait<T_arg2>::pass,typename type_trait<T_arg3>::pass,typename type_trait<T_arg4>::pass,typename type_trait<T_arg5>::pass,typename type_trait<T_arg6>::pass>(_A_1,_A_2,_A_3,_A_4,_A_5,_A_6)); }
  #endif //SIGC_TEMPLATE_SPECIALIZATION_OPERATOR_OVERLOAD

  template <class T_arg1,class T_arg2,class T_arg3,class T_arg4,class T_arg5,class T_arg6,class T_arg7>
  typename deduce_result_type<T_arg1,T_arg2,T_arg3,T_arg4,T_arg5,T_arg6,T_arg7>::type
  operator() (T_arg1 _A_1,T_arg2 _A_2,T_arg3 _A_3,T_arg4 _A_4,T_arg5 _A_5,T_arg6 _A_6,T_arg7 _A_7) const
    { return this->func_.SIGC_WORKAROUND_OPERATOR_PARENTHESES<
          typename value1_type::template deduce_result_type<T_arg1,T_arg2,T_arg3,T_arg4,T_arg5,T_arg6,T_arg7>::type,
          typename value2_type::template deduce_result_type<T_arg1,T_arg2,T_arg3,T_arg4,T_arg5,T_arg6,T_arg7>::type,
          typename value3_type::template deduce_result_type<T_arg1,T_arg2,T_arg3,T_arg4,T_arg5,T_arg6,T_arg7>::type>(
        this->value1_.SIGC_WORKAROUND_OPERATOR_PARENTHESES<
          typename type_trait<T_arg1>::pass,typename type_trait<T_arg2>::pass,typename type_trait<T_arg3>::pass,typename type_trait<T_arg4>::pass,typename type_trait<T_arg5>::pass,typename type_trait<T_arg6>::pass,typename type_trait<T_arg7>::pass>(_A_1,_A_2,_A_3,_A_4,_A_5,_A_6,_A_7),
        this->value2_.SIGC_WORKAROUND_OPERATOR_PARENTHESES<
          typename type_trait<T_arg1>::pass,typename type_trait<T_arg2>::pass,typename type_trait<T_arg3>::pass,typename type_trait<T_arg4>::pass,typename type_trait<T_arg5>::pass,typename type_trait<T_arg6>::pass,typename type_trait<T_arg7>::pass>(_A_1,_A_2,_A_3,_A_4,_A_5,_A_6,_A_7),
        this->value3_.SIGC_WORKAROUND_OPERATOR_PARENTHESES<
          typename type_trait<T_arg1>::pass,typename type_trait<T_arg2>::pass,typename type_trait<T_arg3>::pass,typename type_trait<T_arg4>::pass,typename type_trait<T_arg5>::pass,typename type_trait<T_arg6>::pass,typename type_trait<T_arg7>::pass>(_A_1,_A_2,_A_3,_A_4,_A_5,_A_6,_A_7)); }

  #ifndef SIGC_TEMPLATE_SPECIALIZATION_OPERATOR_OVERLOAD
  template <class T_arg1,class T_arg2,class T_arg3,class T_arg4,class T_arg5,class T_arg6,class T_arg7>
  typename deduce_result_type<T_arg1,T_arg2,T_arg3,T_arg4,T_arg5,T_arg6,T_arg7>::type
  sun_forte_workaround (T_arg1 _A_1,T_arg2 _A_2,T_arg3 _A_3,T_arg4 _A_4,T_arg5 _A_5,T_arg6 _A_6,T_arg7 _A_7) const
    { return this->func_.SIGC_WORKAROUND_OPERATOR_PARENTHESES<
          typename value1_type::template deduce_result_type<T_arg1,T_arg2,T_arg3,T_arg4,T_arg5,T_arg6,T_arg7>::type,
          typename value2_type::template deduce_result_type<T_arg1,T_arg2,T_arg3,T_arg4,T_arg5,T_arg6,T_arg7>::type,
          typename value3_type::template deduce_result_type<T_arg1,T_arg2,T_arg3,T_arg4,T_arg5,T_arg6,T_arg7>::type>(
        this->value1_.SIGC_WORKAROUND_OPERATOR_PARENTHESES<
          typename type_trait<T_arg1>::pass,typename type_trait<T_arg2>::pass,typename type_trait<T_arg3>::pass,typename type_trait<T_arg4>::pass,typename type_trait<T_arg5>::pass,typename type_trait<T_arg6>::pass,typename type_trait<T_arg7>::pass>(_A_1,_A_2,_A_3,_A_4,_A_5,_A_6,_A_7),
        this->value2_.SIGC_WORKAROUND_OPERATOR_PARENTHESES<
          typename type_trait<T_arg1>::pass,typename type_trait<T_arg2>::pass,typename type_trait<T_arg3>::pass,typename type_trait<T_arg4>::pass,typename type_trait<T_arg5>::pass,typename type_trait<T_arg6>::pass,typename type_trait<T_arg7>::pass>(_A_1,_A_2,_A_3,_A_4,_A_5,_A_6,_A_7),
        this->value3_.SIGC_WORKAROUND_OPERATOR_PARENTHESES<
          typename type_trait<T_arg1>::pass,typename type_trait<T_arg2>::pass,typename type_trait<T_arg3>::pass,typename type_trait<T_arg4>::pass,typename type_trait<T_arg5>::pass,typename type_trait<T_arg6>::pass,typename type_trait<T_arg7>::pass>(_A_1,_A_2,_A_3,_A_4,_A_5,_A_6,_A_7)); }
  #endif //SIGC_TEMPLATE_SPECIALIZATION_OPERATOR_OVERLOAD

  lambda_group3(typename type_trait<T_functor>::take _A_func, typename type_trait<T_type1>::take _A_1,typename type_trait<T_type2>::take _A_2,typename type_trait<T_type3>::take _A_3)
    : value1_(_A_1),value2_(_A_2),value3_(_A_3), func_(_A_func) {}

  value1_type value1_;
  value2_type value2_;
  value3_type value3_;
  mutable functor_type func_;
};

template <class T_functor, class T_type1,class T_type2,class T_type3>
typename lambda_group3<T_functor, T_type1,T_type2,T_type3>::result_type
lambda_group3<T_functor, T_type1,T_type2,T_type3>::operator ()() const
  { return func_(value1_(),value2_(),value3_()); }


//template specialization of visit_each<>(action, functor):
template <class T_action, class T_functor, class T_type1,class T_type2,class T_type3>
void visit_each(const T_action& _A_action,
                const lambda_group3<T_functor, T_type1,T_type2,T_type3>& _A_target)
{
  visit_each(_A_action, _A_target.value1_);
  visit_each(_A_action, _A_target.value2_);
  visit_each(_A_action, _A_target.value3_);
  visit_each(_A_action, _A_target.func_);
}



template <class T_functor, class T_type1>
lambda<lambda_group1<T_functor, typename unwrap_reference<T_type1>::type> >
group(const T_functor& _A_func, T_type1 _A_1)
{
  typedef lambda_group1<T_functor, typename unwrap_reference<T_type1>::type> T_lambda;
  return lambda<T_lambda>(T_lambda(_A_func, _A_1));
}

template <class T_functor, class T_type1,class T_type2>
lambda<lambda_group2<T_functor, typename unwrap_reference<T_type1>::type,typename unwrap_reference<T_type2>::type> >
group(const T_functor& _A_func, T_type1 _A_1,T_type2 _A_2)
{
  typedef lambda_group2<T_functor, typename unwrap_reference<T_type1>::type,typename unwrap_reference<T_type2>::type> T_lambda;
  return lambda<T_lambda>(T_lambda(_A_func, _A_1,_A_2));
}

template <class T_functor, class T_type1,class T_type2,class T_type3>
lambda<lambda_group3<T_functor, typename unwrap_reference<T_type1>::type,typename unwrap_reference<T_type2>::type,typename unwrap_reference<T_type3>::type> >
group(const T_functor& _A_func, T_type1 _A_1,T_type2 _A_2,T_type3 _A_3)
{
  typedef lambda_group3<T_functor, typename unwrap_reference<T_type1>::type,typename unwrap_reference<T_type2>::type,typename unwrap_reference<T_type3>::type> T_lambda;
  return lambda<T_lambda>(T_lambda(_A_func, _A_1,_A_2,_A_3));
}



} /* namespace sigc */
#endif /* _SIGC_ADAPTORS_LAMBDA_MACROS_GROUPHM4_ */
