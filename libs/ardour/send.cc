/*
    Copyright (C) 2000 Paul Davis 

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

#include <algorithm>

#include "pbd/xml++.h"

#include "ardour/amp.h"
#include "ardour/send.h"
#include "ardour/session.h"
#include "ardour/port.h"
#include "ardour/audio_port.h"
#include "ardour/buffer_set.h"
#include "ardour/meter.h"
#include "ardour/panner.h"
#include "ardour/io.h"

#include "i18n.h"

using namespace ARDOUR;
using namespace PBD;

Send::Send (Session& s, boost::shared_ptr<MuteMaster> mm, bool internal)
	: Delivery (s, mm, string_compose (_("send %1"), (_bitslot = s.next_send_id()) + 1), 
		    (internal ? Delivery::Listen : Delivery::Send))
	, _metering (false)
{
	_amp.reset (new Amp (_session, _mute_master));
	_meter.reset (new PeakMeter (_session));

	ProcessorCreated (this); /* EMIT SIGNAL */
}

Send::Send (Session& s, boost::shared_ptr<MuteMaster> mm, const XMLNode& node, bool internal)
        : Delivery (s, mm, "send", (internal ? Delivery::Listen : Delivery::Send))
	, _metering (false)
{
	_amp.reset (new Amp (_session, _mute_master));
	_meter.reset (new PeakMeter (_session));

	if (set_state (node)) {
		throw failed_constructor();
	}

	ProcessorCreated (this); /* EMIT SIGNAL */
}

Send::~Send ()
{
	GoingAway ();
}

void
Send::run (BufferSet& bufs, sframes_t start_frame, sframes_t end_frame, nframes_t nframes)
{
	if (!_active || _output->n_ports() == ChanCount::ZERO) {
		_meter->reset ();
		return;
	}

	// we have to copy the input, because deliver_output() may alter the buffers
	// in-place, which a send must never do.
	
	BufferSet& sendbufs = _session.get_mix_buffers (bufs.count());
	sendbufs.read_from (bufs, nframes);
	assert(sendbufs.count() == bufs.count());

	/* gain control */

	// Can't automate gain for sends or returns yet because we need different buffers
	// so that we don't overwrite the main automation data for the route amp
	// _amp->setup_gain_automation (start_frame, end_frame, nframes);
	_amp->run (sendbufs, start_frame, end_frame, nframes);

	/* deliver to outputs */

	Delivery::run (sendbufs, start_frame, end_frame, nframes);

	/* consider metering */
	
	if (_metering) {
		if (_amp->gain_control()->get_value() == 0) {
			_meter->reset();
		} else {
			_meter->run (*_output_buffers, start_frame, end_frame, nframes);
		}
	}
}

XMLNode&
Send::get_state(void)
{
	return state (true);
}

XMLNode&
Send::state(bool full)
{
	XMLNode& node = Delivery::state(full);
	char buf[32];

	node.add_property ("type", "send");
	snprintf (buf, sizeof (buf), "%" PRIu32, _bitslot);
	node.add_property ("bitslot", buf);

	return node;
}

int
Send::set_state(const XMLNode& node)
{
	XMLNodeList nlist = node.children();
	XMLNodeIterator niter;
	const XMLProperty* prop;

	if ((prop = node.property ("bitslot")) == 0) {
		_bitslot = _session.next_send_id();
	} else {
		sscanf (prop->value().c_str(), "%" PRIu32, &_bitslot);
		_session.mark_send_id (_bitslot);
	}

	const XMLNode* insert_node = &node;

	/* XXX need to load automation state & data for amp */
	
	Delivery::set_state (*insert_node);

	return 0;
}

bool
Send::can_support_io_configuration (const ChanCount& in, ChanCount& out) const
{
	if (_output->n_ports() == ChanCount::ZERO) {

		/* not configured yet, we can support anything */

		out = in;
		return true; /* we can support anything the first time we're asked */

	} else {

		/* for a send, processor input corresponds to IO output */

		out = in;
		return true;
	}

	return false;
}

/** Set up the XML description of a send so that its name is unique.
 *  @param state XML send state.
 *  @param session Session.
 */
void
Send::make_unique (XMLNode &state, Session &session)
{
	uint32_t const bitslot = session.next_send_id() + 1;

	char buf[32];
	snprintf (buf, sizeof (buf), "%" PRIu32, bitslot);
	state.property("bitslot")->set_value (buf);

	std::string const name = string_compose (_("send %1"), bitslot);
	
	state.property("name")->set_value (name);

	XMLNode* io = state.child ("IO");
	if (io) {
		io->property("name")->set_value (name);
	}
}

bool
Send::set_name (const std::string& new_name)
{
	std::string unique_name;

	if (_role != Listen) {
		char buf[32];
		snprintf (buf, sizeof (buf), "%u", _bitslot);
		unique_name = new_name;
		unique_name += buf;
	} else {
		unique_name = new_name;
	}

	return Delivery::set_name (unique_name);
}
