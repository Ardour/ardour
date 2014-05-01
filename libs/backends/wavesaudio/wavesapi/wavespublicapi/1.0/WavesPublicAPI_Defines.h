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
