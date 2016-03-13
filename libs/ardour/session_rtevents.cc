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
Session::set_monitoring (boost::shared_ptr<RouteList> rl, MonitorChoice mc, 
                         SessionEvent::RTeventCallback after,
                         Controllable::GroupControlDisposition group_override)
{
	queue_event (get_rt_event (rl, mc, after, group_override, &Session::rt_set_monitoring));
}

void
Session::rt_set_monitoring (boost::shared_ptr<RouteList> rl, MonitorChoice mc, Controllable::GroupControlDisposition group_override)
{
	for (RouteList::iterator i = rl->begin(); i != rl->end(); ++i) {
		if (!(*i)->is_auditioner()) {
			boost::shared_ptr<Track> t = boost::dynamic_pointer_cast<Track> (*i);
			if (t) {
				t->set_monitoring (mc, group_override);
			}
		}
	}

	set_dirty();
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
	set_dirty();
}

void
Session::set_solo (boost::shared_ptr<RouteList> rl, bool yn, SessionEvent::RTeventCallback after,
                   Controllable::GroupControlDisposition group_override)
{
	queue_event (get_rt_event (rl, yn, after, group_override, &Session::rt_set_solo));
}

void
Session::rt_set_solo (boost::shared_ptr<RouteList> rl, bool yn, Controllable::GroupControlDisposition group_override)
{
	for (RouteList::iterator i = rl->begin(); i != rl->end(); ++i) {
		if (!(*i)->is_auditioner()) {
			(*i)->set_solo (yn, group_override);
		}
	}

	set_dirty();

	/* XXX boost::shared_ptr<RouteList>  goes out of scope here and is likley free()ed in RT context
	 * because boost's shared_ptr does reference counting and free/delete in the dtor.
	 * (this also applies to other rt_  methods here)
	 */
}

void
Session::set_implicit_solo (boost::shared_ptr<RouteList> rl, int delta, bool upstream, SessionEvent::RTeventCallback after,
                   Controllable::GroupControlDisposition group_override)
{
	queue_event (get_rt_event (rl, delta, upstream, after, group_override, &Session::rt_set_implicit_solo));
}

void
Session::rt_set_implicit_solo (boost::shared_ptr<RouteList> rl, int delta, bool upstream, PBD::Controllable::GroupControlDisposition)
{
	for (RouteList::iterator i = rl->begin(); i != rl->end(); ++i) {
		if (!(*i)->is_auditioner()) {
			if (upstream) {
				std::cerr << "Changing " << (*i)->name() << " upstream by " << delta << std::endl;
				(*i)->mod_solo_by_others_upstream (delta);
			} else {
				std::cerr << "Changing " << (*i)->name() << " downstream by " << delta << std::endl;
				(*i)->mod_solo_by_others_downstream (delta);
			}
		}
	}

	set_dirty();

	/* XXX boost::shared_ptr<RouteList>  goes out of scope here and is likley free()ed in RT context
	 * because boost's shared_ptr does reference counting and free/delete in the dtor.
	 * (this also applies to other rt_  methods here)
	 */
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

	queue_event (get_rt_event (rl, yn, after, Controllable::NoGroup, &Session::rt_set_just_one_solo));
}

void
Session::rt_set_just_one_solo (boost::shared_ptr<RouteList> just_one, bool yn, Controllable::GroupControlDisposition /*ignored*/)
{
	boost::shared_ptr<RouteList> rl = routes.reader ();
	boost::shared_ptr<Route> r = just_one->front();

	for (RouteList::iterator i = rl->begin(); i != rl->end(); ++i) {
		if (!(*i)->is_auditioner() && r != *i) {
			(*i)->set_solo (!yn, Controllable::NoGroup);
		}
	}

	r->set_solo (yn, Controllable::NoGroup);

	set_dirty();
}

void
Session::set_listen (boost::shared_ptr<RouteList> rl, bool yn, SessionEvent::RTeventCallback after, Controllable::GroupControlDisposition group_override)
{
	queue_event (get_rt_event (rl, yn, after, group_override, &Session::rt_set_listen));
}

