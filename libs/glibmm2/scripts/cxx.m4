
dnl
dnl AC_CXX_NAMESPACES(ACTION_FOUND,ACTION_NOT_FOUND)
dnl
AC_DEFUN([AC_CXX_NAMESPACES],[
AC_MSG_CHECKING(if C++ compiler supports namespaces)
AC_TRY_COMPILE(
[
namespace Foo { struct A {}; }
using namespace Foo;
],[
A a;
(void)a;
],[
 ac_cxx_namespaces=yes
 AC_MSG_RESULT([$ac_cxx_namespaces])
 $1
],[
 ac_cxx_namespaces=no
 AC_MSG_RESULT([$ac_cxx_namespaces])
 $2
])
])

dnl
dnl AC_CXX_NAMESPACES(ACTION_FOUND,ACTION_NOT_FOUND)
dnl
AC_DEFUN([AC_CXX_BOOL],[
AC_MSG_CHECKING(if C++ compiler supports bool)
AC_TRY_COMPILE(
[
],[
   bool b=true;
   bool b1=false;
   (void)b;
   (void)b1;
],[
  ac_cxx_bool=yes
  AC_MSG_RESULT([$ac_cxx_bool])
  $1
],[
  ac_cxx_bool=no
  AC_MSG_RESULT([$ac_cxx_bool])
  $2
])
])

dnl
dnl AC_CXX_MUTABLE(ACTION_FOUND,ACTION_NOT_FOUND)
dnl
AC_DEFUN([AC_CXX_MUTABLE],[
AC_MSG_CHECKING(if C++ compiler supports mutable)
AC_TRY_COMPILE(
[
class k {   
  mutable char *c;
public:
   void foo() const { c=0; }
};
],[
],[
  ac_cxx_mutable=yes
  AC_MSG_RESULT([$ac_cxx_mutable])
  $1
],[
  ac_cxx_mutable=no
  AC_MSG_RESULT([$ac_cxx_mutable])
  $2
]) 
])


dnl
dnl AC_CXX_CONST_CAST(ACTION_FOUND,ACTION_NOT_FOUND)
dnl
AC_DEFUN([AC_CXX_CONST_CAST],[
AC_MSG_CHECKING([if C++ compiler supports const_cast<>])
AC_TRY_COMPILE(
[
   class foo;
],[
   const foo *c=0;
   foo *c1=const_cast<foo*>(c);
   (void)c1;
],[
  ac_cxx_const_cast=yes
  AC_MSG_RESULT([$ac_cxx_const_cast])
],[
  ac_cxx_const_cast=no
  AC_MSG_RESULT([$ac_cxx_const_cast])
])
])


dnl
dnl GLIBMM_CXX_MEMBER_FUNCTIONS_MEMBER_TEMPLATES(ACTION_FOUND,ACTION_NOT_FOUND)
dnl
dnl Test whether the compiler allows member functions to refer to spezialized member function templates.
dnl Some compilers have problems with this. gcc 2.95.3 aborts with an internal compiler error.
dnl
AC_DEFUN([GLIBMM_CXX_MEMBER_FUNCTIONS_MEMBER_TEMPLATES],[
AC_MSG_CHECKING([if C++ compiler allows member functions to refer to member templates])
AC_TRY_COMPILE(
[
  struct foo {
    template <class C> inline
    void doit();
    void thebug();
  };

  template <class C> inline
  void foo::doit() {
  }

  struct bar {
    void neitherabug();
  };

  void notabug() {
    void (foo::*func)();
    func = &foo::doit<int>;
    (void)func;
  }

  void bar::neitherabug() {
    void (foo::*func)();
    func = &foo::doit<int>;
    (void)func;
  }

  void foo::thebug() {
    void (foo::*func)();
    func = &foo::doit<int>; //Compiler bugs usually show here.
    (void)func;
  }
],[],[
  glibmm_cxx_member_functions_member_templates=yes
  AC_DEFINE([GLIBMM_MEMBER_FUNCTIONS_MEMBER_TEMPLATES],[1],[does the C++ compiler allow member functions to refer to member templates])
  AC_MSG_RESULT([$glibmm_cxx_member_functions_member_templates])
],[
  glibmm_cxx_member_functions_member_templates=no
  AC_DEFINE([GLIBMM_MEMBER_FUNCTIONS_MEMBER_TEMPLATES],[0])
  AC_MSG_RESULT([$glibmm_cxx_member_functions_member_templates])
])
])

