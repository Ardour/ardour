/*
 * Copyright (C) 2018 Paul Davis <paul@linuxaudiosystems.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2, or (at your option)
 * any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */

#include <cassert>

#include "ardour/audioengine.h"
#include "ardour/midi_buffer.h"
#include "ardour/midi_state_tracker.h"
#include "ardour/step_sequencer.h"
#include "ardour/tempo.h"

using namespace ARDOUR;
using namespace std;

static int notenum = 35;

Step::Step (StepSequence &s, Temporal::Beats const & b)
	: _sequence (s)
	, _enabled (true)
	, _nominal_beat (b)
	, _skipped (false)
	, _mode (AbsolutePitch)
{
	std::cerr << "step @ " << b << std::endl;

	for (size_t n = 0; n < _notes_per_step; ++n) {
		_notes[n].number = -1;
	}

	/* XXX HACK XXXX */
	_notes[0].number = notenum;

	for (size_t n = 0; n < _parameters_per_step; ++n) {
		_parameters[n].parameter = -1;
	}
}

Step::~Step ()
{
}

StepSequencer&
Step::sequencer() const
{
	return _sequence.sequencer();
}

void
Step::set_mode (Mode m)
{
	_mode = m;
}

void
Step::set_beat (Temporal::Beats const & b)
{
	_nominal_beat = b;
}

void
Step::set_note (double note, double velocity, int32_t duration, int n)
{
	assert (n < _notes_per_step);
	_notes[n].number = note;
	_notes[n].velocity = velocity;
	_notes[n].duration = duration;
}

void
Step::set_chord (size_t note_cnt, double* notes)
{
}

void
Step::set_parameter (int number, double value, int n)
{
	assert (n < _parameters_per_step);
	_parameters[n].parameter = number;
	_parameters[n].value = value;
}

void
Step::set_enabled (bool yn)
{
	_enabled = yn;
}

bool
Step::run (MidiBuffer& buf, bool running, samplepos_t start_sample, samplepos_t end_sample, MidiStateTracker&  tracker)
{
	for (size_t n = 0; n < _notes_per_step; ++n) {
		check_parameter (n, buf, running, start_sample, end_sample);
	}

	for (size_t n = 0; n < _notes_per_step; ++n) {
		check_note (n, buf, running, start_sample, end_sample, tracker);
	}

	if (running) {

		samplepos_t scheduled_samples = sequencer().tempo_map().sample_at_beat (_scheduled_beat.to_double());

		if (scheduled_samples >= start_sample && scheduled_samples < end_sample) {
			/* this step was covered by the run() range, so update its next
			 *  scheduled time.
			 */
			_scheduled_beat += sequencer().duration();
		}

	}

	return true;
}

void
Step::check_parameter (size_t n, MidiBuffer& buf, bool running, samplepos_t start_sample, samplepos_t end_sample)
{
}

void
Step::check_note (size_t n, MidiBuffer& buf, bool running, samplepos_t start_sample, samplepos_t end_sample, MidiStateTracker& tracker)
{
	Note& note (_notes[n]);

	/* could be a note off message to be delivered before any note on
	 * message (and the note number may differ from the current value.
	 * Deliver it now, if appropriate.
	 */

	if (note.on) {

		samplepos_t off_samples = sequencer().tempo_map().sample_at_beat (note.off_at.to_double());

		if (off_samples >= start_sample && off_samples < end_sample) {

			buf.write (off_samples - start_sample, Evoral::MIDI_EVENT, 3, note.off_msg);
			tracker.remove (note.off_msg[1], _sequence.channel());

			/* record keeping */

			note.on = false;
			note.off_at = Temporal::Beats();
		}

		/* XXX we should possibly queue these note offs */

	}

	if (note.number < 0) {
		/* note not set .. ignore */
		return;
	}

	/* figure out when this note would sound */

	Temporal::Beats note_on_time = _scheduled_beat;

	note_on_time += note.offset;

	if (running && !note.on) {

		/* don't play silent notes */

		if (note.velocity == 0) {
			return;
		}

		samplepos_t on_samples = sequencer().tempo_map().sample_at_beat (note_on_time.to_double());

		if (on_samples >= start_sample && on_samples < end_sample) {

			uint8_t mbuf[3];

			/* prepare 3 MIDI bytes for note on */

			mbuf[0] = 0x90 | _sequence.channel();

			switch (_mode) {
			case AbsolutePitch:
				mbuf[1] = note.number;
				break;
			case RelativePitch:
				mbuf[1] = _sequence.root() + note.interval;
				break;
			}

			mbuf[2] = (uint8_t) floor (note.velocity * 127.0);

			note.off_msg[0] = 0x80 | _sequence.channel();
			note.off_msg[1] = mbuf[1];
			note.off_msg[2] = mbuf[2];

			/* Put it into the MIDI buffer */
			buf.write (on_samples - start_sample, Evoral::MIDI_EVENT, 3, mbuf);
			tracker.add (mbuf[1], _sequence.channel());

			/* keep track (even though other things will at different levels */

			note.on = true;

			/* compute note off time based on our duration */

			note.off_at = note_on_time;

			if (note.duration == 1) {
				note.off_at += Temporal::Beats (0, _sequence.step_size().to_ticks() - 1);
			} else {
				note.off_at += Temporal::Beats (0, _sequence.step_size().to_ticks() / note.duration);
			}
		}
	}

	/* if the buffer size is large and the step size or note length is very
	 * small, the note off could be within the same ::run() cycle as the
	 * note on. So check again to see if we should deliver it in this same
	 * ::run() cycle.
	 */

	if (note.on) {

		samplepos_t off_samples = sequencer().tempo_map().sample_at_beat (note.off_at.to_double());

		if (off_samples >= start_sample && off_samples < end_sample) {

			buf.write (off_samples - start_sample, Evoral::MIDI_EVENT, 3, note.off_msg);
			tracker.remove (note.off_msg[1], _sequence.channel());

			/* record keeping */

			note.on = false;
			note.off_at = Temporal::Beats();
		}
	}
}

