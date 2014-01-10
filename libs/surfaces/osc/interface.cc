/*
 *   Copyright (C) 2009 Paul Davis 
 *
 *   This program is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation; either version 2 of the License, or
 *   (at your option) any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with this program; if not, write to the Free Software
 *   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *  
 *   */

#include "ardour/rc_configuration.h"
#include "control_protocol/control_protocol.h"
#include "osc.h"

using namespace ARDOUR;

static ControlProtocol*
new_osc_protocol (ControlProtocolDescriptor* /*descriptor*/, Session* s)
{
	OSC* osc = new OSC (*s, Config->get_osc_port());
	
	osc->set_active (true);

	return osc;
}

static void
delete_osc_protocol (ControlProtocolDescriptor* /*descriptor*/, ControlProtocol* cp)
{
	delete cp;
}

static bool
probe_osc_protocol (ControlProtocolDescriptor* /*descriptor*/)
{
	return true; // we can always do OSC
}

static ControlProtocolDescriptor osc_descriptor = {
	name : "Open Sound Control (OSC)",
	id : "uri://ardour.org/surfaces/osc:0",
	ptr : 0,
	module : 0,
	mandatory : 0,
	supports_feedback : true,
	probe : probe_osc_protocol,
	initialize : new_osc_protocol,
	destroy : delete_osc_protocol
};

extern "C" LIBCONTROLCP_API ControlProtocolDescriptor* protocol_descriptor () { return &osc_descriptor; }

