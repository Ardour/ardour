// -*- c++ -*-
/* Do not edit! -- generated file */
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



template <class T_action>
struct lambda_action {};

template <class T_action>
struct lambda_action_unary {};

template <class T_action, class T_type>
struct lambda_action_convert {};

template <>
struct lambda_action<arithmetic<plus> >
{
  template <class T_arg1, class T_arg2>
  static typename lambda_action_deduce_result_type<arithmetic<plus>, T_arg1, T_arg2>::type
  do_action(T_arg1 _A_1, T_arg2 _A_2)
    { return _A_1 + _A_2; }
};

template <>
struct lambda_action<arithmetic<minus> >
{
  template <class T_arg1, class T_arg2>
  static typename lambda_action_deduce_result_type<arithmetic<minus>, T_arg1, T_arg2>::type
  do_action(T_arg1 _A_1, T_arg2 _A_2)
    { return _A_1 - _A_2; }
};

template <>
struct lambda_action<arithmetic<multiplies> >
{
  template <class T_arg1, class T_arg2>
  static typename lambda_action_deduce_result_type<arithmetic<multiplies>, T_arg1, T_arg2>::type
  do_action(T_arg1 _A_1, T_arg2 _A_2)
    { return _A_1 * _A_2; }
};

template <>
struct lambda_action<arithmetic<divides> >
{
  template <class T_arg1, class T_arg2>
  static typename lambda_action_deduce_result_type<arithmetic<divides>, T_arg1, T_arg2>::type
  do_action(T_arg1 _A_1, T_arg2 _A_2)
    { return _A_1 / _A_2; }
};

template <>
struct lambda_action<arithmetic<modulus> >
{
  template <class T_arg1, class T_arg2>
  static typename lambda_action_deduce_result_type<arithmetic<modulus>, T_arg1, T_arg2>::type
  do_action(T_arg1 _A_1, T_arg2 _A_2)
    { return _A_1 % _A_2; }
};

template <>
struct lambda_action<bitwise<leftshift> >
{
  template <class T_arg1, class T_arg2>
  static typename lambda_action_deduce_result_type<bitwise<leftshift>, T_arg1, T_arg2>::type
  do_action(T_arg1 _A_1, T_arg2 _A_2)
    { return _A_1 << _A_2; }
};

template <>
struct lambda_action<bitwise<rightshift> >
{
  template <class T_arg1, class T_arg2>
  static typename lambda_action_deduce_result_type<bitwise<rightshift>, T_arg1, T_arg2>::type
  do_action(T_arg1 _A_1, T_arg2 _A_2)
    { return _A_1 >> _A_2; }
};

template <>
struct lambda_action<bitwise<and_> >
{
  template <class T_arg1, class T_arg2>
  static typename lambda_action_deduce_result_type<bitwise<and_>, T_arg1, T_arg2>::type
  do_action(T_arg1 _A_1, T_arg2 _A_2)
    { return _A_1 & _A_2; }
};

template <>
struct lambda_action<bitwise<or_> >
{
  template <class T_arg1, class T_arg2>
  static typename lambda_action_deduce_result_type<bitwise<or_>, T_arg1, T_arg2>::type
  do_action(T_arg1 _A_1, T_arg2 _A_2)
    { return _A_1 | _A_2; }
};

template <>
struct lambda_action<bitwise<xor_> >
{
  template <class T_arg1, class T_arg2>
  static typename lambda_action_deduce_result_type<bitwise<xor_>, T_arg1, T_arg2>::type
  do_action(T_arg1 _A_1, T_arg2 _A_2)
    { return _A_1 ^ _A_2; }
};

template <>
struct lambda_action<logical<and_> >
{
  template <class T_arg1, class T_arg2>
  static typename lambda_action_deduce_result_type<logical<and_>, T_arg1, T_arg2>::type
  do_action(T_arg1 _A_1, T_arg2 _A_2)
    { return _A_1 && _A_2; }
};

template <>
struct lambda_action<logical<or_> >
{
  template <class T_arg1, class T_arg2>
  static typename lambda_action_deduce_result_type<logical<or_>, T_arg1, T_arg2>::type
  do_action(T_arg1 _A_1, T_arg2 _A_2)
    { return _A_1 || _A_2; }
};

template <>
struct lambda_action<relational<less> >
{
  template <class T_arg1, class T_arg2>
  static typename lambda_action_deduce_result_type<relational<less>, T_arg1, T_arg2>::type
  do_action(T_arg1 _A_1, T_arg2 _A_2)
    { return _A_1 < _A_2; }
};

template <>
struct lambda_action<relational<greater> >
{
  template <class T_arg1, class T_arg2>
  static typename lambda_action_deduce_result_type<relational<greater>, T_arg1, T_arg2>::type
  do_action(T_arg1 _A_1, T_arg2 _A_2)
    { return _A_1 > _A_2; }
};

template <>
struct lambda_action<relational<less_equal> >
{
  template <class T_arg1, class T_arg2>
  static typename lambda_action_deduce_result_type<relational<less_equal>, T_arg1, T_arg2>::type
  do_action(T_arg1 _A_1, T_arg2 _A_2)
    { return _A_1 <= _A_2; }
};

template <>
struct lambda_action<relational<greater_equal> >
{
  template <class T_arg1, class T_arg2>
  static typename lambda_action_deduce_result_type<relational<greater_equal>, T_arg1, T_arg2>::type
  do_action(T_arg1 _A_1, T_arg2 _A_2)
    { return _A_1 >= _A_2; }
};

template <>
struct lambda_action<relational<equal_to> >
{
  template <class T_arg1, class T_arg2>
  static typename lambda_action_deduce_result_type<relational<equal_to>, T_arg1, T_arg2>::type
  do_action(T_arg1 _A_1, T_arg2 _A_2)
    { return _A_1 == _A_2; }
};

template <>
struct lambda_action<relational<not_equal_to> >
{
  template <class T_arg1, class T_arg2>
  static typename lambda_action_deduce_result_type<relational<not_equal_to>, T_arg1, T_arg2>::type
  do_action(T_arg1 _A_1, T_arg2 _A_2)
    { return _A_1 != _A_2; }
};

template <>
struct lambda_action<arithmetic_assign<plus> >
{
  template <class T_arg1, class T_arg2>
  static typename lambda_action_deduce_result_type<arithmetic_assign<plus>, T_arg1, T_arg2>::type
  do_action(T_arg1 _A_1, T_arg2 _A_2)
    { return _A_1 += _A_2; }
};

template <>
struct lambda_action<arithmetic_assign<minus> >
{
  template <class T_arg1, class T_arg2>
  static typename lambda_action_deduce_result_type<arithmetic_assign<minus>, T_arg1, T_arg2>::type
  do_action(T_arg1 _A_1, T_arg2 _A_2)
    { return _A_1 -= _A_2; }
};

template <>
struct lambda_action<arithmetic_assign<multiplies> >
{
  template <class T_arg1, class T_arg2>
  static typename lambda_action_deduce_result_type<arithmetic_assign<multiplies>, T_arg1, T_arg2>::type
  do_action(T_arg1 _A_1, T_arg2 _A_2)
    { return _A_1 *= _A_2; }
};

template <>
struct lambda_action<arithmetic_assign<divides> >
{
  template <class T_arg1, class T_arg2>
  static typename lambda_action_deduce_result_type<arithmetic_assign<divides>, T_arg1, T_arg2>::type
  do_action(T_arg1 _A_1, T_arg2 _A_2)
    { return _A_1 /= _A_2; }
};

template <>
struct lambda_action<arithmetic_assign<modulus> >
{
  template <class T_arg1, class T_arg2>
  static typename lambda_action_deduce_result_type<arithmetic_assign<modulus>, T_arg1, T_arg2>::type
  do_action(T_arg1 _A_1, T_arg2 _A_2)
    { return _A_1 %= _A_2; }
};

template <>
struct lambda_action<bitwise_assign<leftshift> >
{
  template <class T_arg1, class T_arg2>
  static typename lambda_action_deduce_result_type<bitwise_assign<leftshift>, T_arg1, T_arg2>::type
  do_action(T_arg1 _A_1, T_arg2 _A_2)
    { return _A_1 <<= _A_2; }
};

template <>
struct lambda_action<bitwise_assign<rightshift> >
{
  template <class T_arg1, class T_arg2>
  static typename lambda_action_deduce_result_type<bitwise_assign<rightshift>, T_arg1, T_arg2>::type
  do_action(T_arg1 _A_1, T_arg2 _A_2)
    { return _A_1 >>= _A_2; }
};

template <>
struct lambda_action<bitwise_assign<and_> >
{
  template <class T_arg1, class T_arg2>
  static typename lambda_action_deduce_result_type<bitwise_assign<and_>, T_arg1, T_arg2>::type
  do_action(T_arg1 _A_1, T_arg2 _A_2)
    { return _A_1 &= _A_2; }
};

template <>
struct lambda_action<bitwise_assign<or_> >
{
  template <class T_arg1, class T_arg2>
  static typename lambda_action_deduce_result_type<bitwise_assign<or_>, T_arg1, T_arg2>::type
  do_action(T_arg1 _A_1, T_arg2 _A_2)
    { return _A_1 |= _A_2; }
};

template <>
struct lambda_action<bitwise_assign<xor_> >
{
  template <class T_arg1, class T_arg2>
  static typename lambda_action_deduce_result_type<bitwise_assign<xor_>, T_arg1, T_arg2>::type
  do_action(T_arg1 _A_1, T_arg2 _A_2)
    { return _A_1 ^= _A_2; }
};

template <>
struct lambda_action<other<subscript> >
{
  template <class T_arg1, class T_arg2>
  static typename lambda_action_deduce_result_type<other<subscript>, T_arg1, T_arg2>::type
  do_action(T_arg1 _A_1, T_arg2 _A_2)
    { return _A_1[_A_2]; }
};

template <>
struct lambda_action<other<assign> >
{
  template <class T_arg1, class T_arg2>
  static typename lambda_action_deduce_result_type<other<assign>, T_arg1, T_arg2>::type
  do_action(T_arg1 _A_1, T_arg2 _A_2)
    { return _A_1 = _A_2; }
};

template <>
struct lambda_action_unary<unary_arithmetic<pre_increment> >
{
  template <class T_arg>
  static typename lambda_action_unary_deduce_result_type<unary_arithmetic<pre_increment>, T_arg>::type
  do_action(T_arg _Aa)
    { return ++_Aa; }
};

template <>
struct lambda_action_unary<unary_arithmetic<pre_decrement> >
{
  template <class T_arg>
  static typename lambda_action_unary_deduce_result_type<unary_arithmetic<pre_decrement>, T_arg>::type
  do_action(T_arg _Aa)
    { return --_Aa; }
};

template <>
struct lambda_action_unary<unary_arithmetic<negate> >
{
  template <class T_arg>
  static typename lambda_action_unary_deduce_result_type<unary_arithmetic<negate>, T_arg>::type
  do_action(T_arg _Aa)
    { return -_Aa; }
};

template <>
struct lambda_action_unary<unary_bitwise<not_> >
{
  template <class T_arg>
  static typename lambda_action_unary_deduce_result_type<unary_bitwise<not_>, T_arg>::type
  do_action(T_arg _Aa)
    { return ~_Aa; }
};

template <>
struct lambda_action_unary<unary_logical<not_> >
{
  template <class T_arg>
  static typename lambda_action_unary_deduce_result_type<unary_logical<not_>, T_arg>::type
  do_action(T_arg _Aa)
    { return !_Aa; }
};

template <>
struct lambda_action_unary<unary_other<address> >
{
  template <class T_arg>
  static typename lambda_action_unary_deduce_result_type<unary_other<address>, T_arg>::type
  do_action(T_arg _Aa)
    { return &_Aa; }
};

template <>
struct lambda_action_unary<unary_other<dereference> >
{
  template <class T_arg>
  static typename lambda_action_unary_deduce_result_type<unary_other<dereference>, T_arg>::type
  do_action(T_arg _Aa)
    { return *_Aa; }
};

template <class T_type>
struct lambda_action_convert<cast_<reinterpret_>, T_type>
{
  template <class T_arg>
  static typename lambda_action_convert_deduce_result_type<cast_<reinterpret_>, T_type, T_arg>::type
  do_action(T_arg _Aa)
    { return reinterpret_cast<T_type>(_Aa); }
};

template <class T_type>
struct lambda_action_convert<cast_<static_>, T_type>
{
  template <class T_arg>
  static typename lambda_action_convert_deduce_result_type<cast_<static_>, T_type, T_arg>::type
  do_action(T_arg _Aa)
    { return static_cast<T_type>(_Aa); }
};

template <class T_type>
struct lambda_action_convert<cast_<dynamic_>, T_type>
{
  template <class T_arg>
  static typename lambda_action_convert_deduce_result_type<cast_<dynamic_>, T_type, T_arg>::type
  do_action(T_arg _Aa)
    { return dynamic_cast<T_type>(_Aa); }
};



template <class T_action, class T_type1, class T_type2>
struct lambda_operator : public lambda_base
{
  typedef typename lambda<T_type1>::lambda_type arg1_type;
  typedef typename lambda<T_type2>::lambda_type arg2_type;

