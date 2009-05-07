/*
    Copyright (C) 2006 Paul Davis 
    
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
#include "ardour/control_outputs.h"
#include "ardour/audio_buffer.h"
#include "ardour/buffer_set.h"
#include "ardour/configuration.h"
#include "ardour/io.h"
#include "ardour/session.h"

using namespace std;

namespace ARDOUR {

ControlOutputs::ControlOutputs(Session& s, IO* io)
	: IOProcessor(s, io, "Control Outs")
	, _deliver(true)
{
}

bool
ControlOutputs::can_support_io_configuration (const ChanCount& in, ChanCount& out) const
{
	out = in;
	return true;
}

bool
ControlOutputs::configure_io (ChanCount in, ChanCount out)
{
	if (out != in) { // always 1:1
		return false;
	}
	
	return Processor::configure_io (in, out);
}

void
ControlOutputs::run_in_place (BufferSet& bufs, nframes_t start_frame, nframes_t end_frame, nframes_t nframes)
{
	if (_deliver) {
		_io->deliver_output (bufs, start_frame, end_frame, nframes);
	} else {
		_io->silence (nframes);
	}
}

XMLNode&
ControlOutputs::state (bool full_state)
{
	return get_state();
}

XMLNode&
ControlOutputs::get_state()
{
	XMLNode* node = new XMLNode(state_node_name);
	node->add_property("type", "control-outputs");
	return *node;
}

} // namespace ARDOUR
