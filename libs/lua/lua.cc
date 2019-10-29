/* This is a C++ wrapper to compile the lua C code
 * with settings appropriate for including it with
 * Ardour.
 */

#include "lua/liblua_visibility.h"

#if _MSC_VER
# pragma push_macro("_CRT_SECURE_NO_WARNINGS")
#  ifndef _CRT_SECURE_NO_WARNINGS
#    define _CRT_SECURE_NO_WARNINGS
#  endif
#elif defined(__clang__)
#  pragma clang diagnostic push
#  pragma clang diagnostic ignored "-Wcast-qual"
#elif __GNUC__ > 4 || (__GNUC__ == 4 && __GNUC_MINOR__ >= 6)
#  pragma GCC diagnostic push
#  pragma GCC diagnostic ignored "-Wcast-qual"
#endif

// forward ardour's defines to luaconf.h
#ifdef PLATFORM_WINDOWS
#  define LUA_USE_WINDOWS
#elif defined __APPLE__
#  define LUA_USE_MACOSX
#else
#  define LUA_USE_LINUX
#endif

// forward liblua visibility to luaconf.h
#ifdef LIBLUA_BUILD_AS_DLL
#define LUA_BUILD_AS_DLL
#endif

extern "C"
{

#define lobject_c
#define lvm_c
#define LUA_CORE
#define LUA_LIB
#include "lua-5.3.5/luaconf.h"
#undef lobject_c
#undef lvm_c
#undef LUA_CORE
#undef LUA_LIB

// override luaconf.h symbol export
#ifdef LIBLUA_STATIC // static lib (no DLL)
#  undef LUA_API
#  undef LUALIB_API
#  undef LUAMOD_API
#  define LUA_API     extern "C"
#  define LUALIB_API  LUA_API
#  define LUAMOD_API  LUALIB_API
#endif

// disable support for extenal libs
#undef LUA_DL_DLL
#undef LUA_USE_DLOPEN

// enable bit lib
#define LUA_COMPAT_BITLIB

#if _MSC_VER
#pragma warning (push)
#pragma warning (disable: 4244) /* Possible loss of data */
#pragma warning (disable: 4702) /* Unreachable code */
#endif

#include "lua-5.3.5/ltable.c"

#include "lua-5.3.5/lauxlib.c"
#include "lua-5.3.5/lbaselib.c"

#include "lua-5.3.5/lbitlib.c"
#include "lua-5.3.5/lcorolib.c"
#include "lua-5.3.5/ldblib.c"
#include "lua-5.3.5/linit.c"
#include "lua-5.3.5/liolib.c"
#include "lua-5.3.5/lmathlib.c"
#include "lua-5.3.5/loslib.c"
#include "lua-5.3.5/lstrlib.c"
#include "lua-5.3.5/ltablib.c"

#include "lua-5.3.5/lapi.c"
#include "lua-5.3.5/lcode.c"
#include "lua-5.3.5/lctype.c"
#include "lua-5.3.5/ldebug.c"
#include "lua-5.3.5/ldo.c"
#include "lua-5.3.5/ldump.c"
#include "lua-5.3.5/lfunc.c"
#include "lua-5.3.5/lgc.c"
#include "lua-5.3.5/llex.c"
#include "lua-5.3.5/lmem.c"
#include "lua-5.3.5/lobject.c"
#include "lua-5.3.5/lopcodes.c"
#include "lua-5.3.5/lparser.c"
#include "lua-5.3.5/lstate.c"
#include "lua-5.3.5/lstring.c"
#include "lua-5.3.5/ltm.c"
#include "lua-5.3.5/lundump.c"
#include "lua-5.3.5/lutf8lib.c"
#include "lua-5.3.5/lvm.c"
#include "lua-5.3.5/lzio.c"

#include "lua-5.3.5/loadlib.c"

#if _MSC_VER
#pragma warning (pop)
#endif

} // end extern "C"

#if _MSC_VER
#  pragma pop_macro("_CRT_SECURE_NO_WARNINGS")
#elif defined(__clang__)
#  pragma clang diagnostic pop
#elif __GNUC__ > 4 || (__GNUC__ == 4 && __GNUC_MINOR__ >= 6)
#  pragma GCC diagnostic pop
#endif
