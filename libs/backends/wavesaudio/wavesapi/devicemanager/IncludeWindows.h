#ifndef __IncludeWindows_h__
#define __IncludeWindows_h__

#ifdef PLATFORM_WINDOWS

/* Copy to include
#include "IncludeWindows.h"
*/

#ifndef _WIN32_WINNT
#define _WIN32_WINNT	0x0601   // Windows 7
#endif

#ifndef WINVER
#define WINVER			0x0601   // Windows 7
#endif

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

#ifndef NOMINMAX
#define NOMINMAX // DO NOT REMOVE NOMINMAX - DOING SO CAUSES CONFLICTS WITH STD INCLUDES (<limits> ...)
#endif

#include <WinSock2.h>
#include <Windows.h>
#include <objbase.h>
#endif // #if PLATFORM_WINDOWS 
#endif // #ifndef __IncludeWindows_h__

