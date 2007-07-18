/*
	Copyright (C) 2006,2007 Paul Davis

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
#include <control_protocol/control_protocol.h>
#include "mackie_control_protocol.h"

#include <pbd/error.h>

#include <stdexcept>

using namespace ARDOUR;
using namespace PBD;
using namespace std;

ControlProtocol*
new_mackie_protocol (ControlProtocolDescriptor* descriptor, Session* s)
{
	if ( Config->get_mmc_port_name().substr(0,3) == "mcu" )
	{
		error << "mcu already used as mmc port" << endmsg;
	}
	else if ( Config->get_mtc_port_name().substr(0,3) == "mcu" )
	{
		error << "mcu already used as mtc port" << endmsg;
	}
	else if ( Config->get_midi_port_name().substr(0,3) == "mcu" )
	{
		error << "mcu already used as midi port" << endmsg;
	}
	else
	{
		// no one else is using the port, so try instantiate the object
		MackieControlProtocol * mcp = 0;
		try
		{
			mcp = new MackieControlProtocol (*s);
			mcp->set_active( true );
		}
		catch( exception & e )
		{
			error << "Error instantiating MackieControlProtocol: " << e.what() << endmsg;
			delete mcp;
			mcp = 0;
		}
		return mcp;
	}
	return 0;
}

void
delete_mackie_protocol (ControlProtocolDescriptor* descriptor, ControlProtocol* cp)
{
	try
	{
		delete cp;
	}
	catch ( exception & e )
	{
		cout << "Exception caught trying to destroy MackieControlProtocol: " << e.what() << endl;
	}
}

/**
	This is called on startup to check whether the lib should be loaded.

	So anything that can be changed in the UI should not be used here to
	prevent loading of the lib.
*/
bool
probe_mackie_protocol (ControlProtocolDescriptor* descriptor)
{
	return MackieControlProtocol::probe();
}

static ControlProtocolDescriptor mackie_descriptor = {
	name : "Mackie",
	id : "uri://ardour.org/surfaces/mackie:0",
	ptr : 0,
	module : 0,
	mandatory : 0,
	supports_feedback : false,
	probe : probe_mackie_protocol,
	initialize : new_mackie_protocol,
	destroy : delete_mackie_protocol
};
	

extern "C" {

ControlProtocolDescriptor* 
protocol_descriptor () {
	return &mackie_descriptor;
}

}
