/*
    Copyright (C) 1999-2009 Paul Davis

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

#include <boost/bind.hpp>

#include "pbd/error.h"
#include "pbd/compose.h"

#include "ardour/monitor_control.h"
#include "ardour/route.h"
#include "ardour/session.h"
#include "ardour/track.h"
#include "ardour/vca_manager.h"

#include "pbd/i18n.h"

using namespace std;
using namespace PBD;
using namespace ARDOUR;
using namespace Glib;

void
Session::set_controls (boost::shared_ptr<ControlList> cl, double val, Controllable::GroupControlDisposition gcd)
{
	if (cl->empty()) {
		return;
	}

	for (ControlList::iterator ci = cl->begin(); ci != cl->end(); ++ci) {
		/* as of july 2017 this is a no-op for everything except record enable */
		(*ci)->pre_realtime_queue_stuff (val, gcd);
	}

	queue_event (get_rt_event (cl, val, gcd));
}

void
Session::set_control (boost::shared_ptr<AutomationControl> ac, double val, Controllable::GroupControlDisposition gcd)
{
	if (!ac) {
		return;
	}

	boost::shared_ptr<ControlList> cl (new ControlList);
	cl->push_back (ac);
	set_controls (cl, val, gcd);
}

void
Session::rt_set_controls (boost::shared_ptr<ControlList> cl, double val, Controllable::GroupControlDisposition gcd)
{
	/* Note that we require that all controls in the ControlList are of the
	   same type.
	*/
	if (cl->empty()) {
		return;
	}

	for (ControlList::iterator c = cl->begin(); c != cl->end(); ++c) {
		(*c)->set_value (val, gcd);
	}

	/* some controls need global work to take place after they are set. Do
	 * that here.
	 */

	switch (cl->front()->parameter().type()) {
	case SoloAutomation:
		update_route_solo_state ();
		break;
	default:
		break;
	}
}

void
Session::clear_all_solo_state (boost::shared_ptr<RouteList> rl)
{
	queue_event (get_rt_event (rl, false, rt_cleanup, Controllable::NoGroup, &Session::rt_clear_all_solo_state));
}

void
Session::rt_clear_all_solo_state (boost::shared_ptr<RouteList> rl, bool /* yn */, Controllable::GroupControlDisposition /* group_override */)
{
	for (RouteList::iterator i = rl->begin(); i != rl->end(); ++i) {
		if ((*i)->is_auditioner()) {
			continue;
		}
		(*i)->clear_all_solo_state();
	}

	_vca_manager->clear_all_solo_state ();

	update_route_solo_state ();
}

void
Session::process_rtop (SessionEvent* ev)
{
	ev->rt_slot ();

	if (ev->event_loop) {
		ev->event_loop->call_slot (MISSING_INVALIDATOR, boost::bind (ev->rt_return, ev));
	} else {
		warning << string_compose ("programming error: %1", X_("Session RT event queued from thread without a UI - cleanup in RT thread!")) << endmsg;
		ev->rt_return (ev);
	}
}
