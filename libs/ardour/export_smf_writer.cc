/*
 * Copyright (C) 2022 Robin Gareus <robin@gareus.org>
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

#include <glib/gstdio.h>

#include "ardour/export_smf_writer.h"
#include "ardour/midi_buffer.h"
#include "temporal/tempo.h"

using namespace ARDOUR;
using namespace Evoral;

ExportSMFWriter::ExportSMFWriter ()
	: _pos (0)
	, _last_ev_time_samples (0)
{
}

ExportSMFWriter::~ExportSMFWriter ()
{
	if (!_path.empty ()) {
		end_write (_path);
		SMF::close ();
	}
}

int
ExportSMFWriter::init (std::string const& path, samplepos_t timespan_start)
{
	::g_unlink (path.c_str ());
	if (SMF::create (path)) {
		return -1;
	}
	_path                 = path;
	_pos                  = 0;
	_last_ev_time_samples = 0;
	_timespan_start       = timespan_start;
	_tracker.reset ();
	SMF::begin_write ();
	return 0;
}

void
ExportSMFWriter::process (MidiBuffer const& buf, sampleoffset_t off, samplecnt_t n_samples, bool last_cycle)
{
	if (_path.empty ()) {
		return;
	}
	for (MidiBuffer::const_iterator i = buf.begin (); i != buf.end (); ++i) {
		Evoral::Event<samplepos_t> ev (*i, false);
		if (ev.time () < off) {
			continue;
		}

		samplepos_t pos = _pos + ev.time () - off;
		assert (pos >= _last_ev_time_samples);

		const timepos_t       t1 (pos + _timespan_start);
		const timepos_t       t0 (_last_ev_time_samples + _timespan_start);
		const Temporal::Beats delta_time_beats = t1.beats () - t0.beats ();
		const uint32_t        delta_time_ticks = delta_time_beats.to_ticks (ppqn ());
		//std::cout << "Timespan off: " << _timespan_start << " export pos: " << _pos << " event at" << pos << " beat: " << delta_time_beats << " ticks: " << delta_time_ticks << "\n";

		_tracker.track (ev.buffer ());

		SMF::append_event_delta (delta_time_ticks, ev.size (), ev.buffer (), 0);
		_last_ev_time_samples = pos;
	}

	if (last_cycle) {
		MidiBuffer mb (8192);
		_tracker.resolve_notes (mb, n_samples);
		process (mb, 0, n_samples, false);
		end_write (_path);
		SMF::close ();
		_path.clear ();
	} else {
		_pos += n_samples;
	}
}
