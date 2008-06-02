dnl Copyright 2003, The libsigc++ Development Team 
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

define([RETYPE_OPERATOR],[dnl
ifelse($1,0,[dnl
  result_type operator()();
    
],[dnl
  template <LOOP(class T_arg%1, $1)>
  typename deduce_result_type<LOOP(T_arg%1,$1)>::type
  operator()(LOOP(T_arg%1 _A_a%1, $1))
    { return this->functor_.SIGC_WORKAROUND_OPERATOR_PARENTHESES<LOOP(typename type_trait<T_type%1>::take, $1)>
        (LOOP([[static_cast<T_type%1>(_A_a%1)]], $1));
    }

  #ifndef SIGC_TEMPLATE_SPECIALIZATION_OPERATOR_OVERLOAD
  template <LOOP(class T_arg%1, $1)>
  typename deduce_result_type<LOOP(T_arg%1,$1)>::type
  sun_forte_workaround(LOOP(T_arg%1 _A_a%1, $1))
    { return this->functor_.SIGC_WORKAROUND_OPERATOR_PARENTHESES<LOOP(typename type_trait<T_type%1>::take, $1)>
        (LOOP([[static_cast<T_type%1>(_A_a%1)]], $1));
    }
  #endif

])dnl
])
define([RETYPE_POINTER_FUNCTOR],[dnl
/** Creates an adaptor of type sigc::retype_functor which performs C-style casts on the parameters passed on to the functor.
 * This function template specialization works on sigc::pointer_functor.
 *
 * @param _A_functor Functor that should be wrapped.
 * @return Adaptor that executes @e _A_functor performing C-style casts on the paramters passed on.
 *
 * @ingroup retype
 */
template <LIST(LOOP(class T_arg%1, $1), class T_return)>
inline retype_functor<LIST(pointer_functor$1<LIST(LOOP(T_arg%1, $1), T_return)>, LOOP(T_arg%1, $1)) >
retype(const pointer_functor$1<LIST(LOOP(T_arg%1, $1), T_return)>& _A_functor)
{ return retype_functor<LIST(pointer_functor$1<LIST(LOOP(T_arg%1, $1), T_return)>, LOOP(T_arg%1, $1)) >
    (_A_functor); }

])
define([RETYPE_MEM_FUNCTOR],[dnl
/** Creates an adaptor of type sigc::retype_functor which performs C-style casts on the parameters passed on to the functor.
 * This function template specialization works on sigc::$2[]mem_functor.
 *
 * @param _A_functor Functor that should be wrapped.
 * @return Adaptor that executes @e _A_functor performing C-style casts on the paramters passed on.
 *
 * @ingroup retype
 */
template <LIST(class T_return, class T_obj, LOOP(class T_arg%1, $1))>
inline retype_functor<LIST($2[]mem_functor$1<LIST(T_return, T_obj, LOOP(T_arg%1, $1))>, LOOP(T_arg%1, $1)) >
retype(const $2[]mem_functor$1<LIST(T_return, T_obj, LOOP(T_arg%1, $1))>& _A_functor)
{ return retype_functor<LIST($2[]mem_functor$1<LIST(T_return, T_obj, LOOP(T_arg%1, $1))>, LOOP(T_arg%1, $1)) >
    (_A_functor); }

])

divert(0)dnl
__FIREWALL__
#include <sigc++/adaptors/adaptor_trait.h>
#include <sigc++/functors/ptr_fun.h>
#include <sigc++/functors/mem_fun.h>
#include <sigc++/functors/slot.h>

