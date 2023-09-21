/*
 * Copyright (C) 2009-2016 Paul Davis <paul@linuxaudiosystems.com>
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

#ifndef __pbd_event_loop_h__
#define __pbd_event_loop_h__

#include <atomic>
#include <string>
#include <vector>
#include <map>
#include <boost/function.hpp>
#include <boost/bind.hpp> /* we don't need this here, but anything calling call_slot() probably will, so this is convenient */
#include <stdint.h>
#include <pthread.h>
#include <glibmm/threads.h>

#include "pbd/libpbd_visibility.h"

namespace PBD
{

/** An EventLoop is as basic abstraction designed to be used with any "user
 * interface" (not necessarily graphical) that needs to wait on
 * events/requests and dispatch/process them as they arrive.
 *
 * This is a very basic class that doesn't by itself provide an actual
 * event loop or thread. See BaseUI for the "real" object to be used
 * when something like this is needed (it inherits from EventLoop).
 */

class LIBPBD_API EventLoop
{
public:
	EventLoop (std::string const&);
	virtual ~EventLoop();

	enum RequestType {
		range_guarantee = ~0
	};

	struct BaseRequestObject;

	struct InvalidationRecord {
		std::list<BaseRequestObject*> requests;
		PBD::EventLoop* event_loop;
		std::atomic<int> _valid;
		std::atomic<int> _ref;
		const char* file;
		int line;

		InvalidationRecord() : event_loop (0), _valid (1), _ref (0) {}
		void invalidate () { _valid.store (0); }
		bool valid () { return _valid.load () == 1; }

		void ref ()    { _ref.fetch_add (1); }
		void unref ()  { (void) _ref.fetch_sub (1); }
		bool in_use () { return _ref.load () > 0; }
		int  use_count () { return _ref.load (); }
	};

	static void* invalidate_request (void* data);

	struct BaseRequestObject {
		RequestType             type;
		InvalidationRecord*     invalidation;
		boost::function<void()> the_slot;

		BaseRequestObject() : invalidation (0) {}
		~BaseRequestObject() {
			if (invalidation) {
				invalidation->unref ();
			}
		}
	};

	virtual bool call_slot (InvalidationRecord*, const boost::function<void()>&) = 0;
	virtual Glib::Threads::RWLock& slot_invalidation_rwlock() = 0;

	std::string event_loop_name() const { return _name; }

	static EventLoop* get_event_loop_for_thread();
	static void set_event_loop_for_thread (EventLoop* ui);

	struct ThreadBufferMapping {
		pthread_t emitting_thread;
		size_t num_requests;
	};

	static std::vector<ThreadBufferMapping> get_request_buffers_for_target_thread (const std::string&);

	static void pre_register (const std::string& emitting_thread_name, uint32_t num_requests);
	static void remove_request_buffer_from_map (pthread_t);

	std::list<InvalidationRecord*> trash;

	static InvalidationRecord* __invalidator (sigc::trackable& trackable, const char*, int);

private:
	static Glib::Threads::Private<EventLoop> thread_event_loop;
	std::string _name;

	typedef std::vector<ThreadBufferMapping> ThreadRequestBufferList;
	static ThreadRequestBufferList thread_buffer_requests;
	static Glib::Threads::Mutex   thread_buffer_requests_lock;

	struct RequestBufferSupplier {

		/* @param name : name of object/entity that will/may accept
		 * requests from other threads, via a request buffer.
		 */
		std::string name;

		/* @param factory : a function that can be called (with an
		 * argument specifying the @param number_of_requests) to create and
		 * return a request buffer for communicating with @p name)
		 */
		void* (*factory)(uint32_t nunber_of_requests);
	};
	typedef std::vector<RequestBufferSupplier> RequestBufferSuppliers;
	static RequestBufferSuppliers request_buffer_suppliers;
};

}

#define MISSING_INVALIDATOR nullptr // used to mark places where we fail to provide an invalidator

#endif /* __pbd_event_loop_h__ */
