/*
	Copyright (C) 2006,2007 John Anderson

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

#include <sstream>
#include <cstring>
#include <cerrno>

#include <sigc++/sigc++.h>
#include <boost/shared_array.hpp>

#include "midi++/types.h"
#include "midi++/port.h"
#include "midi++/jack_midi_port.h"
#include "midi++/ipmidi_port.h"
#include "midi++/manager.h"

#include "ardour/debug.h"
#include "ardour/rc_configuration.h"
#include "ardour/session.h"
#include "ardour/audioengine.h"

#include "controls.h"
#include "mackie_control_protocol.h"
#include "surface.h"
#include "surface_port.h"


#include "i18n.h"

using namespace std;
using namespace Mackie;
using namespace PBD;

/** @param input_port Input MIDI::Port; this object takes responsibility for
 *  adding & removing it from the MIDI::Manager and destroying it.  @param
 *  output_port Output MIDI::Port; responsibility similarly taken.
 */
SurfacePort::SurfacePort (Surface& s)
	: _surface (&s)
	, _input_port (0)
	, _output_port (0)
{
	if (_surface->mcp().device_info().uses_ipmidi()) {
		_input_port = new MIDI::IPMIDIPort (_surface->mcp().ipmidi_base() +_surface->number());
		_output_port = _input_port;
	} else {
		jack_client_t* jack = MackieControlProtocol::instance()->get_session().engine().jack();
		
		_input_port = new MIDI::JackMIDIPort (string_compose (_("%1 in"),  _surface->name()), MIDI::Port::IsInput, jack);
		_output_port =new MIDI::JackMIDIPort (string_compose (_("%1 out"), _surface->name()), MIDI::Port::IsOutput, jack);
		
		/* MackieControl has its own thread for handling input from the input
		 * port, and we don't want anything handling output from the output
		 * port. This stops the Generic MIDI UI event loop in ardour from
		 * attempting to handle these ports.
		 */
		
		_input_port->set_centrally_parsed (false);
		_output_port->set_centrally_parsed (false);
		
		MIDI::Manager * mm = MIDI::Manager::instance();
		
		mm->add_port (_input_port);
		mm->add_port (_output_port);
	}
}

SurfacePort::~SurfacePort()
{
	if (_surface->mcp().device_info().uses_ipmidi()) {
		delete _input_port;
	} else {

		MIDI::Manager* mm = MIDI::Manager::instance ();
		
		if (_input_port) {
			mm->remove_port (_input_port);
			delete _input_port;
		}
		
		if (_output_port) {
			_output_port->drain (10000);
			mm->remove_port (_output_port);
			delete _output_port;
		}
	}
}

// wrapper for one day when strerror_r is working properly
string fetch_errmsg (int error_number)
{
	char * msg = strerror (error_number);
	return msg;
}
	
int 
SurfacePort::write (const MidiByteArray & mba)
{
	if (mba.empty()) {
<<<<<<< HEAD
=======
		DEBUG_TRACE (DEBUG::MackieControl, string_compose ("port %1 asked to write an empty MBA\n", output_port().name()));
>>>>>>> master
		return 0;
	}

	DEBUG_TRACE (DEBUG::MackieControl, string_compose ("port %1 write %2\n", output_port().name(), mba));

	if (mba[0] != 0xf0 && mba.size() > 3) {
		std::cerr << "TOO LONG WRITE: " << mba << std::endl;
	}
		
	/* this call relies on std::vector<T> using contiguous storage. not
	 * actually guaranteed by the standard, but way, way beyond likely.
	 */

	int count = output_port().write (&mba[0], mba.size(), 0);

	if  (count != (int) mba.size()) {

		if (errno == 0) {

			cout << "port overflow on " << output_port().name() << ". Did not write all of " << mba << endl;

		} else if  (errno != EAGAIN) {
			ostringstream os;
			os << "Surface: couldn't write to port " << output_port().name();
			os << ", error: " << fetch_errmsg (errno) << "(" << errno << ")";
			cout << os.str() << endl;
		}

		return -1;
	}

	return 0;
}

ostream & 
Mackie::operator <<  (ostream & os, const SurfacePort & port)
{
	os << "{ ";
	os << "name: " << port.input_port().name() << " " << port.output_port().name();
	os << "; ";
	os << " }";
	return os;
}

