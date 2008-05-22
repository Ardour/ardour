// -*- c++ -*-
/* Do not edit! -- generated file */
#ifndef _SIGC_LAMBDA_SELECT_HPP_
#define _SIGC_LAMBDA_SELECT_HPP_
#include <sigc++/adaptors/lambda/base.h>

namespace sigc {

namespace internal {
struct lambda_select1 : public lambda_base
{
  template <class T_arg1=void,class T_arg2=void,class T_arg3=void,class T_arg4=void,class T_arg5=void,class T_arg6=void,class T_arg7=void>
  struct deduce_result_type
    { typedef T_arg1 type; };
  typedef void result_type; // no operator ()() overload

  void operator ()() const; // not implemented
  template <class T_arg1>
  T_arg1 operator ()(T_arg1 _A_1) const { return _A_1; }

  #ifndef SIGC_TEMPLATE_SPECIALIZATION_OPERATOR_OVERLOAD
  template <class T_arg1>
  //Does not work: T_arg1 sun_forte_workaround(T_arg1 _A_1) const { return operator()( _A_1 ); }
  T_arg1 sun_forte_workaround(T_arg1 _A_1) const { return _A_1; }
  #endif
  
  template <class T_arg1,class T_arg2>
  T_arg1 operator ()(T_arg1 _A_1, T_arg2) const { return _A_1; }

  #ifndef SIGC_TEMPLATE_SPECIALIZATION_OPERATOR_OVERLOAD
  template <class T_arg1,class T_arg2>
  //Does not work: T_arg1 sun_forte_workaround(T_arg1 _A_1,T_arg2 _A_2) const { return operator()( _A_1,_A_2 ); }
  T_arg1 sun_forte_workaround(T_arg1 _A_1, T_arg2) const { return _A_1; }
  #endif
  
  template <class T_arg1,class T_arg2,class T_arg3>
  T_arg1 operator ()(T_arg1 _A_1, T_arg2, T_arg3) const { return _A_1; }

  #ifndef SIGC_TEMPLATE_SPECIALIZATION_OPERATOR_OVERLOAD
  template <class T_arg1,class T_arg2,class T_arg3>
  //Does not work: T_arg1 sun_forte_workaround(T_arg1 _A_1,T_arg2 _A_2,T_arg3 _A_3) const { return operator()( _A_1,_A_2,_A_3 ); }
  T_arg1 sun_forte_workaround(T_arg1 _A_1, T_arg2, T_arg3) const { return _A_1; }
  #endif
  
  template <class T_arg1,class T_arg2,class T_arg3,class T_arg4>
  T_arg1 operator ()(T_arg1 _A_1, T_arg2, T_arg3, T_arg4) const { return _A_1; }

  #ifndef SIGC_TEMPLATE_SPECIALIZATION_OPERATOR_OVERLOAD
  template <class T_arg1,class T_arg2,class T_arg3,class T_arg4>
  //Does not work: T_arg1 sun_forte_workaround(T_arg1 _A_1,T_arg2 _A_2,T_arg3 _A_3,T_arg4 _A_4) const { return operator()( _A_1,_A_2,_A_3,_A_4 ); }
  T_arg1 sun_forte_workaround(T_arg1 _A_1, T_arg2, T_arg3, T_arg4) const { return _A_1; }
  #endif
  
  template <class T_arg1,class T_arg2,class T_arg3,class T_arg4,class T_arg5>
  T_arg1 operator ()(T_arg1 _A_1, T_arg2, T_arg3, T_arg4, T_arg5) const { return _A_1; }

  #ifndef SIGC_TEMPLATE_SPECIALIZATION_OPERATOR_OVERLOAD
  template <class T_arg1,class T_arg2,class T_arg3,class T_arg4,class T_arg5>
  //Does not work: T_arg1 sun_forte_workaround(T_arg1 _A_1,T_arg2 _A_2,T_arg3 _A_3,T_arg4 _A_4,T_arg5 _A_5) const { return operator()( _A_1,_A_2,_A_3,_A_4,_A_5 ); }
  T_arg1 sun_forte_workaround(T_arg1 _A_1, T_arg2, T_arg3, T_arg4, T_arg5) const { return _A_1; }
  #endif
  
