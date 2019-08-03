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
#elif defined(__APPLE__) || defined(__FreeBSD__)
#include <stddef.h>
#include <sys/types.h>
#include <sys/sysctl.h>
#elif defined(PLATFORM_WINDOWS)
#include <windows.h>
#endif

#include "pbd/cpus.h"

#if defined(COMPILER_MSVC) && !defined(PTW32_VERSION)
#include <ardourext/pthread.h>  // Gets us 'PTW32_VERSION'
#endif

uint32_t
hardware_concurrency()
{
	if (getenv("ARDOUR_CONCURRENCY")) {
		int c = atoi (getenv("ARDOUR_CONCURRENCY"));
		if (c > 0) {
			return c;
		}
	}
#if defined(PTW32_VERSION) || defined(__hpux)
	return pthread_num_processors_np();
#elif defined(__APPLE__)
	int count;
	size_t size=sizeof(count);
# ifdef MIXBUS
	return sysctlbyname("hw.logicalcpu",&count,&size,NULL,0)?0:count;
# else
	return sysctlbyname("hw.physicalcpu",&count,&size,NULL,0)?0:count;
# endif
#elif defined(__FreeBSD__)
	int count;
	size_t size=sizeof(count);
	return sysctlbyname("hw.ncpu",&count,&size,NULL,0)?0:count;
#elif defined(HAVE_UNISTD) && defined(_SC_NPROCESSORS_ONLN)
	int const count=sysconf(_SC_NPROCESSORS_ONLN);
	return (count>0)?count:0;
#elif defined(PLATFORM_WINDOWS)
	SYSTEM_INFO sys_info;
	GetSystemInfo( &sys_info );
	return sys_info.dwNumberOfProcessors;
#else
	return 0;
#endif
}
