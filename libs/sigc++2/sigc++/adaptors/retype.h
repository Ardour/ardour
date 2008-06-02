// -*- c++ -*-
/* Do not edit! -- generated file */
#ifndef _SIGC_ADAPTORS_MACROS_RETYPEHM4_
#define _SIGC_ADAPTORS_MACROS_RETYPEHM4_
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
 * - @e T_functor Type of the functor to wrap.
 * - @e T_type1 Type of @e T_functor's 1th argument.
 * - @e T_type2 Type of @e T_functor's 2th argument.
 * - @e T_type3 Type of @e T_functor's 3th argument.
 * - @e T_type4 Type of @e T_functor's 4th argument.
 * - @e T_type5 Type of @e T_functor's 5th argument.
 * - @e T_type6 Type of @e T_functor's 6th argument.
 * - @e T_type7 Type of @e T_functor's 7th argument.
 *
 * @ingroup retype
 */
template <class T_functor, class T_type1=nil,class T_type2=nil,class T_type3=nil,class T_type4=nil,class T_type5=nil,class T_type6=nil,class T_type7=nil>
struct retype_functor
  : public adapts<T_functor>
{
  typedef typename adapts<T_functor>::adaptor_type adaptor_type;

  template <class T_arg1=void,class T_arg2=void,class T_arg3=void,class T_arg4=void,class T_arg5=void,class T_arg6=void,class T_arg7=void>
  struct deduce_result_type
    { typedef typename adaptor_type::template deduce_result_type<typename type_trait<T_arg1>::pass,typename type_trait<T_arg2>::pass,typename type_trait<T_arg3>::pass,typename type_trait<T_arg4>::pass,typename type_trait<T_arg5>::pass,typename type_trait<T_arg6>::pass,typename type_trait<T_arg7>::pass>::type type; };
  typedef typename adapts<T_functor>::result_type result_type;

  result_type operator()();
    
  template <class T_arg1>
  typename deduce_result_type<T_arg1>::type
  operator()(T_arg1 _A_a1)
    { return this->functor_.SIGC_WORKAROUND_OPERATOR_PARENTHESES<typename type_trait<T_type1>::take>
        (static_cast<T_type1>(_A_a1));
    }

  #ifndef SIGC_TEMPLATE_SPECIALIZATION_OPERATOR_OVERLOAD
  template <class T_arg1>
  typename deduce_result_type<T_arg1>::type
  sun_forte_workaround(T_arg1 _A_a1)
    { return this->functor_.SIGC_WORKAROUND_OPERATOR_PARENTHESES<typename type_trait<T_type1>::take>
        (static_cast<T_type1>(_A_a1));
    }
  #endif

  template <class T_arg1,class T_arg2>
  typename deduce_result_type<T_arg1,T_arg2>::type
  operator()(T_arg1 _A_a1,T_arg2 _A_a2)
    { return this->functor_.SIGC_WORKAROUND_OPERATOR_PARENTHESES<typename type_trait<T_type1>::take,typename type_trait<T_type2>::take>
        (static_cast<T_type1>(_A_a1),static_cast<T_type2>(_A_a2));
    }

  #ifndef SIGC_TEMPLATE_SPECIALIZATION_OPERATOR_OVERLOAD
  template <class T_arg1,class T_arg2>
  typename deduce_result_type<T_arg1,T_arg2>::type
  sun_forte_workaround(T_arg1 _A_a1,T_arg2 _A_a2)
    { return this->functor_.SIGC_WORKAROUND_OPERATOR_PARENTHESES<typename type_trait<T_type1>::take,typename type_trait<T_type2>::take>
        (static_cast<T_type1>(_A_a1),static_cast<T_type2>(_A_a2));
    }
  #endif

  template <class T_arg1,class T_arg2,class T_arg3>
  typename deduce_result_type<T_arg1,T_arg2,T_arg3>::type
  operator()(T_arg1 _A_a1,T_arg2 _A_a2,T_arg3 _A_a3)
    { return this->functor_.SIGC_WORKAROUND_OPERATOR_PARENTHESES<typename type_trait<T_type1>::take,typename type_trait<T_type2>::take,typename type_trait<T_type3>::take>
        (static_cast<T_type1>(_A_a1),static_cast<T_type2>(_A_a2),static_cast<T_type3>(_A_a3));
    }

  #ifndef SIGC_TEMPLATE_SPECIALIZATION_OPERATOR_OVERLOAD
  template <class T_arg1,class T_arg2,class T_arg3>
  typename deduce_result_type<T_arg1,T_arg2,T_arg3>::type
  sun_forte_workaround(T_arg1 _A_a1,T_arg2 _A_a2,T_arg3 _A_a3)
    { return this->functor_.SIGC_WORKAROUND_OPERATOR_PARENTHESES<typename type_trait<T_type1>::take,typename type_trait<T_type2>::take,typename type_trait<T_type3>::take>
        (static_cast<T_type1>(_A_a1),static_cast<T_type2>(_A_a2),static_cast<T_type3>(_A_a3));
    }
  #endif

  template <class T_arg1,class T_arg2,class T_arg3,class T_arg4>
  typename deduce_result_type<T_arg1,T_arg2,T_arg3,T_arg4>::type
  operator()(T_arg1 _A_a1,T_arg2 _A_a2,T_arg3 _A_a3,T_arg4 _A_a4)
    { return this->functor_.SIGC_WORKAROUND_OPERATOR_PARENTHESES<typename type_trait<T_type1>::take,typename type_trait<T_type2>::take,typename type_trait<T_type3>::take,typename type_trait<T_type4>::take>
        (static_cast<T_type1>(_A_a1),static_cast<T_type2>(_A_a2),static_cast<T_type3>(_A_a3),static_cast<T_type4>(_A_a4));
    }

  #ifndef SIGC_TEMPLATE_SPECIALIZATION_OPERATOR_OVERLOAD
  template <class T_arg1,class T_arg2,class T_arg3,class T_arg4>
  typename deduce_result_type<T_arg1,T_arg2,T_arg3,T_arg4>::type
  sun_forte_workaround(T_arg1 _A_a1,T_arg2 _A_a2,T_arg3 _A_a3,T_arg4 _A_a4)
    { return this->functor_.SIGC_WORKAROUND_OPERATOR_PARENTHESES<typename type_trait<T_type1>::take,typename type_trait<T_type2>::take,typename type_trait<T_type3>::take,typename type_trait<T_type4>::take>
        (static_cast<T_type1>(_A_a1),static_cast<T_type2>(_A_a2),static_cast<T_type3>(_A_a3),static_cast<T_type4>(_A_a4));
    }
  #endif

  template <class T_arg1,class T_arg2,class T_arg3,class T_arg4,class T_arg5>
  typename deduce_result_type<T_arg1,T_arg2,T_arg3,T_arg4,T_arg5>::type
  operator()(T_arg1 _A_a1,T_arg2 _A_a2,T_arg3 _A_a3,T_arg4 _A_a4,T_arg5 _A_a5)
    { return this->functor_.SIGC_WORKAROUND_OPERATOR_PARENTHESES<typename type_trait<T_type1>::take,typename type_trait<T_type2>::take,typename type_trait<T_type3>::take,typename type_trait<T_type4>::take,typename type_trait<T_type5>::take>
        (static_cast<T_type1>(_A_a1),static_cast<T_type2>(_A_a2),static_cast<T_type3>(_A_a3),static_cast<T_type4>(_A_a4),static_cast<T_type5>(_A_a5));
    }

  #ifndef SIGC_TEMPLATE_SPECIALIZATION_OPERATOR_OVERLOAD
  template <class T_arg1,class T_arg2,class T_arg3,class T_arg4,class T_arg5>
  typename deduce_result_type<T_arg1,T_arg2,T_arg3,T_arg4,T_arg5>::type
  sun_forte_workaround(T_arg1 _A_a1,T_arg2 _A_a2,T_arg3 _A_a3,T_arg4 _A_a4,T_arg5 _A_a5)
    { return this->functor_.SIGC_WORKAROUND_OPERATOR_PARENTHESES<typename type_trait<T_type1>::take,typename type_trait<T_type2>::take,typename type_trait<T_type3>::take,typename type_trait<T_type4>::take,typename type_trait<T_type5>::take>
        (static_cast<T_type1>(_A_a1),static_cast<T_type2>(_A_a2),static_cast<T_type3>(_A_a3),static_cast<T_type4>(_A_a4),static_cast<T_type5>(_A_a5));
    }
  #endif

  template <class T_arg1,class T_arg2,class T_arg3,class T_arg4,class T_arg5,class T_arg6>
  typename deduce_result_type<T_arg1,T_arg2,T_arg3,T_arg4,T_arg5,T_arg6>::type
  operator()(T_arg1 _A_a1,T_arg2 _A_a2,T_arg3 _A_a3,T_arg4 _A_a4,T_arg5 _A_a5,T_arg6 _A_a6)
    { return this->functor_.SIGC_WORKAROUND_OPERATOR_PARENTHESES<typename type_trait<T_type1>::take,typename type_trait<T_type2>::take,typename type_trait<T_type3>::take,typename type_trait<T_type4>::take,typename type_trait<T_type5>::take,typename type_trait<T_type6>::take>
        (static_cast<T_type1>(_A_a1),static_cast<T_type2>(_A_a2),static_cast<T_type3>(_A_a3),static_cast<T_type4>(_A_a4),static_cast<T_type5>(_A_a5),static_cast<T_type6>(_A_a6));
    }

  #ifndef SIGC_TEMPLATE_SPECIALIZATION_OPERATOR_OVERLOAD
  template <class T_arg1,class T_arg2,class T_arg3,class T_arg4,class T_arg5,class T_arg6>
  typename deduce_result_type<T_arg1,T_arg2,T_arg3,T_arg4,T_arg5,T_arg6>::type
  sun_forte_workaround(T_arg1 _A_a1,T_arg2 _A_a2,T_arg3 _A_a3,T_arg4 _A_a4,T_arg5 _A_a5,T_arg6 _A_a6)
    { return this->functor_.SIGC_WORKAROUND_OPERATOR_PARENTHESES<typename type_trait<T_type1>::take,typename type_trait<T_type2>::take,typename type_trait<T_type3>::take,typename type_trait<T_type4>::take,typename type_trait<T_type5>::take,typename type_trait<T_type6>::take>
        (static_cast<T_type1>(_A_a1),static_cast<T_type2>(_A_a2),static_cast<T_type3>(_A_a3),static_cast<T_type4>(_A_a4),static_cast<T_type5>(_A_a5),static_cast<T_type6>(_A_a6));
    }
  #endif

  template <class T_arg1,class T_arg2,class T_arg3,class T_arg4,class T_arg5,class T_arg6,class T_arg7>
  typename deduce_result_type<T_arg1,T_arg2,T_arg3,T_arg4,T_arg5,T_arg6,T_arg7>::type
  operator()(T_arg1 _A_a1,T_arg2 _A_a2,T_arg3 _A_a3,T_arg4 _A_a4,T_arg5 _A_a5,T_arg6 _A_a6,T_arg7 _A_a7)
    { return this->functor_.SIGC_WORKAROUND_OPERATOR_PARENTHESES<typename type_trait<T_type1>::take,typename type_trait<T_type2>::take,typename type_trait<T_type3>::take,typename type_trait<T_type4>::take,typename type_trait<T_type5>::take,typename type_trait<T_type6>::take,typename type_trait<T_type7>::take>
        (static_cast<T_type1>(_A_a1),static_cast<T_type2>(_A_a2),static_cast<T_type3>(_A_a3),static_cast<T_type4>(_A_a4),static_cast<T_type5>(_A_a5),static_cast<T_type6>(_A_a6),static_cast<T_type7>(_A_a7));
    }

  #ifndef SIGC_TEMPLATE_SPECIALIZATION_OPERATOR_OVERLOAD
  template <class T_arg1,class T_arg2,class T_arg3,class T_arg4,class T_arg5,class T_arg6,class T_arg7>
  typename deduce_result_type<T_arg1,T_arg2,T_arg3,T_arg4,T_arg5,T_arg6,T_arg7>::type
  sun_forte_workaround(T_arg1 _A_a1,T_arg2 _A_a2,T_arg3 _A_a3,T_arg4 _A_a4,T_arg5 _A_a5,T_arg6 _A_a6,T_arg7 _A_a7)
    { return this->functor_.SIGC_WORKAROUND_OPERATOR_PARENTHESES<typename type_trait<T_type1>::take,typename type_trait<T_type2>::take,typename type_trait<T_type3>::take,typename type_trait<T_type4>::take,typename type_trait<T_type5>::take,typename type_trait<T_type6>::take,typename type_trait<T_type7>::take>
        (static_cast<T_type1>(_A_a1),static_cast<T_type2>(_A_a2),static_cast<T_type3>(_A_a3),static_cast<T_type4>(_A_a4),static_cast<T_type5>(_A_a5),static_cast<T_type6>(_A_a6),static_cast<T_type7>(_A_a7));
    }
  #endif


  /** Constructs a retype_functor object that performs C-style casts on the parameters passed on to the functor.
   * @param _A_functor Functor to invoke from operator()().
   */
  explicit retype_functor(typename type_trait<T_functor>::take _A_functor)
    : adapts<T_functor>(_A_functor)
    {}
};

