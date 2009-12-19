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

namespace PBD {

typedef boost::signals2::connection UnscopedConnection;
typedef boost::signals2::connection Connection;
typedef boost::signals2::scoped_connection ScopedConnection;

class ScopedConnectionList  : public boost::noncopyable
{
  public:
	ScopedConnectionList();
	~ScopedConnectionList ();
	
	void add_connection (const UnscopedConnection& c);
	void drop_connections ();

	template<typename S> void scoped_connect (S& sig, const typename S::slot_function_type& sf) {
		add_connection (sig.connect (sf));
	}

  private:
	/* this class is not copyable */
	ScopedConnectionList(const ScopedConnectionList&) {}

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

    void connect (ScopedConnectionList& clist, 
		  const typename SignalType::slot_function_type& slot) {
	    clist.add_connection (_signal.connect (slot));
    }
    
    void connect (Connection& c, 
		  const typename SignalType::slot_function_type& slot) {
	    c = _signal.connect (slot);
    }
    
    typename SignalType::result_type operator()() {
	    return _signal ();
    }
    
private:
    SignalType _signal;
};

template<typename R, typename A>
class Signal1 {
public:
    Signal1 () {}
    typedef boost::signals2::signal<R(A)> SignalType;

    void connect (ScopedConnectionList& clist, 
		  const typename SignalType::slot_function_type& slot) {
	    clist.add_connection (_signal.connect (slot));
    }

    void connect (Connection& c, 
		  const typename SignalType::slot_function_type& slot) {
	    c = _signal.connect (slot);
    }
    
    typename SignalType::result_type operator()(A arg1) {
	    return _signal (arg1);
    }
    
private:
    SignalType _signal;
};

template<typename R, typename A1, typename A2>
class Signal2 {
public:
    Signal2 () {}
    typedef boost::signals2::signal<R(A1, A2)> SignalType;

    void connect (ScopedConnectionList& clist, 
		  const typename SignalType::slot_function_type& slot) {
	    clist.add_connection (_signal.connect (slot));
    }
    
    void connect (Connection& c, 
		  const typename SignalType::slot_function_type& slot) {
	    c = _signal.connect (slot);
    }
    
    typename SignalType::result_type operator()(A1 arg1, A2 arg2) {
	    return _signal (arg1, arg2);
    }
    
private:
    SignalType _signal;
};

template<typename R, typename A1, typename A2, typename A3>
class Signal3 {
public:
    Signal3 () {}
    typedef boost::signals2::signal<R(A1,A2,A3)> SignalType;

    void connect (ScopedConnectionList& clist, 
		  const typename SignalType::slot_function_type& slot) {
	    clist.add_connection (_signal.connect (slot));
    }
    
    void connect (Connection& c, 
		  const typename SignalType::slot_function_type& slot) {
	    c = _signal.connect (slot);
    }
    
    typename SignalType::result_type operator()(A1 arg1, A2 arg2, A3 arg3) {
	    return _signal (arg1, arg2, arg3);
    }
    
private:
    SignalType _signal;
};

} /* namespace */

#endif /* __pbd_signals_h__ */
