/*
 * Copyright (C) 2016 Robin Gareus <robin@gareus.org>
 * Copyright (C) 2006 Paul Davis
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

#include <algorithm>

#include "pbd/xml++.h"

#include "ardour/audioengine.h"
#include "ardour/buffer.h"
#include "ardour/buffer_set.h"
#include "ardour/io.h"
#include "ardour/session.h"
#include "ardour/sidechain.h"

#include "pbd/i18n.h"

using namespace ARDOUR;
using namespace PBD;


SideChain::SideChain (Session& s, const std::string& name)
	: IOProcessor (s, true, false, name)
{
}

SideChain::~SideChain ()
{
	disconnect ();
}

XMLNode&
SideChain::state (bool full)
{
	XMLNode& node = IOProcessor::state (full);
	node.set_property ("type", "sidechain");
	return node;
}


int
SideChain::set_state (const XMLNode& node, int version)
{
	IOProcessor::set_state (node, version);
	return 0;
}

void
SideChain::run (BufferSet& bufs, framepos_t start_frame, framepos_t end_frame, double /*speed*/, pframes_t nframes, bool)
{
	if (_input->n_ports () == ChanCount::ZERO) {
		// inplace pass-through
		return;
	}

	if (!_active && !_pending_active) {
		// zero buffers
		for (DataType::iterator t = DataType::begin (); t != DataType::end (); ++t) {
			for (uint32_t out = _configured_input.get (*t); out < bufs.count ().get (*t); ++out) {
				bufs.get (*t, out).silence (nframes);
			}
		}
		return;
	}

	_input->collect_input (bufs, nframes, _configured_input);
	bufs.set_count (_configured_output);

	_active = _pending_active;
}

bool
SideChain::can_support_io_configuration (const ChanCount& in, ChanCount& out)
{
	out = in + _input->n_ports ();
	return true;
}

bool
SideChain::configure_io (ChanCount in, ChanCount out)
{
	if (out != in + _input->n_ports ()) {
		/* disabled for now - see PluginInsert::configure_io() */
		// return false;
	}
	return Processor::configure_io (in, out);
}
