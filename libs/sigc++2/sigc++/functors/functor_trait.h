// -*- c++ -*-
/* Do not edit! -- generated file */
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

*/
#ifndef _SIGC_FUNCTORS_MACROS_FUNCTOR_TRAITHM4_
#define _SIGC_FUNCTORS_MACROS_FUNCTOR_TRAITHM4_
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
template <class T_return> class pointer_functor0;
template <class T_return>
struct functor_trait<T_return (*)(), false>
{
  typedef T_return result_type;
  typedef pointer_functor0<T_return> functor_type;
};

template <class T_arg1, class T_return> class pointer_functor1;
template <class T_arg1, class T_return>
struct functor_trait<T_return (*)(T_arg1), false>
{
  typedef T_return result_type;
  typedef pointer_functor1<T_arg1, T_return> functor_type;
};

template <class T_arg1,class T_arg2, class T_return> class pointer_functor2;
template <class T_arg1,class T_arg2, class T_return>
struct functor_trait<T_return (*)(T_arg1,T_arg2), false>
{
  typedef T_return result_type;
  typedef pointer_functor2<T_arg1,T_arg2, T_return> functor_type;
};

template <class T_arg1,class T_arg2,class T_arg3, class T_return> class pointer_functor3;
template <class T_arg1,class T_arg2,class T_arg3, class T_return>
struct functor_trait<T_return (*)(T_arg1,T_arg2,T_arg3), false>
{
  typedef T_return result_type;
  typedef pointer_functor3<T_arg1,T_arg2,T_arg3, T_return> functor_type;
};

template <class T_arg1,class T_arg2,class T_arg3,class T_arg4, class T_return> class pointer_functor4;
template <class T_arg1,class T_arg2,class T_arg3,class T_arg4, class T_return>
struct functor_trait<T_return (*)(T_arg1,T_arg2,T_arg3,T_arg4), false>
{
  typedef T_return result_type;
  typedef pointer_functor4<T_arg1,T_arg2,T_arg3,T_arg4, T_return> functor_type;
};

template <class T_arg1,class T_arg2,class T_arg3,class T_arg4,class T_arg5, class T_return> class pointer_functor5;
template <class T_arg1,class T_arg2,class T_arg3,class T_arg4,class T_arg5, class T_return>
struct functor_trait<T_return (*)(T_arg1,T_arg2,T_arg3,T_arg4,T_arg5), false>
{
  typedef T_return result_type;
  typedef pointer_functor5<T_arg1,T_arg2,T_arg3,T_arg4,T_arg5, T_return> functor_type;
};

template <class T_arg1,class T_arg2,class T_arg3,class T_arg4,class T_arg5,class T_arg6, class T_return> class pointer_functor6;
template <class T_arg1,class T_arg2,class T_arg3,class T_arg4,class T_arg5,class T_arg6, class T_return>
struct functor_trait<T_return (*)(T_arg1,T_arg2,T_arg3,T_arg4,T_arg5,T_arg6), false>
{
  typedef T_return result_type;
  typedef pointer_functor6<T_arg1,T_arg2,T_arg3,T_arg4,T_arg5,T_arg6, T_return> functor_type;
};

template <class T_arg1,class T_arg2,class T_arg3,class T_arg4,class T_arg5,class T_arg6,class T_arg7, class T_return> class pointer_functor7;
template <class T_arg1,class T_arg2,class T_arg3,class T_arg4,class T_arg5,class T_arg6,class T_arg7, class T_return>
struct functor_trait<T_return (*)(T_arg1,T_arg2,T_arg3,T_arg4,T_arg5,T_arg6,T_arg7), false>
{
  typedef T_return result_type;
  typedef pointer_functor7<T_arg1,T_arg2,T_arg3,T_arg4,T_arg5,T_arg6,T_arg7, T_return> functor_type;
};


