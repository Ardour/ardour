/*
 * Copyright (C) 2013-2014 Paul Davis <paul@linuxaudiosystems.com>
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

#ifndef PBD_LOCALTIME_R
#define PBD_LOCALTIME_R
#include <time.h>

#ifdef COMPILER_MSVC

#define localtime_r( _clock, _result ) \
	( *(_result) = *localtime( (_clock) ), (_result) )

#elif defined COMPILER_MINGW

#  ifdef localtime_r
#  undef localtime_r
#  endif

// As in 64 bit time_t is 64 bit integer, compiler breaks compilation
// everytime implicit cast from long int* to time_t* worked in
// the past (32 bit). To unblock such a cast we added the localtime below:
extern struct tm *localtime(const long int *_Time);
extern struct tm *localtime_r(const time_t *const timep, struct tm *p_tm);

#endif

#endif
