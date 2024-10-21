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

#pragma once

#include <csignal>

#include <list>
#include <map>

#ifdef nil
#undef nil
#endif

#include <atomic>

#include <glibmm/threads.h>

#include <boost/bind/protect.hpp>
#include <boost/enable_shared_from_this.hpp>
#include <optional>

#include "pbd/libpbd_visibility.h"
#include "pbd/event_loop.h"

#ifndef NDEBUG
#define DEBUG_PBD_SIGNAL_CONNECTIONS
#endif

#ifdef DEBUG_PBD_SIGNAL_CONNECTIONS
#include "pbd/stacktrace.h"
#include <iostream>
#endif

using namespace std::placeholders;

namespace PBD {

class LIBPBD_API Connection;

class LIBPBD_API ScopedConnection;

class LIBPBD_API ScopedConnectionList;

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
	virtual void disconnect (std::shared_ptr<Connection>) = 0;
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

template<typename R>
class /*LIBPBD_API*/ OptionalLastValue
{
public:
	typedef std::optional<R> result_type;

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

template <typename Combiner, typename _Signature>
class SignalWithCombiner;

template <typename Combiner, typename R, typename... A>
class SignalWithCombiner<Combiner, R(A...)> : public SignalBase
{
public:

	typedef std::function<R(A...)> slot_function_type;

private:

	/** The slots that this signal will call on emission */
	typedef std::map<std::shared_ptr<Connection>, slot_function_type> Slots;
	Slots _slots;

public:

	static void compositor (typename std::function<void(A...)> f,
	                        EventLoop* event_loop,
	                        EventLoop::InvalidationRecord* ir, A... a);

	~SignalWithCombiner ();

	void connect_same_thread (ScopedConnection& c, const slot_function_type& slot);
	void connect_same_thread (ScopedConnectionList& clist, const slot_function_type& slot);
	void connect (ScopedConnectionList& clist,
	              PBD::EventLoop::InvalidationRecord* ir,
	              const slot_function_type& slot,
	              PBD::EventLoop* event_loop);
	void connect (ScopedConnection& c,
	              PBD::EventLoop::InvalidationRecord* ir,
	              const slot_function_type& slot,
	              PBD::EventLoop* event_loop);

	/** If R is any kind of void type,
	 *  then operator() return type must be R,
	 *  else it must be Combiner::result_type.
	 */
	typename std::conditional_t<std::is_void_v<R>, R, typename Combiner::result_type>
	operator() (A... a);

	bool empty () const {
		Glib::Threads::Mutex::Lock lm (_mutex);
		return _slots.empty ();
	}

	size_t size () const {
		Glib::Threads::Mutex::Lock lm (_mutex);
		return _slots.size ();
	}

private:

	friend class Connection;

	std::shared_ptr<Connection> _connect (PBD::EventLoop::InvalidationRecord* ir, slot_function_type f);
	void disconnect (std::shared_ptr<Connection> c);

};

template <typename R>
using DefaultCombiner = OptionalLastValue<R>;

template <typename _Signature>
class Signal;

template <typename R, typename... A>
class Signal<R(A...)> : public SignalWithCombiner<DefaultCombiner<R>, R(A...)> {};

class LIBPBD_API Connection : public std::enable_shared_from_this<Connection>
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

typedef std::shared_ptr<Connection> UnscopedConnection;

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

	UnscopedConnection const & the_connection() const { return _c; }

private:
	UnscopedConnection _c;
};

class LIBPBD_API ScopedConnectionList
{
  public:
	ScopedConnectionList ();
	ScopedConnectionList (const ScopedConnectionList&) = delete;
	ScopedConnectionList& operator= (const ScopedConnectionList&) = delete;
	virtual ~ScopedConnectionList ();

	void add_connection (const UnscopedConnection& c);
	void drop_connections ();

