// -*- c++ -*-
/* Do not edit! -- generated file */
#ifndef _SIGC_ADAPTORS_MACROS_BIND_RETURNHM4_
#define _SIGC_ADAPTORS_MACROS_BIND_RETURNHM4_
#include <sigc++/adaptors/adaptor_trait.h>
#include <sigc++/adaptors/bound_argument.h>

namespace sigc {

/** Adaptor that fixes the return value of the wrapped functor.
 * Use the convenience function sigc::bind_return() to create an instance of sigc::bind_return_functor.
 *
 * The following template arguments are used:
 * - @e T_return Type of the fixed return value.
 * - @e T_functor Type of the functor to wrap.
 *
 * @ingroup bind
 */
template <class T_return, class T_functor>
struct bind_return_functor : public adapts<T_functor>
{
  template <class T_arg1=void,class T_arg2=void,class T_arg3=void,class T_arg4=void,class T_arg5=void,class T_arg6=void,class T_arg7=void>
  struct deduce_result_type
    { typedef typename unwrap_reference<T_return>::type type; };
  typedef typename unwrap_reference<T_return>::type result_type;

  /** Invokes the wrapped functor dropping its return value.
   * @return The fixed return value.
   */
  typename unwrap_reference<T_return>::type operator()();

  /** Invokes the wrapped functor passing on the arguments.,
   * @param _A_arg%1 Argument to be passed on to the functor.)
   * @return The fixed return value.
   */
  template <class T_arg1>
  inline typename unwrap_reference<T_return>::type operator()(T_arg1 _A_a1)
    { this->functor_.SIGC_WORKAROUND_OPERATOR_PARENTHESES<typename type_trait<T_arg1>::pass>
        (_A_a1); return ret_value_.invoke();
    }

  #ifndef SIGC_TEMPLATE_SPECIALIZATION_OPERATOR_OVERLOAD
  template <class T_arg1>
  inline typename unwrap_reference<T_return>::type sun_forte_workaround(T_arg1 _A_a1)
    { this->functor_.SIGC_WORKAROUND_OPERATOR_PARENTHESES<typename type_trait<T_arg1>::pass>
        (_A_a1); return ret_value_.invoke();
    }
  #endif

  /** Invokes the wrapped functor passing on the arguments.,
   * @param _A_arg%1 Argument to be passed on to the functor.)
   * @return The fixed return value.
   */
  template <class T_arg1,class T_arg2>
  inline typename unwrap_reference<T_return>::type operator()(T_arg1 _A_a1,T_arg2 _A_a2)
    { this->functor_.SIGC_WORKAROUND_OPERATOR_PARENTHESES<typename type_trait<T_arg1>::pass,typename type_trait<T_arg2>::pass>
        (_A_a1,_A_a2); return ret_value_.invoke();
    }

  #ifndef SIGC_TEMPLATE_SPECIALIZATION_OPERATOR_OVERLOAD
  template <class T_arg1,class T_arg2>
  inline typename unwrap_reference<T_return>::type sun_forte_workaround(T_arg1 _A_a1,T_arg2 _A_a2)
    { this->functor_.SIGC_WORKAROUND_OPERATOR_PARENTHESES<typename type_trait<T_arg1>::pass,typename type_trait<T_arg2>::pass>
        (_A_a1,_A_a2); return ret_value_.invoke();
    }
  #endif

  /** Invokes the wrapped functor passing on the arguments.,
   * @param _A_arg%1 Argument to be passed on to the functor.)
   * @return The fixed return value.
   */
  template <class T_arg1,class T_arg2,class T_arg3>
  inline typename unwrap_reference<T_return>::type operator()(T_arg1 _A_a1,T_arg2 _A_a2,T_arg3 _A_a3)
    { this->functor_.SIGC_WORKAROUND_OPERATOR_PARENTHESES<typename type_trait<T_arg1>::pass,typename type_trait<T_arg2>::pass,typename type_trait<T_arg3>::pass>
        (_A_a1,_A_a2,_A_a3); return ret_value_.invoke();
    }

  #ifndef SIGC_TEMPLATE_SPECIALIZATION_OPERATOR_OVERLOAD
  template <class T_arg1,class T_arg2,class T_arg3>
  inline typename unwrap_reference<T_return>::type sun_forte_workaround(T_arg1 _A_a1,T_arg2 _A_a2,T_arg3 _A_a3)
    { this->functor_.SIGC_WORKAROUND_OPERATOR_PARENTHESES<typename type_trait<T_arg1>::pass,typename type_trait<T_arg2>::pass,typename type_trait<T_arg3>::pass>
        (_A_a1,_A_a2,_A_a3); return ret_value_.invoke();
    }
  #endif

