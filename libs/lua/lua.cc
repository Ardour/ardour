#if _MSC_VER
#pragma push_macro("_CRT_SECURE_NO_WARNINGS")
#ifndef _CRT_SECURE_NO_WARNINGS
#define _CRT_SECURE_NO_WARNINGS
#endif
#endif

#ifdef PLATFORM_WINDOWS
# define LUA_USE_WINDOWS
#elif defined __APPLE__
# define LUA_USE_MACOSX
#else
# define LUA_USE_LINUX
#endif

extern "C"
{

#define lobject_c
#define lvm_c
#define LUA_CORE
#define LUA_LIB
#include "lua-5.3.2/luaconf.h"
#undef lobject_c
#undef lvm_c
#undef LUA_CORE
#undef LUA_LIB

// override luaconf.h symbol export
#undef LUA_API
#undef LUALIB_API
#undef LUAMOD_API
#define LUA_API		extern "C"
#define LUALIB_API	LUA_API
#define LUAMOD_API	LUALIB_API

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

#include "lua-5.3.2/ltable.c"

#include "lua-5.3.2/lauxlib.c"
#include "lua-5.3.2/lbaselib.c"

#include "lua-5.3.2/lbitlib.c"
#include "lua-5.3.2/lcorolib.c"
#include "lua-5.3.2/ldblib.c"
#include "lua-5.3.2/linit.c"
#include "lua-5.3.2/liolib.c"
#include "lua-5.3.2/lmathlib.c"
#include "lua-5.3.2/loslib.c"
#include "lua-5.3.2/lstrlib.c"
#include "lua-5.3.2/ltablib.c"

#include "lua-5.3.2/lapi.c"
#include "lua-5.3.2/lcode.c"
#include "lua-5.3.2/lctype.c"
#include "lua-5.3.2/ldebug.c"
#include "lua-5.3.2/ldo.c"
#include "lua-5.3.2/ldump.c"
#include "lua-5.3.2/lfunc.c"
#include "lua-5.3.2/lgc.c"
#include "lua-5.3.2/llex.c"
#include "lua-5.3.2/lmem.c"
#include "lua-5.3.2/lobject.c"
#include "lua-5.3.2/lopcodes.c"
#include "lua-5.3.2/lparser.c"
#include "lua-5.3.2/lstate.c"
#include "lua-5.3.2/lstring.c"
#include "lua-5.3.2/ltm.c"
#include "lua-5.3.2/lundump.c"
#include "lua-5.3.2/lutf8lib.c"
#include "lua-5.3.2/lvm.c"
#include "lua-5.3.2/lzio.c"

#include "lua-5.3.2/loadlib.c"

#if _MSC_VER
#pragma warning (pop)
#endif

}

#if _MSC_VER
#pragma pop_macro("_CRT_SECURE_NO_WARNINGS")
#endif
