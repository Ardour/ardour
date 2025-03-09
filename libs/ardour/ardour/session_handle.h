/*
 * Copyright (C) 2009-2017 Paul Davis <paul@linuxaudiosystems.com>
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

#pragma once

#include "pbd/signals.h"

#include "ardour/libardour_visibility.h"

#ifndef NDEBUG
# define TRACE_SETSESSION_NULL
#endif

namespace ARDOUR {
	class Session;

class LIBARDOUR_API SessionHandleRef : public PBD::ScopedConnectionList
{
public:
	SessionHandleRef (ARDOUR::Session& s);
	virtual ~SessionHandleRef ();

protected:
	virtual void session_going_away ();
	virtual void insanity_check ();

	ARDOUR::Session& _session;
};

class LIBARDOUR_API SessionHandlePtr
{
public:
	SessionHandlePtr (ARDOUR::Session* s);
	SessionHandlePtr ();
	virtual ~SessionHandlePtr () {}

	virtual void set_session (ARDOUR::Session *);
	virtual ARDOUR::Session* session() const { return _session; }

protected:
	virtual void session_going_away ();

	ARDOUR::Session*          _session;
	PBD::ScopedConnectionList _session_connections;

#ifdef TRACE_SETSESSION_NULL
private:
	bool _gone_away_emitted;
#endif
};

} /* namespace */

