/*
    Copyright (C) 2006 Paul Davis

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

#include <cassert>
#include <iostream>

#include "pbd/compose.h"
#include "pbd/debug.h"

#include "ardour/audioengine.h"
#include "ardour/data_type.h"
#include "ardour/debug.h"
#include "ardour/midi_buffer.h"
#include "ardour/midi_port.h"
#include "ardour/session.h"

using namespace std;
using namespace ARDOUR;
using namespace PBD;

#define port_engine AudioEngine::instance()->port_engine()

MidiPort::MidiPort (const std::string& name, PortFlags flags)
	: Port (name, DataType::MIDI, flags)
	, _has_been_mixed_down (false)
	, _resolve_required (false)
	, _input_active (true)
	, _always_parse (false)
	, _trace_on (false)
{
	_buffer = new MidiBuffer (AudioEngine::instance()->raw_buffer_size (DataType::MIDI));
}

MidiPort::~MidiPort()
{
	if (_shadow_port) {
		AudioEngine::instance()->unregister_port (_shadow_port);
		_shadow_port.reset ();
	}

	delete _buffer;
}

void
MidiPort::cycle_start (pframes_t nframes)
{
	samplepos_t now = AudioEngine::instance()->sample_time_at_cycle_start();

	Port::cycle_start (nframes);

	_buffer->clear ();

	if (sends_output () && _port_handle) {
		port_engine.midi_clear (port_engine.get_buffer (_port_handle, nframes));
	}

	if (_always_parse || (receives_input() && _trace_on)) {
		MidiBuffer& mb (get_midi_buffer (nframes));

		/* dump incoming MIDI to parser */

		for (MidiBuffer::iterator b = mb.begin(); b != mb.end(); ++b) {
			uint8_t* buf = (*b).buffer();

			_self_parser.set_timestamp (now + (*b).time());

			uint32_t limit = (*b).size();

			for (size_t n = 0; n < limit; ++n) {
				_self_parser.scanner (buf[n]);
			}
		}
	}

	if (inbound_midi_filter) {
		MidiBuffer& mb (get_midi_buffer (nframes));
		inbound_midi_filter (mb, mb);
	}

	if (_shadow_port) {
		MidiBuffer& mb (get_midi_buffer (nframes));
		if (shadow_midi_filter (mb, _shadow_port->get_midi_buffer (nframes))) {
			_shadow_port->flush_buffers (nframes);
		}
	}

}

Buffer&
MidiPort::get_buffer (pframes_t nframes)
{
	return get_midi_buffer (nframes);
}

MidiBuffer &
MidiPort::get_midi_buffer (pframes_t nframes)
{
	if (_has_been_mixed_down) {
		return *_buffer;
	}

	if (receives_input ()) {

		if (_input_active) {

			void* buffer = port_engine.get_buffer (_port_handle, nframes);
			const pframes_t event_count = port_engine.get_midi_event_count (buffer);

			/* suck all relevant MIDI events from the MIDI port buffer
			   into our MidiBuffer
			*/

			for (pframes_t i = 0; i < event_count; ++i) {

				pframes_t timestamp;
				size_t size;
				uint8_t const* buf;

				port_engine.midi_event_get (timestamp, size, &buf, buffer, i);

				if (buf[0] == 0xfe) {
					/* throw away active sensing */
					continue;
				}

				timestamp = floor (timestamp * _speed_ratio);

				/* check that the event is in the acceptable time range */
				if ((timestamp <  (_global_port_buffer_offset)) ||
				    (timestamp >= (_global_port_buffer_offset + nframes))) {
					// XXX this is normal after a split cycles:
					// The engine buffer contains the data for the complete cycle, but
					// only the part after _global_port_buffer_offset is needed.
#ifndef NDEBUG
					cerr << "Dropping incoming MIDI at time " << timestamp << "; offset="
						<< _global_port_buffer_offset << " limit="
						<< (_global_port_buffer_offset + nframes)
						<< " = (" << _global_port_buffer_offset
						<< " + " << nframes
						<< ")\n";
#endif
					continue;
				}

				/* adjust timestamp to match current cycle */
				timestamp -= _global_port_buffer_offset;
				assert (timestamp < nframes);

				if ((buf[0] & 0xF0) == 0x90 && buf[2] == 0) {
					/* normalize note on with velocity 0 to proper note off */
					uint8_t ev[3];
					ev[0] = 0x80 | (buf[0] & 0x0F);  /* note off */
					ev[1] = buf[1];
					ev[2] = 0x40;  /* default velocity */
					_buffer->push_back (timestamp, size, ev);
				} else {
					_buffer->push_back (timestamp, size, buf);
				}
			}

		} else {
			_buffer->silence (nframes);
		}

	} else {
		_buffer->silence (nframes);
	}

	if (nframes) {
		_has_been_mixed_down = true;
	}

	return *_buffer;
}

void
MidiPort::cycle_end (pframes_t /*nframes*/)
{
	_has_been_mixed_down = false;
}

void
MidiPort::cycle_split ()
{
	_has_been_mixed_down = false;
}

