/*
 * Copyright (C) 2011 Carl Hetherington <carl@carlh.net>
 * Copyright (C) 2013-2017 Paul Davis <paul@linuxaudiosystems.com>
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

#include "ardour/capturing_processor.h"
#include "ardour/session.h"
#include "ardour/audioengine.h"

#include "pbd/i18n.h"

namespace ARDOUR {

CapturingProcessor::CapturingProcessor (Session & session, samplecnt_t latency)
	: Processor (session, X_("capture point"), Temporal::AudioTime)
	, block_size (AudioEngine::instance()->samples_per_cycle())
	, _latency (latency)
{
	realloc_buffers ();
}

CapturingProcessor::~CapturingProcessor()
{
}

int
CapturingProcessor::set_block_size (pframes_t nframes)
{
	block_size = nframes;
	realloc_buffers();
	return 0;
}

void
CapturingProcessor::run (BufferSet& bufs, samplepos_t, samplepos_t, double, pframes_t nframes, bool)
{
	if (!active()) {
		_delaybuffers.flush ();
		return;
	}
	for (DataType::iterator t = DataType::begin(); t != DataType::end(); ++t) {
		for (uint32_t b = 0; b < bufs.count().get (*t); ++b) {
			_delaybuffers.delay (*t, b, capture_buffers.get_available (*t, b), bufs.get_available (*t, b), nframes, 0, 0);
		}
	}
}

bool
CapturingProcessor::configure_io (ChanCount in, ChanCount out)
{
	Processor::configure_io (in, out);
	_delaybuffers.set (out, _latency);
	realloc_buffers();
	return true;
}

bool
CapturingProcessor::can_support_io_configuration (const ChanCount& in, ChanCount& out)
{
	out = in;
	return true;
}

void
CapturingProcessor::realloc_buffers()
{
	capture_buffers.ensure_buffers (_configured_input, block_size);
	_delaybuffers.flush ();
}

XMLNode &
CapturingProcessor::state ()
{
	XMLNode& node = Processor::state ();

	node.set_property (X_("type"), X_("capture"));
	return node;
}

} // namespace ARDOUR
