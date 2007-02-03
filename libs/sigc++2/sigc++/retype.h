// -*- c++ -*-
/* Do not edit! -- generated file */
#ifndef _SIGC_MACROS_RETYPEHM4_
#define _SIGC_MACROS_RETYPEHM4_
#include <sigc++/adaptors/adaptor_trait.h>
#include <sigc++/slot.h>

#ifndef LIBSIGC_DISABLE_DEPRECATED

namespace SigC {

template <class T_functor, class T_return, class T_type1=::sigc::nil,class T_type2=::sigc::nil,class T_type3=::sigc::nil,class T_type4=::sigc::nil,class T_type5=::sigc::nil,class T_type6=::sigc::nil,class T_type7=::sigc::nil>
struct retype_slot_functor
  : public ::sigc::adapts<T_functor>
{
  template <class T_arg1=void,class T_arg2=void,class T_arg3=void,class T_arg4=void,class T_arg5=void,class T_arg6=void,class T_arg7=void>
  struct deduce_result_type
    { typedef T_return type; };
  typedef T_return result_type;

  T_return operator()();

  template <class T_arg1>
  inline T_return operator()(T_arg1 _A_a1)
    { return T_return(this->functor_.SIGC_WORKAROUND_OPERATOR_PARENTHESES<typename ::sigc::type_trait<T_type1>::take>
        (static_cast<T_type1>(_A_a1)));
    }

  #ifndef SIGC_TEMPLATE_SPECIALIZATION_OPERATOR_OVERLOAD
  template <class T_arg1>
  inline T_return sun_forte_workaround(T_arg1 _A_a1)
    { return T_return(this->functor_.SIGC_WORKAROUND_OPERATOR_PARENTHESES<typename ::sigc::type_trait<T_type1>::take>
        (static_cast<T_type1>(_A_a1)));
    }
  #endif
  
  template <class T_arg1,class T_arg2>
  inline T_return operator()(T_arg1 _A_a1,T_arg2 _A_a2)
    { return T_return(this->functor_.SIGC_WORKAROUND_OPERATOR_PARENTHESES<typename ::sigc::type_trait<T_type1>::take,typename ::sigc::type_trait<T_type2>::take>
        (static_cast<T_type1>(_A_a1),static_cast<T_type2>(_A_a2)));
    }

  #ifndef SIGC_TEMPLATE_SPECIALIZATION_OPERATOR_OVERLOAD
  template <class T_arg1,class T_arg2>
  inline T_return sun_forte_workaround(T_arg1 _A_a1,T_arg2 _A_a2)
    { return T_return(this->functor_.SIGC_WORKAROUND_OPERATOR_PARENTHESES<typename ::sigc::type_trait<T_type1>::take,typename ::sigc::type_trait<T_type2>::take>
        (static_cast<T_type1>(_A_a1),static_cast<T_type2>(_A_a2)));
    }
  #endif
  
  template <class T_arg1,class T_arg2,class T_arg3>
  inline T_return operator()(T_arg1 _A_a1,T_arg2 _A_a2,T_arg3 _A_a3)
    { return T_return(this->functor_.SIGC_WORKAROUND_OPERATOR_PARENTHESES<typename ::sigc::type_trait<T_type1>::take,typename ::sigc::type_trait<T_type2>::take,typename ::sigc::type_trait<T_type3>::take>
        (static_cast<T_type1>(_A_a1),static_cast<T_type2>(_A_a2),static_cast<T_type3>(_A_a3)));
    }

  #ifndef SIGC_TEMPLATE_SPECIALIZATION_OPERATOR_OVERLOAD
  template <class T_arg1,class T_arg2,class T_arg3>
  inline T_return sun_forte_workaround(T_arg1 _A_a1,T_arg2 _A_a2,T_arg3 _A_a3)
    { return T_return(this->functor_.SIGC_WORKAROUND_OPERATOR_PARENTHESES<typename ::sigc::type_trait<T_type1>::take,typename ::sigc::type_trait<T_type2>::take,typename ::sigc::type_trait<T_type3>::take>
        (static_cast<T_type1>(_A_a1),static_cast<T_type2>(_A_a2),static_cast<T_type3>(_A_a3)));
    }
  #endif
  
  template <class T_arg1,class T_arg2,class T_arg3,class T_arg4>
  inline T_return operator()(T_arg1 _A_a1,T_arg2 _A_a2,T_arg3 _A_a3,T_arg4 _A_a4)
    { return T_return(this->functor_.SIGC_WORKAROUND_OPERATOR_PARENTHESES<typename ::sigc::type_trait<T_type1>::take,typename ::sigc::type_trait<T_type2>::take,typename ::sigc::type_trait<T_type3>::take,typename ::sigc::type_trait<T_type4>::take>
        (static_cast<T_type1>(_A_a1),static_cast<T_type2>(_A_a2),static_cast<T_type3>(_A_a3),static_cast<T_type4>(_A_a4)));
    }

  #ifndef SIGC_TEMPLATE_SPECIALIZATION_OPERATOR_OVERLOAD
  template <class T_arg1,class T_arg2,class T_arg3,class T_arg4>
  inline T_return sun_forte_workaround(T_arg1 _A_a1,T_arg2 _A_a2,T_arg3 _A_a3,T_arg4 _A_a4)
    { return T_return(this->functor_.SIGC_WORKAROUND_OPERATOR_PARENTHESES<typename ::sigc::type_trait<T_type1>::take,typename ::sigc::type_trait<T_type2>::take,typename ::sigc::type_trait<T_type3>::take,typename ::sigc::type_trait<T_type4>::take>
        (static_cast<T_type1>(_A_a1),static_cast<T_type2>(_A_a2),static_cast<T_type3>(_A_a3),static_cast<T_type4>(_A_a4)));
    }
  #endif
  
  template <class T_arg1,class T_arg2,class T_arg3,class T_arg4,class T_arg5>
  inline T_return operator()(T_arg1 _A_a1,T_arg2 _A_a2,T_arg3 _A_a3,T_arg4 _A_a4,T_arg5 _A_a5)
    { return T_return(this->functor_.SIGC_WORKAROUND_OPERATOR_PARENTHESES<typename ::sigc::type_trait<T_type1>::take,typename ::sigc::type_trait<T_type2>::take,typename ::sigc::type_trait<T_type3>::take,typename ::sigc::type_trait<T_type4>::take,typename ::sigc::type_trait<T_type5>::take>
        (static_cast<T_type1>(_A_a1),static_cast<T_type2>(_A_a2),static_cast<T_type3>(_A_a3),static_cast<T_type4>(_A_a4),static_cast<T_type5>(_A_a5)));
    }

  #ifndef SIGC_TEMPLATE_SPECIALIZATION_OPERATOR_OVERLOAD
  template <class T_arg1,class T_arg2,class T_arg3,class T_arg4,class T_arg5>
  inline T_return sun_forte_workaround(T_arg1 _A_a1,T_arg2 _A_a2,T_arg3 _A_a3,T_arg4 _A_a4,T_arg5 _A_a5)
    { return T_return(this->functor_.SIGC_WORKAROUND_OPERATOR_PARENTHESES<typename ::sigc::type_trait<T_type1>::take,typename ::sigc::type_trait<T_type2>::take,typename ::sigc::type_trait<T_type3>::take,typename ::sigc::type_trait<T_type4>::take,typename ::sigc::type_trait<T_type5>::take>
        (static_cast<T_type1>(_A_a1),static_cast<T_type2>(_A_a2),static_cast<T_type3>(_A_a3),static_cast<T_type4>(_A_a4),static_cast<T_type5>(_A_a5)));
    }
  #endif
  
  template <class T_arg1,class T_arg2,class T_arg3,class T_arg4,class T_arg5,class T_arg6>
  inline T_return operator()(T_arg1 _A_a1,T_arg2 _A_a2,T_arg3 _A_a3,T_arg4 _A_a4,T_arg5 _A_a5,T_arg6 _A_a6)
    { return T_return(this->functor_.SIGC_WORKAROUND_OPERATOR_PARENTHESES<typename ::sigc::type_trait<T_type1>::take,typename ::sigc::type_trait<T_type2>::take,typename ::sigc::type_trait<T_type3>::take,typename ::sigc::type_trait<T_type4>::take,typename ::sigc::type_trait<T_type5>::take,typename ::sigc::type_trait<T_type6>::take>
        (static_cast<T_type1>(_A_a1),static_cast<T_type2>(_A_a2),static_cast<T_type3>(_A_a3),static_cast<T_type4>(_A_a4),static_cast<T_type5>(_A_a5),static_cast<T_type6>(_A_a6)));
    }

  #ifndef SIGC_TEMPLATE_SPECIALIZATION_OPERATOR_OVERLOAD
  template <class T_arg1,class T_arg2,class T_arg3,class T_arg4,class T_arg5,class T_arg6>
  inline T_return sun_forte_workaround(T_arg1 _A_a1,T_arg2 _A_a2,T_arg3 _A_a3,T_arg4 _A_a4,T_arg5 _A_a5,T_arg6 _A_a6)
    { return T_return(this->functor_.SIGC_WORKAROUND_OPERATOR_PARENTHESES<typename ::sigc::type_trait<T_type1>::take,typename ::sigc::type_trait<T_type2>::take,typename ::sigc::type_trait<T_type3>::take,typename ::sigc::type_trait<T_type4>::take,typename ::sigc::type_trait<T_type5>::take,typename ::sigc::type_trait<T_type6>::take>
        (static_cast<T_type1>(_A_a1),static_cast<T_type2>(_A_a2),static_cast<T_type3>(_A_a3),static_cast<T_type4>(_A_a4),static_cast<T_type5>(_A_a5),static_cast<T_type6>(_A_a6)));
    }
  #endif
  
  template <class T_arg1,class T_arg2,class T_arg3,class T_arg4,class T_arg5,class T_arg6,class T_arg7>
  inline T_return operator()(T_arg1 _A_a1,T_arg2 _A_a2,T_arg3 _A_a3,T_arg4 _A_a4,T_arg5 _A_a5,T_arg6 _A_a6,T_arg7 _A_a7)
    { return T_return(this->functor_.SIGC_WORKAROUND_OPERATOR_PARENTHESES<typename ::sigc::type_trait<T_type1>::take,typename ::sigc::type_trait<T_type2>::take,typename ::sigc::type_trait<T_type3>::take,typename ::sigc::type_trait<T_type4>::take,typename ::sigc::type_trait<T_type5>::take,typename ::sigc::type_trait<T_type6>::take,typename ::sigc::type_trait<T_type7>::take>
        (static_cast<T_type1>(_A_a1),static_cast<T_type2>(_A_a2),static_cast<T_type3>(_A_a3),static_cast<T_type4>(_A_a4),static_cast<T_type5>(_A_a5),static_cast<T_type6>(_A_a6),static_cast<T_type7>(_A_a7)));
    }

  #ifndef SIGC_TEMPLATE_SPECIALIZATION_OPERATOR_OVERLOAD
  template <class T_arg1,class T_arg2,class T_arg3,class T_arg4,class T_arg5,class T_arg6,class T_arg7>
  inline T_return sun_forte_workaround(T_arg1 _A_a1,T_arg2 _A_a2,T_arg3 _A_a3,T_arg4 _A_a4,T_arg5 _A_a5,T_arg6 _A_a6,T_arg7 _A_a7)
    { return T_return(this->functor_.SIGC_WORKAROUND_OPERATOR_PARENTHESES<typename ::sigc::type_trait<T_type1>::take,typename ::sigc::type_trait<T_type2>::take,typename ::sigc::type_trait<T_type3>::take,typename ::sigc::type_trait<T_type4>::take,typename ::sigc::type_trait<T_type5>::take,typename ::sigc::type_trait<T_type6>::take,typename ::sigc::type_trait<T_type7>::take>
        (static_cast<T_type1>(_A_a1),static_cast<T_type2>(_A_a2),static_cast<T_type3>(_A_a3),static_cast<T_type4>(_A_a4),static_cast<T_type5>(_A_a5),static_cast<T_type6>(_A_a6),static_cast<T_type7>(_A_a7)));
    }
  #endif
  

  retype_slot_functor(typename ::sigc::type_trait<T_functor>::take _A_functor)
    : ::sigc::adapts<T_functor>(_A_functor)
    {}
};

template <class T_functor, class T_return, class T_type1,class T_type2,class T_type3,class T_type4,class T_type5,class T_type6,class T_type7>
T_return retype_slot_functor<T_functor, T_return, T_type1,T_type2,T_type3,T_type4,T_type5,T_type6,T_type7>::operator()()
  { return T_return(this->functor_()); }


// void specialization needed because of explicit cast to T_return
template <class T_functor, class T_type1,class T_type2,class T_type3,class T_type4,class T_type5,class T_type6,class T_type7>
struct retype_slot_functor<T_functor, void, T_type1,T_type2,T_type3,T_type4,T_type5,T_type6,T_type7>
  : public ::sigc::adapts<T_functor>
{
  template <class T_arg1=void,class T_arg2=void,class T_arg3=void,class T_arg4=void,class T_arg5=void,class T_arg6=void,class T_arg7=void>
  struct deduce_result_type
    { typedef void type; };
  typedef void result_type;

  void operator()();

  template <class T_arg1>
  inline void operator()(T_arg1 _A_a1)
    { T_return(this->functor_.SIGC_WORKAROUND_OPERATOR_PARENTHESES<typename ::sigc::type_trait<T_type1>::take>
        (static_cast<T_type1>(_A_a1)));
    }

  #ifndef SIGC_TEMPLATE_SPECIALIZATION_OPERATOR_OVERLOAD
  template <class T_arg1>
  inline void sun_forte_workaround(T_arg1 _A_a1)
    { T_return(this->functor_.SIGC_WORKAROUND_OPERATOR_PARENTHESES<typename ::sigc::type_trait<T_type1>::take>
        (static_cast<T_type1>(_A_a1)));
    }
  #endif
    
  template <class T_arg1,class T_arg2>
  inline void operator()(T_arg1 _A_a1,T_arg2 _A_a2)
    { T_return(this->functor_.SIGC_WORKAROUND_OPERATOR_PARENTHESES<typename ::sigc::type_trait<T_type1>::take,typename ::sigc::type_trait<T_type2>::take>
        (static_cast<T_type1>(_A_a1),static_cast<T_type2>(_A_a2)));
    }

  #ifndef SIGC_TEMPLATE_SPECIALIZATION_OPERATOR_OVERLOAD
  template <class T_arg1,class T_arg2>
  inline void sun_forte_workaround(T_arg1 _A_a1,T_arg2 _A_a2)
    { T_return(this->functor_.SIGC_WORKAROUND_OPERATOR_PARENTHESES<typename ::sigc::type_trait<T_type1>::take,typename ::sigc::type_trait<T_type2>::take>
        (static_cast<T_type1>(_A_a1),static_cast<T_type2>(_A_a2)));
    }
  #endif
    
  template <class T_arg1,class T_arg2,class T_arg3>
  inline void operator()(T_arg1 _A_a1,T_arg2 _A_a2,T_arg3 _A_a3)
    { T_return(this->functor_.SIGC_WORKAROUND_OPERATOR_PARENTHESES<typename ::sigc::type_trait<T_type1>::take,typename ::sigc::type_trait<T_type2>::take,typename ::sigc::type_trait<T_type3>::take>
        (static_cast<T_type1>(_A_a1),static_cast<T_type2>(_A_a2),static_cast<T_type3>(_A_a3)));
    }

  #ifndef SIGC_TEMPLATE_SPECIALIZATION_OPERATOR_OVERLOAD
  template <class T_arg1,class T_arg2,class T_arg3>
  inline void sun_forte_workaround(T_arg1 _A_a1,T_arg2 _A_a2,T_arg3 _A_a3)
    { T_return(this->functor_.SIGC_WORKAROUND_OPERATOR_PARENTHESES<typename ::sigc::type_trait<T_type1>::take,typename ::sigc::type_trait<T_type2>::take,typename ::sigc::type_trait<T_type3>::take>
        (static_cast<T_type1>(_A_a1),static_cast<T_type2>(_A_a2),static_cast<T_type3>(_A_a3)));
    }
  #endif
    
  template <class T_arg1,class T_arg2,class T_arg3,class T_arg4>
  inline void operator()(T_arg1 _A_a1,T_arg2 _A_a2,T_arg3 _A_a3,T_arg4 _A_a4)
    { T_return(this->functor_.SIGC_WORKAROUND_OPERATOR_PARENTHESES<typename ::sigc::type_trait<T_type1>::take,typename ::sigc::type_trait<T_type2>::take,typename ::sigc::type_trait<T_type3>::take,typename ::sigc::type_trait<T_type4>::take>
        (static_cast<T_type1>(_A_a1),static_cast<T_type2>(_A_a2),static_cast<T_type3>(_A_a3),static_cast<T_type4>(_A_a4)));
    }

  #ifndef SIGC_TEMPLATE_SPECIALIZATION_OPERATOR_OVERLOAD
  template <class T_arg1,class T_arg2,class T_arg3,class T_arg4>
  inline void sun_forte_workaround(T_arg1 _A_a1,T_arg2 _A_a2,T_arg3 _A_a3,T_arg4 _A_a4)
    { T_return(this->functor_.SIGC_WORKAROUND_OPERATOR_PARENTHESES<typename ::sigc::type_trait<T_type1>::take,typename ::sigc::type_trait<T_type2>::take,typename ::sigc::type_trait<T_type3>::take,typename ::sigc::type_trait<T_type4>::take>
        (static_cast<T_type1>(_A_a1),static_cast<T_type2>(_A_a2),static_cast<T_type3>(_A_a3),static_cast<T_type4>(_A_a4)));
    }
  #endif
    
  template <class T_arg1,class T_arg2,class T_arg3,class T_arg4,class T_arg5>
  inline void operator()(T_arg1 _A_a1,T_arg2 _A_a2,T_arg3 _A_a3,T_arg4 _A_a4,T_arg5 _A_a5)
    { T_return(this->functor_.SIGC_WORKAROUND_OPERATOR_PARENTHESES<typename ::sigc::type_trait<T_type1>::take,typename ::sigc::type_trait<T_type2>::take,typename ::sigc::type_trait<T_type3>::take,typename ::sigc::type_trait<T_type4>::take,typename ::sigc::type_trait<T_type5>::take>
        (static_cast<T_type1>(_A_a1),static_cast<T_type2>(_A_a2),static_cast<T_type3>(_A_a3),static_cast<T_type4>(_A_a4),static_cast<T_type5>(_A_a5)));
    }

  #ifndef SIGC_TEMPLATE_SPECIALIZATION_OPERATOR_OVERLOAD
  template <class T_arg1,class T_arg2,class T_arg3,class T_arg4,class T_arg5>
  inline void sun_forte_workaround(T_arg1 _A_a1,T_arg2 _A_a2,T_arg3 _A_a3,T_arg4 _A_a4,T_arg5 _A_a5)
    { T_return(this->functor_.SIGC_WORKAROUND_OPERATOR_PARENTHESES<typename ::sigc::type_trait<T_type1>::take,typename ::sigc::type_trait<T_type2>::take,typename ::sigc::type_trait<T_type3>::take,typename ::sigc::type_trait<T_type4>::take,typename ::sigc::type_trait<T_type5>::take>
        (static_cast<T_type1>(_A_a1),static_cast<T_type2>(_A_a2),static_cast<T_type3>(_A_a3),static_cast<T_type4>(_A_a4),static_cast<T_type5>(_A_a5)));
    }
  #endif
    
  template <class T_arg1,class T_arg2,class T_arg3,class T_arg4,class T_arg5,class T_arg6>
  inline void operator()(T_arg1 _A_a1,T_arg2 _A_a2,T_arg3 _A_a3,T_arg4 _A_a4,T_arg5 _A_a5,T_arg6 _A_a6)
    { T_return(this->functor_.SIGC_WORKAROUND_OPERATOR_PARENTHESES<typename ::sigc::type_trait<T_type1>::take,typename ::sigc::type_trait<T_type2>::take,typename ::sigc::type_trait<T_type3>::take,typename ::sigc::type_trait<T_type4>::take,typename ::sigc::type_trait<T_type5>::take,typename ::sigc::type_trait<T_type6>::take>
        (static_cast<T_type1>(_A_a1),static_cast<T_type2>(_A_a2),static_cast<T_type3>(_A_a3),static_cast<T_type4>(_A_a4),static_cast<T_type5>(_A_a5),static_cast<T_type6>(_A_a6)));
    }

  #ifndef SIGC_TEMPLATE_SPECIALIZATION_OPERATOR_OVERLOAD
  template <class T_arg1,class T_arg2,class T_arg3,class T_arg4,class T_arg5,class T_arg6>
  inline void sun_forte_workaround(T_arg1 _A_a1,T_arg2 _A_a2,T_arg3 _A_a3,T_arg4 _A_a4,T_arg5 _A_a5,T_arg6 _A_a6)
    { T_return(this->functor_.SIGC_WORKAROUND_OPERATOR_PARENTHESES<typename ::sigc::type_trait<T_type1>::take,typename ::sigc::type_trait<T_type2>::take,typename ::sigc::type_trait<T_type3>::take,typename ::sigc::type_trait<T_type4>::take,typename ::sigc::type_trait<T_type5>::take,typename ::sigc::type_trait<T_type6>::take>
        (static_cast<T_type1>(_A_a1),static_cast<T_type2>(_A_a2),static_cast<T_type3>(_A_a3),static_cast<T_type4>(_A_a4),static_cast<T_type5>(_A_a5),static_cast<T_type6>(_A_a6)));
    }
  #endif
    
  template <class T_arg1,class T_arg2,class T_arg3,class T_arg4,class T_arg5,class T_arg6,class T_arg7>
  inline void operator()(T_arg1 _A_a1,T_arg2 _A_a2,T_arg3 _A_a3,T_arg4 _A_a4,T_arg5 _A_a5,T_arg6 _A_a6,T_arg7 _A_a7)
    { T_return(this->functor_.SIGC_WORKAROUND_OPERATOR_PARENTHESES<typename ::sigc::type_trait<T_type1>::take,typename ::sigc::type_trait<T_type2>::take,typename ::sigc::type_trait<T_type3>::take,typename ::sigc::type_trait<T_type4>::take,typename ::sigc::type_trait<T_type5>::take,typename ::sigc::type_trait<T_type6>::take,typename ::sigc::type_trait<T_type7>::take>
        (static_cast<T_type1>(_A_a1),static_cast<T_type2>(_A_a2),static_cast<T_type3>(_A_a3),static_cast<T_type4>(_A_a4),static_cast<T_type5>(_A_a5),static_cast<T_type6>(_A_a6),static_cast<T_type7>(_A_a7)));
    }

  #ifndef SIGC_TEMPLATE_SPECIALIZATION_OPERATOR_OVERLOAD
  template <class T_arg1,class T_arg2,class T_arg3,class T_arg4,class T_arg5,class T_arg6,class T_arg7>
  inline void sun_forte_workaround(T_arg1 _A_a1,T_arg2 _A_a2,T_arg3 _A_a3,T_arg4 _A_a4,T_arg5 _A_a5,T_arg6 _A_a6,T_arg7 _A_a7)
    { T_return(this->functor_.SIGC_WORKAROUND_OPERATOR_PARENTHESES<typename ::sigc::type_trait<T_type1>::take,typename ::sigc::type_trait<T_type2>::take,typename ::sigc::type_trait<T_type3>::take,typename ::sigc::type_trait<T_type4>::take,typename ::sigc::type_trait<T_type5>::take,typename ::sigc::type_trait<T_type6>::take,typename ::sigc::type_trait<T_type7>::take>
        (static_cast<T_type1>(_A_a1),static_cast<T_type2>(_A_a2),static_cast<T_type3>(_A_a3),static_cast<T_type4>(_A_a4),static_cast<T_type5>(_A_a5),static_cast<T_type6>(_A_a6),static_cast<T_type7>(_A_a7)));
    }
  #endif
    

  retype_slot_functor(typename ::sigc::type_trait<T_functor>::take _A_functor)
    : ::sigc::adapts<T_functor>(_A_functor)
    {}
};

template <class T_functor, class T_type1,class T_type2,class T_type3,class T_type4,class T_type5,class T_type6,class T_type7>
void retype_slot_functor<T_functor, void, T_type1,T_type2,T_type3,T_type4,T_type5,T_type6,T_type7>::operator()()
  { this->functor_(); }


//template specialization of visit_each<>(action, functor):
template <class T_action, class T_functor, class T_return, class T_type1,class T_type2,class T_type3,class T_type4,class T_type5,class T_type6,class T_type7>
void visit_each(const T_action& _A_action,
                const retype_slot_functor<T_functor, T_return, T_type1,T_type2,T_type3,T_type4,T_type5,T_type6,T_type7>& _A_target)
{
  visit_each(_A_action, _A_target.functor_);
}


template <class T_return, class T_ret>
inline Slot0<T_return>
retype(const Slot0<T_ret>& _A_slot)
{ return Slot0<T_return>
    (retype_slot_functor<Slot0<T_ret>, T_return>
      (_A_slot)); }

template <class T_return, class T_arg1, class T_ret, class T_type1>
inline Slot1<T_return, T_arg1>
retype(const Slot1<T_ret, T_type1>& _A_slot)
{ return Slot1<T_return, T_arg1>
    (retype_slot_functor<Slot1<T_ret, T_type1>, T_return, T_type1>
      (_A_slot)); }

template <class T_return, class T_arg1,class T_arg2, class T_ret, class T_type1,class T_type2>
inline Slot2<T_return, T_arg1,T_arg2>
retype(const Slot2<T_ret, T_type1,T_type2>& _A_slot)
{ return Slot2<T_return, T_arg1,T_arg2>
    (retype_slot_functor<Slot2<T_ret, T_type1, T_type2>, T_return, T_type1,T_type2>
      (_A_slot)); }

template <class T_return, class T_arg1,class T_arg2,class T_arg3, class T_ret, class T_type1,class T_type2,class T_type3>
inline Slot3<T_return, T_arg1,T_arg2,T_arg3>
retype(const Slot3<T_ret, T_type1,T_type2,T_type3>& _A_slot)
{ return Slot3<T_return, T_arg1,T_arg2,T_arg3>
    (retype_slot_functor<Slot3<T_ret, T_type1, T_type2, T_type3>, T_return, T_type1,T_type2,T_type3>
      (_A_slot)); }

template <class T_return, class T_arg1,class T_arg2,class T_arg3,class T_arg4, class T_ret, class T_type1,class T_type2,class T_type3,class T_type4>
inline Slot4<T_return, T_arg1,T_arg2,T_arg3,T_arg4>
retype(const Slot4<T_ret, T_type1,T_type2,T_type3,T_type4>& _A_slot)
{ return Slot4<T_return, T_arg1,T_arg2,T_arg3,T_arg4>
    (retype_slot_functor<Slot4<T_ret, T_type1, T_type2, T_type3, T_type4>, T_return, T_type1,T_type2,T_type3,T_type4>
      (_A_slot)); }

template <class T_return, class T_arg1,class T_arg2,class T_arg3,class T_arg4,class T_arg5, class T_ret, class T_type1,class T_type2,class T_type3,class T_type4,class T_type5>
inline Slot5<T_return, T_arg1,T_arg2,T_arg3,T_arg4,T_arg5>
retype(const Slot5<T_ret, T_type1,T_type2,T_type3,T_type4,T_type5>& _A_slot)
{ return Slot5<T_return, T_arg1,T_arg2,T_arg3,T_arg4,T_arg5>
    (retype_slot_functor<Slot5<T_ret, T_type1, T_type2, T_type3, T_type4, T_type5>, T_return, T_type1,T_type2,T_type3,T_type4,T_type5>
      (_A_slot)); }

template <class T_return, class T_arg1,class T_arg2,class T_arg3,class T_arg4,class T_arg5,class T_arg6, class T_ret, class T_type1,class T_type2,class T_type3,class T_type4,class T_type5,class T_type6>
inline Slot6<T_return, T_arg1,T_arg2,T_arg3,T_arg4,T_arg5,T_arg6>
retype(const Slot6<T_ret, T_type1,T_type2,T_type3,T_type4,T_type5,T_type6>& _A_slot)
{ return Slot6<T_return, T_arg1,T_arg2,T_arg3,T_arg4,T_arg5,T_arg6>
    (retype_slot_functor<Slot6<T_ret, T_type1, T_type2, T_type3, T_type4, T_type5, T_type6>, T_return, T_type1,T_type2,T_type3,T_type4,T_type5,T_type6>
      (_A_slot)); }

template <class T_return, class T_arg1,class T_arg2,class T_arg3,class T_arg4,class T_arg5,class T_arg6,class T_arg7, class T_ret, class T_type1,class T_type2,class T_type3,class T_type4,class T_type5,class T_type6,class T_type7>
inline Slot7<T_return, T_arg1,T_arg2,T_arg3,T_arg4,T_arg5,T_arg6,T_arg7>
retype(const Slot7<T_ret, T_type1,T_type2,T_type3,T_type4,T_type5,T_type6,T_type7>& _A_slot)
{ return Slot7<T_return, T_arg1,T_arg2,T_arg3,T_arg4,T_arg5,T_arg6,T_arg7>
    (retype_slot_functor<Slot7<T_ret, T_type1, T_type2, T_type3, T_type4, T_type5, T_type6, T_type7>, T_return, T_type1,T_type2,T_type3,T_type4,T_type5,T_type6,T_type7>
      (_A_slot)); }


} /* namespace SigC */

#endif /* LIBSIGC_DISABLE_DEPRECATED */
#endif /* _SIGC_MACROS_RETYPEHM4_ */
