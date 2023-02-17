/*
 * Copyright (C) 2000-2013 Paul Davis <paul@linuxaudiosystems.com>
 * Copyright (C) 2009 David Robillard <d@drobilla.net>
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

#ifndef __pbd_rcu_h__
#define __pbd_rcu_h__

#include <atomic>
#include <mutex>
#include <memory>

#include "boost/smart_ptr/detail/yield_k.hpp"

#include <list>

#include "pbd/libpbd_visibility.h"

/** @file rcu.h
 * Define a set of classes to implement Read-Copy-Update.  We do not attempt to define RCU here - use google.
 *
 * The design consists of two parts: an RCUManager and an RCUWriter.
*/

/** An RCUManager is an object which takes over management of a pointer to another object.
 *
 * It provides three key methods:
 *
 * - reader()     : obtains a shared pointer to the managed object that may be used for reading, without synchronization
 * - write_copy() : obtains a shared pointer to the object that may be used for writing/modification
 * - update()     : accepts a shared pointer to a (presumed) modified instance of the object and causes all
 *                  future reader() and write_copy() calls to use that instance.
 *
 * Any existing users of the value returned by reader() can continue to use their copy even as a write_copy()/update() takes place.
 * The RCU manager will manage the various instances of "the managed object" in a way that is transparent to users of the manager
 * and managed object.
*/
template <class T>
class /*LIBPBD_API*/ RCUManager
{
public:
	RCUManager (T* object_to_be_managed)
	{
		_active_reads = 0;
		managed_object = new std::shared_ptr<T> (object_to_be_managed);
	}

	virtual ~RCUManager ()
	{
		/* This just deletes the shared ptr, but of course this may
		   also be the last reference to the managed object.
		*/
		delete managed_object;
	}

	std::shared_ptr<T> reader () const
	{
		std::shared_ptr<T> rv;

		/* Keep count of any readers in this section of code, so writers can
		 * wait until managed_object is no longer in use after an atomic exchange
		 * before dropping it.
		 *
		 * rg: this is not great, 3 consecutive full compiler and hardware
		 * memory barterers. For an edge-case lock that is not usually contended.
		 * consider reverting f87de76b9fc8b3a5a.
		 */
		_active_reads++;
		rv = *managed_object;
		_active_reads--;

		return rv;
	}

	/* this is an abstract base class - how these are implemented depends on the assumptions
	 * that one can make about the users of the RCUManager. See SerializedRCUManager below
	 * for one implementation.
	 */

	virtual std::shared_ptr<T> write_copy ()                         = 0;
	virtual bool               update (std::shared_ptr<T> new_value) = 0;

protected:
	typedef std::shared_ptr<T>* PtrToSharedPtr;
	std::atomic<PtrToSharedPtr> managed_object;

	inline bool active_read () const {
		return _active_reads.load() != 0;
	}

private:
	mutable std::atomic<int> _active_reads;
};

/** Serialized RCUManager implements the RCUManager interface. It is based on the
 * following key assumption: among its users we have readers that are bound by
 * RT time constraints, and writers who are not. Therefore, we do not care how
 * slow the write_copy()/update() operations are, or what synchronization
 * primitives they use.
 *
 * Because of this design assumption, this class will serialize all
 * writers. That is, objects calling write_copy()/update() will be serialized by
 * a mutex. Only a single writer may be in the middle of write_copy()/update();
 * all other writers will block until the first has finished. The order of
 * execution of multiple writers if more than one is blocked in this way is
 * undefined.
 *
 * The class maintains a lock-protected "dead wood" list of old value of
 * *managed_object (i.e. shared_ptr<T>). The list is cleaned up every time we call
 * write_copy(). If the list is the last instance of a shared_ptr<T> that
 * references the object (determined by shared_ptr::unique()) then we
 * erase it from the list, thus deleting the object it points to.  This is lazy
 * destruction - the SerializedRCUManager assumes that there will sufficient
 * calls to write_copy() to ensure that we do not inadvertently leave objects
 * around for excessive periods of time.
 *
 * For extremely well defined circumstances (i.e. it is known that there are no
 * other writer objects in existence), SerializedRCUManager also provides a
 * flush() method that will unconditionally clear out the "dead wood" list. It
 * must be used with significant caution, although the use of shared_ptr<T>
 * means that no actual objects will be deleted incorrectly if this is misused.
 */
template <class T>
class /*LIBPBD_API*/ SerializedRCUManager : public RCUManager<T>
{
public:
	SerializedRCUManager(T* new_managed_object)
		: RCUManager<T>(new_managed_object)
		, _current_write_old (0)
	{
	}

