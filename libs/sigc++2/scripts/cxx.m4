dnl
dnl SIGC_CXX_TEMPLATE_SPECIALIZATION_OPERATOR_OVERLOAD()
dnl
dnl
AC_DEFUN([SIGC_CXX_GCC_TEMPLATE_SPECIALIZATION_OPERATOR_OVERLOAD],[
AC_MSG_CHECKING([if C++ compiler supports the use of a particular specialization when calling operator() template methods.])
AC_TRY_COMPILE(
[
  #include <iostream>

  class Thing
  {
    public:
    Thing()
    {}

    template <class T>
    void operator()(T a, T b)
    {
      T c = a + b;
      std::cout << c << std::endl;
    }
  };

  template<class T2>
  class OtherThing
  {
  public:
    void do_something()
    {
       Thing thing_;
       thing_.template operator()<T2>(1, 2);
       //This fails with or without the template keyword, on SUN Forte C++ 5.3, 5.4, and 5.5:
    }
  };
],
[
  OtherThing<int> thing;
  thing.do_something();
],
[
  sigcm_cxx_gcc_template_specialization_operator_overload=yes
  AC_DEFINE([SIGC_GCC_TEMPLATE_SPECIALIZATION_OPERATOR_OVERLOAD],[1],[does the C++ compiler support the use of a particular specialization when calling operator() template methods.])
  AC_MSG_RESULT([$sigcm_cxx_gcc_template_specialization_operator_overload])
],[
  sigcm_cxx_gcc_template_specialization_operator_overload=no
  AC_MSG_RESULT([$sigcm_cxx_gcc_template_specialization_operator_overload])
])
])

AC_DEFUN([SIGC_CXX_MSVC_TEMPLATE_SPECIALIZATION_OPERATOR_OVERLOAD],[
AC_MSG_CHECKING([if C++ compiler supports the use of a particular specialization when calling operator() template methods omitting the template keyword.])
AC_TRY_COMPILE(
[
  #include <iostream>

  class Thing
  {
    public:
    Thing()
    {}

    template <class T>
    void operator()(T a, T b)
    {
      T c = a + b;
      std::cout << c << std::endl;
    }
  };

  template<class T2>
  class OtherThing
  {
  public:
    void do_something()
    {
       Thing thing_;
       thing_.operator()<T2>(1, 2);
       //This fails with or without the template keyword, on SUN Forte C++ 5.3, 5.4, and 5.5:
    }
  };
],
[
  OtherThing<int> thing;
  thing.do_something();
],
[
  sigcm_cxx_msvc_template_specialization_operator_overload=yes
  AC_DEFINE([SIGC_MSVC_TEMPLATE_SPECIALIZATION_OPERATOR_OVERLOAD],[1],[does the C++ compiler support the use of a particular specialization when calling operator() template methods omitting the template keyword.])
  AC_MSG_RESULT([$sigcm_cxx_msvc_template_specialization_operator_overload])
],[
  sigcm_cxx_msvc_template_specialization_operator_overload=no
  AC_MSG_RESULT([$sigcm_cxx_msvc_template_specialization_operator_overload])
])
])


AC_DEFUN([SIGC_CXX_SELF_REFERENCE_IN_MEMBER_INITIALIZATION], [
AC_MSG_CHECKING([if C++ compiler allows usage of member function in initialization of static member field.])
AC_TRY_COMPILE(
[
  struct test
  {
    static char  test_function();

    // Doesn't work with e.g. GCC 3.2.  However, if test_function()
    // is wrapped in a nested structure, it works just fine.
    static const bool  test_value
      = (sizeof(test_function()) == sizeof(char));
  };
],
[],
[
  sigcm_cxx_self_reference_in_member_initialization=yes
  AC_DEFINE([SIGC_SELF_REFERENCE_IN_MEMBER_INITIALIZATION],[1],
            [does c++ compiler allows usage of member function in initialization of static member field.])
  AC_MSG_RESULT([$sigcm_cxx_self_reference_in_member_initialization])
],[
  sigcm_cxx_self_reference_in_member_initialization=no
  AC_MSG_RESULT([$sigcm_cxx_self_reference_in_member_initialization])
])
])
