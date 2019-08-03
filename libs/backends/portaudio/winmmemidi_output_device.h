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

#ifndef WINMME_MIDI_OUTPUT_DEVICE_H
#define WINMME_MIDI_OUTPUT_DEVICE_H

#include <windows.h>
#include <mmsystem.h>

#include <stdint.h>
#include <pthread.h>

#include <string>

#include <boost/scoped_ptr.hpp>

#include <pbd/ringbuffer.h>

#define MaxWinMidiEventSize 256

namespace ARDOUR {

class WinMMEMidiOutputDevice {
public:
	WinMMEMidiOutputDevice (int index);

	~WinMMEMidiOutputDevice ();

	bool enqueue_midi_event (uint64_t rel_event_time_us,
	                         const uint8_t* data,
	                         const size_t size);

	bool start ();
	bool stop ();

	void set_enabled (bool enable);
	bool get_enabled ();

	std::string name () const { return m_name; }

private: // Methods
	bool open (UINT index, std::string& error_msg);
	bool close (std::string& error_msg);

	bool set_device_name (UINT index);

	std::string get_error_string (MMRESULT error_code);

	bool start_midi_output_thread ();
	bool stop_midi_output_thread ();

	bool signal (HANDLE semaphore);
	bool wait (HANDLE semaphore);

	static void* midi_output_thread (void*);
	void midi_output_thread ();

	bool wait_for_microseconds (int64_t us);

	static void CALLBACK winmm_output_callback (HMIDIOUT handle,
	                                            UINT msg,
	                                            DWORD_PTR instance,
	                                            DWORD_PTR midi_data,
	                                            DWORD_PTR timestamp);

	void midi_output_callback (UINT msg, DWORD_PTR data, DWORD_PTR timestamp);

private: // Data
	HMIDIOUT m_handle;

	HANDLE m_queue_semaphore;
	HANDLE m_sysex_semaphore;

	HANDLE m_timer;

	bool m_started;
	bool m_enabled;

	std::string m_name;

	pthread_t m_output_thread_handle;

	bool m_thread_running;
	bool m_thread_quit;

	boost::scoped_ptr<PBD::RingBuffer<uint8_t> > m_midi_buffer;
};

} // namespace ARDOUR

#endif // WINMME_MIDI_OUTPUT_DEVICE_H