  template <class T_arg1=void,class T_arg2=void,class T_arg3=void,class T_arg4=void,class T_arg5=void,class T_arg6=void,class T_arg7=void>
  struct deduce_result_type
    { typedef typename arg1_type::template deduce_result_type<typename type_trait<T_arg1>::pass,typename type_trait<T_arg2>::pass,typename type_trait<T_arg3>::pass,typename type_trait<T_arg4>::pass,typename type_trait<T_arg5>::pass,typename type_trait<T_arg6>::pass,typename type_trait<T_arg7>::pass>::type left_type;
      typedef typename arg2_type::template deduce_result_type<typename type_trait<T_arg1>::pass,typename type_trait<T_arg2>::pass,typename type_trait<T_arg3>::pass,typename type_trait<T_arg4>::pass,typename type_trait<T_arg5>::pass,typename type_trait<T_arg6>::pass,typename type_trait<T_arg7>::pass>::type right_type;
      typedef typename lambda_action_deduce_result_type<T_action, left_type, right_type>::type type;
    };
  typedef typename lambda_action_deduce_result_type<
      T_action,
      typename arg1_type::result_type,
      typename arg2_type::result_type
    >::type result_type;

  result_type
  operator ()() const;

  template <class T_arg1>
  typename deduce_result_type<T_arg1>::type
  operator ()(T_arg1 _A_1) const
    {
      return lambda_action<T_action>::template do_action<
            typename deduce_result_type<typename type_trait<T_arg1>::pass>::left_type,
            typename deduce_result_type<typename type_trait<T_arg1>::pass>::right_type>
        (arg1_.SIGC_WORKAROUND_OPERATOR_PARENTHESES<typename type_trait<T_arg1>::pass>
            (_A_1),
         arg2_.SIGC_WORKAROUND_OPERATOR_PARENTHESES<typename type_trait<T_arg1>::pass>
            (_A_1));
    }

  #ifndef SIGC_TEMPLATE_SPECIALIZATION_OPERATOR_OVERLOAD
  template <class T_arg1>
  typename deduce_result_type<T_arg1>::type
  sun_forte_workaround(T_arg1 _A_1) const
    {
      return lambda_action<T_action>::template do_action<
            typename deduce_result_type<typename type_trait<T_arg1>::pass>::left_type,
            typename deduce_result_type<typename type_trait<T_arg1>::pass>::right_type>
        (arg1_.SIGC_WORKAROUND_OPERATOR_PARENTHESES<typename type_trait<T_arg1>::pass>
            (_A_1),
         arg2_.SIGC_WORKAROUND_OPERATOR_PARENTHESES<typename type_trait<T_arg1>::pass>
            (_A_1));
    }
  #endif //SIGC_TEMPLATE_SPECIALIZATION_OPERATOR_OVERLOAD

  template <class T_arg1,class T_arg2>
  typename deduce_result_type<T_arg1,T_arg2>::type
  operator ()(T_arg1 _A_1,T_arg2 _A_2) const
    {
      return lambda_action<T_action>::template do_action<
            typename deduce_result_type<typename type_trait<T_arg1>::pass,typename type_trait<T_arg2>::pass>::left_type,
            typename deduce_result_type<typename type_trait<T_arg1>::pass,typename type_trait<T_arg2>::pass>::right_type>
        (arg1_.SIGC_WORKAROUND_OPERATOR_PARENTHESES<typename type_trait<T_arg1>::pass,typename type_trait<T_arg2>::pass>
            (_A_1,_A_2),
         arg2_.SIGC_WORKAROUND_OPERATOR_PARENTHESES<typename type_trait<T_arg1>::pass,typename type_trait<T_arg2>::pass>
            (_A_1,_A_2));
    }

  #ifndef SIGC_TEMPLATE_SPECIALIZATION_OPERATOR_OVERLOAD
  template <class T_arg1,class T_arg2>
  typename deduce_result_type<T_arg1,T_arg2>::type
  sun_forte_workaround(T_arg1 _A_1,T_arg2 _A_2) const
    {
      return lambda_action<T_action>::template do_action<
            typename deduce_result_type<typename type_trait<T_arg1>::pass,typename type_trait<T_arg2>::pass>::left_type,
            typename deduce_result_type<typename type_trait<T_arg1>::pass,typename type_trait<T_arg2>::pass>::right_type>
        (arg1_.SIGC_WORKAROUND_OPERATOR_PARENTHESES<typename type_trait<T_arg1>::pass,typename type_trait<T_arg2>::pass>
            (_A_1,_A_2),
         arg2_.SIGC_WORKAROUND_OPERATOR_PARENTHESES<typename type_trait<T_arg1>::pass,typename type_trait<T_arg2>::pass>
            (_A_1,_A_2));
    }
  #endif //SIGC_TEMPLATE_SPECIALIZATION_OPERATOR_OVERLOAD

  template <class T_arg1,class T_arg2,class T_arg3>
  typename deduce_result_type<T_arg1,T_arg2,T_arg3>::type
  operator ()(T_arg1 _A_1,T_arg2 _A_2,T_arg3 _A_3) const
    {
      return lambda_action<T_action>::template do_action<
            typename deduce_result_type<typename type_trait<T_arg1>::pass,typename type_trait<T_arg2>::pass,typename type_trait<T_arg3>::pass>::left_type,
            typename deduce_result_type<typename type_trait<T_arg1>::pass,typename type_trait<T_arg2>::pass,typename type_trait<T_arg3>::pass>::right_type>
        (arg1_.SIGC_WORKAROUND_OPERATOR_PARENTHESES<typename type_trait<T_arg1>::pass,typename type_trait<T_arg2>::pass,typename type_trait<T_arg3>::pass>
            (_A_1,_A_2,_A_3),
         arg2_.SIGC_WORKAROUND_OPERATOR_PARENTHESES<typename type_trait<T_arg1>::pass,typename type_trait<T_arg2>::pass,typename type_trait<T_arg3>::pass>
            (_A_1,_A_2,_A_3));
    }

  #ifndef SIGC_TEMPLATE_SPECIALIZATION_OPERATOR_OVERLOAD
  template <class T_arg1,class T_arg2,class T_arg3>
  typename deduce_result_type<T_arg1,T_arg2,T_arg3>::type
  sun_forte_workaround(T_arg1 _A_1,T_arg2 _A_2,T_arg3 _A_3) const
    {
      return lambda_action<T_action>::template do_action<
            typename deduce_result_type<typename type_trait<T_arg1>::pass,typename type_trait<T_arg2>::pass,typename type_trait<T_arg3>::pass>::left_type,
            typename deduce_result_type<typename type_trait<T_arg1>::pass,typename type_trait<T_arg2>::pass,typename type_trait<T_arg3>::pass>::right_type>
        (arg1_.SIGC_WORKAROUND_OPERATOR_PARENTHESES<typename type_trait<T_arg1>::pass,typename type_trait<T_arg2>::pass,typename type_trait<T_arg3>::pass>
            (_A_1,_A_2,_A_3),
         arg2_.SIGC_WORKAROUND_OPERATOR_PARENTHESES<typename type_trait<T_arg1>::pass,typename type_trait<T_arg2>::pass,typename type_trait<T_arg3>::pass>
            (_A_1,_A_2,_A_3));
    }
  #endif //SIGC_TEMPLATE_SPECIALIZATION_OPERATOR_OVERLOAD

  template <class T_arg1,class T_arg2,class T_arg3,class T_arg4>
  typename deduce_result_type<T_arg1,T_arg2,T_arg3,T_arg4>::type
  operator ()(T_arg1 _A_1,T_arg2 _A_2,T_arg3 _A_3,T_arg4 _A_4) const
    {
      return lambda_action<T_action>::template do_action<
            typename deduce_result_type<typename type_trait<T_arg1>::pass,typename type_trait<T_arg2>::pass,typename type_trait<T_arg3>::pass,typename type_trait<T_arg4>::pass>::left_type,
            typename deduce_result_type<typename type_trait<T_arg1>::pass,typename type_trait<T_arg2>::pass,typename type_trait<T_arg3>::pass,typename type_trait<T_arg4>::pass>::right_type>
        (arg1_.SIGC_WORKAROUND_OPERATOR_PARENTHESES<typename type_trait<T_arg1>::pass,typename type_trait<T_arg2>::pass,typename type_trait<T_arg3>::pass,typename type_trait<T_arg4>::pass>
            (_A_1,_A_2,_A_3,_A_4),
         arg2_.SIGC_WORKAROUND_OPERATOR_PARENTHESES<typename type_trait<T_arg1>::pass,typename type_trait<T_arg2>::pass,typename type_trait<T_arg3>::pass,typename type_trait<T_arg4>::pass>
            (_A_1,_A_2,_A_3,_A_4));
    }

  #ifndef SIGC_TEMPLATE_SPECIALIZATION_OPERATOR_OVERLOAD
  template <class T_arg1,class T_arg2,class T_arg3,class T_arg4>
  typename deduce_result_type<T_arg1,T_arg2,T_arg3,T_arg4>::type
  sun_forte_workaround(T_arg1 _A_1,T_arg2 _A_2,T_arg3 _A_3,T_arg4 _A_4) const
    {
      return lambda_action<T_action>::template do_action<
            typename deduce_result_type<typename type_trait<T_arg1>::pass,typename type_trait<T_arg2>::pass,typename type_trait<T_arg3>::pass,typename type_trait<T_arg4>::pass>::left_type,
            typename deduce_result_type<typename type_trait<T_arg1>::pass,typename type_trait<T_arg2>::pass,typename type_trait<T_arg3>::pass,typename type_trait<T_arg4>::pass>::right_type>
        (arg1_.SIGC_WORKAROUND_OPERATOR_PARENTHESES<typename type_trait<T_arg1>::pass,typename type_trait<T_arg2>::pass,typename type_trait<T_arg3>::pass,typename type_trait<T_arg4>::pass>
            (_A_1,_A_2,_A_3,_A_4),
         arg2_.SIGC_WORKAROUND_OPERATOR_PARENTHESES<typename type_trait<T_arg1>::pass,typename type_trait<T_arg2>::pass,typename type_trait<T_arg3>::pass,typename type_trait<T_arg4>::pass>
            (_A_1,_A_2,_A_3,_A_4));
    }
  #endif //SIGC_TEMPLATE_SPECIALIZATION_OPERATOR_OVERLOAD

  template <class T_arg1,class T_arg2,class T_arg3,class T_arg4,class T_arg5>
  typename deduce_result_type<T_arg1,T_arg2,T_arg3,T_arg4,T_arg5>::type
  operator ()(T_arg1 _A_1,T_arg2 _A_2,T_arg3 _A_3,T_arg4 _A_4,T_arg5 _A_5) const
    {
      return lambda_action<T_action>::template do_action<
            typename deduce_result_type<typename type_trait<T_arg1>::pass,typename type_trait<T_arg2>::pass,typename type_trait<T_arg3>::pass,typename type_trait<T_arg4>::pass,typename type_trait<T_arg5>::pass>::left_type,
            typename deduce_result_type<typename type_trait<T_arg1>::pass,typename type_trait<T_arg2>::pass,typename type_trait<T_arg3>::pass,typename type_trait<T_arg4>::pass,typename type_trait<T_arg5>::pass>::right_type>
        (arg1_.SIGC_WORKAROUND_OPERATOR_PARENTHESES<typename type_trait<T_arg1>::pass,typename type_trait<T_arg2>::pass,typename type_trait<T_arg3>::pass,typename type_trait<T_arg4>::pass,typename type_trait<T_arg5>::pass>
            (_A_1,_A_2,_A_3,_A_4,_A_5),
         arg2_.SIGC_WORKAROUND_OPERATOR_PARENTHESES<typename type_trait<T_arg1>::pass,typename type_trait<T_arg2>::pass,typename type_trait<T_arg3>::pass,typename type_trait<T_arg4>::pass,typename type_trait<T_arg5>::pass>
            (_A_1,_A_2,_A_3,_A_4,_A_5));
    }

  #ifndef SIGC_TEMPLATE_SPECIALIZATION_OPERATOR_OVERLOAD
  template <class T_arg1,class T_arg2,class T_arg3,class T_arg4,class T_arg5>
  typename deduce_result_type<T_arg1,T_arg2,T_arg3,T_arg4,T_arg5>::type
  sun_forte_workaround(T_arg1 _A_1,T_arg2 _A_2,T_arg3 _A_3,T_arg4 _A_4,T_arg5 _A_5) const
    {
      return lambda_action<T_action>::template do_action<
            typename deduce_result_type<typename type_trait<T_arg1>::pass,typename type_trait<T_arg2>::pass,typename type_trait<T_arg3>::pass,typename type_trait<T_arg4>::pass,typename type_trait<T_arg5>::pass>::left_type,
            typename deduce_result_type<typename type_trait<T_arg1>::pass,typename type_trait<T_arg2>::pass,typename type_trait<T_arg3>::pass,typename type_trait<T_arg4>::pass,typename type_trait<T_arg5>::pass>::right_type>
        (arg1_.SIGC_WORKAROUND_OPERATOR_PARENTHESES<typename type_trait<T_arg1>::pass,typename type_trait<T_arg2>::pass,typename type_trait<T_arg3>::pass,typename type_trait<T_arg4>::pass,typename type_trait<T_arg5>::pass>
            (_A_1,_A_2,_A_3,_A_4,_A_5),
         arg2_.SIGC_WORKAROUND_OPERATOR_PARENTHESES<typename type_trait<T_arg1>::pass,typename type_trait<T_arg2>::pass,typename type_trait<T_arg3>::pass,typename type_trait<T_arg4>::pass,typename type_trait<T_arg5>::pass>
            (_A_1,_A_2,_A_3,_A_4,_A_5));
    }
  #endif //SIGC_TEMPLATE_SPECIALIZATION_OPERATOR_OVERLOAD

