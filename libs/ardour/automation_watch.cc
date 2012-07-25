/*
    Copyright (C) 2012 Paul Davis

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

#include <iostream>

#include "pbd/compose.h"

#include "ardour/automation_control.h"
#include "ardour/automation_watch.h"
#include "ardour/debug.h"
#include "ardour/session.h"

using namespace ARDOUR;
using namespace PBD;
using std::cerr;
using std::endl;

AutomationWatch* AutomationWatch::_instance = 0;

AutomationWatch&
AutomationWatch::instance ()
{
	if (_instance == 0) {
		_instance = new AutomationWatch;
	}
	return *_instance;
}

AutomationWatch::AutomationWatch ()
	: _thread (0)
	, _run_thread (false)
{
	
}

AutomationWatch::~AutomationWatch ()
{
	if (_thread) {
		_run_thread = false;
		_thread->join ();
		_thread = 0;
	}

	Glib::Threads::Mutex::Lock lm (automation_watch_lock);
	automation_watches.clear ();
}

void
AutomationWatch::add_automation_watch (boost::shared_ptr<AutomationControl> ac)
{
	Glib::Threads::Mutex::Lock lm (automation_watch_lock);
	DEBUG_TRACE (DEBUG::Automation, string_compose ("now watching control %1 for automation\n", ac->name()));
	automation_watches.push_back (ac);

	/* if an automation control is added here while the transport is
	 * rolling, make sure that it knows that there is a write pass going
	 * on, rather than waiting for the transport to start.
	 */

	if (_session && _session->transport_rolling() && ac->alist()->automation_write()) {
		DEBUG_TRACE (DEBUG::Automation, string_compose ("\ttransport is rolling @ %1, so enter write pass\n", _session->transport_speed()));
		ac->list()->set_in_write_pass (true);
	}

	/* we can't store shared_ptr<Destructible> in connections because it
	 * creates reference cycles. we don't need to make the weak_ptr<>
	 * explicit here, but it helps to remind us what is going on.
	 */
	
	boost::weak_ptr<AutomationControl> wac (ac);
	ac->DropReferences.connect_same_thread (*this, boost::bind (&AutomationWatch::remove_weak_automation_watch, this, wac));
}

void
AutomationWatch::remove_weak_automation_watch (boost::weak_ptr<AutomationControl> wac)
{
	boost::shared_ptr<AutomationControl> ac = wac.lock();

	if (!ac) {
		return;
	}

	remove_automation_watch (ac);
}

void
AutomationWatch::remove_automation_watch (boost::shared_ptr<AutomationControl> ac)
{
	Glib::Threads::Mutex::Lock lm (automation_watch_lock);
	DEBUG_TRACE (DEBUG::Automation, string_compose ("remove control %1 from automation watch\n", ac->name()));
	automation_watches.remove (ac);
	ac->list()->set_in_write_pass (false);
}

gint
AutomationWatch::timer ()
{
	if (!_session || !_session->transport_rolling()) {
		return TRUE;
	}

	{
		Glib::Threads::Mutex::Lock lm (automation_watch_lock);
		
		framepos_t time = _session->audible_frame ();

		for (AutomationWatches::iterator aw = automation_watches.begin(); aw != automation_watches.end(); ++aw) {
			if ((*aw)->alist()->automation_write()) {
				(*aw)->list()->add (time, (*aw)->user_double());
			}
		}
	}

	return TRUE;
}

void
AutomationWatch::thread ()
{
	while (_run_thread) {
		usleep (100000); // Config->get_automation_interval() * 10);
		timer ();
	}
}

void
AutomationWatch::set_session (Session* s)
{
	transport_connection.disconnect ();

	if (_thread) {
		_run_thread = false;
		_thread->join ();
		_thread = 0;
	}

	SessionHandlePtr::set_session (s);

	if (_session) {
		_run_thread = true;
		_thread = Glib::Threads::Thread::create (boost::bind (&AutomationWatch::thread, this));
		
		_session->TransportStateChange.connect_same_thread (transport_connection, boost::bind (&AutomationWatch::transport_state_change, this));
	}
}

void
AutomationWatch::transport_state_change ()
{
	if (!_session) {
		return;
	}

	bool rolling = _session->transport_rolling();

	{
		Glib::Threads::Mutex::Lock lm (automation_watch_lock);

		for (AutomationWatches::iterator aw = automation_watches.begin(); aw != automation_watches.end(); ++aw) {
			DEBUG_TRACE (DEBUG::Automation, string_compose ("%1: transport state changed, speed %2, in write pass ? %3 writing ? %4\n", 
									(*aw)->name(), _session->transport_speed(), rolling,
									(*aw)->alist()->automation_write()));
			if (rolling && (*aw)->alist()->automation_write()) {
				(*aw)->list()->set_in_write_pass (true);
			} else {
				(*aw)->list()->set_in_write_pass (false);
			}
		}
	}
}
