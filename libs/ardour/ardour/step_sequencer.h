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

#include <boost/atomic.hpp>
#include <boost/rational.hpp>
#include <boost/intrusive/list.hpp>

#include <glibmm/threads.h>

#include "pbd/pool.h"
#include "pbd/ringbuffer.h"
#include "pbd/stateful.h"

#include "evoral/Event.h"

#include "temporal/types.h"
#include "temporal/beats.h"

#include "ardour/mode.h"
#include "ardour/midi_state_tracker.h"
#include "ardour/types.h"

namespace ARDOUR {

class MidiBuffer;
class MidiNoteTracker;
class StepSequencer;
class StepSequence;
class TempoMap;
class SMFSource;

typedef std::pair<Temporal::Beats,samplepos_t> BeatPosition;
typedef std::vector<BeatPosition> BeatPositions;

typedef Evoral::Event<Temporal::Beats> MusicTimeEvent;
typedef std::vector<MusicTimeEvent*> MusicTimeEvents;

class Step : public PBD::Stateful {
  public:
	enum Mode {
		AbsolutePitch,
		RelativePitch
	};

	typedef boost::rational<int> DurationRatio;

	Step (StepSequence&, size_t n, Temporal::Beats const & beat, int notenum);
	~Step ();

	size_t index() const { return _index; }

	void set_note (double note, double velocity = 0.5, int n = 0);
	void set_chord (size_t note_cnt, double* notes);
	void set_parameter (int number, double value, int n = 0);

	void adjust_velocity (int amt);
	void adjust_pitch (int amt);
	void adjust_duration (DurationRatio const & amt);
	void adjust_octave (int amt);
	void adjust_offset (double fraction);

	Mode mode() const { return _mode; }
	void set_mode (Mode m);

	double note (size_t n = 0) const { return _notes[n].number; }
	double velocity (size_t n = 0) const { return _notes[n].velocity; }
	void set_velocity (double, size_t n = 0);

	DurationRatio duration () const { return _duration; }
	void set_duration (DurationRatio const &);

	void set_offset (Temporal::Beats const &, size_t n = 0);
	Temporal::Beats offset (size_t n = 0) const { return _notes[n].offset; }

	int parameter (size_t n = 0) const { return _parameters[n].parameter; }
	int parameter_value (size_t n = 0) const { return _parameters[n].value; }

	void set_enabled (bool);
	bool enabled() const { return _enabled; }

	void set_repeat (size_t r);
	size_t repeat() const { return _repeat; }

	void set_beat (Temporal::Beats const & beat);
	Temporal::Beats beat () const { return _nominal_beat; }

	bool run (MidiBuffer& buf, bool running, samplepos_t, samplepos_t, MidiNoteTracker&);

	bool skipped() const { return _skipped; }
	void set_skipped (bool);

	void reschedule (Temporal::Beats const &, Temporal::Beats const &);

	int octave_shift() const { return _octave_shift; }
	void set_octave_shift (int);

	XMLNode& get_state();
	int set_state (XMLNode const &, int);

	void dump (MusicTimeEvents&, Temporal::Beats const&) const;

	static const int _notes_per_step = 5;
	static const int _parameters_per_step = 5;

  private:
	friend class StepSequence; /* HACK */

	StepSequence&      _sequence;
	size_t             _index;
	bool               _enabled;
	Temporal::Beats    _nominal_beat;
	Temporal::Beats    _scheduled_beat;
	bool               _skipped;
	Mode               _mode;
	int                _octave_shift;
	DurationRatio      _duration;

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
		Temporal::Beats offset;
		MIDI::byte off_msg[3];

		Note () : number (-1), velocity (0.0) {}
		Note (double n, double v,Temporal::Beats const & o) : number (n), velocity (v), offset (o) {}
	};

	Note _notes[_notes_per_step];
	ParameterValue _parameters[_parameters_per_step];
	size_t _repeat;

	void check_note (size_t n, MidiBuffer& buf, bool, samplepos_t, samplepos_t, MidiNoteTracker&);
	void check_parameter (size_t n, MidiBuffer& buf, bool, samplepos_t, samplepos_t);
	void dump_note (MusicTimeEvents&, size_t n, Temporal::Beats const &) const;
	void dump_parameter (MusicTimeEvents&, size_t n, Temporal::Beats const &) const;

	StepSequencer& sequencer() const;
};

class StepSequence : public PBD::Stateful
{
  public:
	enum Direction {
		forwards = 0,
		backwards = 1,
		end_to_end = 2,
		rd_random = 3
	};

	StepSequence (StepSequencer &myseq, size_t index, size_t nsteps, Temporal::Beats const & step_size, Temporal::Beats const & bar_size, int notenum);
	~StepSequence ();

	size_t index() const { return _index; }
	size_t nsteps() const { return _steps.size(); }

	Step& step (size_t n) const;

