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

#include "pbd/i18n.h"

#include "ardour/audioengine.h"
#include "ardour/midi_buffer.h"
#include "ardour/midi_state_tracker.h"
#include "ardour/step_sequencer.h"
#include "ardour/tempo.h"

using namespace PBD;
using namespace ARDOUR;
using namespace std;

Step::Step (StepSequence &s, Temporal::Beats const & b, int base_note)
	: _sequence (s)
	, _enabled (true)
	, _nominal_beat (b)
	, _skipped (false)
	, _mode (AbsolutePitch)
	, _octave_shift (0)
	, _duration (1)
{
	_notes[0].number = base_note;

	for (size_t n = 1; n < _notes_per_step; ++n) {
		_notes[n].number = -1;
	}


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
Step::set_note (double note, double velocity, int n)
{
	assert (n < _notes_per_step);
	_notes[n].number = note;
	_notes[n].velocity = velocity;
}

void
Step::set_velocity (double velocity, size_t n)
{
	assert (n < _notes_per_step);

	if (_notes[n].velocity != velocity) {
		_notes[n].velocity = velocity;

		PropertyChange pc;
		PropertyChanged (pc);
	}
}

void
Step::set_octave_shift (int s)
{
	if (s > 4) {
		s = 4;
	} else if (s < -4) {
		s = -4;
	}

	if (s == _octave_shift) {
		return;
	}

	_octave_shift = s;

	PropertyChange pc;
	PropertyChanged (pc);
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

void
Step::adjust_duration (DurationRatio const & amt)
{
	set_duration (_duration + amt);
}

void
Step::set_duration (DurationRatio const & dur)
{
	if (dur > 1.0) {
		_duration = DurationRatio (1);
	} else if (dur < DurationRatio()) {
		_duration = DurationRatio ();
	} else {
		_duration = dur;
	}

	PropertyChange pc;
	PropertyChanged (pc);
}

void
Step::adjust_pitch (int amt)
{
	Step::Note& note (_notes[0]);

	note.number += amt;

	if (note.number > 127.0) {
		note.number = 127.0;
	}

	if (note.number < 0.0) {
		note.number = 0.0;
	}

	PropertyChange pc;
	PropertyChanged (pc);
}

void
Step::adjust_velocity (int amt)
{
	Step::Note& note (_notes[0]);

	note.velocity += (1.0/128.0) * amt;

	if (note.velocity > 1.0) {
		note.velocity = 1.0;
	}

	if (note.velocity < 0.0) {
		note.velocity = 0.0;
	}

	PropertyChange pc;
	PropertyChanged (pc);
}

void
Step::adjust_octave (int amt)
{
	set_octave_shift (_octave_shift + amt);
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

	if (_duration == DurationRatio ()) {
		/* no duration, so no new notes on */
		return;
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

			if (_octave_shift) {

				const int t = mbuf[1] + (12 * _octave_shift);

				if (t > 127 || t < 0) {
					/* Out of range */
					return;
				}

				mbuf[1] = t;
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

			if (_duration == DurationRatio (1)) {
				/* use 1 tick less than the sequence step size
				 * just to get non-simultaneous on/off events at
				 * step boundaries.
				*/
				note.off_at += Temporal::Beats (0, _sequence.step_size().to_ticks() - 1);
			} else {
				note.off_at += Temporal::Beats (0, (_sequence.step_size().to_ticks() * _duration.numerator()) / _duration.denominator());
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
Step::reschedule (Temporal::Beats const & start, Temporal::Beats const & offset)
{
	if (_nominal_beat < offset) {
		_scheduled_beat = start + _nominal_beat + sequencer().duration(); /* schedule into the next loop iteration */
	} else {
		_scheduled_beat = start + _nominal_beat; /* schedule into the current loop iteration */
	}

	/* MIDI state tracker will deal with any stuck notes, so here we just
	 * update our records to note that all notes are not currently
	 * sounding.
	 */
	for (size_t n = 0; n < _notes_per_step; ++n) {
		_notes[n].on = false;
		_notes[n].off_at = Temporal::Beats();
	}
}

XMLNode&
Step::get_state ()
{
	return *new XMLNode (X_("Step"));
}

int
Step::set_state (XMLNode const &, int)
{
	return 0;
}

/**/

StepSequence::StepSequence (StepSequencer& s, size_t nsteps, Temporal::Beats const & step_size, Temporal::Beats const & bar_size, int r)
	: _sequencer (s)
	, _start (0)
	, _end (nsteps - 1)
	, _channel (0)
	, _step_size (step_size)
	, _bar_size (bar_size)
	, _root (r)
	, _mode (MusicalMode::IonianMajor)
{
	Temporal::Beats beats;

	for (size_t s = 0; s < nsteps; ++s) {
		_steps.push_back (new Step (*this, beats, _root));
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
		(*i)->reschedule (start, offset);
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

Step&
StepSequence::step (size_t n) const
{
	assert (n < _steps.size());
	return *_steps[n];
}


XMLNode&
StepSequence::get_state()
{
	return *new XMLNode (X_("StepSequence"));
}

int
StepSequence::set_state (XMLNode const &, int)
{
	return 0;
}

/**/

StepSequencer::StepSequencer (TempoMap& tmap, size_t nseqs, size_t nsteps, Temporal::Beats const & step_size, Temporal::Beats const & bar_size, int notenum)
	: _tempo_map (tmap)
	, _step_size (step_size)
	, _start (0)
	, _end (nsteps)
	, _last_step (0)
{
	for (size_t n = 0; n < nseqs; ++n) {
		_sequences.push_back (new StepSequence (*this, nsteps, step_size, bar_size, notenum));
		notenum++;
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

	const Temporal::Beats terminal_beat = Temporal::Beats (_tempo_map.beat_at_sample (end_sample - 1));
	const size_t dur_ticks = duration().to_ticks();
	const size_t step_ticks = _step_size.to_ticks();
	_last_step = ((terminal_beat - _last_start).to_ticks() % dur_ticks) / step_ticks;

	return true;
}

int
StepSequencer::last_step () const
{
	return _last_step;
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
	_last_start = start;
	{
		Glib::Threads::Mutex::Lock lm1 (_sequence_lock);
		for (StepSequences::iterator s = _sequences.begin(); s != _sequences.end(); ++s) {
			(*s)->startup (start, offset);
		}
	}

}

StepSequence&
StepSequencer::sequence (size_t n) const
{
	assert (n < _sequences.size());
	return *_sequences[n];
}

XMLNode&
StepSequencer::get_state()
{
	return *new XMLNode (X_("StepSequencer"));
}

int
StepSequencer::set_state (XMLNode const &, int)
{
	return 0;
}