template <class T_functor, class T_type1,class T_type2,class T_type3,class T_type4,class T_type5,class T_type6,class T_type7>
typename retype_functor<T_functor, T_type1,T_type2,T_type3,T_type4,T_type5,T_type6,T_type7>::result_type
retype_functor<T_functor, T_type1,T_type2,T_type3,T_type4,T_type5,T_type6,T_type7>::operator()()
  { return this->functor_(); }

  
//template specialization of visit_each<>(action, functor):
/** Performs a functor on each of the targets of a functor.
 * The function overload for sigc::retype_functor performs a functor on the
 * functor stored in the sigc::retype_functor object.
 *
 * @ingroup retype
 */
template <class T_action, class T_functor, class T_type1,class T_type2,class T_type3,class T_type4,class T_type5,class T_type6,class T_type7>
void visit_each(const T_action& _A_action,
                const retype_functor<T_functor, T_type1,T_type2,T_type3,T_type4,T_type5,T_type6,T_type7>& _A_target)
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
template <class T_return, class T_arg1,class T_arg2,class T_arg3,class T_arg4,class T_arg5,class T_arg6,class T_arg7>
inline retype_functor<slot<T_return, T_arg1, T_arg2, T_arg3, T_arg4, T_arg5, T_arg6, T_arg7>, T_arg1,T_arg2,T_arg3,T_arg4,T_arg5,T_arg6,T_arg7 >
retype(const slot<T_return, T_arg1,T_arg2,T_arg3,T_arg4,T_arg5,T_arg6,T_arg7>& _A_functor)
{ return retype_functor<slot<T_return, T_arg1, T_arg2, T_arg3, T_arg4, T_arg5, T_arg6, T_arg7>, T_arg1,T_arg2,T_arg3,T_arg4,T_arg5,T_arg6,T_arg7 >
    (_A_functor); }


/** Creates an adaptor of type sigc::retype_functor which performs C-style casts on the parameters passed on to the functor.
 * This function template specialization works on sigc::pointer_functor.
 *
 * @param _A_functor Functor that should be wrapped.
 * @return Adaptor that executes @e _A_functor performing C-style casts on the paramters passed on.
 *
 * @ingroup retype
 */
template <class T_return>
inline retype_functor<pointer_functor0<T_return> >
retype(const pointer_functor0<T_return>& _A_functor)
{ return retype_functor<pointer_functor0<T_return> >
    (_A_functor); }

/** Creates an adaptor of type sigc::retype_functor which performs C-style casts on the parameters passed on to the functor.
 * This function template specialization works on sigc::pointer_functor.
 *
 * @param _A_functor Functor that should be wrapped.
 * @return Adaptor that executes @e _A_functor performing C-style casts on the paramters passed on.
 *
 * @ingroup retype
 */
template <class T_arg1, class T_return>
inline retype_functor<pointer_functor1<T_arg1, T_return>, T_arg1 >
retype(const pointer_functor1<T_arg1, T_return>& _A_functor)
{ return retype_functor<pointer_functor1<T_arg1, T_return>, T_arg1 >
    (_A_functor); }

/** Creates an adaptor of type sigc::retype_functor which performs C-style casts on the parameters passed on to the functor.
 * This function template specialization works on sigc::pointer_functor.
 *
 * @param _A_functor Functor that should be wrapped.
 * @return Adaptor that executes @e _A_functor performing C-style casts on the paramters passed on.
 *
 * @ingroup retype
 */
template <class T_arg1,class T_arg2, class T_return>
inline retype_functor<pointer_functor2<T_arg1, T_arg2, T_return>, T_arg1,T_arg2 >
retype(const pointer_functor2<T_arg1,T_arg2, T_return>& _A_functor)
{ return retype_functor<pointer_functor2<T_arg1, T_arg2, T_return>, T_arg1,T_arg2 >
    (_A_functor); }

/** Creates an adaptor of type sigc::retype_functor which performs C-style casts on the parameters passed on to the functor.
 * This function template specialization works on sigc::pointer_functor.
 *
 * @param _A_functor Functor that should be wrapped.
 * @return Adaptor that executes @e _A_functor performing C-style casts on the paramters passed on.
 *
 * @ingroup retype
 */
template <class T_arg1,class T_arg2,class T_arg3, class T_return>
inline retype_functor<pointer_functor3<T_arg1, T_arg2, T_arg3, T_return>, T_arg1,T_arg2,T_arg3 >
retype(const pointer_functor3<T_arg1,T_arg2,T_arg3, T_return>& _A_functor)
{ return retype_functor<pointer_functor3<T_arg1, T_arg2, T_arg3, T_return>, T_arg1,T_arg2,T_arg3 >
    (_A_functor); }

/** Creates an adaptor of type sigc::retype_functor which performs C-style casts on the parameters passed on to the functor.
 * This function template specialization works on sigc::pointer_functor.
 *
 * @param _A_functor Functor that should be wrapped.
 * @return Adaptor that executes @e _A_functor performing C-style casts on the paramters passed on.
 *
 * @ingroup retype
 */
template <class T_arg1,class T_arg2,class T_arg3,class T_arg4, class T_return>
inline retype_functor<pointer_functor4<T_arg1, T_arg2, T_arg3, T_arg4, T_return>, T_arg1,T_arg2,T_arg3,T_arg4 >
retype(const pointer_functor4<T_arg1,T_arg2,T_arg3,T_arg4, T_return>& _A_functor)
{ return retype_functor<pointer_functor4<T_arg1, T_arg2, T_arg3, T_arg4, T_return>, T_arg1,T_arg2,T_arg3,T_arg4 >
    (_A_functor); }

/** Creates an adaptor of type sigc::retype_functor which performs C-style casts on the parameters passed on to the functor.
 * This function template specialization works on sigc::pointer_functor.
 *
 * @param _A_functor Functor that should be wrapped.
 * @return Adaptor that executes @e _A_functor performing C-style casts on the paramters passed on.
 *
 * @ingroup retype
 */
template <class T_arg1,class T_arg2,class T_arg3,class T_arg4,class T_arg5, class T_return>
inline retype_functor<pointer_functor5<T_arg1, T_arg2, T_arg3, T_arg4, T_arg5, T_return>, T_arg1,T_arg2,T_arg3,T_arg4,T_arg5 >
retype(const pointer_functor5<T_arg1,T_arg2,T_arg3,T_arg4,T_arg5, T_return>& _A_functor)
{ return retype_functor<pointer_functor5<T_arg1, T_arg2, T_arg3, T_arg4, T_arg5, T_return>, T_arg1,T_arg2,T_arg3,T_arg4,T_arg5 >
    (_A_functor); }

/** Creates an adaptor of type sigc::retype_functor which performs C-style casts on the parameters passed on to the functor.
 * This function template specialization works on sigc::pointer_functor.
 *
 * @param _A_functor Functor that should be wrapped.
 * @return Adaptor that executes @e _A_functor performing C-style casts on the paramters passed on.
 *
 * @ingroup retype
 */
template <class T_arg1,class T_arg2,class T_arg3,class T_arg4,class T_arg5,class T_arg6, class T_return>
inline retype_functor<pointer_functor6<T_arg1, T_arg2, T_arg3, T_arg4, T_arg5, T_arg6, T_return>, T_arg1,T_arg2,T_arg3,T_arg4,T_arg5,T_arg6 >
retype(const pointer_functor6<T_arg1,T_arg2,T_arg3,T_arg4,T_arg5,T_arg6, T_return>& _A_functor)
{ return retype_functor<pointer_functor6<T_arg1, T_arg2, T_arg3, T_arg4, T_arg5, T_arg6, T_return>, T_arg1,T_arg2,T_arg3,T_arg4,T_arg5,T_arg6 >
    (_A_functor); }

