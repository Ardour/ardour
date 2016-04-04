#ifndef __liblua_visibility_h__
#define __liblua_visibility_h__

#if defined(COMPILER_MSVC)
#  define LIBLUA_DLL_IMPORT __declspec(dllimport)
#  define LIBLUA_DLL_EXPORT __declspec(dllexport)
#  define LIBLUA_DLL_LOCAL
#else
#  define LIBLUA_DLL_IMPORT __attribute__ ((visibility ("default")))
#  define LIBLUA_DLL_EXPORT __attribute__ ((visibility ("default")))
#  define LIBLUA_DLL_LOCAL  __attribute__ ((visibility ("hidden")))
#endif


#ifdef COMPILER_MSVC
// MSVC: build liblua as DLL
#  define LIBLUA_BUILD_AS_DLL
#else
// others currently use a static lib (incl. with libardour)
#  define LIBLUA_STATIC
#endif


#ifdef LIBLUA_STATIC
#  define LIBLUA_API
#else
// define when building the DLL (instead of using it)
#  ifdef LIBLUA_DLL_EXPORTS
#    define LIBLUA_API LIBLUA_DLL_EXPORT
#  else
#    define LIBLUA_API LIBLUA_DLL_IMPORT
#  endif
#endif

#endif /* __liblua_visibility_h__ */
