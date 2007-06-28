/*
	Ardour9pin interface file
	Ben Loftis
	Created: 05/18/06 11:07:56
	Copyright Harrison Audio, LLC, 2007
*/

#include "powermate.h"

using namespace ARDOUR;

ControlProtocol*
new_powermate_protocol (ControlProtocolDescriptor* descriptor, Session* s)
{
	PowermateControlProtocol* pcp = new PowermateControlProtocol (*s);

	if (pcp->set_active (true)) {
		delete pcp;
		return 0;
	}

	return pcp;
	
}

void
delete_powermate_protocol (ControlProtocolDescriptor* descriptor, ControlProtocol* cp)
{
	delete cp;
}

bool
probe_powermate_protocol (ControlProtocolDescriptor* descriptor)
{
	return PowermateControlProtocol::probe ();
}

static ControlProtocolDescriptor powermate_descriptor = {
	name : "powermate",
	id : "uri://ardour.org/ardour/powermate:0",
	ptr : 0,
	module : 0,
	mandatory : 0,
	supports_feedback : false,
	probe : probe_powermate_protocol,
	initialize : new_powermate_protocol,
	destroy : delete_powermate_protocol
};
	

extern "C" {
ControlProtocolDescriptor* 
protocol_descriptor () {
	return &powermate_descriptor;
}
}