void
Session::rt_set_listen (boost::shared_ptr<RouteList> rl, bool yn, Controllable::GroupControlDisposition group_override)
{
	for (RouteList::iterator i = rl->begin(); i != rl->end(); ++i) {
		if (!(*i)->is_auditioner()) {
			(*i)->set_listen (yn, group_override);
		}
	}

	set_dirty();
}

void
Session::set_mute (boost::shared_ptr<RouteList> rl, bool yn, SessionEvent::RTeventCallback after, Controllable::GroupControlDisposition group_override)
{
	/* Set superficial value of mute controls for automation. */
	for (RouteList::iterator i = rl->begin(); i != rl->end(); ++i) {
		boost::shared_ptr<Route::MuteControllable> mc = boost::dynamic_pointer_cast<Route::MuteControllable> ((*i)->mute_control());
		mc->set_superficial_value(yn);
	}

	queue_event (get_rt_event (rl, yn, after, group_override, &Session::rt_set_mute));
}

void
Session::rt_set_mute (boost::shared_ptr<RouteList> rl, bool yn, Controllable::GroupControlDisposition group_override)
{
	for (RouteList::iterator i = rl->begin(); i != rl->end(); ++i) {
		if (!(*i)->is_monitor() && !(*i)->is_auditioner()) {
			(*i)->set_mute (yn, group_override);
		}
	}

	set_dirty();
}

void
Session::set_solo_isolated (boost::shared_ptr<RouteList> rl, bool yn, SessionEvent::RTeventCallback after, Controllable::GroupControlDisposition group_override)
{
	queue_event (get_rt_event (rl, yn, after, group_override, &Session::rt_set_solo_isolated));
}

void
Session::rt_set_solo_isolated (boost::shared_ptr<RouteList> rl, bool yn, Controllable::GroupControlDisposition group_override)
{
	for (RouteList::iterator i = rl->begin(); i != rl->end(); ++i) {
		if (!(*i)->is_master() && !(*i)->is_monitor() && !(*i)->is_auditioner()) {
			(*i)->set_solo_isolated (yn, group_override);
		}
	}

	set_dirty();
}

void
Session::set_record_enabled (boost::shared_ptr<RouteList> rl, bool yn, SessionEvent::RTeventCallback after, Controllable::GroupControlDisposition group_override)
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
		if ((*i)->is_auditioner() || (*i)->record_safe ()) {
			continue;
		}

		boost::shared_ptr<Track> t;

		if ((t = boost::dynamic_pointer_cast<Track>(*i)) != 0) {
			t->prep_record_enabled (yn, group_override);
		}
	}

	queue_event (get_rt_event (rl, yn, after, group_override, &Session::rt_set_record_enabled));
}

void
Session::rt_set_record_enabled (boost::shared_ptr<RouteList> rl, bool yn, Controllable::GroupControlDisposition group_override)
{
	for (RouteList::iterator i = rl->begin(); i != rl->end(); ++i) {
		if ((*i)->is_auditioner() || (*i)->record_safe ()) {
			continue;
		}

		boost::shared_ptr<Track> t;

		if ((t = boost::dynamic_pointer_cast<Track>(*i)) != 0) {
			t->set_record_enabled (yn, group_override);
		}
	}

	set_dirty ();
}


void
Session::set_record_safe (boost::shared_ptr<RouteList> rl, bool yn, SessionEvent::RTeventCallback after, Controllable::GroupControlDisposition group_override)
{
	set_record_enabled (rl, false, after, group_override);
	queue_event (get_rt_event (rl, yn, after, group_override, &Session::rt_set_record_safe));
}

void
Session::rt_set_record_safe (boost::shared_ptr<RouteList> rl, bool yn, Controllable::GroupControlDisposition group_override)
{
	for (RouteList::iterator i = rl->begin (); i != rl->end (); ++i) {
		if ((*i)->is_auditioner ()) { // REQUIRES REVIEW Can audiotioner be in Record Safe mode?
			continue;
		}

		boost::shared_ptr<Track> t;

		if ((t = boost::dynamic_pointer_cast<Track>(*i)) != 0) {
			t->set_record_safe (yn, group_override);
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
