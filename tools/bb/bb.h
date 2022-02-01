#ifndef __bb_h__
#define __bb_h__

#include <algorithm>
#include <vector>
#include <set>
#include <cstring>

#include <stdint.h>

#include <jack/jack.h>

#include "ardour/midi_state_tracker.h"

typedef uint64_t superclock_t;

static const superclock_t superclock_ticks_per_second = 508032000; // 2^10 * 3^4 * 5^3 * 7^2
inline superclock_t superclock_to_samples (superclock_t s, int sr) { return (s * sr) / superclock_ticks_per_second; }
inline superclock_t samples_to_superclock (int samples, int sr) { return (samples * superclock_ticks_per_second) / sr; }

namespace ARDOUR {
class Session;
}

class BeatBox {
  public:
	BeatBox (int sample_rate);
	~BeatBox ();

	int register_ports (jack_client_t*);
	int process (int nframes);

	bool running() const { return _running || _start_requested; }
	void start ();
	void stop ();
	void clear ();

	void set_measure_count (int measures);
	void set_meter (int beats, int beat_type);
	void set_tempo (float bpm);

	void set_quantize (int divisor);

	float tempo() const { return _tempo; }
	int meter_beats() const { return _meter_beats; }
	int meter_beat_type() const { return _meter_beat_type; }

  private:
	bool _start_requested;
	bool _running;
	int   _measures;
	float _tempo;
	float _tempo_request;
	int   _meter_beats;
	int   _meter_beat_type;
	jack_port_t* _input;
	jack_port_t* _output;
	superclock_t  superclock_cnt;
	superclock_t  last_start;

	int _sample_rate;
	superclock_t whole_note_superclocks;
	superclock_t beat_superclocks;
	superclock_t measure_superclocks;
	int _quantize_divisor;
	bool clear_pending;
	ARDOUR::MidiNoteTracker inbound_tracker;
	ARDOUR::MidiNoteTracker outbound_tracker;

	struct Event {
		superclock_t time;
		superclock_t whole_note_superclocks;
		size_t       size;
		unsigned char  buf[24];

		Event () : time (0), size (0) {}
		Event (jack_nframes_t t, size_t sz, unsigned char* b) : time (t), size (sz) { memcpy (buf, b, std::min (sizeof (buf), sz)); }
		Event (Event const & other) : time (other.time), size (other.size) { memcpy (buf, other.buf, other.size); }
	};

	struct EventComparator {
		bool operator () (Event const * a, Event const * b) const;
	};

	typedef std::vector<Event*> IncompleteNotes;
	IncompleteNotes _incomplete_notes;

	typedef std::set<Event*,EventComparator> Events;
	Events _current_events;

	typedef std::vector<Event*> EventPool;
	EventPool  event_pool;

	void compute_tempo_clocks ();
};


#endif /* __bb_h__ */
