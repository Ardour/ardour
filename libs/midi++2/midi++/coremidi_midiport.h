/*
  Copyright (C) 2004 Paul Davis
  Copyright (C) 2004 Grame
  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation; either version 2 of the License, or
  (at your option) any later version.
 
  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. ÊSee the
  GNU General Public License for more details.
 
  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 
 */

#ifndef __coremidi_midiport_h__
#define __coremidi_midiport_h__

#include <list>
#include <string>

#include <fcntl.h>
#include <unistd.h>

#include <midi++/port.h>

#include <CoreMIDI/CoreMIDI.h>

namespace MIDI {

    class CoreMidi_MidiPort:public Port {
      public:
	CoreMidi_MidiPort(PortRequest & req);
	virtual ~ CoreMidi_MidiPort();

	virtual int selectable() const {
	    return -1;
	}
      protected:
	/* Direct I/O */
	int write(byte * msg, size_t msglen);
	int read(byte * buf, size_t max) {
	    return 0;
	} /* CoreMidi callback */
	    static void read_proc(const MIDIPacketList * pktlist,
				  void *refCon, void *connRefCon);

      private:
	byte midi_buffer[1024];
	MIDIClientRef midi_client;
	MIDIEndpointRef midi_destination;
	MIDIEndpointRef midi_source;

	int Open(PortRequest & req);
	void Close();
	static MIDITimeStamp MIDIGetCurrentHostTime();

	bool firstrecv;
    };

}; /* namespace MIDI */

#endif	// __coremidi_midiport_h__
