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

define([LAMBDA_DO],[dnl
  template <LOOP(class T_arg%1, $1)>
  typename deduce_result_type<LOOP(T_arg%1,$1)>::type
  operator ()(LOOP(T_arg%1 _A_%1, $1)) const 
    { return value_.SIGC_WORKAROUND_OPERATOR_PARENTHESES<LOOP(_P_(T_arg%1), $1)>
             (LOOP(_A_%1, $1)); 
    }

  #ifndef SIGC_TEMPLATE_SPECIALIZATION_OPERATOR_OVERLOAD
  template <LOOP(class T_arg%1, $1)>
  typename deduce_result_type<LOOP(T_arg%1,$1)>::type
  sun_forte_workaround(LOOP(T_arg%1 _A_%1, $1)) const
    { return value_.SIGC_WORKAROUND_OPERATOR_PARENTHESES<LOOP(_P_(T_arg%1), $1)>
             (LOOP(_A_%1, $1)); 
    }
  #endif //SIGC_TEMPLATE_SPECIALIZATION_OPERATOR_OVERLOAD

])dnl
define([LAMBDA_DO_VALUE],[dnl
  template <LOOP(class T_arg%1, $1)>
  result_type operator ()(LOOP(T_arg%1, $1)) const 
    { return value_; }

  #ifndef SIGC_TEMPLATE_SPECIALIZATION_OPERATOR_OVERLOAD
  template <LOOP(class T_arg%1, $1)>
  result_type sun_forte_workaround(LOOP(T_arg%1, $1)) const
    { return value_; }
  #endif //SIGC_TEMPLATE_SPECIALIZATION_OPERATOR_OVERLOAD

])dnl

divert(0)dnl
#ifndef _SIGC_LAMBDA_BASE_HPP_
#define _SIGC_LAMBDA_BASE_HPP_
#include <sigc++/adaptors/adaptor_trait.h>
#include <sigc++/reference_wrapper.h>

namespace sigc {

/** @defgroup lambdas Lambdas
 * libsigc++ ships with basic lambda functionality and the sigc::group adaptor that uses lambdas to transform a functor's parameter list.
 *
 * The lambda selectors sigc::_1, sigc::_2, ..., sigc::_9 are used to select the
 * first, second, ..., nineth argument from a list.
 *
 * @par Examples:
 *   @code
 *   std::cout << sigc::_1(10,20,30); // returns 10
 *   std::cout << sigc::_2(10,20,30); // returns 20
 *   ...
 *   @endcode
 *
 * Operators are defined so that lambda selectors can be used e.g. as placeholders in
 * arithmetic expressions.
 *
 * @par Examples:
 *   @code
 *   std::cout << (sigc::_1 + 5)(3); // returns (3 + 5)
 *   std::cout << (sigc::_1 * sigc::_2)(7,10); // returns (7 * 10)
 *   @endcode
 */

/** A hint to the compiler.
 * All lambda types publically inherit from this hint.
 *
 * @ingroup lambdas
 */
struct lambda_base : public adaptor_base {};

// Forward declaration of lambda.
template <class T_type> struct lambda;


namespace internal {

/** Abstracts lambda functionality.
 * Objects of this type store a value that may be of type lambda itself.
 * In this case, operator()() executes the lambda (a lambda is always a functor at the same time).
 * Otherwise, operator()() simply returns the stored value.
 */
template <class T_type, bool I_islambda = is_base_and_derived<lambda_base, T_type>::value> struct lambda_core;

/// Abstracts lambda functionality (template specialization for lambda values).
template <class T_type>
struct lambda_core<T_type, true> : public lambda_base
{
  template <LOOP(class T_arg%1=void,CALL_SIZE)>
  struct deduce_result_type
    { typedef typename T_type::template deduce_result_type<LOOP(_P_(T_arg%1),CALL_SIZE)>::type type; };
  typedef typename T_type::result_type result_type;
  typedef T_type lambda_type;

  result_type
  operator()() const;

FOR(1,CALL_SIZE,[[LAMBDA_DO(%1)]])dnl
  lambda_core() {}

  explicit lambda_core(const T_type& v)
    : value_(v) {}

  T_type value_;
};

template <class T_type>
typename lambda_core<T_type, true>::result_type
lambda_core<T_type, true>::operator()() const
  { return value_(); }


/// Abstracts lambda functionality (template specialization for other value types).
template <class T_type>
struct lambda_core<T_type, false> : public lambda_base
{
  template <LOOP(class T_arg%1=void,CALL_SIZE)>
  struct deduce_result_type
    { typedef T_type type; };
  typedef T_type result_type; // all operator() overloads return T_type.
  typedef lambda<T_type> lambda_type;

  result_type operator()() const;

FOR(1,CALL_SIZE,[[LAMBDA_DO_VALUE(%1)]])dnl
  explicit lambda_core(typename type_trait<T_type>::take v)
    : value_(v) {}

