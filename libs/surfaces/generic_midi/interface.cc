#include <ardour/control_protocol.h>

#include "generic_midi_control_protocol.h"

using namespace ARDOUR;

ControlProtocol*
new_generic_midi_protocol (ControlProtocolDescriptor* descriptor, Session* s)
{
	GenericMidiControlProtocol* gmcp =  new GenericMidiControlProtocol (*s);
	
	if (gmcp->set_active (true)) {
		delete gmcp;
		return 0;
	}

	return gmcp;
}

void
delete_generic_midi_protocol (ControlProtocolDescriptor* descriptor, ControlProtocol* cp)
{
	delete cp;
}

static ControlProtocolDescriptor generic_midi_descriptor = {
	name : "Generic MIDI",
	id : "uri://ardour.org/surfaces/generic_midi:0",
	ptr : 0,
	module : 0,
	initialize : new_generic_midi_protocol,
	destroy : delete_generic_midi_protocol
};
	

extern "C" {
ControlProtocolDescriptor* 
protocol_descriptor () {
	return &generic_midi_descriptor;
}
}

