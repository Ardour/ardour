cv_cxx_has_namespace_std
## GLIBMM_CXX_HAS_NAMESPACE_STD()
##
## Test whether libstdc++ declares namespace std.  For safety,
## also check whether several randomly selected STL symbols
## are available in namespace std.
##
## On success, #define GLIBMM_HAVE_NAMESPACE_STD to 1.
##
AC_DEFUN([GLIBMM_CXX_HAS_NAMESPACE_STD],
[
  AC_CACHE_CHECK(
    [whether C++ library symbols are declared in namespace std],
    [gtkmm_cv_cxx_has_namespace_std],
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
      [gtkmm_cv_cxx_has_namespace_std="yes"],
      [gtkmm_cv_cxx_has_namespace_std="no"]
    )
  ])

  if test "x${gtkmm_cv_cxx_has_namespace_std}" = "xyes"; then
  {
    AC_DEFINE([GLIBMM_HAVE_NAMESPACE_STD],[1], [Defined when the libstdc++ declares the std-namespace])
  }
  fi
])


## GLIBMM_CXX_HAS_STD_ITERATOR_TRAITS()
##
## Check for standard-conform std::iterator_traits<>, and
## #define GLIBMM_HAVE_STD_ITERATOR_TRAITS on success.
##
AC_DEFUN([GLIBMM_CXX_HAS_STD_ITERATOR_TRAITS],
[
  AC_REQUIRE([GLIBMM_CXX_HAS_NAMESPACE_STD])

  AC_CACHE_CHECK(
    [whether the C++ library supports std::iterator_traits],
    [gtkmm_cv_cxx_has_std_iterator_traits],
  [
    AC_TRY_COMPILE(
    [
      #include <iterator>
      #ifdef GLIBMM_HAVE_NAMESPACE_STD
      using namespace std;
      #endif
    ],[
      typedef iterator_traits<char*>::value_type ValueType;
    ],
      [gtkmm_cv_cxx_has_std_iterator_traits="yes"],
      [gtkmm_cv_cxx_has_std_iterator_traits="no"]
    )
  ])

  if test "x${gtkmm_cv_cxx_has_std_iterator_traits}" = "xyes"; then
  {
    AC_DEFINE([GLIBMM_HAVE_STD_ITERATOR_TRAITS],[1], [Defined if std::iterator_traits<> is standard-conforming])
  }
  fi
])


## GLIBMM_CXX_HAS_SUN_REVERSE_ITERATOR()
##
## Check for Sun libCstd style std::reverse_iterator,
## and #define GLIBMM_HAVE_SUN_REVERSE_ITERATOR if found.
##
AC_DEFUN([GLIBMM_CXX_HAS_SUN_REVERSE_ITERATOR],
[
  AC_REQUIRE([GLIBMM_CXX_HAS_NAMESPACE_STD])

  AC_CACHE_CHECK(
    [for non-standard Sun libCstd reverse_iterator],
    [gtkmm_cv_cxx_has_sun_reverse_iterator],
  [
    AC_TRY_COMPILE(
    [
      #include <iterator>
      #ifdef GLIBMM_HAVE_NAMESPACE_STD
      using namespace std;
      #endif
    ],[
      typedef reverse_iterator<char*,random_access_iterator_tag,char,char&,char*,int> ReverseIter;
    ],
      [gtkmm_cv_cxx_has_sun_reverse_iterator="yes"],
      [gtkmm_cv_cxx_has_sun_reverse_iterator="no"]
    )
  ])

  if test "x${gtkmm_cv_cxx_has_sun_reverse_iterator}" = "xyes"; then
  {
    AC_DEFINE([GLIBMM_HAVE_SUN_REVERSE_ITERATOR],[1], [Defined if std::reverse_iterator is in Sun libCstd style])
  }
  fi
])


## GLIBMM_CXX_HAS_TEMPLATE_SEQUENCE_CTORS()
##
## Check whether the STL containers have templated sequence ctors,
## and #define GLIBMM_HAVE_TEMPLATE_SEQUENCE_CTORS on success.
##
AC_DEFUN([GLIBMM_CXX_HAS_TEMPLATE_SEQUENCE_CTORS],
[
  AC_REQUIRE([GLIBMM_CXX_HAS_NAMESPACE_STD])

  AC_CACHE_CHECK(
    [whether STL containers have templated sequence constructors],
    [gtkmm_cv_cxx_has_template_sequence_ctors],
  [
    AC_TRY_COMPILE(
    [
      #include <vector>
      #include <deque>
      #include <list>
      #ifdef GLIBMM_HAVE_NAMESPACE_STD
      using namespace std;
      #endif
    ],[
      const int array[8] = { 0, };
      vector<int>  test_vector (&array[0], &array[8]);
      deque<short> test_deque  (test_vector.begin(), test_vector.end());
      list<long>   test_list   (test_deque.begin(),  test_deque.end());
      test_vector.assign(test_list.begin(), test_list.end());
    ],
      [gtkmm_cv_cxx_has_template_sequence_ctors="yes"],
      [gtkmm_cv_cxx_has_template_sequence_ctors="no"]
    )
  ])

  if test "x${gtkmm_cv_cxx_has_template_sequence_ctors}" = "xyes"; then
  {
    AC_DEFINE([GLIBMM_HAVE_TEMPLATE_SEQUENCE_CTORS],[1], [Defined if the STL containers have templated sequence ctors])
  }
  fi
])

## GLIBMM_CXX_ALLOWS_STATIC_INLINE_NPOS()
##
## Check whether the a static member variable may be initialized inline to std::string::npos.
## The MipsPro (IRIX) compiler does not like this.
## and #define GLIBMM_HAVE_ALLOWS_STATIC_INLINE_NPOS on success.
##
AC_DEFUN([GLIBMM_CXX_ALLOWS_STATIC_INLINE_NPOS],
[
  AC_REQUIRE([GLIBMM_CXX_HAS_NAMESPACE_STD])

  AC_CACHE_CHECK(
    [whether the compiler allows a static member variable to be initialized inline to std::string::npos],
    [gtkmm_cv_cxx_has_allows_static_inline_npos],
  [
    AC_TRY_COMPILE(
    [
      #include <string>
      #include <iostream>
      
      class ustringtest
      {
        public:
        //The MipsPro compiler (IRIX) says "The indicated constant value is not known",
        //so we need to initalize the static member data elsewhere.
        static const std::string::size_type ustringnpos = std::string::npos;
      };
    ],[
      std::cout << "npos=" << ustringtest::ustringnpos << std::endl;
    ],
      [gtkmm_cv_cxx_has_allows_static_inline_npos="yes"],
      [gtkmm_cv_cxx_has_allows_static_inline_npos="no"]
    )
  ])

  if test "x${gtkmm_cv_cxx_has_allows_static_inline_npos}" = "xyes"; then
  {
    AC_DEFINE([GLIBMM_HAVE_ALLOWS_STATIC_INLINE_NPOS],[1], [Defined if a static member variable may be initialized inline to std::string::npos])
  }
  fi
])


