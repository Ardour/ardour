#include <iostream>
#include <cstdio>
#include <cmath>
#include <cstring>

#include <unistd.h>
#include <stdint.h>

#include <jack/jack.h>
#include <jack/midiport.h>

#include "evoral/midi_events.h"

#include "ardour/midi_buffer.h"

#include "bb.h"
#include "gui.h"

using std::cerr;
using std::endl;

using namespace ARDOUR;

BeatBox::BeatBox (int sr)
	: _start_requested (false)
	, _running (false)
	, _measures (2)
	, _tempo (120)
	, _tempo_request (0)
	, _meter_beats (4)
	, _meter_beat_type (4)
	, _input (0)
	, _output (0)
	, superclock_cnt (0)
	, last_start (0)
	, _sample_rate (sr)
	, whole_note_superclocks (0)
	, beat_superclocks (0)
	, measure_superclocks (0)
	, _quantize_divisor (4)
	, clear_pending (false)
{
	for (uint32_t n = 0; n < 1024; ++n) {
		event_pool.push_back (new Event());
	}
}

BeatBox::~BeatBox ()
{
}

int
BeatBox::register_ports (jack_client_t* jack)
{
	if ((_input = jack_port_register (jack, "midi-in", JACK_DEFAULT_MIDI_TYPE, JackPortIsInput, 0)) == 0) {
		cerr << "no input port\n";
		return -1;
	}
	if ((_output = jack_port_register (jack, "midi-out", JACK_DEFAULT_MIDI_TYPE, JackPortIsOutput, 0)) == 0) {
		cerr << "no output port\n";
		jack_port_unregister (jack, _input);
		return -1;
	}

	return 0;
}

void
BeatBox::compute_tempo_clocks ()
{
	whole_note_superclocks = (superclock_ticks_per_second * 60) / (_tempo / _meter_beat_type);
	beat_superclocks = whole_note_superclocks / _meter_beat_type;
	measure_superclocks = beat_superclocks * _meter_beats;
}

void
BeatBox::start ()
{
	/* compute tempo, beat steps etc. */

	compute_tempo_clocks ();

	/* we can start */

	_start_requested = true;
}

void
BeatBox::stop ()
{
	_start_requested = false;
}

void
BeatBox::set_tempo (float bpm)
{
	_tempo_request = bpm;
}