  template <class T_arg1,class T_arg2,class T_arg3,class T_arg4,class T_arg5,class T_arg6>
  typename deduce_result_type<T_arg1,T_arg2,T_arg3,T_arg4,T_arg5,T_arg6>::type
  operator ()(T_arg1 _A_1,T_arg2 _A_2,T_arg3 _A_3,T_arg4 _A_4,T_arg5 _A_5,T_arg6 _A_6) const
    {
      return lambda_action<T_action>::template do_action<
            typename deduce_result_type<typename type_trait<T_arg1>::pass,typename type_trait<T_arg2>::pass,typename type_trait<T_arg3>::pass,typename type_trait<T_arg4>::pass,typename type_trait<T_arg5>::pass,typename type_trait<T_arg6>::pass>::left_type,
            typename deduce_result_type<typename type_trait<T_arg1>::pass,typename type_trait<T_arg2>::pass,typename type_trait<T_arg3>::pass,typename type_trait<T_arg4>::pass,typename type_trait<T_arg5>::pass,typename type_trait<T_arg6>::pass>::right_type>
        (arg1_.SIGC_WORKAROUND_OPERATOR_PARENTHESES<typename type_trait<T_arg1>::pass,typename type_trait<T_arg2>::pass,typename type_trait<T_arg3>::pass,typename type_trait<T_arg4>::pass,typename type_trait<T_arg5>::pass,typename type_trait<T_arg6>::pass>
            (_A_1,_A_2,_A_3,_A_4,_A_5,_A_6),
         arg2_.SIGC_WORKAROUND_OPERATOR_PARENTHESES<typename type_trait<T_arg1>::pass,typename type_trait<T_arg2>::pass,typename type_trait<T_arg3>::pass,typename type_trait<T_arg4>::pass,typename type_trait<T_arg5>::pass,typename type_trait<T_arg6>::pass>
            (_A_1,_A_2,_A_3,_A_4,_A_5,_A_6));
    }

  #ifndef SIGC_TEMPLATE_SPECIALIZATION_OPERATOR_OVERLOAD
  template <class T_arg1,class T_arg2,class T_arg3,class T_arg4,class T_arg5,class T_arg6>
  typename deduce_result_type<T_arg1,T_arg2,T_arg3,T_arg4,T_arg5,T_arg6>::type
  sun_forte_workaround(T_arg1 _A_1,T_arg2 _A_2,T_arg3 _A_3,T_arg4 _A_4,T_arg5 _A_5,T_arg6 _A_6) const
    {
      return lambda_action<T_action>::template do_action<
            typename deduce_result_type<typename type_trait<T_arg1>::pass,typename type_trait<T_arg2>::pass,typename type_trait<T_arg3>::pass,typename type_trait<T_arg4>::pass,typename type_trait<T_arg5>::pass,typename type_trait<T_arg6>::pass>::left_type,
            typename deduce_result_type<typename type_trait<T_arg1>::pass,typename type_trait<T_arg2>::pass,typename type_trait<T_arg3>::pass,typename type_trait<T_arg4>::pass,typename type_trait<T_arg5>::pass,typename type_trait<T_arg6>::pass>::right_type>
        (arg1_.SIGC_WORKAROUND_OPERATOR_PARENTHESES<typename type_trait<T_arg1>::pass,typename type_trait<T_arg2>::pass,typename type_trait<T_arg3>::pass,typename type_trait<T_arg4>::pass,typename type_trait<T_arg5>::pass,typename type_trait<T_arg6>::pass>
            (_A_1,_A_2,_A_3,_A_4,_A_5,_A_6),
         arg2_.SIGC_WORKAROUND_OPERATOR_PARENTHESES<typename type_trait<T_arg1>::pass,typename type_trait<T_arg2>::pass,typename type_trait<T_arg3>::pass,typename type_trait<T_arg4>::pass,typename type_trait<T_arg5>::pass,typename type_trait<T_arg6>::pass>
            (_A_1,_A_2,_A_3,_A_4,_A_5,_A_6));
    }
  #endif //SIGC_TEMPLATE_SPECIALIZATION_OPERATOR_OVERLOAD

  template <class T_arg1,class T_arg2,class T_arg3,class T_arg4,class T_arg5,class T_arg6,class T_arg7>
  typename deduce_result_type<T_arg1,T_arg2,T_arg3,T_arg4,T_arg5,T_arg6,T_arg7>::type
  operator ()(T_arg1 _A_1,T_arg2 _A_2,T_arg3 _A_3,T_arg4 _A_4,T_arg5 _A_5,T_arg6 _A_6,T_arg7 _A_7) const
    {
      return lambda_action<T_action>::template do_action<
            typename deduce_result_type<typename type_trait<T_arg1>::pass,typename type_trait<T_arg2>::pass,typename type_trait<T_arg3>::pass,typename type_trait<T_arg4>::pass,typename type_trait<T_arg5>::pass,typename type_trait<T_arg6>::pass,typename type_trait<T_arg7>::pass>::left_type,
            typename deduce_result_type<typename type_trait<T_arg1>::pass,typename type_trait<T_arg2>::pass,typename type_trait<T_arg3>::pass,typename type_trait<T_arg4>::pass,typename type_trait<T_arg5>::pass,typename type_trait<T_arg6>::pass,typename type_trait<T_arg7>::pass>::right_type>
        (arg1_.SIGC_WORKAROUND_OPERATOR_PARENTHESES<typename type_trait<T_arg1>::pass,typename type_trait<T_arg2>::pass,typename type_trait<T_arg3>::pass,typename type_trait<T_arg4>::pass,typename type_trait<T_arg5>::pass,typename type_trait<T_arg6>::pass,typename type_trait<T_arg7>::pass>
            (_A_1,_A_2,_A_3,_A_4,_A_5,_A_6,_A_7),
         arg2_.SIGC_WORKAROUND_OPERATOR_PARENTHESES<typename type_trait<T_arg1>::pass,typename type_trait<T_arg2>::pass,typename type_trait<T_arg3>::pass,typename type_trait<T_arg4>::pass,typename type_trait<T_arg5>::pass,typename type_trait<T_arg6>::pass,typename type_trait<T_arg7>::pass>
            (_A_1,_A_2,_A_3,_A_4,_A_5,_A_6,_A_7));
    }

  #ifndef SIGC_TEMPLATE_SPECIALIZATION_OPERATOR_OVERLOAD
  template <class T_arg1,class T_arg2,class T_arg3,class T_arg4,class T_arg5,class T_arg6,class T_arg7>
  typename deduce_result_type<T_arg1,T_arg2,T_arg3,T_arg4,T_arg5,T_arg6,T_arg7>::type
  sun_forte_workaround(T_arg1 _A_1,T_arg2 _A_2,T_arg3 _A_3,T_arg4 _A_4,T_arg5 _A_5,T_arg6 _A_6,T_arg7 _A_7) const
    {
      return lambda_action<T_action>::template do_action<
            typename deduce_result_type<typename type_trait<T_arg1>::pass,typename type_trait<T_arg2>::pass,typename type_trait<T_arg3>::pass,typename type_trait<T_arg4>::pass,typename type_trait<T_arg5>::pass,typename type_trait<T_arg6>::pass,typename type_trait<T_arg7>::pass>::left_type,
            typename deduce_result_type<typename type_trait<T_arg1>::pass,typename type_trait<T_arg2>::pass,typename type_trait<T_arg3>::pass,typename type_trait<T_arg4>::pass,typename type_trait<T_arg5>::pass,typename type_trait<T_arg6>::pass,typename type_trait<T_arg7>::pass>::right_type>
        (arg1_.SIGC_WORKAROUND_OPERATOR_PARENTHESES<typename type_trait<T_arg1>::pass,typename type_trait<T_arg2>::pass,typename type_trait<T_arg3>::pass,typename type_trait<T_arg4>::pass,typename type_trait<T_arg5>::pass,typename type_trait<T_arg6>::pass,typename type_trait<T_arg7>::pass>
            (_A_1,_A_2,_A_3,_A_4,_A_5,_A_6,_A_7),
         arg2_.SIGC_WORKAROUND_OPERATOR_PARENTHESES<typename type_trait<T_arg1>::pass,typename type_trait<T_arg2>::pass,typename type_trait<T_arg3>::pass,typename type_trait<T_arg4>::pass,typename type_trait<T_arg5>::pass,typename type_trait<T_arg6>::pass,typename type_trait<T_arg7>::pass>
            (_A_1,_A_2,_A_3,_A_4,_A_5,_A_6,_A_7));
    }
  #endif //SIGC_TEMPLATE_SPECIALIZATION_OPERATOR_OVERLOAD

  lambda_operator(typename type_trait<T_type1>::take a1, typename type_trait<T_type2>::take a2 )
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

  template <class T_arg1=void,class T_arg2=void,class T_arg3=void,class T_arg4=void,class T_arg5=void,class T_arg6=void,class T_arg7=void>
  struct deduce_result_type
    { typedef typename arg_type::template deduce_result_type<typename type_trait<T_arg1>::pass,typename type_trait<T_arg2>::pass,typename type_trait<T_arg3>::pass,typename type_trait<T_arg4>::pass,typename type_trait<T_arg5>::pass,typename type_trait<T_arg6>::pass,typename type_trait<T_arg7>::pass>::type operand_type;
      typedef typename lambda_action_unary_deduce_result_type<T_action, operand_type>::type type;
    };
  typedef typename lambda_action_unary_deduce_result_type<
      T_action,
      typename arg_type::result_type
    >::type result_type;

  result_type
  operator ()() const;

  template <class T_arg1>
  typename deduce_result_type<T_arg1>::type
  operator ()(T_arg1 _A_1) const
    {
      return lambda_action_unary<T_action>::template do_action<
            typename deduce_result_type<typename type_trait<T_arg1>::pass>::operand_type>
        (arg_.SIGC_WORKAROUND_OPERATOR_PARENTHESES<typename type_trait<T_arg1>::pass>
            (_A_1));
    }

  #ifndef SIGC_TEMPLATE_SPECIALIZATION_OPERATOR_OVERLOAD
  template <class T_arg1>
  typename deduce_result_type<T_arg1>::type
  sun_forte_workaround(T_arg1 _A_1) const
    {
      return lambda_action_unary<T_action>::template do_action<
            typename deduce_result_type<typename type_trait<T_arg1>::pass>::operand_type>
        (arg_.SIGC_WORKAROUND_OPERATOR_PARENTHESES<typename type_trait<T_arg1>::pass>
            (_A_1));
    }
  #endif

  template <class T_arg1,class T_arg2>
  typename deduce_result_type<T_arg1,T_arg2>::type
  operator ()(T_arg1 _A_1,T_arg2 _A_2) const
    {
      return lambda_action_unary<T_action>::template do_action<
            typename deduce_result_type<typename type_trait<T_arg1>::pass,typename type_trait<T_arg2>::pass>::operand_type>
        (arg_.SIGC_WORKAROUND_OPERATOR_PARENTHESES<typename type_trait<T_arg1>::pass,typename type_trait<T_arg2>::pass>
            (_A_1,_A_2));
    }

  #ifndef SIGC_TEMPLATE_SPECIALIZATION_OPERATOR_OVERLOAD
  template <class T_arg1,class T_arg2>
  typename deduce_result_type<T_arg1,T_arg2>::type
  sun_forte_workaround(T_arg1 _A_1,T_arg2 _A_2) const
    {
      return lambda_action_unary<T_action>::template do_action<
            typename deduce_result_type<typename type_trait<T_arg1>::pass,typename type_trait<T_arg2>::pass>::operand_type>
        (arg_.SIGC_WORKAROUND_OPERATOR_PARENTHESES<typename type_trait<T_arg1>::pass,typename type_trait<T_arg2>::pass>
            (_A_1,_A_2));
    }
  #endif

  template <class T_arg1,class T_arg2,class T_arg3>
  typename deduce_result_type<T_arg1,T_arg2,T_arg3>::type
  operator ()(T_arg1 _A_1,T_arg2 _A_2,T_arg3 _A_3) const
    {
      return lambda_action_unary<T_action>::template do_action<
            typename deduce_result_type<typename type_trait<T_arg1>::pass,typename type_trait<T_arg2>::pass,typename type_trait<T_arg3>::pass>::operand_type>
        (arg_.SIGC_WORKAROUND_OPERATOR_PARENTHESES<typename type_trait<T_arg1>::pass,typename type_trait<T_arg2>::pass,typename type_trait<T_arg3>::pass>
            (_A_1,_A_2,_A_3));
    }

  #ifndef SIGC_TEMPLATE_SPECIALIZATION_OPERATOR_OVERLOAD
  template <class T_arg1,class T_arg2,class T_arg3>
  typename deduce_result_type<T_arg1,T_arg2,T_arg3>::type
  sun_forte_workaround(T_arg1 _A_1,T_arg2 _A_2,T_arg3 _A_3) const
    {
      return lambda_action_unary<T_action>::template do_action<
            typename deduce_result_type<typename type_trait<T_arg1>::pass,typename type_trait<T_arg2>::pass,typename type_trait<T_arg3>::pass>::operand_type>
        (arg_.SIGC_WORKAROUND_OPERATOR_PARENTHESES<typename type_trait<T_arg1>::pass,typename type_trait<T_arg2>::pass,typename type_trait<T_arg3>::pass>
            (_A_1,_A_2,_A_3));
    }
  #endif

