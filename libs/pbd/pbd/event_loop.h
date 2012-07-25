/*
    Copyright (C) 2009 Paul Davis 

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

#ifndef __pbd_event_loop_h__
#define __pbd_event_loop_h__

#include <boost/function.hpp>
#include <boost/bind.hpp> /* we don't need this here, but anything calling call_slot() probably will, so this is convenient */
#include <glibmm/threads.h>

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

class EventLoop 
{
  public:
	EventLoop() {}
	virtual ~EventLoop() {}

	enum RequestType {
		range_guarantee = ~0
	};

        struct BaseRequestObject;
    
        struct InvalidationRecord {
	    std::list<BaseRequestObject*> requests;
	    PBD::EventLoop* event_loop;
	    const char* file;
	    int line;

	    InvalidationRecord() : event_loop (0) {}
        };

        static void* invalidate_request (void* data);

	struct BaseRequestObject {
	    RequestType             type;
            bool                    valid;
            InvalidationRecord*     invalidation;
	    boost::function<void()> the_slot;
            
            BaseRequestObject() : valid (true), invalidation (0) {}
	};

	virtual void call_slot (InvalidationRecord*, const boost::function<void()>&) = 0;
        virtual Glib::Threads::Mutex& slot_invalidation_mutex() = 0;

	static EventLoop* get_event_loop_for_thread();
	static void set_event_loop_for_thread (EventLoop* ui);

  private:
        static Glib::Threads::Private<EventLoop> thread_event_loop;

};

}

#define MISSING_INVALIDATOR 0 // used to mark places where we fail to provide an invalidator

#endif /* __pbd_event_loop_h__ */
