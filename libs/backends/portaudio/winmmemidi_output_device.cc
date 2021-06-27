/*
 * Copyright (C) 2015-2016 Tim Mayberry <mojofunk@gmail.com>
 * Copyright (C) 2015 Paul Davis <paul@linuxaudiosystems.com>
 * Copyright (C) 2017 Robin Gareus <robin@gareus.org>
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

#include "winmmemidi_output_device.h"

#include <glibmm.h>

#include "pbd/debug.h"
#include "pbd/compose.h"
#include "pbd/microseconds.h"
#include "pbd/pthread_utils.h"
#include "pbd/windows_timer_utils.h"
#include "pbd/windows_mmcss.h"

#include "midi_util.h"

#include "debug.h"

// remove dup with input_device
static const uint32_t MIDI_BUFFER_SIZE = 32768;
static const uint32_t MAX_QUEUE_SIZE = 4096;

namespace ARDOUR {

WinMMEMidiOutputDevice::WinMMEMidiOutputDevice (int index)
	: m_handle(0)
	, m_queue_semaphore(0)
	, m_sysex_semaphore(0)
	, m_timer(0)
	, m_started(false)
	, m_enabled(false)
	, m_thread_running(false)
	, m_thread_quit(false)
	, m_midi_buffer(new PBD::RingBuffer<uint8_t>(MIDI_BUFFER_SIZE))
{
	DEBUG_MIDI (string_compose ("Creating midi output device index: %1\n", index));

	std::string error_msg;

	if (!open (index, error_msg)) {
		DEBUG_MIDI (error_msg);
		throw std::runtime_error (error_msg);
	}

	set_device_name (index);
}

WinMMEMidiOutputDevice::~WinMMEMidiOutputDevice ()
{
	std::string error_msg;
	if (!close (error_msg)) {
		DEBUG_MIDI (error_msg);
	}
}

bool
WinMMEMidiOutputDevice::enqueue_midi_event (uint64_t timestamp,
					    const uint8_t* data,
					    size_t size)
{
	const uint32_t total_bytes = sizeof(MidiEventHeader) + size;
	if (m_midi_buffer->write_space () < total_bytes) {
		DEBUG_MIDI ("WinMMEMidiOutput: ring buffer overflow\n");
		return false;
	}

	MidiEventHeader h (timestamp, size);
	m_midi_buffer->write ((uint8_t*)&h, sizeof(MidiEventHeader));
	m_midi_buffer->write (data, size);

	signal (m_queue_semaphore);
	return true;
}

bool
WinMMEMidiOutputDevice::open (UINT index, std::string& error_msg)
{
	MMRESULT result = midiOutOpen (&m_handle,
				       index,
				       (DWORD_PTR)winmm_output_callback,
				       (DWORD_PTR) this,
				       CALLBACK_FUNCTION);
	if (result != MMSYSERR_NOERROR) {
		error_msg = get_error_string (result);
		return false;
	}

	m_queue_semaphore = CreateSemaphore (NULL, 0, MAX_QUEUE_SIZE, NULL);
	if (m_queue_semaphore == NULL) {
		DEBUG_MIDI ("WinMMEMidiOutput: Unable to create queue semaphore\n");
		return false;
	}
	m_sysex_semaphore = CreateSemaphore (NULL, 0, 1, NULL);
	if (m_sysex_semaphore == NULL) {
		DEBUG_MIDI ("WinMMEMidiOutput: Unable to create sysex semaphore\n");
		return false;
	}
	return true;
}

bool
WinMMEMidiOutputDevice::close (std::string& error_msg)
{
	// return error message for first error encountered?
	bool success = true;
	MMRESULT result = midiOutClose (m_handle);
	if (result != MMSYSERR_NOERROR) {
		error_msg = get_error_string (result);
		DEBUG_MIDI (error_msg);
		success = false;
	}

	if (m_sysex_semaphore) {
		if (!CloseHandle (m_sysex_semaphore)) {
			DEBUG_MIDI ("WinMMEMidiOut Unable to close sysex semaphore\n");
			success = false;
		} else {
			m_sysex_semaphore = 0;
		}
	}
	if (m_queue_semaphore) {
		if (!CloseHandle (m_queue_semaphore)) {
			DEBUG_MIDI ("WinMMEMidiOut Unable to close queue semaphore\n");
			success = false;
		} else {
			m_queue_semaphore = 0;
		}
	}

	m_handle = 0;
	return success;
}

bool
WinMMEMidiOutputDevice::set_device_name (UINT index)
{
	MIDIOUTCAPS capabilities;
	MMRESULT result =
	    midiOutGetDevCaps (index, &capabilities, sizeof(capabilities));

	if (result != MMSYSERR_NOERROR) {
		DEBUG_MIDI (get_error_string (result));
		m_name = "Unknown Midi Output Device";
		return false;
	} else {
		m_name = capabilities.szPname;
	}
	return true;
}

std::string
WinMMEMidiOutputDevice::get_error_string (MMRESULT error_code)
{
	char error_msg[MAXERRORLENGTH];
	MMRESULT result = midiOutGetErrorText (error_code, error_msg, MAXERRORLENGTH);
	if (result != MMSYSERR_NOERROR) {
		return error_msg;
	}
	return "WinMMEMidiOutput: Unknown Error code";
}

bool
WinMMEMidiOutputDevice::start ()
{
	if (m_thread_running) {
		DEBUG_MIDI (
		    string_compose ("WinMMEMidiOutput: device %1 already started\n", m_name));
		return true;
	}

	m_timer = CreateWaitableTimer (NULL, FALSE, NULL);

	if (!m_timer) {
		DEBUG_MIDI ("WinMMEMidiOutput: unable to create waitable timer\n");
		return false;
	}

	if (!start_midi_output_thread ()) {
		DEBUG_MIDI ("WinMMEMidiOutput: Failed to start MIDI output thread\n");

		if (!CloseHandle (m_timer)) {
			DEBUG_MIDI ("WinMMEMidiOutput: unable to close waitable timer\n");
		}
		return false;
	}
	return true;
}

bool
WinMMEMidiOutputDevice::stop ()
{
	if (!m_thread_running) {
		DEBUG_MIDI ("WinMMEMidiOutputDevice: device already stopped\n");
		return true;
	}

	if (!stop_midi_output_thread ()) {
		DEBUG_MIDI ("WinMMEMidiOutput: Failed to stop MIDI output thread\n");
		return false;
	}

	if (!CloseHandle (m_timer)) {
		DEBUG_MIDI ("WinMMEMidiOutput: unable to close waitable timer\n");
		return false;
	}
	m_timer = 0;
	return true;
}

bool
WinMMEMidiOutputDevice::start_midi_output_thread ()
{
	m_thread_quit = false;

	// TODO Use native threads
	if (pbd_realtime_pthread_create (PBD_SCHED_FIFO, PBD_RT_PRI_MIDI, PBD_RT_STACKSIZE_HELP,
				&m_output_thread_handle, midi_output_thread, this)) {
		return false;
	}

	int timeout = 5000;
	while (!m_thread_running && --timeout > 0) { Glib::usleep (1000); }
	if (timeout == 0 || !m_thread_running) {
		DEBUG_MIDI (string_compose ("Unable to start midi output device thread: %1\n",
					    m_name));
		return false;
	}
	return true;
}

bool
WinMMEMidiOutputDevice::stop_midi_output_thread ()
{
	int timeout = 5000;
	m_thread_quit = true;
	signal (m_queue_semaphore);

	while (m_thread_running && --timeout > 0) { Glib::usleep (1000); }
	if (timeout == 0 || m_thread_running) {
		DEBUG_MIDI (string_compose ("Unable to stop midi output device thread: %1\n",
					     m_name));
		return false;
	}

	void *status;
	if (pthread_join (m_output_thread_handle, &status)) {
		DEBUG_MIDI (string_compose ("Unable to join midi output device thread: %1\n",
					    m_name));
		return false;
	}
	return true;
}

bool
WinMMEMidiOutputDevice::signal (HANDLE semaphore)
{
	bool result = (bool)ReleaseSemaphore (semaphore, 1, NULL);
	if (!result) {
		DEBUG_MIDI ("WinMMEMidiOutDevice: Cannot release semaphore\n");
	}
	return result;
}

bool
WinMMEMidiOutputDevice::wait (HANDLE semaphore)
{
	DWORD result = WaitForSingleObject (semaphore, INFINITE);
	switch (result) {
	case WAIT_FAILED:
		DEBUG_MIDI ("WinMMEMidiOutDevice: WaitForSingleObject Failed\n");
		break;
	case WAIT_OBJECT_0:
		return true;
	default:
		DEBUG_MIDI ("WinMMEMidiOutDevice: Unexpected result from WaitForSingleObject\n");
	}
	return false;
}

void CALLBACK
WinMMEMidiOutputDevice::winmm_output_callback (HMIDIOUT handle,
					       UINT msg,
					       DWORD_PTR instance,
					       DWORD_PTR midi_data,
					       DWORD_PTR timestamp)
{
	((WinMMEMidiOutputDevice*)instance)
	    ->midi_output_callback (msg, midi_data, timestamp);
}

void
WinMMEMidiOutputDevice::midi_output_callback (UINT message,
					      DWORD_PTR midi_data,
					      DWORD_PTR timestamp)
{
	switch (message) {
	case MOM_CLOSE:
		DEBUG_MIDI ("WinMMEMidiOutput - MIDI device closed\n");
		break;
	case MOM_DONE:
		signal (m_sysex_semaphore);
		break;
	case MOM_OPEN:
		DEBUG_MIDI ("WinMMEMidiOutput - MIDI device opened\n");
		break;
	case MOM_POSITIONCB:
		LPMIDIHDR header = (LPMIDIHDR)midi_data;
		DEBUG_MIDI (string_compose ("WinMMEMidiOut - %1 bytes out of %2 bytes of "
					    "the current sysex message have been sent.\n",
					    header->dwOffset,
					    header->dwBytesRecorded));
	}
}

void*
WinMMEMidiOutputDevice::midi_output_thread (void *arg)
{
	WinMMEMidiOutputDevice* output_device = reinterpret_cast<WinMMEMidiOutputDevice*> (arg);
	output_device->midi_output_thread ();
	return 0;
}

void
WinMMEMidiOutputDevice::midi_output_thread ()
{
	m_thread_running = true;

	DEBUG_MIDI ("WinMMEMidiOut: MIDI output thread started\n");

#ifdef USE_MMCSS_THREAD_PRIORITIES
	HANDLE task_handle;

	PBD::MMCSS::set_thread_characteristics ("Pro Audio", &task_handle);
	PBD::MMCSS::set_thread_priority (task_handle, PBD::MMCSS::AVRT_PRIORITY_HIGH);
#endif

	while (!m_thread_quit) {
		if (!wait (m_queue_semaphore)) {
			DEBUG_MIDI ("WinMMEMidiOut: output thread waiting for semaphore failed\n");
			break;
		}

		DEBUG_MIDI ("WinMMEMidiOut: output thread woken by semaphore\n");

		MidiEventHeader h (0, 0);
		uint8_t data[MaxWinMidiEventSize];

		const uint32_t read_space = m_midi_buffer->read_space ();

		DEBUG_MIDI (string_compose ("WinMMEMidiOut: total readable MIDI data %1\n", read_space));

		if (read_space > sizeof(MidiEventHeader)) {
			if (m_midi_buffer->read ((uint8_t*)&h, sizeof(MidiEventHeader)) !=
			    sizeof(MidiEventHeader)) {
				DEBUG_MIDI ("WinMMEMidiOut: Garbled MIDI EVENT HEADER!!\n");
				break;
			}
			assert (read_space >= h.size);

			if (h.size > MaxWinMidiEventSize) {
				m_midi_buffer->increment_read_idx (h.size);
				DEBUG_MIDI ("WinMMEMidiOut: MIDI event too large!\n");
				continue;
			}
			if (m_midi_buffer->read (&data[0], h.size) != h.size) {
				DEBUG_MIDI ("WinMMEMidiOut: Garbled MIDI EVENT DATA!!\n");
				break;
			}
		} else {
			// error/assert?
			DEBUG_MIDI ("WinMMEMidiOut: MIDI buffer underrun, shouldn't occur\n");
			continue;
		}
		uint64_t current_time = PBD::get_microseconds ();

		DEBUG_TIMING (string_compose (
		    "WinMMEMidiOut: h.time = %1, current_time = %2\n", h.time, current_time));

		if (h.time > current_time) {

			DEBUG_TIMING (string_compose ("WinMMEMidiOut: waiting at %1 for %2 "
						      "milliseconds before sending message\n",
						      ((double)current_time) / 1000.0,
						      ((double)(h.time - current_time)) / 1000.0));

			if (!wait_for_microseconds (h.time - current_time))
			{
				DEBUG_MIDI ("WinMMEMidiOut: Error waiting for timer\n");
				break;
			}

			uint64_t wakeup_time = PBD::get_microseconds ();
			DEBUG_TIMING (string_compose ("WinMMEMidiOut: woke up at %1(ms)\n",
						      ((double)wakeup_time) / 1000.0));
			if (wakeup_time > h.time) {
				DEBUG_TIMING (string_compose ("WinMMEMidiOut: overslept by %1(ms)\n",
							      ((double)(wakeup_time - h.time)) / 1000.0));
			} else if (wakeup_time < h.time) {
				DEBUG_TIMING (string_compose ("WinMMEMidiOut: woke up %1(ms) too early\n",
							      ((double)(h.time - wakeup_time)) / 1000.0));
			}

		} else if (h.time < current_time) {
			DEBUG_TIMING (string_compose (
			    "WinMMEMidiOut: MIDI event at sent to driver %1(ms) late\n",
			    ((double)(current_time - h.time)) / 1000.0));
		}

		DEBUG_MIDI (string_compose ("WinMMEMidiOut: MIDI event size: %1 time %2 now %3\n", h.size, h.time, current_time));

		DWORD message = 0;
		MMRESULT result;
		switch (h.size) {
		case 3:
			message |= (((DWORD)data[2]) << 16);
			/* fallthrough */
		case 2:
			message |= (((DWORD)data[1]) << 8);
			/* fallthrough */
		case 1:
			message |= (DWORD)data[0];
			result = midiOutShortMsg (m_handle, message);
			if (result != MMSYSERR_NOERROR) {
				DEBUG_MIDI (
				    string_compose ("WinMMEMidiOutput: %1\n", get_error_string (result)));
			}
			continue;
		}

		MIDIHDR header;
		header.dwBufferLength = h.size;
		header.dwFlags = 0;
		header.lpData = (LPSTR)data;

		result = midiOutPrepareHeader (m_handle, &header, sizeof(MIDIHDR));
		if (result != MMSYSERR_NOERROR) {
			DEBUG_MIDI (string_compose ("WinMMEMidiOutput: midiOutPrepareHeader %1\n",
						    get_error_string (result)));
			continue;
		}

		result = midiOutLongMsg (m_handle, &header, sizeof(MIDIHDR));
		if (result != MMSYSERR_NOERROR) {
			DEBUG_MIDI (string_compose ("WinMMEMidiOutput: midiOutLongMsg %1\n",
						    get_error_string (result)));
			continue;
		}

		// Sysex messages may be sent synchronously or asynchronously.  The
		// choice is up to the WinMME driver.  So, we wait until the message is
		// sent, regardless of the driver's choice.

		DEBUG_MIDI ("WinMMEMidiOut: wait for sysex semaphore\n");

		if (!wait (m_sysex_semaphore)) {
			DEBUG_MIDI ("WinMMEMidiOut: wait for sysex semaphore - failed!\n");
			break;
		}

		result = midiOutUnprepareHeader (m_handle, &header, sizeof(MIDIHDR));
		if (result != MMSYSERR_NOERROR) {
			DEBUG_MIDI (string_compose ("WinMMEMidiOutput: midiOutUnprepareHeader %1\n",
						    get_error_string (result)));
			break;
		}
	}

#ifdef USE_MMCSS_THREAD_PRIORITIES
	PBD::MMCSS::revert_thread_characteristics (task_handle);
#endif

	m_thread_running = false;
}

bool
WinMMEMidiOutputDevice::wait_for_microseconds (int64_t wait_us)
{
	LARGE_INTEGER due_time;

	// 100 ns resolution
	due_time.QuadPart = -((LONGLONG)(wait_us * 10));
	if (!SetWaitableTimer (m_timer, &due_time, 0, NULL, NULL, 0)) {
		DEBUG_MIDI ("WinMMEMidiOut: Error waiting for timer\n");
		return false;
	}

	if (!wait (m_timer)) {
		return false;
	}

	return true;
}

} // namespace ARDOUR
