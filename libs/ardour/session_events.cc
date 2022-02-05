/*
 * Copyright (C) 1999-2017 Paul Davis <paul@linuxaudiosystems.com>
 * Copyright (C) 2006-2012 David Robillard <d@drobilla.net>
 * Copyright (C) 2009-2012 Carl Hetherington <carl@carlh.net>
 * Copyright (C) 2015-2018 Robin Gareus <robin@gareus.org>
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

#include <cmath>
#include <unistd.h>

#include "pbd/error.h"
#include "pbd/enumwriter.h"
#include "pbd/pthread_utils.h"

#include "ardour/debug.h"
#include "ardour/session_event.h"

#include "pbd/i18n.h"

using namespace std;
using namespace ARDOUR;
using namespace PBD;

PerThreadPool* SessionEvent::pool;

void
SessionEvent::init_event_pool ()
{
	pool = new PerThreadPool;
}

bool
SessionEvent::has_per_thread_pool ()
{
	return pool->has_per_thread_pool ();
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

SessionEvent::SessionEvent (Type t, Action a, samplepos_t when, samplepos_t where, double spd, bool yn, bool yn2, bool yn3)
	: type (t)
	, action (a)
	, action_sample (when)
	, target_sample (where)
	, speed (spd)
	, yes_or_no (yn)
	, second_yes_or_no (yn2)
	, third_yes_or_no (yn3)
	, event_loop (0)
{
	DEBUG_TRACE (DEBUG::SessionEvents, string_compose ("NEW SESSION EVENT, type = %1 action = %2\n", enum_2_string (type), enum_2_string (action)));
}

void *
SessionEvent::operator new (size_t)
{
	CrossThreadPool* p = pool->per_thread_pool ();
	SessionEvent* ev = static_cast<SessionEvent*> (p->alloc ());
	DEBUG_TRACE (DEBUG::SessionEvents, string_compose ("%1 Allocating SessionEvent from %2 ev @ %3 pool size %4 free %5 used %6\n", pthread_name(), p->name(), ev,
	                                                   p->total(), p->available(), p->used()));

	ev->own_pool = p;
	return ev;
}

void
SessionEvent::operator delete (void *ptr, size_t /*size*/)
{
	Pool* p = pool->per_thread_pool (false);
	SessionEvent* ev = static_cast<SessionEvent*> (ptr);

	DEBUG_TRACE (DEBUG::SessionEvents, string_compose (
		             "%1 Deleting SessionEvent @ %2 type %3 action %4 ev thread pool = %5 ev pool = %6 size %7 free %8 used %9\n",
		             pthread_name(), ev, enum_2_string (ev->type), enum_2_string (ev->action), p->name(), ev->own_pool->name(), ev->own_pool->total(), ev->own_pool->available(), ev->own_pool->used()
		             ));

	if (p && p == ev->own_pool) {
		p->release (ptr);
	} else {
		assert(ev->own_pool);
		ev->own_pool->push (ev);
		DEBUG_TRACE (DEBUG::SessionEvents, string_compose ("%1 was wrong thread for this pool, pushed event onto pending list, will be deleted on next alloc from %2 pool size %3 free %4 used %5 pending %6\n",
		                                                   pthread_name(), ev->own_pool->name(),
		                                                   ev->own_pool->total(), ev->own_pool->available(), ev->own_pool->used(),
		                                                   ev->own_pool->pending_size()));
	}
}

void
SessionEventManager::add_event (samplepos_t sample, SessionEvent::Type type, samplepos_t target_sample)
{
	SessionEvent* ev = new SessionEvent (type, SessionEvent::Add, sample, target_sample, 0);
	queue_event (ev);
}

void
SessionEventManager::remove_event (samplepos_t sample, SessionEvent::Type type)
{
	SessionEvent* ev = new SessionEvent (type, SessionEvent::Remove, sample, 0, 0);
	queue_event (ev);
}

void
SessionEventManager::replace_event (SessionEvent::Type type, samplepos_t sample, samplepos_t target)
{
	assert (sample != SessionEvent::Immediate);
	SessionEvent* ev = new SessionEvent (type, SessionEvent::Replace, sample, target, 0);
	queue_event (ev);
}

void
SessionEventManager::clear_events (SessionEvent::Type type)
{
	SessionEvent* ev = new SessionEvent (type, SessionEvent::Clear, SessionEvent::Immediate, 0, 0);
	queue_event (ev);
}

