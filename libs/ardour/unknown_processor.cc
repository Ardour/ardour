/*
 * Copyright (C) 2010-2011 Carl Hetherington <carl@carlh.net>
 * Copyright (C) 2013-2017 Robin Gareus <robin@gareus.org>
 * Copyright (C) 2016-2017 Paul Davis <paul@linuxaudiosystems.com>
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

#include "ardour/audio_buffer.h"
#include "ardour/unknown_processor.h"

#include "pbd/i18n.h"

using namespace std;
using namespace ARDOUR;

static std::string
proc_type_map (std::string const& str)
{
	/* compare to PluginInsert::set_state */
	if (str == X_("ladspa") || str == X_("Ladspa")) { /* handle old school sessions */
		return "LV1";
	} else if (str == X_("lv2")) {
		return "LV2";
	} else if (str == X_("windows-vst")) {
		return "VST2";
	} else if (str == X_("lxvst")) {
		return "VST2";
	} else if (str == X_("mac-vst")) {
		return "VST2";
	} else if (str == X_("audiounit")) {
		return "AU";
	} else if (str == X_("luaproc")) {
		return "Lua";
	} else if (str == X_("vst3")) {
		return "VST3";
	} else {
		return str;
  }
}

UnknownProcessor::UnknownProcessor (Session& s, XMLNode const & state)
	: Processor (s, "", Temporal::AudioTime)
	, _state (state)
	, have_ioconfig (false)
	, saved_input (0)
	, saved_output (0)
{
	XMLProperty const* pname = state.property (X_("name"));
	if (pname) {
		XMLProperty const* ptype = state.property (X_("type"));
		if (ptype) {
			set_name (string_compose ("%1 (%2)", pname->value (), proc_type_map (ptype->value ())));
		} else {
			set_name (pname->value ());
		}
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
UnknownProcessor::state ()
{
	return *(new XMLNode (_state));
}

bool
UnknownProcessor::can_support_io_configuration (const ChanCount &in, ChanCount & out) {
	if (have_ioconfig && in == *saved_input) {
		out = *saved_output;
		return true;
	} else if (!have_ioconfig) {
		/* pass for old sessions.
		 *
		 * session load assumes processor config succeeds.
		 * if initial configuration fails, processors downstream
		 * remain unconfigured and at least the panner will assert/segfault.
		 *
		 * This may still result in impossible setup, however
		 * Route::configure_processors_unlocked() ignores configure_io() return value
		 * in the inner loop and configures all available processors.
		 *
		 * It can still lead to segfaults IFF the track has no inputs and this is a
		 * generator (processor_max_streams will be zero).
		 */
		PBD::warning << _("Using plugin-stub with unknown i/o configuration for: ") << name() << endmsg;
#if 0
		/* No output channels are fine (or should be, there may be edge-cases with e.g sends).
		 *
		 * Discussion needed.
		 *
		 * This is the safer option (no audio output, no possible damage)
		 * and the way to go in the long run.
		 * An even better solution is to disable the route if there are missing plugins
		 * and/or impossible configurations.
		 *
		 * Currently no output channels results in awkward GUI route display and also
		 * breaks semantics in mixbus (which assumes that the route has channels required
		 * for the always present mixer-strip plugin).
		 */
		out = ChanCount ();
#else
		out = in;
#endif
		return true;
	} else {
		PBD::error << _("Using plugin-stub with mismatching i/o configuration for: ") << name() << endmsg;
		out = in;
	}
	return true;
}

void
UnknownProcessor::run (BufferSet& bufs, samplepos_t /*start_sample*/, samplepos_t /*end_sample*/, double /*speed*/, pframes_t nframes, bool)
{
	if (!have_ioconfig) {
		return;
	}
	// silence excess output buffers
	for (uint32_t i = saved_input->n_audio(); i < saved_output->n_audio(); ++i) {
		bufs.get_audio (i).silence (nframes);
	}
}