  template <class T_arg1,class T_arg2,class T_arg3,class T_arg4>
  typename deduce_result_type<T_arg1,T_arg2,T_arg3,T_arg4>::type
  operator ()(T_arg1 _A_1,T_arg2 _A_2,T_arg3 _A_3,T_arg4 _A_4) const
    {
      return lambda_action_unary<T_action>::template do_action<
            typename deduce_result_type<typename type_trait<T_arg1>::pass,typename type_trait<T_arg2>::pass,typename type_trait<T_arg3>::pass,typename type_trait<T_arg4>::pass>::operand_type>
        (arg_.SIGC_WORKAROUND_OPERATOR_PARENTHESES<typename type_trait<T_arg1>::pass,typename type_trait<T_arg2>::pass,typename type_trait<T_arg3>::pass,typename type_trait<T_arg4>::pass>
            (_A_1,_A_2,_A_3,_A_4));
    }

  #ifndef SIGC_TEMPLATE_SPECIALIZATION_OPERATOR_OVERLOAD
  template <class T_arg1,class T_arg2,class T_arg3,class T_arg4>
  typename deduce_result_type<T_arg1,T_arg2,T_arg3,T_arg4>::type
  sun_forte_workaround(T_arg1 _A_1,T_arg2 _A_2,T_arg3 _A_3,T_arg4 _A_4) const
    {
      return lambda_action_unary<T_action>::template do_action<
            typename deduce_result_type<typename type_trait<T_arg1>::pass,typename type_trait<T_arg2>::pass,typename type_trait<T_arg3>::pass,typename type_trait<T_arg4>::pass>::operand_type>
        (arg_.SIGC_WORKAROUND_OPERATOR_PARENTHESES<typename type_trait<T_arg1>::pass,typename type_trait<T_arg2>::pass,typename type_trait<T_arg3>::pass,typename type_trait<T_arg4>::pass>
            (_A_1,_A_2,_A_3,_A_4));
    }
  #endif

  template <class T_arg1,class T_arg2,class T_arg3,class T_arg4,class T_arg5>
  typename deduce_result_type<T_arg1,T_arg2,T_arg3,T_arg4,T_arg5>::type
  operator ()(T_arg1 _A_1,T_arg2 _A_2,T_arg3 _A_3,T_arg4 _A_4,T_arg5 _A_5) const
    {
      return lambda_action_unary<T_action>::template do_action<
            typename deduce_result_type<typename type_trait<T_arg1>::pass,typename type_trait<T_arg2>::pass,typename type_trait<T_arg3>::pass,typename type_trait<T_arg4>::pass,typename type_trait<T_arg5>::pass>::operand_type>
        (arg_.SIGC_WORKAROUND_OPERATOR_PARENTHESES<typename type_trait<T_arg1>::pass,typename type_trait<T_arg2>::pass,typename type_trait<T_arg3>::pass,typename type_trait<T_arg4>::pass,typename type_trait<T_arg5>::pass>
            (_A_1,_A_2,_A_3,_A_4,_A_5));
    }

  #ifndef SIGC_TEMPLATE_SPECIALIZATION_OPERATOR_OVERLOAD
  template <class T_arg1,class T_arg2,class T_arg3,class T_arg4,class T_arg5>
  typename deduce_result_type<T_arg1,T_arg2,T_arg3,T_arg4,T_arg5>::type
  sun_forte_workaround(T_arg1 _A_1,T_arg2 _A_2,T_arg3 _A_3,T_arg4 _A_4,T_arg5 _A_5) const
    {
      return lambda_action_unary<T_action>::template do_action<
            typename deduce_result_type<typename type_trait<T_arg1>::pass,typename type_trait<T_arg2>::pass,typename type_trait<T_arg3>::pass,typename type_trait<T_arg4>::pass,typename type_trait<T_arg5>::pass>::operand_type>
        (arg_.SIGC_WORKAROUND_OPERATOR_PARENTHESES<typename type_trait<T_arg1>::pass,typename type_trait<T_arg2>::pass,typename type_trait<T_arg3>::pass,typename type_trait<T_arg4>::pass,typename type_trait<T_arg5>::pass>
            (_A_1,_A_2,_A_3,_A_4,_A_5));
    }
  #endif

  template <class T_arg1,class T_arg2,class T_arg3,class T_arg4,class T_arg5,class T_arg6>
  typename deduce_result_type<T_arg1,T_arg2,T_arg3,T_arg4,T_arg5,T_arg6>::type
  operator ()(T_arg1 _A_1,T_arg2 _A_2,T_arg3 _A_3,T_arg4 _A_4,T_arg5 _A_5,T_arg6 _A_6) const
    {
      return lambda_action_unary<T_action>::template do_action<
            typename deduce_result_type<typename type_trait<T_arg1>::pass,typename type_trait<T_arg2>::pass,typename type_trait<T_arg3>::pass,typename type_trait<T_arg4>::pass,typename type_trait<T_arg5>::pass,typename type_trait<T_arg6>::pass>::operand_type>
        (arg_.SIGC_WORKAROUND_OPERATOR_PARENTHESES<typename type_trait<T_arg1>::pass,typename type_trait<T_arg2>::pass,typename type_trait<T_arg3>::pass,typename type_trait<T_arg4>::pass,typename type_trait<T_arg5>::pass,typename type_trait<T_arg6>::pass>
            (_A_1,_A_2,_A_3,_A_4,_A_5,_A_6));
    }

  #ifndef SIGC_TEMPLATE_SPECIALIZATION_OPERATOR_OVERLOAD
  template <class T_arg1,class T_arg2,class T_arg3,class T_arg4,class T_arg5,class T_arg6>
  typename deduce_result_type<T_arg1,T_arg2,T_arg3,T_arg4,T_arg5,T_arg6>::type
  sun_forte_workaround(T_arg1 _A_1,T_arg2 _A_2,T_arg3 _A_3,T_arg4 _A_4,T_arg5 _A_5,T_arg6 _A_6) const
    {
      return lambda_action_unary<T_action>::template do_action<
            typename deduce_result_type<typename type_trait<T_arg1>::pass,typename type_trait<T_arg2>::pass,typename type_trait<T_arg3>::pass,typename type_trait<T_arg4>::pass,typename type_trait<T_arg5>::pass,typename type_trait<T_arg6>::pass>::operand_type>
        (arg_.SIGC_WORKAROUND_OPERATOR_PARENTHESES<typename type_trait<T_arg1>::pass,typename type_trait<T_arg2>::pass,typename type_trait<T_arg3>::pass,typename type_trait<T_arg4>::pass,typename type_trait<T_arg5>::pass,typename type_trait<T_arg6>::pass>
            (_A_1,_A_2,_A_3,_A_4,_A_5,_A_6));
    }
  #endif

  template <class T_arg1,class T_arg2,class T_arg3,class T_arg4,class T_arg5,class T_arg6,class T_arg7>
  typename deduce_result_type<T_arg1,T_arg2,T_arg3,T_arg4,T_arg5,T_arg6,T_arg7>::type
  operator ()(T_arg1 _A_1,T_arg2 _A_2,T_arg3 _A_3,T_arg4 _A_4,T_arg5 _A_5,T_arg6 _A_6,T_arg7 _A_7) const
    {
      return lambda_action_unary<T_action>::template do_action<
            typename deduce_result_type<typename type_trait<T_arg1>::pass,typename type_trait<T_arg2>::pass,typename type_trait<T_arg3>::pass,typename type_trait<T_arg4>::pass,typename type_trait<T_arg5>::pass,typename type_trait<T_arg6>::pass,typename type_trait<T_arg7>::pass>::operand_type>
        (arg_.SIGC_WORKAROUND_OPERATOR_PARENTHESES<typename type_trait<T_arg1>::pass,typename type_trait<T_arg2>::pass,typename type_trait<T_arg3>::pass,typename type_trait<T_arg4>::pass,typename type_trait<T_arg5>::pass,typename type_trait<T_arg6>::pass,typename type_trait<T_arg7>::pass>
            (_A_1,_A_2,_A_3,_A_4,_A_5,_A_6,_A_7));
    }

  #ifndef SIGC_TEMPLATE_SPECIALIZATION_OPERATOR_OVERLOAD
  template <class T_arg1,class T_arg2,class T_arg3,class T_arg4,class T_arg5,class T_arg6,class T_arg7>
  typename deduce_result_type<T_arg1,T_arg2,T_arg3,T_arg4,T_arg5,T_arg6,T_arg7>::type
  sun_forte_workaround(T_arg1 _A_1,T_arg2 _A_2,T_arg3 _A_3,T_arg4 _A_4,T_arg5 _A_5,T_arg6 _A_6,T_arg7 _A_7) const
    {
      return lambda_action_unary<T_action>::template do_action<
            typename deduce_result_type<typename type_trait<T_arg1>::pass,typename type_trait<T_arg2>::pass,typename type_trait<T_arg3>::pass,typename type_trait<T_arg4>::pass,typename type_trait<T_arg5>::pass,typename type_trait<T_arg6>::pass,typename type_trait<T_arg7>::pass>::operand_type>
        (arg_.SIGC_WORKAROUND_OPERATOR_PARENTHESES<typename type_trait<T_arg1>::pass,typename type_trait<T_arg2>::pass,typename type_trait<T_arg3>::pass,typename type_trait<T_arg4>::pass,typename type_trait<T_arg5>::pass,typename type_trait<T_arg6>::pass,typename type_trait<T_arg7>::pass>
            (_A_1,_A_2,_A_3,_A_4,_A_5,_A_6,_A_7));
    }
  #endif

  lambda_operator_unary(typename type_trait<T_type>::take a)
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

  template <class T_arg1=void,class T_arg2=void,class T_arg3=void,class T_arg4=void,class T_arg5=void,class T_arg6=void,class T_arg7=void>
  struct deduce_result_type
    { typedef typename arg_type::template deduce_result_type<typename type_trait<T_arg1>::pass,typename type_trait<T_arg2>::pass,typename type_trait<T_arg3>::pass,typename type_trait<T_arg4>::pass,typename type_trait<T_arg5>::pass,typename type_trait<T_arg6>::pass,typename type_trait<T_arg7>::pass>::type operand_type;
      typedef typename lambda_action_convert_deduce_result_type<T_action, T_type, operand_type>::type type;
    };
  typedef typename lambda_action_convert_deduce_result_type<
      T_action, T_type,
      typename arg_type::result_type
    >::type result_type;

  result_type
  operator ()() const;

  template <class T_arg1>
  typename deduce_result_type<T_arg1>::type
  operator ()(T_arg1 _A_1) const
    {
      return lambda_action_convert<T_action, T_type>::template do_action<
            typename deduce_result_type<typename type_trait<T_arg1>::pass>::operand_type>
        (arg_.SIGC_WORKAROUND_OPERATOR_PARENTHESES<typename type_trait<T_arg1>::pass>
            (_A_1));
    }

  #ifndef SIGC_TEMPLATE_SPECIALIZATION_OPERATOR_OVERLOAD
  template <class T_arg1>
  typename deduce_result_type<T_arg1>::type
  sun_forte_workaround(T_arg1 _A_1) const
    {
      return lambda_action_convert<T_action, T_type>::template do_action<
            typename deduce_result_type<typename type_trait<T_arg1>::pass>::operand_type>
        (arg_.SIGC_WORKAROUND_OPERATOR_PARENTHESES<typename type_trait<T_arg1>::pass>
            (_A_1));
    }
  #endif

  template <class T_arg1,class T_arg2>
  typename deduce_result_type<T_arg1,T_arg2>::type
  operator ()(T_arg1 _A_1,T_arg2 _A_2) const
    {
      return lambda_action_convert<T_action, T_type>::template do_action<
            typename deduce_result_type<typename type_trait<T_arg1>::pass,typename type_trait<T_arg2>::pass>::operand_type>
        (arg_.SIGC_WORKAROUND_OPERATOR_PARENTHESES<typename type_trait<T_arg1>::pass,typename type_trait<T_arg2>::pass>
            (_A_1,_A_2));
    }

  #ifndef SIGC_TEMPLATE_SPECIALIZATION_OPERATOR_OVERLOAD
  template <class T_arg1,class T_arg2>
  typename deduce_result_type<T_arg1,T_arg2>::type
  sun_forte_workaround(T_arg1 _A_1,T_arg2 _A_2) const
    {
      return lambda_action_convert<T_action, T_type>::template do_action<
            typename deduce_result_type<typename type_trait<T_arg1>::pass,typename type_trait<T_arg2>::pass>::operand_type>
        (arg_.SIGC_WORKAROUND_OPERATOR_PARENTHESES<typename type_trait<T_arg1>::pass,typename type_trait<T_arg2>::pass>
            (_A_1,_A_2));
    }
  #endif

  template <class T_arg1,class T_arg2,class T_arg3>
  typename deduce_result_type<T_arg1,T_arg2,T_arg3>::type
  operator ()(T_arg1 _A_1,T_arg2 _A_2,T_arg3 _A_3) const
    {
      return lambda_action_convert<T_action, T_type>::template do_action<
            typename deduce_result_type<typename type_trait<T_arg1>::pass,typename type_trait<T_arg2>::pass,typename type_trait<T_arg3>::pass>::operand_type>
        (arg_.SIGC_WORKAROUND_OPERATOR_PARENTHESES<typename type_trait<T_arg1>::pass,typename type_trait<T_arg2>::pass,typename type_trait<T_arg3>::pass>
            (_A_1,_A_2,_A_3));
    }

