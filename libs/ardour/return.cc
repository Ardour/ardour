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
#include "ardour/audioengine.h"
#include "ardour/buffer_set.h"
#include "ardour/io.h"
#include "ardour/meter.h"
#include "ardour/return.h"
#include "ardour/session.h"

#include "i18n.h"

using namespace ARDOUR;
using namespace PBD;

std::string
Return::name_and_id_new_return (Session& s, uint32_t& bitslot)
{
	bitslot = s.next_return_id();
	return string_compose (_("return %1"), bitslot + 1);
}


Return::Return (Session& s, bool internal)
	: IOProcessor (s, (internal ? false : true), false,
		       name_and_id_new_return (s, _bitslot))
	, _metering (false)
{
	/* never muted */

	_amp.reset (new Amp (_session));
	_meter.reset (new PeakMeter (_session, name()));
}

Return::~Return ()
{
        _session.unmark_return_id (_bitslot);
}

XMLNode&
Return::get_state(void)
{
	return state (true);
}

XMLNode&
Return::state(bool full)
{
	XMLNode& node = IOProcessor::state(full);
	char buf[32];
	node.add_property ("type", "return");
	snprintf (buf, sizeof (buf), "%" PRIu32, _bitslot);
	node.add_property ("bitslot", buf);

	return node;
}

int
Return::set_state (const XMLNode& node, int version)
{
	XMLNodeList nlist = node.children();
	XMLNodeIterator niter;
	const XMLProperty* prop;
	const XMLNode* insert_node = &node;

	/* Return has regular IO automation (gain, pan) */

	for (niter = nlist.begin(); niter != nlist.end(); ++niter) {
		if ((*niter)->name() == "IOProcessor") {
			insert_node = *niter;
		} else if ((*niter)->name() == X_("Automation")) {
			// _io->set_automation_state (*(*niter), Evoral::Parameter(GainAutomation));
		}
	}

	IOProcessor::set_state (*insert_node, version);

	if (!node.property ("ignore-bitslot")) {
		if ((prop = node.property ("bitslot")) == 0) {
			_bitslot = _session.next_return_id();
		} else {
			_session.unmark_return_id (_bitslot);
			sscanf (prop->value().c_str(), "%" PRIu32, &_bitslot);
			_session.mark_return_id (_bitslot);
		}
	}

	return 0;
}

void
Return::run (BufferSet& bufs, framepos_t start_frame, framepos_t end_frame, pframes_t nframes, bool)
{
	if ((!_active && !_pending_active) || _input->n_ports() == ChanCount::ZERO) {
		return;
	}

	_input->collect_input (bufs, nframes, _configured_input);
	bufs.set_count(_configured_output);

	// Can't automate gain for sends or returns yet because we need different buffers
	// so that we don't overwrite the main automation data for the route amp
	// _amp->setup_gain_automation (start_frame, end_frame, nframes);
	_amp->run (bufs, start_frame, end_frame, nframes, true);

	if (_metering) {
		if (_amp->gain_control()->get_value() == 0) {
			_meter->reset();
		} else {
			_meter->run (bufs, start_frame, end_frame, nframes, true);
		}
	}

	_active = _pending_active;
}

bool
Return::can_support_io_configuration (const ChanCount& in, ChanCount& out) const
{
	out = in + _input->n_ports();
	return true;
}

bool
Return::configure_io (ChanCount in, ChanCount out)
{
	if (out != in + _input->n_ports()) {
		return false;
	}

	// Ensure there are enough buffers (since we add some)
	if (_session.get_scratch_buffers(in).count() < out) {
		Glib::Threads::Mutex::Lock em (_session.engine().process_lock());
		IO::PortCountChanged(out);
	}

	Processor::configure_io(in, out);

	return true;
}

