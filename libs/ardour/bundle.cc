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
	Glib::Mutex::Lock lm (_channel_mutex);
	return _channel.size ();
}

Bundle::PortList const &
Bundle::channel_ports (uint32_t c) const
{
	assert (c < nchannels());

	Glib::Mutex::Lock lm (_channel_mutex);
	return _channel[c].ports;
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
		Glib::Mutex::Lock lm (_channel_mutex);
		_channel[ch].ports.push_back (portname);
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
		Glib::Mutex::Lock lm (_channel_mutex);
		PortList& pl = _channel[ch].ports;
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
	return other._channel == _channel;
}


void
Bundle::set_port (uint32_t ch, string portname)
{
	assert (ch < nchannels());

	{
		Glib::Mutex::Lock lm (_channel_mutex);
		_channel[ch].ports.clear ();
		_channel[ch].ports.push_back (portname);
	}

	PortsChanged (ch); /* EMIT SIGNAL */
}

/** @param n Channel name */
void
Bundle::add_channel (std::string const & n)
{
	{
		Glib::Mutex::Lock lm (_channel_mutex);
		_channel.push_back (Channel (n));
	}

	ConfigurationChanged (); /* EMIT SIGNAL */
}

bool
Bundle::port_attached_to_channel (uint32_t ch, std::string portname)
{
	assert (ch < nchannels());
	
	Glib::Mutex::Lock lm (_channel_mutex);
	return (std::find (_channel[ch].ports.begin (), _channel[ch].ports.end (), portname) != _channel[ch].ports.end ());
}

void
Bundle::remove_channel (uint32_t ch)
{
	assert (ch < nchannels ());

	Glib::Mutex::Lock lm (_channel_mutex);
	_channel.erase (_channel.begin () + ch);
}

void
Bundle::remove_channels ()
{
	Glib::Mutex::Lock lm (_channel_mutex);

	_channel.clear ();
}

bool
Bundle::uses_port (std::string p) const
{
	Glib::Mutex::Lock lm (_channel_mutex);

	for (std::vector<Channel>::const_iterator i = _channel.begin(); i != _channel.end(); ++i) {
		for (PortList::const_iterator j = i->ports.begin(); j != i->ports.end(); ++j) {
			if (*j == p) {
				return true;
			}
		}
	}

	return false;
}

std::string
Bundle::channel_name (uint32_t ch) const
{
	assert (ch < nchannels());

	Glib::Mutex::Lock lm (_channel_mutex);
	return _channel[ch].name;
}

void
Bundle::set_channel_name (uint32_t ch, std::string const & n)
{
	assert (ch < nchannels());

	{
		Glib::Mutex::Lock lm (_channel_mutex);
		_channel[ch].name = n;
	}

	NameChanged (); /* EMIT SIGNAL */
}
