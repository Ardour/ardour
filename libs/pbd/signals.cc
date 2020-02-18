/*
 * Copyright (C) 2009-2015 Paul Davis <paul@linuxaudiosystems.com>
 * Copyright (C) 2012 Carl Hetherington <carl@carlh.net>
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
	Glib::Threads::Mutex::Lock lm (_scoped_connection_lock);
	_scoped_connection_list.push_back (new ScopedConnection (c));
}

void
ScopedConnectionList::drop_connections ()
{
	Glib::Threads::Mutex::Lock lm (_scoped_connection_lock);

	for (ConnectionList::iterator i = _scoped_connection_list.begin(); i != _scoped_connection_list.end(); ++i) {
		delete *i;
	}

	_scoped_connection_list.clear ();
}

