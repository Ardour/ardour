/*
    Copyright (C) 2000-2007 Paul Davis
    Author: David Robillard

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

#ifndef __ardour_event_type_map_h__
#define __ardour_event_type_map_h__

#include <string>
#include "evoral/TypeMap.hpp"
#include "evoral/ControlList.hpp"

#include "ardour/libardour_visibility.h"

namespace ARDOUR {

/** This is the interface Ardour provides to Evoral about what
 * parameter and event types/ranges/names etc. to use.
 */
class LIBARDOUR_API EventTypeMap : public Evoral::TypeMap {
public:
	bool     type_is_midi(uint32_t type) const;
	uint8_t  parameter_midi_type(const Evoral::Parameter& param) const;
	uint32_t midi_event_type(uint8_t status) const;
	Evoral::ControlList::InterpolationStyle interpolation_of(const Evoral::Parameter& param);

	bool                 is_integer(const Evoral::Parameter& param) const;
	Evoral::Parameter    new_parameter(uint32_t type, uint8_t channel=0, uint32_t id=0) const;
	Evoral::Parameter    new_parameter(const std::string& str) const;
	std::string          to_symbol(const Evoral::Parameter& param) const;

	bool                 is_midi_parameter(const Evoral::Parameter& param);

	static EventTypeMap& instance() { return event_type_map; }

private:
	static EventTypeMap event_type_map;
};

} // namespace ARDOUR

#endif /* __ardour_event_type_map_h__ */

