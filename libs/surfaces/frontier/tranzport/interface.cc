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

#include "control_protocol/control_protocol.h"
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
	supports_feedback : false,
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

