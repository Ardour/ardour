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

define([RETYPE_RETURN_OPERATOR],[dnl
  template <LOOP(class T_arg%1, $1)>
  inline T_return operator()(LOOP(T_arg%1 _A_a%1, $1))
    { return T_return(this->functor_.SIGC_WORKAROUND_OPERATOR_PARENTHESES<LOOP(_P_(T_arg%1), $1)>
        (LOOP(_A_a%1, $1)));
    }

  #ifndef SIGC_TEMPLATE_SPECIALIZATION_OPERATOR_OVERLOAD
  template <LOOP(class T_arg%1, $1)>
  inline T_return sun_forte_workaround(LOOP(T_arg%1 _A_a%1, $1))
    { return T_return(this->functor_.SIGC_WORKAROUND_OPERATOR_PARENTHESES<LOOP(_P_(T_arg%1), $1)>
        (LOOP(_A_a%1, $1)));
    }
  #endif
    
])
define([RETYPE_RETURN_VOID_OPERATOR],[dnl
  template <LOOP(class T_arg%1, $1)>
  inline void operator()(LOOP(T_arg%1 _A_a%1, $1))
    { this->functor_.SIGC_WORKAROUND_OPERATOR_PARENTHESES<LOOP(_P_(T_arg%1), $1)>
        (LOOP(_A_a%1, $1));
    }

  #ifndef SIGC_TEMPLATE_SPECIALIZATION_OPERATOR_OVERLOAD
  template <LOOP(class T_arg%1, $1)>
  inline void sun_forte_workaround(LOOP(T_arg%1 _A_a%1, $1))
    { this->functor_.SIGC_WORKAROUND_OPERATOR_PARENTHESES<LOOP(_P_(T_arg%1), $1)>
        (LOOP(_A_a%1, $1));
    }
  #endif

])

divert(0)dnl
__FIREWALL__
#include <sigc++/adaptors/adaptor_trait.h>

namespace sigc {

/** Adaptor that perform a C-style cast on the return value of a functor.
 * Use the convenience function sigc::retype_return() to create an instance of retype_return_functor.
 *
 * The following template arguments are used:
 * - @e T_return Target type of the C-style cast.
 * - @e T_functor Type of the functor to wrap.
 *
 * @ingroup retype
 */
template <class T_return, class T_functor>
struct retype_return_functor : public adapts<T_functor>
{
  template <LOOP(class T_arg%1=void, CALL_SIZE)>
  struct deduce_result_type
    { typedef T_return type; };
  typedef T_return result_type;

  T_return operator()();

FOR(1,CALL_SIZE,[[RETYPE_RETURN_OPERATOR(%1)]])dnl
  retype_return_functor() {}

  /** Constructs a retype_return_functor object that perform a C-style cast on the return value of the passed functor.
   * @param _A_functor Functor to invoke from operator()().
   */
  explicit retype_return_functor(_R_(T_functor) _A_functor)
    : adapts<T_functor>(_A_functor)
    {}
};

template <class T_return, class T_functor>
T_return retype_return_functor<T_return, T_functor>::operator()()
  { return T_return(this->functor_()); }


/** Adaptor that perform a C-style cast on the return value of a functor.
 * This template specialization is for a void return. It drops the return value of the functor it invokes.
 * Use the convenience function sigc::hide_return() to create an instance of sigc::retype_return_functor<void>.
 *
 * @ingroup retype
 */
/* The void specialization needed because of explicit cast to T_return.
 */
template <class T_functor>
struct retype_return_functor<void, T_functor> : public adapts<T_functor>
{
  template <LOOP(class T_arg%1=void, CALL_SIZE)>
  struct deduce_result_type
    { typedef void type; };
  typedef void result_type;

  void operator()();

FOR(1,CALL_SIZE,[[RETYPE_RETURN_VOID_OPERATOR(%1)]])dnl
  retype_return_functor() {}
  retype_return_functor(_R_(T_functor) _A_functor)
    : adapts<T_functor>(_A_functor)
    {}
};

template <class T_functor>
void retype_return_functor<void, T_functor>::operator()()
  { this->functor_(); }

  
//template specialization of visit_each<>(action, functor):
/** Performs a functor on each of the targets of a functor.
 * The function overload for sigc::retype_return_functor performs a functor on the
 * functor stored in the sigc::retype_return_functor object.
 *
 * @ingroup retype
 */
template <class T_action, class T_return, class T_functor>
void visit_each(const T_action& _A_action,
                const retype_return_functor<T_return, T_functor>& _A_target)
{
  visit_each(_A_action, _A_target.functor_);
}


/** Creates an adaptor of type sigc::retype_return_functor which performs a C-style cast on the return value of the passed functor.
 * The template argument @e T_return specifies the target type of the cast.
 *
 * @param _A_functor Functor that should be wrapped.
 * @return Adaptor that executes @e _A_functor performing a C-style casts on the return value.
 *
 * @ingroup retype
 */
template <class T_return, class T_functor>
inline retype_return_functor<T_return, T_functor>
retype_return(const T_functor& _A_functor)
  { return retype_return_functor<T_return, T_functor>(_A_functor); }

/** Creates an adaptor of type sigc::retype_return_functor which drops the return value of the passed functor.
 *
 * @param _A_functor Functor that should be wrapped.
 * @return Adaptor that executes @e _A_functor dropping its return value.
 *
 * @ingroup hide
 */
template <class T_functor>
inline retype_return_functor<void, T_functor>
hide_return(const T_functor& _A_functor)
  { return retype_return_functor<void, T_functor>(_A_functor); }

} /* namespace sigc */