  /** Invokes the wrapped functor passing on the arguments.,
   * @param _A_arg%1 Argument to be passed on to the functor.)
   * @return The fixed return value.
   */
  template <class T_arg1,class T_arg2,class T_arg3,class T_arg4>
  inline typename unwrap_reference<T_return>::type operator()(T_arg1 _A_a1,T_arg2 _A_a2,T_arg3 _A_a3,T_arg4 _A_a4)
    { this->functor_.SIGC_WORKAROUND_OPERATOR_PARENTHESES<typename type_trait<T_arg1>::pass,typename type_trait<T_arg2>::pass,typename type_trait<T_arg3>::pass,typename type_trait<T_arg4>::pass>
        (_A_a1,_A_a2,_A_a3,_A_a4); return ret_value_.invoke();
    }

  #ifndef SIGC_TEMPLATE_SPECIALIZATION_OPERATOR_OVERLOAD
  template <class T_arg1,class T_arg2,class T_arg3,class T_arg4>
  inline typename unwrap_reference<T_return>::type sun_forte_workaround(T_arg1 _A_a1,T_arg2 _A_a2,T_arg3 _A_a3,T_arg4 _A_a4)
    { this->functor_.SIGC_WORKAROUND_OPERATOR_PARENTHESES<typename type_trait<T_arg1>::pass,typename type_trait<T_arg2>::pass,typename type_trait<T_arg3>::pass,typename type_trait<T_arg4>::pass>
        (_A_a1,_A_a2,_A_a3,_A_a4); return ret_value_.invoke();
    }
  #endif

  /** Invokes the wrapped functor passing on the arguments.,
   * @param _A_arg%1 Argument to be passed on to the functor.)
   * @return The fixed return value.
   */
  template <class T_arg1,class T_arg2,class T_arg3,class T_arg4,class T_arg5>
  inline typename unwrap_reference<T_return>::type operator()(T_arg1 _A_a1,T_arg2 _A_a2,T_arg3 _A_a3,T_arg4 _A_a4,T_arg5 _A_a5)
    { this->functor_.SIGC_WORKAROUND_OPERATOR_PARENTHESES<typename type_trait<T_arg1>::pass,typename type_trait<T_arg2>::pass,typename type_trait<T_arg3>::pass,typename type_trait<T_arg4>::pass,typename type_trait<T_arg5>::pass>
        (_A_a1,_A_a2,_A_a3,_A_a4,_A_a5); return ret_value_.invoke();
    }

  #ifndef SIGC_TEMPLATE_SPECIALIZATION_OPERATOR_OVERLOAD
  template <class T_arg1,class T_arg2,class T_arg3,class T_arg4,class T_arg5>
  inline typename unwrap_reference<T_return>::type sun_forte_workaround(T_arg1 _A_a1,T_arg2 _A_a2,T_arg3 _A_a3,T_arg4 _A_a4,T_arg5 _A_a5)
    { this->functor_.SIGC_WORKAROUND_OPERATOR_PARENTHESES<typename type_trait<T_arg1>::pass,typename type_trait<T_arg2>::pass,typename type_trait<T_arg3>::pass,typename type_trait<T_arg4>::pass,typename type_trait<T_arg5>::pass>
        (_A_a1,_A_a2,_A_a3,_A_a4,_A_a5); return ret_value_.invoke();
    }
  #endif

  /** Invokes the wrapped functor passing on the arguments.,
   * @param _A_arg%1 Argument to be passed on to the functor.)
   * @return The fixed return value.
   */
  template <class T_arg1,class T_arg2,class T_arg3,class T_arg4,class T_arg5,class T_arg6>
  inline typename unwrap_reference<T_return>::type operator()(T_arg1 _A_a1,T_arg2 _A_a2,T_arg3 _A_a3,T_arg4 _A_a4,T_arg5 _A_a5,T_arg6 _A_a6)
    { this->functor_.SIGC_WORKAROUND_OPERATOR_PARENTHESES<typename type_trait<T_arg1>::pass,typename type_trait<T_arg2>::pass,typename type_trait<T_arg3>::pass,typename type_trait<T_arg4>::pass,typename type_trait<T_arg5>::pass,typename type_trait<T_arg6>::pass>
        (_A_a1,_A_a2,_A_a3,_A_a4,_A_a5,_A_a6); return ret_value_.invoke();
    }

