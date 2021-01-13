/*
 * Copyright (C) 2017-2019 Robin Gareus <robin@gareus.org>
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

#include "ardour/amp.h"
#include "ardour/audio_buffer.h"
#include "ardour/phase_control.h"
#include "ardour/polarity_processor.h"
#include "ardour/session.h"

#include "pbd/i18n.h"

using namespace ARDOUR;
using namespace PBD;

PolarityProcessor::PolarityProcessor (Session& s, boost::shared_ptr<PhaseControl> control)
	: Processor(s, "Polarity", Temporal::AudioTime)
	, _control (control)
{
}

bool
PolarityProcessor::can_support_io_configuration (const ChanCount& in, ChanCount& out)
{
	out = in;
	return true;
}

bool
PolarityProcessor::configure_io (ChanCount in, ChanCount out)
{
	if (out != in) { // always 1:1
		return false;
	}

	_control->resize (in.n_audio ());
	_current_gain.resize (in.n_audio (), 1.f);

	return Processor::configure_io (in, out);
}

void
PolarityProcessor::run (BufferSet& bufs, samplepos_t /*start_sample*/, samplepos_t /*end_sample*/, double /*speed*/, pframes_t nframes, bool)
{
	size_t chn = 0;
	assert (bufs.count().n_audio () <= _current_gain.size());
	if (!_active && !_pending_active) {
		/* fade all to unity */
		for (BufferSet::audio_iterator i = bufs.audio_begin(); i != bufs.audio_end(); ++i, ++chn) {
			_current_gain[chn] = Amp::apply_gain (*i, _session.nominal_sample_rate(), nframes, _current_gain[chn], 1.0);
		}
		return;
	}
	_active = _pending_active;

	for (BufferSet::audio_iterator i = bufs.audio_begin(); i != bufs.audio_end(); ++i, ++chn) {
		_current_gain[chn] = Amp::apply_gain (*i, _session.nominal_sample_rate(), nframes, _current_gain[chn], _control->inverted (chn) ? -1.f : 1.f);
	}
}

XMLNode&
PolarityProcessor::state ()
{
	XMLNode& node (Processor::state ());
	node.set_property("type", "polarity");
	return node;
}
