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
define(__t_div__,divnum)divert(-1)
dnl
dnl  M4 macros for building large files quickly
dnl
divert(0)// -*- c++ -*-
/* Do not edit! -- generated file */
divert(-1)
changequote([, ])
changecom()

dnl
dnl Macros for sigc specifically.
dnl

define([CALL_SIZE],7)

#Generate header guards:
define([__FIREWALL__],[dnl
define(__hfile_temp__,[translit(__file__,/., _)])dnl
define(__hfile_temp2__,[translit(__hfile_temp__,+., _)])dnl
define(__hfile__,[_SIGC_[]patsubst(translit(__hfile_temp2__,a-z.,A-Z_), _M4$)_])dnl
#ifndef __hfile__
#define __hfile__[]dnl
divert(1)dnl
#endif /* __hfile__ */
divert(0)dnl
])

define([_R_],[typename type_trait<$1>::take])
define([_P_],[typename type_trait<$1>::pass])


dnl
dnl General macros
dnl

define([UPPER],[translit([$*],[abcdefghijklmnopqrstuvwxyz],[ABCDEFGHIJKLMNOPQRSTUVWXYZ])])
define([LOWER],[translit([$*],[ABCDEFGHIJKLMNOPQRSTUVWXYZ],[abcdefghijklmnopqrstuvwxyz])])
define([PROT],[[$*]])

define([_LOOP],
[ifelse(eval($1<$2),0,
[indir([_LOOP_FORMAT], $1)],
[indir([_LOOP_FORMAT], $1)[]_LOOP_SEP[]_LOOP(eval($1+1), $2)])])

define([LOOP],
[pushdef([_LOOP_FORMAT], translit([$1],%, $))dnl
pushdef([_LOOP_SEP],ifelse([$3],[],[[, ]],[$3]))dnl
ifelse(eval($2>0),1,[PROT(_LOOP(1, $2))],[PROT()])dnl
popdef([_LOOP_SEP])dnl
popdef([_LOOP_FORMAT])dnl
])

define([NUM],[eval(ifelse([$1],,0,1)ifelse($#,0,0, $#,1,,[+NUM(shift($@))]))])
define([LIST],[ifelse($#,0,, $#,1,[$1],[$1],,[LIST(shift($@))],[__LIST($@)])])
define([__LIST],[ifelse($#,0,, $#,1,[$1],[$1[]ifelse([$2],,,[[, ]])__LIST(shift($@))])])
dnl
define([_NL_],[
])

define([FOR],
[pushdef([_FOR_FUNC],PROT(translit([$3],%, $)))dnl
_FOR($1, $2)[]dnl
popdef([_FOR_FUNC])dnl
])
define([_FOR],[ifelse(eval($1>$2),1,[],[_FOR_FUNC($1)[]_FOR(eval($1+1), $2)])])

divert(__t_div__)dnl
