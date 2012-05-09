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

#ifndef __pbd_signals_h__
#define __pbd_signals_h__

#include <list>
#include <glibmm/thread.h>

#include <boost/signals2.hpp>
#include <boost/noncopyable.hpp>
#include <boost/bind.hpp>
#include <boost/bind/protect.hpp>

#include "pbd/event_loop.h"

namespace PBD {

typedef boost::signals2::connection UnscopedConnection;
typedef boost::signals2::scoped_connection ScopedConnection;

class ScopedConnectionList  : public boost::noncopyable
{
  public:
	ScopedConnectionList();
	~ScopedConnectionList ();
	
	void add_connection (const UnscopedConnection& c);
	void drop_connections ();

  private:
	/* this class is not copyable */
	ScopedConnectionList(const ScopedConnectionList&);

	/* this lock is shared by all instances of a ScopedConnectionList.
	   We do not want one mutex per list, and since we only need the lock
	   when adding or dropping connections, which are generally occuring
	   in object creation and UI operations, the contention on this 
	   lock is low and not of significant consequence. Even though
	   boost::signals2 is thread-safe, this additional list of
	   scoped connections needs to be protected in 2 cases:

	   (1) (unlikely) we make a connection involving a callback on the
	       same object from 2 threads. (wouldn't that just be appalling 
	       programming style?)
	     
	   (2) where we are dropping connections in one thread and adding
	       one from another.
	 */

	static Glib::StaticMutex _lock;

	typedef std::list<ScopedConnection*> ConnectionList;
	ConnectionList _list;
};

template<typename R>
class Signal0 {
public:
    Signal0 () {}
    typedef boost::signals2::signal<R()> SignalType;

    /** Arrange for @a slot to be executed in the context of @a event_loop
	whenever this signal is emitted. Store the connection that represents
	this arrangement to @a c.

	NOTE: @a slot will be executed in the same thread that the signal is
	emitted in.
    */

    void connect_same_thread (ScopedConnection& c, 
		  const typename SignalType::slot_function_type& slot) {
	    c = _signal.connect (slot);
    }

    /** Arrange for @a slot to be executed in the context of @a event_loop
	whenever this signal is emitted. Add the connection that represents
	this arrangement to @a clist.

	NOTE: @a slot will be executed in the same thread that the signal is
	emitted in.
    */
	
    void connect_same_thread (ScopedConnectionList& clist, 
		  const typename SignalType::slot_function_type& slot) {
	    clist.add_connection (_signal.connect (slot));
    }

    /** Arrange for @a slot to be executed in the context of @a event_loop
	whenever this signal is emitted. Add the connection that represents
	this arrangement to @a clist.
	
	If the event loop/thread in which @a slot will be executed will
	outlive the lifetime of any object referenced in @a slot,
	then an InvalidationRecord should be passed, allowing
	any request sent to the @a event_loop and not executed
	before the object is destroyed to be marked invalid.
	
	"outliving the lifetime" doesn't have a specific, detailed meaning,
	but is best illustrated by two contrasting examples:
	
	1) the main GUI event loop/thread - this will outlive more or 
	less all objects in the application, and thus when arranging for
	@a slot to be called in that context, an invalidation record is 
	highly advisable.
	
	2) a secondary event loop/thread which will be destroyed along
	with the objects that are typically referenced by @a slot.
	Assuming that the event loop is stopped before the objects are
	destroyed, there is no reason to pass in an invalidation record,
	and MISSING_INVALIDATOR may be used.
    */

    void connect (ScopedConnectionList& clist, 
                  PBD::EventLoop::InvalidationRecord* ir, 
		  const typename SignalType::slot_function_type& slot,
		  PBD::EventLoop* event_loop) {
	    if (ir) {
		    ir->event_loop = event_loop;
	    }
	    clist.add_connection (_signal.connect (boost::bind (&EventLoop::call_slot, event_loop, ir, slot)));
    }

    /** See notes for the ScopedConnectionList variant of this function. This
     * differs in that it stores the connection to the signal in a single
     * ScopedConnection rather than a ScopedConnectionList.
     */

    void connect (ScopedConnection& c, 
                  PBD::EventLoop::InvalidationRecord* ir, 
		  const typename SignalType::slot_function_type& slot,
		  PBD::EventLoop* event_loop) {
	    if (ir) {
		    ir->event_loop = event_loop;
	    }
	    c = _signal.connect (boost::bind (&EventLoop::call_slot, event_loop, ir, slot));
    }

    /** Emit this signal. This will cause all slots connected to it be executed
	in the order that they were connected (cross-thread issues may alter
	the precise execution time of cross-thread slots).
    */
    
    typename SignalType::result_type operator()() {
	    return _signal ();
    }

    /** Return true if there is nothing connected to this signal, false
     *  otherwise.
     */

