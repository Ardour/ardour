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

define([EXCEPTION_CATCH_OPERATOR],[dnl
  template <LOOP(class T_arg%1, $1)>
  typename deduce_result_type<LOOP(T_arg%1,$1)>::type
  operator()(LOOP(T_arg%1 _A_a%1, $1))
    { 
      try
        {
          return this->functor_.SIGC_WORKAROUND_OPERATOR_PARENTHESES<LOOP(_P_(T_arg%1), $1)>
            (LOOP(_A_a%1, $1));
        } 
      catch (...)
        { return catcher_(); }
    }

])

divert(0)dnl
__FIREWALL__
#include <sigc++/adaptors/adaptor_trait.h>

namespace sigc {

/*
   functor adaptor:  exception_catch(functor, catcher)

   usage:


   Future directions:
     The catcher should be told what type of return it needs to
   return for multiple type functors,  to do this the user
   will need to derive from catcher_base.
*/
/** @defgroup exception_catch exception_catch()
 * sigc::exception_catch() catches an exception thrown from within 
 * the wrapped functor and directs it to a catcher functor.
 * This catcher can then rethrow the exception and catch it with the proper type.
 *
 * Note that the catcher is expected to return the same type
 * as the wrapped functor so that normal flow can continue.
 *
 * Catchers can be cascaded to catch multiple types because uncaught
 * rethrown exceptions proceed to the next catcher adaptor.
 *
 * @par Examples:
 *   @code
 *   struct my_catch
 *   {
 *     int operator()()
 *     {
 *       try { throw; }
 *       catch (std::range_error e) // catch what types we know
 *         { std::cerr << "caught " << e.what() << std::endl; }
 *       return 1;
 *     }
 *   }
 *   int foo(); // throws std::range_error
 *   sigc::exception_catch(&foo, my_catch())();
 *   @endcode
 *
 * The functor sigc::execption_catch() returns can be passed into
 * sigc::signal::connect() directly.
 *
 * @par Example:
 *   @code
 *   sigc::signal<int> some_signal;
 *   some_signal.connect(sigc::exception_catch(&foo, my_catch));
 *   @endcode
 *
 * @ingroup adaptors
 */

template <class T_functor, class T_catcher, class T_return = typename adapts<T_functor>::result_type>
struct exception_catch_functor : public adapts<T_functor>
{
  typedef typename adapts<T_functor>::adaptor_type adaptor_type;

  template <LOOP(class T_arg%1=void, CALL_SIZE)>
  struct deduce_result_type
    { typedef typename adaptor_type::template deduce_result_type<LOOP(_P_(T_arg%1),CALL_SIZE)>::type type; };
  typedef T_return result_type;

  result_type
  operator()();

FOR(1,CALL_SIZE,[[EXCEPTION_CATCH_OPERATOR(%1)]])dnl
  exception_catch_functor(const T_functor& _A_func,
                          const T_catcher& _A_catcher)
    : adapts<T_functor>(_A_func), catcher_(_A_catcher)
    {}

  T_catcher catcher_; 
};

template <class T_functor, class T_catcher, class T_return>
typename exception_catch_functor<T_functor, T_catcher, T_return>::result_type
exception_catch_functor<T_functor, T_catcher, T_return>::operator()()
  { 
    try
      { return this->functor_(); }
    catch (...)
      { return catcher_(); }
  }

// void specialization
template <class T_functor, class T_catcher>
struct exception_catch_functor<T_functor, T_catcher, void> : public adapts<T_functor>
{
  typedef void result_type;
  typedef typename adapts<T_functor>::adaptor_type adaptor_type;

  void
  operator()();

FOR(1,CALL_SIZE,[[EXCEPTION_CATCH_OPERATOR(%1)]])dnl
  exception_catch_functor() {}
  exception_catch_functor(const T_functor& _A_func,
                          const T_catcher& _A_catcher)
    : adapts<T_functor>(_A_func), catcher_(_A_catcher)
    {}
  ~exception_catch_functor() {}

    T_catcher catcher_; 
};

template <class T_functor, class T_catcher>
void exception_catch_functor<T_functor, T_catcher, void>::operator()()
  { 
    try
      { this->functor_(); } // I don't understand why void return doesn't work here (Martin)
    catch (...)
      { this->catcher_(); }
  }

  
//template specialization of visit_each<>(action, functor):
template <class T_action, class T_functor, class T_catcher, class T_return>
void visit_each(const T_action& _A_action,
                const exception_catch_functor<T_functor, T_catcher, T_return>& _A_target)
{
  visit_each(_A_action, _A_target.functor_);
  visit_each(_A_action, _A_target.catcher_);
}


template <class T_functor, class T_catcher>
inline exception_catch_functor<T_functor, T_catcher>
exception_catch(const T_functor& _A_func, const T_catcher& _A_catcher)
  { return exception_catch_functor<T_functor, T_catcher>(_A_func, _A_catcher); }

} /* namespace sigc */
