/* This file is part of Evoral.
 * Copyright (C) 2008 David Robillard <http://drobilla.net>
 * Copyright (C) 2000-2008 Paul Davis
 *
 * Evoral is free software; you can redistribute it and/or modify it under the
 * terms of the GNU General Public License as published by the Free Software
 * Foundation; either version 2 of the License, or (at your option) any later
 * version.
 *
 * Evoral is distributed in the hope that it will be useful, but WITHOUT ANY
 * WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE.  See the GNU General Public License for details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin St, Fifth Floor, Boston, MA 02110-1301 USA
 */

#ifndef EVORAL_TYPES_HPP
#define EVORAL_TYPES_HPP

#include <float.h>
#include <math.h>
#include <stdint.h>

#include <iostream>
#include <limits>
#include <list>

#include "pbd/debug.h"

#include "evoral/Beats.hpp"
#include "evoral/visibility.h"

#include "pbd/debug.h"

namespace Evoral {

/** ID of an event (note or other). This must be operable on by glib
    atomic ops
*/
typedef int32_t event_id_t;

/** Type of an event (opaque, mapped by application) */
typedef uint32_t EventType;

} // namespace Evoral

namespace PBD {
	namespace DEBUG {
		LIBEVORAL_API extern DebugBits Sequence;
		LIBEVORAL_API extern DebugBits Note;
		LIBEVORAL_API extern DebugBits ControlList;
	}
}

#endif // EVORAL_TYPES_HPP