  #ifndef SIGC_TEMPLATE_SPECIALIZATION_OPERATOR_OVERLOAD
  template <class T_arg1,class T_arg2,class T_arg3,class T_arg4,class T_arg5,class T_arg6>
  inline typename unwrap_reference<T_return>::type sun_forte_workaround(T_arg1 _A_a1,T_arg2 _A_a2,T_arg3 _A_a3,T_arg4 _A_a4,T_arg5 _A_a5,T_arg6 _A_a6)
    { this->functor_.SIGC_WORKAROUND_OPERATOR_PARENTHESES<typename type_trait<T_arg1>::pass,typename type_trait<T_arg2>::pass,typename type_trait<T_arg3>::pass,typename type_trait<T_arg4>::pass,typename type_trait<T_arg5>::pass,typename type_trait<T_arg6>::pass>
        (_A_a1,_A_a2,_A_a3,_A_a4,_A_a5,_A_a6); return ret_value_.invoke();
    }
  #endif

  /** Invokes the wrapped functor passing on the arguments.,
   * @param _A_arg%1 Argument to be passed on to the functor.)
   * @return The fixed return value.
   */
  template <class T_arg1,class T_arg2,class T_arg3,class T_arg4,class T_arg5,class T_arg6,class T_arg7>
  inline typename unwrap_reference<T_return>::type operator()(T_arg1 _A_a1,T_arg2 _A_a2,T_arg3 _A_a3,T_arg4 _A_a4,T_arg5 _A_a5,T_arg6 _A_a6,T_arg7 _A_a7)
    { this->functor_.SIGC_WORKAROUND_OPERATOR_PARENTHESES<typename type_trait<T_arg1>::pass,typename type_trait<T_arg2>::pass,typename type_trait<T_arg3>::pass,typename type_trait<T_arg4>::pass,typename type_trait<T_arg5>::pass,typename type_trait<T_arg6>::pass,typename type_trait<T_arg7>::pass>
        (_A_a1,_A_a2,_A_a3,_A_a4,_A_a5,_A_a6,_A_a7); return ret_value_.invoke();
    }

  #ifndef SIGC_TEMPLATE_SPECIALIZATION_OPERATOR_OVERLOAD
  template <class T_arg1,class T_arg2,class T_arg3,class T_arg4,class T_arg5,class T_arg6,class T_arg7>
  inline typename unwrap_reference<T_return>::type sun_forte_workaround(T_arg1 _A_a1,T_arg2 _A_a2,T_arg3 _A_a3,T_arg4 _A_a4,T_arg5 _A_a5,T_arg6 _A_a6,T_arg7 _A_a7)
    { this->functor_.SIGC_WORKAROUND_OPERATOR_PARENTHESES<typename type_trait<T_arg1>::pass,typename type_trait<T_arg2>::pass,typename type_trait<T_arg3>::pass,typename type_trait<T_arg4>::pass,typename type_trait<T_arg5>::pass,typename type_trait<T_arg6>::pass,typename type_trait<T_arg7>::pass>
        (_A_a1,_A_a2,_A_a3,_A_a4,_A_a5,_A_a6,_A_a7); return ret_value_.invoke();
    }
  #endif


  /** Constructs a bind_return_functor object that fixes the return value to @p _A_ret_value.
   * @param _A_functor Functor to invoke from operator()().
   * @param _A_ret_value Value to return from operator()().
   */
  bind_return_functor(typename type_trait<T_functor>::take _A_functor, typename type_trait<T_return>::take _A_ret_value)
    : adapts<T_functor>(_A_functor), ret_value_(_A_ret_value)
    {}

  /// The fixed return value.
  bound_argument<T_return> ret_value_; // public, so that visit_each() can access it
};

template <class T_return, class T_functor>
typename unwrap_reference<T_return>::type bind_return_functor<T_return, T_functor>::operator()()
  { this->functor_(); return ret_value_.invoke(); }


//template specialization of visit_each<>(action, functor):
/** Performs a functor on each of the targets of a functor.
 * The function overload for sigc::bind_return_functor performs a functor on the
 * functor and on the object instance stored in the sigc::bind_return_functor object.
 *
 * @ingroup bind
 */
template <class T_action, class T_return, class T_functor>
void visit_each(const T_action& _A_action,
                const bind_return_functor<T_return, T_functor>& _A_target)
{
  visit_each(_A_action, _A_target.ret_value_);
  visit_each(_A_action, _A_target.functor_);
}


/** Creates an adaptor of type sigc::bind_return_functor which fixes the return value of the passed functor to the passed argument.
 *
 * @param _A_functor Functor that should be wrapped.
 * @param _A_ret_value Argument to fix the return value of @e _A_functor to.
 * @return Adaptor that executes @e _A_functor on invokation and returns @e _A_ret_value.
 *
 * @ingroup bind
 */
template <class T_return, class T_functor>
inline bind_return_functor<T_return, T_functor>
bind_return(const T_functor& _A_functor, T_return _A_ret_value)
{ return bind_return_functor<T_return, T_functor>(_A_functor, _A_ret_value); }

} /* namespace sigc */
#endif /* _SIGC_ADAPTORS_MACROS_BIND_RETURNHM4_ */
