/*
 * Copyright (C) 2015-2017 Robin Gareus <robin@gareus.org>
 * Copyright (C) 2015 Tim Mayberry <mojofunk@gmail.com>
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

#ifndef WINMME_MIDI_IO_H
#define WINMME_MIDI_IO_H

#include <map>
#include <vector>
#include <string>
#include <stdint.h>
#include <pthread.h>

#include <boost/shared_ptr.hpp>
#include "pbd/ringbuffer.h"

#include "winmmemidi_input_device.h"
#include "winmmemidi_output_device.h"

#include "midi_device_info.h"

namespace ARDOUR {

struct WinMMEMIDIPacket {

#if 0
	WinMMEMIDIPacket (const WinMMEMIDIPacket& other)
	    : timeStamp (other.timeStamp)
	    , length (other.length)
	{
		if (length > 0) {
			memcpy (data, other.data, length);
		}
	}
#endif

	// MIDITimeStamp timeStamp;
	uint16_t length;
	uint8_t data[MaxWinMidiEventSize];
};

typedef std::vector<boost::shared_ptr<WinMMEMIDIPacket> > WinMMEMIDIQueue;

class WinMMEMidiIO {
public:
	WinMMEMidiIO ();
	~WinMMEMidiIO ();

	void start ();
	void stop ();

	bool dequeue_input_event (uint32_t port,
	                          uint64_t timestamp_start,
	                          uint64_t timestamp_end,
	                          uint64_t& timestamp,
	                          uint8_t* data,
	                          size_t& size);

	bool enqueue_output_event (uint32_t port,
	                           uint64_t timestamp,
	                           const uint8_t* data,
	                           const size_t size);

	uint32_t n_midi_inputs (void) const { return m_inputs.size(); }
	uint32_t n_midi_outputs (void) const { return m_outputs.size(); }

	std::vector<WinMMEMidiInputDevice*> get_inputs () { return m_inputs; }
	std::vector<WinMMEMidiOutputDevice*> get_outputs () { return m_outputs; }

	void update_device_info ();

	std::vector<MidiDeviceInfo*> get_device_info () { return m_device_info; }

	MidiDeviceInfo* get_device_info (const std::string& name);

	std::string port_id (uint32_t, bool input);
	std::string port_name (uint32_t, bool input);

	void set_enabled (bool yn = true) { m_enabled = yn; }
	bool enabled (void) const { return m_active && m_enabled; }

	void set_port_changed_callback (void (changed_callback (void*)), void *arg) {
		m_changed_callback = changed_callback;
		m_changed_arg = arg;
	}

private: // Methods

	void clear_device_info ();

	static bool get_input_name_from_index (int index, std::string& name);
	static bool get_output_name_from_index (int index, std::string& name);

	void discover ();
	void cleanup ();

	void create_input_devices ();
	void create_output_devices ();

	void destroy_input_devices ();
	void destroy_output_devices ();

	void start_devices ();
	void stop_devices ();

private: // Data

	std::vector<MidiDeviceInfo*> m_device_info;

	std::vector<WinMMEMidiInputDevice*> m_inputs;
	std::vector<WinMMEMidiOutputDevice*> m_outputs;

	bool              m_active;
	bool              m_enabled;
	bool              m_run;

	void (* m_changed_callback) (void*);
	void  * m_changed_arg;

	// protects access to m_inputs and m_outputs
	pthread_mutex_t m_device_lock;
};

} // namespace

#endif // WINMME_MIDI_IO_H

