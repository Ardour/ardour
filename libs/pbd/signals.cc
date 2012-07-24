/*
    Copyright (C) 2009 Paul Davis 

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

#include "pbd/signals.h"
#include "pbd/demangle.h"

using namespace PBD;

ScopedConnectionList::ScopedConnectionList()
{
}

ScopedConnectionList::~ScopedConnectionList()
{
	drop_connections ();
}

void
ScopedConnectionList::add_connection (const UnscopedConnection& c)
{
	Glib::Threads::Mutex::Lock lm (_lock);
	_list.push_back (new ScopedConnection (c));
}

void
ScopedConnectionList::drop_connections ()
{
	Glib::Threads::Mutex::Lock lm (_lock);

	for (ConnectionList::iterator i = _list.begin(); i != _list.end(); ++i) {
		delete *i;
	}

	_list.clear ();
}

