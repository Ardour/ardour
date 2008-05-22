// -*- c++ -*-
/* Do not edit! -- generated file */
#ifndef _SIGC_ADAPTORS_MACROS_RETYPE_RETURNHM4_
#define _SIGC_ADAPTORS_MACROS_RETYPE_RETURNHM4_
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
  template <class T_arg1=void,class T_arg2=void,class T_arg3=void,class T_arg4=void,class T_arg5=void,class T_arg6=void,class T_arg7=void>
  struct deduce_result_type
    { typedef T_return type; };
  typedef T_return result_type;

  T_return operator()();

  template <class T_arg1>
  inline T_return operator()(T_arg1 _A_a1)
    { return T_return(this->functor_.SIGC_WORKAROUND_OPERATOR_PARENTHESES<typename type_trait<T_arg1>::pass>
        (_A_a1));
    }

  #ifndef SIGC_TEMPLATE_SPECIALIZATION_OPERATOR_OVERLOAD
  template <class T_arg1>
  inline T_return sun_forte_workaround(T_arg1 _A_a1)
    { return T_return(this->functor_.SIGC_WORKAROUND_OPERATOR_PARENTHESES<typename type_trait<T_arg1>::pass>
        (_A_a1));
    }
  #endif
    
  template <class T_arg1,class T_arg2>
  inline T_return operator()(T_arg1 _A_a1,T_arg2 _A_a2)
    { return T_return(this->functor_.SIGC_WORKAROUND_OPERATOR_PARENTHESES<typename type_trait<T_arg1>::pass,typename type_trait<T_arg2>::pass>
        (_A_a1,_A_a2));
    }

  #ifndef SIGC_TEMPLATE_SPECIALIZATION_OPERATOR_OVERLOAD
  template <class T_arg1,class T_arg2>
  inline T_return sun_forte_workaround(T_arg1 _A_a1,T_arg2 _A_a2)
    { return T_return(this->functor_.SIGC_WORKAROUND_OPERATOR_PARENTHESES<typename type_trait<T_arg1>::pass,typename type_trait<T_arg2>::pass>
        (_A_a1,_A_a2));
    }
  #endif
    
  template <class T_arg1,class T_arg2,class T_arg3>
  inline T_return operator()(T_arg1 _A_a1,T_arg2 _A_a2,T_arg3 _A_a3)
    { return T_return(this->functor_.SIGC_WORKAROUND_OPERATOR_PARENTHESES<typename type_trait<T_arg1>::pass,typename type_trait<T_arg2>::pass,typename type_trait<T_arg3>::pass>
        (_A_a1,_A_a2,_A_a3));
    }

  #ifndef SIGC_TEMPLATE_SPECIALIZATION_OPERATOR_OVERLOAD
  template <class T_arg1,class T_arg2,class T_arg3>
  inline T_return sun_forte_workaround(T_arg1 _A_a1,T_arg2 _A_a2,T_arg3 _A_a3)
    { return T_return(this->functor_.SIGC_WORKAROUND_OPERATOR_PARENTHESES<typename type_trait<T_arg1>::pass,typename type_trait<T_arg2>::pass,typename type_trait<T_arg3>::pass>
        (_A_a1,_A_a2,_A_a3));
    }
  #endif
    
  template <class T_arg1,class T_arg2,class T_arg3,class T_arg4>
  inline T_return operator()(T_arg1 _A_a1,T_arg2 _A_a2,T_arg3 _A_a3,T_arg4 _A_a4)
    { return T_return(this->functor_.SIGC_WORKAROUND_OPERATOR_PARENTHESES<typename type_trait<T_arg1>::pass,typename type_trait<T_arg2>::pass,typename type_trait<T_arg3>::pass,typename type_trait<T_arg4>::pass>
        (_A_a1,_A_a2,_A_a3,_A_a4));
    }

  #ifndef SIGC_TEMPLATE_SPECIALIZATION_OPERATOR_OVERLOAD
  template <class T_arg1,class T_arg2,class T_arg3,class T_arg4>
  inline T_return sun_forte_workaround(T_arg1 _A_a1,T_arg2 _A_a2,T_arg3 _A_a3,T_arg4 _A_a4)
    { return T_return(this->functor_.SIGC_WORKAROUND_OPERATOR_PARENTHESES<typename type_trait<T_arg1>::pass,typename type_trait<T_arg2>::pass,typename type_trait<T_arg3>::pass,typename type_trait<T_arg4>::pass>
        (_A_a1,_A_a2,_A_a3,_A_a4));
    }
  #endif
    
  template <class T_arg1,class T_arg2,class T_arg3,class T_arg4,class T_arg5>
  inline T_return operator()(T_arg1 _A_a1,T_arg2 _A_a2,T_arg3 _A_a3,T_arg4 _A_a4,T_arg5 _A_a5)
    { return T_return(this->functor_.SIGC_WORKAROUND_OPERATOR_PARENTHESES<typename type_trait<T_arg1>::pass,typename type_trait<T_arg2>::pass,typename type_trait<T_arg3>::pass,typename type_trait<T_arg4>::pass,typename type_trait<T_arg5>::pass>
        (_A_a1,_A_a2,_A_a3,_A_a4,_A_a5));
    }

  #ifndef SIGC_TEMPLATE_SPECIALIZATION_OPERATOR_OVERLOAD
  template <class T_arg1,class T_arg2,class T_arg3,class T_arg4,class T_arg5>
  inline T_return sun_forte_workaround(T_arg1 _A_a1,T_arg2 _A_a2,T_arg3 _A_a3,T_arg4 _A_a4,T_arg5 _A_a5)
    { return T_return(this->functor_.SIGC_WORKAROUND_OPERATOR_PARENTHESES<typename type_trait<T_arg1>::pass,typename type_trait<T_arg2>::pass,typename type_trait<T_arg3>::pass,typename type_trait<T_arg4>::pass,typename type_trait<T_arg5>::pass>
        (_A_a1,_A_a2,_A_a3,_A_a4,_A_a5));
    }
  #endif
    
  template <class T_arg1,class T_arg2,class T_arg3,class T_arg4,class T_arg5,class T_arg6>
  inline T_return operator()(T_arg1 _A_a1,T_arg2 _A_a2,T_arg3 _A_a3,T_arg4 _A_a4,T_arg5 _A_a5,T_arg6 _A_a6)
    { return T_return(this->functor_.SIGC_WORKAROUND_OPERATOR_PARENTHESES<typename type_trait<T_arg1>::pass,typename type_trait<T_arg2>::pass,typename type_trait<T_arg3>::pass,typename type_trait<T_arg4>::pass,typename type_trait<T_arg5>::pass,typename type_trait<T_arg6>::pass>
        (_A_a1,_A_a2,_A_a3,_A_a4,_A_a5,_A_a6));
    }

  #ifndef SIGC_TEMPLATE_SPECIALIZATION_OPERATOR_OVERLOAD
  template <class T_arg1,class T_arg2,class T_arg3,class T_arg4,class T_arg5,class T_arg6>
  inline T_return sun_forte_workaround(T_arg1 _A_a1,T_arg2 _A_a2,T_arg3 _A_a3,T_arg4 _A_a4,T_arg5 _A_a5,T_arg6 _A_a6)
    { return T_return(this->functor_.SIGC_WORKAROUND_OPERATOR_PARENTHESES<typename type_trait<T_arg1>::pass,typename type_trait<T_arg2>::pass,typename type_trait<T_arg3>::pass,typename type_trait<T_arg4>::pass,typename type_trait<T_arg5>::pass,typename type_trait<T_arg6>::pass>
        (_A_a1,_A_a2,_A_a3,_A_a4,_A_a5,_A_a6));
    }
  #endif
    
  template <class T_arg1,class T_arg2,class T_arg3,class T_arg4,class T_arg5,class T_arg6,class T_arg7>
  inline T_return operator()(T_arg1 _A_a1,T_arg2 _A_a2,T_arg3 _A_a3,T_arg4 _A_a4,T_arg5 _A_a5,T_arg6 _A_a6,T_arg7 _A_a7)
    { return T_return(this->functor_.SIGC_WORKAROUND_OPERATOR_PARENTHESES<typename type_trait<T_arg1>::pass,typename type_trait<T_arg2>::pass,typename type_trait<T_arg3>::pass,typename type_trait<T_arg4>::pass,typename type_trait<T_arg5>::pass,typename type_trait<T_arg6>::pass,typename type_trait<T_arg7>::pass>
        (_A_a1,_A_a2,_A_a3,_A_a4,_A_a5,_A_a6,_A_a7));
    }

  #ifndef SIGC_TEMPLATE_SPECIALIZATION_OPERATOR_OVERLOAD
  template <class T_arg1,class T_arg2,class T_arg3,class T_arg4,class T_arg5,class T_arg6,class T_arg7>
  inline T_return sun_forte_workaround(T_arg1 _A_a1,T_arg2 _A_a2,T_arg3 _A_a3,T_arg4 _A_a4,T_arg5 _A_a5,T_arg6 _A_a6,T_arg7 _A_a7)
    { return T_return(this->functor_.SIGC_WORKAROUND_OPERATOR_PARENTHESES<typename type_trait<T_arg1>::pass,typename type_trait<T_arg2>::pass,typename type_trait<T_arg3>::pass,typename type_trait<T_arg4>::pass,typename type_trait<T_arg5>::pass,typename type_trait<T_arg6>::pass,typename type_trait<T_arg7>::pass>
        (_A_a1,_A_a2,_A_a3,_A_a4,_A_a5,_A_a6,_A_a7));
    }
  #endif
    
  retype_return_functor() {}

  /** Constructs a retype_return_functor object that perform a C-style cast on the return value of the passed functor.
   * @param _A_functor Functor to invoke from operator()().
   */
  explicit retype_return_functor(typename type_trait<T_functor>::take _A_functor)
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
  template <class T_arg1=void,class T_arg2=void,class T_arg3=void,class T_arg4=void,class T_arg5=void,class T_arg6=void,class T_arg7=void>
  struct deduce_result_type
    { typedef void type; };
  typedef void result_type;

  void operator()();

  template <class T_arg1>
  inline void operator()(T_arg1 _A_a1)
    { this->functor_.SIGC_WORKAROUND_OPERATOR_PARENTHESES<typename type_trait<T_arg1>::pass>
        (_A_a1);
    }

  #ifndef SIGC_TEMPLATE_SPECIALIZATION_OPERATOR_OVERLOAD
  template <class T_arg1>
  inline void sun_forte_workaround(T_arg1 _A_a1)
    { this->functor_.SIGC_WORKAROUND_OPERATOR_PARENTHESES<typename type_trait<T_arg1>::pass>
        (_A_a1);
    }
  #endif

  template <class T_arg1,class T_arg2>
  inline void operator()(T_arg1 _A_a1,T_arg2 _A_a2)
    { this->functor_.SIGC_WORKAROUND_OPERATOR_PARENTHESES<typename type_trait<T_arg1>::pass,typename type_trait<T_arg2>::pass>
        (_A_a1,_A_a2);
    }

  #ifndef SIGC_TEMPLATE_SPECIALIZATION_OPERATOR_OVERLOAD
  template <class T_arg1,class T_arg2>
  inline void sun_forte_workaround(T_arg1 _A_a1,T_arg2 _A_a2)
    { this->functor_.SIGC_WORKAROUND_OPERATOR_PARENTHESES<typename type_trait<T_arg1>::pass,typename type_trait<T_arg2>::pass>
        (_A_a1,_A_a2);
    }
  #endif

  template <class T_arg1,class T_arg2,class T_arg3>
  inline void operator()(T_arg1 _A_a1,T_arg2 _A_a2,T_arg3 _A_a3)
    { this->functor_.SIGC_WORKAROUND_OPERATOR_PARENTHESES<typename type_trait<T_arg1>::pass,typename type_trait<T_arg2>::pass,typename type_trait<T_arg3>::pass>
        (_A_a1,_A_a2,_A_a3);
    }

  #ifndef SIGC_TEMPLATE_SPECIALIZATION_OPERATOR_OVERLOAD
  template <class T_arg1,class T_arg2,class T_arg3>
  inline void sun_forte_workaround(T_arg1 _A_a1,T_arg2 _A_a2,T_arg3 _A_a3)
    { this->functor_.SIGC_WORKAROUND_OPERATOR_PARENTHESES<typename type_trait<T_arg1>::pass,typename type_trait<T_arg2>::pass,typename type_trait<T_arg3>::pass>
        (_A_a1,_A_a2,_A_a3);
    }
  #endif

  template <class T_arg1,class T_arg2,class T_arg3,class T_arg4>
  inline void operator()(T_arg1 _A_a1,T_arg2 _A_a2,T_arg3 _A_a3,T_arg4 _A_a4)
    { this->functor_.SIGC_WORKAROUND_OPERATOR_PARENTHESES<typename type_trait<T_arg1>::pass,typename type_trait<T_arg2>::pass,typename type_trait<T_arg3>::pass,typename type_trait<T_arg4>::pass>
        (_A_a1,_A_a2,_A_a3,_A_a4);
    }

  #ifndef SIGC_TEMPLATE_SPECIALIZATION_OPERATOR_OVERLOAD
  template <class T_arg1,class T_arg2,class T_arg3,class T_arg4>
  inline void sun_forte_workaround(T_arg1 _A_a1,T_arg2 _A_a2,T_arg3 _A_a3,T_arg4 _A_a4)
    { this->functor_.SIGC_WORKAROUND_OPERATOR_PARENTHESES<typename type_trait<T_arg1>::pass,typename type_trait<T_arg2>::pass,typename type_trait<T_arg3>::pass,typename type_trait<T_arg4>::pass>
        (_A_a1,_A_a2,_A_a3,_A_a4);
    }
  #endif

  template <class T_arg1,class T_arg2,class T_arg3,class T_arg4,class T_arg5>
  inline void operator()(T_arg1 _A_a1,T_arg2 _A_a2,T_arg3 _A_a3,T_arg4 _A_a4,T_arg5 _A_a5)
    { this->functor_.SIGC_WORKAROUND_OPERATOR_PARENTHESES<typename type_trait<T_arg1>::pass,typename type_trait<T_arg2>::pass,typename type_trait<T_arg3>::pass,typename type_trait<T_arg4>::pass,typename type_trait<T_arg5>::pass>
        (_A_a1,_A_a2,_A_a3,_A_a4,_A_a5);
    }

  #ifndef SIGC_TEMPLATE_SPECIALIZATION_OPERATOR_OVERLOAD
  template <class T_arg1,class T_arg2,class T_arg3,class T_arg4,class T_arg5>
  inline void sun_forte_workaround(T_arg1 _A_a1,T_arg2 _A_a2,T_arg3 _A_a3,T_arg4 _A_a4,T_arg5 _A_a5)
    { this->functor_.SIGC_WORKAROUND_OPERATOR_PARENTHESES<typename type_trait<T_arg1>::pass,typename type_trait<T_arg2>::pass,typename type_trait<T_arg3>::pass,typename type_trait<T_arg4>::pass,typename type_trait<T_arg5>::pass>
        (_A_a1,_A_a2,_A_a3,_A_a4,_A_a5);
    }
  #endif

  template <class T_arg1,class T_arg2,class T_arg3,class T_arg4,class T_arg5,class T_arg6>
  inline void operator()(T_arg1 _A_a1,T_arg2 _A_a2,T_arg3 _A_a3,T_arg4 _A_a4,T_arg5 _A_a5,T_arg6 _A_a6)
    { this->functor_.SIGC_WORKAROUND_OPERATOR_PARENTHESES<typename type_trait<T_arg1>::pass,typename type_trait<T_arg2>::pass,typename type_trait<T_arg3>::pass,typename type_trait<T_arg4>::pass,typename type_trait<T_arg5>::pass,typename type_trait<T_arg6>::pass>
        (_A_a1,_A_a2,_A_a3,_A_a4,_A_a5,_A_a6);
    }

  #ifndef SIGC_TEMPLATE_SPECIALIZATION_OPERATOR_OVERLOAD
  template <class T_arg1,class T_arg2,class T_arg3,class T_arg4,class T_arg5,class T_arg6>
  inline void sun_forte_workaround(T_arg1 _A_a1,T_arg2 _A_a2,T_arg3 _A_a3,T_arg4 _A_a4,T_arg5 _A_a5,T_arg6 _A_a6)
    { this->functor_.SIGC_WORKAROUND_OPERATOR_PARENTHESES<typename type_trait<T_arg1>::pass,typename type_trait<T_arg2>::pass,typename type_trait<T_arg3>::pass,typename type_trait<T_arg4>::pass,typename type_trait<T_arg5>::pass,typename type_trait<T_arg6>::pass>
        (_A_a1,_A_a2,_A_a3,_A_a4,_A_a5,_A_a6);
    }
  #endif

  template <class T_arg1,class T_arg2,class T_arg3,class T_arg4,class T_arg5,class T_arg6,class T_arg7>
  inline void operator()(T_arg1 _A_a1,T_arg2 _A_a2,T_arg3 _A_a3,T_arg4 _A_a4,T_arg5 _A_a5,T_arg6 _A_a6,T_arg7 _A_a7)
    { this->functor_.SIGC_WORKAROUND_OPERATOR_PARENTHESES<typename type_trait<T_arg1>::pass,typename type_trait<T_arg2>::pass,typename type_trait<T_arg3>::pass,typename type_trait<T_arg4>::pass,typename type_trait<T_arg5>::pass,typename type_trait<T_arg6>::pass,typename type_trait<T_arg7>::pass>
        (_A_a1,_A_a2,_A_a3,_A_a4,_A_a5,_A_a6,_A_a7);
    }

  #ifndef SIGC_TEMPLATE_SPECIALIZATION_OPERATOR_OVERLOAD
  template <class T_arg1,class T_arg2,class T_arg3,class T_arg4,class T_arg5,class T_arg6,class T_arg7>
  inline void sun_forte_workaround(T_arg1 _A_a1,T_arg2 _A_a2,T_arg3 _A_a3,T_arg4 _A_a4,T_arg5 _A_a5,T_arg6 _A_a6,T_arg7 _A_a7)
    { this->functor_.SIGC_WORKAROUND_OPERATOR_PARENTHESES<typename type_trait<T_arg1>::pass,typename type_trait<T_arg2>::pass,typename type_trait<T_arg3>::pass,typename type_trait<T_arg4>::pass,typename type_trait<T_arg5>::pass,typename type_trait<T_arg6>::pass,typename type_trait<T_arg7>::pass>
        (_A_a1,_A_a2,_A_a3,_A_a4,_A_a5,_A_a6,_A_a7);
    }
  #endif

  retype_return_functor() {}
  retype_return_functor(typename type_trait<T_functor>::take _A_functor)
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
#endif /* _SIGC_ADAPTORS_MACROS_RETYPE_RETURNHM4_ */
