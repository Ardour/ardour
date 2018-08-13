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

#ifndef __ardour_beatbox_h__
#define __ardour_beatbox_h__

#include <algorithm>
#include <vector>
#include <set>
#include <cstring>

#include <stdint.h>

#include "pbd/pool.h"
#include "pbd/ringbuffer.h"

#include "temporal/bbt_time.h"

#include "ardour/midi_state_tracker.h"
#include "ardour/processor.h"

namespace ARDOUR {

class Source;
class SMFSource;

typedef uint64_t superclock_t;

static const superclock_t superclock_ticks_per_second = 508032000; // 2^10 * 3^4 * 5^3 * 7^2
inline superclock_t superclock_to_samples (superclock_t s, int sr) { return (s * sr) / superclock_ticks_per_second; }
inline superclock_t samples_to_superclock (int samples, int sr) { return (samples * superclock_ticks_per_second) / sr; }

class BeatBox : public ARDOUR::Processor {
  public:
	BeatBox (ARDOUR::Session& s);
	~BeatBox ();

	void run (BufferSet& /*bufs*/, samplepos_t /*start_frame*/, samplepos_t /*end_frame*/, double speed, pframes_t /*nframes*/, bool /*result_required*/);
	void silence (samplecnt_t nframes, samplepos_t start_frame);
	bool can_support_io_configuration (const ChanCount& in, ChanCount& out);

	bool running() const { return _running || _start_requested; }
	void start ();
	void stop ();
	void clear ();

	Timecode::BBT_Time get_last_time () const;

	void add_note (int number, int velocity, Timecode::BBT_Time at);
	void remove_note (int number, Timecode::BBT_Time at);
	void edit_note_number (int old_number, int new_number);

	void set_measure_count (int measures);
	void set_meter (int beats, int beat_type);
	void set_tempo (float bpm);

	void set_quantize (int divisor);

	float tempo() const { return _tempo; }
	int meter_beats() const { return _meter_beats; }
	int meter_beat_type() const { return _meter_beat_type; }

	XMLNode& state();
	XMLNode& get_state(void);

	bool fill_source (boost::shared_ptr<Source>);

  private:
	bool _start_requested;
	bool _running;
	int   _measures;
	float _tempo;
	float _tempo_request;
	int   _meter_beats;
	int   _meter_beat_type;
	superclock_t  superclock_cnt;
	superclock_t  last_start;
	Timecode::BBT_Time last_time;

	int _sample_rate;
	superclock_t whole_note_superclocks;
	superclock_t beat_superclocks;
	superclock_t measure_superclocks;
	int _quantize_divisor;
	bool clear_pending;
	ARDOUR::MidiStateTracker inbound_tracker;
	ARDOUR::MidiStateTracker outbound_tracker;

	struct Event {
		superclock_t  time;
		superclock_t  whole_note_superclocks;
		size_t        size;
		unsigned char buf[24];
		int           once;

		Event () : time (0), size (0), once (0) {}
		Event (superclock_t t, size_t sz, unsigned char* b) : time (t), size (sz), once (0) { memcpy (buf, b, std::min (sizeof (buf), sz)); }
		Event (Event const & other) : time (other.time), size (other.size), once (0) { memcpy (buf, other.buf, other.size); }

		static MultiAllocSingleReleasePool pool;

		void *operator new (size_t) {
			return pool.alloc ();
		}

		void operator delete (void* ptr, size_t /* size */) {
			pool.release (ptr);
		}
	};

	struct EventComparator {
		bool operator () (Event const * a, Event const * b) const;
	};

	typedef std::vector<Event*> IncompleteNotes;
	IncompleteNotes _incomplete_notes;

	typedef std::set<Event*,EventComparator> Events;
	Events _current_events;

	void compute_tempo_clocks ();

	PBD::RingBuffer<Event*> add_queue;
	PBD::RingBuffer<Event*> remove_queue;

	bool fill_midi_source (boost::shared_ptr<SMFSource>);

};

} /* namespace */

#endif /* __ardour_beatbox_h__ */
