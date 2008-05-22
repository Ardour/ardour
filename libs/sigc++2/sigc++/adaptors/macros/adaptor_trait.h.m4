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
dnl The idea here is simple.  To prevent the need to 
dnl specializing every adaptor for every type of functor
dnl and worse non-functors like function pointers, we
dnl will make an adaptor trait which can take ordinary
dnl functors and make them adaptor functors for which 
dnl we will of course be able to avoid excess copies. 
dnl (in theory)
dnl
dnl this all depends on partial specialization to allow
dnl us to do
dnl   functor_.template operator() <types> (args);
dnl

dnl I don't understand much of the above. However, I can
dnl see that adaptors are implemented like they are because
dnl there is no way to extract the return type and the argument
dnl types from a functor type. Therefore, operator() is templated.
dnl It's instatiated in slot_call#<>::operator() where the
dnl argument types are known. The return type is finally determined
dnl via the callof<> template - a tricky way to detect the return
dnl type of a functor when the argument types are known. Martin.

])
define([ADAPTOR_DO],[dnl
ifelse($1,0,[dnl
dnl  typename internal::callof_safe0<T_functor>::result_type // doesn't compile if T_functor has an overloaded operator()!
dnl  typename functor_trait<T_functor>::result_type
dnl  operator()() const
dnl    { return functor_(); }
],[dnl
  /** Invokes the wrapped functor passing on the arguments.dnl
FOR(1, $1,[
   * @param _A_arg%1 Argument to be passed on to the functor.])
   * @return The return value of the functor invocation.
   */
  template <LOOP([class T_arg%1], $1)>
  typename deduce_result_type<LOOP(T_arg%1, $1)>::type
  operator()(LOOP(T_arg%1 _A_arg%1, $1)) const
    { return functor_(LOOP(_A_arg%1, $1)); }

  #ifndef SIGC_TEMPLATE_SPECIALIZATION_OPERATOR_OVERLOAD
  template <LOOP([class T_arg%1], $1)>
  typename deduce_result_type<LOOP(T_arg%1, $1)>::type
  sun_forte_workaround(LOOP(T_arg%1 _A_arg%1, $1)) const
    { //Just calling operator() tries to copy the argument:
      return functor_(LOOP(_A_arg%1, $1));
    }
  #endif
  
])dnl
])

divert(0)dnl
__FIREWALL__
#include <sigc++config.h> //To get SIGC_TEMPLATE_KEYWORD_OPERATOR_OVERLOAD
#include <sigc++/visit_each.h>
#include <sigc++/functors/functor_trait.h>
#include <sigc++/functors/ptr_fun.h>
#include <sigc++/functors/mem_fun.h>
#include <sigc++/adaptors/deduce_result_type.h>

namespace sigc {

// Call either operator()<>() or sun_forte_workaround<>(),
// depending on the compiler:
#ifdef SIGC_GCC_TEMPLATE_SPECIALIZATION_OPERATOR_OVERLOAD
  #define SIGC_WORKAROUND_OPERATOR_PARENTHESES template operator()
  #define SIGC_TEMPLATE_SPECIALIZATION_OPERATOR_OVERLOAD
#else
  #ifdef SIGC_MSVC_TEMPLATE_SPECIALIZATION_OPERATOR_OVERLOAD
    #define SIGC_WORKAROUND_OPERATOR_PARENTHESES operator()
    #define SIGC_TEMPLATE_SPECIALIZATION_OPERATOR_OVERLOAD
  #else
    #define SIGC_WORKAROUND_OPERATOR_PARENTHESES sun_forte_workaround
  #endif
#endif


template <class T_functor> struct adapts;

/** @defgroup adaptors Adaptors
 * Adaptors are functors that alter the signature of a functor's
 * operator()().
 *
 * The adaptor types libsigc++ provides
 * are created with bind(), bind_return(), hide(), hide_return(),
 * retype_return(), retype(), compose(), exception_catch() and group().
 *
 * You can easily derive your own adaptor type from sigc::adapts.
 */

/** Converts an arbitrary functor into an adaptor type.
 * All adaptor tyes in libsigc++ are unnumbered and have
 * a <tt>template operator()</tt> member of every argument count
 * they support. These functions in turn invoke a stored adaptor's
 * <tt>template operator()</tt> processing the arguments and return
 * value in a characteristic manner. Explicit function template
 * instantiation is used to pass type hints thus saving copy costs.
 *
 * adaptor_functor is a glue between adaptors and arbitrary functors
 * that just passes on the arguments. You won't use this type directly.
 *
 * The template argument @e T_functor determines the type of stored
 * functor.
 *
 * @ingroup adaptors
 */
template <class T_functor>
struct adaptor_functor : public adaptor_base
{
  template <LOOP(class T_arg%1=void, CALL_SIZE)>
  struct deduce_result_type
    { typedef typename sigc::deduce_result_type<LIST(T_functor, LOOP(T_arg%1,CALL_SIZE))>::type type; };
  typedef typename functor_trait<T_functor>::result_type result_type;

  /** Invokes the wrapped functor passing on the arguments.
   * @return The return value of the functor invocation.
   */
  result_type
  operator()() const;

