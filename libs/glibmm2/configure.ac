# Configure.in
#
#  This file tests for various compiler features needed to configure 
#  the gtkmm package.  Original skeleton was provided by Stephan Kulow.
#  All tests were written by Tero Pulkkinen, Mirko Streckenbach, and 
#  Karl Nelson.
#
#  NOTE! IF YOU DO CHANGES HERE, CHECK IF YOU NEED TO MODIFY .m4 TOO!!!
#
#  Copyright 2001 Free Software Foundation
#  Copyright 1999 gtkmm Development Team
#  Copyright 1998 Stephan Kulow 
#
AC_INIT(glibmmconfig.h.in)
AC_PREREQ(2.50)

#########################################################################
#  Version and initialization
#########################################################################
GLIBMM_MAJOR_VERSION=2
GLIBMM_MINOR_VERSION=6
GLIBMM_MICRO_VERSION=1
GLIBMM_VERSION=$GLIBMM_MAJOR_VERSION.$GLIBMM_MINOR_VERSION.$GLIBMM_MICRO_VERSION
GLIBMM_RELEASE=$GLIBMM_MAJOR_VERSION.$GLIBMM_MINOR_VERSION
AC_DEFINE_UNQUOTED(GLIBMM_MAJOR_VERSION, $GLIBMM_MAJOR_VERSION)
AC_DEFINE_UNQUOTED(GLIBMM_MINOR_VERSION, $GLIBMM_MINOR_VERSION)
AC_DEFINE_UNQUOTED(GLIBMM_MICRO_VERSION, $GLIBMM_MICRO_VERSION)
AC_SUBST(GLIBMM_MAJOR_VERSION)
AC_SUBST(GLIBMM_MINOR_VERSION)
AC_SUBST(GLIBMM_MICRO_VERSION)
AC_SUBST(GLIBMM_VERSION)
AC_SUBST(GLIBMM_RELEASE)

#
# +1 : ? : +1  == new interface that does not break old one
# +1 : ? : 0   == new interface that breaks old one
#  ? : ? : 0   == no new interfaces, but breaks apps
#  ? :+1 : ?   == just some internal changes, nothing breaks but might work 
#                 better
# CURRENT : REVISION : AGE
LIBGLIBMM_SO_VERSION=1:13:0
AC_SUBST(LIBGLIBMM_SO_VERSION)

AC_CONFIG_AUX_DIR(scripts)

dnl For automake.
VERSION=$GLIBMM_VERSION
PACKAGE=glibmm

dnl Initialize automake stuff
AM_INIT_AUTOMAKE($PACKAGE, $VERSION)

dnl Specify a configuration file (no autoheader)
AM_CONFIG_HEADER(glibmmconfig.h)
AM_MAINTAINER_MODE
AL_ACLOCAL_INCLUDE(scripts)


#########################################################################
#  Configure arguments 
#########################################################################

#########################################################################
#  Environment Checks
#########################################################################
AC_PROG_CC
AC_PROG_CPP
AC_PROG_MAKE_SET
AC_CANONICAL_BUILD
AC_CANONICAL_HOST

AC_DISABLE_STATIC
AC_PROG_LIBTOOL

AL_PROG_GNU_M4(AC_MSG_ERROR([dnl
SUN m4 does not work for building gtkmm. 
Please install GNU m4.]))

AL_PROG_GNU_MAKE(AC_MSG_ERROR([dnl
SUN make does not work for building gtkmm.
Please install GNU make.]))

#########################################################################
#  Dependancy checks
#########################################################################
gtkmm_min_glib_version=2.6.0

PKG_CHECK_MODULES(GLIBMM, glib-2.0 >= ${gtkmm_min_glib_version} gobject-2.0 >= ${gtkmm_min_glib_version} gmodule-2.0 >= ${gtkmm_min_glib_version})
AC_SUBST(GLIBMM_CFLAGS)
AC_SUBST(GLIBMM_LIBS)

# gthread isn't a requirement, but we should use its CFLAGS if available.
PKG_CHECK_MODULES(GTHREAD, gthread-2.0 >= ${gtkmm_min_glib_version},[],[GTHREAD_CFLAGS=''; GTHREAD_LIBS=''])
AC_SUBST(GTHREAD_CFLAGS)
AC_SUBST(GTHREAD_LIBS)


#########################################################################
#  C++ checks
#########################################################################
AC_PROG_CXX

AC_LANG_CPLUSPLUS

AC_CXX_BOOL(,config_error=yes)
AC_CXX_NAMESPACES(,config_error=yes)
AC_CXX_MUTABLE(,config_error=yes)

AC_MSG_CHECKING(if C++ environment provides all required features)
if test "x$config_error" = xyes ; then
  AC_MSG_RESULT([no])
  AC_MSG_ERROR([Your compiler is not powerful enough to compile gtkmm. If it should be, see config.log for more information of why it failed.])
fi
AC_MSG_RESULT([yes])

GLIBMM_CXX_HAS_NAMESPACE_STD()
GLIBMM_CXX_HAS_STD_ITERATOR_TRAITS()
GLIBMM_CXX_HAS_SUN_REVERSE_ITERATOR()
GLIBMM_CXX_HAS_TEMPLATE_SEQUENCE_CTORS()
GLIBMM_CXX_MEMBER_FUNCTIONS_MEMBER_TEMPLATES()
GLIBMM_CXX_CAN_DISAMBIGUATE_CONST_TEMPLATE_SPECIALIZATIONS()
GLIBMM_CXX_CAN_USE_DYNAMIC_CAST_IN_UNUSED_TEMPLATE_WITHOUT_DEFINITION()
GLIBMM_CXX_CAN_ASSIGN_NON_EXTERN_C_FUNCTIONS_TO_EXTERN_C_CALLBACKS()
GLIBMM_CXX_CAN_USE_NAMESPACES_INSIDE_EXTERNC()
GLIBMM_CXX_ALLOWS_STATIC_INLINE_NPOS()

# Check whether --enable-debug-refcounting was given.
GLIBMM_ARG_ENABLE_DEBUG_REFCOUNTING()

# Evaluate the --enable-warnings=level option.
GTKMM_ARG_ENABLE_WARNINGS()

# Dummy conditional just to make automake-1.4 happy.
# We need an always-false condition in docs/Makefile.am.
AM_CONDITIONAL(GTKMM_FALSE,[false])

# HACK:  Assign a dummy in order to prevent execution of autoheader by the
# maintainer-mode rules.  That would fail since we aren't using autoheader.
AUTOHEADER=':'

AC_CONFIG_FILES([
  Makefile

  glibmm-2.4.pc
  glibmm/Makefile
])

AC_OUTPUT()

