/*
 * Copyright (C) 2006-2007 John Anderson
 * Copyright (C) 2007-2016 Paul Davis <paul@linuxaudiosystems.com>
 * Copyright (C) 2010-2011 Carl Hetherington <carl@carlh.net>
 * Copyright (C) 2015-2017 Robin Gareus <robin@gareus.org>
 * Copyright (C) 2015 Len Ovens <len@ovenwerks.net>
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

#include "controls.h"
#include "mackie_control_protocol.h"
#include "surface.h"
#include "surface_port.h"

#include "pbd/i18n.h"

using namespace std;
using namespace PBD;
using namespace ARDOUR;
using namespace ArdourSurface;
using namespace Mackie;

SurfacePort::SurfacePort (Surface& s)
	: _surface (&s)
{
	if (_surface->mcp().device_info().uses_ipmidi()) {
		_input_port = new MIDI::IPMIDIPort (_surface->mcp().ipmidi_base() +_surface->number());
		_output_port = _input_port;

	} else {

		string in_name;
		string out_name;

		if (_surface->mcp().device_info().extenders() > 0) {
			if (_surface->number() == _surface->mcp().device_info().master_position()) {
				in_name = X_("mackie control in");
				out_name = X_("mackie control out");
			} else {
				in_name = string_compose (X_("mackie control in ext %1"), (_surface->number() + 1));
				out_name = string_compose (X_("mackie control out ext %1"), _surface->number() + 1);
			}
		} else {
			in_name = X_("mackie control in");
			out_name = X_("mackie control out");
		}

		_async_in  = AudioEngine::instance()->register_input_port (DataType::MIDI, in_name, true);
		_async_out = AudioEngine::instance()->register_output_port (DataType::MIDI, out_name, true);

		if (_async_in == 0 || _async_out == 0) {
			throw failed_constructor();
		}

		_input_port = boost::dynamic_pointer_cast<AsyncMIDIPort>(_async_in).get();
		_output_port = boost::dynamic_pointer_cast<AsyncMIDIPort>(_async_out).get();
	}
}

SurfacePort::~SurfacePort()
{
	if (dynamic_cast<MIDI::IPMIDIPort*>(_input_port)) {
		delete _input_port;
		_input_port = 0;
	} else {
		if (_async_in) {
			DEBUG_TRACE (DEBUG::MackieControl, string_compose ("unregistering input port %1\n", _async_in->name()));
			Glib::Threads::Mutex::Lock em (AudioEngine::instance()->process_lock());
			AudioEngine::instance()->unregister_port (_async_in);
			_async_in.reset ((ARDOUR::Port*) 0);
		}

		if (_async_out) {
			_output_port->drain (10000, 250000);
			DEBUG_TRACE (DEBUG::MackieControl, string_compose ("unregistering output port %1\n", _async_out->name()));
			Glib::Threads::Mutex::Lock em (AudioEngine::instance()->process_lock());
			AudioEngine::instance()->unregister_port (_async_out);
			_async_out.reset ((ARDOUR::Port*) 0);
		}
	}
}

XMLNode&
SurfacePort::get_state ()
{
	XMLNode* node = new XMLNode (X_("Port"));

	if (dynamic_cast<MIDI::IPMIDIPort*>(_input_port)) {
		/* no state required for IPMidi ports */
		return *node;
	}

	XMLNode* child;

	child = new XMLNode (X_("Input"));
	child->add_child_nocopy (_async_in->get_state());
	node->add_child_nocopy (*child);


	child = new XMLNode (X_("Output"));
	child->add_child_nocopy (_async_out->get_state());
	node->add_child_nocopy (*child);

	return *node;
}

int
SurfacePort::set_state (const XMLNode& node, int version)
{
	if (dynamic_cast<MIDI::IPMIDIPort*>(_input_port)) {
		return 0;
	}

	XMLNode* child;

	if ((child = node.child (X_("Input"))) != 0) {
		XMLNode* portnode = child->child (Port::state_node_name.c_str());
		if (portnode) {
			portnode->remove_property ("name");
			_async_in->set_state (*portnode, version);
		}
	}

	if ((child = node.child (X_("Output"))) != 0) {
		XMLNode* portnode = child->child (Port::state_node_name.c_str());
		if (portnode) {
			portnode->remove_property ("name");
			_async_out->set_state (*portnode, version);
		}
	}

	return 0;
}

void
SurfacePort::reconnect ()
{
	_async_out->reconnect ();
	_async_in->reconnect ();

}

std::string
SurfacePort::input_name () const
{
	return _async_in->name();
}

std::string
SurfacePort::output_name () const
{
	return _async_out->name();
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

#ifndef NDEBUG
	/* skip meter output since it makes too much output for normal use */
	if (mba[0] != 0xd0 && mba[0] != 0xd1) {
		DEBUG_TRACE (DEBUG::MackieControl, string_compose ("port %1 write %2\n", output_port().name(), mba));
	}
#endif

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