int
BeatBox::process (int nsamples)
{
	if (!_running) {
		if (_start_requested) {
			_running = true;
			last_start = superclock_cnt;
		}

	} else {
		if (!_start_requested) {
			_running = false;
		}
	}

	superclock_t superclocks = samples_to_superclock (nsamples, _sample_rate);

	if (_tempo_request) {
		double ratio = _tempo / _tempo_request;
		_tempo = _tempo_request;
		_tempo_request = 0;

		compute_tempo_clocks ();

		/* recompute all the event times based on the ratio between the
		 * new and old tempo.
		 */

		for (Events::iterator ee = _current_events.begin(); ee != _current_events.end(); ++ee) {
			(*ee)->time = llrintf ((*ee)->time * ratio);
		}
	}

	if (!_running) {
		superclock_cnt += superclocks;
		return 0;
	}

	superclock_t process_start = superclock_cnt - last_start;
	superclock_t process_end = process_start + superclocks;
	const superclock_t loop_length = _measures * measure_superclocks;
	const superclock_t orig_superclocks = superclocks;

	process_start %= loop_length;
	process_end   %= loop_length;

	bool two_pass_required;
	superclock_t offset = 0;

	if (process_end < process_start) {
		two_pass_required = true;
		process_end = loop_length;
		superclocks = process_end - process_start;
	} else {
		two_pass_required = false;
	}

	unsigned char* buffer;
	void* out_buf;
	void* in_buf;
	jack_midi_event_t in_event;
	jack_nframes_t event_index;

	/* do this on the first pass only */
	out_buf = jack_port_get_buffer (_output, nsamples);
	jack_midi_clear_buffer (out_buf);

  second_pass:

	/* Output */

	if (clear_pending) {

		for (Events::iterator ee = _current_events.begin(); ee != _current_events.end(); ++ee) {
			event_pool.push_back (*ee);
		}
		_current_events.clear ();
		_incomplete_notes.clear ();
		clear_pending = false;
	}

	samplepos_t last_output_time = 0;

	for (Events::iterator ee = _current_events.begin(); ee != _current_events.end(); ++ee) {
		Event* e = (*ee);

		if (e->size && (e->time >= process_start && e->time < process_end)) {
			samplepos_t sample_offset_in_buffer = superclock_to_samples (offset + e->time - process_start, _sample_rate);
			if ((buffer = jack_midi_event_reserve (out_buf, sample_offset_in_buffer, e->size)) != 0) {
				memcpy (buffer, e->buf, e->size);
				outbound_tracker.track (e->buf);
				last_output_time = sample_offset_in_buffer;
			} else {
				cerr << "Could not reserve space for output event @ " << e << " of size " << e->size << " @ " << offset + e->time - process_start
				     << " (samples: " << superclock_to_samples (offset + e->time - process_start, _sample_rate) << ") offset is " << offset
				     << ")\n";
			}
		}

		if (e->time >= process_end) {
			break;
		}
	}

	/* input */

	in_buf = jack_port_get_buffer (_input, nsamples);
	event_index = 0;

	Events::iterator loop_iterator;

	while (jack_midi_event_get (&in_event, in_buf, event_index++) == 0) {

		superclock_t event_time = superclock_cnt + samples_to_superclock (in_event.time, _sample_rate);
		superclock_t elapsed_time = event_time - last_start;
		superclock_t in_loop_time = elapsed_time % loop_length;
		superclock_t quantized_time;

		if (_quantize_divisor != 0) {
			const superclock_t time_per_grid_unit = whole_note_superclocks / _quantize_divisor;

			if ((in_event.buffer[0] & 0xf) == MIDI_CMD_NOTE_OFF) {

				/* note off is special - it must be quantized
				 * to at least 1 quantization "spacing" after
				 * the corresponding note on.
				 */

				/* look for the note on */

				IncompleteNotes::iterator ee;

				for (ee = _incomplete_notes.begin(); ee != _incomplete_notes.end(); ++ee) {
					/* check for same note and channel */
					if (((*ee)->buf[1] == in_event.buffer[1]) && ((*ee)->buf[0] & 0xf) == (in_event.buffer[0] & 0xf)) {
						quantized_time = (*ee)->time + time_per_grid_unit;
						_incomplete_notes.erase (ee);
						break;
					}
				}

				if (ee == _incomplete_notes.end()) {
					cerr << "Note off for " << (int) (*ee)->buf[1] << " seen without corresponding note on among " << _incomplete_notes.size() << endl;
					continue;
				}

			} else {
				quantized_time = (in_loop_time / time_per_grid_unit) * time_per_grid_unit;
			}

		} else {
			quantized_time = elapsed_time;
		}

		if (in_event.size > 24) {
			cerr << "Ignored large MIDI event\n";
			continue;
		}

		if (event_pool.empty()) {
			cerr << "No more events, grow pool\n";
			continue;
		}

		Event* e = event_pool.back();
		event_pool.pop_back ();

		e->time = quantized_time;
		e->whole_note_superclocks = whole_note_superclocks;
		e->size = in_event.size;
		memcpy (e->buf, in_event.buffer, in_event.size);

		inbound_tracker.track (e->buf);

		_current_events.insert (e);

		if ((e->buf[0] & 0xf) == MIDI_CMD_NOTE_ON) {
			_incomplete_notes.push_back (e);
		}

		/* play it to out outputs so that we can hear it immediately */
		/* XXX this smooshes together all inbound notes ... tricky */
		if ((buffer = jack_midi_event_reserve (out_buf, last_output_time++, e->size)) != 0) {
			memcpy (buffer, e->buf, e->size);
			outbound_tracker.track (e->buf);
		}
	}

	superclock_cnt += superclocks;

	if (two_pass_required) {
		offset = superclocks;
		superclocks = orig_superclocks - superclocks;
		process_start = 0;
		process_end = superclocks;
		two_pass_required = false;
		goto second_pass;
	}

	return 0;
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

static int
process (jack_nframes_t nsamples, void* arg)
{
	BeatBox* bbox = static_cast<BeatBox*> (arg);
	return bbox->process (nsamples);
}

int
main (int argc, char* argv[])
{
	jack_client_t* jack;
	const char *server_name = NULL;
	jack_options_t options = JackNullOption;
	jack_status_t status;

	if ((jack = jack_client_open ("beatbox", options, &status, server_name)) == 0) {
		cerr << "Could not connect to JACK\n";
		return -1;
	}

	BeatBox* bbox = new BeatBox (jack_get_sample_rate (jack));
	BBGUI* gui = new BBGUI (&argc, &argv, jack, bbox);

	bbox->register_ports (jack);

	jack_set_process_callback (jack, process, bbox);
	jack_activate (jack);

	bbox->start ();

	gui->run ();
}
