/*
 * Copyright (C) 2015-2017 Robin Gareus <robin@gareus.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#ifndef __libbackend_coremidi_io_h__
#define __libbackend_coremidi_io_h__

#include <CoreServices/CoreServices.h>
#include <CoreAudio/CoreAudio.h>
#include <AudioUnit/AudioUnit.h>

#include <AudioUnit/AudioUnit.h>
#include <AudioToolbox/AudioToolbox.h>

#include <map>
#include <vector>
#include <string>

#include <boost/shared_ptr.hpp>
#include "pbd/ringbuffer.h"

namespace ARDOUR {

typedef struct _CoreMIDIPacket {
	MIDITimeStamp timeStamp;
	UInt16 length;
	Byte data[256];
#if 0 // unused
	_CoreMIDIPacket (MIDITimeStamp t, Byte *d, UInt16 l)
		: timeStamp(t)
		, length (l)
	{
		if (l > 256) {
			length = 256;
		}
		if (length > 0) {
			memcpy(data, d, length);
		}
	}
#endif
	_CoreMIDIPacket (const MIDIPacket *other)
		: timeStamp(other->timeStamp)
		, length (other->length)
	{
		if (length > 0) {
			memcpy(data, other->data, length);
		}
	}
} CoreMIDIPacket;

typedef std::vector<boost::shared_ptr<CoreMIDIPacket> > CoreMIDIQueue;

class CoreMidiIo {
public:
	CoreMidiIo (void);
	~CoreMidiIo (void);

	void start ();
	void stop ();

	void start_cycle ();

	int send_event (uint32_t, double, const uint8_t *, const size_t);
	int send_events (uint32_t, double, const void *);
	size_t recv_event (uint32_t, double, uint64_t &, uint8_t *, size_t &);

	uint32_t n_midi_inputs (void) const { return _n_midi_in; }
	uint32_t n_midi_outputs (void) const { return _n_midi_out; }
	std::string port_id (uint32_t, bool input);
	std::string port_name (uint32_t, bool input);

	void notify_proc (const MIDINotification *message);

	void set_enabled (bool yn = true) { _enabled = yn; }
	bool enabled (void) const { return _active && _enabled; }

	void set_port_changed_callback (void (changed_callback (void*)), void *arg) {
		_changed_callback = changed_callback;
		_changed_arg = arg;
	}

private:
	void discover ();
	void cleanup ();

	MIDIClientRef     _midi_client;
	MIDIEndpointRef * _input_endpoints;
	MIDIEndpointRef * _output_endpoints;
	MIDIPortRef     * _input_ports;
	MIDIPortRef     * _output_ports;
	CoreMIDIQueue   * _input_queue;

	PBD::RingBuffer<uint8_t> ** _rb;

	uint32_t          _n_midi_in;
	uint32_t          _n_midi_out;

	MIDITimeStamp     _time_at_cycle_start;
	bool              _active; // internal deactivate during discovery etc
	bool              _enabled; // temporary disable, e.g. during freewheeli
	bool              _run; // general status

	void (* _changed_callback) (void*);
	void  * _changed_arg;

	pthread_mutex_t _discovery_lock;
};

} // namespace

#endif /* __libbackend_coremidi_io */
