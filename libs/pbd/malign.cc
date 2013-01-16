/*
    Copyright (C) 2012 Paul Davis 

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

#include "libpbd-config.h"

#include <cstring>
#include <cerrno>

#include "pbd/malign.h"
#include "pbd/error.h"

#include "i18n.h"

using namespace PBD;

#ifdef __x86_64__
static const int CPU_CACHE_ALIGN = 64;
#else
static const int CPU_CACHE_ALIGN = 16; /* arguably 32 on most arches, but it matters less */
#endif

int cache_aligned_malloc (void** memptr, size_t size)
{
#ifndef HAVE_POSIX_MEMALIGN
	if (((*memptr) = malloc (size)) == 0) {
		fatal << string_compose (_("Memory allocation error: malloc (%1 * %2) failed (%3)"),
					 CPU_CACHE_ALIGN, size, strerror (errno)) << endmsg;
		return errno;
	} else {
		return 0;
	}
#else
        if (posix_memalign (memptr, CPU_CACHE_ALIGN, size)) {
		fatal << string_compose (_("Memory allocation error: posix_memalign (%1 * %2) failed (%3)"),
					 CPU_CACHE_ALIGN, size, strerror (errno)) << endmsg;
	}

	return 0;
#endif	
}
