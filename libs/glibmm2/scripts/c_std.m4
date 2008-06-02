cv_c_std_time_t_is_not_int32
## GLIBMM_CXX_HAS_NAMESPACE_STD()
##
## Test whether libstdc++ declares namespace std.  For safety,
## also check whether several randomly selected STL symbols
## are available in namespace std.
##
## On success, #define GLIBMM_HAVE_NAMESPACE_STD to 1.
##
AC_DEFUN([GLIBMM_C_STD_TIME_T_IS_NOT_INT32],
[
  AC_CACHE_CHECK(
    [whether time_t is not equivalent to gint32, meaning that it can be used for a method overload],
    [gtkmm_cv_c_std_time_t_is_not_int32],
  [
    AC_TRY_COMPILE(
    [
      #include <time.h>
    ],[
      typedef signed int gint32;
      class Test
      {
        void something(gint32 val)
        {}

        void something(time_t val)
        {}
      };
    ],
      [gtkmm_cv_c_std_time_t_is_not_int32="yes"],
      [gtkmm_cv_c_std_time_t_is_not_int32="no"]
    )
  ])

  if test "x${gtkmm_cv_c_std_time_t_is_not_int32}" = "xyes"; then
  {
    AC_DEFINE([GLIBMM_HAVE_C_STD_TIME_T_IS_NOT_INT32],[1], [Defined when time_t is not equivalent to gint32, meaning that it can be used for a method overload])
  }
  fi
])