template <class T_return, class T_obj> class mem_functor0;
template <class T_return, class T_obj> class const_mem_functor0;
template <class T_return, class T_obj>
struct functor_trait<T_return (T_obj::*)(), false>
{
  typedef T_return result_type;
  typedef mem_functor0<T_return, T_obj> functor_type;
};
template <class T_return, class T_obj>
struct functor_trait<T_return (T_obj::*)() const, false>
{
  typedef T_return result_type;
  typedef const_mem_functor0<T_return, T_obj> functor_type;
};

template <class T_arg1, class T_return, class T_obj> class mem_functor1;
template <class T_arg1, class T_return, class T_obj> class const_mem_functor1;
template <class T_arg1, class T_return, class T_obj>
struct functor_trait<T_return (T_obj::*)(T_arg1), false>
{
  typedef T_return result_type;
  typedef mem_functor1<T_arg1, T_return, T_obj> functor_type;
};
template <class T_arg1, class T_return, class T_obj>
struct functor_trait<T_return (T_obj::*)(T_arg1) const, false>
{
  typedef T_return result_type;
  typedef const_mem_functor1<T_arg1, T_return, T_obj> functor_type;
};

template <class T_arg1,class T_arg2, class T_return, class T_obj> class mem_functor2;
template <class T_arg1,class T_arg2, class T_return, class T_obj> class const_mem_functor2;
template <class T_arg1,class T_arg2, class T_return, class T_obj>
struct functor_trait<T_return (T_obj::*)(T_arg1,T_arg2), false>
{
  typedef T_return result_type;
  typedef mem_functor2<T_arg1,T_arg2, T_return, T_obj> functor_type;
};
template <class T_arg1,class T_arg2, class T_return, class T_obj>
struct functor_trait<T_return (T_obj::*)(T_arg1,T_arg2) const, false>
{
  typedef T_return result_type;
  typedef const_mem_functor2<T_arg1,T_arg2, T_return, T_obj> functor_type;
};

template <class T_arg1,class T_arg2,class T_arg3, class T_return, class T_obj> class mem_functor3;
template <class T_arg1,class T_arg2,class T_arg3, class T_return, class T_obj> class const_mem_functor3;
template <class T_arg1,class T_arg2,class T_arg3, class T_return, class T_obj>
struct functor_trait<T_return (T_obj::*)(T_arg1,T_arg2,T_arg3), false>
{
  typedef T_return result_type;
  typedef mem_functor3<T_arg1,T_arg2,T_arg3, T_return, T_obj> functor_type;
};
template <class T_arg1,class T_arg2,class T_arg3, class T_return, class T_obj>
struct functor_trait<T_return (T_obj::*)(T_arg1,T_arg2,T_arg3) const, false>
{
  typedef T_return result_type;
  typedef const_mem_functor3<T_arg1,T_arg2,T_arg3, T_return, T_obj> functor_type;
};

template <class T_arg1,class T_arg2,class T_arg3,class T_arg4, class T_return, class T_obj> class mem_functor4;
template <class T_arg1,class T_arg2,class T_arg3,class T_arg4, class T_return, class T_obj> class const_mem_functor4;
template <class T_arg1,class T_arg2,class T_arg3,class T_arg4, class T_return, class T_obj>
struct functor_trait<T_return (T_obj::*)(T_arg1,T_arg2,T_arg3,T_arg4), false>
{
  typedef T_return result_type;
  typedef mem_functor4<T_arg1,T_arg2,T_arg3,T_arg4, T_return, T_obj> functor_type;
};
template <class T_arg1,class T_arg2,class T_arg3,class T_arg4, class T_return, class T_obj>
struct functor_trait<T_return (T_obj::*)(T_arg1,T_arg2,T_arg3,T_arg4) const, false>
{
  typedef T_return result_type;
  typedef const_mem_functor4<T_arg1,T_arg2,T_arg3,T_arg4, T_return, T_obj> functor_type;
};

