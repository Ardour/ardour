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

#ifdef WAF_BUILD
#include "gtk2ardour-config.h"
#endif

#include "ardour/rt_safe_delete.h"
#include "ardour/session.h"

#include "gui_thread.h"
#include "rta_manager.h"
#include "timers.h"
#include "ui_config.h"

using namespace ARDOUR;

RTAManager* RTAManager::_instance = 0;

RTAManager*
RTAManager::instance ()
{
	if (!_instance) {
		_instance = new RTAManager;
	}
	return _instance;
}

RTAManager::RTAManager ()
{
}

RTAManager::~RTAManager ()
{
}

XMLNode&
RTAManager::get_state () const
{
	XMLNode* node = new XMLNode ("RTAManager");
	return *node;
}

void
RTAManager::set_session (ARDOUR::Session* s)
{
	if (!s) {
		return;
	}
	SessionHandlePtr::set_session (s);

	if (_session->master_out ()) {
		attach (_session->master_out ());
	}
	_update_connection = Timers::super_rapid_connect (sigc::mem_fun (*this, &RTAManager::run_rta));
}

void
RTAManager::session_going_away ()
{
	ENSURE_GUI_THREAD (*this, &RTAManager::session_going_away);
	_update_connection.disconnect ();

	SessionHandlePtr::session_going_away ();
	_session = 0;
}

void
RTAManager::set_active (bool en)
{
}

void
RTAManager::attach (std::shared_ptr<ARDOUR::Route> route)
{
}

void
RTAManager::remove (std::shared_ptr<ARDOUR::Route> route)
{
}

bool
RTAManager::attached (std::shared_ptr<ARDOUR::Route> route) const
{
	return false;
}

void
RTAManager::run_rta ()
{
}