    bool empty() const { return _signal.empty(); }
    
private:
    SignalType _signal;
};

template<typename R, typename A, typename C = boost::signals2::optional_last_value<R> >
class Signal1 {
public:
    Signal1 () {}
    typedef boost::signals2::signal<R(A), C> SignalType;

    void connect_same_thread (ScopedConnectionList& clist, 
		  const typename SignalType::slot_function_type& slot) {
	    clist.add_connection (_signal.connect (slot));
    }

    void connect_same_thread (ScopedConnection& c, 
		     const typename SignalType::slot_function_type& slot) {
	    c = _signal.connect (slot);
    }

    static void compositor (typename boost::function<void(A)> f, EventLoop* event_loop, EventLoop::InvalidationRecord* ir, A arg) {
	    event_loop->call_slot (ir, boost::bind (f, arg));
    }

    void connect (ScopedConnectionList& clist, 
                  PBD::EventLoop::InvalidationRecord* ir, 
		  const typename SignalType::slot_function_type& slot,
		  PBD::EventLoop* event_loop) {
	    if (ir) {
		    ir->event_loop = event_loop;
	    }
	    clist.add_connection (_signal.connect (boost::bind (&compositor, slot, event_loop, ir, _1)));
    }

    void connect (ScopedConnection& c, 
                  PBD::EventLoop::InvalidationRecord* ir, 
		  const typename SignalType::slot_function_type& slot,
		  PBD::EventLoop* event_loop) {
	    if (ir) {
		    ir->event_loop = event_loop;
	    }
	    c = _signal.connect (boost::bind (&compositor, slot, event_loop, ir, _1));

    }
    
    typename SignalType::result_type operator()(A arg1) {
	    return _signal (arg1);
    }
    
    bool empty() const { return _signal.empty(); }

private:
    SignalType _signal;
};

template<typename R, typename A1, typename A2>
class Signal2 {
public:
    Signal2 () {}
    typedef boost::signals2::signal<R(A1, A2)> SignalType;

    void connect_same_thread (ScopedConnectionList& clist, 
		  const typename SignalType::slot_function_type& slot) {
	    clist.add_connection (_signal.connect (slot));
    }

    void connect_same_thread (ScopedConnection& c, 
		     const typename SignalType::slot_function_type& slot) {
	    c = _signal.connect (slot);
    }

    static void compositor (typename boost::function<void(A1,A2)> f, PBD::EventLoop* event_loop, 
                            EventLoop::InvalidationRecord* ir,
                            A1 arg1, A2 arg2) {
	    event_loop->call_slot (ir, boost::bind (f, arg1, arg2));
    }

    void connect (ScopedConnectionList& clist, 
                  PBD::EventLoop::InvalidationRecord* ir, 
		  const typename SignalType::slot_function_type& slot,
		  PBD::EventLoop* event_loop) {
	    if (ir) {
		    ir->event_loop = event_loop;
	    }
	    clist.add_connection (_signal.connect (boost::bind (&compositor, slot, event_loop, ir, _1, _2)));
    }

    void connect (ScopedConnection& c, 
                  PBD::EventLoop::InvalidationRecord* ir, 
		  const typename SignalType::slot_function_type& slot,
		  PBD::EventLoop* event_loop) {
	    if (ir) {
		    ir->event_loop = event_loop;
	    }
	    c = _signal.connect (boost::bind (&compositor, slot, event_loop, ir, _1, _2));
    }

    typename SignalType::result_type operator()(A1 arg1, A2 arg2) {
	    return _signal (arg1, arg2);
    }
    
    bool empty() const { return _signal.empty(); }

private:
    SignalType _signal;
};

template<typename R, typename A1, typename A2, typename A3>
class Signal3 {
public:
    Signal3 () {}
    typedef boost::signals2::signal<R(A1,A2,A3)> SignalType;

    void connect_same_thread (ScopedConnectionList& clist, 
		  const typename SignalType::slot_function_type& slot) {
	    clist.add_connection (_signal.connect (slot));
    }

    void connect_same_thread (ScopedConnection& c, 
			      const typename SignalType::slot_function_type& slot) {
	    c = _signal.connect (slot);
    }

    static void compositor (typename boost::function<void(A1,A2,A3)> f, PBD::EventLoop* event_loop, 
                            EventLoop::InvalidationRecord* ir, 
                            A1 arg1, A2 arg2, A3 arg3) {
	    event_loop->call_slot (ir, boost::bind (f, arg1, arg2, arg3));
    }

    void connect (ScopedConnectionList& clist, 
                  PBD::EventLoop::InvalidationRecord* ir, 
		  const typename SignalType::slot_function_type& slot,
		  PBD::EventLoop* event_loop) {
	    if (ir) {
		    ir->event_loop = event_loop;
	    }
	    clist.add_connection (_signal.connect (boost::bind (&compositor, slot, event_loop, ir, _1, _2, _3)));
    }
    
