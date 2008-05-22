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
dnl Macros to make operators
define([LAMBDA_OPERATOR_DO],[dnl
  template <LOOP(class T_arg%1, $1)>
  typename deduce_result_type<LOOP(T_arg%1,$1)>::type
  operator ()(LOOP(T_arg%1 _A_%1, $1)) const
    {
      return lambda_action<T_action>::template do_action<
            typename deduce_result_type<LOOP(_P_(T_arg%1),$1)>::left_type,
            typename deduce_result_type<LOOP(_P_(T_arg%1),$1)>::right_type>
        (arg1_.SIGC_WORKAROUND_OPERATOR_PARENTHESES<LOOP(_P_(T_arg%1), $1)>
            (LOOP(_A_%1, $1)),
         arg2_.SIGC_WORKAROUND_OPERATOR_PARENTHESES<LOOP(_P_(T_arg%1), $1)>
            (LOOP(_A_%1, $1)));
    }

  #ifndef SIGC_TEMPLATE_SPECIALIZATION_OPERATOR_OVERLOAD
  template <LOOP(class T_arg%1, $1)>
  typename deduce_result_type<LOOP(T_arg%1,$1)>::type
  sun_forte_workaround(LOOP(T_arg%1 _A_%1, $1)) const
    {
      return lambda_action<T_action>::template do_action<
            typename deduce_result_type<LOOP(_P_(T_arg%1),$1)>::left_type,
            typename deduce_result_type<LOOP(_P_(T_arg%1),$1)>::right_type>
        (arg1_.SIGC_WORKAROUND_OPERATOR_PARENTHESES<LOOP(_P_(T_arg%1), $1)>
            (LOOP(_A_%1, $1)),
         arg2_.SIGC_WORKAROUND_OPERATOR_PARENTHESES<LOOP(_P_(T_arg%1), $1)>
            (LOOP(_A_%1, $1)));
    }
  #endif //SIGC_TEMPLATE_SPECIALIZATION_OPERATOR_OVERLOAD

])dnl
define([LAMBDA_OPERATOR_UNARY_DO],[dnl
  template <LOOP(class T_arg%1, $1)>
  typename deduce_result_type<LOOP(T_arg%1,$1)>::type
  operator ()(LOOP(T_arg%1 _A_%1, $1)) const
    {
      return lambda_action_unary<T_action>::template do_action<
            typename deduce_result_type<LOOP(_P_(T_arg%1),$1)>::operand_type>
        (arg_.SIGC_WORKAROUND_OPERATOR_PARENTHESES<LOOP(_P_(T_arg%1), $1)>
            (LOOP(_A_%1, $1)));
    }

  #ifndef SIGC_TEMPLATE_SPECIALIZATION_OPERATOR_OVERLOAD
  template <LOOP(class T_arg%1, $1)>
  typename deduce_result_type<LOOP(T_arg%1,$1)>::type
  sun_forte_workaround(LOOP(T_arg%1 _A_%1, $1)) const
    {
      return lambda_action_unary<T_action>::template do_action<
            typename deduce_result_type<LOOP(_P_(T_arg%1),$1)>::operand_type>
        (arg_.SIGC_WORKAROUND_OPERATOR_PARENTHESES<LOOP(_P_(T_arg%1), $1)>
            (LOOP(_A_%1, $1)));
    }
  #endif

])dnl
define([LAMBDA_OPERATOR_CONVERT_DO],[dnl
  template <LOOP(class T_arg%1, $1)>
  typename deduce_result_type<LOOP(T_arg%1,$1)>::type
  operator ()(LOOP(T_arg%1 _A_%1, $1)) const
    {
      return lambda_action_convert<T_action, T_type>::template do_action<
            typename deduce_result_type<LOOP(_P_(T_arg%1),$1)>::operand_type>
        (arg_.SIGC_WORKAROUND_OPERATOR_PARENTHESES<LOOP(_P_(T_arg%1), $1)>
            (LOOP(_A_%1, $1)));
    }

  #ifndef SIGC_TEMPLATE_SPECIALIZATION_OPERATOR_OVERLOAD
  template <LOOP(class T_arg%1, $1)>
  typename deduce_result_type<LOOP(T_arg%1,$1)>::type
  sun_forte_workaround(LOOP(T_arg%1 _A_%1, $1)) const
    {
      return lambda_action_convert<T_action, T_type>::template do_action<
            typename deduce_result_type<LOOP(_P_(T_arg%1),$1)>::operand_type>
        (arg_.SIGC_WORKAROUND_OPERATOR_PARENTHESES<LOOP(_P_(T_arg%1), $1)>
            (LOOP(_A_%1, $1)));
    }
  #endif

])dnl
define([LAMBDA_OPERATOR],[dnl
divert(1)dnl
template <>
struct lambda_action<$1 >
{
  template <class T_arg1, class T_arg2>
  static typename lambda_action_deduce_result_type<$1, T_arg1, T_arg2>::type
  do_action(T_arg1 _A_1, T_arg2 _A_2)
    { return _A_1 $2 _A_2; }
};

divert(2)dnl
// Operators for lambda action $1. At least one of the arguments needs to be of type lamdba, hence the overloads.
template <class T_arg1, class T_arg2>
lambda<lambda_operator<$1, T_arg1, T_arg2> >
operator $2 (const lambda<T_arg1>& a1, const lambda<T_arg2>& a2)
{ typedef lambda_operator<$1, T_arg1, T_arg2> operator_type;
  return lambda<operator_type>(operator_type(a1.value_,a2.value_)); }
template <class T_arg1, class T_arg2>
lambda<lambda_operator<$1, T_arg1, typename unwrap_reference<T_arg2>::type> >
operator $2 (const lambda<T_arg1>& a1, const T_arg2& a2)
{ typedef lambda_operator<$1, T_arg1, typename unwrap_reference<T_arg2>::type> operator_type;
  return lambda<operator_type>(operator_type(a1.value_,a2)); }
template <class T_arg1, class T_arg2>
lambda<lambda_operator<$1, typename unwrap_reference<T_arg1>::type, T_arg2> >
operator $2 (const T_arg1& a1, const lambda<T_arg2>& a2)
{ typedef lambda_operator<$1, typename unwrap_reference<T_arg1>::type, T_arg2> operator_type;
  return lambda<operator_type>(operator_type(a1,a2.value_)); }

divert(0)dnl
])
define([LAMBDA_OPERATOR_UNARY],[dnl
divert(1)dnl
template <>
struct lambda_action_unary<$1 >
{
  template <class T_arg>
  static typename lambda_action_unary_deduce_result_type<$1, T_arg>::type
  do_action(T_arg _Aa)
    { return $2[]_Aa; }
};

divert(2)dnl
// Operator for lambda action $1.
template <class T_arg>
lambda<lambda_operator_unary<$1, T_arg> >
operator $2 (const lambda<T_arg>& a)
{ typedef lambda_operator_unary<$1, T_arg> operator_type;
  return lambda<operator_type>(operator_type(a.value_)); }

divert(0)dnl
])
define([LAMBDA_OPERATOR_CONVERT],[dnl
divert(1)dnl
template <class T_type>
struct lambda_action_convert<$1, T_type>
{
  template <class T_arg>
  static typename lambda_action_convert_deduce_result_type<$1, T_type, T_arg>::type
  do_action(T_arg _Aa)
    { return $2<T_type>(_Aa); }
};

divert(2)dnl
// Creators for lambda action $1.
template <class T_type, class T_arg>
lambda<lambda_operator_convert<$1, T_type, typename unwrap_lambda_type<T_arg>::type> >
$2_(const T_arg& a)
{ typedef lambda_operator_convert<$1, T_type, typename unwrap_lambda_type<T_arg>::type> operator_type;
  return lambda<operator_type>(operator_type(unwrap_lambda_value(a))); }

divert(0)dnl
])
divert(0)dnl
#ifndef _SIGC_LAMBDA_OPERATOR_HPP_
#define _SIGC_LAMBDA_OPERATOR_HPP_
#include <sigc++/adaptors/lambda/base.h>

