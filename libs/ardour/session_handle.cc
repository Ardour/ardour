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

#include "pbd/demangle.h"
#include "pbd/error.h"
#include "pbd/boost_debug.h"

#include "ardour/session.h"
#include "ardour/session_handle.h"

#include "i18n.h"

using namespace std;
using namespace ARDOUR;
using namespace PBD;

SessionHandlePtr::SessionHandlePtr (Session* s)
	: _session (s)
{
	if (_session) {
		_session->DropReferences.connect_same_thread (_session_connections, boost::bind (&SessionHandlePtr::session_going_away, this));
	}
}

void
SessionHandlePtr::set_session (Session* s)
{
	_session_connections.drop_connections ();

	if (_session) {
		_session = 0;
	}

	if (s) {
		_session = s;
		_session->DropReferences.connect_same_thread (_session_connections, boost::bind (&SessionHandlePtr::session_going_away, this));
	}
}

void
SessionHandlePtr::session_going_away ()
{
	set_session (0);
}

/*-------------------------*/


SessionHandleRef::SessionHandleRef (Session& s)
	: _session (s)
{
	_session.DropReferences.connect_same_thread (*this, boost::bind (&SessionHandleRef::session_going_away, this));
	_session.Destroyed.connect_same_thread (*this, boost::bind (&SessionHandleRef::insanity_check, this));
}

SessionHandleRef::~SessionHandleRef ()
{
}

void
SessionHandleRef::session_going_away ()
{
	/* a handleRef is allowed to exist at the time of DropReferences, but not at the time of Destroyed
	 */
}

void
SessionHandleRef::insanity_check ()
{
	cerr << string_compose (
	        _("programming error: %1"),
	        string_compose("SessionHandleRef exists across session deletion! Dynamic type: %1 @ %2",
	                       PBD::demangled_name (*this), this))
	     << endl;
}
