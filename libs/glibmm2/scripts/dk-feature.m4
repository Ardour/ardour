## Copyright (c) 2004-2007  Daniel Elstner  <daniel.kitta@gmail.com>
##
## This file is part of danielk's Autostuff.
##
## danielk's Autostuff is free software; you can redistribute it and/or
## modify it under the terms of the GNU General Public License as published
## by the Free Software Foundation; either version 2 of the License, or (at
## your option) any later version.
##
## danielk's Autostuff is distributed in the hope that it will be useful, but
## WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
## or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
## for more details.
##
## You should have received a copy of the GNU General Public License along
## with danielk's Autostuff; if not, write to the Free Software Foundation,
## Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA

#serial 20070105

## _DK_SH_VAR_PUSH_DEPTH(depth, variable, [value])
##
m4_define([_DK_SH_VAR_PUSH_DEPTH],
[dnl
m4_pushdef([_DK_SH_VAR_DEPTH_$2], [$1])[]dnl
dk_save_sh_var_$2_$1=$$2
m4_if([$3], [],, [$2=$3
])[]dnl
])

## _DK_SH_VAR_POP_DEPTH(depth, variable)
##
m4_define([_DK_SH_VAR_POP_DEPTH],
[dnl
$2=$dk_save_sh_var_$2_$1
m4_popdef([_DK_SH_VAR_DEPTH_$2])[]dnl
])

## DK_SH_VAR_PUSH(variable, [value])
##
## Temporarily replace the current value of the shell variable <variable>
## with <value> until DK_SH_VAR_POP(<variable>) is invoked to restore the
## original value.  If <value> is empty, <variable> is left unchanged but
## its current value is still saved.
##
## This macro may safely be used repeatedly on the same shell variable,
## as long as each DK_SH_VAR_PUSH(variable) is matched by a corresponding
## DK_SH_VAR_POP(variable).
##
AC_DEFUN([DK_SH_VAR_PUSH],
[dnl
m4_if([$1],, [AC_FATAL([argument expected])])[]dnl
_DK_SH_VAR_PUSH_DEPTH(m4_ifdef([_DK_SH_VAR_DEPTH_$1],
                               [m4_incr(_DK_SH_VAR_DEPTH_$1)],
                               [1]),
                      [$1], [$2])[]dnl
])

## DK_SH_VAR_POP(variable)
##
## Restore the original value of the shell variable <variable> which it had
## before the corresponding invocation of DK_SH_VAR_PUSH(<variable>).
##
AC_DEFUN([DK_SH_VAR_POP],
[dnl
m4_if([$1],, [AC_FATAL([argument expected])])[]dnl
_DK_SH_VAR_POP_DEPTH(_DK_SH_VAR_DEPTH_$1, [$1])[]dnl
])

## _DK_CHECK_FEATURE_VAR(feature, source, cache var, shell var, cpp define)
##
m4_define([_DK_CHECK_FEATURE_VAR],
[dnl
AC_CACHE_CHECK([for $1], [$3],
               [AC_LINK_IFELSE([$2], [$3=yes], [$3=no])])
$4=$$3

AS_IF([test "x$$4" = xyes],
      [AC_DEFINE([$5], [1], [Define to 1 if $1 is available.])
])[]dnl
])

## DK_CHECK_FEATURE(feature, test source)
##
## Check for a feature of the C/C++ environment.  If compiling and linking
## the supplied test program is successful, the configuration header macro
## <PACKAGE_TARNAME>_HAVE_<FEATURE> is defined to 1 and "yes" is assigned
## to the shell variable <PACKAGE_TARNAME>_FEATURE_<FEATURE>.  Otherwise,
## <PACKAGE_TARNAME>_FEATURE_<FEATURE> is set to "no".
##
## This macro is intended to be used in conjunction with AC_LANG_PROGRAM
## or AC_LANG_SOURCE.
##
AC_DEFUN([DK_CHECK_FEATURE],
[dnl
m4_if([$2],, [AC_FATAL([2 arguments expected])])[]dnl
_DK_CHECK_FEATURE_VAR([$1], [$2],
                      m4_quote(AS_TR_SH([dk_cv_feature_$1])),
                      m4_quote(AS_TR_CPP(AC_PACKAGE_TARNAME[_FEATURE_$1])),
                      m4_quote(AS_TR_CPP(AC_PACKAGE_TARNAME[_HAVE_$1])))[]dnl
])