  private:
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

template <typename Combiner, typename R, typename... A>
void
SignalWithCombiner<Combiner, R(A...)>::compositor (typename std::function<void(A...)> f,
                                                   EventLoop* event_loop,
                                                   EventLoop::InvalidationRecord* ir, A... a)
{
	event_loop->call_slot (ir, std::bind (f, a...));
}

template <typename Combiner, typename R, typename... A>
SignalWithCombiner<Combiner, R(A...)>::~SignalWithCombiner ()
{
	_in_dtor.store (true, std::memory_order_release);
	Glib::Threads::Mutex::Lock lm (_mutex);
	/* Tell our connection objects that we are going away, so they don't try to call us */
	for (typename Slots::const_iterator i = _slots.begin(); i != _slots.end(); ++i) {
		i->first->signal_going_away ();
	}
}

/** Arrange for @a slot to be executed whenever this signal is emitted.
 * Store the connection that represents this arrangement in @a c.
 *
 * NOTE: @a slot will be executed in the same thread that the signal is
 * emitted in.
 */

template <typename Combiner, typename R, typename... A>
void
SignalWithCombiner<Combiner, R(A...)>::connect_same_thread (ScopedConnection& c,
                                                            const slot_function_type& slot)
{
	c = _connect (0, slot);
}

/** Arrange for @a slot to be executed whenever this signal is emitted.
 * Add the connection that represents this arrangement to @a clist.
 *
 * NOTE: @a slot will be executed in the same thread that the signal is
 * emitted in.
 */

template <typename Combiner, typename R, typename... A>
void
SignalWithCombiner<Combiner, R(A...)>::connect_same_thread (ScopedConnectionList& clist,
                                                            const slot_function_type& slot)
{
	clist.add_connection (_connect (0, slot));
}

/** Arrange for @a slot to be executed in the context of @a event_loop
 * whenever this signal is emitted. Add the connection that represents
 * this arrangement to @a clist.
 *
 * If the event loop/thread in which @a slot will be executed will
 * outlive the lifetime of any object referenced in @a slot,
 * then an InvalidationRecord should be passed, allowing
 * any request sent to the @a event_loop and not executed
 * before the object is destroyed to be marked invalid.
 *
 * "outliving the lifetime" doesn't have a specific, detailed meaning,
 * but is best illustrated by two contrasting examples:
 *
 * 1) the main GUI event loop/thread - this will outlive more or
 * less all objects in the application, and thus when arranging for
 * @a slot to be called in that context, an invalidation record is
 * highly advisable.
 *
 * 2) a secondary event loop/thread which will be destroyed along
 * with the objects that are typically referenced by @a slot.
 * Assuming that the event loop is stopped before the objects are
 * destroyed, there is no reason to pass in an invalidation record,
 * and MISSING_INVALIDATOR may be used.
 */

template <typename Combiner, typename R, typename... A>
void
SignalWithCombiner<Combiner, R(A...)>::connect (ScopedConnectionList& clist,
                                                PBD::EventLoop::InvalidationRecord* ir,
                                                const slot_function_type& slot,
                                                PBD::EventLoop* event_loop)
{
	if (ir) {
		ir->event_loop = event_loop;
	}

	clist.add_connection (_connect (ir, [slot, event_loop, ir](A... a) {
		return compositor(slot, event_loop, ir, a...);
	}));
}

/** See notes for the ScopedConnectionList variant of this function. This
 *  differs in that it stores the connection to the signal in a single
 *  ScopedConnection rather than a ScopedConnectionList.
 */

template <typename Combiner, typename R, typename... A>
void
SignalWithCombiner<Combiner, R(A...)>::connect (ScopedConnection& c,
                                                PBD::EventLoop::InvalidationRecord* ir,
                                                const slot_function_type& slot,
                                                PBD::EventLoop* event_loop)
{
	if (ir) {
		ir->event_loop = event_loop;
	}

	c = _connect (ir, [slot, event_loop, ir](A... a) {
		return compositor(slot, event_loop, ir, a...);
	});
}

/** Emit this signal. This will cause all slots connected to it be executed
 * in the order that they were connected (cross-thread issues may alter
 * the precise execution time of cross-thread slots).
 */

template <typename Combiner, typename R, typename... A>
typename std::conditional_t<std::is_void_v<R>, R, typename Combiner::result_type>
SignalWithCombiner<Combiner, R(A...)>::operator() (A... a)
{
	/* First, take a copy of our list of slots as it is now */

	Slots s;
	{
		Glib::Threads::Mutex::Lock lm (_mutex);
		s = _slots;
	}

	if constexpr (std::is_void_v<R>) {
		for (typename Slots::const_iterator i = s.begin(); i != s.end(); ++i) {

			/* We may have just called a slot, and this may have resulted in
			* disconnection of other slots from us.  The list copy means that
			* this won't cause any problems with invalidated iterators, but we
			* must check to see if the slot we are about to call is still on the list.
			*/
			bool still_there = false;
			{
				Glib::Threads::Mutex::Lock lm (_mutex);
				still_there = _slots.find (i->first) != _slots.end ();
			}

			if (still_there) {
				(i->second)(a...);
			}
		}
	} else {
		std::list<R> r;
		for (typename Slots::const_iterator i = s.begin(); i != s.end(); ++i) {

			/* We may have just called a slot, and this may have resulted in
			* disconnection of other slots from us.  The list copy means that
			* this won't cause any problems with invalidated iterators, but we
			* must check to see if the slot we are about to call is still on the list.
			*/
			bool still_there = false;
			{
				Glib::Threads::Mutex::Lock lm (_mutex);
				still_there = _slots.find (i->first) != _slots.end ();
			}

			if (still_there) {
				r.push_back ((i->second)(a...));
			}
		}

		/* Call our combiner to do whatever is required to the result values */
		Combiner c;
		return c (r.begin(), r.end());
	}
}

template <typename Combiner, typename R, typename... A>
std::shared_ptr<Connection>
SignalWithCombiner<Combiner, R(A...)>::_connect (PBD::EventLoop::InvalidationRecord* ir,
                                                 slot_function_type f)
{
	std::shared_ptr<Connection> c (new Connection (this, ir));
	Glib::Threads::Mutex::Lock lm (_mutex);
	_slots[c] = f;
	#ifdef DEBUG_PBD_SIGNAL_CONNECTIONS
	if (_debug_connection) {
		std::cerr << "+++++++ CONNECT " << this << " size now " << _slots.size() << std::endl;
		stacktrace (std::cerr, 10);
	}
	#endif
	return c;
}

template <typename Combiner, typename R, typename... A>
void
SignalWithCombiner<Combiner, R(A...)>::disconnect (std::shared_ptr<Connection> c)
{
	/* ~ScopedConnection can call this concurrently with our d'tor */
	Glib::Threads::Mutex::Lock lm (_mutex, Glib::Threads::TRY_LOCK);
	while (!lm.locked()) {
		if (_in_dtor.load (std::memory_order_acquire)) {
			/* d'tor signal_going_away() took care of everything already */
			return;
		}
		/* Spin */
		lm.try_acquire ();
	}
	_slots.erase (c);
	lm.release ();

	c->disconnected ();
	#ifdef DEBUG_PBD_SIGNAL_CONNECTIONS
	if (_debug_connection) {
		std::cerr << "------- DISCCONNECT " << this << " size now " << _slots.size() << std::endl;
		stacktrace (std::cerr, 10);
	}
	#endif
}

} /* namespace */