    void connect (ScopedConnection& c, 
                  PBD::EventLoop::InvalidationRecord* ir, 
		  const typename SignalType::slot_function_type& slot,
		  PBD::EventLoop* event_loop) {
	    if (ir) {
		    ir->event_loop = event_loop;
	    }
	    c = _signal.connect (_signal.connect (boost::bind (&compositor, slot, event_loop, ir, _1, _2, _3)));
    }
    
    typename SignalType::result_type operator()(A1 arg1, A2 arg2, A3 arg3) {
	    return _signal (arg1, arg2, arg3);
    }
    
    bool empty() const { return _signal.empty(); }

private:
    SignalType _signal;
};

template<typename R, typename A1, typename A2, typename A3, typename A4>
class Signal4 {
public:
    Signal4 () {}
    typedef boost::signals2::signal<R(A1,A2,A3,A4)> SignalType;

    void connect_same_thread (ScopedConnectionList& clist, 
		  const typename SignalType::slot_function_type& slot) {
	    clist.add_connection (_signal.connect (slot));
    }

    void connect_same_thread (ScopedConnection& c, 
			      const typename SignalType::slot_function_type& slot) {
	    c = _signal.connect (slot);
    }

    static void compositor (typename boost::function<void(A1,A2,A3)> f, PBD::EventLoop* event_loop, 
                            EventLoop::InvalidationRecord* ir, 
                            A1 arg1, A2 arg2, A3 arg3, A4 arg4) {
	    event_loop->call_slot (ir, boost::bind (f, arg1, arg2, arg3, arg4));
    }

    void connect (ScopedConnectionList& clist, 
                  PBD::EventLoop::InvalidationRecord* ir, 
		  const typename SignalType::slot_function_type& slot,
		  PBD::EventLoop* event_loop) {
	    if (ir) {
		    ir->event_loop = event_loop;
	    }
	    clist.add_connection (_signal.connect (boost::bind (&compositor, slot, event_loop, ir, _1, _2, _3, _4)));
    }
    
    void connect (ScopedConnection& c, 
                  PBD::EventLoop::InvalidationRecord* ir, 
		  const typename SignalType::slot_function_type& slot,
		  PBD::EventLoop* event_loop) {
	    if (ir) {
		    ir->event_loop = event_loop;
	    }
	    c = _signal.connect (_signal.connect (boost::bind (&compositor, slot, event_loop, ir, _1, _2, _3, _4)));
    }
    
    typename SignalType::result_type operator()(A1 arg1, A2 arg2, A3 arg3, A4 arg4) {
	    return _signal (arg1, arg2, arg3, arg4);
    }
    
    bool empty() const { return _signal.empty(); }

private:
    SignalType _signal;
};

template<typename R, typename A1, typename A2, typename A3, typename A4, typename A5>
class Signal5 {
public:
    Signal5 () {}
    typedef boost::signals2::signal<R(A1,A2,A3,A4,A5)> SignalType;

    void connect_same_thread (ScopedConnectionList& clist, 
		  const typename SignalType::slot_function_type& slot) {
	    clist.add_connection (_signal.connect (slot));
    }

    void connect_same_thread (ScopedConnection& c, 
			      const typename SignalType::slot_function_type& slot) {
	    c = _signal.connect (slot);
    }

    static void compositor (typename boost::function<void(A1,A2,A3,A4,A5)> f, PBD::EventLoop* event_loop, 
                            EventLoop::InvalidationRecord* ir, 
                            A1 arg1, A2 arg2, A3 arg3, A4 arg4, A5 arg5) {
	    event_loop->call_slot (ir, boost::bind (f, arg1, arg2, arg3, arg4, arg5));
    }

    void connect (ScopedConnectionList& clist, 
                  PBD::EventLoop::InvalidationRecord* ir, 
		  const typename SignalType::slot_function_type& slot,
		  PBD::EventLoop* event_loop) {
	    if (ir) {
		    ir->event_loop = event_loop;
	    }
	    clist.add_connection (_signal.connect (boost::bind (&compositor, slot, event_loop, ir, _1, _2, _3, _4, _5)));
    }
    
    void connect (ScopedConnection& c, 
                  PBD::EventLoop::InvalidationRecord* ir, 
		  const typename SignalType::slot_function_type& slot,
		  PBD::EventLoop* event_loop) {
	    if (ir) {
		    ir->event_loop = event_loop;
	    }
	    c = _signal.connect (_signal.connect (boost::bind (&compositor, slot, event_loop, ir, _1, _2, _3, _4, _5)));
    }
    
    typename SignalType::result_type operator()(A1 arg1, A2 arg2, A3 arg3, A4 arg4, A5 arg5) {
	    return _signal (arg1, arg2, arg3, arg4, arg5);
    }
    
    bool empty() const { return _signal.empty(); }

private:
    SignalType _signal;
};
	
} /* namespace */

#endif /* __pbd_signals_h__ */
