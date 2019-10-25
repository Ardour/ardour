/*
 * Copyright (C) 2016 David Robillard <d@drobilla.net>
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

#ifndef EVORAL_MIDI_XML_HPP
#define EVORAL_MIDI_XML_HPP

#include "evoral/Event.h"
#include "pbd/xml++.h"

namespace Evoral {
namespace MIDIXML {

template<typename Time>
bool
xml_to_midi(const XMLNode& node, Evoral::Event<Time>& ev)
{
	if (node.name() == "ControlChange") {
		ev.set_type(MIDI_CMD_CONTROL);
		ev.set_cc_number(atoi(node.property("Control")->value().c_str()));
		ev.set_cc_value(atoi(node.property("Value")->value().c_str()));
		return true;
	} else if (node.name() == "ProgramChange") {
		ev.set_type(MIDI_CMD_PGM_CHANGE);
		ev.set_pgm_number(atoi(node.property("Number")->value().c_str()));
		return true;
	}

	return false;
}

template<typename Time>
boost::shared_ptr<XMLNode>
midi_to_xml(const Evoral::Event<Time>& ev)
{
	XMLNode* result = 0;

	switch (ev.type()) {
	case MIDI_CMD_CONTROL:
		result = new XMLNode("ControlChange");
		result->add_property("Channel", long(ev.channel()));
		result->add_property("Control", long(ev.cc_number()));
		result->add_property("Value",   long(ev.cc_value()));
		break;

	case MIDI_CMD_PGM_CHANGE:
		result = new XMLNode("ProgramChange");
		result->add_property("Channel", long(ev.channel()));
		result->add_property("Number",  long(ev.pgm_number()));
		break;

	case MIDI_CMD_NOTE_ON:
		result = new XMLNode("NoteOn");
		result->add_property("Channel",  long(ev.channel()));
		result->add_property("Note",     long(ev.note()));
		result->add_property("Velocity", long(ev.velocity()));
		break;

	case MIDI_CMD_NOTE_OFF:
		result = new XMLNode("NoteOff");
		result->add_property("Channel",  long(ev.channel()));
		result->add_property("Note",     long(ev.note()));
		result->add_property("Velocity", long(ev.velocity()));
		break;

	case MIDI_CMD_BENDER:
		result = new XMLNode("PitchBendChange");
		result->add_property("Channel", long(ev.channel()));
		result->add_property("Value",   long(ev.pitch_bender_value()));
		break;

	default:
		return boost::shared_ptr<XMLNode>();
	}

	return boost::shared_ptr<XMLNode>(result);
}

} // namespace MIDIXML
} // namespace Evoral

#endif // EVORAL_MIDI_XML_HPP
