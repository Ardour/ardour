/*
    Copyright (C) 2012 Paul Davis 

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

#include <pbd/failed_constructor.h>

#include "control_protocol/control_protocol.h"
#include "generic_midi_control_protocol.h"

using namespace ARDOUR;

ControlProtocol*
new_generic_midi_protocol (ControlProtocolDescriptor* /*descriptor*/, Session* s)
{
	GenericMidiControlProtocol* gmcp;
		
	try {
		gmcp =  new GenericMidiControlProtocol (*s);
	} catch (failed_constructor& err) {
		return 0;
	}
	
	if (gmcp->set_active (true)) {
		delete gmcp;
		return 0;
	}

	return gmcp;
}

void
delete_generic_midi_protocol (ControlProtocolDescriptor* /*descriptor*/, ControlProtocol* cp)
{
	delete cp;
}

bool
probe_generic_midi_protocol (ControlProtocolDescriptor* /*descriptor*/)
{
	return GenericMidiControlProtocol::probe ();
}

static ControlProtocolDescriptor generic_midi_descriptor = {
	.name = "Generic MIDI",
	.id = "uri://ardour.org/surfaces/generic_midi:0",
	.ptr = 0,
	.module = 0,
	.mandatory = 0,
	.supports_feedback = true,
	.probe = probe_generic_midi_protocol,
	.initialize = new_generic_midi_protocol,
	.destroy = delete_generic_midi_protocol
};


extern "C" {
ControlProtocolDescriptor* 
protocol_descriptor () {
	return &generic_midi_descriptor;
}
}

