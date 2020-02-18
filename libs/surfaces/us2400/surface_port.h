/*
 * Copyright (C) 2017 Ben Loftis <ben@harrisonconsoles.com>
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
#ifndef surface_port_h
#define surface_port_h

#include <midi++/types.h>

#include "pbd/signals.h"


#include "midi_byte_array.h"
#include "types.h"

namespace MIDI {
	class Parser;
	class Port;
}


namespace ARDOUR {
	class AsyncMIDIPort;
	class Port;
}

namespace ArdourSurface {

class US2400Protocol;

namespace US2400
{

class Surface;

/**
   Make a relationship between a midi port and a Mackie device.
*/

class SurfacePort
{
  public:
	SurfacePort (US2400::Surface&);
	virtual ~SurfacePort();

	/// an easier way to output bytes via midi
	int write (const MidiByteArray&);

	MIDI::Port& input_port() const { return *_input_port; }
	MIDI::Port& output_port() const { return *_output_port; }

	ARDOUR::Port& input() const { return *_async_in; }
	ARDOUR::Port& output() const { return *_async_out; }

	std::string input_name() const;
	std::string output_name() const;

	void reconnect ();

	XMLNode& get_state ();
	int set_state (const XMLNode&, int version);

  protected:

  private:
	US2400::Surface*   _surface;
	MIDI::Port* _input_port;
	MIDI::Port* _output_port;
	boost::shared_ptr<ARDOUR::Port> _async_in;
	boost::shared_ptr<ARDOUR::Port> _async_out;
};

std::ostream& operator <<  (std::ostream& , const SurfacePort& port);

}
}

#endif
