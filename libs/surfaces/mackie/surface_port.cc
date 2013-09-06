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

#include "pbd/failed_constructor.h"

#include "midi++/types.h"
#include "midi++/ipmidi_port.h"

#include "ardour/async_midi_port.h"
#include "ardour/debug.h"
#include "ardour/rc_configuration.h"
#include "ardour/session.h"
#include "ardour/audioengine.h"
#include "ardour/async_midi_port.h"
#include "ardour/midiport_manager.h"

#include "controls.h"
#include "mackie_control_protocol.h"
#include "surface.h"
#include "surface_port.h"

#include "i18n.h"

using namespace std;
using namespace Mackie;
using namespace PBD;
using namespace ARDOUR;

SurfacePort::SurfacePort (Surface& s)
	: _surface (&s)
{

	if (_surface->mcp().device_info().uses_ipmidi()) {
		_input_port = new MIDI::IPMIDIPort (_surface->mcp().ipmidi_base() +_surface->number());
		_output_port = _input_port;
	} else {
		_async_in  = AudioEngine::instance()->register_input_port (DataType::MIDI, string_compose (_("%1 in"),  _surface->name()), true);
		_async_out = AudioEngine::instance()->register_output_port (DataType::MIDI, string_compose (_("%1 out"),  _surface->name()), true);

		if (_async_in == 0 || _async_out == 0) {
			throw failed_constructor();
		}

		_input_port = boost::dynamic_pointer_cast<AsyncMIDIPort>(_async_in).get();
		_output_port = boost::dynamic_pointer_cast<AsyncMIDIPort>(_async_out).get();
	}
}

SurfacePort::~SurfacePort()
{
	if (_surface->mcp().device_info().uses_ipmidi()) {
		delete _input_port;
	} else {

		if (_async_in) {
			AudioEngine::instance()->unregister_port (_async_in);
			_async_in.reset ();
		}
		
		if (_async_out) {
			_output_port->drain (10000);
			AudioEngine::instance()->unregister_port (_async_out);
			_async_out.reset ();
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
		DEBUG_TRACE (DEBUG::MackieControl, string_compose ("port %1 asked to write an empty MBA\n", output_port().name()));
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