  #ifndef SIGC_TEMPLATE_SPECIALIZATION_OPERATOR_OVERLOAD
  result_type sun_forte_workaround() const
    { return operator(); }
  #endif
  
FOR(0,CALL_SIZE,[[ADAPTOR_DO(%1)]])dnl
  /// Constructs an invalid functor.
  adaptor_functor()
    {}

  /** Constructs an adaptor_functor object that wraps the passed functor.
   * @param _A_functor Functor to invoke from operator()().
   */
  explicit adaptor_functor(const T_functor& _A_functor)
    : functor_(_A_functor)
    {}

  /** Constructs an adaptor_functor object that wraps the passed (member)
   * function pointer.
   * @param _A_type Pointer to function or class method to invoke from operator()().
   */
  template <class T_type>
  explicit adaptor_functor(const T_type& _A_type)
    : functor_(_A_type)
    {}

  /// Functor that is invoked from operator()().
  mutable T_functor functor_;
};

template <class T_functor>
typename adaptor_functor<T_functor>::result_type
adaptor_functor<T_functor>::operator()() const
  { return functor_(); }


//template specialization of visit_each<>(action, functor):
/** Performs a functor on each of the targets of a functor.
 * The function overload for sigc::adaptor_functor performs a functor
 * on the functor stored in the sigc::adaptor_functor object.
 *
 * @ingroup adaptors
 */
template <class T_action, class T_functor>
void visit_each(const T_action& _A_action,
                const adaptor_functor<T_functor>& _A_target)
{
  //The extra sigc:: prefix avoids ambiguity in some strange
  //situations.
  sigc::visit_each(_A_action, _A_target.functor_);
}


/** Trait that specifies what is the adaptor version of a functor type.
 * Template specializations for sigc::adaptor_base derived functors,
 * for function pointers and for class methods are provided.
 *
 * The template argument @e T_functor is the functor type to convert.
 * @e I_isadaptor indicates whether @e T_functor inherits from sigc::adaptor_base.
 *
 * @ingroup adaptors
 */
template <class T_functor, bool I_isadaptor = is_base_and_derived<adaptor_base, T_functor>::value> struct adaptor_trait;

/** Trait that specifies what is the adaptor version of a functor type.
 * This template specialization is used for types that inherit from adaptor_base.
 * adaptor_type is equal to @p T_functor in this case.
 */
template <class T_functor> 
struct adaptor_trait<T_functor, true>
{
  typedef typename T_functor::result_type result_type;
  typedef T_functor functor_type;
  typedef T_functor adaptor_type;
};

/** Trait that specifies what is the adaptor version of a functor type.
 * This template specialization is used for arbitrary functors,
 * for function pointers and for class methods are provided.
 * The latter are converted into @p pointer_functor or @p mem_functor types.
 * adaptor_type is equal to @p adaptor_functor<functor_type>.
 */
template <class T_functor>
struct adaptor_trait<T_functor, false>
{
  typedef typename functor_trait<T_functor>::result_type result_type;
  typedef typename functor_trait<T_functor>::functor_type functor_type;
  typedef adaptor_functor<functor_type> adaptor_type;
};


/** Base type for adaptors.
 * adapts wraps adaptors, functors, function pointers and class methods.
 * It contains a single member functor which is always a sigc::adaptor_base.
 * The typedef adaptor_type defines the exact type that is used
 * to store the adaptor, functor, function pointer or class method passed
 * into the constructor. It differs from @e T_functor unless @e T_functor
 * inherits from sigc::adaptor_base.
 *
 * @par Example of a simple adaptor:
 *   @code
 *   template <T_functor>
 *   struct my_adpator : public sigc::adapts<T_functor>
 *   {
 *     template <class T_arg1=void, class T_arg2=void>
 *     struct deduce_result_type
 *     { typedef typename sigc::deduce_result_type<T_functor, T_arg1, T_arg2>::type type; };
 *     typedef typename sigc::functor_trait<T_functor>::result_type result_type;
 *
 *     result_type
 *     operator()() const;
 *
 *     template <class T_arg1>
 *     typename deduce_result_type<T_arg1>::type
 *     operator()(T_arg1 _A_arg1) const;
 *
 *     template <class T_arg1, class T_arg2>
 *     typename deduce_result_type<T_arg1, T_arg2>::type
 *     operator()(T_arg1 _A_arg1, class T_arg2) const;
 *
 *     explicit adaptor_functor(const T_functor& _A_functor) // Constructs a my_functor object that wraps the passed functor.
 *       : sigc::adapts<T_functor>(_A_functor) {}
 *
 *     mutable T_functor functor_; // Functor that is invoked from operator()().
 *   };
 *   @endcode
 *
 * @ingroup adaptors
 */
template <class T_functor>
struct adapts : public adaptor_base
{
  typedef typename adaptor_trait<T_functor>::result_type  result_type;
  typedef typename adaptor_trait<T_functor>::adaptor_type adaptor_type;

  /** Constructs an adaptor that wraps the passed functor.
   * @param _A_functor Functor to invoke from operator()().
   */
  explicit adapts(const T_functor& _A_functor)
    : functor_(_A_functor)
    {}

  /// Adaptor that is invoked from operator()().
  mutable adaptor_type functor_;
};

} /* namespace sigc */
