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

*/

#ifndef __nullmidi_h__
#define __nullmidi_h__

#include <fcntl.h>
#include <vector>
#include <string>

#include <midi++/port.h>

namespace MIDI {

class Null_MidiPort : public Port 

{
  public:
	Null_MidiPort (PortRequest &req) 
		: Port (req) { 

		/* reset devname and tagname */
		
		_devname = "nullmidi";
		_tagname = "null";
		_type = Port::Null;
		_ok = true;
	}

	virtual ~Null_MidiPort () {};

	/* Direct I/O */
	int write (byte *msg, size_t msglen, timestamp_t timestamp) {
		return msglen;
	}

	int read (byte *buf, size_t max) {
		return 0;
	}
	
	virtual int selectable() const { return -1; }

	static std::string typestring;

  protected:
	std::string get_typestring () const {
		return typestring;
	}
};

} // namespace MIDI

#endif // __nullmidi_h__
