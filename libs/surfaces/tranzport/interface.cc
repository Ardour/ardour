#include <control_protocol/control_protocol.h>
#include "tranzport_control_protocol.h"

using namespace ARDOUR;

ControlProtocol*
new_tranzport_protocol (ControlProtocolDescriptor* descriptor, Session* s)
{
	TranzportControlProtocol* tcp = new TranzportControlProtocol (*s);

	if (tcp->set_active (true)) {
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

bool
probe_tranzport_protocol (ControlProtocolDescriptor* descriptor)
{
	return TranzportControlProtocol::probe();
}

static ControlProtocolDescriptor tranzport_descriptor = {
	name : "Tranzport",
	id : "uri://ardour.org/surfaces/tranzport:0",
	ptr : 0,
	module : 0,
	mandatory : 0,
	probe : probe_tranzport_protocol,
	initialize : new_tranzport_protocol,
	destroy : delete_tranzport_protocol
};
	

extern "C" {
ControlProtocolDescriptor* 
protocol_descriptor () {
	return &tranzport_descriptor;
}
}

