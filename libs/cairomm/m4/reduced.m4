## CAIROMM_ARG_ENABLE_API_EXCEPTIONS()
##
## Provide the --enable-api-exceptions configure argument, enabled
## by default.
##
AC_DEFUN([CAIROMM_ARG_ENABLE_API_EXCEPTIONS],
[
  AC_ARG_ENABLE([api-exceptions],
      [  --enable-api-exceptions  Build exceptions API.
                              [[default=yes]]],
      [cairomm_enable_api_exceptions="$enableval"],
      [cairomm_enable_api_exceptions='yes'])

  if test "x$cairomm_enable_api_exceptions" = "xyes"; then
  {
    AC_DEFINE([CAIROMM_EXCEPTIONS_ENABLED],[1], [Defined when the --enable-api-exceptions configure argument was given])
  }
  fi
])