/** Creates an adaptor of type sigc::retype_functor which performs C-style casts on the parameters passed on to the functor.
 * This function template specialization works on sigc::pointer_functor.
 *
 * @param _A_functor Functor that should be wrapped.
 * @return Adaptor that executes @e _A_functor performing C-style casts on the paramters passed on.
 *
 * @ingroup retype
 */
template <class T_arg1,class T_arg2,class T_arg3,class T_arg4,class T_arg5,class T_arg6,class T_arg7, class T_return>
inline retype_functor<pointer_functor7<T_arg1, T_arg2, T_arg3, T_arg4, T_arg5, T_arg6, T_arg7, T_return>, T_arg1,T_arg2,T_arg3,T_arg4,T_arg5,T_arg6,T_arg7 >
retype(const pointer_functor7<T_arg1,T_arg2,T_arg3,T_arg4,T_arg5,T_arg6,T_arg7, T_return>& _A_functor)
{ return retype_functor<pointer_functor7<T_arg1, T_arg2, T_arg3, T_arg4, T_arg5, T_arg6, T_arg7, T_return>, T_arg1,T_arg2,T_arg3,T_arg4,T_arg5,T_arg6,T_arg7 >
    (_A_functor); }


/** Creates an adaptor of type sigc::retype_functor which performs C-style casts on the parameters passed on to the functor.
 * This function template specialization works on sigc::mem_functor.
 *
 * @param _A_functor Functor that should be wrapped.
 * @return Adaptor that executes @e _A_functor performing C-style casts on the paramters passed on.
 *
 * @ingroup retype
 */
template <class T_return, class T_obj>
inline retype_functor<mem_functor0<T_return, T_obj> >
retype(const mem_functor0<T_return, T_obj>& _A_functor)
{ return retype_functor<mem_functor0<T_return, T_obj> >
    (_A_functor); }

/** Creates an adaptor of type sigc::retype_functor which performs C-style casts on the parameters passed on to the functor.
 * This function template specialization works on sigc::mem_functor.
 *
 * @param _A_functor Functor that should be wrapped.
 * @return Adaptor that executes @e _A_functor performing C-style casts on the paramters passed on.
 *
 * @ingroup retype
 */
template <class T_return, class T_obj, class T_arg1>
inline retype_functor<mem_functor1<T_return, T_obj, T_arg1>, T_arg1 >
retype(const mem_functor1<T_return, T_obj, T_arg1>& _A_functor)
{ return retype_functor<mem_functor1<T_return, T_obj, T_arg1>, T_arg1 >
    (_A_functor); }

/** Creates an adaptor of type sigc::retype_functor which performs C-style casts on the parameters passed on to the functor.
 * This function template specialization works on sigc::mem_functor.
 *
 * @param _A_functor Functor that should be wrapped.
 * @return Adaptor that executes @e _A_functor performing C-style casts on the paramters passed on.
 *
 * @ingroup retype
 */
template <class T_return, class T_obj, class T_arg1,class T_arg2>
inline retype_functor<mem_functor2<T_return, T_obj, T_arg1, T_arg2>, T_arg1,T_arg2 >
retype(const mem_functor2<T_return, T_obj, T_arg1,T_arg2>& _A_functor)
{ return retype_functor<mem_functor2<T_return, T_obj, T_arg1, T_arg2>, T_arg1,T_arg2 >
    (_A_functor); }

/** Creates an adaptor of type sigc::retype_functor which performs C-style casts on the parameters passed on to the functor.
 * This function template specialization works on sigc::mem_functor.
 *
 * @param _A_functor Functor that should be wrapped.
 * @return Adaptor that executes @e _A_functor performing C-style casts on the paramters passed on.
 *
 * @ingroup retype
 */
template <class T_return, class T_obj, class T_arg1,class T_arg2,class T_arg3>
inline retype_functor<mem_functor3<T_return, T_obj, T_arg1, T_arg2, T_arg3>, T_arg1,T_arg2,T_arg3 >
retype(const mem_functor3<T_return, T_obj, T_arg1,T_arg2,T_arg3>& _A_functor)
{ return retype_functor<mem_functor3<T_return, T_obj, T_arg1, T_arg2, T_arg3>, T_arg1,T_arg2,T_arg3 >
    (_A_functor); }

/** Creates an adaptor of type sigc::retype_functor which performs C-style casts on the parameters passed on to the functor.
 * This function template specialization works on sigc::mem_functor.
 *
 * @param _A_functor Functor that should be wrapped.
 * @return Adaptor that executes @e _A_functor performing C-style casts on the paramters passed on.
 *
 * @ingroup retype
 */
template <class T_return, class T_obj, class T_arg1,class T_arg2,class T_arg3,class T_arg4>
inline retype_functor<mem_functor4<T_return, T_obj, T_arg1, T_arg2, T_arg3, T_arg4>, T_arg1,T_arg2,T_arg3,T_arg4 >
retype(const mem_functor4<T_return, T_obj, T_arg1,T_arg2,T_arg3,T_arg4>& _A_functor)
{ return retype_functor<mem_functor4<T_return, T_obj, T_arg1, T_arg2, T_arg3, T_arg4>, T_arg1,T_arg2,T_arg3,T_arg4 >
    (_A_functor); }

/** Creates an adaptor of type sigc::retype_functor which performs C-style casts on the parameters passed on to the functor.
 * This function template specialization works on sigc::mem_functor.
 *
 * @param _A_functor Functor that should be wrapped.
 * @return Adaptor that executes @e _A_functor performing C-style casts on the paramters passed on.
 *
 * @ingroup retype
 */
template <class T_return, class T_obj, class T_arg1,class T_arg2,class T_arg3,class T_arg4,class T_arg5>
inline retype_functor<mem_functor5<T_return, T_obj, T_arg1, T_arg2, T_arg3, T_arg4, T_arg5>, T_arg1,T_arg2,T_arg3,T_arg4,T_arg5 >
retype(const mem_functor5<T_return, T_obj, T_arg1,T_arg2,T_arg3,T_arg4,T_arg5>& _A_functor)
{ return retype_functor<mem_functor5<T_return, T_obj, T_arg1, T_arg2, T_arg3, T_arg4, T_arg5>, T_arg1,T_arg2,T_arg3,T_arg4,T_arg5 >
    (_A_functor); }

/** Creates an adaptor of type sigc::retype_functor which performs C-style casts on the parameters passed on to the functor.
 * This function template specialization works on sigc::mem_functor.
 *
 * @param _A_functor Functor that should be wrapped.
 * @return Adaptor that executes @e _A_functor performing C-style casts on the paramters passed on.
 *
 * @ingroup retype
 */
template <class T_return, class T_obj, class T_arg1,class T_arg2,class T_arg3,class T_arg4,class T_arg5,class T_arg6>
inline retype_functor<mem_functor6<T_return, T_obj, T_arg1, T_arg2, T_arg3, T_arg4, T_arg5, T_arg6>, T_arg1,T_arg2,T_arg3,T_arg4,T_arg5,T_arg6 >
retype(const mem_functor6<T_return, T_obj, T_arg1,T_arg2,T_arg3,T_arg4,T_arg5,T_arg6>& _A_functor)
{ return retype_functor<mem_functor6<T_return, T_obj, T_arg1, T_arg2, T_arg3, T_arg4, T_arg5, T_arg6>, T_arg1,T_arg2,T_arg3,T_arg4,T_arg5,T_arg6 >
    (_A_functor); }

/** Creates an adaptor of type sigc::retype_functor which performs C-style casts on the parameters passed on to the functor.
 * This function template specialization works on sigc::mem_functor.
 *
 * @param _A_functor Functor that should be wrapped.
 * @return Adaptor that executes @e _A_functor performing C-style casts on the paramters passed on.
 *
 * @ingroup retype
 */
template <class T_return, class T_obj, class T_arg1,class T_arg2,class T_arg3,class T_arg4,class T_arg5,class T_arg6,class T_arg7>
inline retype_functor<mem_functor7<T_return, T_obj, T_arg1, T_arg2, T_arg3, T_arg4, T_arg5, T_arg6, T_arg7>, T_arg1,T_arg2,T_arg3,T_arg4,T_arg5,T_arg6,T_arg7 >
retype(const mem_functor7<T_return, T_obj, T_arg1,T_arg2,T_arg3,T_arg4,T_arg5,T_arg6,T_arg7>& _A_functor)
{ return retype_functor<mem_functor7<T_return, T_obj, T_arg1, T_arg2, T_arg3, T_arg4, T_arg5, T_arg6, T_arg7>, T_arg1,T_arg2,T_arg3,T_arg4,T_arg5,T_arg6,T_arg7 >
    (_A_functor); }

/** Creates an adaptor of type sigc::retype_functor which performs C-style casts on the parameters passed on to the functor.
 * This function template specialization works on sigc::const_mem_functor.
 *
 * @param _A_functor Functor that should be wrapped.
 * @return Adaptor that executes @e _A_functor performing C-style casts on the paramters passed on.
 *
 * @ingroup retype
 */
template <class T_return, class T_obj>
inline retype_functor<const_mem_functor0<T_return, T_obj> >
retype(const const_mem_functor0<T_return, T_obj>& _A_functor)
{ return retype_functor<const_mem_functor0<T_return, T_obj> >
    (_A_functor); }

/** Creates an adaptor of type sigc::retype_functor which performs C-style casts on the parameters passed on to the functor.
 * This function template specialization works on sigc::const_mem_functor.
 *
 * @param _A_functor Functor that should be wrapped.
 * @return Adaptor that executes @e _A_functor performing C-style casts on the paramters passed on.
 *
 * @ingroup retype
 */
template <class T_return, class T_obj, class T_arg1>
inline retype_functor<const_mem_functor1<T_return, T_obj, T_arg1>, T_arg1 >
retype(const const_mem_functor1<T_return, T_obj, T_arg1>& _A_functor)
{ return retype_functor<const_mem_functor1<T_return, T_obj, T_arg1>, T_arg1 >
    (_A_functor); }

/** Creates an adaptor of type sigc::retype_functor which performs C-style casts on the parameters passed on to the functor.
 * This function template specialization works on sigc::const_mem_functor.
 *
 * @param _A_functor Functor that should be wrapped.
 * @return Adaptor that executes @e _A_functor performing C-style casts on the paramters passed on.
 *
 * @ingroup retype
 */
template <class T_return, class T_obj, class T_arg1,class T_arg2>
inline retype_functor<const_mem_functor2<T_return, T_obj, T_arg1, T_arg2>, T_arg1,T_arg2 >
retype(const const_mem_functor2<T_return, T_obj, T_arg1,T_arg2>& _A_functor)
{ return retype_functor<const_mem_functor2<T_return, T_obj, T_arg1, T_arg2>, T_arg1,T_arg2 >
    (_A_functor); }