namespace sigc {

/** Deduces the base type of a reference or a pointer.
 * @ingroup internal
 */
template <class T_type>
struct dereference_trait
  { typedef void type; };

template <class T_type>
struct dereference_trait<T_type*>
  { typedef T_type type; };

template <class T_type>
struct dereference_trait<const T_type*>
  { typedef const T_type type; };

template <class T_type>
struct dereference_trait<T_type*&>
  { typedef T_type type; };

template <class T_type>
struct dereference_trait<const T_type*&>
  { typedef const T_type type; };

template <class T_type>
struct dereference_trait<T_type* const&>
  { typedef T_type type; };

template <class T_type>
struct dereference_trait<const T_type* const&>
  { typedef const T_type type; };

template <class T_type>
struct arithmetic {};

template <class T_type>
struct bitwise {};

template <class T_type>
struct logical {};

template <class T_type>
struct relational {};

template <class T_type>
struct arithmetic_assign {};

template <class T_type>
struct bitwise_assign {};

template <class T_type>
struct other {};

template <class T_type>
struct unary_arithmetic {};

template <class T_type>
struct unary_bitwise {};

template <class T_type>
struct unary_logical {};

template <class T_type>
struct unary_other {};

template <class T_type>
struct cast_ {};

struct plus {};
struct minus {};
struct multiplies {};
struct divides {};
struct modulus {};
struct leftshift {};
struct rightshift {};
struct and_ {};
struct or_ {};
struct xor_ {};
struct less {};
struct greater {};
struct less_equal {};
struct greater_equal {};
struct equal_to {};
struct not_equal_to {};
struct subscript {};
struct assign {};
struct pre_increment {};
struct pre_decrement {};
struct negate {};
struct not_ {};
struct address {};
struct dereference {};
struct reinterpret_ {};
struct static_ {};
struct dynamic_ {};

template <class T_action, class T_test1, class T_test2>
struct lambda_action_deduce_result_type
  { typedef typename type_trait<T_test1>::type type; }; // TODO: e.g. T_test1=int, T_test2=double yields int but it should yield double !

template <class T_action, class T_test1, class T_test2>
struct lambda_action_deduce_result_type<logical<T_action>, T_test1, T_test2>
  { typedef bool type; };

template <class T_action, class T_test1, class T_test2>
struct lambda_action_deduce_result_type<relational<T_action>, T_test1, T_test2>
  { typedef bool type; };

template <class T_action, class T_test1, class T_test2>
struct lambda_action_deduce_result_type<arithmetic_assign<T_action>, T_test1, T_test2>
  { typedef T_test1 type; };

template <class T_action, class T_test1, class T_test2>
struct lambda_action_deduce_result_type<bitwise_assign<T_action>, T_test1, T_test2>
  { typedef T_test1 type; };

template <class T_test1, class T_test2>
struct lambda_action_deduce_result_type<other<subscript>, T_test1, T_test2>
  { typedef typename type_trait<typename dereference_trait<T_test1>::type>::pass type; };

template <class T_action, class T_test>
struct lambda_action_unary_deduce_result_type
  { typedef typename type_trait<T_test>::type type; };

template <class T_action, class T_type, class T_test>
struct lambda_action_convert_deduce_result_type
  { typedef typename type_trait<T_type>::type type; };

template <class T_action, class T_test>
struct lambda_action_unary_deduce_result_type<unary_logical<T_action>, T_test>
  { typedef bool type; };

template <class T_test>
struct lambda_action_unary_deduce_result_type<unary_other<address>, T_test>
  { typedef typename type_trait<T_test>::pointer type; };

template <class T_test>
struct lambda_action_unary_deduce_result_type<unary_other<dereference>, T_test>
  { typedef typename type_trait<typename dereference_trait<T_test>::type>::pass type; };

LAMBDA_OPERATOR(arithmetic<plus>,+)dnl
LAMBDA_OPERATOR(arithmetic<minus>,-)dnl
LAMBDA_OPERATOR(arithmetic<multiplies>,*)dnl
LAMBDA_OPERATOR(arithmetic<divides>,/)dnl
LAMBDA_OPERATOR(arithmetic<modulus>,%)dnl
LAMBDA_OPERATOR(bitwise<leftshift>,<<)dnl
LAMBDA_OPERATOR(bitwise<rightshift>,>>)dnl
LAMBDA_OPERATOR(bitwise<and_>,&)dnl
LAMBDA_OPERATOR(bitwise<or_>,|)dnl
LAMBDA_OPERATOR(bitwise<xor_>,^)dnl
LAMBDA_OPERATOR(logical<and_>,&&)dnl
LAMBDA_OPERATOR(logical<or_>,||)dnl
LAMBDA_OPERATOR(relational<less>,<)dnl
LAMBDA_OPERATOR(relational<greater>,>)dnl
LAMBDA_OPERATOR(relational<less_equal>,<=)dnl
LAMBDA_OPERATOR(relational<greater_equal>,>=)dnl
LAMBDA_OPERATOR(relational<equal_to>,==)dnl
LAMBDA_OPERATOR(relational<not_equal_to>,!=)dnl
LAMBDA_OPERATOR(arithmetic_assign<plus>,+=)dnl
LAMBDA_OPERATOR(arithmetic_assign<minus>,-=)dnl
LAMBDA_OPERATOR(arithmetic_assign<multiplies>,*=)dnl
LAMBDA_OPERATOR(arithmetic_assign<divides>,/=)dnl
LAMBDA_OPERATOR(arithmetic_assign<modulus>,%=)dnl
LAMBDA_OPERATOR(bitwise_assign<leftshift>,<<=)dnl
LAMBDA_OPERATOR(bitwise_assign<rightshift>,>>=)dnl
LAMBDA_OPERATOR(bitwise_assign<and_>,&=)dnl
LAMBDA_OPERATOR(bitwise_assign<or_>,|=)dnl
LAMBDA_OPERATOR(bitwise_assign<xor_>,^=)dnl
divert(1)dnl
template <>
struct lambda_action<other<subscript> >
{
  template <class T_arg1, class T_arg2>
  static typename lambda_action_deduce_result_type<other<subscript>, T_arg1, T_arg2>::type
  do_action(T_arg1 _A_1, T_arg2 _A_2)
    { return _A_1[[_A_2]]; }
};

template <>
struct lambda_action<other<assign> >
{
  template <class T_arg1, class T_arg2>
  static typename lambda_action_deduce_result_type<other<assign>, T_arg1, T_arg2>::type
  do_action(T_arg1 _A_1, T_arg2 _A_2)
    { return _A_1 = _A_2; }
};

divert(0)dnl

LAMBDA_OPERATOR_UNARY(unary_arithmetic<pre_increment>,++)dnl
LAMBDA_OPERATOR_UNARY(unary_arithmetic<pre_decrement>,--)dnl
LAMBDA_OPERATOR_UNARY(unary_arithmetic<negate>,-)dnl
LAMBDA_OPERATOR_UNARY(unary_bitwise<not_>,~)dnl
LAMBDA_OPERATOR_UNARY(unary_logical<not_>,!)dnl
LAMBDA_OPERATOR_UNARY(unary_other<address>,&)dnl
LAMBDA_OPERATOR_UNARY(unary_other<dereference>,*)dnl
LAMBDA_OPERATOR_CONVERT(cast_<reinterpret_>,reinterpret_cast)dnl
LAMBDA_OPERATOR_CONVERT(cast_<static_>,static_cast)dnl
LAMBDA_OPERATOR_CONVERT(cast_<dynamic_>,dynamic_cast)dnl

template <class T_action>
struct lambda_action {};

template <class T_action>
struct lambda_action_unary {};

template <class T_action, class T_type>
struct lambda_action_convert {};

undivert(1)

template <class T_action, class T_type1, class T_type2>
struct lambda_operator : public lambda_base
{
  typedef typename lambda<T_type1>::lambda_type arg1_type;
  typedef typename lambda<T_type2>::lambda_type arg2_type;

