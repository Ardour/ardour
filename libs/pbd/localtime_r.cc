/*
 * Copyright (C) 2013 John Emmas <john@creativepost.co.uk>
 * Copyright (C) 2013 Paul Davis <paul@linuxaudiosystems.com>
 * Copyright (C) 2014 Robin Gareus <robin@gareus.org>
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

#ifndef HAVE_LOCALTIME_R
#include <time.h>
#include <string.h>

#include "pbd/pthread_utils.h"
#include "pbd/localtime_r.h"

#ifdef localtime_r
#undef localtime_r
#endif

struct tm *
localtime_r(const time_t *const timep, struct tm *p_tm)
{
	static pthread_mutex_t time_mutex;
	static int time_mutex_inited = 0;
	struct tm *tmp;

	if (!time_mutex_inited)
	{
		time_mutex_inited = 1;
		pthread_mutex_init(&time_mutex, NULL);
	}

	pthread_mutex_lock(&time_mutex);
	tmp = localtime(timep);
	if (tmp)
	{
		memcpy(p_tm, tmp, sizeof(struct tm));
		tmp = p_tm;
	}
	pthread_mutex_unlock(&time_mutex);

	return tmp;
}

#endif

#ifdef __MINGW64__
struct tm *
__cdecl localtime(const long int *_Time)
{
	if (_Time == NULL)
	{
		return localtime((const time_t *const)NULL); // Unpredictable behavior in case of _Time == NULL;
	}
	else
	{
		const time_t tempTime = *_Time;
		return localtime(&tempTime);
	}
}
#endif
