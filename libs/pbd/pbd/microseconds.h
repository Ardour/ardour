/*
 * Copyright (C) 2021 Paul Davis <paul@linuxaudiosystems.com>
 * Copyright (C) 2013-2015 Tim Mayberry <mojofunk@gmail.com>
 * Copyright (C) 2014-2015 John Emmas <john@creativepost.co.uk>
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

#ifndef __libpbd_microseconds_h__
#define __libpbd_microseconds_h__

#include <inttypes.h>

#include "pbd/libpbd_visibility.h"

namespace PBD {
	typedef int64_t microseconds_t;
	LIBPBD_API microseconds_t get_microseconds();
	void microsecond_timer_init ();
}

#endif /* __libpbd_microseconds_h__ */
