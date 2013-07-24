/*
    Copyright (C) 2011 Paul Davis
    Author: Sakari Bergen

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

#include "ardour/capturing_processor.h"
#include "ardour/session.h"
#include "ardour/audioengine.h"

#include "i18n.h"

namespace ARDOUR {

CapturingProcessor::CapturingProcessor (Session & session)
	: Processor (session, X_("capture point"))
	, block_size (AudioEngine::instance()->samples_per_cycle())
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
CapturingProcessor::run (BufferSet& bufs, framepos_t, framepos_t, pframes_t nframes, bool)
{
	if (active()) {
		capture_buffers.read_from (bufs, nframes);
	}
}

bool
CapturingProcessor::configure_io (ChanCount in, ChanCount out)
{
	Processor::configure_io (in, out);
	realloc_buffers();
	return true;
}

bool
CapturingProcessor::can_support_io_configuration (const ChanCount& in, ChanCount& out) const
{
	out = in;
	return true;
}

void
CapturingProcessor::realloc_buffers()
{
	capture_buffers.ensure_buffers (_configured_input, block_size);
}

XMLNode &
CapturingProcessor::state (bool full)
{
	XMLNode& node = Processor::state (full);

	node.add_property (X_("type"), X_("capture"));
	return node;
}

} // namespace ARDOUR
