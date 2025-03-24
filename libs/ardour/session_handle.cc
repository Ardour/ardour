/*
 * Copyright (C) 2009-2016 Paul Davis <paul@linuxaudiosystems.com>
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

#include "pbd/demangle.h"
#include "pbd/error.h"

#ifdef TRACE_SETSESSION_NULL
#include <cassert>
#include "pbd/stacktrace.h"
#endif

#include "ardour/boost_debug.h"
#include "ardour/session.h"
#include "ardour/session_handle.h"

#include "pbd/i18n.h"

using namespace std;
using namespace ARDOUR;
using namespace PBD;

SessionHandlePtr::SessionHandlePtr ()
	: _session (0)
#ifdef TRACE_SETSESSION_NULL
	, _gone_away_emitted (false)
#endif
{
}

SessionHandlePtr::SessionHandlePtr (Session* s)
	: _session (s)
#ifdef TRACE_SETSESSION_NULL
	, _gone_away_emitted (false)
#endif
{
	if (_session) {
		_session->DropReferences.connect_same_thread (_session_connections, std::bind (&SessionHandlePtr::session_going_away, this));
	}
}

void
SessionHandlePtr::set_session (Session* s)
{
	_session_connections.drop_connections ();

#ifdef TRACE_SETSESSION_NULL
	/* DropReferences may already have been disconnected due to signal emission ordering.
	 *
	 * An instance of this class (e.g. Ardour_UI) will need to call ::set_session() on member instances.
	 *
	 * Yet, when session_going_away() first calls set_session (0) on an instance that has SessionHandlePtr members,
	 * they will reach here, and disocnnect signal handlers. Their derived implementation of ::session_going_away()
	 * will not be called.
	 */
	if (!_gone_away_emitted && _session && !s) {
		/* if this assert goes off, some ::set_session() implementation calls
		 * some_member->set_session (0);
		 *
		 * replace it with
		 *
		 * if (session) { 
		 *   some_member->set_session (session);
		 * }
		 */
		PBD::stacktrace (cerr, 10);
		assert (0);
		_gone_away_emitted = true;
		session_going_away ();
	}
#endif

	if (_session) {
		_session = 0;
	}

	if (s) {
		_session = s;
		_session->DropReferences.connect_same_thread (_session_connections, std::bind (&SessionHandlePtr::session_going_away, this));
#ifdef TRACE_SETSESSION_NULL
		_gone_away_emitted = false;
#endif
	}
}

void
SessionHandlePtr::session_going_away ()
{
#ifdef TRACE_SETSESSION_NULL
	if (_session && !_gone_away_emitted) {
		_gone_away_emitted = true;
		set_session (0);
	}
#else
	set_session (0);
#endif
}

/*-------------------------*/


SessionHandleRef::SessionHandleRef (Session& s)
	: _session (s)
{
	_session.DropReferences.connect_same_thread (*this, std::bind (&SessionHandleRef::session_going_away, this));
	_session.Destroyed.connect_same_thread (*this, std::bind (&SessionHandleRef::insanity_check, this));
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
#ifndef NDEBUG
	cerr << string_compose (
	        _("programming error: %1"),
	        string_compose("SessionHandleRef exists across session deletion! Dynamic type: %1 @ %2",
	                       PBD::demangled_name (*this), this))
	     << endl;
#endif
}
