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

void
PortFactory::add_port_request (vector<PortRequest *> &reqs, 
			           const string &str)
	
{
	PortRequest *req;

	req = new PortRequest;
	req->devname = strdup (str.c_str());
	req->tagname = strdup (str.c_str());

	req->mode = O_RDWR;
	req->type = Port::ALSA_RawMidi;

	reqs.push_back (req);
}

