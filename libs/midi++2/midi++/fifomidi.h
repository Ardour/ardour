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

#ifndef __fifomidi_h__
#define __fifomidi_h__

#include <fcntl.h>
#include <vector>
#include <string>
#include <unistd.h>

#include <midi++/port.h>
#include <midi++/fd_midiport.h>

namespace MIDI {

class FIFO_MidiPort : public MIDI::FD_MidiPort 

{
  public:
	FIFO_MidiPort (const XMLNode&);
	~FIFO_MidiPort () {};

	static std::string typestring;

  protected:
	std::string get_typestring () const {
		return typestring;
	}

  private:
	void open (const Port::Descriptor&);
};

} // namespace MIDI

#endif // __fifomidi_h__
