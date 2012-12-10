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

#include "ardour/session.h"
#include "ardour/route.h"
#include "ardour/track.h"

#include "i18n.h"

using namespace std;
using namespace PBD;
using namespace ARDOUR;
using namespace Glib;

void
Session::set_monitoring (boost::shared_ptr<RouteList> rl, MonitorChoice mc, SessionEvent::RTeventCallback after, bool group_override)
{
	queue_event (get_rt_event (rl, mc, after, group_override, &Session::rt_set_monitoring));
}

void
Session::rt_set_monitoring (boost::shared_ptr<RouteList> rl, MonitorChoice mc, bool /* group_override */)
{
	for (RouteList::iterator i = rl->begin(); i != rl->end(); ++i) {
		if (!(*i)->is_hidden()) {
			boost::shared_ptr<Track> t = boost::dynamic_pointer_cast<Track> (*i);
			if (t) {
				t->set_monitoring (mc);
			}
		}
	}

	set_dirty();
}

void
Session::set_solo (boost::shared_ptr<RouteList> rl, bool yn, SessionEvent::RTeventCallback after, bool group_override)
{
	queue_event (get_rt_event (rl, yn, after, group_override, &Session::rt_set_solo));
}

void
Session::rt_set_solo (boost::shared_ptr<RouteList> rl, bool yn, bool /* group_override */)
{
	for (RouteList::iterator i = rl->begin(); i != rl->end(); ++i) {
		if (!(*i)->is_hidden()) {
			(*i)->set_solo (yn, this);
		}
	}

	set_dirty();
}

void
Session::cancel_solo_after_disconnect (boost::shared_ptr<Route> r, bool upstream, SessionEvent::RTeventCallback after)
{
	boost::shared_ptr<RouteList> rl (new RouteList);
	rl->push_back (r);

	queue_event (get_rt_event (rl, upstream, after, false, &Session::rt_cancel_solo_after_disconnect));
}

void
Session::rt_cancel_solo_after_disconnect (boost::shared_ptr<RouteList> rl, bool upstream, bool /* group_override */)
{
	for (RouteList::iterator i = rl->begin(); i != rl->end(); ++i) {
		if (!(*i)->is_hidden()) {
			(*i)->cancel_solo_after_disconnect (upstream);
		}
	}
	/* no need to call set-dirty - the disconnect will already have done that */
}

void
Session::set_just_one_solo (boost::shared_ptr<Route> r, bool yn, SessionEvent::RTeventCallback after)
{
	/* its a bit silly to have to do this, but it keeps the API for this public method sane (we're
	   only going to solo one route) and keeps our ability to use get_rt_event() for the internal
	   private method.
	*/

	boost::shared_ptr<RouteList> rl (new RouteList);
	rl->push_back (r);

	queue_event (get_rt_event (rl, yn, after, false, &Session::rt_set_just_one_solo));
}

void
Session::rt_set_just_one_solo (boost::shared_ptr<RouteList> just_one, bool yn, bool /*ignored*/)
{
	boost::shared_ptr<RouteList> rl = routes.reader ();
	boost::shared_ptr<Route> r = just_one->front();

	for (RouteList::iterator i = rl->begin(); i != rl->end(); ++i) {
		if (!(*i)->is_hidden() && r != *i) {
			(*i)->set_solo (!yn, (*i)->route_group());
		}
	}

	r->set_solo (yn, r->route_group());

	set_dirty();
}

void
Session::set_listen (boost::shared_ptr<RouteList> rl, bool yn, SessionEvent::RTeventCallback after, bool group_override)
{
	queue_event (get_rt_event (rl, yn, after, group_override, &Session::rt_set_listen));
}

void
Session::rt_set_listen (boost::shared_ptr<RouteList> rl, bool yn, bool /*group_override*/ )
{
	for (RouteList::iterator i = rl->begin(); i != rl->end(); ++i) {
		if (!(*i)->is_hidden()) {
			(*i)->set_listen (yn, this);
		}
	}

	set_dirty();
}

void
Session::set_mute (boost::shared_ptr<RouteList> rl, bool yn, SessionEvent::RTeventCallback after, bool group_override)
{
	queue_event (get_rt_event (rl, yn, after, group_override, &Session::rt_set_mute));
}

void
Session::rt_set_mute (boost::shared_ptr<RouteList> rl, bool yn, bool /*group_override*/)
{
	for (RouteList::iterator i = rl->begin(); i != rl->end(); ++i) {
		if (!(*i)->is_hidden()) {
			(*i)->set_mute (yn, this);
		}
	}

	set_dirty();
}

void
Session::set_solo_isolated (boost::shared_ptr<RouteList> rl, bool yn, SessionEvent::RTeventCallback after, bool group_override)
{
	queue_event (get_rt_event (rl, yn, after, group_override, &Session::rt_set_solo_isolated));
}

void
Session::rt_set_solo_isolated (boost::shared_ptr<RouteList> rl, bool yn, bool /*group_override*/)
{
	for (RouteList::iterator i = rl->begin(); i != rl->end(); ++i) {
		if (!(*i)->is_master() && !(*i)->is_monitor() && !(*i)->is_hidden()) {
			(*i)->set_solo_isolated (yn, this);
		}
	}

	set_dirty();
}

void
Session::set_record_enabled (boost::shared_ptr<RouteList> rl, bool yn, SessionEvent::RTeventCallback after, bool group_override)
{
	if (!writable()) {
		return;
	}

	/* do the non-RT part of rec-enabling first - the RT part will be done
	 * on the next process cycle. This does mean that theoretically we are
	 * doing things provisionally on the assumption that the rec-enable
	 * change will work, but this had better be a solid assumption for
	 * other reasons.
	 */

	for (RouteList::iterator i = rl->begin(); i != rl->end(); ++i) {
		if ((*i)->is_hidden()) {
			continue;
		}

		boost::shared_ptr<Track> t;

		if ((t = boost::dynamic_pointer_cast<Track>(*i)) != 0) {
			t->prep_record_enabled (yn, (group_override ? (void*) t->route_group() : (void *) this));
		}
	}

	queue_event (get_rt_event (rl, yn, after, group_override, &Session::rt_set_record_enabled));
}

void
Session::rt_set_record_enabled (boost::shared_ptr<RouteList> rl, bool yn, bool group_override)
{
	for (RouteList::iterator i = rl->begin(); i != rl->end(); ++i) {
		if ((*i)->is_hidden()) {
			continue;
		}

		boost::shared_ptr<Track> t;

		if ((t = boost::dynamic_pointer_cast<Track>(*i)) != 0) {
			t->set_record_enabled (yn, (group_override ? (void*) t->route_group() : (void *) this));
		}
	}

	set_dirty ();
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
