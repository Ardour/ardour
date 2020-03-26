#ifndef __libzresampler_visibility_h__
#define __libzresampler_visibility_h__

#if defined(COMPILER_MSVC)
  #define LIBZRESAMPLER_DLL_IMPORT __declspec(dllimport)
  #define LIBZRESAMPLER_DLL_EXPORT __declspec(dllexport)
  #define LIBZRESAMPLER_DLL_LOCAL
#else
  #define LIBZRESAMPLER_DLL_IMPORT __attribute__ ((visibility ("default")))
  #define LIBZRESAMPLER_DLL_EXPORT __attribute__ ((visibility ("default")))
  #define LIBZRESAMPLER_DLL_LOCAL  __attribute__ ((visibility ("hidden")))
#endif

#ifdef LIBZRESAMPLER_STATIC // libzita-resampler is a DLL
  #define LIBZRESAMPLER_API
  #define LIBZRESAMPLER_LOCAL
  #define LIBZRESAMPLER_TEMPLATE_API
  #define LIBZRESAMPLER_TEMPLATE_MEMBER_API
#else
  #ifdef LIBZRESAMPLER_DLL_EXPORTS // defined if we are building the libzita-resampler DLL (instead of using it)
    #define LIBZRESAMPLER_API LIBZRESAMPLER_DLL_EXPORT
    #define LIBZRESAMPLER_TEMPLATE_API LIBZRESAMPLER_TEMPLATE_DLL_EXPORT
    #define LIBZRESAMPLER_TEMPLATE_MEMBER_API LIBZRESAMPLER_TEMPLATE_MEMBER_DLL_EXPORT
  #else
    #define LIBZRESAMPLER_API LIBZRESAMPLER_DLL_IMPORT
    #define LIBZRESAMPLER_TEMPLATE_API LIBZRESAMPLER_TEMPLATE_DLL_IMPORT
    #define LIBZRESAMPLER_TEMPLATE_MEMBER_API LIBZRESAMPLER_TEMPLATE_MEMBER_DLL_IMPORT
  #endif
  #define LIBZRESAMPLER_LOCAL LIBZRESAMPLER_DLL_LOCAL
#endif

#endif