template <class T_arg1,class T_arg2,class T_arg3,class T_arg4,class T_arg5, class T_return, class T_obj> class mem_functor5;
template <class T_arg1,class T_arg2,class T_arg3,class T_arg4,class T_arg5, class T_return, class T_obj> class const_mem_functor5;
template <class T_arg1,class T_arg2,class T_arg3,class T_arg4,class T_arg5, class T_return, class T_obj>
struct functor_trait<T_return (T_obj::*)(T_arg1,T_arg2,T_arg3,T_arg4,T_arg5), false>
{
  typedef T_return result_type;
  typedef mem_functor5<T_arg1,T_arg2,T_arg3,T_arg4,T_arg5, T_return, T_obj> functor_type;
};
template <class T_arg1,class T_arg2,class T_arg3,class T_arg4,class T_arg5, class T_return, class T_obj>
struct functor_trait<T_return (T_obj::*)(T_arg1,T_arg2,T_arg3,T_arg4,T_arg5) const, false>
{
  typedef T_return result_type;
  typedef const_mem_functor5<T_arg1,T_arg2,T_arg3,T_arg4,T_arg5, T_return, T_obj> functor_type;
};

template <class T_arg1,class T_arg2,class T_arg3,class T_arg4,class T_arg5,class T_arg6, class T_return, class T_obj> class mem_functor6;
template <class T_arg1,class T_arg2,class T_arg3,class T_arg4,class T_arg5,class T_arg6, class T_return, class T_obj> class const_mem_functor6;
template <class T_arg1,class T_arg2,class T_arg3,class T_arg4,class T_arg5,class T_arg6, class T_return, class T_obj>
struct functor_trait<T_return (T_obj::*)(T_arg1,T_arg2,T_arg3,T_arg4,T_arg5,T_arg6), false>
{
  typedef T_return result_type;
  typedef mem_functor6<T_arg1,T_arg2,T_arg3,T_arg4,T_arg5,T_arg6, T_return, T_obj> functor_type;
};
template <class T_arg1,class T_arg2,class T_arg3,class T_arg4,class T_arg5,class T_arg6, class T_return, class T_obj>
struct functor_trait<T_return (T_obj::*)(T_arg1,T_arg2,T_arg3,T_arg4,T_arg5,T_arg6) const, false>
{
  typedef T_return result_type;
  typedef const_mem_functor6<T_arg1,T_arg2,T_arg3,T_arg4,T_arg5,T_arg6, T_return, T_obj> functor_type;
};

template <class T_arg1,class T_arg2,class T_arg3,class T_arg4,class T_arg5,class T_arg6,class T_arg7, class T_return, class T_obj> class mem_functor7;
template <class T_arg1,class T_arg2,class T_arg3,class T_arg4,class T_arg5,class T_arg6,class T_arg7, class T_return, class T_obj> class const_mem_functor7;
template <class T_arg1,class T_arg2,class T_arg3,class T_arg4,class T_arg5,class T_arg6,class T_arg7, class T_return, class T_obj>
struct functor_trait<T_return (T_obj::*)(T_arg1,T_arg2,T_arg3,T_arg4,T_arg5,T_arg6,T_arg7), false>
{
  typedef T_return result_type;
  typedef mem_functor7<T_arg1,T_arg2,T_arg3,T_arg4,T_arg5,T_arg6,T_arg7, T_return, T_obj> functor_type;
};
template <class T_arg1,class T_arg2,class T_arg3,class T_arg4,class T_arg5,class T_arg6,class T_arg7, class T_return, class T_obj>
struct functor_trait<T_return (T_obj::*)(T_arg1,T_arg2,T_arg3,T_arg4,T_arg5,T_arg6,T_arg7) const, false>
{
  typedef T_return result_type;
  typedef const_mem_functor7<T_arg1,T_arg2,T_arg3,T_arg4,T_arg5,T_arg6,T_arg7, T_return, T_obj> functor_type;
};



} /* namespace sigc */
#endif /* _SIGC_FUNCTORS_MACROS_FUNCTOR_TRAITHM4_ */