namespace sigc {

/** @defgroup retype retype(), retype_return()
 * sigc::retype() alters a sigc::pointer_functor, a sigc::mem_functor or a sigc::slot
 * in that it makes C-style casts to the functor's parameter types
 * of all parameters passed through operator()().
 *
 * Use this adaptor for inline conversion between numeric or other simple types.
 * @par Example:
 *   @code
 *   void foo(int);
 *   sigc::retype(sigc::ptr_fun(&foo))(5.7F); // calls foo(5)
 *   @endcode
 *
 * The functor sigc::retype() returns can be passed into
 * sigc::signal::connect() directly.
 *
 * @par Example:
 *   @code
 *   sigc::signal<void,float> some_signal;
 *   void foo(int);
 *   some_signal.connect(sigc::retype(sigc::ptr_fun(&foo)));
 *   @endcode
 *
 * This adaptor builds an exception in that it only works on sig::pointer_functor,
 * sigc::mem_functor and sigc::slot because it needs sophisticated information about
 * the parameter types that cannot be deduced from arbitrary functor types.
 *
 * sigc::retype_return() alters the return type of an arbitrary functor.
 * Like in sigc::retype() a C-style cast is preformed. Usage sigc::retype_return() is
 * not restricted to libsigc++ functor types but you need to
 * specify the new return type as a template parameter.
 *
 * @par Example:
 *   @code
 *   float foo();
 *   std::cout << sigc::retype_return<int>(&foo)(); // converts foo's return value to an integer
 *   @endcode
 *
 * @ingroup adaptors
 */

/** Adaptor that performs C-style casts on the parameters passed on to the functor.
 * Use the convenience function sigc::retype() to create an instance of retype_functor.
 *
 * The following template arguments are used:
 * - @e T_functor Type of the functor to wrap.dnl
FOR(1, CALL_SIZE,[
 * - @e T_type%1 Type of @e T_functor's %1th argument.])
 *
 * @ingroup retype
 */
template <LIST(class T_functor, LOOP(class T_type%1=nil, CALL_SIZE))>
struct retype_functor
  : public adapts<T_functor>
{
  typedef typename adapts<T_functor>::adaptor_type adaptor_type;

  template <LOOP(class T_arg%1=void, CALL_SIZE)>
  struct deduce_result_type
    { typedef typename adaptor_type::template deduce_result_type<LOOP(_P_(T_arg%1),CALL_SIZE)>::type type; };
  typedef typename adapts<T_functor>::result_type result_type;

FOR(0,CALL_SIZE,[[RETYPE_OPERATOR(%1)]])dnl

  /** Constructs a retype_functor object that performs C-style casts on the parameters passed on to the functor.
   * @param _A_functor Functor to invoke from operator()().
   */
  explicit retype_functor(typename type_trait<T_functor>::take _A_functor)
    : adapts<T_functor>(_A_functor)
    {}
};

template <LIST(class T_functor, LOOP(class T_type%1, CALL_SIZE))>
typename retype_functor<LIST(T_functor, LOOP(T_type%1, CALL_SIZE))>::result_type
retype_functor<LIST(T_functor, LOOP(T_type%1, CALL_SIZE))>::operator()()
  { return this->functor_(); }

  
//template specialization of visit_each<>(action, functor):
/** Performs a functor on each of the targets of a functor.
 * The function overload for sigc::retype_functor performs a functor on the
 * functor stored in the sigc::retype_functor object.
 *
 * @ingroup retype
 */
template <LIST(class T_action, class T_functor, LOOP(class T_type%1, CALL_SIZE))>
void visit_each(const T_action& _A_action,
                const retype_functor<LIST(T_functor, LOOP(T_type%1, CALL_SIZE))>& _A_target)
{
  visit_each(_A_action, _A_target.functor_);
}


/** Creates an adaptor of type sigc::retype_functor which performs C-style casts on the parameters passed on to the functor.
 * This function template specialization works on sigc::slot.
 *
 * @param _A_functor Functor that should be wrapped.
 * @return Adaptor that executes @e _A_functor performing C-style casts on the paramters passed on.
 *
 * @ingroup retype
 */
template <LIST(class T_return, LOOP(class T_arg%1, CALL_SIZE))>
inline retype_functor<LIST(slot<LIST(T_return, LOOP(T_arg%1, CALL_SIZE))>, LOOP(T_arg%1, CALL_SIZE)) >
retype(const slot<LIST(T_return, LOOP(T_arg%1, CALL_SIZE))>& _A_functor)
{ return retype_functor<LIST(slot<LIST(T_return, LOOP(T_arg%1, CALL_SIZE))>, LOOP(T_arg%1, CALL_SIZE)) >
    (_A_functor); }


FOR(0,CALL_SIZE,[[RETYPE_POINTER_FUNCTOR(%1)]])dnl

FOR(0,CALL_SIZE,[[RETYPE_MEM_FUNCTOR(%1,[])]])dnl
FOR(0,CALL_SIZE,[[RETYPE_MEM_FUNCTOR(%1,[const_])]])dnl
FOR(0,CALL_SIZE,[[RETYPE_MEM_FUNCTOR(%1,[volatile_])]])dnl
FOR(0,CALL_SIZE,[[RETYPE_MEM_FUNCTOR(%1,[const_volatile_])]])dnl
FOR(0,CALL_SIZE,[[RETYPE_MEM_FUNCTOR(%1,[bound_])]])dnl
FOR(0,CALL_SIZE,[[RETYPE_MEM_FUNCTOR(%1,[bound_const_])]])dnl
FOR(0,CALL_SIZE,[[RETYPE_MEM_FUNCTOR(%1,[bound_volatile_])]])dnl
FOR(0,CALL_SIZE,[[RETYPE_MEM_FUNCTOR(%1,[bound_const_volatile_])]])dnl

} /* namespace sigc */
