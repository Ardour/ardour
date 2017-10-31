/*
 * Copyright (C) 2017 Robin Gareus <robin@gareus.org>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */

#include "ardour/audio_buffer.h"
#include "ardour/phase_control.h"
#include "ardour/polarity_processor.h"

#include "pbd/i18n.h"

using namespace ARDOUR;
using namespace PBD;

PolarityProcessor::PolarityProcessor (Session& s, boost::shared_ptr<PhaseControl> control)
	: Processor(s, "Polarity")
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

	return Processor::configure_io (in, out);
}

void
PolarityProcessor::run (BufferSet& bufs, samplepos_t /*start_sample*/, samplepos_t /*end_sample*/, double /*speed*/, pframes_t nframes, bool)
{
	if (!_active && !_pending_active) {
		return;
	}
	_active = _pending_active;

	if (_control->none()) {
		return;
	}
	int chn = 0;

	for (BufferSet::audio_iterator i = bufs.audio_begin(); i != bufs.audio_end(); ++i, ++chn) {
		Sample* const sp = i->data();
		if (_control->inverted (chn)) {
			for (pframes_t nx = 0; nx < nframes; ++nx) {
				sp[nx] = -sp[nx];
			}
		}
	}
}

XMLNode&
PolarityProcessor::state ()
{
	XMLNode& node (Processor::state ());
	node.set_property("type", "polarity");
	return node;
}
