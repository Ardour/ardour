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

#include <stdint.h>
#include <list>
#include <cmath>
#include <cfloat>

#include "pbd/debug.h"

namespace Evoral {

/** ID of an event (note or other). This must be operable on by glib
    atomic ops
*/
typedef int32_t event_id_t;

/** Musical time: beats relative to some defined origin */
typedef double MusicalTime;
const MusicalTime MaxMusicalTime = DBL_MAX;
const MusicalTime MinMusicalTime = DBL_MIN;

static inline bool musical_time_equal (MusicalTime a, MusicalTime b) {
	/* acceptable tolerance is 1 tick. Nice if there was no magic number here */
	return fabs (a - b) <= (1.0/1920.0);
}

/** Type of an event (opaque, mapped by application) */
typedef uint32_t EventType;

} // namespace Evoral

namespace PBD {
	namespace DEBUG {
		extern uint64_t Sequence;
		extern uint64_t Note;
	}
}

#endif // EVORAL_TYPES_HPP