  template <class T_arg1,class T_arg2,class T_arg3,class T_arg4,class T_arg5,class T_arg6>
  T_arg1 operator ()(T_arg1 _A_1, T_arg2, T_arg3, T_arg4, T_arg5, T_arg6) const { return _A_1; }

  #ifndef SIGC_TEMPLATE_SPECIALIZATION_OPERATOR_OVERLOAD
  template <class T_arg1,class T_arg2,class T_arg3,class T_arg4,class T_arg5,class T_arg6>
  //Does not work: T_arg1 sun_forte_workaround(T_arg1 _A_1,T_arg2 _A_2,T_arg3 _A_3,T_arg4 _A_4,T_arg5 _A_5,T_arg6 _A_6) const { return operator()( _A_1,_A_2,_A_3,_A_4,_A_5,_A_6 ); }
  T_arg1 sun_forte_workaround(T_arg1 _A_1, T_arg2, T_arg3, T_arg4, T_arg5, T_arg6) const { return _A_1; }
  #endif
  
  template <class T_arg1,class T_arg2,class T_arg3,class T_arg4,class T_arg5,class T_arg6,class T_arg7>
  T_arg1 operator ()(T_arg1 _A_1, T_arg2, T_arg3, T_arg4, T_arg5, T_arg6, T_arg7) const { return _A_1; }

  #ifndef SIGC_TEMPLATE_SPECIALIZATION_OPERATOR_OVERLOAD
  template <class T_arg1,class T_arg2,class T_arg3,class T_arg4,class T_arg5,class T_arg6,class T_arg7>
  //Does not work: T_arg1 sun_forte_workaround(T_arg1 _A_1,T_arg2 _A_2,T_arg3 _A_3,T_arg4 _A_4,T_arg5 _A_5,T_arg6 _A_6,T_arg7 _A_7) const { return operator()( _A_1,_A_2,_A_3,_A_4,_A_5,_A_6,_A_7 ); }
  T_arg1 sun_forte_workaround(T_arg1 _A_1, T_arg2, T_arg3, T_arg4, T_arg5, T_arg6, T_arg7) const { return _A_1; }
  #endif
  
};

struct lambda_select2 : public lambda_base
{
  template <class T_arg1=void,class T_arg2=void,class T_arg3=void,class T_arg4=void,class T_arg5=void,class T_arg6=void,class T_arg7=void>
  struct deduce_result_type
    { typedef T_arg2 type; };
  typedef void result_type; // no operator ()() overload

  void operator ()() const; // not implemented
  template <class T_arg1,class T_arg2>
  T_arg2 operator ()(T_arg1, T_arg2 _A_2) const { return _A_2; }

  #ifndef SIGC_TEMPLATE_SPECIALIZATION_OPERATOR_OVERLOAD
  template <class T_arg1,class T_arg2>
  //Does not work: T_arg2 sun_forte_workaround(T_arg1 _A_1,T_arg2 _A_2) const { return operator()( _A_1,_A_2 ); }
  T_arg2 sun_forte_workaround(T_arg1, T_arg2 _A_2) const { return _A_2; }
  #endif
  
  template <class T_arg1,class T_arg2,class T_arg3>
  T_arg2 operator ()(T_arg1, T_arg2 _A_2, T_arg3) const { return _A_2; }

  #ifndef SIGC_TEMPLATE_SPECIALIZATION_OPERATOR_OVERLOAD
  template <class T_arg1,class T_arg2,class T_arg3>
  //Does not work: T_arg2 sun_forte_workaround(T_arg1 _A_1,T_arg2 _A_2,T_arg3 _A_3) const { return operator()( _A_1,_A_2,_A_3 ); }
  T_arg2 sun_forte_workaround(T_arg1, T_arg2 _A_2, T_arg3) const { return _A_2; }
  #endif
  
  template <class T_arg1,class T_arg2,class T_arg3,class T_arg4>
  T_arg2 operator ()(T_arg1, T_arg2 _A_2, T_arg3, T_arg4) const { return _A_2; }

