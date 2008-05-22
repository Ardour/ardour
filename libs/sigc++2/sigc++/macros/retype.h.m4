dnl Copyright 2003, The libsigc++ Development Team 
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

define([RETYPE_SLOT_OPERATOR],[dnl
ifelse($1,0,[dnl
  T_return operator()();
],[dnl
  template <LOOP(class T_arg%1, $1)>
  inline T_return operator()(LOOP(T_arg%1 _A_a%1, $1))
    { return T_return(this->functor_.SIGC_WORKAROUND_OPERATOR_PARENTHESES<LOOP(typename ::sigc::type_trait<T_type%1>::take, $1)>
        (LOOP([[static_cast<T_type%1>(_A_a%1)]], $1)));
    }

  #ifndef SIGC_TEMPLATE_SPECIALIZATION_OPERATOR_OVERLOAD
  template <LOOP(class T_arg%1, $1)>
  inline T_return sun_forte_workaround(LOOP(T_arg%1 _A_a%1, $1))
    { return T_return(this->functor_.SIGC_WORKAROUND_OPERATOR_PARENTHESES<LOOP(typename ::sigc::type_trait<T_type%1>::take, $1)>
        (LOOP([[static_cast<T_type%1>(_A_a%1)]], $1)));
    }
  #endif
  
])dnl
])
define([RETYPE_SLOT_VOID_OPERATOR],[dnl
ifelse($1,0,[dnl
  void operator()();
],[dnl
  template <LOOP(class T_arg%1, $1)>
  inline void operator()(LOOP(T_arg%1 _A_a%1, $1))
    { T_return(this->functor_.SIGC_WORKAROUND_OPERATOR_PARENTHESES<LOOP(typename ::sigc::type_trait<T_type%1>::take, $1)>
        (LOOP([[static_cast<T_type%1>(_A_a%1)]], $1)));
    }

  #ifndef SIGC_TEMPLATE_SPECIALIZATION_OPERATOR_OVERLOAD
  template <LOOP(class T_arg%1, $1)>
  inline void sun_forte_workaround(LOOP(T_arg%1 _A_a%1, $1))
    { T_return(this->functor_.SIGC_WORKAROUND_OPERATOR_PARENTHESES<LOOP(typename ::sigc::type_trait<T_type%1>::take, $1)>
        (LOOP([[static_cast<T_type%1>(_A_a%1)]], $1)));
    }
  #endif
    
])dnl
])
define([RETYPE],[dnl
template <LIST(class T_return, LOOP(class T_arg%1, $1), class T_ret, LOOP(class T_type%1, $1))>
inline Slot$1<LIST(T_return, LOOP(T_arg%1, $1))>
retype(const Slot$1<LIST(T_ret, LOOP(T_type%1, $1))>& _A_slot)
{ return Slot$1<LIST(T_return, LOOP(T_arg%1, $1))>
    (retype_slot_functor<LIST(Slot$1<LIST(T_ret, LOOP(T_type%1, $1))>, T_return, LOOP(T_type%1, $1))>
      (_A_slot)); }

])

divert(0)dnl
__FIREWALL__
#include <sigc++/adaptors/adaptor_trait.h>
#include <sigc++/slot.h>

