/*
 * Copyright (C) 2008-2016 David Robillard <d@drobilla.net>
 * Copyright (C) 2009-2017 Paul Davis <paul@linuxaudiosystems.com>
 * Copyright (C) 2010-2012 Carl Hetherington <carl@carlh.net>
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

#include <iostream>

#include "pbd/compose.h"

#include "evoral/EventList.h"

#include "ardour/debug.h"
#include "ardour/midi_source.h"
#include "ardour/midi_state_tracker.h"
#include "ardour/parameter_types.h"

using namespace std;
using namespace ARDOUR;


MidiNoteTracker::MidiNoteTracker ()
{
	reset ();
}

void
MidiNoteTracker::reset ()
{
	DEBUG_TRACE (PBD::DEBUG::MidiTrackers, string_compose ("%1: reset\n", this));
	memset (_active_notes, 0, sizeof (_active_notes));
	_on = 0;
}

void
MidiNoteTracker::add (uint8_t note, uint8_t chn)
{
	const int coff = chn << 7;
	if (_active_notes[note + coff] == 0) {
		++_on;
	}
	++_active_notes[note + coff];

#if 0
	if (_active_notes[note + coff] > 1) {
		cerr << this << " note " << (int) note << '/' << (int) chn << " was already on, now at " << (int) _active_notes[note + coff] << endl;
	}
#endif

	DEBUG_TRACE (PBD::DEBUG::MidiTrackers, string_compose ("%1 ON %2/%3 voices %5 total on %4\n",
							       this, (int) note, (int) chn, _on,
							       (int) _active_notes[note + coff]));
}

void
MidiNoteTracker::remove (uint8_t note, uint8_t chn)
{
	const int coff = chn << 7;
	switch (_active_notes[note + coff]) {
	case 0:
		break;
	case 1:
		--_on;
		_active_notes [note + coff] = 0;
		break;
	default:
		--_active_notes [note + coff];
		break;

	}
	DEBUG_TRACE (PBD::DEBUG::MidiTrackers, string_compose ("%1 OFF %2/%3 current voices = %5 total on %4\n",
							       this, (int) note, (int) chn, _on,
							       (int) _active_notes[note + coff]));
}

void
MidiNoteTracker::track (const MidiBuffer::const_iterator &from, const MidiBuffer::const_iterator &to)
{
	for (MidiBuffer::const_iterator i = from; i != to; ++i) {
		track(*i);
	}
}

void
MidiNoteTracker::track (const uint8_t* evbuf)
{
	const uint8_t type = evbuf[0] & 0xF0;
	const uint8_t chan = evbuf[0] & 0x0F;
	switch (type) {
	case MIDI_CTL_ALL_NOTES_OFF:
		reset();
		break;
	case MIDI_CMD_NOTE_ON:
		add(evbuf[1], chan);
		break;
	case MIDI_CMD_NOTE_OFF:
		remove(evbuf[1], chan);
		break;
	}
}

void
MidiNoteTracker::resolve_notes (MidiBuffer &dst, samplepos_t time, bool reset)
{
	push_notes (dst, time, reset, MIDI_CMD_NOTE_OFF, 64);
}

void
MidiNoteTracker::flush_notes (MidiBuffer &dst, samplepos_t time, bool reset)
{
	push_notes (dst, time, reset, MIDI_CMD_NOTE_ON, 64);
}

void
MidiNoteTracker::push_notes (MidiBuffer &dst, samplepos_t time, bool reset, int cmd, int velocity)
{
	DEBUG_TRACE (PBD::DEBUG::MidiTrackers, string_compose ("%1 MB-resolve notes @ %2 on = %3\n", this, time, _on));

	if (!_on) {
		return;
	}

	for (int channel = 0; channel < 16; ++channel) {
		const int coff = channel << 7;
		for (int note = 0; note < 128; ++note) {
			while (_active_notes[note + coff]) {
				uint8_t buffer[3] = { ((uint8_t) (cmd | channel)), uint8_t (note), (uint8_t) velocity };
				Evoral::Event<MidiBuffer::TimeType> ev (Evoral::MIDI_EVENT, time, 3, buffer, false);
				/* note that we do not care about failure from
				   push_back() ... should we warn someone ?
				*/
				dst.push_back (ev);
				_active_notes[note + coff]--;
				DEBUG_TRACE (PBD::DEBUG::MidiTrackers, string_compose ("%1: MB-push note %2/%3 at %4\n", this, (int) note, (int) channel, time));
			}
		}
	}
	if (reset) {
		_on = 0;
	}
}

