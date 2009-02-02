/* This file is part of Evoral.
 * Copyright (C) 2008 Dave Robillard <http://drobilla.net>
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

#include <string>
#include "evoral/MIDIEvent.hpp"
#ifdef EVORAL_MIDI_XML
	#include <pbd/xml++.h>
#endif

using namespace std;

namespace Evoral {

#ifdef EVORAL_MIDI_XML

template<typename T>
MIDIEvent<T>::MIDIEvent(const XMLNode& event) 
  : Event<T>()
{
	string name = event.name();
	
	if (name == "ControlChange") {
		this->_buf = (uint8_t*) ::malloc(3);
		this->_owns_buf = true;
		set_type(MIDI_CMD_CONTROL);
		
		set_cc_number(atoi(event.property("Control")->value().c_str()));
		set_cc_value (atoi(event.property("Value")->value().c_str()));
	} else if (name == "ProgramChange") {
		this->_buf = (uint8_t*) ::malloc(2);
		this->_owns_buf = true;
		set_type(MIDI_CMD_PGM_CHANGE);

		set_pgm_number(atoi(event.property("Number")->value().c_str()));
	}
}


template<typename T>
boost::shared_ptr<XMLNode> 
MIDIEvent<T>::to_xml() const
{
	XMLNode *result = 0;
	
	switch (type()) {
	case MIDI_CMD_CONTROL:
		result = new XMLNode("ControlChange");
		result->add_property("Channel", channel());
		result->add_property("Control", cc_number());
		result->add_property("Value",   cc_value());
		break;
			
	case MIDI_CMD_PGM_CHANGE:
		result = new XMLNode("ProgramChange");
		result->add_property("Channel", channel());
		result->add_property("Number",  pgm_number());
		break;
		
	default:
		// The implementation is continued as needed
		break;
	}
	
	return boost::shared_ptr<XMLNode>(result);
}

#endif // EVORAL_MIDI_XML

template class MIDIEvent<double>;

} // namespace Evoral

