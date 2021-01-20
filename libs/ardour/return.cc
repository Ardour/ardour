/*
 * Copyright (C) 2009-2011 Carl Hetherington <carl@carlh.net>
 * Copyright (C) 2009-2012 David Robillard <d@drobilla.net>
 * Copyright (C) 2009-2017 Paul Davis <paul@linuxaudiosystems.com>
 * Copyright (C) 2013-2017 Robin Gareus <robin@gareus.org>
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

#include <algorithm>

#include "pbd/xml++.h"

#include "ardour/amp.h"
#include "ardour/audioengine.h"
#include "ardour/buffer_set.h"
#include "ardour/gain_control.h"
#include "ardour/io.h"
#include "ardour/meter.h"
#include "ardour/return.h"
#include "ardour/session.h"

#include "pbd/i18n.h"

using namespace ARDOUR;
using namespace PBD;

std::string
Return::name_and_id_new_return (Session& s, uint32_t& bitslot)
{
	bitslot = s.next_return_id();
	return string_compose (_("return %1"), bitslot + 1);
}


Return::Return (Session& s, bool internal)
	: IOProcessor (s, (internal ? false : true), false, name_and_id_new_return (s, _bitslot), "", DataType::AUDIO, true)
	, _metering (false)
{
	/* never muted */

	boost::shared_ptr<AutomationList> gl (new AutomationList (Evoral::Parameter (GainAutomation), time_domain()));
	_gain_control = boost::shared_ptr<GainControl> (new GainControl (_session, Evoral::Parameter (GainAutomation), gl));
	add_control (_gain_control);

	_amp.reset (new Amp (_session, X_("Fader"), _gain_control, true));
	_meter.reset (new PeakMeter (_session, name()));
}

Return::~Return ()
{
        _session.unmark_return_id (_bitslot);
}

XMLNode&
Return::state()
{
	XMLNode& node = IOProcessor::state ();
	node.set_property ("type", "return");
	node.set_property ("bitslot", _bitslot);

	return node;
}

int
Return::set_state (const XMLNode& node, int version)
{
	XMLNodeList nlist = node.children();
	XMLNodeIterator niter;
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
		uint32_t bitslot;
		if (node.get_property ("bitslot", bitslot)) {
			_session.unmark_return_id (_bitslot);
			_bitslot = bitslot;
			_session.mark_return_id (_bitslot);
		} else {
			_bitslot = _session.next_return_id();
		}
	}

	return 0;
}

void
Return::run (BufferSet& bufs, samplepos_t start_sample, samplepos_t end_sample, double speed, pframes_t nframes, bool)
{
	if ((!_active && !_pending_active) || _input->n_ports() == ChanCount::ZERO) {
		return;
	}

	_input->collect_input (bufs, nframes, _configured_input);
	bufs.set_count(_configured_output);

	// Can't automate gain for sends or returns yet because we need different buffers
	// so that we don't overwrite the main automation data for the route amp
	// _amp->setup_gain_automation (start_sample, end_sample, nframes);
	_amp->run (bufs, start_sample, end_sample, speed, nframes, true);

	if (_metering) {
		if (_amp->gain_control()->get_value() == 0) {
			_meter->reset();
		} else {
			_meter->run (bufs, start_sample, end_sample, speed, nframes, true);
		}
	}

	_active = _pending_active;
}

bool
Return::can_support_io_configuration (const ChanCount& in, ChanCount& out)
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
