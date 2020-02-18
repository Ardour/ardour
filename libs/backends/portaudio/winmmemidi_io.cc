/*
 * Copyright (C) 2015-2016 Tim Mayberry <mojofunk@gmail.com>
 * Copyright (C) 2016 Robin Gareus <robin@gareus.org>
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

#include <windows.h>
#include <mmsystem.h>
#include <glibmm.h>

#include <sstream>
#include <set>

#include "pbd/error.h"
#include "pbd/compose.h"
#include "pbd/windows_timer_utils.h"

#include "winmmemidi_io.h"
#include "debug.h"

#include "pbd/i18n.h"

using namespace ARDOUR;

WinMMEMidiIO::WinMMEMidiIO()
	: m_active (false)
	, m_enabled (true)
	, m_run (false)
	, m_changed_callback (0)
	, m_changed_arg (0)
{
	pthread_mutex_init (&m_device_lock, 0);
}

WinMMEMidiIO::~WinMMEMidiIO()
{
	pthread_mutex_lock (&m_device_lock);
	cleanup();
	pthread_mutex_unlock (&m_device_lock);
	pthread_mutex_destroy (&m_device_lock);
}

void
WinMMEMidiIO::cleanup()
{
	DEBUG_MIDI ("MIDI cleanup\n");
	m_active = false;

	destroy_input_devices ();
	destroy_output_devices ();
}

bool
WinMMEMidiIO::dequeue_input_event (uint32_t port,
                                   uint64_t timestamp_start,
                                   uint64_t timestamp_end,
                                   uint64_t &timestamp,
                                   uint8_t *d,
                                   size_t &s)
{
	if (!m_active) {
		return false;
	}
	assert(port < m_inputs.size());

	// m_inputs access should be protected by trylock
	return m_inputs[port]->dequeue_midi_event (
	    timestamp_start, timestamp_end, timestamp, d, s);
}

bool
WinMMEMidiIO::enqueue_output_event (uint32_t port,
                                    uint64_t timestamp,
                                    const uint8_t *d,
                                    const size_t s)
{
	if (!m_active) {
		return false;
	}
	assert(port < m_outputs.size());

	// m_outputs access should be protected by trylock
	return m_outputs[port]->enqueue_midi_event (timestamp, d, s);
}


std::string
WinMMEMidiIO::port_id (uint32_t port, bool input)
{
	std::stringstream ss;

	if (input) {
		ss << "system:midi_capture_";
		ss << port;
	} else {
		ss << "system:midi_playback_";
		ss << port;
	}
	return ss.str();
}

std::string WinMMEMidiIO::port_name(uint32_t port, bool input)
{
	if (input) {
		if (port < m_inputs.size ()) {
			return m_inputs[port]->name ();
		}
	} else {
		if (port < m_outputs.size ()) {
			return m_outputs[port]->name ();
		}
	}
	return "";
}

void
WinMMEMidiIO::start ()
{
	if (m_run) {
		DEBUG_MIDI ("MIDI driver already started\n");
		return;
	}

	m_run = true;
	DEBUG_MIDI ("Starting MIDI driver\n");

	PBD::MMTIMERS::set_min_resolution();
	discover();
	start_devices ();
}


void
WinMMEMidiIO::stop ()
{
	if (!m_run) {
		DEBUG_MIDI ("MIDI driver already stopped\n");
		return;
	}
	DEBUG_MIDI ("Stopping MIDI driver\n");
	m_run = false;
	stop_devices ();
	pthread_mutex_lock (&m_device_lock);
	cleanup ();
	pthread_mutex_unlock (&m_device_lock);

	PBD::MMTIMERS::reset_resolution();
}

void
WinMMEMidiIO::start_devices ()
{
	for (std::vector<WinMMEMidiInputDevice*>::iterator i = m_inputs.begin ();
	     i < m_inputs.end();
	     ++i) {
		if (!(*i)->start ()) {
			PBD::error << string_compose (_("Unable to start MIDI input device %1\n"),
			                              (*i)->name ()) << endmsg;
		}
	}
	for (std::vector<WinMMEMidiOutputDevice*>::iterator i = m_outputs.begin ();
	     i < m_outputs.end();
	     ++i) {
		if (!(*i)->start ()) {
			PBD::error << string_compose (_ ("Unable to start MIDI output device %1\n"),
			                              (*i)->name ()) << endmsg;
		}
	}
}

void
WinMMEMidiIO::stop_devices ()
{
	for (std::vector<WinMMEMidiInputDevice*>::iterator i = m_inputs.begin ();
	     i < m_inputs.end();
	     ++i) {
		if (!(*i)->stop ()) {
			PBD::error << string_compose (_ ("Unable to stop MIDI input device %1\n"),
			                              (*i)->name ()) << endmsg;
		}
	}
	for (std::vector<WinMMEMidiOutputDevice*>::iterator i = m_outputs.begin ();
	     i < m_outputs.end();
	     ++i) {
		if (!(*i)->stop ()) {
			PBD::error << string_compose (_ ("Unable to stop MIDI output device %1\n"),
			                              (*i)->name ()) << endmsg;
		}
	}
}

void
WinMMEMidiIO::clear_device_info ()
{
	for (std::vector<MidiDeviceInfo*>::iterator i = m_device_info.begin();
	     i != m_device_info.end();
	     ++i) {
	  delete *i;
	}
	m_device_info.clear();
}

bool
WinMMEMidiIO::get_input_name_from_index (int index, std::string& name)
{
	MIDIINCAPS capabilities;
	MMRESULT result = midiInGetDevCaps(index, &capabilities, sizeof(capabilities));

	if (result == MMSYSERR_NOERROR) {
		DEBUG_MIDI(string_compose("Input Device: name : %1, mid : %2, pid : %3\n",
		                          capabilities.szPname,
		                          capabilities.wMid,
		                          capabilities.wPid));

		name = Glib::locale_to_utf8 (capabilities.szPname);
		return true;
	} else {
		DEBUG_MIDI ("Unable to get WinMME input device capabilities\n");
	}
	return false;
}

bool
WinMMEMidiIO::get_output_name_from_index (int index, std::string& name)
{
	MIDIOUTCAPS capabilities;
	MMRESULT result = midiOutGetDevCaps(index, &capabilities, sizeof(capabilities));
	if (result == MMSYSERR_NOERROR) {
		DEBUG_MIDI(string_compose("Output Device: name : %1, mid : %2, pid : %3\n",
		                          capabilities.szPname,
		                          capabilities.wMid,
		                          capabilities.wPid));

		name = Glib::locale_to_utf8 (capabilities.szPname);
		return true;
	} else {
		DEBUG_MIDI ("Unable to get WinMME output device capabilities\n");
	}
	return false;
}

void
WinMMEMidiIO::update_device_info ()
{
	std::set<std::string> device_names;

	int in_count = midiInGetNumDevs ();

	for (int i = 0; i < in_count; ++i) {
		std::string input_name;
		if (get_input_name_from_index(i, input_name)) {
			device_names.insert(input_name);
		}
	}

	int out_count = midiOutGetNumDevs ();

	for (int i = 0; i < out_count; ++i) {
		std::string output_name;
		if (get_output_name_from_index(i, output_name)) {
			device_names.insert(output_name);
		}
	}

	clear_device_info ();

	for (std::set<std::string>::const_iterator i = device_names.begin();
	     i != device_names.end();
	     ++i) {
	  m_device_info.push_back(new MidiDeviceInfo(*i));
	}
}

MidiDeviceInfo*
WinMMEMidiIO::get_device_info (const std::string& name)
{
	for (std::vector<MidiDeviceInfo*>::const_iterator i = m_device_info.begin();
	     i != m_device_info.end();
	     ++i) {
		if ((*i)->device_name == name) {
			return *i;
		}
	}
	return 0;
}

void
WinMMEMidiIO::create_input_devices ()
{
	int srcCount = midiInGetNumDevs ();

	DEBUG_MIDI (string_compose ("MidiIn count: %1\n", srcCount));

	for (int i = 0; i < srcCount; ++i) {
		std::string input_name;
		if (!get_input_name_from_index (i, input_name)) {
			DEBUG_MIDI ("Unable to get MIDI input name from index\n");
			continue;
		}

		MidiDeviceInfo* info = get_device_info (input_name);

		if (!info) {
			DEBUG_MIDI ("Unable to MIDI device info from name\n");
			continue;
		}

		if (!info->enable) {
			DEBUG_MIDI(string_compose(
			    "MIDI input device %1 not enabled, not opening device\n", input_name));
			continue;
		}

		try {
			WinMMEMidiInputDevice* midi_input = new WinMMEMidiInputDevice (i);
			if (midi_input) {
				m_inputs.push_back (midi_input);
			}
		}
		catch (...) {
			DEBUG_MIDI ("Unable to create MIDI input\n");
			continue;
		}
	}
}
void
WinMMEMidiIO::create_output_devices ()
{
	int dstCount = midiOutGetNumDevs ();

	DEBUG_MIDI (string_compose ("MidiOut count: %1\n", dstCount));

	for (int i = 0; i < dstCount; ++i) {
		std::string output_name;
		if (!get_output_name_from_index (i, output_name)) {
			DEBUG_MIDI ("Unable to get MIDI output name from index\n");
			continue;
		}

		MidiDeviceInfo* info = get_device_info (output_name);

		if (!info) {
			DEBUG_MIDI ("Unable to MIDI device info from name\n");
			continue;
		}

		if (!info->enable) {
			DEBUG_MIDI(string_compose(
			    "MIDI output device %1 not enabled, not opening device\n", output_name));
			continue;
		}

		try {
			WinMMEMidiOutputDevice* midi_output = new WinMMEMidiOutputDevice(i);
			if (midi_output) {
				m_outputs.push_back(midi_output);
			}
		} catch(...) {
			DEBUG_MIDI ("Unable to create MIDI output\n");
			continue;
		}
	}
}

void
WinMMEMidiIO::destroy_input_devices ()
{
	while (!m_inputs.empty ()) {
		WinMMEMidiInputDevice* midi_input = m_inputs.back ();
		// assert(midi_input->stopped ());
		m_inputs.pop_back ();
		delete midi_input;
	}
}

void
WinMMEMidiIO::destroy_output_devices ()
{
	while (!m_outputs.empty ()) {
		WinMMEMidiOutputDevice* midi_output = m_outputs.back ();
		// assert(midi_output->stopped ());
		m_outputs.pop_back ();
		delete midi_output;
	}
}

void
WinMMEMidiIO::discover()
{
	if (!m_run) {
		return;
	}

	if (pthread_mutex_trylock (&m_device_lock)) {
		return;
	}

	cleanup ();

	create_input_devices ();
	create_output_devices ();

	if (!(m_inputs.size () || m_outputs.size ())) {
		DEBUG_MIDI ("No midi inputs or outputs\n");
		pthread_mutex_unlock (&m_device_lock);
		return;
	}

	DEBUG_MIDI (string_compose ("Discovered %1 inputs and %2 outputs\n",
	                            m_inputs.size (),
	                            m_outputs.size ()));

	if (m_changed_callback) {
		m_changed_callback(m_changed_arg);
	}

	m_active = true;
	pthread_mutex_unlock (&m_device_lock);
}