  #ifndef SIGC_TEMPLATE_SPECIALIZATION_OPERATOR_OVERLOAD
  template <class T_arg1,class T_arg2,class T_arg3,class T_arg4>
  //Does not work: T_arg2 sun_forte_workaround(T_arg1 _A_1,T_arg2 _A_2,T_arg3 _A_3,T_arg4 _A_4) const { return operator()( _A_1,_A_2,_A_3,_A_4 ); }
  T_arg2 sun_forte_workaround(T_arg1, T_arg2 _A_2, T_arg3, T_arg4) const { return _A_2; }
  #endif
  
  template <class T_arg1,class T_arg2,class T_arg3,class T_arg4,class T_arg5>
  T_arg2 operator ()(T_arg1, T_arg2 _A_2, T_arg3, T_arg4, T_arg5) const { return _A_2; }

  #ifndef SIGC_TEMPLATE_SPECIALIZATION_OPERATOR_OVERLOAD
  template <class T_arg1,class T_arg2,class T_arg3,class T_arg4,class T_arg5>
  //Does not work: T_arg2 sun_forte_workaround(T_arg1 _A_1,T_arg2 _A_2,T_arg3 _A_3,T_arg4 _A_4,T_arg5 _A_5) const { return operator()( _A_1,_A_2,_A_3,_A_4,_A_5 ); }
  T_arg2 sun_forte_workaround(T_arg1, T_arg2 _A_2, T_arg3, T_arg4, T_arg5) const { return _A_2; }
  #endif
  
  template <class T_arg1,class T_arg2,class T_arg3,class T_arg4,class T_arg5,class T_arg6>
  T_arg2 operator ()(T_arg1, T_arg2 _A_2, T_arg3, T_arg4, T_arg5, T_arg6) const { return _A_2; }

  #ifndef SIGC_TEMPLATE_SPECIALIZATION_OPERATOR_OVERLOAD
  template <class T_arg1,class T_arg2,class T_arg3,class T_arg4,class T_arg5,class T_arg6>
  //Does not work: T_arg2 sun_forte_workaround(T_arg1 _A_1,T_arg2 _A_2,T_arg3 _A_3,T_arg4 _A_4,T_arg5 _A_5,T_arg6 _A_6) const { return operator()( _A_1,_A_2,_A_3,_A_4,_A_5,_A_6 ); }
  T_arg2 sun_forte_workaround(T_arg1, T_arg2 _A_2, T_arg3, T_arg4, T_arg5, T_arg6) const { return _A_2; }
  #endif
  
  template <class T_arg1,class T_arg2,class T_arg3,class T_arg4,class T_arg5,class T_arg6,class T_arg7>
  T_arg2 operator ()(T_arg1, T_arg2 _A_2, T_arg3, T_arg4, T_arg5, T_arg6, T_arg7) const { return _A_2; }

  #ifndef SIGC_TEMPLATE_SPECIALIZATION_OPERATOR_OVERLOAD
  template <class T_arg1,class T_arg2,class T_arg3,class T_arg4,class T_arg5,class T_arg6,class T_arg7>
  //Does not work: T_arg2 sun_forte_workaround(T_arg1 _A_1,T_arg2 _A_2,T_arg3 _A_3,T_arg4 _A_4,T_arg5 _A_5,T_arg6 _A_6,T_arg7 _A_7) const { return operator()( _A_1,_A_2,_A_3,_A_4,_A_5,_A_6,_A_7 ); }
  T_arg2 sun_forte_workaround(T_arg1, T_arg2 _A_2, T_arg3, T_arg4, T_arg5, T_arg6, T_arg7) const { return _A_2; }
  #endif
  
};

struct lambda_select3 : public lambda_base
{
  template <class T_arg1=void,class T_arg2=void,class T_arg3=void,class T_arg4=void,class T_arg5=void,class T_arg6=void,class T_arg7=void>
  struct deduce_result_type
    { typedef T_arg3 type; };
  typedef void result_type; // no operator ()() overload

  void operator ()() const; // not implemented
  template <class T_arg1,class T_arg2,class T_arg3>
  T_arg3 operator ()(T_arg1, T_arg2, T_arg3 _A_3) const { return _A_3; }

