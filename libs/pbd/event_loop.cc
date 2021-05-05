/*
 * Copyright (C) 2009-2018 Paul Davis <paul@linuxaudiosystems.com>
 * Copyright (C) 2010 Carl Hetherington <carl@carlh.net>
 * Copyright (C) 2015-2017 Robin Gareus <robin@gareus.org>
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

#include <cstring>

#include <pthread.h>

#include "pbd/compose.h"
#include "pbd/debug.h"
#include "pbd/event_loop.h"
#include "pbd/error.h"
#include "pbd/pthread_utils.h"

#include "pbd/i18n.h"

using namespace PBD;
using namespace std;

static void do_not_delete_the_loop_pointer (void*) { }

Glib::Threads::Private<EventLoop> EventLoop::thread_event_loop (do_not_delete_the_loop_pointer);

Glib::Threads::RWLock EventLoop::thread_buffer_requests_lock;
EventLoop::ThreadRequestBufferList EventLoop::thread_buffer_requests;
EventLoop::RequestBufferSuppliers EventLoop::request_buffer_suppliers;

EventLoop::EventLoop (string const& name)
	: _name (name)
{
}

EventLoop::~EventLoop ()
{
	trash.sort();
	trash.unique();
	for (std::list<InvalidationRecord*>::iterator r = trash.begin(); r != trash.end(); ++r) {
		if (!(*r)->in_use ()) {
			delete *r;
		}
	}
	trash.clear ();
}

EventLoop*
EventLoop::get_event_loop_for_thread()
{
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
		DEBUG_TRACE (PBD::DEBUG::EventLoop, string_compose ("%1: invalidating request from %2 (%3) @ %4\n", pthread_name(), ir->event_loop, ir->event_loop->event_loop_name(), ir));
		Glib::Threads::Mutex::Lock lm (ir->event_loop->slot_invalidation_mutex());
		ir->invalidate ();
		ir->event_loop->trash.push_back(ir);
	}

	return 0;
}

vector<EventLoop::ThreadBufferMapping>
EventLoop::get_request_buffers_for_target_thread (const std::string& target_thread)
{
	vector<ThreadBufferMapping> ret;
	Glib::Threads::RWLock::WriterLock lm (thread_buffer_requests_lock);

	for (ThreadRequestBufferList::const_iterator x = thread_buffer_requests.begin();
	     x != thread_buffer_requests.end(); ++x) {

		if (x->second.target_thread_name == target_thread) {
			ret.push_back (x->second);
		}
	}

	DEBUG_TRACE (PBD::DEBUG::EventLoop, string_compose ("for thread \"%1\", found %2 request buffers\n", target_thread, ret.size()));

	return ret;
}

void
EventLoop::register_request_buffer_factory (const string& target_thread_name,
                                            void* (*factory)(uint32_t))
{

	RequestBufferSupplier trs;
	trs.name = target_thread_name;
	trs.factory = factory;

	{
		Glib::Threads::RWLock::WriterLock lm (thread_buffer_requests_lock);
		request_buffer_suppliers.push_back (trs);
	}
}

void
EventLoop::pre_register (const string& emitting_thread_name, uint32_t num_requests)
{
	/* Threads that need to emit signals "towards" other threads, but with
	   RT safe behavior may be created before the receiving threads
	   exist. This makes it impossible for them to use the
	   ThreadCreatedWithRequestSize signal to notify receiving threads of
	   their existence.

	   This function creates a request buffer for them to use with
	   the (not yet) created threads, and stores it where the receiving
	   thread can find it later.
	 */

	ThreadBufferMapping mapping;
	Glib::Threads::RWLock::WriterLock lm (thread_buffer_requests_lock);

	for (RequestBufferSuppliers::iterator trs = request_buffer_suppliers.begin(); trs != request_buffer_suppliers.end(); ++trs) {

		if (!trs->factory) {
			/* no factory - no request buffer required or expected */
			continue;
		}

		if (emitting_thread_name == trs->name) {
			/* no need to register an emitter with itself */
			continue;
		}

		mapping.emitting_thread = pthread_self();
		mapping.target_thread_name = trs->name;

		/* Allocate a suitably sized request buffer. This will set the
		 * thread-local variable that holds a pointer to this request
		 * buffer.
		 */
		mapping.request_buffer = trs->factory (num_requests);

		/* now store it where the receiving thread (trs->name) can find
		   it if and when it is created. (Discovery happens in the
		   AbstractUI constructor. Note that if
		*/

		const string key = string_compose ("%1/%2", emitting_thread_name, mapping.target_thread_name);

		/* management of the thread_request_buffers map works as
		 * follows:
		 *
		 * when the factory method was called above, the pointer to the
		 * created buffer is set as a thread-local-storage (TLS) value
		 * for this (the emitting) thread.
		 *
		 * The TLS value is set up with a destructor that marks the
		 * request buffer as "dead" when the emitting thread exits.
		 *
		 * An entry will remain in the map after the thread exits.
		 *
		 * The receiving thread may (if it receives requests from other
		 * threads) notice the dead buffer. If it does, it will delete
		 * the request buffer, and call
		 * ::remove_request_buffer_from_map() to get rid of it from the map.
		 *
		 * This does mean that the lifetime of the request buffer is
		 * indeterminate: if the receiving thread were to receive no
		 * further requests, the request buffer will live on
		 * forever. But this is OK, because if there are no requests
		 * arriving, the receiving thread is not attempting to use the
		 * request buffer(s) in any way.
		 *
		 * Note, however, that *if* an emitting thread is recreated
		 * with the same name (e.g. when a control surface is
		 * enabled/disabled/enabled), then the request buffer for the
		 * new thread will replace the map entry for the key, because
		 * of the matching thread names. This does mean that
		 * potentially the request buffer can leak in this case, but
		 * (a) these buffers are not really that large anyway (b) the
		 * scenario is not particularly common (c) the buffers would
		 * typically last across a session instance if not program
		 * lifetime anyway.
		 */

		thread_buffer_requests[key] = mapping;
		DEBUG_TRACE (PBD::DEBUG::EventLoop, string_compose ("pre-registered request buffer for \"%1\" to send to \"%2\", buffer @ %3 (key was %4)\n",
		                                                    emitting_thread_name, trs->name, mapping.request_buffer, key));
	}
}

void
EventLoop::remove_request_buffer_from_map (void* ptr)
{
	Glib::Threads::RWLock::WriterLock lm (thread_buffer_requests_lock);

	for (ThreadRequestBufferList::iterator x = thread_buffer_requests.begin(); x != thread_buffer_requests.end(); ++x) {
		if (x->second.request_buffer == ptr) {
			thread_buffer_requests.erase (x);
			break;
		}
	}
}
