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

#include "pbd/event_loop.h"
#include "pbd/stacktrace.h"

using namespace PBD;
using namespace std;

static void do_not_delete_the_loop_pointer (void*) { }

Glib::Threads::Private<EventLoop> EventLoop::thread_event_loop (do_not_delete_the_loop_pointer); 

EventLoop* 
EventLoop::get_event_loop_for_thread() {
	return thread_event_loop.get ();
}

void 
EventLoop::set_event_loop_for_thread (EventLoop* loop) 
{
	thread_event_loop.set (loop);
}

void* 
EventLoop::invalidate_request (void* data)
{
        InvalidationRecord* ir = (InvalidationRecord*) data;

	/* Some of the requests queued with an EventLoop may involve functors
	 * that make method calls to objects whose lifetime is shorter
	 * than the EventLoop's. We do not want to make those calls if the
	 * object involve has been destroyed. To prevent this, we 
	 * provide a way to invalidate those requests when the object is
	 * destroyed.
	 *
	 * An object was passed to __invalidator() which added a callback to
	 * EventLoop::invalidate_request() to its "notify when destroyed"
	 * list. __invalidator() returned an InvalidationRecord that has been
	 * to passed to this function as data.
	 *
	 * The object is currently being destroyed and so we want to
	 * mark all requests involving this object that are queued with
	 * any EventLoop as invalid. 
	 *
	 * As of April 2012, we are usign sigc::trackable as the base object
	 * used to queue calls to ::invalidate_request() to be made upon
	 * destruction, via its ::add_destroy_notify_callback() API. This is
	 * not necessarily ideal, but it is very close to precisely what we
	 * want, and many of the objects we want to do this with already
	 * inherit (indirectly) from sigc::trackable.
	 */
	
        if (ir->event_loop) {
		Glib::Threads::Mutex::Lock lm (ir->event_loop->slot_invalidation_mutex());
		for (list<BaseRequestObject*>::iterator i = ir->requests.begin(); i != ir->requests.end(); ++i) {
			(*i)->valid = false;
			(*i)->invalidation = 0;
		}
		delete ir;
        } 

        return 0;
}

