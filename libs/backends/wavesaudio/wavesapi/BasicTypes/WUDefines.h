/*
    Copyright (C) 2014 Waves Audio Ltd.

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.

*/

#ifndef __WUDefines_h__
    #define __WUDefines_h__

/*Copy to include
#include "BasicTypes/WUDefines.h"
*/

#include "1.0/WavesPublicAPI_Defines.h"

// When changing wvNS value also do the same change in Objective_C_MangledNames.h
// because CWSAUCocoaViewFactoryAsString is hard coded there
#define wvNS wvWavesV9_3 
#ifdef __APPLE__
    #define ObjCNameSpace(__className__) wvWavesV9_3_ ## __className__
#endif

#ifdef INSIDE_NETSHELL
    #define DllExport
#else
    #define DllExport   WPAPI_DllExport 
#endif

#define __CDECL     __WPAPI_CDECL
#define __STDCALL   __WPAPI_STDCALL


#ifndef NULL
    #define NULL (0)
#endif

#ifndef nil
    #define nil NULL
#endif

#define PASCAL_MAC_ONLY #error do not use PASCAL_MAC_ONLY. See defintions in WavesFTT.h for replacment.
#define CALLCON #error do not use CALLCON. See defintions in WavesFTT.h for replacment.
#define FUNCEXP #error do not use FUNCEXP. See defintions in WavesFTT.h for replacment.

#define WUNUSED_PARAM(__SOME_UNUSED_PARAM__) ((void)__SOME_UNUSED_PARAM__)

#ifdef __APPLE__
    const char* const  OS_NAME = "Mac";

    #define WIN_ONLY(__Something_only_for_windows__)
    #define MAC_ONLY(__Something_only_for_mac__) __Something_only_for_mac__
    
    #if defined(i386) || defined(__i386) || defined(__i386__)
        #define kNumArchBits 32
    #endif
    #if defined(__x86_64) || defined(__x86_64__)
        #define kNumArchBits 64
    #endif 

    #if (__i386 || __x86_64) && !defined(__LITTLE_ENDIAN__)
        #define __LITTLE_ENDIAN__ 
    #endif
    #if !(__i386 || __x86_64) && !defined(__BIG_ENDIAN__)
        #define __BIG_ENDIAN__
    #endif
    #ifdef __GNUC__
        #define STD_EXCEPT_WIN std
        #define FAR 
        #define PASCAL 
        // #define HINSTANCE void*
        #define WINAPI
    
    #else
    
        #define DllExport_WinOnly
        #define STD_EXCEPT_WIN std
        #define FAR 
        #define PASCAL          // windows' pascal
        #define HINSTANCE void*
        #define WINAPI

    #endif
    #define THROW_SPEC(THROW_OBJ) throw (THROW_OBJ)

    #define WUNUSED_PARAM_ON_MAC(__SOME_UNUSED_PARAM__) WUNUSED_PARAM(__SOME_UNUSED_PARAM__)
    #define WUNUSED_PARAM_ON_WIN(__SOME_UNUSED_PARAM__)
#endif


#ifdef PLATFORM_WINDOWS
    const char* const  OS_NAME = "Win";

    #define WIN_ONLY(__Something_only_for_windows__) __Something_only_for_windows__
    #define MAC_ONLY(__Something_only_for_mac__)

    #if defined(_M_X64)
        #define kNumArchBits 64
    #else // not sure what are the VisualStudio macros for 32 bits
        #define kNumArchBits 32
    #endif

    #define DllExport_WinOnly DllExport     // help solve window specific link errors
    #define STD_EXCEPT_WIN

    #if !defined(__MINGW64__)
		#define round(x) (floor(x+0.5))
	#endif

    #define __LITTLE_ENDIAN__
    #define THROW_SPEC(THROW_OBJ) throw (...)

    #define WUNUSED_PARAM_ON_MAC(__SOME_UNUSED_PARAM__)
    #define WUNUSED_PARAM_ON_WIN(__SOME_UNUSED_PARAM__) WUNUSED_PARAM(__SOME_UNUSED_PARAM__)

#endif 

#ifdef __linux__
    const char* const  OS_NAME = "Linux";

    #define WIN_ONLY(__Something_only_for_windows__)
    #define MAC_ONLY(__Something_only_for_mac__)

    #define DllExport_WinOnly
    #define STD_EXCEPT_WIN std
    #define FAR 
    #define PASCAL 
    // #define HINSTANCE void*
    #define WINAPI
    #if __i386 && !defined(__LITTLE_ENDIAN__)
        #define __LITTLE_ENDIAN__ 
    #endif
    #if !__i386 && !defined(__BIG_ENDIAN__)
        #define __BIG_ENDIAN__
    #endif
    #define THROW_SPEC(THROW_OBJ) throw (THROW_OBJ)
    
    #if defined(__x86_64) || defined(__LP64__)
        #error "64 bit not suported yet on linux"
    #else
        #define kNumArchBits 32
    #endif
#endif

#ifndef _WU_DECL
    #define _WU_DECL __CDECL // the default is calling model is cdecl, but you can also set this macro from the outside to something different 
#endif

#ifndef _XML_DECL
    #define _XML_DECL __CDECL // the default is calling model is cdecl, but you can also set this macro from the outside to something different 
#endif

#ifndef kNumArchBits
    #error Macro kNumArchBits was not defined
#endif

#if kNumArchBits == 64
    const char* const kNumArchBits_c_str = "64";
#endif
#if kNumArchBits == 32
    const char* const kNumArchBits_c_str = "32";
#endif

#endif //__WUDefines_h__
