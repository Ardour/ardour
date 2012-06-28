/*
    Copyright (C) 1999-2004 Paul Davis

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

#include <cmath>
#include <unistd.h>

#include "pbd/error.h"
#include "pbd/enumwriter.h"
#include "pbd/stacktrace.h"

#include "ardour/debug.h"
#include "ardour/session_event.h"

#include "i18n.h"

using namespace std;
using namespace ARDOUR;
using namespace PBD;

PerThreadPool* SessionEvent::pool;

void
SessionEvent::init_event_pool ()
{
	pool = new PerThreadPool;
}

void
SessionEvent::create_per_thread_pool (const std::string& name, uint32_t nitems)
{
	/* this is a per-thread call that simply creates a thread-private ptr to
	   a CrossThreadPool for use by this thread whenever events are allocated/released
	   from SessionEvent::pool()
	*/
	pool->create_per_thread_pool (name, sizeof (SessionEvent), nitems);
}

void *
SessionEvent::operator new (size_t)
{
	CrossThreadPool* p = pool->per_thread_pool ();
	SessionEvent* ev = static_cast<SessionEvent*> (p->alloc ());
	DEBUG_TRACE (DEBUG::SessionEvents, string_compose ("%1 Allocating SessionEvent from %2 ev @ %3\n", pthread_self(), p->name(), ev));
#ifndef NDEBUG
	if (DEBUG::SessionEvents & PBD::debug_bits) {
		stacktrace (cerr, 40);
	}
#endif
	ev->own_pool = p;
	return ev;
}

void
SessionEvent::operator delete (void *ptr, size_t /*size*/)
{
	Pool* p = pool->per_thread_pool ();
	SessionEvent* ev = static_cast<SessionEvent*> (ptr);

	DEBUG_TRACE (DEBUG::SessionEvents, string_compose (
		             "%1 Deleting SessionEvent @ %2 ev thread pool = %3 ev pool = %4\n",
		             pthread_self(), ev, p->name(), ev->own_pool->name()
		             ));

#ifndef NDEBUG
	if (DEBUG::SessionEvents & PBD::debug_bits) {
		stacktrace (cerr, 40);
	}
#endif

	if (p == ev->own_pool) {
		p->release (ptr);
	} else {
		ev->own_pool->push (ev);
	}
}

void
SessionEventManager::add_event (framepos_t frame, SessionEvent::Type type, framepos_t target_frame)
{
	SessionEvent* ev = new SessionEvent (type, SessionEvent::Add, frame, target_frame, 0);
	queue_event (ev);
}

void
SessionEventManager::remove_event (framepos_t frame, SessionEvent::Type type)
{
	SessionEvent* ev = new SessionEvent (type, SessionEvent::Remove, frame, 0, 0);
	queue_event (ev);
}

void
SessionEventManager::replace_event (SessionEvent::Type type, framepos_t frame, framepos_t target)
{
	SessionEvent* ev = new SessionEvent (type, SessionEvent::Replace, frame, target, 0);
	queue_event (ev);
}

void
SessionEventManager::clear_events (SessionEvent::Type type)
{
	SessionEvent* ev = new SessionEvent (type, SessionEvent::Clear, 0, 0, 0);
	queue_event (ev);
}


void
SessionEventManager::dump_events () const
{
	cerr << "EVENT DUMP" << endl;
	for (Events::const_iterator i = events.begin(); i != events.end(); ++i) {
		cerr << "\tat " << (*i)->action_frame << ' ' << (*i)->type << " target = " << (*i)->target_frame << endl;
	}
	cerr << "Next event: ";

	if ((Events::const_iterator) next_event == events.end()) {
		cerr << "none" << endl;
	} else {
		cerr << "at " << (*next_event)->action_frame << ' '
		     << (*next_event)->type << " target = "
		     << (*next_event)->target_frame << endl;
	}
	cerr << "Immediate events pending:\n";
	for (Events::const_iterator i = immediate_events.begin(); i != immediate_events.end(); ++i) {
		cerr << "\tat " << (*i)->action_frame << ' ' << (*i)->type << " target = " << (*i)->target_frame << endl;
	}
	cerr << "END EVENT_DUMP" << endl;
}

void
SessionEventManager::merge_event (SessionEvent* ev)
{
	switch (ev->action) {
	case SessionEvent::Remove:
		_remove_event (ev);
		delete ev;
		return;

	case SessionEvent::Replace:
		_replace_event (ev);
		return;

	case SessionEvent::Clear:
		_clear_event_type (ev->type);
		delete ev;
		return;

	case SessionEvent::Add:
		break;
	}

	/* try to handle immediate events right here */

	if (ev->action_frame == 0) {
		process_event (ev);
		return;
	}

	switch (ev->type) {
	case SessionEvent::AutoLoop:
	case SessionEvent::AutoLoopDeclick:
	case SessionEvent::StopOnce:
		_clear_event_type (ev->type);
		break;

	default:
		for (Events::iterator i = events.begin(); i != events.end(); ++i) {
			if ((*i)->type == ev->type && (*i)->action_frame == ev->action_frame) {
			  error << string_compose(_("Session: cannot have two events of type %1 at the same frame (%2)."),
						  enum_2_string (ev->type), ev->action_frame) << endmsg;
				return;
			}
		}
	}

	events.insert (events.begin(), ev);
	events.sort (SessionEvent::compare);
	next_event = events.begin();
	set_next_event ();
}

/** @return true when @a ev is deleted. */
bool
SessionEventManager::_replace_event (SessionEvent* ev)
{
	bool ret = false;
	Events::iterator i;

	/* private, used only for events that can only exist once in the queue */

	for (i = events.begin(); i != events.end(); ++i) {
		if ((*i)->type == ev->type) {
			(*i)->action_frame = ev->action_frame;
			(*i)->target_frame = ev->target_frame;
			if ((*i) == ev) {
				ret = true;
			}
			delete ev;
			break;
		}
	}

	if (i == events.end()) {
		events.insert (events.begin(), ev);
	}

	events.sort (SessionEvent::compare);
	next_event = events.end();
	set_next_event ();

	return ret;
}

/** @return true when @a ev is deleted. */
bool
SessionEventManager::_remove_event (SessionEvent* ev)
{
	bool ret = false;
	Events::iterator i;

	for (i = events.begin(); i != events.end(); ++i) {
		if ((*i)->type == ev->type && (*i)->action_frame == ev->action_frame) {
			if ((*i) == ev) {
				ret = true;
			}

			delete *i;
			if (i == next_event) {
				++next_event;
			}
			events.erase (i);
			break;
		}
	}

	if (i != events.end()) {
		set_next_event ();
	}

	return ret;
}

void
SessionEventManager::_clear_event_type (SessionEvent::Type type)
{
	Events::iterator i, tmp;

	for (i = events.begin(); i != events.end(); ) {

		tmp = i;
		++tmp;

		if ((*i)->type == type) {
			delete *i;
			if (i == next_event) {
				++next_event;
			}
			events.erase (i);
		}

		i = tmp;
	}

	for (i = immediate_events.begin(); i != immediate_events.end(); ) {

		tmp = i;
		++tmp;

		if ((*i)->type == type) {
			delete *i;
			immediate_events.erase (i);
		}

		i = tmp;
	}

	set_next_event ();
}