	void startup (Temporal::Beats const & start, Temporal::Beats const & offset);

	int root() const { return _root; }
	void set_root (int n);

	int channel() const { return _channel; }
	void set_channel (int);

	Temporal::Beats wrap (Temporal::Beats const &) const;

	MusicalMode mode() const { return _mode; }
	void set_mode (MusicalMode m);

	void shift_left (size_t n = 1);
	void shift_right (size_t n = 1);

	void reset ();
	void reschedule (Temporal::Beats const &, Temporal::Beats const &);
	void schedule (Temporal::Beats const &);

	bool run (MidiBuffer& buf, bool running, samplepos_t, samplepos_t, MidiNoteTracker&);

	StepSequencer& sequencer() const { return _sequencer; }

	XMLNode& get_state();
	int set_state (XMLNode const &, int);

	void dump (MusicTimeEvents&, Temporal::Beats const &) const;

  private:
	StepSequencer& _sequencer;
	int         _index;
	mutable Glib::Threads::Mutex _step_lock;
	typedef std::vector<Step*> Steps;

	Steps       _steps;
	int         _channel; /* MIDI channel */
	int         _root;
	MusicalMode _mode;
};

class StepSequencer : public PBD::Stateful
{
  public:
	StepSequencer (TempoMap&, size_t nseqs, size_t nsteps, Temporal::Beats const & step_size, Temporal::Beats const & bar_size, int notenum);
	~StepSequencer ();

	size_t step_capacity() const { return _step_capacity; }
	size_t nsteps() const { return _end_step - _start_step; }
	size_t nsequences() const { return _sequences.size(); }

	int last_step() const;

	StepSequence& sequence (size_t n) const;

	Temporal::Beats duration() const;

	Temporal::Beats step_size () const { return _step_size; }
	void set_step_size (Temporal::Beats const &);

	void set_start_step (size_t);
	void set_end_step (size_t);

	size_t start_step() const { return _start_step; }
	size_t end_step() const { return _end_step; }

	void sync ();        /* return all rows to start step */
	void reset ();       /* return entire state to default */

	bool run (MidiBuffer& buf, samplepos_t, samplepos_t, double, pframes_t, bool);

	TempoMap& tempo_map() const { return _tempo_map; }

	XMLNode& get_state();
	int set_state (XMLNode const &, int);

	void queue_note_off (Temporal::Beats const &, uint8_t note, uint8_t velocity, uint8_t channel);

	boost::shared_ptr<Source> write_to_source (Session& s, std::string p = std::string()) const;

  private:
	mutable Glib::Threads::Mutex       _sequence_lock;
	TempoMap&       _tempo_map;

	typedef std::vector<StepSequence*> StepSequences;
	StepSequences  _sequences;

	Temporal::Beats _last_startup; /* last time we started running */
	size_t          _last_step;  /* last step that we executed */
	Temporal::Beats _step_size;
	size_t          _start_step;
	size_t          _end_step;
	samplepos_t     last_start;
	samplepos_t     last_end;   /* end sample time of last run() call */
	bool            _running;
	size_t          _step_capacity;

	ARDOUR::MidiNoteTracker outbound_tracker;

	struct Request {

		/* bitwise types, so we can combine multiple in one
		 */

		enum Type {
			SetStartStep = 0x1,
			SetEndStep = 0x2,
			SetNSequences = 0x4,
			SetStepSize = 0x8,
		};

		Type type;

		Temporal::Beats step_size;
		size_t          nsequences;
		size_t          start_step;
		size_t          end_step;

		static MultiAllocSingleReleasePool pool;

		void *operator new (size_t) {
			return pool.alloc ();
		}

		void operator delete (void* ptr, size_t /* size */) {
			pool.release (ptr);
		}
	};

	PBD::RingBuffer<Request*> requests;
	bool check_requests ();
	Temporal::Beats reschedule (samplepos_t);

	struct NoteOffBlob : public boost::intrusive::list_base_hook<> {

		NoteOffBlob (Temporal::Beats const & w, uint8_t n, uint8_t v, uint8_t c)
			: when (w) { buf[0] = 0x80|c; buf[1] = n; buf[2] = v; }

		Temporal::Beats when;
		uint8_t buf[3];

		static Pool pool;

		void *operator new (size_t) {
			return pool.alloc ();
		}

		void operator delete (void* ptr, size_t /* size */) {
			pool.release (ptr);
		}

		bool operator< (NoteOffBlob const & other) const {
			return when < other.when;
		}
	};

	typedef boost::intrusive::list<NoteOffBlob> NoteOffList;

	NoteOffList note_offs;
	void check_note_offs (ARDOUR::MidiBuffer&, samplepos_t start_sample, samplepos_t last_sample);
	void clear_note_offs ();

	bool fill_midi_source (boost::shared_ptr<SMFSource> src) const;

};

} /* namespace */

#endif /* __libardour_step_sequencer_h__ */