  T_type value_;
};

template <class T_type>
typename lambda_core<T_type, false>::result_type lambda_core<T_type, false>::operator()() const
  { return value_; }

} /* namespace internal */


//template specialization of visit_each<>(action, functor):
template <class T_action, class T_functor, bool I_islambda>
void visit_each(const T_action& _A_action,
                const internal::lambda_core<T_functor, I_islambda>& _A_target)
{
  visit_each(_A_action, _A_target.value_);
}


// forward declarations for lambda operators other<subscript> and other<assign>
template <class T_type>
struct other;
struct subscript;
struct assign;

template <class T_action, class T_type1, class T_type2>
struct lambda_operator;

template <class T_type>
struct unwrap_lambda_type;


/** Lambda type.
 * Objects of this type store a value that may be of type lambda itself.
 * In this case, operator()() executes the lambda (a lambda is always a functor at the same time).
 * Otherwise, operator()() simply returns the stored value.
 * The assign and subscript operators are defined to return a lambda operator.
 *
 * @ingroup lambdas
 */
template <class T_type>
struct lambda : public internal::lambda_core<T_type>
{
  typedef lambda<T_type> self;

  lambda()
    {}

  lambda(typename type_trait<T_type>::take v)
    : internal::lambda_core<T_type>(v) 
    {}

  // operators for other<subscript>
  template <class T_arg>
  lambda<lambda_operator<other<subscript>, self, typename unwrap_lambda_type<T_arg>::type> >
  operator [[]] (const T_arg& a) const
    { typedef lambda_operator<other<subscript>, self, typename unwrap_lambda_type<T_arg>::type> lambda_operator_type;
      return lambda<lambda_operator_type>(lambda_operator_type(this->value_, unwrap_lambda_value(a))); }

  // operators for other<assign>
  template <class T_arg>
  lambda<lambda_operator<other<assign>, self, typename unwrap_lambda_type<T_arg>::type> >
  operator = (const T_arg& a) const
    { typedef lambda_operator<other<assign>, self, typename unwrap_lambda_type<T_arg>::type> lambda_operator_type;
      return lambda<lambda_operator_type>(lambda_operator_type(this->value_, unwrap_lambda_value(a))); }
};


//template specialization of visit_each<>(action, functor):
template <class T_action, class T_type>
void visit_each(const T_action& _A_action,
                const lambda<T_type>& _A_target)
{
  visit_each(_A_action, _A_target.value_);
}

dnl /* With the Sun FORTE and the Compaq C++ compiler,
dnl  * sigc::var() doesn't work with string constants.
dnl  * Some work-araound is needed to convert 'const (&) char[N]'
dnl  * into 'const char*'. The following work-around works with gcc
dnl  * but neither with the Sun FORTE nor with the Compaq C++ compiler
dnl  * (for the gcc the work-around is not needed, anyway):
dnl  */
dnl namespace internal {
dnl 
dnl template <class T_type>
dnl struct convert_array
dnl { typedef T_type& type; };
dnl 
dnl template <class T_type, int N>
dnl struct convert_array<T_type[[N]]>
dnl { typedef T_type* type; };
dnl 
dnl } /* namespace internal */
dnl 
dnl /// Converts a constant variable into a lambda object.
dnl template <class T_type>
dnl lambda<T_type> constant(const T_type& v)
dnl { return lambda<T_type>(v); }
dnl 
dnl /// Converts a reference into a lambda object.
dnl template <class T_type>
dnl lambda<typename internal::convert_array<T_type>::type> var(T_type& v)
dnl { return lambda<typename internal::convert_array<T_type>::type>(v); }
dnl 
dnl /// Converts a constant reference into a lambda object.
dnl template <class T_type>
dnl lambda<typename internal::convert_array<const T_type>::type> var(const T_type& v)
dnl { return lambda<typename internal::convert_array<const T_type>::type>(v); }

/** Converts a reference into a lambda object.
 * sigc::var creates a 0-ary functor, returning the value of a referenced variable. 
 *
 * @par Example:
 *   @code
 *   int main(int argc, char* argv[])
 *   {
 *     int data;
 *     sigc::signal<int> readValue;
 *
 *     readValue.connect(sigc::var(data));
 *
 *     data = 3;
 *     std::cout << readValue() << std::endl; //Prints 3.
 *
 *    data = 5;
 *    std::cout << readValue() << std::endl; //Prints 5.
 *   }
 *   @endcode
 */
template <class T_type>
lambda<T_type&> var(T_type& v)
{ return lambda<T_type&>(v); }

/** Converts a constant reference into a lambda object.
 */
template <class T_type>
lambda<const T_type&> var(const T_type& v)
{ return lambda<const T_type&>(v); }


/** Deduces the type of the object stored in an object of the passed lambda type.
 * If the type passed as template argument is no lambda type,
 * type is defined to unwrap_reference<T_type>::type.
 */
template <class T_type>
struct unwrap_lambda_type
{ typedef typename unwrap_reference<T_type>::type type; };

template <class T_type>
struct unwrap_lambda_type<lambda<T_type> >
{ typedef T_type type; };


/** Gets the object stored inside a lambda object.
 * Returns the object passed as argument if it is not of type lambda.
 */
template <class T_type>
T_type& unwrap_lambda_value(T_type& a)
{ return a; }

template <class T_type>
const T_type& unwrap_lambda_value(const T_type& a)
{ return a; }

template <class T_type>
const T_type& unwrap_lambda_value(const lambda<T_type>& a)
{ return a.value_; }

} /* namespace sigc */

#endif /* _SIGC_LAMBDA_BASE_HPP_ */
