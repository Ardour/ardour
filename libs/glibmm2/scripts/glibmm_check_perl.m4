dnl
dnl Some macros needed for autoconf
dnl


## GLIBMM_CV_PERL_VERSION(version)
##
## Helper macro of GLIBMM_CHECK_PERL().  It generates a cache variable
## name that includes the version number, in order to avoid clashes.
##
AC_DEFUN([GLIBMM_CV_PERL_VERSION],[glibmm_cv_perl_version_[]m4_translit([$1],[.${}],[____])])


## GLIBMM_CHECK_PERL(version)
##
## Check for Perl >= version and set PERL_PATH.  If Perl is not found
## and maintainer-mode is enabled, abort with an error message.  If not
## in maintainer-mode, set PERL_PATH=perl on failure.
##
AC_DEFUN([GLIBMM_CHECK_PERL],
[
  glibmm_perl_result=no

  AC_PATH_PROGS([PERL_PATH], [perl perl5], [not found])

  if test "x$PERL_PATH" != "xnot found"; then
  {
    AC_CACHE_CHECK(
      [whether Perl is new enough],
      GLIBMM_CV_PERL_VERSION([$1]),
    [
      ]GLIBMM_CV_PERL_VERSION([$1])[=no
      "$PERL_PATH" -e "require v$1; exit 0;" >/dev/null 2>&1 && ]GLIBMM_CV_PERL_VERSION([$1])[=yes
    ])
    test "x${GLIBMM_CV_PERL_VERSION([$1])}" = xyes && glibmm_perl_result=yes
  }
  else
  {
    # Make sure we have something sensible, even if it doesn't work.
    PERL_PATH=perl
  }
  fi

  if test "x$glibmm_perl_result" = xno && test "x$USE_MAINTAINER_MODE" = xyes; then
  {
    AC_MSG_ERROR([[
*** Perl >= ]$1[ is required for building $PACKAGE in maintainer-mode.
]])
  }
  fi

  AC_SUBST([PERL_PATH])
])

