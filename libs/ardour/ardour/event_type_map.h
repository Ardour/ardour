/*
 * Copyright (C) 2008-2016 David Robillard <d@drobilla.net>
 * Copyright (C) 2009 Hans Baier <hansfbaier@googlemail.com>
 * Copyright (C) 2013 Paul Davis <paul@linuxaudiosystems.com>
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

#ifndef __ardour_event_type_map_h__
#define __ardour_event_type_map_h__

#include <map>
#include <string>

#include "evoral/TypeMap.h"
#include "evoral/ControlList.h"
#include "evoral/ParameterDescriptor.h"

#include "ardour/libardour_visibility.h"

namespace ARDOUR {

class URIMap;

/** This is the interface Ardour provides to Evoral about what
 * parameter and event types/ranges/names etc. to use.
 */
class LIBARDOUR_API EventTypeMap : public Evoral::TypeMap {
public:
	static EventTypeMap& instance();

	bool     type_is_midi(uint32_t type) const;
	uint8_t  parameter_midi_type(const Evoral::Parameter& param) const;

	Evoral::ParameterType midi_parameter_type(const uint8_t* buf, uint32_t len) const;

	Evoral::ControlList::InterpolationStyle interpolation_of(const Evoral::Parameter& param);

	Evoral::Parameter from_symbol(const std::string& str) const;
	std::string       to_symbol(const Evoral::Parameter& param) const;

	Evoral::ParameterDescriptor descriptor(const Evoral::Parameter& param) const;

	void set_descriptor(const Evoral::Parameter&           param,
	                    const Evoral::ParameterDescriptor& desc);

private:
	typedef std::map<Evoral::Parameter, Evoral::ParameterDescriptor> Descriptors;

	EventTypeMap(URIMap* uri_map) : _uri_map(uri_map) {}

	URIMap*     _uri_map;
	Descriptors _descriptors;

	static EventTypeMap* event_type_map;
};

} // namespace ARDOUR

#endif /* __ardour_event_type_map_h__ */

