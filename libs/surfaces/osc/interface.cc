#include <ardour/control_protocol.h>

#include "osc_server.h"

using namespace ARDOUR;

ControlProtocol*
new_osc_protocol (ControlProtocolDescriptor* descriptor, Session* s)
{
	ControlOSC* osc = new ControlOSC (*s, 3891);

	if (osc->set_active (true)) {
		delete osc;
		return 0;
	}

	return osc;
	
}

void
delete_osc_protocol (ControlProtocolDescriptor* descriptor, ControlProtocol* cp)
{
	delete cp;
}

static ControlProtocolDescriptor osc_descriptor = {
	name : "OSC",
	id : "uri://ardour.org/surfaces/osc:0",
	ptr : 0,
	module : 0,
	initialize : new_osc_protocol,
	destroy : delete_osc_protocol

};
	

extern "C" {
ControlProtocolDescriptor* 
protocol_descriptor () {
	return &osc_descriptor;
}
}