void
MidiNoteTracker::resolve_notes (Evoral::EventSink<samplepos_t> &dst, samplepos_t time)
{
	uint8_t buf[3];

	DEBUG_TRACE (PBD::DEBUG::MidiTrackers, string_compose ("%1 EVS-resolve notes @ %2 on = %3\n", this, time, _on));

	if (!_on) {
		return;
	}

	for (int channel = 0; channel < 16; ++channel) {
		const int coff = channel << 7;
		for (int note = 0; note < 128; ++note) {
			while (_active_notes[note + coff]) {
				buf[0] = MIDI_CMD_NOTE_OFF|channel;
				buf[1] = note;
				buf[2] = 0;
				/* note that we do not care about failure from
				   write() ... should we warn someone ?
				*/
				dst.write (time, Evoral::MIDI_EVENT, 3, buf);
				_active_notes[note + coff]--;
				DEBUG_TRACE (PBD::DEBUG::MidiTrackers, string_compose ("%1: EVS-resolved note %2/%3 at %4\n",
										       this, (int) note, (int) channel, time));
			}
		}
	}

	_on = 0;
}

void
MidiNoteTracker::resolve_notes (MidiSource& src, const MidiSource::WriterLock& lock, Temporal::Beats time)
{
	DEBUG_TRACE (PBD::DEBUG::MidiTrackers, string_compose ("%1 MS-resolve notes @ %2 on = %3\n", this, time, _on));

	if (!_on) {
		return;
	}

	/* NOTE: the src must be locked */

	for (int channel = 0; channel < 16; ++channel) {
		const int coff = channel << 7;
		for (int note = 0; note < 128; ++note) {
			while (_active_notes[note + coff]) {
				Evoral::Event<Temporal::Beats> ev (Evoral::MIDI_EVENT, time, 3, 0, true);
				ev.set_type (MIDI_CMD_NOTE_OFF);
				ev.set_channel (channel);
				ev.set_note (note);
				ev.set_velocity (0);
				src.append_event_beats (lock, ev);
				DEBUG_TRACE (PBD::DEBUG::MidiTrackers, string_compose ("%1: MS-resolved note %2/%3 at %4\n",
										       this, (int) note, (int) channel, time));
				_active_notes[note + coff]--;
				/* don't stack events up at the same time */
				time += Temporal::Beats::one_tick();
			}
		}
	}

	_on = 0;
}

void
MidiNoteTracker::dump (ostream& o)
{
	o << "****** NOTES\n";
	for (int c = 0; c < 16; ++c) {
		const int coff = c << 7;
		for (int x = 0; x < 128; ++x) {
			if (_active_notes[coff + x]) {
				o << "Channel " << c+1 << " Note " << x << " is on ("
				  << (int) _active_notes[coff+x] <<  " times)\n";
			}
		}
	}
	o << "+++++\n";
}

/*----------------*/

MidiStateTracker::MidiStateTracker ()
{
	reset ();
}

void
MidiStateTracker::reset ()
{
	const size_t n_channels = 16;
	const size_t n_controls = 127;

	MidiNoteTracker::reset ();

	for (size_t n = 0; n < n_channels; ++n) {
		program[n] = 0x80;
		bender[n] = 0x8000;
	}

	for (size_t chn = 0; chn < n_channels; ++chn) {
		for (size_t c = 0; c < n_controls; ++c) {
			control[chn][c] = 0x80;
		}
	}
}

