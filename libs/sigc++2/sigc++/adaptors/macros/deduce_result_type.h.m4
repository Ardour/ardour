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

define([DEDUCE_RESULT_TYPE_ADAPTOR],[dnl
/** Deduce the return type of a functor.
 * This is the template specialization of the sigc::deduce_result_type template
 * for $1 arguments.
 */
template <LIST(class T_functor, LOOP(class T_arg%1, $1))>
struct deduce_result_type<LIST(T_functor, LOOP(T_arg%1,$1), LOOP(void,eval($2-$1)), true)>
  { typedef typename T_functor::template deduce_result_type<LOOP(T_arg%1,$1)>::type type; };

])
dnl 01.11.2003: Completely removed support for typeof() since it is non-standard!
dnl define([DEDUCE_RESULT_TYPE_TYPEOF],[dnl
dnl template <LIST(class T_functor, LOOP(class T_arg%1, $1))>
dnl struct deduce_result_type<LIST(T_functor, LOOP(T_arg%1,$1), LOOP(void,eval($2-$1)), false)>
dnl {
dnl   typedef typeof(type_trait<T_functor>::instance().
dnl                    T_functor::operator()(LOOP([
dnl                       type_trait<T_arg%1>::instance()], $1))) type;
dnl };
dnl 
dnl ])

divert(0)dnl
/*
*/
__FIREWALL__
#include <sigc++/functors/functor_trait.h>


namespace sigc {

/** A hint to the compiler.
 * Functors which have all methods based on templates
 * should publicly inherit from this hint and define 
 * a nested template class @p deduce_result_type that
 * can be used to deduce the methods' return types.
 *
 * adaptor_base inherits from the functor_base hint so
 * derived types should also have a result_type defined.
 *
 * Adaptors don't inherit from this type directly. They use
 * use sigc::adapts as a base type instead. sigc::adaptors
 * wraps arbitrary functor types as well as function pointers
 * and class methods.
 *
 * @ingroup adaptors
 */
struct adaptor_base : public functor_base {};


/** Deduce the return type of a functor.
 * <tt>typename deduce_result_type<functor_type, [list of arg_types]>::type</tt>
 * deduces a functor's result type if @p functor_type inherits from
 * sigc::functor_base and defines @p result_type or if @p functor_type
 * is actually a (member) function type. Multi-type functors are not
 * supported.
 *
 * sigc++ adaptors use
 * <tt>typename deduce_result_type<functor_type, [list of arg_types]>::type</tt>
 * to determine the return type of their <tt>templated operator()</tt> overloads.
 *
 * Adaptors in turn define a nested template class @p deduce_result_type
 * that is used by template specializations of the global deduce_result_type
 * template to correctly deduce the return types of the adaptor's suitable
 * <tt>template operator()</tt> overload.
 *
 * @ingroup adaptors
 */
template <class T_functor,
          LOOP(class T_arg%1=void, CALL_SIZE),
          bool I_derives_adaptor_base=is_base_and_derived<adaptor_base,T_functor>::value>
struct deduce_result_type
  { typedef typename functor_trait<T_functor>::result_type type; };

FOR(0,CALL_SIZE,[[DEDUCE_RESULT_TYPE_ADAPTOR(%1,CALL_SIZE)]])dnl

dnl #ifdef SIGC_CXX_TYPEOF
dnl FOR(0,CALL_SIZE,[[DEDUCE_RESULT_TYPE_TYPEOF(%1,CALL_SIZE)]])
dnl #endif
dnl
} /* namespace sigc */
