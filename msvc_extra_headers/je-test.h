#ifndef _TARGETSXS_H_
#define _TARGETSXS_H_

#pragma warning( disable : 4996 )

#ifndef HAVE_LV2
#define HAVE_SUIL
#define HAVE_LV2
/* Comment out the above lines to build Mixbus without LV2 support */
#endif

#ifndef VST_SUPPORT
#define VST_SUPPORT
/* Comment out the above line to build Mixbus without VST support */
#endif

#ifndef JACK_32_64
#define JACK_32_64
/* Shouldn't really be needed but make sure that any structs we
   obtain from libjack will have 1-byte packing alignment where
   necessary (belt & braces approach to be on the safe side) */
#endif

#ifdef _DEBUG
#define _SECURE_SCL 1
#define _HAS_ITERATOR_DEBUGGING 1
/* #define to zero for a more conventional Debug build */
#endif

#ifndef __midl
#if defined(_DEBUG) || defined (DEBUG)
/* Experimental - link to the lowest DebugCRT so we can run on another system */
#define _SXS_ASSEMBLY_VERSION "8.0.50727.42"
#else
#define _SXS_ASSEMBLY_VERSION "8.0.50727.6195"
#endif
#define _CRT_ASSEMBLY_VERSION _SXS_ASSEMBLY_VERSION
#define _MFC_ASSEMBLY_VERSION _SXS_ASSEMBLY_VERSION
#define _ATL_ASSEMBLY_VERSION _SXS_ASSEMBLY_VERSION

#ifdef __cplusplus
extern "C" {
#endif
__declspec(selectany) int _forceCRTManifest;
__declspec(selectany) int _forceMFCManifest;
__declspec(selectany) int _forceAtlDllManifest;
__declspec(selectany) int _forceCRTManifestRTM;
__declspec(selectany) int _forceMFCManifestRTM;
__declspec(selectany) int _forceAtlDllManifestRTM;
#ifdef __cplusplus
}
#endif
#endif

/* 'stdint.h' conflicts with various other libraries so
   let's #include stdint.h first to ensure one consistent
   implementation for commonly used integer types. */
#include <stdint.h>

#if (BUILDING_ARDOUR)
#if defined(_MSC_VER) && !defined(__MINGW__) && !defined(__MINGW32__)
#include <ardourext/misc.h>
#endif
#endif

#endif /*_TARGETSXS_H_*/
