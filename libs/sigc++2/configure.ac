dnl Configure.in
dnl
dnl  Source for generating compiler independent libraries.
dnl

#We use pushdef here because we can't use shell variables before AC_INIT, but we want to use a variable with AC_INIT:
dnl   thus make format_package-0.0.1.tar.gz 
pushdef([FP_MAJOR_VERSION],     [2])
pushdef([FP_MINOR_VERSION],     [2])
pushdef([FP_MICRO_VERSION],     [2])
pushdef([FP_EXTRA_VERSION],     [])
pushdef([FP_VERSION], FP_MAJOR_VERSION.FP_MINOR_VERSION.FP_MICRO_VERSION[]FP_EXTRA_VERSION)
# package name, version, support contact, tarball name.
AC_INIT([libsigc++], FP_VERSION, [libsigc-list@gnome.org], [libsigc++])

dnl AC_CONFIG_SRCDIR is required name a file which is unique to the package
dnl just to prevent someone from copying the configure to the wrong package.
AC_CONFIG_SRCDIR([sigc++])

AC_PREREQ(2.59)

#########################################################################
#  Version and initialization
#########################################################################
[FP_MAJOR_VERSION]=FP_MAJOR_VERSION
[FP_MINOR_VERSION]=FP_MINOR_VERSION
[FP_MICRO_VERSION]=FP_MICRO_VERSION
[FP_EXTRA_VERSION]=FP_EXTRA_VERSION
[FP_VERSION]=FP_VERSION
popdef([FP_MAJOR_VERSION])
popdef([FP_MINOR_VERSION])
popdef([FP_MICRO_VERSION])
popdef([FP_EXTRA_VERSION])
popdef([FP_VERSION])
FP_RELEASE=$FP_MAJOR_VERSION.$FP_MINOR_VERSION
AC_DEFINE_UNQUOTED(FP_MAJOR_VERSION, $FP_MAJOR_VERSION, [Major version of libsigc++])
AC_DEFINE_UNQUOTED(FP_MINOR_VERSION, $FP_MINOR_VERSION, [Minor version of libsigc++])
AC_DEFINE_UNQUOTED(FP_MICRO_VERSION, $FP_MICRO_VERSION, [Micro version of libsigc++])
AC_SUBST(FP_VERSION)
AC_SUBST(FP_RELEASE)
AC_SUBST(FP_MAJOR_VERSION)
AC_SUBST(FP_MINOR_VERSION)
AC_SUBST(FP_MICRO_VERSION)

dnl For automake.
VERSION=$FP_VERSION
PACKAGE=libsigc++

# Initialize automake stuff
# tar-ustar asks it to use a sensible tar format that can handle long filenames.
AM_INIT_AUTOMAKE([1.9 tar-ustar])

dnl Specify a configuration file:
AC_CONFIG_HEADER(sigc++config.h)

dnl Macros to support windows platforms
AC_CYGWIN
#AC_MINGW32
AC_EXEEXT

dnl this package needs m4
AC_CHECK_PROGS(M4, gm4 m4, m4)

dnl perl is needed for building the reference documentation
AC_PATH_PROGS([PERL_PATH], [perl perl5], [perl])
AC_SUBST([PERL_PATH])

dnl disable autoheader
AUTOHEADER=':'

dnl Maintainer support (autodependencies and packaging)
AM_MAINTAINER_MODE

dnl Using C compiler
AC_PROG_CC
AC_PROG_CPP

dnl Used for enabling the "-no-undefined" flag while generating DLLs
dnl Borrowed from the official gtk+-2 configure.in
AC_MSG_CHECKING([for some Win32 platform])
case "$host" in
  *-*-mingw*|*-*-cygwin*)
    platform_win32=yes
    ;;
  *)
    platform_win32=no
    ;;
esac
AC_MSG_RESULT([$platform_win32])
AM_CONDITIONAL(PLATFORM_WIN32, test "$platform_win32" = "yes")

dnl Using libtool
AC_CONFIG_MACRO_DIR(scripts)
AC_LIBTOOL_WIN32_DLL
dnl: Use with libtool 1.5a instead of AM_PROG_LIBTOOL: LT_INIT
AM_PROG_LIBTOOL

dnl Using C++ compiler
AC_PROG_CXX
AC_LANG_CPLUSPLUS

SIGC_CXX_GCC_TEMPLATE_SPECIALIZATION_OPERATOR_OVERLOAD()
SIGC_CXX_MSVC_TEMPLATE_SPECIALIZATION_OPERATOR_OVERLOAD()
SIGC_CXX_SELF_REFERENCE_IN_MEMBER_INITIALIZATION()
SIGC_CXX_HAS_NAMESPACE_STD()
SIGC_CXX_HAS_SUN_REVERSE_ITERATOR()

if test "X$config_error" = "Xyes" ; then
  AC_ERROR(
[One or more of the required compiler features is missing.
If you believe this is in error, please consult the config.log file
for further details.
])
fi
 

# Generate output
AC_OUTPUT([
  Makefile
  sigc++-2.0.pc

  sigc++/Makefile
  scripts/Makefile
  libsigc++-2.0.spec

])
