/*
    Copyright (C) 1998-99 Paul Barton-Davis 
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

#include <cassert>
#include <stdint.h>

#include "pbd/error.h"
#include "pbd/convert.h"

#include "midi++/types.h"
#include "midi++/factory.h"
#include "midi++/fifomidi.h"

#ifdef WITH_JACK_MIDI
#include "midi++/jack.h"

std::string MIDI::JACK_MidiPort::typestring = "jack";
#endif // WITH_JACK_MIDI

std::string MIDI::FIFO_MidiPort::typestring = "fifo";

#ifdef WITH_ALSA
#include "midi++/alsa_sequencer.h"
#include "midi++/alsa_rawmidi.h"

std::string MIDI::ALSA_SequencerMidiPort::typestring = "alsa/sequencer";
std::string MIDI::ALSA_RawMidiPort::typestring = "alsa/raw";

#endif // WITH_ALSA

#ifdef WITH_COREMIDI
#include "midi++/coremidi_midiport.h"

std::string MIDI::CoreMidi_MidiPort::typestring = "coremidi";

#endif // WITH_COREMIDI

using namespace std;
using namespace MIDI;
using namespace PBD;

// FIXME: void* data pointer, filthy
Port *
PortFactory::create_port (const XMLNode& node, void* data)

{
	Port::Descriptor desc (node);
	Port *port;
	
	switch (desc.type) {
#ifdef WITH_JACK_MIDI
	case Port::JACK_Midi:
		assert(data != NULL);
		port = new JACK_MidiPort (node, (jack_client_t*) data);
		break;
#endif // WITH_JACK_MIDI
	
#ifdef WITH_ALSA
	case Port::ALSA_RawMidi:
		port = new ALSA_RawMidiPort (node);
		break;

	case Port::ALSA_Sequencer:
		port = new ALSA_SequencerMidiPort (node);
		break;
#endif // WITH_ALSA

#if WITH_COREMIDI
	case Port::CoreMidi_MidiPort:
		port = new CoreMidi_MidiPort (node);
		break;
#endif // WITH_COREMIDI

	case Port::FIFO:
		port = new FIFO_MidiPort (node);
		break;

	default:
		return 0;
	}

	return port;
}

bool 
PortFactory::ignore_duplicate_devices (Port::Type type)
{
	bool ret = false;

	switch (type) {
#ifdef WITH_ALSA
	case Port::ALSA_Sequencer:
		ret = true;
		break;
#endif // WITH_ALSA

#if WITH_COREMIDI
	case Port::CoreMidi_MidiPort:
		ret = true;
		break;
#endif // WITH_COREMIDI

	default:
		break;
	}

	return ret;
}

int
#if defined (WITH_ALSA) || defined (WITH_COREMIDI)
PortFactory::get_known_ports (vector<PortSet>& ports)
#else
PortFactory::get_known_ports (vector<PortSet>&)
#endif	
{
	int n = 0;
#ifdef WITH_ALSA
	n += ALSA_SequencerMidiPort::discover (ports);
#endif // WITH_ALSA

#if WITH_COREMIDI
	n += CoreMidi_MidiPort::discover (ports);
#endif // WITH_COREMIDI
	
	return n;
}

std::string
PortFactory::default_port_type ()
{
#ifdef WITH_JACK_MIDI
	return "jack";
#endif

#ifdef WITH_ALSA
	return "alsa/sequencer";
#endif

#ifdef WITH_COREMIDI
	return "coremidi";
#endif // WITH_COREMIDI
	
	PBD::fatal << "programming error: no default port type defined in midifactory.cc" << endmsg;
	/*NOTREACHED*/
	return "";
}

Port::Type
PortFactory::string_to_type (const string& xtype)
{
	if (0){ 
#ifdef WITH_ALSA
	} else if (strings_equal_ignore_case (xtype, ALSA_RawMidiPort::typestring)) {
		return Port::ALSA_RawMidi;
	} else if (strings_equal_ignore_case (xtype, ALSA_SequencerMidiPort::typestring)) {
		return Port::ALSA_Sequencer;
#endif 
#ifdef WITH_COREMIDI
	} else if (strings_equal_ignore_case (xtype, CoreMidi_MidiPort::typestring)) {
		return Port::CoreMidi_MidiPort;
#endif
	} else if (strings_equal_ignore_case (xtype, FIFO_MidiPort::typestring)) {
		return Port::FIFO;
#ifdef WITH_JACK_MIDI
	} else if (strings_equal_ignore_case (xtype, JACK_MidiPort::typestring)) {
		return Port::JACK_Midi;
#endif
	}

	return Port::Unknown;
}

string
PortFactory::mode_to_string (int mode)
{
	if (mode == O_RDONLY) {
		return "input";
	} else if (mode == O_WRONLY) {
		return "output";
	} 

	return "duplex";
}

int
PortFactory::string_to_mode (const string& str)
{
	if (strings_equal_ignore_case (str, "output") || strings_equal_ignore_case (str, "out")) {
		return O_WRONLY;
	} else if (strings_equal_ignore_case (str, "input") || strings_equal_ignore_case (str, "in")) {
		return O_RDONLY;
	}

	return O_RDWR;
}
