#include <ardour/control_protocol.h>

#include "tranzport_control_protocol.h"

using namespace ARDOUR;

ControlProtocol*
new_tranzport_protocol (ControlProtocolDescriptor* descriptor, Session* s)
{
	return new TranzportControlProtocol (*s);
}

void
delete_tranzport_protocol (ControlProtocolDescriptor* descriptor, ControlProtocol* cp)
{
	delete cp;
}

static ControlProtocolDescriptor tranzport_descriptor = {
	name : "Tranzport",
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