/** Creates an adaptor of type sigc::retype_functor which performs C-style casts on the parameters passed on to the functor.
 * This function template specialization works on sigc::const_mem_functor.
 *
 * @param _A_functor Functor that should be wrapped.
 * @return Adaptor that executes @e _A_functor performing C-style casts on the paramters passed on.
 *
 * @ingroup retype
 */
template <class T_return, class T_obj, class T_arg1,class T_arg2,class T_arg3>
inline retype_functor<const_mem_functor3<T_return, T_obj, T_arg1, T_arg2, T_arg3>, T_arg1,T_arg2,T_arg3 >
retype(const const_mem_functor3<T_return, T_obj, T_arg1,T_arg2,T_arg3>& _A_functor)
{ return retype_functor<const_mem_functor3<T_return, T_obj, T_arg1, T_arg2, T_arg3>, T_arg1,T_arg2,T_arg3 >
    (_A_functor); }

/** Creates an adaptor of type sigc::retype_functor which performs C-style casts on the parameters passed on to the functor.
 * This function template specialization works on sigc::const_mem_functor.
 *
 * @param _A_functor Functor that should be wrapped.
 * @return Adaptor that executes @e _A_functor performing C-style casts on the paramters passed on.
 *
 * @ingroup retype
 */
template <class T_return, class T_obj, class T_arg1,class T_arg2,class T_arg3,class T_arg4>
inline retype_functor<const_mem_functor4<T_return, T_obj, T_arg1, T_arg2, T_arg3, T_arg4>, T_arg1,T_arg2,T_arg3,T_arg4 >
retype(const const_mem_functor4<T_return, T_obj, T_arg1,T_arg2,T_arg3,T_arg4>& _A_functor)
{ return retype_functor<const_mem_functor4<T_return, T_obj, T_arg1, T_arg2, T_arg3, T_arg4>, T_arg1,T_arg2,T_arg3,T_arg4 >
    (_A_functor); }

/** Creates an adaptor of type sigc::retype_functor which performs C-style casts on the parameters passed on to the functor.
 * This function template specialization works on sigc::const_mem_functor.
 *
 * @param _A_functor Functor that should be wrapped.
 * @return Adaptor that executes @e _A_functor performing C-style casts on the paramters passed on.
 *
 * @ingroup retype
 */
template <class T_return, class T_obj, class T_arg1,class T_arg2,class T_arg3,class T_arg4,class T_arg5>
inline retype_functor<const_mem_functor5<T_return, T_obj, T_arg1, T_arg2, T_arg3, T_arg4, T_arg5>, T_arg1,T_arg2,T_arg3,T_arg4,T_arg5 >
retype(const const_mem_functor5<T_return, T_obj, T_arg1,T_arg2,T_arg3,T_arg4,T_arg5>& _A_functor)
{ return retype_functor<const_mem_functor5<T_return, T_obj, T_arg1, T_arg2, T_arg3, T_arg4, T_arg5>, T_arg1,T_arg2,T_arg3,T_arg4,T_arg5 >
    (_A_functor); }

/** Creates an adaptor of type sigc::retype_functor which performs C-style casts on the parameters passed on to the functor.
 * This function template specialization works on sigc::const_mem_functor.
 *
 * @param _A_functor Functor that should be wrapped.
 * @return Adaptor that executes @e _A_functor performing C-style casts on the paramters passed on.
 *
 * @ingroup retype
 */
template <class T_return, class T_obj, class T_arg1,class T_arg2,class T_arg3,class T_arg4,class T_arg5,class T_arg6>
inline retype_functor<const_mem_functor6<T_return, T_obj, T_arg1, T_arg2, T_arg3, T_arg4, T_arg5, T_arg6>, T_arg1,T_arg2,T_arg3,T_arg4,T_arg5,T_arg6 >
retype(const const_mem_functor6<T_return, T_obj, T_arg1,T_arg2,T_arg3,T_arg4,T_arg5,T_arg6>& _A_functor)
{ return retype_functor<const_mem_functor6<T_return, T_obj, T_arg1, T_arg2, T_arg3, T_arg4, T_arg5, T_arg6>, T_arg1,T_arg2,T_arg3,T_arg4,T_arg5,T_arg6 >
    (_A_functor); }

/** Creates an adaptor of type sigc::retype_functor which performs C-style casts on the parameters passed on to the functor.
 * This function template specialization works on sigc::const_mem_functor.
 *
 * @param _A_functor Functor that should be wrapped.
 * @return Adaptor that executes @e _A_functor performing C-style casts on the paramters passed on.
 *
 * @ingroup retype
 */
template <class T_return, class T_obj, class T_arg1,class T_arg2,class T_arg3,class T_arg4,class T_arg5,class T_arg6,class T_arg7>
inline retype_functor<const_mem_functor7<T_return, T_obj, T_arg1, T_arg2, T_arg3, T_arg4, T_arg5, T_arg6, T_arg7>, T_arg1,T_arg2,T_arg3,T_arg4,T_arg5,T_arg6,T_arg7 >
retype(const const_mem_functor7<T_return, T_obj, T_arg1,T_arg2,T_arg3,T_arg4,T_arg5,T_arg6,T_arg7>& _A_functor)
{ return retype_functor<const_mem_functor7<T_return, T_obj, T_arg1, T_arg2, T_arg3, T_arg4, T_arg5, T_arg6, T_arg7>, T_arg1,T_arg2,T_arg3,T_arg4,T_arg5,T_arg6,T_arg7 >
    (_A_functor); }

/** Creates an adaptor of type sigc::retype_functor which performs C-style casts on the parameters passed on to the functor.
 * This function template specialization works on sigc::volatile_mem_functor.
 *
 * @param _A_functor Functor that should be wrapped.
 * @return Adaptor that executes @e _A_functor performing C-style casts on the paramters passed on.
 *
 * @ingroup retype
 */
template <class T_return, class T_obj>
inline retype_functor<volatile_mem_functor0<T_return, T_obj> >
retype(const volatile_mem_functor0<T_return, T_obj>& _A_functor)
{ return retype_functor<volatile_mem_functor0<T_return, T_obj> >
    (_A_functor); }

/** Creates an adaptor of type sigc::retype_functor which performs C-style casts on the parameters passed on to the functor.
 * This function template specialization works on sigc::volatile_mem_functor.
 *
 * @param _A_functor Functor that should be wrapped.
 * @return Adaptor that executes @e _A_functor performing C-style casts on the paramters passed on.
 *
 * @ingroup retype
 */
template <class T_return, class T_obj, class T_arg1>
inline retype_functor<volatile_mem_functor1<T_return, T_obj, T_arg1>, T_arg1 >
retype(const volatile_mem_functor1<T_return, T_obj, T_arg1>& _A_functor)
{ return retype_functor<volatile_mem_functor1<T_return, T_obj, T_arg1>, T_arg1 >
    (_A_functor); }

/** Creates an adaptor of type sigc::retype_functor which performs C-style casts on the parameters passed on to the functor.
 * This function template specialization works on sigc::volatile_mem_functor.
 *
 * @param _A_functor Functor that should be wrapped.
 * @return Adaptor that executes @e _A_functor performing C-style casts on the paramters passed on.
 *
 * @ingroup retype
 */
template <class T_return, class T_obj, class T_arg1,class T_arg2>
inline retype_functor<volatile_mem_functor2<T_return, T_obj, T_arg1, T_arg2>, T_arg1,T_arg2 >
retype(const volatile_mem_functor2<T_return, T_obj, T_arg1,T_arg2>& _A_functor)
{ return retype_functor<volatile_mem_functor2<T_return, T_obj, T_arg1, T_arg2>, T_arg1,T_arg2 >
    (_A_functor); }

/** Creates an adaptor of type sigc::retype_functor which performs C-style casts on the parameters passed on to the functor.
 * This function template specialization works on sigc::volatile_mem_functor.
 *
 * @param _A_functor Functor that should be wrapped.
 * @return Adaptor that executes @e _A_functor performing C-style casts on the paramters passed on.
 *
 * @ingroup retype
 */
template <class T_return, class T_obj, class T_arg1,class T_arg2,class T_arg3>
inline retype_functor<volatile_mem_functor3<T_return, T_obj, T_arg1, T_arg2, T_arg3>, T_arg1,T_arg2,T_arg3 >
retype(const volatile_mem_functor3<T_return, T_obj, T_arg1,T_arg2,T_arg3>& _A_functor)
{ return retype_functor<volatile_mem_functor3<T_return, T_obj, T_arg1, T_arg2, T_arg3>, T_arg1,T_arg2,T_arg3 >
    (_A_functor); }

/** Creates an adaptor of type sigc::retype_functor which performs C-style casts on the parameters passed on to the functor.
 * This function template specialization works on sigc::volatile_mem_functor.
 *
 * @param _A_functor Functor that should be wrapped.
 * @return Adaptor that executes @e _A_functor performing C-style casts on the paramters passed on.
 *
 * @ingroup retype
 */
template <class T_return, class T_obj, class T_arg1,class T_arg2,class T_arg3,class T_arg4>
inline retype_functor<volatile_mem_functor4<T_return, T_obj, T_arg1, T_arg2, T_arg3, T_arg4>, T_arg1,T_arg2,T_arg3,T_arg4 >
retype(const volatile_mem_functor4<T_return, T_obj, T_arg1,T_arg2,T_arg3,T_arg4>& _A_functor)
{ return retype_functor<volatile_mem_functor4<T_return, T_obj, T_arg1, T_arg2, T_arg3, T_arg4>, T_arg1,T_arg2,T_arg3,T_arg4 >
    (_A_functor); }

/** Creates an adaptor of type sigc::retype_functor which performs C-style casts on the parameters passed on to the functor.
 * This function template specialization works on sigc::volatile_mem_functor.
 *
 * @param _A_functor Functor that should be wrapped.
 * @return Adaptor that executes @e _A_functor performing C-style casts on the paramters passed on.
 *
 * @ingroup retype
 */
template <class T_return, class T_obj, class T_arg1,class T_arg2,class T_arg3,class T_arg4,class T_arg5>
inline retype_functor<volatile_mem_functor5<T_return, T_obj, T_arg1, T_arg2, T_arg3, T_arg4, T_arg5>, T_arg1,T_arg2,T_arg3,T_arg4,T_arg5 >
retype(const volatile_mem_functor5<T_return, T_obj, T_arg1,T_arg2,T_arg3,T_arg4,T_arg5>& _A_functor)
{ return retype_functor<volatile_mem_functor5<T_return, T_obj, T_arg1, T_arg2, T_arg3, T_arg4, T_arg5>, T_arg1,T_arg2,T_arg3,T_arg4,T_arg5 >
    (_A_functor); }

