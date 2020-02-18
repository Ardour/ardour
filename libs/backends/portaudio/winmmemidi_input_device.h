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

#ifndef WINMME_MIDI_INPUT_DEVICE_H
#define WINMME_MIDI_INPUT_DEVICE_H

#include <windows.h>
#include <mmsystem.h>

#include <stdint.h>

#include <string>

#include <boost/scoped_array.hpp>
#include <boost/scoped_ptr.hpp>

#include <pbd/ringbuffer.h>

namespace ARDOUR {

class WinMMEMidiInputDevice {
public: // ctors
	WinMMEMidiInputDevice (int index);

	~WinMMEMidiInputDevice ();

public: // methods

	/**
	 * Dequeue events that have accumulated in winmm_input_callback.
	 *
	 * This is called by the audio processing thread/callback to transfer events
	 * into midi ports before processing.
	 */
	bool dequeue_midi_event (uint64_t timestamp_start,
	                         uint64_t timestamp_end,
	                         uint64_t& timestamp,
	                         uint8_t* data,
	                         size_t& size);

	bool start ();
	bool stop ();

	void set_enabled (bool enable);

	bool get_enabled ();

	/**
	 * @return Name of midi device
	 */
	std::string name () const { return m_name; }

private: // methods
	bool open (UINT index, std::string& error_msg);
	bool close (std::string& error_msg);

	bool add_sysex_buffer (std::string& error_msg);
	bool set_device_name (UINT index);

	std::string get_error_string (MMRESULT error_code);

	static void CALLBACK winmm_input_callback (HMIDIIN handle,
	                                           UINT msg,
	                                           DWORD_PTR instance,
	                                           DWORD_PTR midi_msg,
	                                           DWORD timestamp);

	void handle_short_msg (const uint8_t* midi_data, uint32_t timestamp);

	void handle_sysex_msg (MIDIHDR* const midi_header, uint32_t timestamp);

	bool enqueue_midi_msg (const uint8_t* midi_data, size_t size, uint32_t timestamp);

private: // data
	HMIDIIN m_handle;
	MIDIHDR m_sysex_header;

	bool m_started;

	std::string m_name;

	// can't use unique_ptr yet
	boost::scoped_ptr<PBD::RingBuffer<uint8_t> > m_midi_buffer;
	boost::scoped_array<uint8_t> m_sysex_buffer;
};

}

#endif // WINMME_MIDI_INPUT_DEVICE_H
