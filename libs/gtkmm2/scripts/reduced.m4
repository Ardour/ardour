## GLIBMM_ARG_ENABLE_API_ATKMM()
##
## Provide the --enable-api-atkmm configure argument, enabled
## by default.
##
AC_DEFUN([GTKMM_ARG_ENABLE_API_ATKMM],
[
  AC_ARG_ENABLE([api-atkmm],
      [  --enable-api-atkmm  Build atkmm API.
                              [[default=yes]]],
      [gtkmm_enable_api_atkmm="$enableval"],
      [gtkmm_enable_api_atkmm='yes'])

  if test "x$gtkmm_enable_api_atkmm" = "xyes"; then
  {
    AC_DEFINE([GTKMM_ATKMM_ENABLED],[1], [Defined when the --enable-api-atkmm configure argument was given])
  }
  fi
])


## GLIBMM_ARG_ENABLE_API_MAEMO_EXTENSIONS()
##
## Provide the --enable-api-maemo-extensions configure argument, disabled
## by default. This provides gtkmm API for the additional GTK+ methods in
## the version of GTK+ that is in the Maemo platform.
##
AC_DEFUN([GTKMM_ARG_ENABLE_API_MAEMO_EXTENSIONS],
[
  AC_ARG_ENABLE([api-maemo-extensions],
      [  --enable-api-maemo-extensions  Build Maemo Extensions API.
                              [[default=no]]],
      [gtkmm_enable_api_maemo_extensions="$enableval"],
      [gtkmm_enable_api_maemo_extensions='no'])

  if test "x$gtkmm_enable_api_maemo_extensions" = "xyes"; then
  {
    AC_DEFINE([GTKMM_MAEMO_EXTENSIONS_ENABLED],[1], [Defined when the --enable-maemo-extensions configure argument was given])
  }
  fi
])

