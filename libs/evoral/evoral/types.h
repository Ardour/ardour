/*
 * Copyright (C) 2008-2012 Carl Hetherington <carl@carlh.net>
 * Copyright (C) 2008-2016 David Robillard <d@drobilla.net>
 * Copyright (C) 2009-2015 Paul Davis <paul@linuxaudiosystems.com>
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

#ifndef EVORAL_TYPES_HPP
#define EVORAL_TYPES_HPP

#include <float.h>
#include <math.h>
#include <stdint.h>

#include <iostream>
#include <limits>
#include <list>

#include "evoral/visibility.h"
#include "pbd/debug.h"

namespace Evoral {

/** ID of an event (note or other). This must be operable on by glib
    atomic ops
*/
typedef int32_t event_id_t;

/** Type of an event (opaque, mapped by application, e.g. MIDI).
 *
 * Event types are really an arbitrary integer provided by the type map, and it
 * is safe to use values not in this enum, but this enum exists so the compiler
 * can catch mistakes like setting the event type to a MIDI status byte.  Event
 * types come from the type map and describe a format/protocol like MIDI, and
 * must not be confused with the payload (such as a note on or CC change).
 * There is a static value for MIDI as this type is handled specially by
 * various parts of Evoral.
 */
enum EventType {
	NO_EVENT,
	MIDI_EVENT,
	LIVE_MIDI_EVENT,
#if defined(__arm__) || defined(__aarch64__)
	_Force32BitAlignment = 0xffffffff
#endif
};

/** Type of a parameter (opaque, mapped by application, e.g. gain) */
typedef uint32_t ParameterType;

} // namespace Evoral

namespace PBD {
	namespace DEBUG {
		LIBEVORAL_API extern DebugBits Sequence;
		LIBEVORAL_API extern DebugBits Note;
		LIBEVORAL_API extern DebugBits ControlList;
	}
}

#endif // EVORAL_TYPES_HPP
