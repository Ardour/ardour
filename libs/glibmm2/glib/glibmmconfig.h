/* glib/glibmmconfig.h.  Generated from glibmmconfig.h.in by configure.  */
#ifndef _GLIBMM_CONFIG_H
#define _GLIBMM_CONFIG_H 1

/* version numbers */
#define GLIBMM_MAJOR_VERSION 2
#define GLIBMM_MINOR_VERSION 14
#define GLIBMM_MICRO_VERSION 2

// detect common platforms
#if defined(_WIN32)
// Win32 compilers have a lot of varation
#if defined(_MSC_VER)
#define GLIBMM_MSC
#define GLIBMM_WIN32
#define GLIBMM_DLL
#elif defined(__CYGWIN__)
#define GLIBMM_CONFIGURE
#elif defined(__MINGW32__)
#define GLIBMM_WIN32
#define GLIBMM_DLL
#define GLIBMM_CONFIGURE
#else
//AIX clR compiler complains about this even though it doesn't get this far:
//#warning "Unknown architecture (send me gcc --dumpspecs or equiv)"
#endif
#else
#define GLIBMM_CONFIGURE
#endif /* _WIN32 */

#ifdef GLIBMM_CONFIGURE
/* #undef GLIBMM_CXX_HAVE_MUTABLE */
/* #undef GLIBMM_CXX_HAVE_NAMESPACES */
/* #undef GLIBMM_HAVE_WIDE_STREAM */
//#undef GLIBMM_CXX_GAUB
//#undef GLIBMM_CXX_AMBIGUOUS_TEMPLATES
#define GLIBMM_HAVE_NAMESPACE_STD 1
#define GLIBMM_HAVE_STD_ITERATOR_TRAITS 1
/* #undef GLIBMM_HAVE_SUN_REVERSE_ITERATOR */
#define GLIBMM_HAVE_TEMPLATE_SEQUENCE_CTORS 1
#define GLIBMM_HAVE_DISAMBIGUOUS_CONST_TEMPLATE_SPECIALIZATIONS 1
#define GLIBMM_HAVE_C_STD_TIME_T_IS_NOT_INT32 1
/* #undef GLIBMM_COMPILER_SUN_FORTE */
/* #undef GLIBMM_DEBUG_REFCOUNTING */
#define GLIBMM_CAN_USE_DYNAMIC_CAST_IN_UNUSED_TEMPLATE_WITHOUT_DEFINITION 1
#define GLIBMM_CAN_ASSIGN_NON_EXTERN_C_FUNCTIONS_TO_EXTERN_C_CALLBACKS 1
#define GLIBMM_CAN_USE_NAMESPACES_INSIDE_EXTERNC 1
#define GLIBMM_HAVE_ALLOWS_STATIC_INLINE_NPOS 1
#define GLIBMM_PROPERTIES_ENABLED 1
#define GLIBMM_VFUNCS_ENABLED 1
#define GLIBMM_EXCEPTIONS_ENABLED 1
#define GLIBMM_DEFAULT_SIGNAL_HANDLERS_ENABLED 1
#endif

#ifdef GLIBMM_MSC
  #define GLIBMM_CXX_HAVE_MUTABLE 1
  #define GLIBMM_CXX_HAVE_NAMESPACES 1
  #define GLIBMM_HAVE_NAMESPACE_STD 1
  #define GLIBMM_HAVE_STD_ITERATOR_TRAITS 1
  #define GLIBMM_HAVE_TEMPLATE_SEQUENCE_CTORS 1
  #define GLIBMM_HAVE_DISAMBIGUOUS_CONST_TEMPLATE_SPECIALIZATIONS 1
  #define GLIBMM_HAVE_C_STD_TIME_T_IS_NOT_INT32 1
  #define GLIBMM_CAN_USE_DYNAMIC_CAST_IN_UNUSED_TEMPLATE_WITHOUT_DEFINITION 1
  #define GLIBMM_CAN_ASSIGN_NON_EXTERN_C_FUNCTIONS_TO_EXTERN_C_CALLBACKS 1
  #define GLIBMM_CAN_USE_NAMESPACES_INSIDE_EXTERNC 1
  #define GLIBMM_PROPERTIES_ENABLED 1
  #define GLIBMM_VFUNCS_ENABLED 1
  #define GLIBMM_EXCEPTIONS_ENABLED 1
  #define GLIBMM_DEFAULT_SIGNAL_HANDLERS_ENABLED 1
  #pragma warning (disable: 4786 4355 4800 4181)
#endif

#ifndef GLIBMM_HAVE_NAMESPACE_STD
#  define GLIBMM_USING_STD(Symbol) namespace std { using ::Symbol; }
#else
#  define GLIBMM_USING_STD(Symbol) /* empty */
#endif

#ifdef GLIBMM_DLL
  #if defined(GLIBMM_BUILD) && defined(_WINDLL)
    /* Do not dllexport as it is handled by gendef on MSVC */
    #define GLIBMM_API 
  #elif !defined(GLIBMM_BUILD)
    #define GLIBMM_API __declspec(dllimport)
  #else
    /* Build a static library */
    #define GLIBMM_API
  #endif /* GLIBMM_BUILD - _WINDLL */
#else
  #define GLIBMM_API
#endif /* GLIBMM_DLL */

#endif /* _GLIBMM_CONFIG_H */

