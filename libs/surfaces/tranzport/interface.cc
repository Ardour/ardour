#include <ardour/control_protocol.h>

#include "tranzport_control_protocol.h"

using namespace ARDOUR;

ControlProtocol*
new_tranzport_protocol (ControlProtocolDescriptor* descriptor, Session* s)
{
	TranzportControlProtocol* tcp = new TranzportControlProtocol (*s);

	if (tcp->init ()) {
		delete tcp;
		return 0;
	}

	return tcp;
	
}

void
delete_tranzport_protocol (ControlProtocolDescriptor* descriptor, ControlProtocol* cp)
{
	delete cp;
}

static ControlProtocolDescriptor tranzport_descriptor = {
	name : "Tranzport",
	id : "uri://ardour.org/surfaces/tranzport:0",
	ptr : 0,
	module : 0,
	initialize : new_tranzport_protocol,
	destroy : delete_tranzport_protocol

};
	

extern "C" {
ControlProtocolDescriptor* 
protocol_descriptor () {
	return &tranzport_descriptor;
}
}

