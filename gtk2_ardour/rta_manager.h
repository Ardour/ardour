/*
 * Copyright (C) 2025 Robin Gareus <robin@gareus.org>
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

#include "ardour/ardour.h"
#include "ardour/dsp_filter.h"
#include "ardour/session_handle.h"
#include "ardour/types.h"

class RTAManager
	: public ARDOUR::SessionHandlePtr
	, public PBD::ScopedConnectionList
{
public:
	static RTAManager* instance ();
	~RTAManager ();

	void set_session (ARDOUR::Session*);
	XMLNode& get_state () const;

	void attach (std::shared_ptr<ARDOUR::Route>);
	void remove (std::shared_ptr<ARDOUR::Route>);
	bool attached (std::shared_ptr<ARDOUR::Route>) const;

	void run_rta ();
	void set_active (bool);

private:
	RTAManager ();
	static RTAManager* _instance;

	void session_going_away ();

	sigc::connection _update_connection;
};