void
SessionEventManager::clear_events (SessionEvent::Type type, boost::function<void (void)> after)
{
	SessionEvent* ev = new SessionEvent (type, SessionEvent::Clear, SessionEvent::Immediate, 0, 0);
	ev->rt_slot = after;

	/* in the calling thread, after the clear is complete, arrange to flush things from the event
	   pool pending list (i.e. to make sure they are really back in the free list and available
	   for future events).
	*/

	ev->event_loop = PBD::EventLoop::get_event_loop_for_thread ();
	if (ev->event_loop) {
		ev->rt_return = boost::bind (&CrossThreadPool::flush_pending_with_ev, ev->event_pool(), _1);
	}

	queue_event (ev);
}

void
SessionEventManager::dump_events () const
{
	cerr << "EVENT DUMP" << endl;
	for (Events::const_iterator i = events.begin(); i != events.end(); ++i) {

		cerr << "\tat " << (*i)->action_sample << " type " << enum_2_string ((*i)->type) << " target = " << (*i)->target_sample << endl;
	}
	cerr << "Next event: ";

	if ((Events::const_iterator) next_event == events.end()) {
		cerr << "none" << endl;
	} else {
		cerr << "at " << (*next_event)->action_sample << ' '
		     << enum_2_string ((*next_event)->type) << " target = "
		     << (*next_event)->target_sample << endl;
	}
	cerr << "Immediate events pending:\n";
	for (Events::const_iterator i = immediate_events.begin(); i != immediate_events.end(); ++i) {
		cerr << "\tat " << (*i)->action_sample << ' ' << enum_2_string((*i)->type) << " target = " << (*i)->target_sample << endl;
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
		/* run any additional realtime callback, if any */
		if (ev->rt_slot) {
			ev->rt_slot ();
		}
		if (ev->event_loop) {
			/* run non-realtime callback (in some other thread) */
			ev->event_loop->call_slot (MISSING_INVALIDATOR, boost::bind (ev->rt_return, ev));
		} else {
			delete ev;
		}
		return;

	case SessionEvent::Add:
		break;
	}

	/* try to handle immediate events right here */

	if (ev->type == SessionEvent::Locate || ev->type == SessionEvent::LocateRoll) {
		/* remove any existing Locates that are waiting to execute */
		_clear_event_type (ev->type);
	}

	if (ev->action_sample == SessionEvent::Immediate) {
		process_event (ev);
		return;
	}

	switch (ev->type) {
	case SessionEvent::AutoLoop:
		_clear_event_type (ev->type);
		break;
	default:
		for (Events::iterator i = events.begin(); i != events.end(); ++i) {
			if ((*i)->type == ev->type && (*i)->action_sample == ev->action_sample) {
			  error << string_compose(_("Session: cannot have two events of type %1 at the same sample (%2)."),
						  enum_2_string (ev->type), ev->action_sample) << endmsg;
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

	/* use only for events that can only exist once in the respective queue */
	Events& e (ev->action_sample == SessionEvent::Immediate ? immediate_events : events);

	for (i = e.begin(); i != e.end(); ++i) {
		if ((*i)->type == ev->type && ev->type == SessionEvent::Overwrite && (*i)->track.lock() == ev->track.lock()) {
			assert (ev->action_sample == SessionEvent::Immediate);
			(*i)->overwrite = ARDOUR::OverwriteReason ((*i)->overwrite | ev->overwrite);
			delete ev;
			return true;
		}
		else if ((*i)->type == ev->type && ev->type != SessionEvent::Overwrite) {
			assert (ev->action_sample != SessionEvent::Immediate);
			assert (ev->type == SessionEvent::PunchIn || ev->type == SessionEvent::PunchOut ||  ev->type == SessionEvent::AutoLoop);
			(*i)->action_sample = ev->action_sample;
			(*i)->target_sample = ev->target_sample;
			if ((*i) == ev) {
				ret = true;
			}
			delete ev;
			break;
		}
	}

	if (i == e.end()) {
		e.insert (e.begin(), ev);
	}

	if (ev->action_sample == SessionEvent::Immediate) {
		/* no need to sort immediate events */
		return ret;
	}

	e.sort (SessionEvent::compare);
	next_event = e.end();
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
		if ((*i)->type == ev->type && (*i)->action_sample == ev->action_sample) {
			assert ((*i)->action_sample != SessionEvent::Immediate);
			if ((*i) == ev) {
				ret = true;
			}

			delete *i;
			if (i == next_event) {
				++next_event;
			}
			i = events.erase (i);
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