  #ifndef SIGC_TEMPLATE_SPECIALIZATION_OPERATOR_OVERLOAD
  template <class T_arg1,class T_arg2,class T_arg3>
  typename deduce_result_type<T_arg1,T_arg2,T_arg3>::type
  sun_forte_workaround(T_arg1 _A_1,T_arg2 _A_2,T_arg3 _A_3) const
    {
      return lambda_action_convert<T_action, T_type>::template do_action<
            typename deduce_result_type<typename type_trait<T_arg1>::pass,typename type_trait<T_arg2>::pass,typename type_trait<T_arg3>::pass>::operand_type>
        (arg_.SIGC_WORKAROUND_OPERATOR_PARENTHESES<typename type_trait<T_arg1>::pass,typename type_trait<T_arg2>::pass,typename type_trait<T_arg3>::pass>
            (_A_1,_A_2,_A_3));
    }
  #endif

  template <class T_arg1,class T_arg2,class T_arg3,class T_arg4>
  typename deduce_result_type<T_arg1,T_arg2,T_arg3,T_arg4>::type
  operator ()(T_arg1 _A_1,T_arg2 _A_2,T_arg3 _A_3,T_arg4 _A_4) const
    {
      return lambda_action_convert<T_action, T_type>::template do_action<
            typename deduce_result_type<typename type_trait<T_arg1>::pass,typename type_trait<T_arg2>::pass,typename type_trait<T_arg3>::pass,typename type_trait<T_arg4>::pass>::operand_type>
        (arg_.SIGC_WORKAROUND_OPERATOR_PARENTHESES<typename type_trait<T_arg1>::pass,typename type_trait<T_arg2>::pass,typename type_trait<T_arg3>::pass,typename type_trait<T_arg4>::pass>
            (_A_1,_A_2,_A_3,_A_4));
    }

  #ifndef SIGC_TEMPLATE_SPECIALIZATION_OPERATOR_OVERLOAD
  template <class T_arg1,class T_arg2,class T_arg3,class T_arg4>
  typename deduce_result_type<T_arg1,T_arg2,T_arg3,T_arg4>::type
  sun_forte_workaround(T_arg1 _A_1,T_arg2 _A_2,T_arg3 _A_3,T_arg4 _A_4) const
    {
      return lambda_action_convert<T_action, T_type>::template do_action<
            typename deduce_result_type<typename type_trait<T_arg1>::pass,typename type_trait<T_arg2>::pass,typename type_trait<T_arg3>::pass,typename type_trait<T_arg4>::pass>::operand_type>
        (arg_.SIGC_WORKAROUND_OPERATOR_PARENTHESES<typename type_trait<T_arg1>::pass,typename type_trait<T_arg2>::pass,typename type_trait<T_arg3>::pass,typename type_trait<T_arg4>::pass>
            (_A_1,_A_2,_A_3,_A_4));
    }
  #endif

  template <class T_arg1,class T_arg2,class T_arg3,class T_arg4,class T_arg5>
  typename deduce_result_type<T_arg1,T_arg2,T_arg3,T_arg4,T_arg5>::type
  operator ()(T_arg1 _A_1,T_arg2 _A_2,T_arg3 _A_3,T_arg4 _A_4,T_arg5 _A_5) const
    {
      return lambda_action_convert<T_action, T_type>::template do_action<
            typename deduce_result_type<typename type_trait<T_arg1>::pass,typename type_trait<T_arg2>::pass,typename type_trait<T_arg3>::pass,typename type_trait<T_arg4>::pass,typename type_trait<T_arg5>::pass>::operand_type>
        (arg_.SIGC_WORKAROUND_OPERATOR_PARENTHESES<typename type_trait<T_arg1>::pass,typename type_trait<T_arg2>::pass,typename type_trait<T_arg3>::pass,typename type_trait<T_arg4>::pass,typename type_trait<T_arg5>::pass>
            (_A_1,_A_2,_A_3,_A_4,_A_5));
    }

  #ifndef SIGC_TEMPLATE_SPECIALIZATION_OPERATOR_OVERLOAD
  template <class T_arg1,class T_arg2,class T_arg3,class T_arg4,class T_arg5>
  typename deduce_result_type<T_arg1,T_arg2,T_arg3,T_arg4,T_arg5>::type
  sun_forte_workaround(T_arg1 _A_1,T_arg2 _A_2,T_arg3 _A_3,T_arg4 _A_4,T_arg5 _A_5) const
    {
      return lambda_action_convert<T_action, T_type>::template do_action<
            typename deduce_result_type<typename type_trait<T_arg1>::pass,typename type_trait<T_arg2>::pass,typename type_trait<T_arg3>::pass,typename type_trait<T_arg4>::pass,typename type_trait<T_arg5>::pass>::operand_type>
        (arg_.SIGC_WORKAROUND_OPERATOR_PARENTHESES<typename type_trait<T_arg1>::pass,typename type_trait<T_arg2>::pass,typename type_trait<T_arg3>::pass,typename type_trait<T_arg4>::pass,typename type_trait<T_arg5>::pass>
            (_A_1,_A_2,_A_3,_A_4,_A_5));
    }
  #endif

  template <class T_arg1,class T_arg2,class T_arg3,class T_arg4,class T_arg5,class T_arg6>
  typename deduce_result_type<T_arg1,T_arg2,T_arg3,T_arg4,T_arg5,T_arg6>::type
  operator ()(T_arg1 _A_1,T_arg2 _A_2,T_arg3 _A_3,T_arg4 _A_4,T_arg5 _A_5,T_arg6 _A_6) const
    {
      return lambda_action_convert<T_action, T_type>::template do_action<
            typename deduce_result_type<typename type_trait<T_arg1>::pass,typename type_trait<T_arg2>::pass,typename type_trait<T_arg3>::pass,typename type_trait<T_arg4>::pass,typename type_trait<T_arg5>::pass,typename type_trait<T_arg6>::pass>::operand_type>
        (arg_.SIGC_WORKAROUND_OPERATOR_PARENTHESES<typename type_trait<T_arg1>::pass,typename type_trait<T_arg2>::pass,typename type_trait<T_arg3>::pass,typename type_trait<T_arg4>::pass,typename type_trait<T_arg5>::pass,typename type_trait<T_arg6>::pass>
            (_A_1,_A_2,_A_3,_A_4,_A_5,_A_6));
    }

  #ifndef SIGC_TEMPLATE_SPECIALIZATION_OPERATOR_OVERLOAD
  template <class T_arg1,class T_arg2,class T_arg3,class T_arg4,class T_arg5,class T_arg6>
  typename deduce_result_type<T_arg1,T_arg2,T_arg3,T_arg4,T_arg5,T_arg6>::type
  sun_forte_workaround(T_arg1 _A_1,T_arg2 _A_2,T_arg3 _A_3,T_arg4 _A_4,T_arg5 _A_5,T_arg6 _A_6) const
    {
      return lambda_action_convert<T_action, T_type>::template do_action<
            typename deduce_result_type<typename type_trait<T_arg1>::pass,typename type_trait<T_arg2>::pass,typename type_trait<T_arg3>::pass,typename type_trait<T_arg4>::pass,typename type_trait<T_arg5>::pass,typename type_trait<T_arg6>::pass>::operand_type>
        (arg_.SIGC_WORKAROUND_OPERATOR_PARENTHESES<typename type_trait<T_arg1>::pass,typename type_trait<T_arg2>::pass,typename type_trait<T_arg3>::pass,typename type_trait<T_arg4>::pass,typename type_trait<T_arg5>::pass,typename type_trait<T_arg6>::pass>
            (_A_1,_A_2,_A_3,_A_4,_A_5,_A_6));
    }
  #endif

  template <class T_arg1,class T_arg2,class T_arg3,class T_arg4,class T_arg5,class T_arg6,class T_arg7>
  typename deduce_result_type<T_arg1,T_arg2,T_arg3,T_arg4,T_arg5,T_arg6,T_arg7>::type
  operator ()(T_arg1 _A_1,T_arg2 _A_2,T_arg3 _A_3,T_arg4 _A_4,T_arg5 _A_5,T_arg6 _A_6,T_arg7 _A_7) const
    {
      return lambda_action_convert<T_action, T_type>::template do_action<
            typename deduce_result_type<typename type_trait<T_arg1>::pass,typename type_trait<T_arg2>::pass,typename type_trait<T_arg3>::pass,typename type_trait<T_arg4>::pass,typename type_trait<T_arg5>::pass,typename type_trait<T_arg6>::pass,typename type_trait<T_arg7>::pass>::operand_type>
        (arg_.SIGC_WORKAROUND_OPERATOR_PARENTHESES<typename type_trait<T_arg1>::pass,typename type_trait<T_arg2>::pass,typename type_trait<T_arg3>::pass,typename type_trait<T_arg4>::pass,typename type_trait<T_arg5>::pass,typename type_trait<T_arg6>::pass,typename type_trait<T_arg7>::pass>
            (_A_1,_A_2,_A_3,_A_4,_A_5,_A_6,_A_7));
    }

  #ifndef SIGC_TEMPLATE_SPECIALIZATION_OPERATOR_OVERLOAD
  template <class T_arg1,class T_arg2,class T_arg3,class T_arg4,class T_arg5,class T_arg6,class T_arg7>
  typename deduce_result_type<T_arg1,T_arg2,T_arg3,T_arg4,T_arg5,T_arg6,T_arg7>::type
  sun_forte_workaround(T_arg1 _A_1,T_arg2 _A_2,T_arg3 _A_3,T_arg4 _A_4,T_arg5 _A_5,T_arg6 _A_6,T_arg7 _A_7) const
    {
      return lambda_action_convert<T_action, T_type>::template do_action<
            typename deduce_result_type<typename type_trait<T_arg1>::pass,typename type_trait<T_arg2>::pass,typename type_trait<T_arg3>::pass,typename type_trait<T_arg4>::pass,typename type_trait<T_arg5>::pass,typename type_trait<T_arg6>::pass,typename type_trait<T_arg7>::pass>::operand_type>
        (arg_.SIGC_WORKAROUND_OPERATOR_PARENTHESES<typename type_trait<T_arg1>::pass,typename type_trait<T_arg2>::pass,typename type_trait<T_arg3>::pass,typename type_trait<T_arg4>::pass,typename type_trait<T_arg5>::pass,typename type_trait<T_arg6>::pass,typename type_trait<T_arg7>::pass>
            (_A_1,_A_2,_A_3,_A_4,_A_5,_A_6,_A_7));
    }
  #endif

