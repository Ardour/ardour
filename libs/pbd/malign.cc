/*
 * Copyright (C) 2009-2016 Paul Davis <paul@linuxaudiosystems.com>
 * Copyright (C) 2015 Robin Gareus <robin@gareus.org>
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

#include "libpbd-config.h"

#include <cstring>
#include <cerrno>

#include "pbd/malign.h"
#include "pbd/error.h"

#include "pbd/i18n.h"

using namespace PBD;

#if ( defined(__x86_64__) || defined(_M_X64) )
static const int CPU_CACHE_ALIGN = 64;
#elif defined ARM_NEON_SUPPORT
static const int CPU_CACHE_ALIGN = 128; // sizeof(float32x4_t)
#else
static const int CPU_CACHE_ALIGN = 16; /* arguably 32 on most arches, but it matters less */
#endif

int cache_aligned_malloc (void** memptr, size_t size)
{
#ifndef HAVE_POSIX_MEMALIGN
#ifdef PLATFORM_WINDOWS
	if (((*memptr) = _aligned_malloc (size, CPU_CACHE_ALIGN)) == 0) {
		fatal << string_compose (_("Memory allocation error: malloc (%1 * %2) failed (%3)"),
					 CPU_CACHE_ALIGN, size, strerror (errno)) << endmsg;
		return errno;
	} else {
		return 0;
	}
#else
	if (((*memptr) = malloc (size)) == 0) {
		fatal << string_compose (_("Memory allocation error: malloc (%1 * %2) failed (%3)"),
					 CPU_CACHE_ALIGN, size, strerror (errno)) << endmsg;
		return errno;
	} else {
		return 0;
	}
#endif
#else
        if (posix_memalign (memptr, CPU_CACHE_ALIGN, size)) {
		fatal << string_compose (_("Memory allocation error: posix_memalign (%1 * %2) failed (%3)"),
					 CPU_CACHE_ALIGN, size, strerror (errno)) << endmsg;
	}

	return 0;
#endif
}

void cache_aligned_free (void* memptr)
{
#ifdef PLATFORM_WINDOWS
	_aligned_free (memptr);
#else
	free (memptr);
#endif
}

int  aligned_malloc (void** memptr, size_t size, size_t alignment)
{
#ifndef HAVE_POSIX_MEMALIGN
#ifdef PLATFORM_WINDOWS
	if (((*memptr) = _aligned_malloc (size, alignment)) == 0) {
		fatal << string_compose (_("Memory allocation error: malloc (%1 * %2) failed (%3)"),
					 alignment, size, strerror (errno)) << endmsg;
		return errno;
	} else {
		return 0;
	}
#else
	if (((*memptr) = malloc (size)) == 0) {
		fatal << string_compose (_("Memory allocation error: malloc (%1 * %2) failed (%3)"),
					 alignment, size, strerror (errno)) << endmsg;
		return errno;
	} else {
		return 0;
	}
#endif
#else
        if (posix_memalign (memptr, alignment, size)) {
		fatal << string_compose (_("Memory allocation error: posix_memalign (%1 * %2) failed (%3)"),
					 alignment, size, strerror (errno)) << endmsg;
	}

	return 0;
#endif
}

void aligned_free (void* memptr)
{
#ifdef PLATFORM_WINDOWS
	_aligned_free (memptr);
#else
	free (memptr);
#endif
}
