/*
    Copyright (C) 2010 Paul Davis

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

#include "ardour/audio_buffer.h"
#include "ardour/unknown_processor.h"

#include "i18n.h"

using namespace std;
using namespace ARDOUR;

UnknownProcessor::UnknownProcessor (Session& s, XMLNode const & state)
	: Processor (s, "")
	, _state (state)
	, have_ioconfig (false)
	, saved_input (0)
	, saved_output (0)
{
	XMLProperty const * prop = state.property (X_("name"));
	if (prop) {
		set_name (prop->value ());
		_display_to_user = true;
	}

	int have_io = 0;
	XMLNodeList kids = state.children ();
	for (XMLNodeIterator i = kids.begin(); i != kids.end(); ++i) {
		if ((*i)->name() == X_("ConfiguredInput")) {
			have_io |= 1;
			saved_input = new ChanCount(**i);
		}
		if ((*i)->name() == X_("ConfiguredOutput")) {
			have_io |= 2;
			saved_output = new ChanCount(**i);
		}
	}
	have_ioconfig = (have_io == 3);
}

UnknownProcessor::~UnknownProcessor () {
	delete saved_input;;
	delete saved_output;
}

XMLNode &
UnknownProcessor::state (bool)
{
	return *(new XMLNode (_state));
}

bool
UnknownProcessor::can_support_io_configuration (const ChanCount &in, ChanCount & out) {
	if (have_ioconfig && in == *saved_input) {
		out = *saved_output;
		return true;
	}
	return false;
}

void
UnknownProcessor::run (BufferSet& bufs, framepos_t /*start_frame*/, framepos_t /*end_frame*/, pframes_t nframes, bool)
{
	if (!have_ioconfig) {
		return;
	}
	// silence excess output buffers
	for (uint32_t i = saved_input->n_audio(); i < saved_output->n_audio(); ++i) {
		bufs.get_audio (i).silence (nframes);
	}
}
