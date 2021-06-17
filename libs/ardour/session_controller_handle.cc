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

#include "ardour/boost_debug.h"
#include "ardour/session.h"
#include "ardour/session_controller.h"
#include "ardour/session_controller_handle.h"

#include "pbd/i18n.h"

using namespace std;
using namespace ARDOUR;
using namespace PBD;

SessionControllerHandleRef::SessionControllerHandleRef (Session& s)
	: SessionHandleRef (s)
	, _controller (&s)
{
	_session.DropReferences.connect_same_thread (*this, boost::bind (&SessionControllerHandleRef::session_going_away, this));
	_session.Destroyed.connect_same_thread (*this, boost::bind (&SessionControllerHandleRef::insanity_check, this));
}

SessionControllerHandleRef::~SessionControllerHandleRef ()
{
}

/*-------------------------*/

SessionControllerHandlePtr::SessionControllerHandlePtr (Session* s)
	: SessionHandlePtr (s)
	, _controller (s)
{
}

SessionControllerHandlePtr::SessionControllerHandlePtr ()
	: SessionHandlePtr ()
	, _controller (NULL)
{
}

SessionControllerHandlePtr::~SessionControllerHandlePtr ()
{
}

void
SessionControllerHandlePtr::set_session (Session* s)
{
	SessionHandlePtr::set_session (s);

	_controller = SessionController (s);
}
