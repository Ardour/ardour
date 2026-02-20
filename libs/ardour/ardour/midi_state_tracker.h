/*
 * Copyright (C) 2008-2015 David Robillard <d@drobilla.net>
 * Copyright (C) 2009-2017 Paul Davis <paul@linuxaudiosystems.com>
 * Copyright (C) 2010-2011 Carl Hetherington <carl@carlh.net>
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

#pragma once

#include <glibmm/threads.h>

#include "temporal/beats.h"

#include "ardour/debug.h"
#include "ardour/midi_buffer.h"
#include "ardour/source.h"

namespace Evoral {
template <typename T> class EventSink;
template <typename T> class EventList;
}

namespace ARDOUR {

class MidiSource;

/** Tracks played notes, so they can be resolved in potential stuck note
 * situations (e.g. looping, transport stop, etc).
 */
class LIBARDOUR_API MidiNoteTracker
{
public:
	MidiNoteTracker();
	virtual ~MidiNoteTracker() {}

	virtual void track (const uint8_t* evbuf);
	virtual void dump (std::ostream&) const;
	virtual void reset ();

	void track (const MidiBuffer::const_iterator& from, const MidiBuffer::const_iterator& to);
	void resolve_notes (MidiBuffer& buffer, samplepos_t time, bool reset = true);
	void resolve_notes (Evoral::EventSink<samplepos_t>& buffer, samplepos_t time);
	void resolve_notes (MidiSource& src, const Source::WriterLock& lock, Temporal::Beats time);

	void flush_notes (MidiBuffer& buffer, samplepos_t time, bool reset = true);

	template<typename Time>
	void flush_notes (Evoral::EventSink<Time> &sink, Time time, bool reset = true) {
		push_notes<Time> (sink, time, reset, MIDI_CMD_NOTE_ON);
	}

	bool empty() const { return _on == 0; }
	uint16_t on() const { return _on; }
	bool active (uint8_t note, uint8_t channel) const {
		return _active_notes[(channel*128)+note] > 0;
	}

	template<typename Time>
	void track (const Evoral::Event<Time>& ev) {
		track (ev.buffer());
	}

	void add (uint8_t note, uint8_t chn, uint8_t velocity);
	void remove (uint8_t note, uint8_t chn);

  private:
	uint8_t  _active_notes[128*16];
	uint8_t  _active_velocities[128*16];
	uint16_t _on;

	template<typename Time>
	void push_notes (Evoral::EventSink<Time> &dst, Time time, bool reset, int cmd) {
		DEBUG_TRACE (PBD::DEBUG::MidiTrackers, string_compose ("%1 ES::push_notes @ %2 on = %3\n", this, time, _on));

		if (!_on) {
			return;
		}

		for (int channel = 0; channel < 16; ++channel) {
			const int coff = channel << 7;
			for (int note = 0; note < 128; ++note) {
				uint8_t cnt = _active_notes[note + coff];
				uint8_t vel = _active_velocities[note + coff];
				while (cnt) {
					uint8_t buffer[3] = { ((uint8_t) (cmd | channel)), uint8_t (note), vel};
					/* note that we do not care about failure from
					   write() ... should we warn someone ?
					*/
					dst.write (time, Evoral::MIDI_EVENT, 3, buffer);
					cnt--;
					DEBUG_TRACE (PBD::DEBUG::MidiTrackers, string_compose ("%1: MB-push note %2/%3 vel %5 at %4\n", this, (int) note, (int) channel, time, (int) vel));
				}
				if (reset) {
					_active_notes [note + coff] = 0;
					_active_velocities [note + coff] = 0;
				}
			}
		}
		if (reset) {
			_on = 0;
		}
	}
};

class LIBARDOUR_API MidiStateTracker : public MidiNoteTracker
{
  public:
	MidiStateTracker ();
	~MidiStateTracker() {}

	void track (const uint8_t* evbuf);
	void dump (std::ostream&) const;
	void reset ();

	void flush (MidiBuffer&, samplepos_t, bool reset);
	void resolve_state (Evoral::EventSink<samplepos_t>&, Evoral::EventList<samplepos_t> const&, samplepos_t time, bool reset = true);
	void resolve_diff (MidiStateTracker const& other, Evoral::EventSink<samplepos_t>&, samplepos_t time, bool reset = true);

  private:
	uint8_t  program[16];
	uint16_t bender[16];
	uint16_t pressure[16];
	uint8_t  control[16][127];
};

} // namespace ARDOUR