## GLIBMM_CXX_CAN_DISAMBIGUATE_CONST_TEMPLATE_SPECIALIZATIONS()
##
## Check whether the compiler finds it ambiguous to have both
## const and non-const template specializations,
## The SUN Forte compiler has this problem, though we are
## not 100% sure that it's a C++ standards violation.
##
AC_DEFUN([GLIBMM_CXX_CAN_DISAMBIGUATE_CONST_TEMPLATE_SPECIALIZATIONS],
[
  AC_REQUIRE([GLIBMM_CXX_HAS_NAMESPACE_STD])

  AC_CACHE_CHECK(
    [whether the compiler finds it ambiguous to have both const and non-const template specializations],
    [glibmm_cv_cxx_can_disambiguate_const_template_specializations],
  [
    AC_TRY_COMPILE(
    [
      #include <iostream>

      template <class T> class Foo {};

      template <typename T> class Traits {
      public:
          const char* whoami() {
              return "generic template";
          }
      };

      template <typename T> class Traits<Foo<T> > {
      public:
          const char* whoami() {
              return "partial specialization for Foo<T>";
          }
      };

      template <typename T> class Traits<Foo<const T> > {
      public:
          const char* whoami() {
              return "partial specialization for Foo<const T>";
          }
      };
      
    ],[
          Traits<int> it;
          Traits<Foo<int> > fit;
          Traits<Foo<const int> > cfit;

          std::cout << "Traits<int>             --> "
                    << it.whoami() << std::endl;
          std::cout << "Traits<Foo<int>>        --> "
                    << fit.whoami() << std::endl;
          std::cout << "Traits<Foo<const int >> --> "
                    << cfit.whoami() << std::endl;
    ],
      [glibmm_cv_cxx_can_disambiguate_const_template_specializations="yes"],
      [glibmm_cv_cxx_can_disambiguate_const_template_specializations="no"]
    )
  ])

  if test "x${glibmm_cv_cxx_can_disambiguate_const_template_specializations}" = "xyes"; then
  {
    AC_DEFINE([GLIBMM_HAVE_DISAMBIGUOUS_CONST_TEMPLATE_SPECIALIZATIONS],[1], [Defined if the compiler does not find it ambiguous to have both const and non-const template specializations])
  }
  fi
])



## GLIBMM_CXX_CAN_USE_DYNAMIC_CAST_IN_UNUSED_TEMPLATE_WITHOUT_DEFINITION()
##
## Check whether the compiler allows us to define a template that uses 
## dynamic_cast<> with an object whose type is not defined, 
## even if we do not use that template before we have defined the type.
## This should probably not be allowed anyway.
##
AC_DEFUN([GLIBMM_CXX_CAN_USE_DYNAMIC_CAST_IN_UNUSED_TEMPLATE_WITHOUT_DEFINITION],
[
  AC_CACHE_CHECK(
    [whether the compiler allows us to define a template that uses dynamic_cast<> with an object whose type is not yet defined],
    [glibmm_cv_cxx_can_use_dynamic_cast_in_unused_template_without_definition],
  [
    AC_TRY_COMPILE(
    [
      class SomeClass;

      SomeClass* some_function();

      template <class T>
      class SomeTemplate
      {
        static bool do_something()
        {
          //This does not compile, with the MipsPro (IRIX) compiler
          //even if we don't use this template at all.
          //(We would use it later, after we have defined the type).
          return dynamic_cast<T*>(some_function());
        }
      };
      
    ],[
       
    ],
      [glibmm_cv_cxx_can_use_dynamic_cast_in_unused_template_without_definition="yes"],
      [glibmm_cv_cxx_can_use_dynamic_cast_in_unused_template_without_definition="no"]
    )
  ])

  if test "x${glibmm_cv_cxx_can_use_dynamic_cast_in_unused_template_without_definition}" = "xyes"; then
  {
    AC_DEFINE([GLIBMM_CAN_USE_DYNAMIC_CAST_IN_UNUSED_TEMPLATE_WITHOUT_DEFINITION],[1], [Defined if the compiler allows us to define a template that uses dynamic_cast<> with an object whose type is not yet defined.])
  }
  fi
])


