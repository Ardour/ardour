/*
    Copyright (C) 2007 Paul Davis 

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

#include <pbd/error.h>
#include <ardour/internal_port.h>
#include <ardour/audioengine.h>

#include "i18n.h"

using namespace ARDOUR;
using namespace std;

AudioEngine* InternalPort::engine = 0;

void
InternalPort::set_engine (AudioEngine* e) 
{
	engine = e;
}

InternalPort::InternalPort (const string& str, DataType type, Flags flags)
{
	set_name (str);
	_type = type;
	_flags = flags;
}

InternalPort::~InternalPort ()
{
	disconnect ();
}

void
InternalPort::set_latency (nframes_t val)
{
	_latency = val;
}

bool
InternalPort::connected_to (const string& portname) const
{
	/* caller must hold process lock */

	for (list<InternalPort*>::const_iterator p = _connections.begin(); p != _connections.end(); ++p) {
		if ((*p)->name() == portname) {
			return true;
		}
	}

	return false;
}

const char** 
InternalPort::get_connections () const
{
	/* caller must hold process lock */

	int i;
	list<InternalPort*>::const_iterator p;

	if (_connections.empty()) {
		return 0;
	}

	char **names = (char**) malloc (sizeof (char*) * ( _connections.size() + 1));
	

	for (i = 0, p = _connections.begin(); p != _connections.end(); ++p, ++i) {
		names[i] = (char*) (*p)->name().c_str();
	}

	names[i] = 0;

	return (const char**) names;
}

int
InternalPort::connected() const 
{
	/* caller must hold process lock */
	return !_connections.empty();
}

int
InternalPort::set_name (string str)
{
	_name = "internal:";
	_name += str;

	return 0;
}

string
InternalPort::short_name () 
{
	return _name.substr (9);
}

void
InternalPort::connect (InternalPort& src, InternalPort& dst)
{
	/* caller must hold process lock */

	src._connections.push_back (&dst);
	dst._connections.push_back (&src);
}

void
InternalPort::disconnect (InternalPort& a, InternalPort& b)
{
	/* caller must hold process lock */
	a._connections.remove (&b);
	b._connections.remove (&a);
}

int
InternalPort::disconnect ()
{
	/* caller must hold process lock */

	for (list<InternalPort*>::const_iterator p = _connections.begin(); p != _connections.end(); ) {
		list<InternalPort*>::const_iterator tmp;

		tmp = p;
		++tmp;
		       
		disconnect (*this, **p);
		
		p = tmp;
	}

	_connections.clear ();
	
	return 0;
}

int
InternalPort::reestablish ()
{
	return 0;
}

void
InternalPort::recompute_total_latency () const
{
	return;
}

