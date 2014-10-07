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

#include <winsock2.h>
#include <windows.h>
#include <objbase.h>
#endif // #if PLATFORM_WINDOWS 
#endif // #ifndef __IncludeWindows_h__