/** Creates an adaptor of type sigc::retype_functor which performs C-style casts on the parameters passed on to the functor.
 * This function template specialization works on sigc::volatile_mem_functor.
 *
 * @param _A_functor Functor that should be wrapped.
 * @return Adaptor that executes @e _A_functor performing C-style casts on the paramters passed on.
 *
 * @ingroup retype
 */
template <class T_return, class T_obj, class T_arg1,class T_arg2,class T_arg3,class T_arg4,class T_arg5,class T_arg6>
inline retype_functor<volatile_mem_functor6<T_return, T_obj, T_arg1, T_arg2, T_arg3, T_arg4, T_arg5, T_arg6>, T_arg1,T_arg2,T_arg3,T_arg4,T_arg5,T_arg6 >
retype(const volatile_mem_functor6<T_return, T_obj, T_arg1,T_arg2,T_arg3,T_arg4,T_arg5,T_arg6>& _A_functor)
{ return retype_functor<volatile_mem_functor6<T_return, T_obj, T_arg1, T_arg2, T_arg3, T_arg4, T_arg5, T_arg6>, T_arg1,T_arg2,T_arg3,T_arg4,T_arg5,T_arg6 >
    (_A_functor); }

/** Creates an adaptor of type sigc::retype_functor which performs C-style casts on the parameters passed on to the functor.
 * This function template specialization works on sigc::volatile_mem_functor.
 *
 * @param _A_functor Functor that should be wrapped.
 * @return Adaptor that executes @e _A_functor performing C-style casts on the paramters passed on.
 *
 * @ingroup retype
 */
template <class T_return, class T_obj, class T_arg1,class T_arg2,class T_arg3,class T_arg4,class T_arg5,class T_arg6,class T_arg7>
inline retype_functor<volatile_mem_functor7<T_return, T_obj, T_arg1, T_arg2, T_arg3, T_arg4, T_arg5, T_arg6, T_arg7>, T_arg1,T_arg2,T_arg3,T_arg4,T_arg5,T_arg6,T_arg7 >
retype(const volatile_mem_functor7<T_return, T_obj, T_arg1,T_arg2,T_arg3,T_arg4,T_arg5,T_arg6,T_arg7>& _A_functor)
{ return retype_functor<volatile_mem_functor7<T_return, T_obj, T_arg1, T_arg2, T_arg3, T_arg4, T_arg5, T_arg6, T_arg7>, T_arg1,T_arg2,T_arg3,T_arg4,T_arg5,T_arg6,T_arg7 >
    (_A_functor); }

/** Creates an adaptor of type sigc::retype_functor which performs C-style casts on the parameters passed on to the functor.
 * This function template specialization works on sigc::const_volatile_mem_functor.
 *
 * @param _A_functor Functor that should be wrapped.
 * @return Adaptor that executes @e _A_functor performing C-style casts on the paramters passed on.
 *
 * @ingroup retype
 */
template <class T_return, class T_obj>
inline retype_functor<const_volatile_mem_functor0<T_return, T_obj> >
retype(const const_volatile_mem_functor0<T_return, T_obj>& _A_functor)
{ return retype_functor<const_volatile_mem_functor0<T_return, T_obj> >
    (_A_functor); }

/** Creates an adaptor of type sigc::retype_functor which performs C-style casts on the parameters passed on to the functor.
 * This function template specialization works on sigc::const_volatile_mem_functor.
 *
 * @param _A_functor Functor that should be wrapped.
 * @return Adaptor that executes @e _A_functor performing C-style casts on the paramters passed on.
 *
 * @ingroup retype
 */
template <class T_return, class T_obj, class T_arg1>
inline retype_functor<const_volatile_mem_functor1<T_return, T_obj, T_arg1>, T_arg1 >
retype(const const_volatile_mem_functor1<T_return, T_obj, T_arg1>& _A_functor)
{ return retype_functor<const_volatile_mem_functor1<T_return, T_obj, T_arg1>, T_arg1 >
    (_A_functor); }

/** Creates an adaptor of type sigc::retype_functor which performs C-style casts on the parameters passed on to the functor.
 * This function template specialization works on sigc::const_volatile_mem_functor.
 *
 * @param _A_functor Functor that should be wrapped.
 * @return Adaptor that executes @e _A_functor performing C-style casts on the paramters passed on.
 *
 * @ingroup retype
 */
template <class T_return, class T_obj, class T_arg1,class T_arg2>
inline retype_functor<const_volatile_mem_functor2<T_return, T_obj, T_arg1, T_arg2>, T_arg1,T_arg2 >
retype(const const_volatile_mem_functor2<T_return, T_obj, T_arg1,T_arg2>& _A_functor)
{ return retype_functor<const_volatile_mem_functor2<T_return, T_obj, T_arg1, T_arg2>, T_arg1,T_arg2 >
    (_A_functor); }

/** Creates an adaptor of type sigc::retype_functor which performs C-style casts on the parameters passed on to the functor.
 * This function template specialization works on sigc::const_volatile_mem_functor.
 *
 * @param _A_functor Functor that should be wrapped.
 * @return Adaptor that executes @e _A_functor performing C-style casts on the paramters passed on.
 *
 * @ingroup retype
 */
template <class T_return, class T_obj, class T_arg1,class T_arg2,class T_arg3>
inline retype_functor<const_volatile_mem_functor3<T_return, T_obj, T_arg1, T_arg2, T_arg3>, T_arg1,T_arg2,T_arg3 >
retype(const const_volatile_mem_functor3<T_return, T_obj, T_arg1,T_arg2,T_arg3>& _A_functor)
{ return retype_functor<const_volatile_mem_functor3<T_return, T_obj, T_arg1, T_arg2, T_arg3>, T_arg1,T_arg2,T_arg3 >
    (_A_functor); }

/** Creates an adaptor of type sigc::retype_functor which performs C-style casts on the parameters passed on to the functor.
 * This function template specialization works on sigc::const_volatile_mem_functor.
 *
 * @param _A_functor Functor that should be wrapped.
 * @return Adaptor that executes @e _A_functor performing C-style casts on the paramters passed on.
 *
 * @ingroup retype
 */
template <class T_return, class T_obj, class T_arg1,class T_arg2,class T_arg3,class T_arg4>
inline retype_functor<const_volatile_mem_functor4<T_return, T_obj, T_arg1, T_arg2, T_arg3, T_arg4>, T_arg1,T_arg2,T_arg3,T_arg4 >
retype(const const_volatile_mem_functor4<T_return, T_obj, T_arg1,T_arg2,T_arg3,T_arg4>& _A_functor)
{ return retype_functor<const_volatile_mem_functor4<T_return, T_obj, T_arg1, T_arg2, T_arg3, T_arg4>, T_arg1,T_arg2,T_arg3,T_arg4 >
    (_A_functor); }

/** Creates an adaptor of type sigc::retype_functor which performs C-style casts on the parameters passed on to the functor.
 * This function template specialization works on sigc::const_volatile_mem_functor.
 *
 * @param _A_functor Functor that should be wrapped.
 * @return Adaptor that executes @e _A_functor performing C-style casts on the paramters passed on.
 *
 * @ingroup retype
 */
template <class T_return, class T_obj, class T_arg1,class T_arg2,class T_arg3,class T_arg4,class T_arg5>
inline retype_functor<const_volatile_mem_functor5<T_return, T_obj, T_arg1, T_arg2, T_arg3, T_arg4, T_arg5>, T_arg1,T_arg2,T_arg3,T_arg4,T_arg5 >
retype(const const_volatile_mem_functor5<T_return, T_obj, T_arg1,T_arg2,T_arg3,T_arg4,T_arg5>& _A_functor)
{ return retype_functor<const_volatile_mem_functor5<T_return, T_obj, T_arg1, T_arg2, T_arg3, T_arg4, T_arg5>, T_arg1,T_arg2,T_arg3,T_arg4,T_arg5 >
    (_A_functor); }

/** Creates an adaptor of type sigc::retype_functor which performs C-style casts on the parameters passed on to the functor.
 * This function template specialization works on sigc::const_volatile_mem_functor.
 *
 * @param _A_functor Functor that should be wrapped.
 * @return Adaptor that executes @e _A_functor performing C-style casts on the paramters passed on.
 *
 * @ingroup retype
 */
template <class T_return, class T_obj, class T_arg1,class T_arg2,class T_arg3,class T_arg4,class T_arg5,class T_arg6>
inline retype_functor<const_volatile_mem_functor6<T_return, T_obj, T_arg1, T_arg2, T_arg3, T_arg4, T_arg5, T_arg6>, T_arg1,T_arg2,T_arg3,T_arg4,T_arg5,T_arg6 >
retype(const const_volatile_mem_functor6<T_return, T_obj, T_arg1,T_arg2,T_arg3,T_arg4,T_arg5,T_arg6>& _A_functor)
{ return retype_functor<const_volatile_mem_functor6<T_return, T_obj, T_arg1, T_arg2, T_arg3, T_arg4, T_arg5, T_arg6>, T_arg1,T_arg2,T_arg3,T_arg4,T_arg5,T_arg6 >
    (_A_functor); }

/** Creates an adaptor of type sigc::retype_functor which performs C-style casts on the parameters passed on to the functor.
 * This function template specialization works on sigc::const_volatile_mem_functor.
 *
 * @param _A_functor Functor that should be wrapped.
 * @return Adaptor that executes @e _A_functor performing C-style casts on the paramters passed on.
 *
 * @ingroup retype
 */
template <class T_return, class T_obj, class T_arg1,class T_arg2,class T_arg3,class T_arg4,class T_arg5,class T_arg6,class T_arg7>
inline retype_functor<const_volatile_mem_functor7<T_return, T_obj, T_arg1, T_arg2, T_arg3, T_arg4, T_arg5, T_arg6, T_arg7>, T_arg1,T_arg2,T_arg3,T_arg4,T_arg5,T_arg6,T_arg7 >
retype(const const_volatile_mem_functor7<T_return, T_obj, T_arg1,T_arg2,T_arg3,T_arg4,T_arg5,T_arg6,T_arg7>& _A_functor)
{ return retype_functor<const_volatile_mem_functor7<T_return, T_obj, T_arg1, T_arg2, T_arg3, T_arg4, T_arg5, T_arg6, T_arg7>, T_arg1,T_arg2,T_arg3,T_arg4,T_arg5,T_arg6,T_arg7 >
    (_A_functor); }