void
MidiStateTracker::dump (ostream& o)
{
	const size_t n_channels = 16;
	const size_t n_controls = 127;
	bool need_comma = false;

	o << "DUMP for MidiStateTracker @ " << this << std::endl;
	MidiNoteTracker::dump (o);

	for (size_t chn = 0; chn < n_channels; ++chn) {
		if ((program[chn] & 0x80) == 0) {
			if (need_comma) {
				o << ", ";
			}
			o << "program[" << chn << "] = " << int (program[chn] & 0x7f);
			need_comma = true;
		}
	}
	o << std::endl;

	need_comma = false;

	for (size_t chn = 0; chn < n_channels; ++chn) {
		for (size_t ctl = 0; ctl < n_controls; ++ctl) {
			if ((control[chn][ctl] & 0x80) == 0) {
				if (need_comma) {
					o << ", ";
				}
				o << "ctrl[" << chn << "][" << ctl << "] = " << int (control[chn][ctl] & 0x7f);
				need_comma = true;
			}
		}
	}
	o << std::endl;
}

void
MidiStateTracker::track (const uint8_t* evbuf)
{
	const uint8_t type = evbuf[0] & 0xF0;
	const uint8_t chan = evbuf[0] & 0x0F;

	switch (type) {
	case MIDI_CTL_ALL_NOTES_OFF:
		MidiNoteTracker::reset();
		break;

	case MIDI_CMD_NOTE_ON:
		add (evbuf[1], chan);
		break;
	case MIDI_CMD_NOTE_OFF:
		remove (evbuf[1], chan);
		break;

	case MIDI_CMD_CONTROL:
		control[chan][evbuf[1]] = evbuf[2];
		break;

	case MIDI_CMD_PGM_CHANGE:
		program[chan] = evbuf[1];
		break;

	case MIDI_CMD_CHANNEL_PRESSURE:
		pressure[chan] = evbuf[1];
		break;

	case MIDI_CMD_NOTE_PRESSURE:
		break;

	case MIDI_CMD_BENDER:
		bender[chan] = ((evbuf[2]<<7) | evbuf[1]) & 0x3fff;
		break;

	case MIDI_CMD_COMMON_RESET:
		reset ();
		break;

	default:
		break;
	}
}

void
MidiStateTracker::flush (MidiBuffer& dst, samplepos_t time, bool reset)
{
	uint8_t buf[3];
	const size_t n_channels = 16;
	const size_t n_controls = 127;

	flush_notes (dst, time, reset);

	for (size_t chn = 0; chn < n_channels; ++chn) {
		for (size_t ctl = 0; ctl < n_controls; ++ctl) {
			if ((control[chn][ctl] & 0x80) == 0) {
				buf[0] = MIDI_CMD_CONTROL|chn;
				buf[1] = ctl;
				buf[2] = control[chn][ctl] & 0x7f;
				dst.write (time, Evoral::MIDI_EVENT, 3, buf);
				if (reset) {
					control[chn][ctl] = 0x80;
				}
			}
		}

		if ((program[chn] & 0x80) == 0) {
			buf[0] = MIDI_CMD_PGM_CHANGE|chn;
			buf[1] = program[chn] & 0x7f;
			dst.write (time, Evoral::MIDI_EVENT, 2, buf);
			if (reset) {
				program[chn] = 0x80;
			}
		}

		/* XXX bender */
		/* XXX pressure */
	}
}

/* return 0 if event is not found
 * return 1 if event is found before time t
 * return -1 if event is found at time t
 */
static int
find_event (Evoral::EventList<samplepos_t> const& evlist, samplepos_t time, uint8_t* buf)
{
	for (auto const& e : evlist) {
		Evoral::Event<samplepos_t>* ev (e);
		timepos_t                   t (ev->time ());
		if (t > time) {
			break;
		}
		uint8_t const* evbuf = ev->buffer ();
		if (evbuf[0] == buf[0]) {
			if (buf[1] != 0x80 && evbuf[1] != buf[1]) {
				continue;
			}
			for (uint32_t i = 1; i < ev->size (); ++i) {
				buf[i] = evbuf[i];
			}
			return t == time ? -1 : 1;
		}
	}
	return 0;
}