## GLIBMM_CXX_CAN_ASSIGN_NON_EXTERN_C_FUNCTIONS_TO_EXTERN_C_CALLBACKS()
##
## Check whether the compiler allows us to use a non-extern "C" function, 
## such as a static member function, to an extern "C" function pointer, 
## such as a GTK+ callback.
## This should not be allowed anyway.
##
AC_DEFUN([GLIBMM_CXX_CAN_ASSIGN_NON_EXTERN_C_FUNCTIONS_TO_EXTERN_C_CALLBACKS],
[
  AC_CACHE_CHECK(
    [whether the the compilerallows us to use a non-extern "C" function for an extern "C" function pointer.],
    [glibmm_cv_cxx_can_assign_non_extern_c_functions_to_extern_c_callbacks],
  [
    AC_TRY_COMPILE(
    [
      extern "C"
      {
        struct somestruct
        {
          void (*callback) (int);
        };
        
      } // extern "C"
      
      void somefunction(int)
      {
      }
      
    ],[ 
      somestruct something;
      something.callback = &somefunction;
    ],
      [glibmm_cv_cxx_can_assign_non_extern_c_functions_to_extern_c_callbacks="yes"],
      [glibmm_cv_cxx_can_assign_non_extern_c_functions_to_extern_c_callbacks="no"]
    )
  ])

  if test "x${glibmm_cv_cxx_can_assign_non_extern_c_functions_to_extern_c_callbacks}" = "xyes"; then
  {
    AC_DEFINE([GLIBMM_CAN_ASSIGN_NON_EXTERN_C_FUNCTIONS_TO_EXTERN_C_CALLBACKS],[1], [Defined if the compiler allows us to use a non-extern "C" function for an extern "C" function pointer.])
  }
  fi
])

## GLIBMM_CXX_CAN_USE_NAMESPACES_INSIDE_EXTERNC()
##
## Check whether the compiler puts extern "C" functions in the global namespace, 
## even inside a namespace declaration. The AIX xlC compiler does this, and also 
## gets confused if we declare the namespace again inside the extern "C" block.
## This seems like a compiler bug, but not a serious one.
##
AC_DEFUN([GLIBMM_CXX_CAN_USE_NAMESPACES_INSIDE_EXTERNC],
[
  AC_CACHE_CHECK(
    [whether the compiler uses namespace declarations inside extern "C" blocks.],
    [glibmm_cv_cxx_can_use_namespaces_inside_externc],
  [
    AC_TRY_COMPILE(
    [
      namespace test
      {
      
      extern "C"
      {
      
      void do_something();
      
      } //extern C
      
      
      class Something
      {
      protected:
        int i;
      
        friend void do_something();
      };
      
      void do_something()
      {
        Something something;
        something.i = 1;
      }
      
      } //namespace

      
    ],[ 
     
    ],
      [glibmm_cv_cxx_can_use_namespaces_inside_externc="yes"],
      [glibmm_cv_cxx_can_use_namespaces_inside_externc="no"]
    )
  ])

  if test "x${glibmm_cv_cxx_can_use_namespaces_inside_externc}" = "xyes"; then
  {
    AC_DEFINE([GLIBMM_CAN_USE_NAMESPACES_INSIDE_EXTERNC],[1], [Defined if the compiler whether the compiler uses namespace declarations inside extern "C" blocks.])
  }
  fi
])