  #ifndef SIGC_TEMPLATE_SPECIALIZATION_OPERATOR_OVERLOAD
  template <class T_arg1,class T_arg2,class T_arg3>
  //Does not work: T_arg3 sun_forte_workaround(T_arg1 _A_1,T_arg2 _A_2,T_arg3 _A_3) const { return operator()( _A_1,_A_2,_A_3 ); }
  T_arg3 sun_forte_workaround(T_arg1, T_arg2, T_arg3 _A_3) const { return _A_3; }
  #endif
  
  template <class T_arg1,class T_arg2,class T_arg3,class T_arg4>
  T_arg3 operator ()(T_arg1, T_arg2, T_arg3 _A_3, T_arg4) const { return _A_3; }

  #ifndef SIGC_TEMPLATE_SPECIALIZATION_OPERATOR_OVERLOAD
  template <class T_arg1,class T_arg2,class T_arg3,class T_arg4>
  //Does not work: T_arg3 sun_forte_workaround(T_arg1 _A_1,T_arg2 _A_2,T_arg3 _A_3,T_arg4 _A_4) const { return operator()( _A_1,_A_2,_A_3,_A_4 ); }
  T_arg3 sun_forte_workaround(T_arg1, T_arg2, T_arg3 _A_3, T_arg4) const { return _A_3; }
  #endif
  
  template <class T_arg1,class T_arg2,class T_arg3,class T_arg4,class T_arg5>
  T_arg3 operator ()(T_arg1, T_arg2, T_arg3 _A_3, T_arg4, T_arg5) const { return _A_3; }

  #ifndef SIGC_TEMPLATE_SPECIALIZATION_OPERATOR_OVERLOAD
  template <class T_arg1,class T_arg2,class T_arg3,class T_arg4,class T_arg5>
  //Does not work: T_arg3 sun_forte_workaround(T_arg1 _A_1,T_arg2 _A_2,T_arg3 _A_3,T_arg4 _A_4,T_arg5 _A_5) const { return operator()( _A_1,_A_2,_A_3,_A_4,_A_5 ); }
  T_arg3 sun_forte_workaround(T_arg1, T_arg2, T_arg3 _A_3, T_arg4, T_arg5) const { return _A_3; }
  #endif
  
  template <class T_arg1,class T_arg2,class T_arg3,class T_arg4,class T_arg5,class T_arg6>
  T_arg3 operator ()(T_arg1, T_arg2, T_arg3 _A_3, T_arg4, T_arg5, T_arg6) const { return _A_3; }

  #ifndef SIGC_TEMPLATE_SPECIALIZATION_OPERATOR_OVERLOAD
  template <class T_arg1,class T_arg2,class T_arg3,class T_arg4,class T_arg5,class T_arg6>
  //Does not work: T_arg3 sun_forte_workaround(T_arg1 _A_1,T_arg2 _A_2,T_arg3 _A_3,T_arg4 _A_4,T_arg5 _A_5,T_arg6 _A_6) const { return operator()( _A_1,_A_2,_A_3,_A_4,_A_5,_A_6 ); }
  T_arg3 sun_forte_workaround(T_arg1, T_arg2, T_arg3 _A_3, T_arg4, T_arg5, T_arg6) const { return _A_3; }
  #endif
  
  template <class T_arg1,class T_arg2,class T_arg3,class T_arg4,class T_arg5,class T_arg6,class T_arg7>
  T_arg3 operator ()(T_arg1, T_arg2, T_arg3 _A_3, T_arg4, T_arg5, T_arg6, T_arg7) const { return _A_3; }

  #ifndef SIGC_TEMPLATE_SPECIALIZATION_OPERATOR_OVERLOAD
  template <class T_arg1,class T_arg2,class T_arg3,class T_arg4,class T_arg5,class T_arg6,class T_arg7>
  //Does not work: T_arg3 sun_forte_workaround(T_arg1 _A_1,T_arg2 _A_2,T_arg3 _A_3,T_arg4 _A_4,T_arg5 _A_5,T_arg6 _A_6,T_arg7 _A_7) const { return operator()( _A_1,_A_2,_A_3,_A_4,_A_5,_A_6,_A_7 ); }
  T_arg3 sun_forte_workaround(T_arg1, T_arg2, T_arg3 _A_3, T_arg4, T_arg5, T_arg6, T_arg7) const { return _A_3; }
  #endif
  
};

struct lambda_select4 : public lambda_base
{
  template <class T_arg1=void,class T_arg2=void,class T_arg3=void,class T_arg4=void,class T_arg5=void,class T_arg6=void,class T_arg7=void>
  struct deduce_result_type
    { typedef T_arg4 type; };
  typedef void result_type; // no operator ()() overload

