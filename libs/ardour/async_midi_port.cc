/*
    Copyright (C) 1998 Paul Barton-Davis

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

    $Id$
*/

#include <iostream>
#include <vector>

#include <glibmm/timer.h>

#include "pbd/error.h"
#include "pbd/stacktrace.h"

#include "midi++/types.h"

#include "ardour/async_midi_port.h"
#include "ardour/audioengine.h"
#include "ardour/midi_buffer.h"

using namespace MIDI;
using namespace ARDOUR;
using namespace std;
using namespace PBD;

pthread_t AsyncMIDIPort::_process_thread;

#define port_engine AudioEngine::instance()->port_engine()

AsyncMIDIPort::AsyncMIDIPort (string const & name, PortFlags flags)
	: MidiPort (name, flags)
	, MIDI::Port (name, MIDI::Port::Flags (0))
	, _currently_in_cycle (false)
	, _last_write_timestamp (0)
	, _flush_at_cycle_start (false)
	, have_timer (false)
	, output_fifo (2048)
	, input_fifo (1024)
	, _xthread (true)
{
}

AsyncMIDIPort::~AsyncMIDIPort ()
{
}

void
AsyncMIDIPort::set_timer (boost::function<MIDI::framecnt_t (void)>& f)
{
	timer = f;
	have_timer = true;
}

void
AsyncMIDIPort::flush_output_fifo (MIDI::pframes_t nframes)
{
	timestamp_t time;
	Evoral::EventType type;
	uint32_t size;
	vector<MIDI::byte> buffer (output_fifo.capacity());

	MidiBuffer& mb (get_midi_buffer (nframes));

	while (output_fifo.read (&time, &type, &size, &buffer[0])) {
		mb.push_back (time, size, buffer.data());
	}
}

void
AsyncMIDIPort::cycle_start (MIDI::pframes_t nframes)
{
	_currently_in_cycle = true;
	MidiPort::cycle_start (nframes);

	/* dump anything waiting in the output FIFO at the start of the port
	 * buffer
	 */

	if (ARDOUR::Port::sends_output()) {
		flush_output_fifo (nframes);
		if (_flush_at_cycle_start) {
			flush_buffers (nframes);
		}
	}

	/* copy incoming data from the port buffer into the input FIFO
	   and if necessary wakeup the reader
	*/

	if (ARDOUR::Port::receives_input()) {
		MidiBuffer& mb (get_midi_buffer (nframes));
		framecnt_t when;

		if (have_timer) {
			when = timer ();
		} else {
			when = AudioEngine::instance()->sample_time_at_cycle_start();
		}

		for (MidiBuffer::iterator b = mb.begin(); b != mb.end(); ++b) {
			Evoral::Event<MidiBuffer::TimeType>* ev (*b);
			if (!have_timer) {
				when += ev->time();
			}
			input_fifo.write (when, Evoral::MIDI_EVENT, ev->size(), ev->buffer());
		}

		if (!mb.empty()) {
			_xthread.wakeup ();
		}

	}
}

void
AsyncMIDIPort::cycle_end (MIDI::pframes_t nframes)
{
	if (ARDOUR::Port::sends_output() && !_flush_at_cycle_start) {
		/* move any additional data from output FIFO into the port
		   buffer.
		*/
		flush_output_fifo (nframes);
	}

	MidiPort::cycle_end (nframes);

	_currently_in_cycle = false;
}

/** wait for the output FIFO to be emptied by successive process() callbacks.
 *
 * Cannot be called from a processing thread.
 */
void
AsyncMIDIPort::drain (int check_interval_usecs, int total_usecs_to_wait)
{
	if (!AudioEngine::instance()->running() || AudioEngine::instance()->session() == 0) {
		/* no more process calls - it will never drain */
		return;
	}


	if (is_process_thread()) {
		error << "Process thread called MIDI::AsyncMIDIPort::drain() - this cannot work" << endmsg;
		return;
	}

	microseconds_t now = get_microseconds ();
	microseconds_t end = now + total_usecs_to_wait;

	while (now < end) {
		if (output_fifo.write_space() >= output_fifo.bufsize() - 1) {
			/* ringbuffer is now empty XXX should be a method for this */
			break;
		}
		Glib::usleep (check_interval_usecs);
		now = get_microseconds();
	}
}

int
AsyncMIDIPort::write (const MIDI::byte * msg, size_t msglen, MIDI::timestamp_t timestamp)
{
	int ret = 0;

	if (!ARDOUR::Port::sends_output()) {
		return ret;
	}

	if (!is_process_thread()) {

		/* this is the best estimate of "when" this MIDI data is being
		 * delivered
		 */

		_parser->set_timestamp (AudioEngine::instance()->sample_time() + timestamp);
		for (size_t n = 0; n < msglen; ++n) {
			_parser->scanner (msg[n]);
		}

		Glib::Threads::Mutex::Lock lm (output_fifo_lock);

		if (output_fifo.write (timestamp, Evoral::MIDI_EVENT, msglen, msg) != msglen) {
			error << "no space in FIFO for non-process thread MIDI write" << endmsg;
			ret = 0;
		} else {
			ret = msglen;
		}

	} else {

		_parser->set_timestamp (AudioEngine::instance()->sample_time_at_cycle_start() + timestamp);
		for (size_t n = 0; n < msglen; ++n) {
			_parser->scanner (msg[n]);
		}

		if (timestamp >= _cycle_nframes) {
			std::cerr << "attempting to write MIDI event of " << msglen << " MIDI::bytes at time "
				  << timestamp << " of " << _cycle_nframes
				  << " (this will not work - needs a code fix)"
				  << std::endl;
		}

		/* This is the process thread, which makes checking
		 * _currently_in_cycle atomic and safe, since it is only
		 * set from cycle_start() and cycle_end(), also called
		 * only from the process thread.
		 */

		if (_currently_in_cycle) {

			MidiBuffer& mb (get_midi_buffer (_cycle_nframes));

			if (timestamp == 0) {
				timestamp = _last_write_timestamp;
			}

			if (mb.push_back (timestamp, msglen, msg)) {
				ret = msglen;
				_last_write_timestamp = timestamp;

			} else {
				cerr << "AsyncMIDIPort (" << ARDOUR::Port::name() << "): write of " << msglen << " @ " << timestamp << " failed\n" << endl;
				PBD::stacktrace (cerr, 20);
				ret = 0;
			}
		} else {
			cerr << "write to JACK midi port failed: not currently in a process cycle." << endl;
			PBD::stacktrace (cerr, 20);
		}
	}

	return ret;
}


int
AsyncMIDIPort::read (MIDI::byte *, size_t)
{
	if (!ARDOUR::Port::receives_input()) {
		return 0;
	}

	timestamp_t time;
	Evoral::EventType type;
	uint32_t size;
	vector<MIDI::byte> buffer(input_fifo.capacity());

	while (input_fifo.read (&time, &type, &size, &buffer[0])) {
		_parser->set_timestamp (time);
		for (uint32_t i = 0; i < size; ++i) {
			_parser->scanner (buffer[i]);
		}
	}

	return 0;
}

void
AsyncMIDIPort::parse (MIDI::framecnt_t)
{
	MIDI::byte buf[1];

	/* see ::read() to realize why buf is not used */
	read (buf, sizeof (buf));
}

void
AsyncMIDIPort::set_process_thread (pthread_t thr)
{
	_process_thread = thr;
}

bool
AsyncMIDIPort::is_process_thread()
{
	return pthread_equal (pthread_self(), _process_thread);
}
