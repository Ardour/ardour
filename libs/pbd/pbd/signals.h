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
#include "pbd/stack_allocator.h"

#ifndef NDEBUG
#define DEBUG_PBD_SIGNAL_CONNECTIONS
#define DEBUG_PBD_SIGNAL_EMISSION
#endif

#ifdef DEBUG_PBD_SIGNAL_CONNECTIONS
#include "pbd/stacktrace.h"
#include <iostream>
#endif


using namespace std::placeholders;

namespace PBD {

#ifdef DEBUG_PBD_SIGNAL_CONNECTIONS
static std::size_t max_signal_subscribers;
#endif

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
#ifdef DEBUG_PBD_SIGNAL_EMISSION
	, _debug_emission (false)
#endif
	{}
	virtual ~SignalBase () { }
	virtual void disconnect (std::shared_ptr<Connection>) = 0;
#ifdef DEBUG_PBD_SIGNAL_CONNECTIONS
	void set_debug_connection (bool yn) { _debug_connection = yn; }
#endif
#ifdef DEBUG_PBD_SIGNAL_EMISSION
	void set_debug_emission (bool yn) { _debug_emission = yn; }
#endif

protected:
	mutable Glib::Threads::Mutex _mutex;
	std::atomic<bool>            _in_dtor;
#ifdef DEBUG_PBD_SIGNAL_CONNECTIONS
	bool _debug_connection;
#endif
#ifdef DEBUG_PBD_SIGNAL_EMISSION
	bool _debug_emission;
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

	std::list<ScopedConnectionList*>::size_type size() const { Glib::Threads::Mutex::Lock lm (_scoped_connection_lock); return _scoped_connection_list.size(); }

  private:
	/* Even though our signals code is thread-safe, this additional list of
	   scoped connections needs to be protected in 2 cases:

	   (1) (unlikely) we make a connection involving a callback on the
	       same object from 2 threads. (wouldn't that just be appalling
	       programming style?)

	   (2) where we are dropping connections in one thread and adding
	       one from another.
	 */

	mutable Glib::Threads::Mutex _scoped_connection_lock;

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
#ifdef DEBUG_PBD_SIGNAL_EMISSION
	if (_debug_emission) {
		std::cerr << "------ Signal @ " << this << " emission process begins with " << _slots.size() << std::endl;
		PBD::stacktrace (std::cerr, 19);
	}
#endif

#ifdef _MSC_VER   /* Regarding the note (below) it was initially
			       * thought that the problem got fixed in VS2015
			       * but in fact it still persists even in VS2022 */
		/* Use the older (heap based) mapping when building with MSVC.
		 * Our StackAllocator class depends on 'boost::aligned_storage'
		 * which is known to be troublesome with Visual C++ :-
		 * https://www.boost.org/doc/libs/1_65_0/libs/type_traits/doc/html/boost_typetraits/reference/aligned_storage.html
		 */
	std::vector<Connection*> s;
#else
	const std::size_t nslots = 512;
	std::vector<Connection*,PBD::StackAllocator<Connection*,nslots> > s;
#endif

	/* First, make a copy of the current connection state for us to iterate
	 * over later (the connection state may be changed by a signal handler.
	 */

	{
		Glib::Threads::Mutex::Lock lm (_mutex);
		/* copy only the raw pointer, no need for a shared_ptr in this
		 * context, we only use the address as a lookup into the _slots
		 * container. Note: because of the use of a stack allocator,
		 * this is *unlikely* to cause any (heap) memory
		 * allocation. That will only happen if the number of
		 * connections to this signal exceeds the value of nslots
		 * defined above. As of April 2025, the maximum number of
		 * connections appears to be ntracks+1.
		 */
		for (auto const & [connection,functor] : _slots) {
			s.push_back (connection.get());
		}
	}

	if constexpr (std::is_void_v<R>) {
		slot_function_type functor;

		for (auto const & c : s) {

			/* We may have just called a slot, and this may have
			 * resulted in disconnection of other slots from us.
			 * The list copy means that this won't cause any
			 * problems with invalidated iterators, but we must
			 * check to see if the slot we are about to call is
			 * still on the list.
			 */
			bool still_there = false;

			{
				Glib::Threads::Mutex::Lock lm (_mutex);
				typename Slots::const_iterator f = std::find_if (_slots.begin(), _slots.end(), [&](typename Slots::value_type const & elem) { return elem.first.get() == c; });
				if (f != _slots.end()) {
					functor = f->second;
					still_there = true;
				}
			}

			if (still_there) {
#ifdef DEBUG_PBD_SIGNAL_EMISSION
				if (_debug_emission) {
					std::cerr << "signal @ " << this << " calling slot for connection @ " << c << " of " << _slots.size() << std::endl;
				}
#endif
				functor (a...);
			} else {
#ifdef DEBUG_PBD_SIGNAL_EMISSION
				if (_debug_emission) {
					std::cerr << "signal @ " << this << " connection  " << c << " of " << _slots.size() << " was no longer in the slot list\n";
				}
#endif
			}
		}

#ifdef DEBUG_PBD_SIGNAL_EMISSION
		if (_debug_emission) {
			std::cerr << "------ Signal @ " << this << " emission process ends\n";
		}
#endif
		return;

	} else {
		if (s.empty()) {
			return typename Combiner::result_type ();
		}

		/* We would like to use a stack allocator here, but for reasons
		 * not really understood, this breaks on macOS when using
		 * the custom combiner used by libs/ardour IO's
		 * PortCountChanging signal.
		 *
		 * Using a vector here is not RT-safe but a manual code
		 * inspection reveals that there are no combiner-based signals
		 * (i.e. Signals with a return value) that are ever used in RT
		 * code.
		 *
		 * The alternative is to use alloca() but that could
		 * theoretically cause stack overflows if the number of
		 * handlers for the signal is too large (it would have to be
		 * very large, however).
		 *
		 * In short, std::vector<T> is the least-bad of two bad
		 * choices, and we've chosen this because of the lack of RT use
		 * cases for a Signal with a return value.
		 *
		 */

		std::vector<R> r;
		r.reserve (s.size());
		slot_function_type functor;

		for (auto const & c : s) {

			/* We may have just called a slot, and this may have resulted in
			 * disconnection of other slots from us.  The list copy means that
			 * this won't cause any problems with invalidated iterators, but we
			 * must check to see if the slot we are about to call is still on the list.
			 */
			bool still_there = false;

			{
				Glib::Threads::Mutex::Lock lm (_mutex);
				typename Slots::const_iterator f = std::find_if (_slots.begin(), _slots.end(), [&](typename Slots::value_type const & elem) { return elem.first.get() == c; });

				if (f != _slots.end()) {
					functor = f->second;
					still_there = true;
				}
			}
			if (still_there) {
#ifdef DEBUG_PBD_SIGNAL_EMISSION
				if (_debug_emission) {
					std::cerr << "signal @ " << this << " calling non-void slot for connection @ " << c << " of " << _slots.size() << std::endl;
				}
#endif
				r.push_back (functor (a...));
			}

#ifdef DEBUG_PBD_SIGNAL_EMISSION
			if (_debug_emission) {
				std::cerr << "------ Signal @ " << this << " emission process ends\n";
			}
#endif
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
	if (_slots.size() > max_signal_subscribers) {
		max_signal_subscribers = _slots.size();
	}
	if (_debug_connection) {
		std::cerr << "+++++++ CONNECT " << this << " via connection @ " << c << " size now " << _slots.size() << std::endl;
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
