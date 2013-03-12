/*
    Copyright (C) 2012 Paul Davis 

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

#include <cassert>
#include "ardour/auto_bundle.h"

ARDOUR::AutoBundle::AutoBundle (bool i)
	: Bundle (i)
{

}

ARDOUR::AutoBundle::AutoBundle (std::string const & n, bool i)
	: Bundle (n, i)
{

}

ARDOUR::ChanCount
ARDOUR::AutoBundle::nchannels () const
{
	Glib::Threads::Mutex::Lock lm (_ports_mutex);
	return ChanCount (type(), _ports.size ());
}

const ARDOUR::PortList&
ARDOUR::AutoBundle::channel_ports (uint32_t c) const
{
	assert (c < nchannels().get (type()));

	Glib::Threads::Mutex::Lock lm (_ports_mutex);
	return _ports[c];
}

void
ARDOUR::AutoBundle::set_channels (uint32_t n)
{
	Glib::Threads::Mutex::Lock lm (_ports_mutex);
	_ports.resize (n);
}

void
ARDOUR::AutoBundle::set_port (uint32_t c, std::string const & p)
{
	assert (c < nchannels ().get (type()));

	Glib::Threads::Mutex::Lock lm (_ports_mutex);
	_ports[c].resize (1);
	_ports[c][0] = p;
}