void
Step::set_timeline_offset (Temporal::Beats const & start, Temporal::Beats const & offset)
{
	timeline_offset = offset;

	if (_nominal_beat < offset) {
		_scheduled_beat = start + _nominal_beat + sequencer().duration(); /* schedule into the next loop iteration */
	} else {
		_scheduled_beat = start + _nominal_beat; /* schedule into the current loop iteration */
	}

	/* MIDI state tracker will deal with any stuck notes */
	for (size_t n = 0; n < _notes_per_step; ++n) {
		_notes[n].on = false;
		_notes[n].off_at = Temporal::Beats();
	}
}

/**/

StepSequence::StepSequence (StepSequencer& s, size_t nsteps, Temporal::Beats const & step_size, Temporal::Beats const & bar_size)
	: _sequencer (s)
	, _start (0)
	, _end (nsteps)
	, _channel (0)
	, _step_size (step_size)
	, _bar_size (bar_size)
	, _root (64)
	, _mode (MusicalMode::IonianMajor)
{
	Temporal::Beats beats;

	for (size_t s = 0; s < nsteps; ++s) {
		_steps.push_back (new Step (*this, beats));
		beats += step_size;
	}

	end_beat = beats;
}

StepSequence::~StepSequence ()
{
	for (Steps::iterator i = _steps.begin(); i != _steps.end(); ++i) {
		delete *i;
	}
}

void
StepSequence::startup (Temporal::Beats const & start, Temporal::Beats const & offset)
{
	for (Steps::iterator i = _steps.begin(); i != _steps.end(); ++i) {
		(*i)->set_timeline_offset (start, offset);
	}
}

void
StepSequence::reset ()
{
}

void
StepSequence::set_channel (int c)
{
	_channel = c;
}

Temporal::Beats
StepSequence::wrap (Temporal::Beats const & b) const
{
	if (b < end_beat) {
		return b;
	}

	return b - end_beat;
}


bool
StepSequence::run (MidiBuffer& buf, bool running, samplepos_t start_sample, samplepos_t end_sample, MidiStateTracker& tracker)
{
	for (Steps::iterator s = _steps.begin(); s != _steps.end(); ++s) {
		(*s)->run (buf, running, start_sample, end_sample, tracker);
	}
	return true;
}

void
StepSequence::adjust_step_pitch (int step, int amt)
{
	if (step >= _steps.size()) {
		return;
	}

	Step::Note& note (_steps[step]->_notes[0]);

	note.number += amt;

	if (note.number > 127.0) {
		note.number = 127.0;
	}

	if (note.number < 0.0) {
		note.number = 0.0;
	}
}

void
StepSequence::adjust_step_velocity (int step, int amt)
{
	if (step >= _steps.size()) {
		return;
	}

	Step::Note& note (_steps[step]->_notes[0]);

	note.velocity += (1.0/128.0) * amt;

	if (note.velocity > 127.0) {
		note.velocity = 127.0;
	}

	if (note.velocity < 0.0) {
		note.velocity = 0.0;
	}
}

/**/

StepSequencer::StepSequencer (TempoMap& tmap, size_t nseqs, size_t nsteps, Temporal::Beats const & step_size, Temporal::Beats const & bar_size)
	: _tempo_map (tmap)
	, _step_size (step_size)
	, _start (0)
	, _end (nsteps)
{
	for (size_t n = 0; n < nseqs; ++n) {
		_sequences.push_back (new StepSequence (*this, nsteps, step_size, bar_size));
	}
}

StepSequencer::~StepSequencer ()
{
	for (StepSequences::iterator i = _sequences.begin(); i != _sequences.end(); ++i) {
		delete *i;
	}
}

bool
StepSequencer::run (MidiBuffer& buf, bool running, samplepos_t start_sample, samplepos_t end_sample, MidiStateTracker& tracker)
{
	Glib::Threads::Mutex::Lock lm (_sequence_lock);

	for (StepSequences::iterator s = _sequences.begin(); s != _sequences.end(); ++s) {
		(*s)->run (buf, running, start_sample, end_sample, tracker);
	}

	return true;
}

void
StepSequencer::sync ()
{
}

void
StepSequencer::reset ()
{
	{
		Glib::Threads::Mutex::Lock lm1 (_sequence_lock);
		for (StepSequences::iterator s = _sequences.begin(); s != _sequences.end(); ++s) {
			(*s)->reset ();
		}
	}
}

Temporal::Beats
StepSequencer::duration() const
{
	return _step_size * (_end - _start) ;
}

void
StepSequencer::startup (Temporal::Beats const & start, Temporal::Beats const & offset)
{
	{
		Glib::Threads::Mutex::Lock lm1 (_sequence_lock);
		for (StepSequences::iterator s = _sequences.begin(); s != _sequences.end(); ++s) {
			(*s)->startup (start, offset);
		}
	}

}

void
StepSequencer::adjust_step_pitch (int seq, int step, int amt)
{
	_sequences.front()->adjust_step_pitch (step, amt);
}

void
StepSequencer::adjust_step_velocity (int seq, int step, int amt)
{
	_sequences.front()->adjust_step_velocity (step, amt);
}