  lambda_operator_convert(typename type_trait<T_arg>::take a)
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


// Operators for lambda action arithmetic<plus>. At least one of the arguments needs to be of type lamdba, hence the overloads.
template <class T_arg1, class T_arg2>
lambda<lambda_operator<arithmetic<plus>, T_arg1, T_arg2> >
operator + (const lambda<T_arg1>& a1, const lambda<T_arg2>& a2)
{ typedef lambda_operator<arithmetic<plus>, T_arg1, T_arg2> operator_type;
  return lambda<operator_type>(operator_type(a1.value_,a2.value_)); }
template <class T_arg1, class T_arg2>
lambda<lambda_operator<arithmetic<plus>, T_arg1, typename unwrap_reference<T_arg2>::type> >
operator + (const lambda<T_arg1>& a1, const T_arg2& a2)
{ typedef lambda_operator<arithmetic<plus>, T_arg1, typename unwrap_reference<T_arg2>::type> operator_type;
  return lambda<operator_type>(operator_type(a1.value_,a2)); }
template <class T_arg1, class T_arg2>
lambda<lambda_operator<arithmetic<plus>, typename unwrap_reference<T_arg1>::type, T_arg2> >
operator + (const T_arg1& a1, const lambda<T_arg2>& a2)
{ typedef lambda_operator<arithmetic<plus>, typename unwrap_reference<T_arg1>::type, T_arg2> operator_type;
  return lambda<operator_type>(operator_type(a1,a2.value_)); }

// Operators for lambda action arithmetic<minus>. At least one of the arguments needs to be of type lamdba, hence the overloads.
template <class T_arg1, class T_arg2>
lambda<lambda_operator<arithmetic<minus>, T_arg1, T_arg2> >
operator - (const lambda<T_arg1>& a1, const lambda<T_arg2>& a2)
{ typedef lambda_operator<arithmetic<minus>, T_arg1, T_arg2> operator_type;
  return lambda<operator_type>(operator_type(a1.value_,a2.value_)); }
template <class T_arg1, class T_arg2>
lambda<lambda_operator<arithmetic<minus>, T_arg1, typename unwrap_reference<T_arg2>::type> >
operator - (const lambda<T_arg1>& a1, const T_arg2& a2)
{ typedef lambda_operator<arithmetic<minus>, T_arg1, typename unwrap_reference<T_arg2>::type> operator_type;
  return lambda<operator_type>(operator_type(a1.value_,a2)); }
template <class T_arg1, class T_arg2>
lambda<lambda_operator<arithmetic<minus>, typename unwrap_reference<T_arg1>::type, T_arg2> >
operator - (const T_arg1& a1, const lambda<T_arg2>& a2)
{ typedef lambda_operator<arithmetic<minus>, typename unwrap_reference<T_arg1>::type, T_arg2> operator_type;
  return lambda<operator_type>(operator_type(a1,a2.value_)); }

// Operators for lambda action arithmetic<multiplies>. At least one of the arguments needs to be of type lamdba, hence the overloads.
template <class T_arg1, class T_arg2>
lambda<lambda_operator<arithmetic<multiplies>, T_arg1, T_arg2> >
operator * (const lambda<T_arg1>& a1, const lambda<T_arg2>& a2)
{ typedef lambda_operator<arithmetic<multiplies>, T_arg1, T_arg2> operator_type;
  return lambda<operator_type>(operator_type(a1.value_,a2.value_)); }
template <class T_arg1, class T_arg2>
lambda<lambda_operator<arithmetic<multiplies>, T_arg1, typename unwrap_reference<T_arg2>::type> >
operator * (const lambda<T_arg1>& a1, const T_arg2& a2)
{ typedef lambda_operator<arithmetic<multiplies>, T_arg1, typename unwrap_reference<T_arg2>::type> operator_type;
  return lambda<operator_type>(operator_type(a1.value_,a2)); }
template <class T_arg1, class T_arg2>
lambda<lambda_operator<arithmetic<multiplies>, typename unwrap_reference<T_arg1>::type, T_arg2> >
operator * (const T_arg1& a1, const lambda<T_arg2>& a2)
{ typedef lambda_operator<arithmetic<multiplies>, typename unwrap_reference<T_arg1>::type, T_arg2> operator_type;
  return lambda<operator_type>(operator_type(a1,a2.value_)); }

// Operators for lambda action arithmetic<divides>. At least one of the arguments needs to be of type lamdba, hence the overloads.
template <class T_arg1, class T_arg2>
lambda<lambda_operator<arithmetic<divides>, T_arg1, T_arg2> >
operator / (const lambda<T_arg1>& a1, const lambda<T_arg2>& a2)
{ typedef lambda_operator<arithmetic<divides>, T_arg1, T_arg2> operator_type;
  return lambda<operator_type>(operator_type(a1.value_,a2.value_)); }
template <class T_arg1, class T_arg2>
lambda<lambda_operator<arithmetic<divides>, T_arg1, typename unwrap_reference<T_arg2>::type> >
operator / (const lambda<T_arg1>& a1, const T_arg2& a2)
{ typedef lambda_operator<arithmetic<divides>, T_arg1, typename unwrap_reference<T_arg2>::type> operator_type;
  return lambda<operator_type>(operator_type(a1.value_,a2)); }
template <class T_arg1, class T_arg2>
lambda<lambda_operator<arithmetic<divides>, typename unwrap_reference<T_arg1>::type, T_arg2> >
operator / (const T_arg1& a1, const lambda<T_arg2>& a2)
{ typedef lambda_operator<arithmetic<divides>, typename unwrap_reference<T_arg1>::type, T_arg2> operator_type;
  return lambda<operator_type>(operator_type(a1,a2.value_)); }

// Operators for lambda action arithmetic<modulus>. At least one of the arguments needs to be of type lamdba, hence the overloads.
template <class T_arg1, class T_arg2>
lambda<lambda_operator<arithmetic<modulus>, T_arg1, T_arg2> >
operator % (const lambda<T_arg1>& a1, const lambda<T_arg2>& a2)
{ typedef lambda_operator<arithmetic<modulus>, T_arg1, T_arg2> operator_type;
  return lambda<operator_type>(operator_type(a1.value_,a2.value_)); }
template <class T_arg1, class T_arg2>
lambda<lambda_operator<arithmetic<modulus>, T_arg1, typename unwrap_reference<T_arg2>::type> >
operator % (const lambda<T_arg1>& a1, const T_arg2& a2)
{ typedef lambda_operator<arithmetic<modulus>, T_arg1, typename unwrap_reference<T_arg2>::type> operator_type;
  return lambda<operator_type>(operator_type(a1.value_,a2)); }
template <class T_arg1, class T_arg2>
lambda<lambda_operator<arithmetic<modulus>, typename unwrap_reference<T_arg1>::type, T_arg2> >
operator % (const T_arg1& a1, const lambda<T_arg2>& a2)
{ typedef lambda_operator<arithmetic<modulus>, typename unwrap_reference<T_arg1>::type, T_arg2> operator_type;
  return lambda<operator_type>(operator_type(a1,a2.value_)); }

// Operators for lambda action bitwise<leftshift>. At least one of the arguments needs to be of type lamdba, hence the overloads.
template <class T_arg1, class T_arg2>
lambda<lambda_operator<bitwise<leftshift>, T_arg1, T_arg2> >
operator << (const lambda<T_arg1>& a1, const lambda<T_arg2>& a2)
{ typedef lambda_operator<bitwise<leftshift>, T_arg1, T_arg2> operator_type;
  return lambda<operator_type>(operator_type(a1.value_,a2.value_)); }
template <class T_arg1, class T_arg2>
lambda<lambda_operator<bitwise<leftshift>, T_arg1, typename unwrap_reference<T_arg2>::type> >
operator << (const lambda<T_arg1>& a1, const T_arg2& a2)
{ typedef lambda_operator<bitwise<leftshift>, T_arg1, typename unwrap_reference<T_arg2>::type> operator_type;
  return lambda<operator_type>(operator_type(a1.value_,a2)); }
template <class T_arg1, class T_arg2>
lambda<lambda_operator<bitwise<leftshift>, typename unwrap_reference<T_arg1>::type, T_arg2> >
operator << (const T_arg1& a1, const lambda<T_arg2>& a2)
{ typedef lambda_operator<bitwise<leftshift>, typename unwrap_reference<T_arg1>::type, T_arg2> operator_type;
  return lambda<operator_type>(operator_type(a1,a2.value_)); }

// Operators for lambda action bitwise<rightshift>. At least one of the arguments needs to be of type lamdba, hence the overloads.
template <class T_arg1, class T_arg2>
lambda<lambda_operator<bitwise<rightshift>, T_arg1, T_arg2> >
operator >> (const lambda<T_arg1>& a1, const lambda<T_arg2>& a2)
{ typedef lambda_operator<bitwise<rightshift>, T_arg1, T_arg2> operator_type;
  return lambda<operator_type>(operator_type(a1.value_,a2.value_)); }
template <class T_arg1, class T_arg2>
lambda<lambda_operator<bitwise<rightshift>, T_arg1, typename unwrap_reference<T_arg2>::type> >
operator >> (const lambda<T_arg1>& a1, const T_arg2& a2)
{ typedef lambda_operator<bitwise<rightshift>, T_arg1, typename unwrap_reference<T_arg2>::type> operator_type;
  return lambda<operator_type>(operator_type(a1.value_,a2)); }
template <class T_arg1, class T_arg2>
lambda<lambda_operator<bitwise<rightshift>, typename unwrap_reference<T_arg1>::type, T_arg2> >
operator >> (const T_arg1& a1, const lambda<T_arg2>& a2)
{ typedef lambda_operator<bitwise<rightshift>, typename unwrap_reference<T_arg1>::type, T_arg2> operator_type;
  return lambda<operator_type>(operator_type(a1,a2.value_)); }

// Operators for lambda action bitwise<and_>. At least one of the arguments needs to be of type lamdba, hence the overloads.
template <class T_arg1, class T_arg2>
lambda<lambda_operator<bitwise<and_>, T_arg1, T_arg2> >
operator & (const lambda<T_arg1>& a1, const lambda<T_arg2>& a2)
{ typedef lambda_operator<bitwise<and_>, T_arg1, T_arg2> operator_type;
  return lambda<operator_type>(operator_type(a1.value_,a2.value_)); }
template <class T_arg1, class T_arg2>
lambda<lambda_operator<bitwise<and_>, T_arg1, typename unwrap_reference<T_arg2>::type> >
operator & (const lambda<T_arg1>& a1, const T_arg2& a2)
{ typedef lambda_operator<bitwise<and_>, T_arg1, typename unwrap_reference<T_arg2>::type> operator_type;
  return lambda<operator_type>(operator_type(a1.value_,a2)); }
template <class T_arg1, class T_arg2>
lambda<lambda_operator<bitwise<and_>, typename unwrap_reference<T_arg1>::type, T_arg2> >
operator & (const T_arg1& a1, const lambda<T_arg2>& a2)
{ typedef lambda_operator<bitwise<and_>, typename unwrap_reference<T_arg1>::type, T_arg2> operator_type;
  return lambda<operator_type>(operator_type(a1,a2.value_)); }

// Operators for lambda action bitwise<or_>. At least one of the arguments needs to be of type lamdba, hence the overloads.
template <class T_arg1, class T_arg2>
lambda<lambda_operator<bitwise<or_>, T_arg1, T_arg2> >
operator | (const lambda<T_arg1>& a1, const lambda<T_arg2>& a2)
{ typedef lambda_operator<bitwise<or_>, T_arg1, T_arg2> operator_type;
  return lambda<operator_type>(operator_type(a1.value_,a2.value_)); }
template <class T_arg1, class T_arg2>
lambda<lambda_operator<bitwise<or_>, T_arg1, typename unwrap_reference<T_arg2>::type> >
operator | (const lambda<T_arg1>& a1, const T_arg2& a2)
{ typedef lambda_operator<bitwise<or_>, T_arg1, typename unwrap_reference<T_arg2>::type> operator_type;
  return lambda<operator_type>(operator_type(a1.value_,a2)); }
template <class T_arg1, class T_arg2>
lambda<lambda_operator<bitwise<or_>, typename unwrap_reference<T_arg1>::type, T_arg2> >
operator | (const T_arg1& a1, const lambda<T_arg2>& a2)
{ typedef lambda_operator<bitwise<or_>, typename unwrap_reference<T_arg1>::type, T_arg2> operator_type;
  return lambda<operator_type>(operator_type(a1,a2.value_)); }

// Operators for lambda action bitwise<xor_>. At least one of the arguments needs to be of type lamdba, hence the overloads.
template <class T_arg1, class T_arg2>
lambda<lambda_operator<bitwise<xor_>, T_arg1, T_arg2> >
operator ^ (const lambda<T_arg1>& a1, const lambda<T_arg2>& a2)
{ typedef lambda_operator<bitwise<xor_>, T_arg1, T_arg2> operator_type;
  return lambda<operator_type>(operator_type(a1.value_,a2.value_)); }
template <class T_arg1, class T_arg2>
lambda<lambda_operator<bitwise<xor_>, T_arg1, typename unwrap_reference<T_arg2>::type> >
operator ^ (const lambda<T_arg1>& a1, const T_arg2& a2)
{ typedef lambda_operator<bitwise<xor_>, T_arg1, typename unwrap_reference<T_arg2>::type> operator_type;
  return lambda<operator_type>(operator_type(a1.value_,a2)); }
template <class T_arg1, class T_arg2>
lambda<lambda_operator<bitwise<xor_>, typename unwrap_reference<T_arg1>::type, T_arg2> >
operator ^ (const T_arg1& a1, const lambda<T_arg2>& a2)
{ typedef lambda_operator<bitwise<xor_>, typename unwrap_reference<T_arg1>::type, T_arg2> operator_type;
  return lambda<operator_type>(operator_type(a1,a2.value_)); }

// Operators for lambda action logical<and_>. At least one of the arguments needs to be of type lamdba, hence the overloads.
template <class T_arg1, class T_arg2>
lambda<lambda_operator<logical<and_>, T_arg1, T_arg2> >
operator && (const lambda<T_arg1>& a1, const lambda<T_arg2>& a2)
{ typedef lambda_operator<logical<and_>, T_arg1, T_arg2> operator_type;
  return lambda<operator_type>(operator_type(a1.value_,a2.value_)); }
template <class T_arg1, class T_arg2>
lambda<lambda_operator<logical<and_>, T_arg1, typename unwrap_reference<T_arg2>::type> >
operator && (const lambda<T_arg1>& a1, const T_arg2& a2)
{ typedef lambda_operator<logical<and_>, T_arg1, typename unwrap_reference<T_arg2>::type> operator_type;
  return lambda<operator_type>(operator_type(a1.value_,a2)); }
template <class T_arg1, class T_arg2>
lambda<lambda_operator<logical<and_>, typename unwrap_reference<T_arg1>::type, T_arg2> >
operator && (const T_arg1& a1, const lambda<T_arg2>& a2)
{ typedef lambda_operator<logical<and_>, typename unwrap_reference<T_arg1>::type, T_arg2> operator_type;
  return lambda<operator_type>(operator_type(a1,a2.value_)); }

// Operators for lambda action logical<or_>. At least one of the arguments needs to be of type lamdba, hence the overloads.
template <class T_arg1, class T_arg2>
lambda<lambda_operator<logical<or_>, T_arg1, T_arg2> >
operator || (const lambda<T_arg1>& a1, const lambda<T_arg2>& a2)
{ typedef lambda_operator<logical<or_>, T_arg1, T_arg2> operator_type;
  return lambda<operator_type>(operator_type(a1.value_,a2.value_)); }
template <class T_arg1, class T_arg2>
lambda<lambda_operator<logical<or_>, T_arg1, typename unwrap_reference<T_arg2>::type> >
operator || (const lambda<T_arg1>& a1, const T_arg2& a2)
{ typedef lambda_operator<logical<or_>, T_arg1, typename unwrap_reference<T_arg2>::type> operator_type;
  return lambda<operator_type>(operator_type(a1.value_,a2)); }
template <class T_arg1, class T_arg2>
lambda<lambda_operator<logical<or_>, typename unwrap_reference<T_arg1>::type, T_arg2> >
operator || (const T_arg1& a1, const lambda<T_arg2>& a2)
{ typedef lambda_operator<logical<or_>, typename unwrap_reference<T_arg1>::type, T_arg2> operator_type;
  return lambda<operator_type>(operator_type(a1,a2.value_)); }

// Operators for lambda action relational<less>. At least one of the arguments needs to be of type lamdba, hence the overloads.
template <class T_arg1, class T_arg2>
lambda<lambda_operator<relational<less>, T_arg1, T_arg2> >
operator < (const lambda<T_arg1>& a1, const lambda<T_arg2>& a2)
{ typedef lambda_operator<relational<less>, T_arg1, T_arg2> operator_type;
  return lambda<operator_type>(operator_type(a1.value_,a2.value_)); }
template <class T_arg1, class T_arg2>
lambda<lambda_operator<relational<less>, T_arg1, typename unwrap_reference<T_arg2>::type> >
operator < (const lambda<T_arg1>& a1, const T_arg2& a2)
{ typedef lambda_operator<relational<less>, T_arg1, typename unwrap_reference<T_arg2>::type> operator_type;
  return lambda<operator_type>(operator_type(a1.value_,a2)); }
template <class T_arg1, class T_arg2>
lambda<lambda_operator<relational<less>, typename unwrap_reference<T_arg1>::type, T_arg2> >
operator < (const T_arg1& a1, const lambda<T_arg2>& a2)
{ typedef lambda_operator<relational<less>, typename unwrap_reference<T_arg1>::type, T_arg2> operator_type;
  return lambda<operator_type>(operator_type(a1,a2.value_)); }

// Operators for lambda action relational<greater>. At least one of the arguments needs to be of type lamdba, hence the overloads.
template <class T_arg1, class T_arg2>
lambda<lambda_operator<relational<greater>, T_arg1, T_arg2> >
operator > (const lambda<T_arg1>& a1, const lambda<T_arg2>& a2)
{ typedef lambda_operator<relational<greater>, T_arg1, T_arg2> operator_type;
  return lambda<operator_type>(operator_type(a1.value_,a2.value_)); }
template <class T_arg1, class T_arg2>
lambda<lambda_operator<relational<greater>, T_arg1, typename unwrap_reference<T_arg2>::type> >
operator > (const lambda<T_arg1>& a1, const T_arg2& a2)
{ typedef lambda_operator<relational<greater>, T_arg1, typename unwrap_reference<T_arg2>::type> operator_type;
  return lambda<operator_type>(operator_type(a1.value_,a2)); }
template <class T_arg1, class T_arg2>
lambda<lambda_operator<relational<greater>, typename unwrap_reference<T_arg1>::type, T_arg2> >
operator > (const T_arg1& a1, const lambda<T_arg2>& a2)
{ typedef lambda_operator<relational<greater>, typename unwrap_reference<T_arg1>::type, T_arg2> operator_type;
  return lambda<operator_type>(operator_type(a1,a2.value_)); }

// Operators for lambda action relational<less_equal>. At least one of the arguments needs to be of type lamdba, hence the overloads.
template <class T_arg1, class T_arg2>
lambda<lambda_operator<relational<less_equal>, T_arg1, T_arg2> >
operator <= (const lambda<T_arg1>& a1, const lambda<T_arg2>& a2)
{ typedef lambda_operator<relational<less_equal>, T_arg1, T_arg2> operator_type;
  return lambda<operator_type>(operator_type(a1.value_,a2.value_)); }
template <class T_arg1, class T_arg2>
lambda<lambda_operator<relational<less_equal>, T_arg1, typename unwrap_reference<T_arg2>::type> >
operator <= (const lambda<T_arg1>& a1, const T_arg2& a2)
{ typedef lambda_operator<relational<less_equal>, T_arg1, typename unwrap_reference<T_arg2>::type> operator_type;
  return lambda<operator_type>(operator_type(a1.value_,a2)); }
template <class T_arg1, class T_arg2>
lambda<lambda_operator<relational<less_equal>, typename unwrap_reference<T_arg1>::type, T_arg2> >
operator <= (const T_arg1& a1, const lambda<T_arg2>& a2)
{ typedef lambda_operator<relational<less_equal>, typename unwrap_reference<T_arg1>::type, T_arg2> operator_type;
  return lambda<operator_type>(operator_type(a1,a2.value_)); }

// Operators for lambda action relational<greater_equal>. At least one of the arguments needs to be of type lamdba, hence the overloads.
template <class T_arg1, class T_arg2>
lambda<lambda_operator<relational<greater_equal>, T_arg1, T_arg2> >
operator >= (const lambda<T_arg1>& a1, const lambda<T_arg2>& a2)
{ typedef lambda_operator<relational<greater_equal>, T_arg1, T_arg2> operator_type;
  return lambda<operator_type>(operator_type(a1.value_,a2.value_)); }
template <class T_arg1, class T_arg2>
lambda<lambda_operator<relational<greater_equal>, T_arg1, typename unwrap_reference<T_arg2>::type> >
operator >= (const lambda<T_arg1>& a1, const T_arg2& a2)
{ typedef lambda_operator<relational<greater_equal>, T_arg1, typename unwrap_reference<T_arg2>::type> operator_type;
  return lambda<operator_type>(operator_type(a1.value_,a2)); }
template <class T_arg1, class T_arg2>
lambda<lambda_operator<relational<greater_equal>, typename unwrap_reference<T_arg1>::type, T_arg2> >
operator >= (const T_arg1& a1, const lambda<T_arg2>& a2)
{ typedef lambda_operator<relational<greater_equal>, typename unwrap_reference<T_arg1>::type, T_arg2> operator_type;
  return lambda<operator_type>(operator_type(a1,a2.value_)); }

// Operators for lambda action relational<equal_to>. At least one of the arguments needs to be of type lamdba, hence the overloads.
template <class T_arg1, class T_arg2>
lambda<lambda_operator<relational<equal_to>, T_arg1, T_arg2> >
operator == (const lambda<T_arg1>& a1, const lambda<T_arg2>& a2)
{ typedef lambda_operator<relational<equal_to>, T_arg1, T_arg2> operator_type;
  return lambda<operator_type>(operator_type(a1.value_,a2.value_)); }
template <class T_arg1, class T_arg2>
lambda<lambda_operator<relational<equal_to>, T_arg1, typename unwrap_reference<T_arg2>::type> >
operator == (const lambda<T_arg1>& a1, const T_arg2& a2)
{ typedef lambda_operator<relational<equal_to>, T_arg1, typename unwrap_reference<T_arg2>::type> operator_type;
  return lambda<operator_type>(operator_type(a1.value_,a2)); }
template <class T_arg1, class T_arg2>
lambda<lambda_operator<relational<equal_to>, typename unwrap_reference<T_arg1>::type, T_arg2> >
operator == (const T_arg1& a1, const lambda<T_arg2>& a2)
{ typedef lambda_operator<relational<equal_to>, typename unwrap_reference<T_arg1>::type, T_arg2> operator_type;
  return lambda<operator_type>(operator_type(a1,a2.value_)); }

// Operators for lambda action relational<not_equal_to>. At least one of the arguments needs to be of type lamdba, hence the overloads.
template <class T_arg1, class T_arg2>
lambda<lambda_operator<relational<not_equal_to>, T_arg1, T_arg2> >
operator != (const lambda<T_arg1>& a1, const lambda<T_arg2>& a2)
{ typedef lambda_operator<relational<not_equal_to>, T_arg1, T_arg2> operator_type;
  return lambda<operator_type>(operator_type(a1.value_,a2.value_)); }
template <class T_arg1, class T_arg2>
lambda<lambda_operator<relational<not_equal_to>, T_arg1, typename unwrap_reference<T_arg2>::type> >
operator != (const lambda<T_arg1>& a1, const T_arg2& a2)
{ typedef lambda_operator<relational<not_equal_to>, T_arg1, typename unwrap_reference<T_arg2>::type> operator_type;
  return lambda<operator_type>(operator_type(a1.value_,a2)); }
template <class T_arg1, class T_arg2>
lambda<lambda_operator<relational<not_equal_to>, typename unwrap_reference<T_arg1>::type, T_arg2> >
operator != (const T_arg1& a1, const lambda<T_arg2>& a2)
{ typedef lambda_operator<relational<not_equal_to>, typename unwrap_reference<T_arg1>::type, T_arg2> operator_type;
  return lambda<operator_type>(operator_type(a1,a2.value_)); }

// Operators for lambda action arithmetic_assign<plus>. At least one of the arguments needs to be of type lamdba, hence the overloads.
template <class T_arg1, class T_arg2>
lambda<lambda_operator<arithmetic_assign<plus>, T_arg1, T_arg2> >
operator += (const lambda<T_arg1>& a1, const lambda<T_arg2>& a2)
{ typedef lambda_operator<arithmetic_assign<plus>, T_arg1, T_arg2> operator_type;
  return lambda<operator_type>(operator_type(a1.value_,a2.value_)); }
template <class T_arg1, class T_arg2>
lambda<lambda_operator<arithmetic_assign<plus>, T_arg1, typename unwrap_reference<T_arg2>::type> >
operator += (const lambda<T_arg1>& a1, const T_arg2& a2)
{ typedef lambda_operator<arithmetic_assign<plus>, T_arg1, typename unwrap_reference<T_arg2>::type> operator_type;
  return lambda<operator_type>(operator_type(a1.value_,a2)); }
template <class T_arg1, class T_arg2>
lambda<lambda_operator<arithmetic_assign<plus>, typename unwrap_reference<T_arg1>::type, T_arg2> >
operator += (const T_arg1& a1, const lambda<T_arg2>& a2)
{ typedef lambda_operator<arithmetic_assign<plus>, typename unwrap_reference<T_arg1>::type, T_arg2> operator_type;
  return lambda<operator_type>(operator_type(a1,a2.value_)); }

// Operators for lambda action arithmetic_assign<minus>. At least one of the arguments needs to be of type lamdba, hence the overloads.
template <class T_arg1, class T_arg2>
lambda<lambda_operator<arithmetic_assign<minus>, T_arg1, T_arg2> >
operator -= (const lambda<T_arg1>& a1, const lambda<T_arg2>& a2)
{ typedef lambda_operator<arithmetic_assign<minus>, T_arg1, T_arg2> operator_type;
  return lambda<operator_type>(operator_type(a1.value_,a2.value_)); }
template <class T_arg1, class T_arg2>
lambda<lambda_operator<arithmetic_assign<minus>, T_arg1, typename unwrap_reference<T_arg2>::type> >
operator -= (const lambda<T_arg1>& a1, const T_arg2& a2)
{ typedef lambda_operator<arithmetic_assign<minus>, T_arg1, typename unwrap_reference<T_arg2>::type> operator_type;
  return lambda<operator_type>(operator_type(a1.value_,a2)); }
template <class T_arg1, class T_arg2>
lambda<lambda_operator<arithmetic_assign<minus>, typename unwrap_reference<T_arg1>::type, T_arg2> >
operator -= (const T_arg1& a1, const lambda<T_arg2>& a2)
{ typedef lambda_operator<arithmetic_assign<minus>, typename unwrap_reference<T_arg1>::type, T_arg2> operator_type;
  return lambda<operator_type>(operator_type(a1,a2.value_)); }

// Operators for lambda action arithmetic_assign<multiplies>. At least one of the arguments needs to be of type lamdba, hence the overloads.
template <class T_arg1, class T_arg2>
lambda<lambda_operator<arithmetic_assign<multiplies>, T_arg1, T_arg2> >
operator *= (const lambda<T_arg1>& a1, const lambda<T_arg2>& a2)
{ typedef lambda_operator<arithmetic_assign<multiplies>, T_arg1, T_arg2> operator_type;
  return lambda<operator_type>(operator_type(a1.value_,a2.value_)); }
template <class T_arg1, class T_arg2>
lambda<lambda_operator<arithmetic_assign<multiplies>, T_arg1, typename unwrap_reference<T_arg2>::type> >
operator *= (const lambda<T_arg1>& a1, const T_arg2& a2)
{ typedef lambda_operator<arithmetic_assign<multiplies>, T_arg1, typename unwrap_reference<T_arg2>::type> operator_type;
  return lambda<operator_type>(operator_type(a1.value_,a2)); }
template <class T_arg1, class T_arg2>
lambda<lambda_operator<arithmetic_assign<multiplies>, typename unwrap_reference<T_arg1>::type, T_arg2> >
operator *= (const T_arg1& a1, const lambda<T_arg2>& a2)
{ typedef lambda_operator<arithmetic_assign<multiplies>, typename unwrap_reference<T_arg1>::type, T_arg2> operator_type;
  return lambda<operator_type>(operator_type(a1,a2.value_)); }

// Operators for lambda action arithmetic_assign<divides>. At least one of the arguments needs to be of type lamdba, hence the overloads.
template <class T_arg1, class T_arg2>
lambda<lambda_operator<arithmetic_assign<divides>, T_arg1, T_arg2> >
operator /= (const lambda<T_arg1>& a1, const lambda<T_arg2>& a2)
{ typedef lambda_operator<arithmetic_assign<divides>, T_arg1, T_arg2> operator_type;
  return lambda<operator_type>(operator_type(a1.value_,a2.value_)); }
template <class T_arg1, class T_arg2>
lambda<lambda_operator<arithmetic_assign<divides>, T_arg1, typename unwrap_reference<T_arg2>::type> >
operator /= (const lambda<T_arg1>& a1, const T_arg2& a2)
{ typedef lambda_operator<arithmetic_assign<divides>, T_arg1, typename unwrap_reference<T_arg2>::type> operator_type;
  return lambda<operator_type>(operator_type(a1.value_,a2)); }
template <class T_arg1, class T_arg2>
lambda<lambda_operator<arithmetic_assign<divides>, typename unwrap_reference<T_arg1>::type, T_arg2> >
operator /= (const T_arg1& a1, const lambda<T_arg2>& a2)
{ typedef lambda_operator<arithmetic_assign<divides>, typename unwrap_reference<T_arg1>::type, T_arg2> operator_type;
  return lambda<operator_type>(operator_type(a1,a2.value_)); }

// Operators for lambda action arithmetic_assign<modulus>. At least one of the arguments needs to be of type lamdba, hence the overloads.
template <class T_arg1, class T_arg2>
lambda<lambda_operator<arithmetic_assign<modulus>, T_arg1, T_arg2> >
operator %= (const lambda<T_arg1>& a1, const lambda<T_arg2>& a2)
{ typedef lambda_operator<arithmetic_assign<modulus>, T_arg1, T_arg2> operator_type;
  return lambda<operator_type>(operator_type(a1.value_,a2.value_)); }
template <class T_arg1, class T_arg2>
lambda<lambda_operator<arithmetic_assign<modulus>, T_arg1, typename unwrap_reference<T_arg2>::type> >
operator %= (const lambda<T_arg1>& a1, const T_arg2& a2)
{ typedef lambda_operator<arithmetic_assign<modulus>, T_arg1, typename unwrap_reference<T_arg2>::type> operator_type;
  return lambda<operator_type>(operator_type(a1.value_,a2)); }
template <class T_arg1, class T_arg2>
lambda<lambda_operator<arithmetic_assign<modulus>, typename unwrap_reference<T_arg1>::type, T_arg2> >
operator %= (const T_arg1& a1, const lambda<T_arg2>& a2)
{ typedef lambda_operator<arithmetic_assign<modulus>, typename unwrap_reference<T_arg1>::type, T_arg2> operator_type;
  return lambda<operator_type>(operator_type(a1,a2.value_)); }

// Operators for lambda action bitwise_assign<leftshift>. At least one of the arguments needs to be of type lamdba, hence the overloads.
template <class T_arg1, class T_arg2>
lambda<lambda_operator<bitwise_assign<leftshift>, T_arg1, T_arg2> >
operator <<= (const lambda<T_arg1>& a1, const lambda<T_arg2>& a2)
{ typedef lambda_operator<bitwise_assign<leftshift>, T_arg1, T_arg2> operator_type;
  return lambda<operator_type>(operator_type(a1.value_,a2.value_)); }
template <class T_arg1, class T_arg2>
lambda<lambda_operator<bitwise_assign<leftshift>, T_arg1, typename unwrap_reference<T_arg2>::type> >
operator <<= (const lambda<T_arg1>& a1, const T_arg2& a2)
{ typedef lambda_operator<bitwise_assign<leftshift>, T_arg1, typename unwrap_reference<T_arg2>::type> operator_type;
  return lambda<operator_type>(operator_type(a1.value_,a2)); }
template <class T_arg1, class T_arg2>
lambda<lambda_operator<bitwise_assign<leftshift>, typename unwrap_reference<T_arg1>::type, T_arg2> >
operator <<= (const T_arg1& a1, const lambda<T_arg2>& a2)
{ typedef lambda_operator<bitwise_assign<leftshift>, typename unwrap_reference<T_arg1>::type, T_arg2> operator_type;
  return lambda<operator_type>(operator_type(a1,a2.value_)); }

// Operators for lambda action bitwise_assign<rightshift>. At least one of the arguments needs to be of type lamdba, hence the overloads.
template <class T_arg1, class T_arg2>
lambda<lambda_operator<bitwise_assign<rightshift>, T_arg1, T_arg2> >
operator >>= (const lambda<T_arg1>& a1, const lambda<T_arg2>& a2)
{ typedef lambda_operator<bitwise_assign<rightshift>, T_arg1, T_arg2> operator_type;
  return lambda<operator_type>(operator_type(a1.value_,a2.value_)); }
template <class T_arg1, class T_arg2>
lambda<lambda_operator<bitwise_assign<rightshift>, T_arg1, typename unwrap_reference<T_arg2>::type> >
operator >>= (const lambda<T_arg1>& a1, const T_arg2& a2)
{ typedef lambda_operator<bitwise_assign<rightshift>, T_arg1, typename unwrap_reference<T_arg2>::type> operator_type;
  return lambda<operator_type>(operator_type(a1.value_,a2)); }
template <class T_arg1, class T_arg2>
lambda<lambda_operator<bitwise_assign<rightshift>, typename unwrap_reference<T_arg1>::type, T_arg2> >
operator >>= (const T_arg1& a1, const lambda<T_arg2>& a2)
{ typedef lambda_operator<bitwise_assign<rightshift>, typename unwrap_reference<T_arg1>::type, T_arg2> operator_type;
  return lambda<operator_type>(operator_type(a1,a2.value_)); }

// Operators for lambda action bitwise_assign<and_>. At least one of the arguments needs to be of type lamdba, hence the overloads.
template <class T_arg1, class T_arg2>
lambda<lambda_operator<bitwise_assign<and_>, T_arg1, T_arg2> >
operator &= (const lambda<T_arg1>& a1, const lambda<T_arg2>& a2)
{ typedef lambda_operator<bitwise_assign<and_>, T_arg1, T_arg2> operator_type;
  return lambda<operator_type>(operator_type(a1.value_,a2.value_)); }
template <class T_arg1, class T_arg2>
lambda<lambda_operator<bitwise_assign<and_>, T_arg1, typename unwrap_reference<T_arg2>::type> >
operator &= (const lambda<T_arg1>& a1, const T_arg2& a2)
{ typedef lambda_operator<bitwise_assign<and_>, T_arg1, typename unwrap_reference<T_arg2>::type> operator_type;
  return lambda<operator_type>(operator_type(a1.value_,a2)); }
template <class T_arg1, class T_arg2>
lambda<lambda_operator<bitwise_assign<and_>, typename unwrap_reference<T_arg1>::type, T_arg2> >
operator &= (const T_arg1& a1, const lambda<T_arg2>& a2)
{ typedef lambda_operator<bitwise_assign<and_>, typename unwrap_reference<T_arg1>::type, T_arg2> operator_type;
  return lambda<operator_type>(operator_type(a1,a2.value_)); }

// Operators for lambda action bitwise_assign<or_>. At least one of the arguments needs to be of type lamdba, hence the overloads.
template <class T_arg1, class T_arg2>
lambda<lambda_operator<bitwise_assign<or_>, T_arg1, T_arg2> >
operator |= (const lambda<T_arg1>& a1, const lambda<T_arg2>& a2)
{ typedef lambda_operator<bitwise_assign<or_>, T_arg1, T_arg2> operator_type;
  return lambda<operator_type>(operator_type(a1.value_,a2.value_)); }
template <class T_arg1, class T_arg2>
lambda<lambda_operator<bitwise_assign<or_>, T_arg1, typename unwrap_reference<T_arg2>::type> >
operator |= (const lambda<T_arg1>& a1, const T_arg2& a2)
{ typedef lambda_operator<bitwise_assign<or_>, T_arg1, typename unwrap_reference<T_arg2>::type> operator_type;
  return lambda<operator_type>(operator_type(a1.value_,a2)); }
template <class T_arg1, class T_arg2>
lambda<lambda_operator<bitwise_assign<or_>, typename unwrap_reference<T_arg1>::type, T_arg2> >
operator |= (const T_arg1& a1, const lambda<T_arg2>& a2)
{ typedef lambda_operator<bitwise_assign<or_>, typename unwrap_reference<T_arg1>::type, T_arg2> operator_type;
  return lambda<operator_type>(operator_type(a1,a2.value_)); }

// Operators for lambda action bitwise_assign<xor_>. At least one of the arguments needs to be of type lamdba, hence the overloads.
template <class T_arg1, class T_arg2>
lambda<lambda_operator<bitwise_assign<xor_>, T_arg1, T_arg2> >
operator ^= (const lambda<T_arg1>& a1, const lambda<T_arg2>& a2)
{ typedef lambda_operator<bitwise_assign<xor_>, T_arg1, T_arg2> operator_type;
  return lambda<operator_type>(operator_type(a1.value_,a2.value_)); }
template <class T_arg1, class T_arg2>
lambda<lambda_operator<bitwise_assign<xor_>, T_arg1, typename unwrap_reference<T_arg2>::type> >
operator ^= (const lambda<T_arg1>& a1, const T_arg2& a2)
{ typedef lambda_operator<bitwise_assign<xor_>, T_arg1, typename unwrap_reference<T_arg2>::type> operator_type;
  return lambda<operator_type>(operator_type(a1.value_,a2)); }
template <class T_arg1, class T_arg2>
lambda<lambda_operator<bitwise_assign<xor_>, typename unwrap_reference<T_arg1>::type, T_arg2> >
operator ^= (const T_arg1& a1, const lambda<T_arg2>& a2)
{ typedef lambda_operator<bitwise_assign<xor_>, typename unwrap_reference<T_arg1>::type, T_arg2> operator_type;
  return lambda<operator_type>(operator_type(a1,a2.value_)); }

// Operator for lambda action unary_arithmetic<pre_increment>.
template <class T_arg>
lambda<lambda_operator_unary<unary_arithmetic<pre_increment>, T_arg> >
operator ++ (const lambda<T_arg>& a)
{ typedef lambda_operator_unary<unary_arithmetic<pre_increment>, T_arg> operator_type;
  return lambda<operator_type>(operator_type(a.value_)); }

// Operator for lambda action unary_arithmetic<pre_decrement>.
template <class T_arg>
lambda<lambda_operator_unary<unary_arithmetic<pre_decrement>, T_arg> >
operator -- (const lambda<T_arg>& a)
{ typedef lambda_operator_unary<unary_arithmetic<pre_decrement>, T_arg> operator_type;
  return lambda<operator_type>(operator_type(a.value_)); }

// Operator for lambda action unary_arithmetic<negate>.
template <class T_arg>
lambda<lambda_operator_unary<unary_arithmetic<negate>, T_arg> >
operator - (const lambda<T_arg>& a)
{ typedef lambda_operator_unary<unary_arithmetic<negate>, T_arg> operator_type;
  return lambda<operator_type>(operator_type(a.value_)); }

// Operator for lambda action unary_bitwise<not_>.
template <class T_arg>
lambda<lambda_operator_unary<unary_bitwise<not_>, T_arg> >
operator ~ (const lambda<T_arg>& a)
{ typedef lambda_operator_unary<unary_bitwise<not_>, T_arg> operator_type;
  return lambda<operator_type>(operator_type(a.value_)); }

// Operator for lambda action unary_logical<not_>.
template <class T_arg>
lambda<lambda_operator_unary<unary_logical<not_>, T_arg> >
operator ! (const lambda<T_arg>& a)
{ typedef lambda_operator_unary<unary_logical<not_>, T_arg> operator_type;
  return lambda<operator_type>(operator_type(a.value_)); }

// Operator for lambda action unary_other<address>.
template <class T_arg>
lambda<lambda_operator_unary<unary_other<address>, T_arg> >
operator & (const lambda<T_arg>& a)
{ typedef lambda_operator_unary<unary_other<address>, T_arg> operator_type;
  return lambda<operator_type>(operator_type(a.value_)); }

// Operator for lambda action unary_other<dereference>.
template <class T_arg>
lambda<lambda_operator_unary<unary_other<dereference>, T_arg> >
operator * (const lambda<T_arg>& a)
{ typedef lambda_operator_unary<unary_other<dereference>, T_arg> operator_type;
  return lambda<operator_type>(operator_type(a.value_)); }

// Creators for lambda action cast_<reinterpret_>.
template <class T_type, class T_arg>
lambda<lambda_operator_convert<cast_<reinterpret_>, T_type, typename unwrap_lambda_type<T_arg>::type> >
reinterpret_cast_(const T_arg& a)
{ typedef lambda_operator_convert<cast_<reinterpret_>, T_type, typename unwrap_lambda_type<T_arg>::type> operator_type;
  return lambda<operator_type>(operator_type(unwrap_lambda_value(a))); }

// Creators for lambda action cast_<static_>.
template <class T_type, class T_arg>
lambda<lambda_operator_convert<cast_<static_>, T_type, typename unwrap_lambda_type<T_arg>::type> >
static_cast_(const T_arg& a)
{ typedef lambda_operator_convert<cast_<static_>, T_type, typename unwrap_lambda_type<T_arg>::type> operator_type;
  return lambda<operator_type>(operator_type(unwrap_lambda_value(a))); }

// Creators for lambda action cast_<dynamic_>.
template <class T_type, class T_arg>
lambda<lambda_operator_convert<cast_<dynamic_>, T_type, typename unwrap_lambda_type<T_arg>::type> >
dynamic_cast_(const T_arg& a)
{ typedef lambda_operator_convert<cast_<dynamic_>, T_type, typename unwrap_lambda_type<T_arg>::type> operator_type;
  return lambda<operator_type>(operator_type(unwrap_lambda_value(a))); }


} /* namespace sigc */

#endif /* _SIGC_LAMBDA_OPERATOR_HPP_ */
