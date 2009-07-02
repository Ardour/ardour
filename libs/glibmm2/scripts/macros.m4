dnl
dnl Some macros needed for autoconf
dnl

dnl AL_PROG_GNU_M4(ACTION_NOT_FOUND)
dnl  Check for GNU m4.  (sun won't do.)
dnl
AC_DEFUN([AL_PROG_GNU_M4],[
AC_CHECK_PROGS(M4, gm4 m4, m4)

if test "$M4" = "m4"; then
  AC_MSG_CHECKING(whether m4 is GNU m4)
  if $M4 --version </dev/null 2>/dev/null | grep -i '^GNU M4 ' >/dev/null ; then
    AC_MSG_RESULT(yes)
  else
    AC_MSG_RESULT(no)
    if test "$host_vendor" = "sun"; then
      $1
    fi
  fi
fi
])


dnl AL_PROG_GNU_MAKE(ACTION_NOT_FOUND)
dnl   Check for GNU make (no sun make)
dnl
AC_DEFUN([AL_PROG_GNU_MAKE],[
dnl 
dnl Check for GNU make (stolen from gtk+/configure.in)
AC_MSG_CHECKING(whether make is GNU Make)
if ${MAKE-make} --version 2>/dev/null | grep '^GNU Make ' >/dev/null ; then
        AC_MSG_RESULT(yes)
else
        AC_MSG_RESULT(no)
        if test "$host_vendor" = "sun" ; then
           $1
        fi
fi
])

dnl AL_ACLOCAL_INCLUDE(macrodir)
dnl   Add a directory to macro search (from gnome)
AC_DEFUN([AL_ACLOCAL_INCLUDE],
[
  test "x$ACLOCAL_FLAGS" = "x" || ACLOCAL="$ACLOCAL $ACLOCAL_FLAGS"
  for dir in $1
  do
    ACLOCAL="$ACLOCAL -I $srcdir/$dir"
  done
])


## GLIBMM_ARG_ENABLE_DEBUG_REFCOUNTING()
##
## Provide the --enable-debug-refcounting configure argument, disabled
## by default.  If enabled, #define GTKMM_DEBUG_REFCOUNTING.
##
AC_DEFUN([GLIBMM_ARG_ENABLE_DEBUG_REFCOUNTING],
[
  AC_ARG_ENABLE([debug-refcounting],
      [  --enable-debug-refcounting  Print a debug message on every ref/unref.
                              [[default=disabled]]],
      [glibmm_debug_refcounting="$enableval"],
      [glibmm_debug_refcounting='no'])

  if test "x$glibmm_debug_refcounting" = "xyes"; then
  {
    AC_DEFINE([GLIBMM_DEBUG_REFCOUNTING],[1], [Defined when the --enable-debug-refcounting configure argument was given])
  }
  fi
])


## GTKMM_ARG_ENABLE_WARNINGS()
##
## Provide the --enable-warnings configure argument, set to 'minimum'
## by default.
##
AC_DEFUN([GTKMM_ARG_ENABLE_WARNINGS],
[
  AC_ARG_ENABLE([warnings],
      [  --enable-warnings=[[none|minimum|maximum|hardcore]]
                          Control compiler pickyness.  [[default=minimum]]],
      [gtkmm_enable_warnings="$enableval"],
      [gtkmm_enable_warnings='minimum'])

  AC_MSG_CHECKING([for compiler warning flags to use])

  gtkmm_warning_flags=''

  case "$gtkmm_enable_warnings" in
    minimum|yes) gtkmm_warning_flags='-Wall -Wno-long-long';;
    maximum)     gtkmm_warning_flags='-pedantic -W -Wall -Wno-long-long';;
    hardcore)    gtkmm_warning_flags='-pedantic -W -Wall -Wno-long-long -Werror';;
  esac

  gtkmm_use_flags=''

  if test "x$gtkmm_warning_flags" != "x"
  then
    echo 'int foo() { return 0; }' > conftest.cc

    for flag in $gtkmm_warning_flags
    do
      # Test whether the compiler accepts the flag.  GCC doesn't bail
      # out when given an unsupported flag but prints a warning, so
      # check the compiler output instead.
      gtkmm_cxx_out="`$CXX $flag -c conftest.cc 2>&1`"
      rm -f conftest.$OBJEXT
      test "x${gtkmm_cxx_out}" = "x" && \
        gtkmm_use_flags="${gtkmm_use_flags:+$gtkmm_use_flags }$flag"
    done

    rm -f conftest.cc
    gtkmm_cxx_out=''
  fi

  if test "x$gtkmm_use_flags" != "x"
  then
    for flag in $gtkmm_use_flags
    do
      case " $CXXFLAGS " in
        *" $flag "*) ;; # don't add flags twice
        *)           CXXFLAGS="${CXXFLAGS:+$CXXFLAGS }$flag";;
      esac
    done
  else
    gtkmm_use_flags='none'
  fi

  AC_MSG_RESULT([$gtkmm_use_flags])
])

