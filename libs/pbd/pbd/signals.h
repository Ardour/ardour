/*
    Copyright (C) 2009-2012 Paul Davis 

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

#include <boost/noncopyable.hpp>
#include <boost/bind.hpp>
#include <boost/bind/protect.hpp>
#include <boost/function.hpp>
#include <boost/thread/mutex.hpp>
#include <boost/enable_shared_from_this.hpp>
#include <boost/optional.hpp>

#include "pbd/event_loop.h"

namespace PBD {

class Connection;

class SignalBase
{
public:
	virtual ~SignalBase () {}
	virtual void disconnect (boost::shared_ptr<Connection>) = 0;

protected:
	boost::mutex _mutex;
};

class Connection : public boost::enable_shared_from_this<Connection>
{
public:
	Connection (SignalBase* b) : _signal (b) {}

	void disconnect ()
	{
		boost::mutex::scoped_lock lm (_mutex);
		if (_signal) {
			_signal->disconnect (shared_from_this ());
		} 
	}

	void signal_going_away ()
	{
		boost::mutex::scoped_lock lm (_mutex);
		_signal = 0;
	}

private:
	boost::mutex _mutex;
	SignalBase* _signal;
};

template<typename R>
class OptionalLastValue
{
public:
	typedef boost::optional<R> result_type;

	template <typename Iter>
	result_type operator() (Iter first, Iter last) const {
		result_type r;
		while (first != last) {
			r = *first;
			++first;
		}

		return r;
	}
};
	
typedef boost::shared_ptr<Connection> UnscopedConnection;
	
class ScopedConnection
{
public:
	ScopedConnection () {}
	ScopedConnection (UnscopedConnection c) : _c (c) {}
	~ScopedConnection () {
		disconnect ();
	}

	void disconnect ()
	{
		if (_c) {
			_c->disconnect ();
		}
	}

	ScopedConnection& operator= (UnscopedConnection const & o)
	{
		_c = o;
		return *this;
	}

private:
	UnscopedConnection _c;
};
	
class ScopedConnectionList  : public boost::noncopyable
{
  public:
	ScopedConnectionList();
	virtual ~ScopedConnectionList ();
	
	void add_connection (const UnscopedConnection& c);
	void drop_connections ();

  private:
	/* this class is not copyable */
	ScopedConnectionList(const ScopedConnectionList&);

	/* Even though our signals code is thread-safe, this additional list of
	   scoped connections needs to be protected in 2 cases:

	   (1) (unlikely) we make a connection involving a callback on the
	       same object from 2 threads. (wouldn't that just be appalling 
	       programming style?)
	     
	   (2) where we are dropping connections in one thread and adding
	       one from another.
	 */

	Glib::Mutex _lock;

	typedef std::list<ScopedConnection*> ConnectionList;
	ConnectionList _list;
};

#include "pbd/signals_generated.h"	
	
} /* namespace */

#endif /* __pbd_signals_h__ */
