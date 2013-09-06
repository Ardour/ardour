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

namespace Evoral {
	template class EventRingBuffer<timestamp_t>;
}

pthread_t AsyncMIDIPort::_process_thread;

#define port_engine AudioEngine::instance()->port_engine()

AsyncMIDIPort::AsyncMIDIPort (string const & name, PortFlags flags)
	: MidiPort (name, flags)
	, MIDI::Port (name, MIDI::Port::Flags (0))
	, _currently_in_cycle (false)
	, _last_write_timestamp (0)
	, output_fifo (512)
	, input_fifo (1024)
	, xthread (true)
{
}

AsyncMIDIPort::~AsyncMIDIPort ()
{
}

void
AsyncMIDIPort::flush_output_fifo (pframes_t nframes)
{
	RingBuffer< Evoral::Event<double> >::rw_vector vec = { { 0, 0 }, { 0, 0 } };
	size_t written;

	output_fifo.get_read_vector (&vec);

	MidiBuffer& mb (get_midi_buffer (nframes));
			
	if (vec.len[0]) {
		Evoral::Event<double>* evp = vec.buf[0];
		
		for (size_t n = 0; n < vec.len[0]; ++n, ++evp) {
			mb.push_back (evp->time(), evp->size(), evp->buffer());
		}
	}
	
	if (vec.len[1]) {
		Evoral::Event<double>* evp = vec.buf[1];

		for (size_t n = 0; n < vec.len[1]; ++n, ++evp) {
			mb.push_back (evp->time(), evp->size(), evp->buffer());
		}
	}
	
	if ((written = vec.len[0] + vec.len[1]) != 0) {
		output_fifo.increment_read_idx (written);
	}
}

void
AsyncMIDIPort::cycle_start (pframes_t nframes)
{
	_currently_in_cycle = true;
	MidiPort::cycle_start (nframes);

	/* dump anything waiting in the output FIFO at the start of the port
	 * buffer
	 */

	if (ARDOUR::Port::sends_output()) {
		flush_output_fifo (nframes);
	} 
	
	/* copy incoming data from the port buffer into the input FIFO
	   and if necessary wakeup the reader
	*/

	if (ARDOUR::Port::receives_input()) {
		MidiBuffer& mb (get_midi_buffer (nframes));
		pframes_t when = AudioEngine::instance()->sample_time_at_cycle_start();

		for (MidiBuffer::iterator b = mb.begin(); b != mb.end(); ++b) {
			input_fifo.write (when, (Evoral::EventType) 0, (*b).size(), (*b).buffer());
		}
		
		if (!mb.empty()) {
			xthread.wakeup ();
		}
	}

}

void
AsyncMIDIPort::cycle_end (pframes_t nframes)
{
	if (ARDOUR::Port::sends_output()) {
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
AsyncMIDIPort::drain (int check_interval_usecs)
{
	RingBuffer< Evoral::Event<double> >::rw_vector vec = { { 0, 0 }, { 0, 0} };

	if (!AudioEngine::instance()->running() || AudioEngine::instance()->session() == 0) {
		/* no more process calls - it will never drain */
		return;
	}


	if (is_process_thread()) {
		error << "Process thread called MIDI::AsyncMIDIPort::drain() - this cannot work" << endmsg;
		return;
	}

	while (1) {
		output_fifo.get_write_vector (&vec);
		if (vec.len[0] + vec.len[1] >= output_fifo.bufsize() - 1) {
			break;
		}
		usleep (check_interval_usecs);
	}
}

int
AsyncMIDIPort::write (const byte * msg, size_t msglen, timestamp_t timestamp)
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
		RingBuffer< Evoral::Event<double> >::rw_vector vec = { { 0, 0 }, { 0, 0} };
		
		output_fifo.get_write_vector (&vec);

		if (vec.len[0] + vec.len[1] < 1) {
			error << "no space in FIFO for non-process thread MIDI write" << endmsg;
			return 0;
		}

		if (vec.len[0]) {
                        if (!vec.buf[0]->owns_buffer()) {
                                vec.buf[0]->set_buffer (0, 0, true);
                        }
			vec.buf[0]->set (msg, msglen, timestamp);
		} else {
                        if (!vec.buf[1]->owns_buffer()) {
                                vec.buf[1]->set_buffer (0, 0, true);
                        }
			vec.buf[1]->set (msg, msglen, timestamp);
		}

		output_fifo.increment_write_idx (1);
		
		ret = msglen;

	} else {

		_parser->set_timestamp (AudioEngine::instance()->sample_time_at_cycle_start() + timestamp);
		for (size_t n = 0; n < msglen; ++n) {
			_parser->scanner (msg[n]);
		}

		if (timestamp >= _cycle_nframes) {
			std::cerr << "attempting to write MIDI event of " << msglen << " bytes at time "
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
	byte buffer[input_fifo.capacity()];

	while (input_fifo.read (&time, &type, &size, buffer)) {
		_parser->set_timestamp (time);
		for (uint32_t i = 0; i < size; ++i) {
			_parser->scanner (buffer[i]);
		}
	}

	return 0;
}

void
AsyncMIDIPort::parse (framecnt_t)
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

