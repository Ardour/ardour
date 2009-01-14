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

*/

#include <algorithm>

#include <pbd/failed_constructor.h>
#include <ardour/ardour.h>
#include <ardour/bundle.h>
#include <pbd/xml++.h>

#include "i18n.h"

using namespace ARDOUR;
using namespace PBD;

uint32_t
Bundle::nchannels () const
{
	Glib::Mutex::Lock lm (_ports_mutex);
	return _ports.size ();
}

Bundle::PortList const &
Bundle::channel_ports (uint32_t c) const
{
	assert (c < nchannels());

	Glib::Mutex::Lock lm (_ports_mutex);
	return _ports[c];
}

/** Add an association between one of our channels and a port.
 *  @param ch Channel index.
 *  @param portname port name to associate with.
 */
void
Bundle::add_port_to_channel (uint32_t ch, string portname)
{
	assert (ch < nchannels());

	{
		Glib::Mutex::Lock lm (_ports_mutex);
		_ports[ch].push_back (portname);
	}
	
	PortsChanged (ch); /* EMIT SIGNAL */
}

/** Disassociate a port from one of our channels.
 *  @param ch Channel index.
 *  @param portname port name to disassociate from.
 */
void
Bundle::remove_port_from_channel (uint32_t ch, string portname)
{
	assert (ch < nchannels());

	bool changed = false;

	{
		Glib::Mutex::Lock lm (_ports_mutex);
		PortList& pl = _ports[ch];
		PortList::iterator i = find (pl.begin(), pl.end(), portname);
		
		if (i != pl.end()) {
			pl.erase (i);
			changed = true;
		}
	}

	if (changed) {
		 PortsChanged (ch); /* EMIT SIGNAL */
	}
}

/** operator== for Bundles; they are equal if their channels are the same.
 * @param other Bundle to compare with this one.
 */
bool
Bundle::operator== (const Bundle& other) const
{
	return other._ports == _ports;
}


/** Set the number of channels.
 * @param n New number of channels.
 */

void
Bundle::set_nchannels (uint32_t n)
{
	{
		Glib::Mutex::Lock lm (_ports_mutex);
		_ports.clear ();
		for (uint32_t i = 0; i < n; ++i) {
			_ports.push_back (PortList());
		}
	}

	ConfigurationChanged (); /* EMIT SIGNAL */
}

void
Bundle::set_port (uint32_t ch, string portname)
{
	assert (ch < nchannels());

	{
		Glib::Mutex::Lock lm (_ports_mutex);
		_ports[ch].clear ();
		_ports[ch].push_back (portname);
	}

	PortsChanged (ch); /* EMIT SIGNAL */
}

void
Bundle::add_channel ()
{
	{
		Glib::Mutex::Lock lm (_ports_mutex);
		_ports.push_back (PortList ());
	}

	ConfigurationChanged (); /* EMIT SIGNAL */
}

bool
Bundle::port_attached_to_channel (uint32_t ch, std::string portname)
{
	assert (ch < nchannels());
	
	Glib::Mutex::Lock lm (_ports_mutex);
	return (std::find (_ports[ch].begin (), _ports[ch].end (), portname) != _ports[ch].end ());
}

void
Bundle::remove_channel (uint32_t ch)
{
	assert (ch < nchannels ());

	Glib::Mutex::Lock lm (_ports_mutex);
	_ports.erase (_ports.begin () + ch);
}