/** Creates an adaptor of type sigc::retype_functor which performs C-style casts on the parameters passed on to the functor.
 * This function template specialization works on sigc::bound_mem_functor.
 *
 * @param _A_functor Functor that should be wrapped.
 * @return Adaptor that executes @e _A_functor performing C-style casts on the paramters passed on.
 *
 * @ingroup retype
 */
template <class T_return, class T_obj>
inline retype_functor<bound_mem_functor0<T_return, T_obj> >
retype(const bound_mem_functor0<T_return, T_obj>& _A_functor)
{ return retype_functor<bound_mem_functor0<T_return, T_obj> >
    (_A_functor); }

/** Creates an adaptor of type sigc::retype_functor which performs C-style casts on the parameters passed on to the functor.
 * This function template specialization works on sigc::bound_mem_functor.
 *
 * @param _A_functor Functor that should be wrapped.
 * @return Adaptor that executes @e _A_functor performing C-style casts on the paramters passed on.
 *
 * @ingroup retype
 */
template <class T_return, class T_obj, class T_arg1>
inline retype_functor<bound_mem_functor1<T_return, T_obj, T_arg1>, T_arg1 >
retype(const bound_mem_functor1<T_return, T_obj, T_arg1>& _A_functor)
{ return retype_functor<bound_mem_functor1<T_return, T_obj, T_arg1>, T_arg1 >
    (_A_functor); }

/** Creates an adaptor of type sigc::retype_functor which performs C-style casts on the parameters passed on to the functor.
 * This function template specialization works on sigc::bound_mem_functor.
 *
 * @param _A_functor Functor that should be wrapped.
 * @return Adaptor that executes @e _A_functor performing C-style casts on the paramters passed on.
 *
 * @ingroup retype
 */
template <class T_return, class T_obj, class T_arg1,class T_arg2>
inline retype_functor<bound_mem_functor2<T_return, T_obj, T_arg1, T_arg2>, T_arg1,T_arg2 >
retype(const bound_mem_functor2<T_return, T_obj, T_arg1,T_arg2>& _A_functor)
{ return retype_functor<bound_mem_functor2<T_return, T_obj, T_arg1, T_arg2>, T_arg1,T_arg2 >
    (_A_functor); }

/** Creates an adaptor of type sigc::retype_functor which performs C-style casts on the parameters passed on to the functor.
 * This function template specialization works on sigc::bound_mem_functor.
 *
 * @param _A_functor Functor that should be wrapped.
 * @return Adaptor that executes @e _A_functor performing C-style casts on the paramters passed on.
 *
 * @ingroup retype
 */
template <class T_return, class T_obj, class T_arg1,class T_arg2,class T_arg3>
inline retype_functor<bound_mem_functor3<T_return, T_obj, T_arg1, T_arg2, T_arg3>, T_arg1,T_arg2,T_arg3 >
retype(const bound_mem_functor3<T_return, T_obj, T_arg1,T_arg2,T_arg3>& _A_functor)
{ return retype_functor<bound_mem_functor3<T_return, T_obj, T_arg1, T_arg2, T_arg3>, T_arg1,T_arg2,T_arg3 >
    (_A_functor); }

/** Creates an adaptor of type sigc::retype_functor which performs C-style casts on the parameters passed on to the functor.
 * This function template specialization works on sigc::bound_mem_functor.
 *
 * @param _A_functor Functor that should be wrapped.
 * @return Adaptor that executes @e _A_functor performing C-style casts on the paramters passed on.
 *
 * @ingroup retype
 */
template <class T_return, class T_obj, class T_arg1,class T_arg2,class T_arg3,class T_arg4>
inline retype_functor<bound_mem_functor4<T_return, T_obj, T_arg1, T_arg2, T_arg3, T_arg4>, T_arg1,T_arg2,T_arg3,T_arg4 >
retype(const bound_mem_functor4<T_return, T_obj, T_arg1,T_arg2,T_arg3,T_arg4>& _A_functor)
{ return retype_functor<bound_mem_functor4<T_return, T_obj, T_arg1, T_arg2, T_arg3, T_arg4>, T_arg1,T_arg2,T_arg3,T_arg4 >
    (_A_functor); }

/** Creates an adaptor of type sigc::retype_functor which performs C-style casts on the parameters passed on to the functor.
 * This function template specialization works on sigc::bound_mem_functor.
 *
 * @param _A_functor Functor that should be wrapped.
 * @return Adaptor that executes @e _A_functor performing C-style casts on the paramters passed on.
 *
 * @ingroup retype
 */
template <class T_return, class T_obj, class T_arg1,class T_arg2,class T_arg3,class T_arg4,class T_arg5>
inline retype_functor<bound_mem_functor5<T_return, T_obj, T_arg1, T_arg2, T_arg3, T_arg4, T_arg5>, T_arg1,T_arg2,T_arg3,T_arg4,T_arg5 >
retype(const bound_mem_functor5<T_return, T_obj, T_arg1,T_arg2,T_arg3,T_arg4,T_arg5>& _A_functor)
{ return retype_functor<bound_mem_functor5<T_return, T_obj, T_arg1, T_arg2, T_arg3, T_arg4, T_arg5>, T_arg1,T_arg2,T_arg3,T_arg4,T_arg5 >
    (_A_functor); }

/** Creates an adaptor of type sigc::retype_functor which performs C-style casts on the parameters passed on to the functor.
 * This function template specialization works on sigc::bound_mem_functor.
 *
 * @param _A_functor Functor that should be wrapped.
 * @return Adaptor that executes @e _A_functor performing C-style casts on the paramters passed on.
 *
 * @ingroup retype
 */
template <class T_return, class T_obj, class T_arg1,class T_arg2,class T_arg3,class T_arg4,class T_arg5,class T_arg6>
inline retype_functor<bound_mem_functor6<T_return, T_obj, T_arg1, T_arg2, T_arg3, T_arg4, T_arg5, T_arg6>, T_arg1,T_arg2,T_arg3,T_arg4,T_arg5,T_arg6 >
retype(const bound_mem_functor6<T_return, T_obj, T_arg1,T_arg2,T_arg3,T_arg4,T_arg5,T_arg6>& _A_functor)
{ return retype_functor<bound_mem_functor6<T_return, T_obj, T_arg1, T_arg2, T_arg3, T_arg4, T_arg5, T_arg6>, T_arg1,T_arg2,T_arg3,T_arg4,T_arg5,T_arg6 >
    (_A_functor); }

/** Creates an adaptor of type sigc::retype_functor which performs C-style casts on the parameters passed on to the functor.
 * This function template specialization works on sigc::bound_mem_functor.
 *
 * @param _A_functor Functor that should be wrapped.
 * @return Adaptor that executes @e _A_functor performing C-style casts on the paramters passed on.
 *
 * @ingroup retype
 */
template <class T_return, class T_obj, class T_arg1,class T_arg2,class T_arg3,class T_arg4,class T_arg5,class T_arg6,class T_arg7>
inline retype_functor<bound_mem_functor7<T_return, T_obj, T_arg1, T_arg2, T_arg3, T_arg4, T_arg5, T_arg6, T_arg7>, T_arg1,T_arg2,T_arg3,T_arg4,T_arg5,T_arg6,T_arg7 >
retype(const bound_mem_functor7<T_return, T_obj, T_arg1,T_arg2,T_arg3,T_arg4,T_arg5,T_arg6,T_arg7>& _A_functor)
{ return retype_functor<bound_mem_functor7<T_return, T_obj, T_arg1, T_arg2, T_arg3, T_arg4, T_arg5, T_arg6, T_arg7>, T_arg1,T_arg2,T_arg3,T_arg4,T_arg5,T_arg6,T_arg7 >
    (_A_functor); }

/** Creates an adaptor of type sigc::retype_functor which performs C-style casts on the parameters passed on to the functor.
 * This function template specialization works on sigc::bound_const_mem_functor.
 *
 * @param _A_functor Functor that should be wrapped.
 * @return Adaptor that executes @e _A_functor performing C-style casts on the paramters passed on.
 *
 * @ingroup retype
 */
template <class T_return, class T_obj>
inline retype_functor<bound_const_mem_functor0<T_return, T_obj> >
retype(const bound_const_mem_functor0<T_return, T_obj>& _A_functor)
{ return retype_functor<bound_const_mem_functor0<T_return, T_obj> >
    (_A_functor); }

/** Creates an adaptor of type sigc::retype_functor which performs C-style casts on the parameters passed on to the functor.
 * This function template specialization works on sigc::bound_const_mem_functor.
 *
 * @param _A_functor Functor that should be wrapped.
 * @return Adaptor that executes @e _A_functor performing C-style casts on the paramters passed on.
 *
 * @ingroup retype
 */
template <class T_return, class T_obj, class T_arg1>
inline retype_functor<bound_const_mem_functor1<T_return, T_obj, T_arg1>, T_arg1 >
retype(const bound_const_mem_functor1<T_return, T_obj, T_arg1>& _A_functor)
{ return retype_functor<bound_const_mem_functor1<T_return, T_obj, T_arg1>, T_arg1 >
    (_A_functor); }

/** Creates an adaptor of type sigc::retype_functor which performs C-style casts on the parameters passed on to the functor.
 * This function template specialization works on sigc::bound_const_mem_functor.
 *
 * @param _A_functor Functor that should be wrapped.
 * @return Adaptor that executes @e _A_functor performing C-style casts on the paramters passed on.
 *
 * @ingroup retype
 */
template <class T_return, class T_obj, class T_arg1,class T_arg2>
inline retype_functor<bound_const_mem_functor2<T_return, T_obj, T_arg1, T_arg2>, T_arg1,T_arg2 >
retype(const bound_const_mem_functor2<T_return, T_obj, T_arg1,T_arg2>& _A_functor)
{ return retype_functor<bound_const_mem_functor2<T_return, T_obj, T_arg1, T_arg2>, T_arg1,T_arg2 >
    (_A_functor); }

/** Creates an adaptor of type sigc::retype_functor which performs C-style casts on the parameters passed on to the functor.
 * This function template specialization works on sigc::bound_const_mem_functor.
 *
 * @param _A_functor Functor that should be wrapped.
 * @return Adaptor that executes @e _A_functor performing C-style casts on the paramters passed on.
 *
 * @ingroup retype
 */
template <class T_return, class T_obj, class T_arg1,class T_arg2,class T_arg3>
inline retype_functor<bound_const_mem_functor3<T_return, T_obj, T_arg1, T_arg2, T_arg3>, T_arg1,T_arg2,T_arg3 >
retype(const bound_const_mem_functor3<T_return, T_obj, T_arg1,T_arg2,T_arg3>& _A_functor)
{ return retype_functor<bound_const_mem_functor3<T_return, T_obj, T_arg1, T_arg2, T_arg3>, T_arg1,T_arg2,T_arg3 >
    (_A_functor); }

