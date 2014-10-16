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

#ifndef __WavesPublicAPI_Defines_h__
	#define __WavesPublicAPI_Defines_h__

/*Copy to include
#include "WavesPublicAPI_Defines.h"
*/

#ifdef __APPLE__

    #ifdef __GNUC__
        #define WPAPI_DllExport __attribute__ ((visibility("default")))
        #define __WPAPI_CDECL
        #define __WPAPI_STDCALL
    
    #else
    
        #define WPAPI_DllExport __declspec(export)
        #define __WPAPI_CDECL
        #define __WPAPI_STDCALL

    #endif

#endif


#ifdef PLATFORM_WINDOWS
    #define WPAPI_DllExport __declspec(dllexport)
	#define __WPAPI_CDECL __cdecl
	#define __WPAPI_STDCALL __stdcall
#endif 

#ifdef __linux__

    #define WPAPI_DllExport __attribute__ ((visibility("default")))

	#define __WPAPI_CDECL
	#define __WPAPI_STDCALL

#endif

#endif //__WavesPublicAPI_Defines_h__
