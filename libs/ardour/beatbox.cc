/*
    Copyright (C) 2017 Paul Davis

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

#include <iostream>
#include <cstdio>
#include <cmath>
#include <cstring>

#include <unistd.h>
#include <stdint.h>

#include "pbd/i18n.h"

#include "evoral/midi_events.h"

#include "ardour/audioengine.h"
#include "ardour/beatbox.h"
#include "ardour/midi_buffer.h"
#include "ardour/session.h"
#include "ardour/smf_source.h"
#include "ardour/step_sequencer.h"
#include "ardour/tempo.h"

using std::cerr;
using std::endl;

using namespace ARDOUR;

MultiAllocSingleReleasePool BeatBox::Event::pool (X_("beatbox events"), sizeof (Event), 2048);

BeatBox::BeatBox (Session& s)
	: Processor (s, _("BeatBox"))
	, _sequencer (0)
	, _start_requested (false)
	, _running (false)
	, _measures (2)
	, _tempo (-1.0)
	, _meter_beats (-1)
	, _meter_beat_type (-1)
	, superclock_cnt (0)
	, last_start (0)
	, whole_note_superclocks (0)
	, tick_superclocks (0)
	, beat_superclocks (0)
	, measure_superclocks (0)
	, _quantize_divisor (4)
	, clear_pending (false)
	, add_queue (64)
	, remove_queue (64)
{
	_display_to_user = true;
	_sequencer = new StepSequencer (s.tempo_map(), 1, 8, Temporal::Beats (0, Temporal::Beats::PPQN/4), Temporal::Beats (4, 0));
}

BeatBox::~BeatBox ()
{
	delete _sequencer;
}

void
BeatBox::start ()
{
	/* we can start */

	_start_requested = true;
}

void
BeatBox::stop ()
{
	_start_requested = false;
}

void
BeatBox::silence (samplecnt_t, samplepos_t)
{
	/* do nothing, we have no inputs or outputs */
}

void
BeatBox::run (BufferSet& bufs, samplepos_t start_sample, samplepos_t end_sample, double speed, pframes_t nsamples, bool /*result_required*/)
{
	if (bufs.count().n_midi() == 0) {
		return;
	}

	bool resolve = false;

	if (speed == 0) {
		if (_running) {
			resolve = true;
			_running = false;
		}
	}

	if (speed != 0)  {

		if (!_running || (last_end != start_sample)) {

			if (last_end != start_sample) {
				resolve = true;
			}

			/* compute the beat position of this first "while-moving
			 * run() call as an offset into the sequencer's current loop
			 * length.
			 */

			TempoMap& tmap (_session.tempo_map());

			const Temporal::Beats start_beat (tmap.beat_at_sample (start_sample));
			const int32_t tick_duration = _sequencer->duration().to_ticks();



			Temporal::Beats closest_previous_loop_start = Temporal::Beats::ticks ((start_beat.to_ticks() / tick_duration) * tick_duration);
			Temporal::Beats offset = Temporal::Beats::ticks ((start_beat.to_ticks() % tick_duration));
			_sequencer->startup (closest_previous_loop_start, offset);
			last_start = start_sample;
			_running = true;

		}
	}

	if (resolve) {
		outbound_tracker.resolve_notes (bufs.get_midi(0), 0);
	}

	_sequencer->run (bufs.get_midi (0), _running, start_sample, end_sample, outbound_tracker);
	last_end = end_sample;
}

void
BeatBox::set_quantize (int divisor)
{
	_quantize_divisor = divisor;
}

void
BeatBox::clear ()
{
	clear_pending = true;
}

bool
BeatBox::EventComparator::operator() (Event const * a, Event const *b) const
{
	if (a->time == b->time) {
		if (a->buf[0] == b->buf[0]) {
			return a < b;
		}
		return !ARDOUR::MidiBuffer::second_simultaneous_midi_byte_is_first (a->buf[0], b->buf[0]);
	}
	return a->time < b->time;
}

bool
BeatBox::can_support_io_configuration (const ChanCount& in, ChanCount& out)
{
	return true;
}

XMLNode&
BeatBox::get_state(void)
{
	return state ();
}

XMLNode&
BeatBox::state()
{
	XMLNode& node = Processor::state();
	node.set_property ("type", "beatbox");

	return node;
}

Timecode::BBT_Time
BeatBox::get_last_time() const
{
	/* need mutex */
	return last_time;
}

void
BeatBox::edit_note_number (int old_number, int new_number)
{
	for (Events::iterator e = _current_events.begin(); e != _current_events.end(); ++e) {
		if (((*e)->buf[0] & 0xf0) == MIDI_CMD_NOTE_OFF || ((*e)->buf[0] & 0xf0) == MIDI_CMD_NOTE_ON) {
			if ((*e)->buf[1] == old_number) {
				(*e)->buf[1] = new_number;
			}
		}
	}
}

void
BeatBox::remove_note (int note, Timecode::BBT_Time at)
{
}

void
BeatBox::add_note (int note, int velocity, Timecode::BBT_Time at)
{
	Event* on = new Event; // pool allocated, thread safe

	if (!on) {
		cerr << "No more events for injection, grow pool\n";
		return;
	}
	/* convert to zero-base */
	at.bars--;
	at.beats--;

	/* clamp to current loop configuration */
	at.bars %= _measures;
	at.beats %= _meter_beats;

	on->time = (measure_superclocks * at.bars) + (beat_superclocks * at.beats);
	on->size = 3;
	on->buf[0] = MIDI_CMD_NOTE_ON | (0 & 0xf);
	on->buf[1] = note;
	on->buf[2] = velocity;

	Event* off = new Event; // pool allocated, thread safe

	if (!off) {
		cerr << "No more events for injection, grow pool\n";
		return;
	}

	if (_quantize_divisor != 0) {
		off->time = on->time + (beat_superclocks / _quantize_divisor);
	} else {
		/* 1/4 second note .. totally arbitrary */
		off->time = on->time + (_session.sample_rate() / 4);
	}
	off->size = 3;
	off->buf[0] = MIDI_CMD_NOTE_OFF | (0 & 0xf);
	off->buf[1] = note;
	off->buf[2] = 0;

	add_queue.write (&on, 1);
	add_queue.write (&off, 1);
}

bool
BeatBox::fill_source (boost::shared_ptr<Source> src)
{
	boost::shared_ptr<SMFSource> msrc = boost::dynamic_pointer_cast<SMFSource> (src);

	if (msrc) {
		return fill_midi_source (msrc);
	}

	return false;
}

bool
BeatBox::fill_midi_source (boost::shared_ptr<SMFSource> src)
{
	Temporal::Beats smf_beats;

	if (_current_events.empty()) {
		return false;
	}

	Source::Lock lck (src->mutex());

	try {
		src->mark_streaming_midi_write_started (lck, Sustained);
		src->begin_write ();

		for (Events::const_iterator e = _current_events.begin(); e != _current_events.end(); ++e) {
			/* convert to quarter notes */
			smf_beats = Temporal::Beats ((*e)->time / (beat_superclocks * (4.0 / _meter_beat_type)));
			Evoral::Event<Temporal::Beats> ee (Evoral::MIDI_EVENT, smf_beats, (*e)->size, (*e)->buf, false);
			src->append_event_beats (lck, ee);
			// last_time = (*e)->time;
		}

		src->end_write (src->path());
		src->mark_nonremovable ();
		src->mark_streaming_write_completed (lck);
		return true;

	} catch (...) {
		cerr << "Exception during beatbox write to SMF... " << endl;
	}

	return false;
}
