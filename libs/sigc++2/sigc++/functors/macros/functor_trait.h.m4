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

define([FUNCTOR_PTR_FUN],[dnl
template <LIST(LOOP(class T_arg%1, $1), class T_return)> class pointer_functor$1;
template <LIST(LOOP(class T_arg%1, $1), class T_return)>
struct functor_trait<T_return (*)(LOOP(T_arg%1, $1)), false>
{
  typedef T_return result_type;
  typedef pointer_functor$1<LIST(LOOP(T_arg%1, $1), T_return)> functor_type;
};

])
define([FUNCTOR_MEM_FUN],[dnl
template <LIST(LOOP(class T_arg%1, $1), class T_return, class T_obj)> class mem_functor$1;
template <LIST(LOOP(class T_arg%1, $1), class T_return, class T_obj)> class const_mem_functor$1;
template <LIST(LOOP(class T_arg%1, $1), class T_return, class T_obj)>
struct functor_trait<T_return (T_obj::*)(LOOP(T_arg%1, $1)), false>
{
  typedef T_return result_type;
  typedef mem_functor$1<LIST(LOOP(T_arg%1, $1), T_return, T_obj)> functor_type;
};
template <LIST(LOOP(class T_arg%1, $1), class T_return, class T_obj)>
struct functor_trait<T_return (T_obj::*)(LOOP(T_arg%1, $1)) const, false>
{
  typedef T_return result_type;
  typedef const_mem_functor$1<LIST(LOOP(T_arg%1, $1), T_return, T_obj)> functor_type;
};

])

divert(0)dnl
/*
  Trait functor_trait<functor>:

  This trait allows the user to specific what is the return type
  of any type. It has been overloaded to detect the return type and
  the functor version of function pointers and class methods as well.

  To populate the return type of user defined and third party functors
  use the macro SIGC_FUNCTOR_TRAIT(T_functor,T_return) in
  namespace sigc. Multi-type functors are only partly supported.
  Try specifying the return type of the functor's operator()() overload.

  Alternatively, you can derive your functors from functor_base and
  place "typedef T_return result_type;" in the class definition.

  Use SIGC_FUNCTORS_HAVE_RESULT_TYPE if you want sigc++ to assume that
  result_type is defined in all user defined or 3rd-party functors
  (except those you specify a return type explicitly with SIGC_FUNCTOR_TRAIT()).

dnl 01.11.2003: Completely removed support for typeof() since it is non-standard!
dnl   You might get away without these conventions if your compiler supports
dnl   typeof() and if you don't happen to use the operator()() overload of
dnl   sigc++'s adaptors in your program.
dnl 
*/
__FIREWALL__
#include <sigc++/type_traits.h>


namespace sigc {

/** nil struct type.
 * The nil struct type is used as default template argument in the
 * unnumbered sigc::signal and sigc::slot templates.
 *
 * @ingroup signal
 * @ingroup slot
 */
struct nil;


/** @defgroup functors Functors
 * Functors are copyable types that define operator()().
 *
 * Types that define operator()() overloads with different return types are referred to
 * as multi-type functors. Multi-type functors are only partly supported in libsigc++.
 *
 * Closures are functors that store all information needed to invoke a callback from operator()().
 *
 * Adaptors are functors that alter the signature of a functor's operator()().
 *
 * libsigc++ defines numerous functors, closures and adaptors.
 * Since libsigc++ is a callback libaray, most functors are also closures.
 * The documentation doesn't distinguish between functors and closures.
 *
 * The basic functor types libsigc++ provides are created with ptr_fun() and mem_fun()
 * and can be converted into slots implicitly.
 * The set of adaptors that ships with libsigc++ is documented in the equally named module. 
 */

/** A hint to the compiler.
 * All functors which define @p result_type should publically inherit from this hint.
 *
 * @ingroup functors
 */
struct functor_base {};


template <class T_functor, bool I_derives_functor_base=is_base_and_derived<functor_base,T_functor>::value>
struct functor_trait
{
  typedef void result_type;
  typedef T_functor functor_type;
};

template <class T_functor>
struct functor_trait<T_functor,true>
{
  typedef typename T_functor::result_type result_type;
  typedef T_functor functor_type;
};

/** If you want to mix functors from a different library with libsigc++ and
 * these functors define @p result_type simply use this macro inside namespace sigc like so:
 * @code
 * namespace sigc { SIGC_FUNCTORS_HAVE_RESULT_TYPE }
 * @endcode
 *
 * @ingroup functors
 */
#define SIGC_FUNCTORS_HAVE_RESULT_TYPE                 \
template <class T_functor>                             \
struct functor_trait<T_functor,false>                  \
{                                                      \
  typedef typename T_functor::result_type result_type; \
  typedef T_functor functor_type;                      \
};

/** If you want to mix functors from a different library with libsigc++ and
 * these functors don't define @p result_type use this macro inside namespace sigc
 * to expose the return type of the functors like so:
 * @code
 * namespace sigc {
 *   SIGC_FUNCTOR_TRAIT(first_functor_type, return_type_of_first_functor_type)
 *   SIGC_FUNCTOR_TRAIT(second_functor_type, return_type_of_second_functor_type)
 *   ...
 * }
 * @endcode
 *
 * @ingroup functors
 */
#define SIGC_FUNCTOR_TRAIT(T_functor,T_return) \
template <>                                    \
struct functor_trait<T_functor,false>          \
{                                              \
  typedef T_return result_type;                \
  typedef T_functor functor_type;              \
};

// detect the return type and the functor version of non-functor types.
FOR(0,CALL_SIZE,[[FUNCTOR_PTR_FUN(%1)]])
FOR(0,CALL_SIZE,[[FUNCTOR_MEM_FUN(%1)]])

} /* namespace sigc */
