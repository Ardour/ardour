/*
 * Copyright (C) 2010-2013 Paul Davis <paul@linuxaudiosystems.com>
 * Copyright (C) 2013-2019 Robin Gareus <robin@gareus.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#ifdef WAF_BUILD
#include "libpbd-config.h"
#endif

#include <stdlib.h>

#ifdef __linux__
#include <unistd.h>
#elif defined(__APPLE__) || defined(__FreeBSD__) || defined(__NetBSD__) || defined(__OpenBSD__)
#include <stddef.h>
#include <sys/types.h>
#include <sys/sysctl.h>
#elif defined(PLATFORM_WINDOWS)
#include <windows.h>
#endif

#include "pbd/cpus.h"

#if defined(COMPILER_MSVC) && !defined(PTW32_VERSION)
#ifndef OTHER1
#include <ardourext/pthread.h>  // Gets us 'PTW32_VERSION'
#else
#include <pthread.h> //Gets us '__PTW32_VERSION'
#endif
#endif

int32_t
PBD::max_mmcss_threads_per_process ()
{
#ifdef PLATFORM_WINDOWS
	DWORD dwType = REG_DWORD;
	DWORD dwSize = 4;
	int32_t rv   = 32;
	HKEY hKey;
	if (ERROR_SUCCESS == RegOpenKeyExA (HKEY_LOCAL_MACHINE, "Software\\Microsoft\\Windows NT\\CurrentVersion\\Multimedia\\SystemProfile", 0, KEY_READ, &hKey)) {
		if (ERROR_SUCCESS == RegQueryValueExA (hKey, "MaxThreadsPerProcess", 0, &dwType, (LPBYTE)&rv, &dwSize)) {
			if (dwType == REG_DWORD && dwSize == 4) {
				return rv;
			}
		}
	}
	return 32;
#else
	return INT32_MAX;
#endif
}

uint32_t
PBD::hardware_concurrency()
{
	if (getenv("ARDOUR_CONCURRENCY")) {
		int c = atoi (getenv("ARDOUR_CONCURRENCY"));
		if (c > 0) {
			return c;
		}
	}
#if defined(PTW32_VERSION) || defined(__hpux) || defined(__PTW32_VERSION)
	return pthread_num_processors_np();
#elif defined(__APPLE__)
	int count;
	size_t size=sizeof(count);
# ifdef MIXBUS
	return sysctlbyname ("hw.logicalcpu", &count, &size, NULL, 0) ? 1 : count;
# else
	return sysctlbyname ("hw.physicalcpu", &count, &size, NULL, 0) ? 1 :count;
# endif
#elif defined(__FreeBSD__)
	int count;
	size_t size=sizeof(count);
	return sysctlbyname ("hw.ncpu", &count, &size, NULL, 0) ? 1 : count;
#elif defined(HAVE_UNISTD_H) && defined(_SC_NPROCESSORS_ONLN)
	int const count = sysconf (_SC_NPROCESSORS_ONLN);
	return count > 1 ? count : 1;
#elif defined(PLATFORM_WINDOWS)
	SYSTEM_INFO sys_info;
	GetSystemInfo (&sys_info);
	return sys_info.dwNumberOfProcessors;
#else
	return 1;
#endif
}
