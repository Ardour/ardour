// -*- c++ -*-
/* Do not edit! -- generated file */
/*
*/
#ifndef _SIGC_ADAPTORS_MACROS_DEDUCE_RESULT_TYPEHM4_
#define _SIGC_ADAPTORS_MACROS_DEDUCE_RESULT_TYPEHM4_
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
 * <tt>typename deduce_result_type<functor_type, list of arg_types>::type</tt>
 * deduces a functor's result type if @p functor_type inherits from
 * sigc::functor_base and defines @p result_type or if @p functor_type
 * is actually a (member) function type. Multi-type functors are not
 * supported.
 *
 * sigc++ adaptors use
 * <tt>typename deduce_result_type<functor_type, list of arg_types>::type</tt>
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
          class T_arg1=void,class T_arg2=void,class T_arg3=void,class T_arg4=void,class T_arg5=void,class T_arg6=void,class T_arg7=void,
          bool I_derives_adaptor_base=is_base_and_derived<adaptor_base,T_functor>::value>
struct deduce_result_type
  { typedef typename functor_trait<T_functor>::result_type type; };

/** Deduce the return type of a functor.
 * This is the template specialization of the sigc::deduce_result_type template
 * for 0 arguments.
 */
template <class T_functor>
struct deduce_result_type<T_functor, void,void,void,void,void,void,void, true>
  { typedef typename T_functor::template deduce_result_type<>::type type; };

/** Deduce the return type of a functor.
 * This is the template specialization of the sigc::deduce_result_type template
 * for 1 arguments.
 */
template <class T_functor, class T_arg1>
struct deduce_result_type<T_functor, T_arg1, void,void,void,void,void,void, true>
  { typedef typename T_functor::template deduce_result_type<T_arg1>::type type; };

/** Deduce the return type of a functor.
 * This is the template specialization of the sigc::deduce_result_type template
 * for 2 arguments.
 */
template <class T_functor, class T_arg1,class T_arg2>
struct deduce_result_type<T_functor, T_arg1,T_arg2, void,void,void,void,void, true>
  { typedef typename T_functor::template deduce_result_type<T_arg1,T_arg2>::type type; };

/** Deduce the return type of a functor.
 * This is the template specialization of the sigc::deduce_result_type template
 * for 3 arguments.
 */
template <class T_functor, class T_arg1,class T_arg2,class T_arg3>
struct deduce_result_type<T_functor, T_arg1,T_arg2,T_arg3, void,void,void,void, true>
  { typedef typename T_functor::template deduce_result_type<T_arg1,T_arg2,T_arg3>::type type; };

/** Deduce the return type of a functor.
 * This is the template specialization of the sigc::deduce_result_type template
 * for 4 arguments.
 */
template <class T_functor, class T_arg1,class T_arg2,class T_arg3,class T_arg4>
struct deduce_result_type<T_functor, T_arg1,T_arg2,T_arg3,T_arg4, void,void,void, true>
  { typedef typename T_functor::template deduce_result_type<T_arg1,T_arg2,T_arg3,T_arg4>::type type; };

/** Deduce the return type of a functor.
 * This is the template specialization of the sigc::deduce_result_type template
 * for 5 arguments.
 */
template <class T_functor, class T_arg1,class T_arg2,class T_arg3,class T_arg4,class T_arg5>
struct deduce_result_type<T_functor, T_arg1,T_arg2,T_arg3,T_arg4,T_arg5, void,void, true>
  { typedef typename T_functor::template deduce_result_type<T_arg1,T_arg2,T_arg3,T_arg4,T_arg5>::type type; };

/** Deduce the return type of a functor.
 * This is the template specialization of the sigc::deduce_result_type template
 * for 6 arguments.
 */
template <class T_functor, class T_arg1,class T_arg2,class T_arg3,class T_arg4,class T_arg5,class T_arg6>
struct deduce_result_type<T_functor, T_arg1,T_arg2,T_arg3,T_arg4,T_arg5,T_arg6, void, true>
  { typedef typename T_functor::template deduce_result_type<T_arg1,T_arg2,T_arg3,T_arg4,T_arg5,T_arg6>::type type; };

/** Deduce the return type of a functor.
 * This is the template specialization of the sigc::deduce_result_type template
 * for 7 arguments.
 */
template <class T_functor, class T_arg1,class T_arg2,class T_arg3,class T_arg4,class T_arg5,class T_arg6,class T_arg7>
struct deduce_result_type<T_functor, T_arg1,T_arg2,T_arg3,T_arg4,T_arg5,T_arg6,T_arg7, true>
  { typedef typename T_functor::template deduce_result_type<T_arg1,T_arg2,T_arg3,T_arg4,T_arg5,T_arg6,T_arg7>::type type; };


} /* namespace sigc */
#endif /* _SIGC_ADAPTORS_MACROS_DEDUCE_RESULT_TYPEHM4_ */