void
MidiPort::resolve_notes (void* port_buffer, MidiBuffer::TimeType when)
{
	for (uint8_t channel = 0; channel <= 0xF; channel++) {

		uint8_t ev[3] = { ((uint8_t) (MIDI_CMD_CONTROL | channel)), MIDI_CTL_SUSTAIN, 0 };
		pframes_t tme = floor (when / _speed_ratio);

		/* we need to send all notes off AND turn the
		 * sustain/damper pedal off to handle synths
		 * that prioritize sustain over AllNotesOff
		 */

		if (port_engine.midi_event_put (port_buffer, tme, ev, 3) != 0) {
			cerr << "failed to deliver sustain-zero on channel " << (int)channel << " on port " << name() << endl;
		}

		ev[1] = MIDI_CTL_ALL_NOTES_OFF;

		if (port_engine.midi_event_put (port_buffer, tme, ev, 3) != 0) {
			cerr << "failed to deliver ALL NOTES OFF on channel " << (int)channel << " on port " << name() << endl;
		}
	}
}

void
MidiPort::flush_buffers (pframes_t nframes)
{
	if (sends_output ()) {

		void* port_buffer = 0;

		if (_resolve_required) {
			port_buffer = port_engine.get_buffer (_port_handle, nframes);
			/* resolve all notes at the start of the buffer */
			resolve_notes (port_buffer, _global_port_buffer_offset);
			_resolve_required = false;
		}

		if (_buffer->empty()) {
			return;
		}

		if (!port_buffer) {
			port_buffer = port_engine.get_buffer (_port_handle, nframes);
		}


		for (MidiBuffer::iterator i = _buffer->begin(); i != _buffer->end(); ++i) {

			const Evoral::Event<MidiBuffer::TimeType> ev (*i, false);


			if (sends_output() && _trace_on) {
				uint8_t const * const buf = ev.buffer();
				const samplepos_t now = AudioEngine::instance()->sample_time_at_cycle_start();

				_self_parser.set_timestamp (now + ev.time());

				uint32_t limit = ev.size();

				for (size_t n = 0; n < limit; ++n) {
					_self_parser.scanner (buf[n]);
				}
			}


			// event times are in samples, relative to cycle start

#ifndef NDEBUG
			if (DEBUG_ENABLED (DEBUG::MidiIO)) {
				const Session* s = AudioEngine::instance()->session();
				const samplepos_t now = (s ? s->transport_sample() : 0);
				DEBUG_STR_DECL(a);
				DEBUG_STR_APPEND(a, string_compose ("MidiPort %7 %1 pop event    @ %2 (global %4, within %5 gpbo %6 sz %3 ", _buffer, ev.time(), ev.size(),
				                                    now + ev.time(), nframes, _global_port_buffer_offset, name()));
				for (size_t i=0; i < ev.size(); ++i) {
					DEBUG_STR_APPEND(a,hex);
					DEBUG_STR_APPEND(a,"0x");
					DEBUG_STR_APPEND(a,(int)(ev.buffer()[i]));
					DEBUG_STR_APPEND(a,' ');
				}
				DEBUG_STR_APPEND(a,'\n');
				DEBUG_TRACE (DEBUG::MidiIO, DEBUG_STR(a).str());
			}
#endif

			assert (ev.time() < (nframes + _global_port_buffer_offset));

			if (ev.time() >= _global_port_buffer_offset) {
				pframes_t tme = floor (ev.time() / _speed_ratio);
				if (port_engine.midi_event_put (port_buffer, tme, ev.buffer(), ev.size()) != 0) {
					cerr << "write failed, dropped event, time "
					     << ev.time()
							 << " > " << _global_port_buffer_offset << endl;
				}
			} else {
				cerr << "drop flushed event on the floor, time " << ev.time()
				     << " too early for " << _global_port_buffer_offset;
				for (size_t xx = 0; xx < ev.size(); ++xx) {
					cerr << ' ' << hex << (int) ev.buffer()[xx];
				}
				cerr << dec << endl;
			}
		}

		/* done.. the data has moved to the port buffer, mark it so
		 */

		_buffer->clear ();
	}
}

void
MidiPort::require_resolve ()
{
	_resolve_required = true;
}

void
MidiPort::transport_stopped ()
{
	_resolve_required = true;
}

void
MidiPort::realtime_locate ()
{
	_resolve_required = true;
}

void
MidiPort::reset ()
{
	Port::reset ();
	delete _buffer;
	cerr << name() << " new MIDI buffer of size " << AudioEngine::instance()->raw_buffer_size (DataType::MIDI) << endl;
	_buffer = new MidiBuffer (AudioEngine::instance()->raw_buffer_size (DataType::MIDI));
}

void
MidiPort::set_input_active (bool yn)
{
	_input_active = yn;
}

void
MidiPort::set_always_parse (bool yn)
{
	_always_parse = yn;
}

void
MidiPort::set_trace_on (bool yn)
{
	_trace_on = yn;
}

int
MidiPort::add_shadow_port (string const & name, MidiFilter mf)
{
	if (!ARDOUR::Port::receives_input()) {
		return -1;
	}

	if (_shadow_port) {
		return -2;
	}

	shadow_midi_filter = mf;

	if (!(_shadow_port = boost::dynamic_pointer_cast<MidiPort> (AudioEngine::instance()->register_output_port (DataType::MIDI, name, false, PortFlags (Shadow|IsTerminal))))) {
		return -3;
	}

	/* forward on our port latency to the shadow port.

	   XXX: need to capture latency changes and forward them too.
	*/

	LatencyRange latency = private_latency_range (false);
	_shadow_port->set_private_latency_range (latency, false);

	return 0;
}
