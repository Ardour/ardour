/*
    Copyright (C) 2013 Tim Mayberry 

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

#include "pbd/ffs.h"

#ifndef COMPILER_MSVC
#include <strings.h>
#endif

namespace PBD {
int
ffs (int x)
{
#if defined (COMPILER_MINGW)
	return __builtin_ffs(x);
#elif defined (COMPILER_MSVC)
	unsigned long index;
#ifdef WIN64	
	if (0 != _BitScanForward64(&index, (__int64)x))
#else
	if (0 != _BitScanForward(&index, (unsigned long)x))
#endif
		index++;    // Make the result 1-based
	else
		index = 0;  // All bits were zero

	return (int)index;
#else
	return ::ffs(x);
#endif
}

}
