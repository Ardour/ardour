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

dnl
dnl Macros to make select arguments
define([LAMBDA_SELECT_DO],[dnl
  template <LOOP(class T_arg%1, $2)>
dnl T_arg$1 operator ()(LOOP(T_arg%1 _A_%1, $2)) const { return _A_$1; }
  T_arg$1 operator ()(LIST(FOR(1,eval($1-1),[T_arg%1,]),T_arg$1 _A_$1,FOR(eval($1+1),$2,[T_arg%1,]))) const { return _A_$1; }

  #ifndef SIGC_TEMPLATE_SPECIALIZATION_OPERATOR_OVERLOAD
  template <LOOP(class T_arg%1, $2)>
  //Does not work: T_arg$1 sun_forte_workaround(LOOP(T_arg%1 _A_%1, $2)) const { return operator()( LOOP(_A_%1, $2) ); }
  T_arg$1 sun_forte_workaround(LIST(FOR(1,eval($1-1),[T_arg%1,]),T_arg$1 _A_$1,FOR(eval($1+1),$2,[T_arg%1,]))) const { return _A_$1; }
  #endif
  
])
define([LAMBDA_SELECT],[dnl
struct lambda_select$1 : public lambda_base
{
  template <LOOP(class T_arg%1=void,$2)>
  struct deduce_result_type
    { typedef T_arg$1 type; };
  typedef void result_type; // no operator ()() overload

  void operator ()() const; // not implemented
FOR($1, $2,[[LAMBDA_SELECT_DO($1,%1)]])dnl
};

])

divert(0)dnl
#ifndef _SIGC_LAMBDA_SELECT_HPP_
#define _SIGC_LAMBDA_SELECT_HPP_
#include <sigc++/adaptors/lambda/base.h>

namespace sigc {

namespace internal {
FOR(1,CALL_SIZE,[[LAMBDA_SELECT(%1,CALL_SIZE)]])
} /* namespace internal */

FOR(1,CALL_SIZE,[[extern SIGC_API const lambda<internal::lambda_select%1> _%1;
]])

} /* namespace sigc */

#endif /* _SIGC_LAMBDA_SELECT_HPP_ */