  void operator ()() const; // not implemented
  template <class T_arg1,class T_arg2,class T_arg3,class T_arg4>
  T_arg4 operator ()(T_arg1, T_arg2, T_arg3, T_arg4 _A_4) const { return _A_4; }

  #ifndef SIGC_TEMPLATE_SPECIALIZATION_OPERATOR_OVERLOAD
  template <class T_arg1,class T_arg2,class T_arg3,class T_arg4>
  //Does not work: T_arg4 sun_forte_workaround(T_arg1 _A_1,T_arg2 _A_2,T_arg3 _A_3,T_arg4 _A_4) const { return operator()( _A_1,_A_2,_A_3,_A_4 ); }
  T_arg4 sun_forte_workaround(T_arg1, T_arg2, T_arg3, T_arg4 _A_4) const { return _A_4; }
  #endif
  
  template <class T_arg1,class T_arg2,class T_arg3,class T_arg4,class T_arg5>
  T_arg4 operator ()(T_arg1, T_arg2, T_arg3, T_arg4 _A_4, T_arg5) const { return _A_4; }

  #ifndef SIGC_TEMPLATE_SPECIALIZATION_OPERATOR_OVERLOAD
  template <class T_arg1,class T_arg2,class T_arg3,class T_arg4,class T_arg5>
  //Does not work: T_arg4 sun_forte_workaround(T_arg1 _A_1,T_arg2 _A_2,T_arg3 _A_3,T_arg4 _A_4,T_arg5 _A_5) const { return operator()( _A_1,_A_2,_A_3,_A_4,_A_5 ); }
  T_arg4 sun_forte_workaround(T_arg1, T_arg2, T_arg3, T_arg4 _A_4, T_arg5) const { return _A_4; }
  #endif
  
  template <class T_arg1,class T_arg2,class T_arg3,class T_arg4,class T_arg5,class T_arg6>
  T_arg4 operator ()(T_arg1, T_arg2, T_arg3, T_arg4 _A_4, T_arg5, T_arg6) const { return _A_4; }

  #ifndef SIGC_TEMPLATE_SPECIALIZATION_OPERATOR_OVERLOAD
  template <class T_arg1,class T_arg2,class T_arg3,class T_arg4,class T_arg5,class T_arg6>
  //Does not work: T_arg4 sun_forte_workaround(T_arg1 _A_1,T_arg2 _A_2,T_arg3 _A_3,T_arg4 _A_4,T_arg5 _A_5,T_arg6 _A_6) const { return operator()( _A_1,_A_2,_A_3,_A_4,_A_5,_A_6 ); }
  T_arg4 sun_forte_workaround(T_arg1, T_arg2, T_arg3, T_arg4 _A_4, T_arg5, T_arg6) const { return _A_4; }
  #endif
  
  template <class T_arg1,class T_arg2,class T_arg3,class T_arg4,class T_arg5,class T_arg6,class T_arg7>
  T_arg4 operator ()(T_arg1, T_arg2, T_arg3, T_arg4 _A_4, T_arg5, T_arg6, T_arg7) const { return _A_4; }

