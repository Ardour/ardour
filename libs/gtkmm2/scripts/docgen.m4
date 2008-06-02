
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


