/*
    Copyright (C) 2009-2013 Paul Davis
    Author: Johannes Mueller

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

#include "pbd/failed_constructor.h"
#include "pbd/error.h"

#include "ardour/session.h"
#include "control_protocol/control_protocol.h"

#include "shuttlepro.h"

using namespace ARDOUR;
using namespace PBD;
using namespace ArdourSurface;

static ControlProtocol*
new_shuttlepro_protocol (ControlProtocolDescriptor*, Session* s)
{
	ShuttleproControlProtocol* wmcp = new ShuttleproControlProtocol (*s);
	wmcp->set_active (true);
	return wmcp;
}

static void
delete_shuttlepro_protocol (ControlProtocolDescriptor* /*descriptor*/, ControlProtocol* cp)
{
	delete cp;
}

static bool
probe_shuttlepro_protocol (ControlProtocolDescriptor*)
{
	return ShuttleproControlProtocol::probe ();
}

static ControlProtocolDescriptor shuttlepro_descriptor = {
	name : "Shuttlepro",
	id : "uri://ardour.org/surfaces/shuttlepro:0",
	ptr : 0,
	module : 0,
	mandatory : 0,
	supports_feedback : false,
	probe : probe_shuttlepro_protocol,
	initialize : new_shuttlepro_protocol,
	destroy : delete_shuttlepro_protocol
};

extern "C" ARDOURSURFACE_API ControlProtocolDescriptor* protocol_descriptor () { return &shuttlepro_descriptor; }
