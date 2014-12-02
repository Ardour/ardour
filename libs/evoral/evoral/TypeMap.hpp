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

#ifndef EVORAL_TYPE_MAP_HPP
#define EVORAL_TYPE_MAP_HPP

#include <stdint.h>

#include <string>

#include "evoral/visibility.h"

namespace Evoral {

class Parameter;
class ParameterDescriptor;

/** The applications passes one of these which provide the implementation
 * with required information about event types in an opaque, type neutral way
 */
class /*LIBEVORAL_API*/ TypeMap {
public:
	virtual ~TypeMap() {}

	/** Return true iff the type is a MIDI event.
	 * The contents of the event will be used for specific ID
	 */
	virtual bool type_is_midi(uint32_t type) const = 0;

	/** Return the MIDI type (ie status byte with channel 0) for a
	 * parameter, or 0 if parameter can not be expressed as a MIDI event
	 */
	virtual uint8_t parameter_midi_type(const Parameter& param) const = 0;

	/** The type ID for a MIDI event with the given status byte
	 */
	virtual uint32_t midi_event_type(uint8_t status) const = 0;

	/** Return the description of a parameter. */
	virtual ParameterDescriptor descriptor(const Parameter& param) const = 0;

	virtual std::string to_symbol(const Parameter& param) const = 0;
};

} // namespace Evoral

#endif // EVORAL_TYPE_MAP_HPP
