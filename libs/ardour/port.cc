/*
    Copyright (C) 2002-2006 Paul Davis 

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

#include <ardour/port.h>

using namespace ARDOUR;
using namespace std;

Port::Port (jack_port_t *p) 
	: _port (p)
	, _metering(0)
	, _last_monitor(false)
{
	if (_port == 0) {
		throw failed_constructor();
	}
	
	_flags = JackPortFlags (jack_port_flags (_port));
	_type  = jack_port_type (_port); 
	_name = jack_port_name (_port);

	reset ();
}

void
Port::reset ()
{
	_last_monitor = false;
}

int 
Port::set_name (string str)
{
	int ret;

	if ((ret = jack_port_set_name (_port, str.c_str())) == 0) {
		_name = str;
	}
	
	return ret;
}

	