	void init (std::shared_ptr<T> object_to_be_managed) {
		assert  (*RCUManager<T>::managed_object == std::shared_ptr<T> ());
		RCUManager<T>::managed_object = new std::shared_ptr<T> (object_to_be_managed);
	}

	std::shared_ptr<T> write_copy ()
	{
		_lock.lock ();

		// clean out any dead wood

		typename std::list<std::shared_ptr<T> >::iterator i;

		for (i = _dead_wood.begin (); i != _dead_wood.end ();) {
			if ((*i).unique ()) {
				i = _dead_wood.erase (i);
			} else {
				++i;
			}
		}

		/* store the current so that we can do compare and exchange
		 * when someone calls update(). Notice that we hold
		 * a lock, so this store of managed_object is atomic.
		 */

		_current_write_old = RCUManager<T>::managed_object;

		/* now do the (potentially arbitrarily expensive data copy of
		 * the RCU-managed object
		 */

		std::shared_ptr<T> new_copy (new T (**_current_write_old));

		return new_copy;

		/* notice that the write lock is still held: update() or abort() MUST
		 * be called or we will cause another writer to stall.
		 */
	}

	void abort () {
		_lock.unlock();
	}

	bool update (std::shared_ptr<T> new_value)
	{
		/* we still hold the write lock - other writers are locked out */

		typename RCUManager<T>::PtrToSharedPtr new_spp = new std::shared_ptr<T> (new_value);

		/* update, by atomic compare&swap. Only succeeds if the old
		 * value has not been changed.
		 *
		 * XXX but how could it? we hold the freakin' lock!
		 */

		bool ret = RCUManager<T>::managed_object.compare_exchange_strong (_current_write_old, new_spp);

		if (ret) {
			/* successful update
			 *
			 * wait until there are no active readers. This ensures that any
			 * references to the old value have been fully copied into a new
			 * shared_ptr, and thus have had their reference count incremented.
			 */

			for (unsigned i = 0; RCUManager<T>::active_read (); ++i) {
				/* spin being nice to the scheduler/CPU */
				boost::detail::yield (i);
			}

			/* if we are not the only user, put the old value into dead_wood.
			 * if we are the only user, then it is safe to drop it here.
			 */

			if (!_current_write_old->unique ()) {
				_dead_wood.push_back (*_current_write_old); 
			}

			/* now delete it - if we are the only user, this deletes the
			 * underlying object. If other users existed, then there will
			 * be an extra reference in _dead_wood, ensuring that the
			 * underlying object lives on even when the other users
			 * are done with it
			 */

			delete _current_write_old;
		}

		/* unlock, allowing other writers to proceed */

		_lock.unlock ();

		return ret;
	}

	void no_update () {
		/* just releases the lock, in the event that no changes are
		   made to a write copy.
		*/
		_lock.unlock ();
	}

	void flush ()
	{
		std::lock_guard<std::mutex> lm (_lock);
		_dead_wood.clear ();
	}

private:
	std::mutex                             _lock;
	typename RCUManager<T>::PtrToSharedPtr _current_write_old;
	std::list<std::shared_ptr<T> >         _dead_wood;
};

/** RCUWriter is a convenience object that implements write_copy/update via
 * lifetime management. Creating the object obtains a writable copy, which can
 * be obtained via the get_copy() method; deleting the object will update
 * the manager's copy. Code doing a write/update thus looks like:
 *
 * @code
 * {
 *      RCUWriter writer (object_manager);
 *      std::shared_ptr<T> copy = writer.get_copy();
 *      ... modify copy ...
 *
 * } <= writer goes out of scope, update invoked
 * @endcode
 *
 */
template <class T>
class /*LIBPBD_API*/ RCUWriter
{
public:
	RCUWriter (RCUManager<T>& manager)
	    : _manager (manager)
	    , _copy (_manager.write_copy ())
	{
	}

	~RCUWriter ()
	{
		if (_copy.unique ()) {
			/* As intended, our copy is the only reference
			   to the object pointed to by _copy. Update
			   the manager with the (presumed) modified
			   version.
			*/
			_manager.update (_copy);
		} else {
			/* This means that some other object is using our copy
			 * of the object. This can only happen if the scope in
			 * which this RCUWriter exists passed it to a function
			 * that created a persistent reference to it, since the
			 * copy was private to this particular RCUWriter. Doing
			 * so will not actually break anything but it violates
			 * the design intention here and so we do not bother to
			 * update the manager's copy.
			 *
			 * XXX should we print a warning about this?
			 */
		}
	}

	std::shared_ptr<T> get_copy () const
	{
		return _copy;
	}

private:
	RCUManager<T>&     _manager;
	std::shared_ptr<T> _copy;
};

#endif /* __pbd_rcu_h__ */