void
MidiStateTracker::resolve_state (Evoral::EventSink<samplepos_t>& dst, Evoral::EventList<samplepos_t> const& evlist, samplepos_t time, bool reset)
{
	/* XXX implement me */

	uint8_t      buf[3];
	const size_t n_channels = 16;
	const size_t n_controls = 127;

	resolve_notes (dst, time);

	for (size_t chn = 0; chn < n_channels; ++chn) {

		/* restore CC */
		for (size_t ctl = 0; ctl < n_controls; ++ctl) {
			if ((control[chn][ctl] & 0x80) == 0) {
				if (reset) {
					control[chn][ctl] = 0x80;
				}
				buf[0] = MIDI_CMD_CONTROL | chn;
				buf[1] = ctl;
				switch (find_event (evlist, time, buf)) {
					case 1:
						/* (event found before tme)
						 * restore prior CC (notably bank select)
						 *
						 *    Layer 1: [CX....]         [.......]
						 *    Layer 2:      [.....CY.......]
						 * restore CX:                   ^
						 */
						dst.write (time, Evoral::MIDI_EVENT, 3, buf);
						break;
					case 0:
						/* (no event was found before, or at tme)
						 * The goal is to reset a conroller, unless there already
						 * is an CC event at the start of above region (case -1:).
						 *
						 *    Layer 1: [......]         [CZ......]
						 *    Layer 2:      [.....CY.......]
						 * reset, unless CZ exist:       ^
						 */
						switch (ctl) {
							/* clang-format off */
							case 0x01: buf[2] = 0x00; break; /* mod wheel MSB */
							case 0x21: buf[2] = 0x00; break; /* mod wheel LSB */
							case 0x02: buf[2] = 0x00; break; /* breath MSB */
							case 0x22: buf[2] = 0x00; break; /* breath LSB */
							case 0x07: buf[2] = 0x7f; break; /* volume MSB */
							case 0x27: buf[2] = 0x7f; break; /* volume LSB */
							case 0x08: buf[2] = 0x40; break; /* balance MSB */
							case 0x28: buf[2] = 0x00; break; /* balance LSB */
							case 0x0a: buf[2] = 0x40; break; /* pan MSB */
							case 0x2a: buf[2] = 0x00; break; /* pan LSB */
							case 0x40: buf[2] = 0x00; break; /* sustain */
							case 0x41: buf[2] = 0x00; break; /* portamento */
							case 0x42: buf[2] = 0x00; break; /* sostenuto */
							case 0x43: buf[2] = 0x00; break; /* soft pedal */
							case 0x44: buf[2] = 0x00; break; /* legato switch */
							/* clang-format on */
							default:
								/* do not reset other controls */
								continue;
						}
						dst.write (time, Evoral::MIDI_EVENT, 3, buf);
						break;

					default:
						/* do nothing */
						break;
				}
			}
		}

		/* If the program was modified, replay the most recent event found in evlist before *time*.
		 *
		 *    Layer 1: [P1....]         [.......]
		 *    Layer 2:      [.....P2.......]
		 * restore P1:                   ^
		 */
		if ((program[chn] & 0x80) == 0) {
			buf[0] = MIDI_CMD_PGM_CHANGE | chn;
			buf[1] = 0x80;
			if (find_event (evlist, time, buf) > 0) {
				dst.write (time, Evoral::MIDI_EVENT, 2, buf);
			}
			if (reset) {
				program[chn] = 0x80;
			}
		}

		/* reset pitch-bend */
		if ((bender[chn] & 0x8000) == 0) {
			buf[0] = MIDI_CMD_BENDER | chn;
			buf[1] = 0x80;
			/* .. unless there is a PB event at the start */
			if (find_event (evlist, time, buf) >= 0) {
				buf[1] = 0x00;
				buf[2] = 0x40;
				dst.write (time, Evoral::MIDI_EVENT, 3, buf);
			}
			if (reset) {
				bender[chn] = 0x8000;
			}
		}
	}
}
