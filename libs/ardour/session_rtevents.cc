/*
 * Copyright (C) 1999-2017 Paul Davis <paul@linuxaudiosystems.com>
 * Copyright (C) 2011-2014 David Robillard <d@drobilla.net>
 * Copyright (C) 2015 Robin Gareus <robin@gareus.org>
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

#include <boost/bind.hpp>
#include <glibmm/timer.h>

#include "pbd/error.h"
#include "pbd/compose.h"

#include "ardour/audioengine.h"
#include "ardour/butler.h"
#include "ardour/monitor_control.h"
#include "ardour/route.h"
#include "ardour/session.h"
#include "ardour/solo_mute_release.h"
#include "ardour/track.h"
#include "ardour/vca_manager.h"

#include "pbd/i18n.h"

using namespace std;
using namespace PBD;
using namespace ARDOUR;
using namespace Glib;

void
Session::set_controls (std::shared_ptr<AutomationControlList> cl, double val, Controllable::GroupControlDisposition gcd)
{
	if (cl->empty()) {
		return;
	}

#if 1
	/* This is called by the GUI thread, so we can wait if neccessary to prevent
	 * "POOL OUT OF MEMORY" fatal errors.
	 *
	 * This is not a good solution, because if this happens
	 * event_loop->call_slot() will most likely also fail to queue a request
	 * to delete the Events. There is likely an additional Changed() signal
	 * which needds a EventLoop RequestBuffer slot.
	 *
	 * Ideally the EventLoop RequestBuffer would be at least twice the size
	 * of the the SessionEvent Pool, but it isn't, and even then there may
	 * still be other signals scheduling events...
	 */
	if (SessionEvent::pool_available () < 8) {
		int sleeptm = std::max (40000, engine().usecs_per_cycle ());
		int timeout = std::max (10, 1000000 / sleeptm);
		do {
			Glib::usleep (sleeptm);
			ARDOUR::GUIIdle ();
		}
		while (SessionEvent::pool_available () < 8 && --timeout > 0);
	}
#endif

	std::shared_ptr<WeakAutomationControlList> wcl (new WeakAutomationControlList);
	for (AutomationControlList::iterator ci = cl->begin(); ci != cl->end(); ++ci) {
		/* as of july 2017 this is a no-op for everything except record enable */
		(*ci)->pre_realtime_queue_stuff (val, gcd);
		/* fill in weak pointer ctrl list */
		wcl->push_back (*ci);
	}

	queue_event (get_rt_event (wcl, val, gcd));
}

void
Session::set_control (std::shared_ptr<AutomationControl> ac, double val, Controllable::GroupControlDisposition gcd)
{
	if (!ac) {
		return;
	}

	std::shared_ptr<AutomationControlList> cl (new AutomationControlList);
	cl->push_back (ac);
	set_controls (cl, val, gcd);
}

void
Session::rt_set_controls (std::shared_ptr<WeakAutomationControlList> cl, double val, Controllable::GroupControlDisposition gcd)
{
	/* Note that we require that all controls in the ControlList are of the
	   same type.
	*/
	if (cl->empty()) {
		return;
	}

	bool update_solo_state = false;

	for (auto const& c : *cl) {
		std::shared_ptr<AutomationControl> ac = c.lock ();
		if (ac) {
			ac->set_value (val, gcd);
			update_solo_state |= SoloAutomation == ac->desc().type;
		}
	}

	/* some controls need global work to take place after they are set. Do
	 * that here.
	 */

	if (update_solo_state) {
		update_route_solo_state ();
	}
}

void
Session::prepare_momentary_solo (SoloMuteRelease* smr, bool exclusive, std::shared_ptr<Route> route)
{
	std::shared_ptr<StripableList> routes_on (new StripableList);
	std::shared_ptr<StripableList> routes_off (new StripableList);
	std::shared_ptr<RouteList const> routes = get_routes();

	for (auto const & r : *routes) {
#ifdef MIXBUS
		if (route && (0 == route->mixbus()) != (0 == r->mixbus ())) {
			continue;
		}
#endif
		if (r->soloed ()) {
			routes_on->push_back (r);
		} else if (smr) {
			routes_off->push_back (r);
		}
	}

	if (exclusive) {
		set_controls (stripable_list_to_control_list (routes_on, &Stripable::solo_control), false, Controllable::UseGroup);
	}

	if (smr) {
		smr->set (routes_on, routes_off);
	}

	if (_monitor_out) {
		if (smr) {
			std::shared_ptr<std::list<std::string> > pml (new std::list<std::string>);
			_engine.monitor_port().active_monitors (*pml);
			smr->set (pml);
		}
		if (exclusive) {
			/* unset any input monitors */
			_engine.monitor_port().clear_ports (false);
		}
	}
}

void
Session::clear_all_solo_state (std::shared_ptr<RouteList const> rl)
{
	queue_event (get_rt_event (rl, false, rt_cleanup, Controllable::NoGroup, &Session::rt_clear_all_solo_state));
}

void
Session::rt_clear_all_solo_state (std::shared_ptr<RouteList const> rl, bool /* yn */, Controllable::GroupControlDisposition /* group_override */)
{
	for (auto const& i : *rl) {
		if (i->is_auditioner()) {
			continue;
		}
		i->clear_all_solo_state();
	}

	_vca_manager->clear_all_solo_state ();

	update_route_solo_state ();
}

void
Session::process_rtop (SessionEvent* ev)
{
	ev->rt_slot ();

	if (ev->event_loop) {
		if (!ev->event_loop->call_slot (MISSING_INVALIDATOR, boost::bind (ev->rt_return, ev))) {
			/* The event must be deleted, otherwise the SessionEvent Pool may fill up */
			if (!butler ()->delegate (boost::bind (ev->rt_return, ev))) {
				ev->rt_return (ev);
			}
		}
	} else {
		warning << string_compose ("programming error: %1", X_("Session RT event queued from thread without a UI - cleanup in RT thread!")) << endmsg;
		ev->rt_return (ev);
	}
}
