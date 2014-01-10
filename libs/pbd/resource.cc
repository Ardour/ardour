/*
    Copyright (C) 2011 Tim Mayberry

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

#ifdef PLATFORM_WINDOWS
#include <stdio.h>
#else
#include <sys/time.h>
#include <sys/resource.h>
#endif

#include "pbd/resource.h"

namespace PBD {

bool
get_resource_limit (ResourceType resource, ResourceLimit& limit)
{
	if (resource == OpenFiles)
	{
#ifdef PLATFORM_WINDOWS
		limit.current_limit = _getmaxstdio();
		limit.max_limit = 2048;
		return true;
#else
		struct rlimit rl;
		if (getrlimit (RLIMIT_NOFILE, &rl) == 0) {
			limit.current_limit = rl.rlim_cur;
			limit.max_limit = rl.rlim_max;
			return true;
		}
#endif
	}

	return false;
}

bool
set_resource_limit (ResourceType resource, const ResourceLimit& limit)
{
	if (resource == OpenFiles)
	{
#ifdef PLATFORM_WINDOWS
		// no soft and hard limits on windows
		rlimit_t new_max = _setmaxstdio(limit.current_limit);

		if (new_max == limit.current_limit) return true;
#else
		struct rlimit rl;
		rl.rlim_cur = limit.current_limit;
		rl.rlim_max = limit.max_limit;
		if (setrlimit (RLIMIT_NOFILE, &rl) == 0) {
			return true;
		}

#endif
	}

	return false;
}

} // namespace PBD