  template <LOOP(class T_arg%1=void,CALL_SIZE)>
  struct deduce_result_type
    { typedef typename arg1_type::template deduce_result_type<LOOP(_P_(T_arg%1),CALL_SIZE)>::type left_type;
      typedef typename arg2_type::template deduce_result_type<LOOP(_P_(T_arg%1),CALL_SIZE)>::type right_type;
      typedef typename lambda_action_deduce_result_type<T_action, left_type, right_type>::type type;
    };
  typedef typename lambda_action_deduce_result_type<
      T_action,
      typename arg1_type::result_type,
      typename arg2_type::result_type
    >::type result_type;

  result_type
  operator ()() const;

FOR(1, CALL_SIZE,[[LAMBDA_OPERATOR_DO]](%1))dnl
  lambda_operator(_R_(T_type1) a1, _R_(T_type2) a2 )
    : arg1_(a1), arg2_(a2) {}

  arg1_type arg1_;
  arg2_type arg2_;
};

template <class T_action, class T_type1, class T_type2>
typename lambda_operator<T_action, T_type1, T_type2>::result_type
lambda_operator<T_action, T_type1, T_type2>::operator ()() const
  { return lambda_action<T_action>::template do_action<
      typename arg1_type::result_type,
      typename arg2_type::result_type>
      (arg1_(), arg2_()); }

//template specialization of visit_each<>(action, functor):      
template <class T_action, class T_lambda_action, class T_arg1, class T_arg2>
void visit_each(const T_action& _A_action,
                const lambda_operator<T_lambda_action, T_arg1, T_arg2>& _A_target)
{
  visit_each(_A_action, _A_target.arg1_);
  visit_each(_A_action, _A_target.arg2_);
}


template <class T_action, class T_type>
struct lambda_operator_unary : public lambda_base
{
  typedef typename lambda<T_type>::lambda_type arg_type;