  #ifndef SIGC_TEMPLATE_SPECIALIZATION_OPERATOR_OVERLOAD
  template <class T_arg1,class T_arg2,class T_arg3,class T_arg4,class T_arg5,class T_arg6,class T_arg7>
  //Does not work: T_arg4 sun_forte_workaround(T_arg1 _A_1,T_arg2 _A_2,T_arg3 _A_3,T_arg4 _A_4,T_arg5 _A_5,T_arg6 _A_6,T_arg7 _A_7) const { return operator()( _A_1,_A_2,_A_3,_A_4,_A_5,_A_6,_A_7 ); }
  T_arg4 sun_forte_workaround(T_arg1, T_arg2, T_arg3, T_arg4 _A_4, T_arg5, T_arg6, T_arg7) const { return _A_4; }
  #endif
  
};

struct lambda_select5 : public lambda_base
{
  template <class T_arg1=void,class T_arg2=void,class T_arg3=void,class T_arg4=void,class T_arg5=void,class T_arg6=void,class T_arg7=void>
  struct deduce_result_type
    { typedef T_arg5 type; };
  typedef void result_type; // no operator ()() overload

  void operator ()() const; // not implemented
  template <class T_arg1,class T_arg2,class T_arg3,class T_arg4,class T_arg5>
  T_arg5 operator ()(T_arg1, T_arg2, T_arg3, T_arg4, T_arg5 _A_5) const { return _A_5; }

  #ifndef SIGC_TEMPLATE_SPECIALIZATION_OPERATOR_OVERLOAD
  template <class T_arg1,class T_arg2,class T_arg3,class T_arg4,class T_arg5>
  //Does not work: T_arg5 sun_forte_workaround(T_arg1 _A_1,T_arg2 _A_2,T_arg3 _A_3,T_arg4 _A_4,T_arg5 _A_5) const { return operator()( _A_1,_A_2,_A_3,_A_4,_A_5 ); }
  T_arg5 sun_forte_workaround(T_arg1, T_arg2, T_arg3, T_arg4, T_arg5 _A_5) const { return _A_5; }
  #endif
  
  template <class T_arg1,class T_arg2,class T_arg3,class T_arg4,class T_arg5,class T_arg6>
  T_arg5 operator ()(T_arg1, T_arg2, T_arg3, T_arg4, T_arg5 _A_5, T_arg6) const { return _A_5; }

  #ifndef SIGC_TEMPLATE_SPECIALIZATION_OPERATOR_OVERLOAD
  template <class T_arg1,class T_arg2,class T_arg3,class T_arg4,class T_arg5,class T_arg6>
  //Does not work: T_arg5 sun_forte_workaround(T_arg1 _A_1,T_arg2 _A_2,T_arg3 _A_3,T_arg4 _A_4,T_arg5 _A_5,T_arg6 _A_6) const { return operator()( _A_1,_A_2,_A_3,_A_4,_A_5,_A_6 ); }
  T_arg5 sun_forte_workaround(T_arg1, T_arg2, T_arg3, T_arg4, T_arg5 _A_5, T_arg6) const { return _A_5; }
  #endif
  
  template <class T_arg1,class T_arg2,class T_arg3,class T_arg4,class T_arg5,class T_arg6,class T_arg7>
  T_arg5 operator ()(T_arg1, T_arg2, T_arg3, T_arg4, T_arg5 _A_5, T_arg6, T_arg7) const { return _A_5; }

  #ifndef SIGC_TEMPLATE_SPECIALIZATION_OPERATOR_OVERLOAD
  template <class T_arg1,class T_arg2,class T_arg3,class T_arg4,class T_arg5,class T_arg6,class T_arg7>
  //Does not work: T_arg5 sun_forte_workaround(T_arg1 _A_1,T_arg2 _A_2,T_arg3 _A_3,T_arg4 _A_4,T_arg5 _A_5,T_arg6 _A_6,T_arg7 _A_7) const { return operator()( _A_1,_A_2,_A_3,_A_4,_A_5,_A_6,_A_7 ); }
  T_arg5 sun_forte_workaround(T_arg1, T_arg2, T_arg3, T_arg4, T_arg5 _A_5, T_arg6, T_arg7) const { return _A_5; }
  #endif
  
};

struct lambda_select6 : public lambda_base
{
  template <class T_arg1=void,class T_arg2=void,class T_arg3=void,class T_arg4=void,class T_arg5=void,class T_arg6=void,class T_arg7=void>
  struct deduce_result_type
    { typedef T_arg6 type; };
  typedef void result_type; // no operator ()() overload

