cv_cxx_has_namespace_std
## SIGC_CXX_HAS_NAMESPACE_STD()
##
## Test whether libstdc++ declares namespace std.  For safety,
## also check whether several randomly selected STL symbols
## are available in namespace std.
##
## On success, #define SIGC_HAVE_NAMESPACE_STD to 1.
##
AC_DEFUN([SIGC_CXX_HAS_NAMESPACE_STD],
[
  AC_CACHE_CHECK(
    [whether C++ library symbols are declared in namespace std],
    [sigc_cv_cxx_has_namespace_std],
  [
    AC_TRY_COMPILE(
    [
      #include <algorithm>
      #include <iterator>
      #include <iostream>
      #include <string>
    ],[
      using std::min;
      using std::find;
      using std::copy;
      using std::bidirectional_iterator_tag;
      using std::string;
      using std::istream;
      using std::cout;
    ],
      [sigc_cv_cxx_has_namespace_std="yes"],
      [sigc_cv_cxx_has_namespace_std="no"]
    )
  ])

  if test "x${sigc_cv_cxx_has_namespace_std}" = "xyes"; then
  {
    AC_DEFINE([SIGC_HAVE_NAMESPACE_STD],[1], [Defined when the libstdc++ declares the std-namespace])
  }
  fi
])

## SIGC_CXX_HAS_SUN_REVERSE_ITERATOR()
##
## Check for Sun libCstd style std::reverse_iterator, which demands more than just one template parameter.
## and #define SIGC_HAVE_SUN_REVERSE_ITERATOR if found.
##
AC_DEFUN([SIGC_CXX_HAS_SUN_REVERSE_ITERATOR],
[
  AC_REQUIRE([SIGC_CXX_HAS_NAMESPACE_STD])

  AC_CACHE_CHECK(
    [for non-standard Sun libCstd reverse_iterator],
    [sigc_cv_cxx_has_sun_reverse_iterator],
  [
    AC_TRY_COMPILE(
    [
      #include <iterator>
      #ifdef SIGC_HAVE_NAMESPACE_STD
      using namespace std;
      #endif
    ],[
      typedef reverse_iterator<char*,random_access_iterator_tag,char,char&,char*,int> ReverseIter;
    ],
      [sigc_cv_cxx_has_sun_reverse_iterator="yes"],
      [sigc_cv_cxx_has_sun_reverse_iterator="no"]
    )
  ])

  if test "x${sigc_cv_cxx_has_sun_reverse_iterator}" = "xyes"; then
  {
    AC_DEFINE([SIGC_HAVE_SUN_REVERSE_ITERATOR],[1])
  }
  fi
])