/** Creates an adaptor of type sigc::retype_functor which performs C-style casts on the parameters passed on to the functor.
 * This function template specialization works on sigc::bound_const_mem_functor.
 *
 * @param _A_functor Functor that should be wrapped.
 * @return Adaptor that executes @e _A_functor performing C-style casts on the paramters passed on.
 *
 * @ingroup retype
 */
template <class T_return, class T_obj, class T_arg1,class T_arg2,class T_arg3,class T_arg4>
inline retype_functor<bound_const_mem_functor4<T_return, T_obj, T_arg1, T_arg2, T_arg3, T_arg4>, T_arg1,T_arg2,T_arg3,T_arg4 >
retype(const bound_const_mem_functor4<T_return, T_obj, T_arg1,T_arg2,T_arg3,T_arg4>& _A_functor)
{ return retype_functor<bound_const_mem_functor4<T_return, T_obj, T_arg1, T_arg2, T_arg3, T_arg4>, T_arg1,T_arg2,T_arg3,T_arg4 >
    (_A_functor); }

/** Creates an adaptor of type sigc::retype_functor which performs C-style casts on the parameters passed on to the functor.
 * This function template specialization works on sigc::bound_const_mem_functor.
 *
 * @param _A_functor Functor that should be wrapped.
 * @return Adaptor that executes @e _A_functor performing C-style casts on the paramters passed on.
 *
 * @ingroup retype
 */
template <class T_return, class T_obj, class T_arg1,class T_arg2,class T_arg3,class T_arg4,class T_arg5>
inline retype_functor<bound_const_mem_functor5<T_return, T_obj, T_arg1, T_arg2, T_arg3, T_arg4, T_arg5>, T_arg1,T_arg2,T_arg3,T_arg4,T_arg5 >
retype(const bound_const_mem_functor5<T_return, T_obj, T_arg1,T_arg2,T_arg3,T_arg4,T_arg5>& _A_functor)
{ return retype_functor<bound_const_mem_functor5<T_return, T_obj, T_arg1, T_arg2, T_arg3, T_arg4, T_arg5>, T_arg1,T_arg2,T_arg3,T_arg4,T_arg5 >
    (_A_functor); }

/** Creates an adaptor of type sigc::retype_functor which performs C-style casts on the parameters passed on to the functor.
 * This function template specialization works on sigc::bound_const_mem_functor.
 *
 * @param _A_functor Functor that should be wrapped.
 * @return Adaptor that executes @e _A_functor performing C-style casts on the paramters passed on.
 *
 * @ingroup retype
 */
template <class T_return, class T_obj, class T_arg1,class T_arg2,class T_arg3,class T_arg4,class T_arg5,class T_arg6>
inline retype_functor<bound_const_mem_functor6<T_return, T_obj, T_arg1, T_arg2, T_arg3, T_arg4, T_arg5, T_arg6>, T_arg1,T_arg2,T_arg3,T_arg4,T_arg5,T_arg6 >
retype(const bound_const_mem_functor6<T_return, T_obj, T_arg1,T_arg2,T_arg3,T_arg4,T_arg5,T_arg6>& _A_functor)
{ return retype_functor<bound_const_mem_functor6<T_return, T_obj, T_arg1, T_arg2, T_arg3, T_arg4, T_arg5, T_arg6>, T_arg1,T_arg2,T_arg3,T_arg4,T_arg5,T_arg6 >
    (_A_functor); }

/** Creates an adaptor of type sigc::retype_functor which performs C-style casts on the parameters passed on to the functor.
 * This function template specialization works on sigc::bound_const_mem_functor.
 *
 * @param _A_functor Functor that should be wrapped.
 * @return Adaptor that executes @e _A_functor performing C-style casts on the paramters passed on.
 *
 * @ingroup retype
 */
template <class T_return, class T_obj, class T_arg1,class T_arg2,class T_arg3,class T_arg4,class T_arg5,class T_arg6,class T_arg7>
inline retype_functor<bound_const_mem_functor7<T_return, T_obj, T_arg1, T_arg2, T_arg3, T_arg4, T_arg5, T_arg6, T_arg7>, T_arg1,T_arg2,T_arg3,T_arg4,T_arg5,T_arg6,T_arg7 >
retype(const bound_const_mem_functor7<T_return, T_obj, T_arg1,T_arg2,T_arg3,T_arg4,T_arg5,T_arg6,T_arg7>& _A_functor)
{ return retype_functor<bound_const_mem_functor7<T_return, T_obj, T_arg1, T_arg2, T_arg3, T_arg4, T_arg5, T_arg6, T_arg7>, T_arg1,T_arg2,T_arg3,T_arg4,T_arg5,T_arg6,T_arg7 >
    (_A_functor); }

/** Creates an adaptor of type sigc::retype_functor which performs C-style casts on the parameters passed on to the functor.
 * This function template specialization works on sigc::bound_volatile_mem_functor.
 *
 * @param _A_functor Functor that should be wrapped.
 * @return Adaptor that executes @e _A_functor performing C-style casts on the paramters passed on.
 *
 * @ingroup retype
 */
template <class T_return, class T_obj>
inline retype_functor<bound_volatile_mem_functor0<T_return, T_obj> >
retype(const bound_volatile_mem_functor0<T_return, T_obj>& _A_functor)
{ return retype_functor<bound_volatile_mem_functor0<T_return, T_obj> >
    (_A_functor); }

/** Creates an adaptor of type sigc::retype_functor which performs C-style casts on the parameters passed on to the functor.
 * This function template specialization works on sigc::bound_volatile_mem_functor.
 *
 * @param _A_functor Functor that should be wrapped.
 * @return Adaptor that executes @e _A_functor performing C-style casts on the paramters passed on.
 *
 * @ingroup retype
 */
template <class T_return, class T_obj, class T_arg1>
inline retype_functor<bound_volatile_mem_functor1<T_return, T_obj, T_arg1>, T_arg1 >
retype(const bound_volatile_mem_functor1<T_return, T_obj, T_arg1>& _A_functor)
{ return retype_functor<bound_volatile_mem_functor1<T_return, T_obj, T_arg1>, T_arg1 >
    (_A_functor); }

/** Creates an adaptor of type sigc::retype_functor which performs C-style casts on the parameters passed on to the functor.
 * This function template specialization works on sigc::bound_volatile_mem_functor.
 *
 * @param _A_functor Functor that should be wrapped.
 * @return Adaptor that executes @e _A_functor performing C-style casts on the paramters passed on.
 *
 * @ingroup retype
 */
template <class T_return, class T_obj, class T_arg1,class T_arg2>
inline retype_functor<bound_volatile_mem_functor2<T_return, T_obj, T_arg1, T_arg2>, T_arg1,T_arg2 >
retype(const bound_volatile_mem_functor2<T_return, T_obj, T_arg1,T_arg2>& _A_functor)
{ return retype_functor<bound_volatile_mem_functor2<T_return, T_obj, T_arg1, T_arg2>, T_arg1,T_arg2 >
    (_A_functor); }

/** Creates an adaptor of type sigc::retype_functor which performs C-style casts on the parameters passed on to the functor.
 * This function template specialization works on sigc::bound_volatile_mem_functor.
 *
 * @param _A_functor Functor that should be wrapped.
 * @return Adaptor that executes @e _A_functor performing C-style casts on the paramters passed on.
 *
 * @ingroup retype
 */
template <class T_return, class T_obj, class T_arg1,class T_arg2,class T_arg3>
inline retype_functor<bound_volatile_mem_functor3<T_return, T_obj, T_arg1, T_arg2, T_arg3>, T_arg1,T_arg2,T_arg3 >
retype(const bound_volatile_mem_functor3<T_return, T_obj, T_arg1,T_arg2,T_arg3>& _A_functor)
{ return retype_functor<bound_volatile_mem_functor3<T_return, T_obj, T_arg1, T_arg2, T_arg3>, T_arg1,T_arg2,T_arg3 >
    (_A_functor); }

/** Creates an adaptor of type sigc::retype_functor which performs C-style casts on the parameters passed on to the functor.
 * This function template specialization works on sigc::bound_volatile_mem_functor.
 *
 * @param _A_functor Functor that should be wrapped.
 * @return Adaptor that executes @e _A_functor performing C-style casts on the paramters passed on.
 *
 * @ingroup retype
 */
template <class T_return, class T_obj, class T_arg1,class T_arg2,class T_arg3,class T_arg4>
inline retype_functor<bound_volatile_mem_functor4<T_return, T_obj, T_arg1, T_arg2, T_arg3, T_arg4>, T_arg1,T_arg2,T_arg3,T_arg4 >
retype(const bound_volatile_mem_functor4<T_return, T_obj, T_arg1,T_arg2,T_arg3,T_arg4>& _A_functor)
{ return retype_functor<bound_volatile_mem_functor4<T_return, T_obj, T_arg1, T_arg2, T_arg3, T_arg4>, T_arg1,T_arg2,T_arg3,T_arg4 >
    (_A_functor); }

/** Creates an adaptor of type sigc::retype_functor which performs C-style casts on the parameters passed on to the functor.
 * This function template specialization works on sigc::bound_volatile_mem_functor.
 *
 * @param _A_functor Functor that should be wrapped.
 * @return Adaptor that executes @e _A_functor performing C-style casts on the paramters passed on.
 *
 * @ingroup retype
 */
template <class T_return, class T_obj, class T_arg1,class T_arg2,class T_arg3,class T_arg4,class T_arg5>
inline retype_functor<bound_volatile_mem_functor5<T_return, T_obj, T_arg1, T_arg2, T_arg3, T_arg4, T_arg5>, T_arg1,T_arg2,T_arg3,T_arg4,T_arg5 >
retype(const bound_volatile_mem_functor5<T_return, T_obj, T_arg1,T_arg2,T_arg3,T_arg4,T_arg5>& _A_functor)
{ return retype_functor<bound_volatile_mem_functor5<T_return, T_obj, T_arg1, T_arg2, T_arg3, T_arg4, T_arg5>, T_arg1,T_arg2,T_arg3,T_arg4,T_arg5 >
    (_A_functor); }

/** Creates an adaptor of type sigc::retype_functor which performs C-style casts on the parameters passed on to the functor.
 * This function template specialization works on sigc::bound_volatile_mem_functor.
 *
 * @param _A_functor Functor that should be wrapped.
 * @return Adaptor that executes @e _A_functor performing C-style casts on the paramters passed on.
 *
 * @ingroup retype
 */