  void operator ()() const; // not implemented
  template <class T_arg1,class T_arg2,class T_arg3,class T_arg4,class T_arg5,class T_arg6>
  T_arg6 operator ()(T_arg1, T_arg2, T_arg3, T_arg4, T_arg5, T_arg6 _A_6) const { return _A_6; }

  #ifndef SIGC_TEMPLATE_SPECIALIZATION_OPERATOR_OVERLOAD
  template <class T_arg1,class T_arg2,class T_arg3,class T_arg4,class T_arg5,class T_arg6>
  //Does not work: T_arg6 sun_forte_workaround(T_arg1 _A_1,T_arg2 _A_2,T_arg3 _A_3,T_arg4 _A_4,T_arg5 _A_5,T_arg6 _A_6) const { return operator()( _A_1,_A_2,_A_3,_A_4,_A_5,_A_6 ); }
  T_arg6 sun_forte_workaround(T_arg1, T_arg2, T_arg3, T_arg4, T_arg5, T_arg6 _A_6) const { return _A_6; }
  #endif
  
  template <class T_arg1,class T_arg2,class T_arg3,class T_arg4,class T_arg5,class T_arg6,class T_arg7>
  T_arg6 operator ()(T_arg1, T_arg2, T_arg3, T_arg4, T_arg5, T_arg6 _A_6, T_arg7) const { return _A_6; }

  #ifndef SIGC_TEMPLATE_SPECIALIZATION_OPERATOR_OVERLOAD
  template <class T_arg1,class T_arg2,class T_arg3,class T_arg4,class T_arg5,class T_arg6,class T_arg7>
  //Does not work: T_arg6 sun_forte_workaround(T_arg1 _A_1,T_arg2 _A_2,T_arg3 _A_3,T_arg4 _A_4,T_arg5 _A_5,T_arg6 _A_6,T_arg7 _A_7) const { return operator()( _A_1,_A_2,_A_3,_A_4,_A_5,_A_6,_A_7 ); }
  T_arg6 sun_forte_workaround(T_arg1, T_arg2, T_arg3, T_arg4, T_arg5, T_arg6 _A_6, T_arg7) const { return _A_6; }
  #endif
  
};

struct lambda_select7 : public lambda_base
{
  template <class T_arg1=void,class T_arg2=void,class T_arg3=void,class T_arg4=void,class T_arg5=void,class T_arg6=void,class T_arg7=void>
  struct deduce_result_type
    { typedef T_arg7 type; };
  typedef void result_type; // no operator ()() overload

  void operator ()() const; // not implemented
  template <class T_arg1,class T_arg2,class T_arg3,class T_arg4,class T_arg5,class T_arg6,class T_arg7>
  T_arg7 operator ()(T_arg1, T_arg2, T_arg3, T_arg4, T_arg5, T_arg6, T_arg7 _A_7) const { return _A_7; }

  #ifndef SIGC_TEMPLATE_SPECIALIZATION_OPERATOR_OVERLOAD
  template <class T_arg1,class T_arg2,class T_arg3,class T_arg4,class T_arg5,class T_arg6,class T_arg7>
  //Does not work: T_arg7 sun_forte_workaround(T_arg1 _A_1,T_arg2 _A_2,T_arg3 _A_3,T_arg4 _A_4,T_arg5 _A_5,T_arg6 _A_6,T_arg7 _A_7) const { return operator()( _A_1,_A_2,_A_3,_A_4,_A_5,_A_6,_A_7 ); }
  T_arg7 sun_forte_workaround(T_arg1, T_arg2, T_arg3, T_arg4, T_arg5, T_arg6, T_arg7 _A_7) const { return _A_7; }
  #endif
  
};


} /* namespace internal */

extern SIGC_API const lambda<internal::lambda_select1> _1;
extern SIGC_API const lambda<internal::lambda_select2> _2;
extern SIGC_API const lambda<internal::lambda_select3> _3;
extern SIGC_API const lambda<internal::lambda_select4> _4;
extern SIGC_API const lambda<internal::lambda_select5> _5;
extern SIGC_API const lambda<internal::lambda_select6> _6;
extern SIGC_API const lambda<internal::lambda_select7> _7;


} /* namespace sigc */

#endif /* _SIGC_LAMBDA_SELECT_HPP_ */
