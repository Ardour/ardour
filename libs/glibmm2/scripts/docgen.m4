
## GTKMM_DOXYGEN_INPUT_SUBDIRS(subdirectory list)
##
AC_DEFUN([GTKMM_DOXYGEN_INPUT_SUBDIRS],
[
GTKMM_DOXYGEN_INPUT=
gtkmm_srcdir=`cd "$srcdir" >/dev/null && pwd`

gtkmm_list="$@"
for gtkmm_sublib in $gtkmm_list
do
  GTKMM_DOXYGEN_INPUT="$GTKMM_DOXYGEN_INPUT ${gtkmm_srcdir}/${gtkmm_sublib}/${gtkmm_sublib}mm/"
done

AC_SUBST(GTKMM_DOXYGEN_INPUT)
])


## GTKMM_ARG_ENABLE_FULLDOCS()
##
## Check whether to build the full docs into the generated source.  If yes,
## set GTKMMPROC_MERGECDOCS='--mergecdocs', which will be passed to gtkmmproc
## (in build_shared/Makefile_gensrc.am_fragment).  This will be much slower.
##
AC_DEFUN([GTKMM_ARG_ENABLE_FULLDOCS],
[
AC_REQUIRE([GLIBMM_CHECK_PERL])

AC_MSG_CHECKING([[whether to merge C reference docs into generated headers]])

AC_ARG_ENABLE([fulldocs],
    [  --enable-fulldocs       Generate fully-documented reference docs, takes
                          longer to build.  [[default=enabled for CVS builds]]],
    [gtkmm_enable_fulldocs=$enableval],
    [gtkmm_enable_fulldocs=$USE_MAINTAINER_MODE])

AC_MSG_RESULT([${gtkmm_enable_fulldocs}])

GTKMMPROC_MERGECDOCS=

if test "x$gtkmm_enable_fulldocs" = xyes; then
{
  GTKMMPROC_MERGECDOCS='--mergecdocs'

  if test "x$USE_MAINTAINER_MODE" != xyes; then
  {
    AC_MSG_WARN([[
*** --enable-fulldocs only works if --enable-maintainer-mode is also set.
*** gtkmm source tarballs should be packaged with --enable-fulldocs, so
*** usually you don't need this option unless you got gtkmm from CVS.
]])
  }
  fi

  AC_CACHE_CHECK(
    [whether the XML::Parser module is available],
    [gtkmm_cv_have_xml_parser],
  [
    gtkmm_cv_have_xml_parser=no
    "$PERL_PATH" -e 'use strict; use XML::Parser; exit 0;' >&5 2>&5 && gtkmm_cv_have_xml_parser=yes
  ])

  if test "x$gtkmm_cv_have_xml_parser" = xno; then
  {
    AC_MSG_ERROR([[
*** The Perl module XML::Parser is required to build $PACKAGE from CVS.
]])
  }
  fi
}
fi

AC_SUBST(GTKMMPROC_MERGECDOCS)
])

