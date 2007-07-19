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

#include <pbd/error.h>

#include <midi++/types.h>
#include <midi++/factory.h>
#include <midi++/nullmidi.h>
#include <midi++/fifomidi.h>

#ifdef WITH_ALSA
#include <midi++/alsa_sequencer.h>
#include <midi++/alsa_rawmidi.h>
#endif // WITH_ALSA

#ifdef WITH_COREMIDI
#include <midi++/coremidi_midiport.h>
#endif // WITH_COREMIDI


using namespace std;
using namespace MIDI;

Port *
PortFactory::create_port (PortRequest &req)

{
	Port *port;
	
	switch (req.type) {
#ifdef WITH_ALSA
	case Port::ALSA_RawMidi:
		port = new ALSA_RawMidiPort (req);
		break;

	case Port::ALSA_Sequencer:
		port = new ALSA_SequencerMidiPort (req);
		break;
#endif // WITH_ALSA

#if WITH_COREMIDI
	case Port::CoreMidi_MidiPort:
		port = new CoreMidi_MidiPort (req);
		break;
#endif // WITH_COREMIDI

	case Port::Null:
		port = new Null_MidiPort (req);
		break;

	case Port::FIFO:
		port = new FIFO_MidiPort (req);
		break;

	default:
		req.status = PortRequest::TypeUnsupported;
		return 0;
	}

	req.status = PortRequest::OK;

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
PortFactory::get_known_ports (vector<PortSet>& ports)
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

#ifdef WITH_ALSA
	return "alsa/sequencer";
#endif

#ifdef WITH_COREMIDI
	return "coremidi";
#endif // WITH_COREMIDI
	
	PBD::fatal << "programming error: no default port type defined in midifactory.cc" << endmsg;
}
