## GLIBMM_ARG_ENABLE_API_PROPERTIES()
##
## Provide the --enable-api-properties configure argument, enabled
## by default.
##
AC_DEFUN([GLIBMM_ARG_ENABLE_API_PROPERTIES],
[
  AC_ARG_ENABLE([api-properties],
      [  --enable-api-properties  Build properties API.
                              [[default=yes]]],
      [glibmm_enable_api_properties="$enableval"],
      [glibmm_enable_api_properties='yes'])

  if test "x$glibmm_enable_api_properties" = "xyes"; then
  {
    AC_DEFINE([GLIBMM_PROPERTIES_ENABLED],[1], [Defined when the --enable-api-properties configure argument was given])
  }
  fi
])

## GLIBMM_ARG_ENABLE_API_VFUNCS()
##
## Provide the --enable-api-vfuncs configure argument, enabled
## by default.
##
AC_DEFUN([GLIBMM_ARG_ENABLE_API_VFUNCS],
[
  AC_ARG_ENABLE([api-vfuncs],
      [  --enable-api-vfuncs  Build vfuncs API.
                              [[default=yes]]],
      [glibmm_enable_api_vfuncs="$enableval"],
      [glibmm_enable_api_vfuncs='yes'])

  if test "x$glibmm_enable_api_vfuncs" = "xyes"; then
  {
    AC_DEFINE([GLIBMM_VFUNCS_ENABLED],[1], [Defined when the --enable-api-vfuncs configure argument was given])
  }
  fi
])

## GLIBMM_ARG_ENABLE_API_EXCEPTIONS()
##
## Provide the --enable-api-exceptions configure argument, enabled
## by default.
##
AC_DEFUN([GLIBMM_ARG_ENABLE_API_EXCEPTIONS],
[
  AC_ARG_ENABLE([api-exceptions],
      [  --enable-api-exceptions  Build exceptions API.
                              [[default=yes]]],
      [glibmm_enable_api_exceptions="$enableval"],
      [glibmm_enable_api_exceptions='yes'])

  if test "x$glibmm_enable_api_exceptions" = "xyes"; then
  {
    AC_DEFINE([GLIBMM_EXCEPTIONS_ENABLED],[1], [Defined when the --enable-api-exceptions configure argument was given])
  }
  fi
])

## GLIBMM_ARG_ENABLE_API_DEPRECATED()
##
## Provide the --enable-deprecated-api configure argument, enabled
## by default.
##
AC_DEFUN([GLIBMM_ARG_ENABLE_API_DEPRECATED],
[
  AC_ARG_ENABLE(deprecated-api, 
      [  --enable-deprecated-api  Include (build) deprecated API in the libraries.
                              [[default=yes]]],
      [glibmm_enable_api_deprecated="$enableval"],
      [glibmm_enable_api_deprecated='yes'])

  if test "x$glibmm_enable_api_deprecated" = "xyes"; then
  {
    AC_MSG_WARN([Deprecated API will be built, for backwards-compatibility.])
  }
  else
  {
    AC_MSG_WARN([Deprecated API will not be built, breaking backwards-compatibility. Do not use this build for distribution packages.])
    DISABLE_DEPRECATED_API_CFLAGS="-DGLIBMM_DISABLE_DEPRECATED"
    AC_SUBST(DISABLE_DEPRECATED_API_CFLAGS)
  }
  fi
])


## GLIBMM_ARG_ENABLE_API_DEFAULT_SIGNAL_HANDLERS()
##
## Provide the --enable-api-default-signal-handlers configure argument, enabled
## by default.
##
AC_DEFUN([GLIBMM_ARG_ENABLE_API_DEFAULT_SIGNAL_HANDLERS],
[
  AC_ARG_ENABLE([api-default-signal-handlers],
      [  --enable-api-default-signal-handlers  Build default signal handlers API.
                              [[default=yes]]],
      [glibmm_enable_api_default_signal_handlers="$enableval"],
      [glibmm_enable_api_default_signal_handlers='yes'])

  if test "x$glibmm_enable_api_default_signal_handlers" = "xyes"; then
  {
    AC_DEFINE([GLIBMM_DEFAULT_SIGNAL_HANDLERS_ENABLED],[1], [Defined when the --enable-api-default-signal-handlers configure argument was given])
  }
  fi
])