template <class T_return, class T_obj, class T_arg1,class T_arg2,class T_arg3,class T_arg4,class T_arg5,class T_arg6>
inline retype_functor<bound_volatile_mem_functor6<T_return, T_obj, T_arg1, T_arg2, T_arg3, T_arg4, T_arg5, T_arg6>, T_arg1,T_arg2,T_arg3,T_arg4,T_arg5,T_arg6 >
retype(const bound_volatile_mem_functor6<T_return, T_obj, T_arg1,T_arg2,T_arg3,T_arg4,T_arg5,T_arg6>& _A_functor)
{ return retype_functor<bound_volatile_mem_functor6<T_return, T_obj, T_arg1, T_arg2, T_arg3, T_arg4, T_arg5, T_arg6>, T_arg1,T_arg2,T_arg3,T_arg4,T_arg5,T_arg6 >
    (_A_functor); }

/** Creates an adaptor of type sigc::retype_functor which performs C-style casts on the parameters passed on to the functor.
 * This function template specialization works on sigc::bound_volatile_mem_functor.
 *
 * @param _A_functor Functor that should be wrapped.
 * @return Adaptor that executes @e _A_functor performing C-style casts on the paramters passed on.
 *
 * @ingroup retype
 */
template <class T_return, class T_obj, class T_arg1,class T_arg2,class T_arg3,class T_arg4,class T_arg5,class T_arg6,class T_arg7>
inline retype_functor<bound_volatile_mem_functor7<T_return, T_obj, T_arg1, T_arg2, T_arg3, T_arg4, T_arg5, T_arg6, T_arg7>, T_arg1,T_arg2,T_arg3,T_arg4,T_arg5,T_arg6,T_arg7 >
retype(const bound_volatile_mem_functor7<T_return, T_obj, T_arg1,T_arg2,T_arg3,T_arg4,T_arg5,T_arg6,T_arg7>& _A_functor)
{ return retype_functor<bound_volatile_mem_functor7<T_return, T_obj, T_arg1, T_arg2, T_arg3, T_arg4, T_arg5, T_arg6, T_arg7>, T_arg1,T_arg2,T_arg3,T_arg4,T_arg5,T_arg6,T_arg7 >
    (_A_functor); }

/** Creates an adaptor of type sigc::retype_functor which performs C-style casts on the parameters passed on to the functor.
 * This function template specialization works on sigc::bound_const_volatile_mem_functor.
 *
 * @param _A_functor Functor that should be wrapped.
 * @return Adaptor that executes @e _A_functor performing C-style casts on the paramters passed on.
 *
 * @ingroup retype
 */
template <class T_return, class T_obj>
inline retype_functor<bound_const_volatile_mem_functor0<T_return, T_obj> >
retype(const bound_const_volatile_mem_functor0<T_return, T_obj>& _A_functor)
{ return retype_functor<bound_const_volatile_mem_functor0<T_return, T_obj> >
    (_A_functor); }

/** Creates an adaptor of type sigc::retype_functor which performs C-style casts on the parameters passed on to the functor.
 * This function template specialization works on sigc::bound_const_volatile_mem_functor.
 *
 * @param _A_functor Functor that should be wrapped.
 * @return Adaptor that executes @e _A_functor performing C-style casts on the paramters passed on.
 *
 * @ingroup retype
 */
template <class T_return, class T_obj, class T_arg1>
inline retype_functor<bound_const_volatile_mem_functor1<T_return, T_obj, T_arg1>, T_arg1 >
retype(const bound_const_volatile_mem_functor1<T_return, T_obj, T_arg1>& _A_functor)
{ return retype_functor<bound_const_volatile_mem_functor1<T_return, T_obj, T_arg1>, T_arg1 >
    (_A_functor); }

/** Creates an adaptor of type sigc::retype_functor which performs C-style casts on the parameters passed on to the functor.
 * This function template specialization works on sigc::bound_const_volatile_mem_functor.
 *
 * @param _A_functor Functor that should be wrapped.
 * @return Adaptor that executes @e _A_functor performing C-style casts on the paramters passed on.
 *
 * @ingroup retype
 */
template <class T_return, class T_obj, class T_arg1,class T_arg2>
inline retype_functor<bound_const_volatile_mem_functor2<T_return, T_obj, T_arg1, T_arg2>, T_arg1,T_arg2 >
retype(const bound_const_volatile_mem_functor2<T_return, T_obj, T_arg1,T_arg2>& _A_functor)
{ return retype_functor<bound_const_volatile_mem_functor2<T_return, T_obj, T_arg1, T_arg2>, T_arg1,T_arg2 >
    (_A_functor); }

/** Creates an adaptor of type sigc::retype_functor which performs C-style casts on the parameters passed on to the functor.
 * This function template specialization works on sigc::bound_const_volatile_mem_functor.
 *
 * @param _A_functor Functor that should be wrapped.
 * @return Adaptor that executes @e _A_functor performing C-style casts on the paramters passed on.
 *
 * @ingroup retype
 */
template <class T_return, class T_obj, class T_arg1,class T_arg2,class T_arg3>
inline retype_functor<bound_const_volatile_mem_functor3<T_return, T_obj, T_arg1, T_arg2, T_arg3>, T_arg1,T_arg2,T_arg3 >
retype(const bound_const_volatile_mem_functor3<T_return, T_obj, T_arg1,T_arg2,T_arg3>& _A_functor)
{ return retype_functor<bound_const_volatile_mem_functor3<T_return, T_obj, T_arg1, T_arg2, T_arg3>, T_arg1,T_arg2,T_arg3 >
    (_A_functor); }

/** Creates an adaptor of type sigc::retype_functor which performs C-style casts on the parameters passed on to the functor.
 * This function template specialization works on sigc::bound_const_volatile_mem_functor.
 *
 * @param _A_functor Functor that should be wrapped.
 * @return Adaptor that executes @e _A_functor performing C-style casts on the paramters passed on.
 *
 * @ingroup retype
 */
template <class T_return, class T_obj, class T_arg1,class T_arg2,class T_arg3,class T_arg4>
inline retype_functor<bound_const_volatile_mem_functor4<T_return, T_obj, T_arg1, T_arg2, T_arg3, T_arg4>, T_arg1,T_arg2,T_arg3,T_arg4 >
retype(const bound_const_volatile_mem_functor4<T_return, T_obj, T_arg1,T_arg2,T_arg3,T_arg4>& _A_functor)
{ return retype_functor<bound_const_volatile_mem_functor4<T_return, T_obj, T_arg1, T_arg2, T_arg3, T_arg4>, T_arg1,T_arg2,T_arg3,T_arg4 >
    (_A_functor); }

/** Creates an adaptor of type sigc::retype_functor which performs C-style casts on the parameters passed on to the functor.
 * This function template specialization works on sigc::bound_const_volatile_mem_functor.
 *
 * @param _A_functor Functor that should be wrapped.
 * @return Adaptor that executes @e _A_functor performing C-style casts on the paramters passed on.
 *
 * @ingroup retype
 */
template <class T_return, class T_obj, class T_arg1,class T_arg2,class T_arg3,class T_arg4,class T_arg5>
inline retype_functor<bound_const_volatile_mem_functor5<T_return, T_obj, T_arg1, T_arg2, T_arg3, T_arg4, T_arg5>, T_arg1,T_arg2,T_arg3,T_arg4,T_arg5 >
retype(const bound_const_volatile_mem_functor5<T_return, T_obj, T_arg1,T_arg2,T_arg3,T_arg4,T_arg5>& _A_functor)
{ return retype_functor<bound_const_volatile_mem_functor5<T_return, T_obj, T_arg1, T_arg2, T_arg3, T_arg4, T_arg5>, T_arg1,T_arg2,T_arg3,T_arg4,T_arg5 >
    (_A_functor); }

/** Creates an adaptor of type sigc::retype_functor which performs C-style casts on the parameters passed on to the functor.
 * This function template specialization works on sigc::bound_const_volatile_mem_functor.
 *
 * @param _A_functor Functor that should be wrapped.
 * @return Adaptor that executes @e _A_functor performing C-style casts on the paramters passed on.
 *
 * @ingroup retype
 */
template <class T_return, class T_obj, class T_arg1,class T_arg2,class T_arg3,class T_arg4,class T_arg5,class T_arg6>
inline retype_functor<bound_const_volatile_mem_functor6<T_return, T_obj, T_arg1, T_arg2, T_arg3, T_arg4, T_arg5, T_arg6>, T_arg1,T_arg2,T_arg3,T_arg4,T_arg5,T_arg6 >
retype(const bound_const_volatile_mem_functor6<T_return, T_obj, T_arg1,T_arg2,T_arg3,T_arg4,T_arg5,T_arg6>& _A_functor)
{ return retype_functor<bound_const_volatile_mem_functor6<T_return, T_obj, T_arg1, T_arg2, T_arg3, T_arg4, T_arg5, T_arg6>, T_arg1,T_arg2,T_arg3,T_arg4,T_arg5,T_arg6 >
    (_A_functor); }

/** Creates an adaptor of type sigc::retype_functor which performs C-style casts on the parameters passed on to the functor.
 * This function template specialization works on sigc::bound_const_volatile_mem_functor.
 *
 * @param _A_functor Functor that should be wrapped.
 * @return Adaptor that executes @e _A_functor performing C-style casts on the paramters passed on.
 *
 * @ingroup retype
 */
template <class T_return, class T_obj, class T_arg1,class T_arg2,class T_arg3,class T_arg4,class T_arg5,class T_arg6,class T_arg7>
inline retype_functor<bound_const_volatile_mem_functor7<T_return, T_obj, T_arg1, T_arg2, T_arg3, T_arg4, T_arg5, T_arg6, T_arg7>, T_arg1,T_arg2,T_arg3,T_arg4,T_arg5,T_arg6,T_arg7 >
retype(const bound_const_volatile_mem_functor7<T_return, T_obj, T_arg1,T_arg2,T_arg3,T_arg4,T_arg5,T_arg6,T_arg7>& _A_functor)
{ return retype_functor<bound_const_volatile_mem_functor7<T_return, T_obj, T_arg1, T_arg2, T_arg3, T_arg4, T_arg5, T_arg6, T_arg7>, T_arg1,T_arg2,T_arg3,T_arg4,T_arg5,T_arg6,T_arg7 >
    (_A_functor); }


} /* namespace sigc */
#endif /* _SIGC_ADAPTORS_MACROS_RETYPEHM4_ */
