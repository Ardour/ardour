/*
    Copyright (C) 2009 Paul Davis 
    
    This program is free software; you can redistribute it and/or modify it
    under the terms of the GNU General Public License as published by the Free
    Software Foundation; either version 2 of the License, or (at your option)
    any later version.
    
    This program is distributed in the hope that it will be useful, but WITHOUT
    ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
    FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
    for more details.
    
    You should have received a copy of the GNU General Public License along
    with this program; if not, write to the Free Software Foundation, Inc.,
    675 Mass Ave, Cambridge, MA 02139, USA.
*/

#include <cmath>
#include <algorithm>

#include "pbd/enumwriter.h"
#include "ardour/delivery.h"
#include "ardour/audio_buffer.h"
#include "ardour/buffer_set.h"
#include "ardour/configuration.h"
#include "ardour/io.h"
#include "ardour/meter.h"
#include "ardour/session.h"

using namespace std;
using namespace ARDOUR;

/* deliver to an existing IO object */

Delivery::Delivery (Session& s, IO* io, const string& name, Role r)
	: IOProcessor(s, io, name)
	, _role (r)
	, _metering (false)
	, _muted_by_self (false)
	, _muted_by_others (false)
{
}

/* deliver to a new IO object */

Delivery::Delivery (Session& s, const string& name, Role r)
	: IOProcessor(s, name)
	, _role (r)
	, _metering (false)
	, _muted_by_self (false)
	, _muted_by_others (false)
{
}

/* reconstruct from XML */

Delivery::Delivery (Session& s, const XMLNode& node)
	: IOProcessor (s, "reset")
	, _role (Role (0))
	, _metering (false)
	, _muted_by_self (false)
	, _muted_by_others (false)
{
	if (set_state (node)) {
		throw failed_constructor ();
	}
}


bool
Delivery::visible () const
{
	if (_role & (Main|Solo)) {
		return false;
	}

	return true;
}

bool
Delivery::can_support_io_configuration (const ChanCount& in, ChanCount& out) const
{
	out = in;
	return true;
}

bool
Delivery::configure_io (ChanCount in, ChanCount out)
{
	if (out != in) { // always 1:1
		return false;
	}
	
	return Processor::configure_io (in, out);
}

void
Delivery::run_in_place (BufferSet& bufs, sframes_t start_frame, sframes_t end_frame, nframes_t nframes)
{
	if (_io->n_outputs().get (_io->default_type()) == 0) {
		return;
	}

	if (!active() || _muted_by_self || _muted_by_others) {
		silence (nframes);
		if (_metering) {
			_io->peak_meter().reset();
		}
	} else {

		// we have to copy the input, because IO::deliver_output may alter the buffers
		// in-place, which a send must never do.

		BufferSet& sendbufs = _session.get_mix_buffers (bufs.count());

		sendbufs.read_from(bufs, nframes);
		assert(sendbufs.count() == bufs.count());

		_io->deliver_output (sendbufs, start_frame, end_frame, nframes);
		
		if (_metering) {
			if (_io->effective_gain() == 0) {
				_io->peak_meter().reset();
			} else {
				_io->peak_meter().run_in_place(_io->output_buffers(), start_frame, end_frame, nframes);
			}
		}
	}
}

void
Delivery::set_metering (bool yn)
{
	_metering = yn;
	
	if (!_metering) {
		/* XXX possible thread hazard here */
		_io->peak_meter().reset();
	}
}
void
Delivery::set_self_mute (bool yn)
{
	if (yn != _muted_by_self) {
		_muted_by_self = yn;
		SelfMuteChange (); // emit signal
	}
}

void
Delivery::set_nonself_mute (bool yn)
{
	if (yn != _muted_by_others) {
		_muted_by_others = yn;
		OtherMuteChange (); // emit signal
	}
}

XMLNode&
Delivery::state (bool full_state)
{
	XMLNode& node (IOProcessor::state (full_state));

	if (_role & Main) {
		node.add_property("type", "main-outs");
	} else if (_role & Listen) {
		node.add_property("type", "listen");
	} else {
		node.add_property("type", "delivery");
	}

	node.add_property("metering", (_metering ? "yes" : "no"));
	node.add_property("self-muted", (_muted_by_self ? "yes" : "no"));
	node.add_property("other-muted", (_muted_by_others ? "yes" : "no"));
	node.add_property("role", enum_2_string(_role));

	return node;
}

int
Delivery::set_state (const XMLNode& node)
{
	const XMLProperty* prop;
	
	if ((prop = node.property ("role")) != 0) {
		_role = Role (string_2_enum (prop->value(), _role));
	}

	if ((prop = node.property ("metering")) != 0) {
		set_metering (prop->value() == "yes");
	}

	if ((prop = node.property ("self-muted")) != 0) {
		set_self_mute (prop->value() == "yes");
	}

	if ((prop = node.property ("other-muted")) != 0) {
		set_nonself_mute (prop->value() == "yes");
	}

	return 0;
}
