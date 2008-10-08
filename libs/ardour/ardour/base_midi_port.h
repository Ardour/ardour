/*
    Copyright (C) 2002 Paul Davis 

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

    $Id: port.h 712 2006-07-28 01:08:57Z drobilla $
*/

#ifndef __ardour_base_midi_port_h__
#define __ardour_base_midi_port_h__

#include <sigc++/signal.h>
#include <pbd/failed_constructor.h>
#include <ardour/ardour.h>
#include <ardour/port.h>
#include <ardour/midi_buffer.h>

namespace ARDOUR {

class MidiEngine;

class BaseMidiPort : public virtual Port {
   public:
	virtual ~BaseMidiPort();
	
	DataType type() const { return DataType::MIDI; }

	Buffer& get_buffer( nframes_t nframes, nframes_t offset ) {
		return get_midi_buffer( nframes, offset );
	}

	virtual MidiBuffer& get_midi_buffer (nframes_t nframes, nframes_t offset ) = 0;
	
	size_t capacity() { return _buffer->capacity(); }
	size_t size()     { return _buffer->size(); }

	void set_mixdown_function (void (*func)(const std::set<Port*>&, MidiBuffer*, nframes_t, nframes_t, bool));

  protected:
	BaseMidiPort (const std::string& name, Flags);
	
	MidiBuffer*     _buffer;
	bool            _own_buffer;

	void (*_mixdown)(const std::set<Port*>&, MidiBuffer*, nframes_t, nframes_t, bool);
	static void default_mixdown (const std::set<Port*>&, MidiBuffer*, nframes_t, nframes_t, bool);
};
 
} // namespace ARDOUR

#endif /* __ardour_base_midi_port_h__ */
