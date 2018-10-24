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

#ifndef __libardour_step_sequencer_h__
#define __libardour_step_sequencer_h__

#include <vector>
#include <unistd.h>

#include <glibmm/threads.h>

#include "temporal/types.h"
#include "temporal/bbt_time.h"

#include "ardour/mode.h"
#include "ardour/types.h"

namespace ARDOUR {

class MidiBuffer;

class StepSequencer;
class StepSequence;

class Step {
  public:
	enum Mode {
		AbsolutePitch,
		RelativePitch
	};

	Step (StepSequence&, Timecode::BBT_Time const & nominal_on);
	~Step ();

	void set_note (double note, double velocity = 0.5, double duration = 0.9, int n = 0);
	void set_chord (size_t note_cnt, double* notes);
	void set_parameter (int number, double value, int n = 0);

	Mode mode() const { return _mode; }
	void set_mode (Mode m);

	double note (size_t n = 0) const { return _notes[n].number; }
	double velocity (size_t n = 0) const { return _notes[n].velocity; }
	Timecode::BBT_Time beat_duration (size_t n = 0) const;
	double duration (size_t n = 0) const { return _notes[n].duration; }

	void set_offset (Timecode::BBT_Time const &, size_t n = 0);
	Timecode::BBT_Time offset (size_t n = 0) const { return _notes[n].offset; }

	int parameter (size_t n = 0) const { return _parameters[n].parameter; }
	int parameter_value (size_t n = 0) const { return _parameters[n].value; }

	void set_enabled (bool);
	bool enabled() const { return _enabled; }

	void set_repeat (size_t r);
	size_t repeat() const { return _repeat; }

	void set_nominal_on (Timecode::BBT_Time const &);
	bool run (MidiBuffer& buf, Timecode::BBT_Time const & start, Timecode::BBT_Time const & end, samplecnt_t beat_samples);

	bool skipped() const { return _skipped; }
	void set_skipped (bool);

  private:
	StepSequence&      _sequence;
	bool               _enabled;
	Timecode::BBT_Time _nominal_on;
	bool               _skipped;
	Mode               _mode;

	struct ParameterValue {
		int parameter;
		double value;
	};

	struct Note {
		union {
			double number;   /* typically MIDI note number */
			double interval; /* semitones */
		};
		double velocity;
		double duration;
		Timecode::BBT_Time offset;
		bool on;
		Timecode::BBT_Time off_at;

		Note () : number (-1), on (false) {}
		Note (double n, double v, double d, Timecode::BBT_Time o)
			: number (n), velocity (v), duration (d), offset (o), on (false) {}
	};

	static const int _notes_per_step = 5;
	static const int _parameters_per_step = 5;

	Note _notes[_notes_per_step];
	ParameterValue _parameters[_parameters_per_step];
	size_t _repeat;

	void check_note (size_t n, MidiBuffer& buf, Timecode::BBT_Time const & start, Timecode::BBT_Time const & end, ARDOUR::samplecnt_t beat_samples);
	void check_parameter (size_t n, MidiBuffer& buf, Timecode::BBT_Time const & start, Timecode::BBT_Time const & end, ARDOUR::samplecnt_t beat_samples);

};

class StepSequence
{
  public:
	enum Direction {
		forwards = 0,
		backwards = 1,
		end_to_end = 2,
		rd_random = 3
	};

	StepSequence (size_t numsteps, StepSequencer &myseq);
	~StepSequence ();

	double root() const { return _root; }
	void set_root (double n);

	int channel() const { return _channel; }
	void set_channel (int);

	MusicalMode mode() const { return _mode; }
	void set_mode (MusicalMode m);

	void shift_left (size_t n = 1);
	void shift_right (size_t n = 1);

	void set_start_step (size_t);
	void set_end_step (size_t);
	void set_start_and_end_step (size_t, size_t);

	void set_beat_divisor (size_t);
	size_t beat_divisor () const { return _beat_divisor; }

	void reset ();

	void set_tempo (double quarters_per_minute, int sr);
	bool run (MidiBuffer& buf, Timecode::BBT_Time const & start, Timecode::BBT_Time const & end);

  private:
	StepSequencer& _sequencer;
	Glib::Threads::Mutex _step_lock;
	typedef std::vector<Step*> Steps;

	Steps       _steps;
	size_t      _start;
	size_t      _end;
	double      _root;
	MusicalMode _mode;
	size_t      _beat_divisor;
	int         _channel;
	samplecnt_t _beat_samples;
};

class StepSequencer {
  public:
	enum State {
		Running,
		Halted,
		Paused,
	};

	StepSequencer (size_t nseqs, size_t nsteps);
	~StepSequencer ();

	void set_start_step (size_t);
	void set_end_step (size_t);
	void set_start_and_end_step (size_t, size_t);

	bool running() const { return _state == Running; }
	bool halted() const { return _state == Halted; }
	bool paused() const { return _state == Paused; }

	void start ();
	void halt ();        /* stop everything, reset */
	void play ();
	void pause ();
	void toggle_pause ();
	void sync ();        /* return all rows to start step */
	void reset ();       /* return entire state to default */

	double tempo() const; /* quarters per minute, not beats per minute */
	void set_tempo (double, int sr);

	bool run (MidiBuffer& buf, Timecode::BBT_Time const & start, Timecode::BBT_Time const & end);

  private:
	Glib::Threads::Mutex       _sequence_lock;
	Glib::Threads::Mutex       _state_lock;

	typedef std::vector<StepSequence*> StepSequences;

	StepSequences  _sequences;
	State          _state;

	Timecode::BBT_Time _target_start;
	Timecode::BBT_Time _target_end;
	double        _target_tempo;

	Timecode::BBT_Time _start;
	Timecode::BBT_Time _end;
	double        _tempo;
};

} /* namespace */

#endif /* __libardour_step_sequencer_h__ */