  template <LOOP(class T_arg%1=void,CALL_SIZE)>
  struct deduce_result_type
    { typedef typename arg_type::template deduce_result_type<LOOP(_P_(T_arg%1),CALL_SIZE)>::type operand_type;
      typedef typename lambda_action_unary_deduce_result_type<T_action, operand_type>::type type;
    };
  typedef typename lambda_action_unary_deduce_result_type<
      T_action,
      typename arg_type::result_type
    >::type result_type;

  result_type
  operator ()() const;

FOR(1, CALL_SIZE,[[LAMBDA_OPERATOR_UNARY_DO]](%1))dnl
  lambda_operator_unary(_R_(T_type) a)
    : arg_(a) {}

  arg_type arg_;
};

template <class T_action, class T_type>
typename lambda_operator_unary<T_action, T_type>::result_type
lambda_operator_unary<T_action, T_type>::operator ()() const
  { return lambda_action_unary<T_action>::template do_action<
      typename arg_type::result_type>
      (arg_()); }

//template specialization of visit_each<>(action, functor):
template <class T_action, class T_lambda_action, class T_arg>
void visit_each(const T_action& _A_action,
                const lambda_operator_unary<T_lambda_action, T_arg>& _A_target)
{
  visit_each(_A_action, _A_target.arg_);
}


template <class T_action, class T_type, class T_arg>
struct lambda_operator_convert : public lambda_base
{
  typedef typename lambda<T_arg>::lambda_type arg_type;

