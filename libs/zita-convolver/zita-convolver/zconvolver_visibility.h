#ifndef __libzconvolver_visibility_h__
#define __libzconvolver_visibility_h__

#if defined(COMPILER_MSVC)
  #define LIBZCONVOLVER_DLL_IMPORT __declspec(dllimport)
  #define LIBZCONVOLVER_DLL_EXPORT __declspec(dllexport)
  #define LIBZCONVOLVER_DLL_LOCAL
#else
  #define LIBZCONVOLVER_DLL_IMPORT __attribute__ ((visibility ("default")))
  #define LIBZCONVOLVER_DLL_EXPORT __attribute__ ((visibility ("default")))
  #define LIBZCONVOLVER_DLL_LOCAL  __attribute__ ((visibility ("hidden")))
#endif

#ifdef LIBZCONVOLVER_STATIC // libzita-convolver is a DLL
  #define LIBZCONVOLVER_API
  #define LIBZCONVOLVER_LOCAL
  #define LIBZCONVOLVER_TEMPLATE_API
  #define LIBZCONVOLVER_TEMPLATE_MEMBER_API
#else
  #ifdef LIBZCONVOLVER_DLL_EXPORTS // defined if we are building the libzita-convolver DLL (instead of using it)
    #define LIBZCONVOLVER_API LIBZCONVOLVER_DLL_EXPORT
    #define LIBZCONVOLVER_TEMPLATE_API LIBZCONVOLVER_TEMPLATE_DLL_EXPORT
    #define LIBZCONVOLVER_TEMPLATE_MEMBER_API LIBZCONVOLVER_TEMPLATE_MEMBER_DLL_EXPORT
  #else
    #define LIBZCONVOLVER_API LIBZCONVOLVER_DLL_IMPORT
    #define LIBZCONVOLVER_TEMPLATE_API LIBZCONVOLVER_TEMPLATE_DLL_IMPORT
    #define LIBZCONVOLVER_TEMPLATE_MEMBER_API LIBZCONVOLVER_TEMPLATE_MEMBER_DLL_IMPORT
  #endif
  #define LIBZCONVOLVER_LOCAL LIBZCONVOLVER_DLL_LOCAL
#endif

#endif
