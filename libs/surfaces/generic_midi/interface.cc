/*
 * Copyright (C) 2006-2009 David Robillard <d@drobilla.net>
 * Copyright (C) 2006-2015 Paul Davis <paul@linuxaudiosystems.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#include <pbd/failed_constructor.h>

#include "control_protocol/control_protocol.h"
#include "generic_midi_control_protocol.h"

using namespace ARDOUR;

static ControlProtocol*
new_generic_midi_protocol (ControlProtocolDescriptor* /*descriptor*/, Session* s)
{
	GenericMidiControlProtocol* gmcp;

	try {
		gmcp =  new GenericMidiControlProtocol (*s);
	} catch (failed_constructor& err) {
		return 0;
	}

	if (gmcp->set_active (true)) {
		delete gmcp;
		return 0;
	}

	return gmcp;
}

static void
delete_generic_midi_protocol (ControlProtocolDescriptor* /*descriptor*/, ControlProtocol* cp)
{
	delete cp;
}

static bool
probe_generic_midi_protocol (ControlProtocolDescriptor* /*descriptor*/)
{
	return GenericMidiControlProtocol::probe ();
}

// Field names commented out by JE - 06-01-2010
static ControlProtocolDescriptor generic_midi_descriptor = {
	/*name :              */   "Generic MIDI",
	/*id :                */   "uri://ardour.org/surfaces/generic_midi:0",
	/*ptr :               */   0,
	/*module :            */   0,
	/*mandatory :         */   0,
	/*supports_feedback : */   true,
	/*probe :             */   probe_generic_midi_protocol,
	/*initialize :        */   new_generic_midi_protocol,
	/*destroy :           */   delete_generic_midi_protocol,
	/*request_buffer_factory : */ 0  /* no buffer factory because this runs
	                                  * in the midiUI event loop (which has
	                                  * its own request buffer factory.
	                                  */
};

extern "C" ARDOURSURFACE_API ControlProtocolDescriptor* protocol_descriptor () { return &generic_midi_descriptor; }