  template <LOOP(class T_arg%1=void,CALL_SIZE)>
  struct deduce_result_type
    { typedef typename arg_type::template deduce_result_type<LOOP(_P_(T_arg%1),CALL_SIZE)>::type operand_type;
      typedef typename lambda_action_convert_deduce_result_type<T_action, T_type, operand_type>::type type;
    };
  typedef typename lambda_action_convert_deduce_result_type<
      T_action, T_type,
      typename arg_type::result_type
    >::type result_type;

  result_type
  operator ()() const;

FOR(1, CALL_SIZE,[[LAMBDA_OPERATOR_CONVERT_DO]](%1))dnl
  lambda_operator_convert(_R_(T_arg) a)
    : arg_(a) {}

  arg_type arg_;
};

template <class T_action, class T_type, class T_arg>
typename lambda_operator_convert<T_action, T_type, T_arg>::result_type
lambda_operator_convert<T_action, T_type, T_arg>::operator ()() const
  { return lambda_action_convert<T_action, T_type>::template do_action<
      typename arg_type::result_type>
      (arg_()); }

//template specialization of visit_each<>(action, functor):
template <class T_action, class T_lambda_action, class T_type, class T_arg>
void visit_each(const T_action& _A_action,
                const lambda_operator_convert<T_lambda_action, T_type, T_arg>& _A_target)
{
  visit_each(_A_action, _A_target.arg_);
}


undivert(2)dnl

} /* namespace sigc */

#endif /* _SIGC_LAMBDA_OPERATOR_HPP_ */
