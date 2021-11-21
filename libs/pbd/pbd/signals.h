/*
 * Copyright (C) 2009-2016 Paul Davis <paul@linuxaudiosystems.com>
 * Copyright (C) 2010-2012 Carl Hetherington <carl@carlh.net>
 * Copyright (C) 2013 John Emmas <john@creativepost.co.uk>
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

#ifndef __pbd_signals_h__
#define __pbd_signals_h__

#include <csignal>

#include <list>
#include <map>

#ifdef nil
#undef nil
#endif

#include <atomic>

#include <glibmm/threads.h>

#include <boost/noncopyable.hpp>
#include <boost/bind.hpp>
#include <boost/bind/protect.hpp>
#include <boost/function.hpp>
#include <boost/enable_shared_from_this.hpp>
#include <boost/optional.hpp>

#include "pbd/libpbd_visibility.h"
#include "pbd/event_loop.h"

#ifndef NDEBUG
#define DEBUG_PBD_SIGNAL_CONNECTIONS
#endif

#ifdef DEBUG_PBD_SIGNAL_CONNECTIONS
#include "pbd/stacktrace.h"
#include <iostream>
#endif

namespace PBD {

class LIBPBD_API Connection;

class LIBPBD_API SignalBase
{
public:
	SignalBase ()
	: _in_dtor (false)
#ifdef DEBUG_PBD_SIGNAL_CONNECTIONS
	, _debug_connection (false)
#endif
	{}
	virtual ~SignalBase () { }
	virtual void disconnect (boost::shared_ptr<Connection>) = 0;
#ifdef DEBUG_PBD_SIGNAL_CONNECTIONS
	void set_debug_connection (bool yn) { _debug_connection = yn; }
#endif

protected:
	mutable Glib::Threads::Mutex _mutex;
	std::atomic<bool>            _in_dtor;
#ifdef DEBUG_PBD_SIGNAL_CONNECTIONS
	bool _debug_connection;
#endif
};

class LIBPBD_API Connection : public boost::enable_shared_from_this<Connection>
{
public:
	Connection (SignalBase* b, PBD::EventLoop::InvalidationRecord* ir)
		: _signal (b)
		, _invalidation_record (ir)
	{
		if (_invalidation_record) {
			_invalidation_record->ref ();
		}
	}

	void disconnect ()
	{
		Glib::Threads::Mutex::Lock lm (_mutex);
		SignalBase* signal = _signal.exchange (0, std::memory_order_acq_rel);
		if (signal) {
			/* It is safe to assume that signal has not been destructed.
			 * If ~Signal d'tor runs, it will call our signal_going_away()
			 * which will block until we're done here.
			 *
			 * This will lock Signal::_mutex, and call disconnected ()
			 * or return immediately if Signal is being destructed.
			 */
			signal->disconnect (shared_from_this ());
		}
	}

	void disconnected ()
	{
		if (_invalidation_record) {
			_invalidation_record->unref ();
		}
	}

	void signal_going_away ()
	{
		/* called with Signal::_mutex held */
		if (!_signal.exchange (0, std::memory_order_acq_rel)) {
			/* disconnect () grabbed the signal, but signal->disconnect()
			 * has not [yet] removed the entry from the list.
			 *
			 * Allow disconnect () to complete, which will
			 * be an effective NO-OP since SignalBase::_in_dtor is true,
			 * then we can proceed.
			 */
			Glib::Threads::Mutex::Lock lm (_mutex);
		}
		if (_invalidation_record) {
			_invalidation_record->unref ();
		}
	}

private:
	Glib::Threads::Mutex     _mutex;
	std::atomic<SignalBase*> _signal;
	PBD::EventLoop::InvalidationRecord* _invalidation_record;
};

template<typename R>
class /*LIBPBD_API*/ OptionalLastValue
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

class LIBPBD_API ScopedConnection
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
		if (_c == o) {
			return *this;
		}

		disconnect ();
		_c = o;
		return *this;
	}

private:
	UnscopedConnection _c;
};

class LIBPBD_API ScopedConnectionList  : public boost::noncopyable
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

	Glib::Threads::Mutex _scoped_connection_lock;

	typedef std::list<ScopedConnection*> ConnectionList;
	ConnectionList _scoped_connection_list;
};

#include "pbd/signals_generated.h"

} /* namespace */

#endif /* __pbd_signals_h__ */
