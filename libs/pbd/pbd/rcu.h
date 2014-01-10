/*
    Copyright (C) 2000-2007 Paul Davis

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

#ifndef __pbd_rcu_h__
#define __pbd_rcu_h__

#include "boost/shared_ptr.hpp"
#include "glibmm/threads.h"

#include <list>

#include "pbd/libpbd_visibility.h"

/** @file Defines a set of classes to implement Read-Copy-Update.  We do not attempt to define RCU here - use google.

   The design consists of two parts: an RCUManager and an RCUWriter.
*/

/** An RCUManager is an object which takes over management of a pointer to another object.
   It provides three key methods:

           - reader() : obtains a shared pointer to the managed object that may be used for reading, without synchronization
	       - write_copy() : obtains a shared pointer to the object that may be used for writing/modification
	       - update() : accepts a shared pointer to a (presumed) modified instance of the object and causes all
	                    future reader() and write_copy() calls to use that instance.

   Any existing users of the value returned by reader() can continue to use their copy even as a write_copy()/update() takes place.
   The RCU manager will manage the various instances of "the managed object" in a way that is transparent to users of the manager
   and managed object.
*/
template<class T>
class /*LIBPBD_API*/ RCUManager
{
  public:

	RCUManager (T* new_rcu_value) {
		x.m_rcu_value = new boost::shared_ptr<T> (new_rcu_value);
	}

	virtual ~RCUManager() { delete x.m_rcu_value; }

	boost::shared_ptr<T> reader () const { return *((boost::shared_ptr<T> *) g_atomic_pointer_get (&x.gptr)); }

	/* this is an abstract base class - how these are implemented depends on the assumptions
	   that one can make about the users of the RCUManager. See SerializedRCUManager below
	   for one implementation.
	*/

	virtual boost::shared_ptr<T> write_copy () = 0;
	virtual bool update (boost::shared_ptr<T> new_value) = 0;

  protected:
	/* ordinarily this would simply be a declaration of a ptr to a shared_ptr<T>. however, the atomic
	   operations that we are using (from glib) have sufficiently strict typing that it proved hard
	   to get them to accept even a cast value of the ptr-to-shared-ptr() as the argument to get()
	   and comp_and_exchange(). Consequently, we play a litle trick here that relies on the fact
	   that sizeof(A*) == sizeof(B*) no matter what the types of A and B are. for most purposes
	   we will use x.m_rcu_value, but when we need to use an atomic op, we use x.gptr. Both expressions
	   evaluate to the same address.
	*/

	union {
	    boost::shared_ptr<T>* m_rcu_value;
	    mutable volatile gpointer gptr;
	} x;
};


/** Serialized RCUManager implements the RCUManager interface. It is based on the
   following key assumption: among its users we have readers that are bound by
   RT time constraints, and writers who are not. Therefore, we do not care how
   slow the write_copy()/update() operations are, or what synchronization
   primitives they use.

   Because of this design assumption, this class will serialize all
   writers. That is, objects calling write_copy()/update() will be serialized by
   a mutex. Only a single writer may be in the middle of write_copy()/update();
   all other writers will block until the first has finished. The order of
   execution of multiple writers if more than one is blocked in this way is
   undefined.

   The class maintains a lock-protected "dead wood" list of old value of
   *m_rcu_value (i.e. shared_ptr<T>). The list is cleaned up every time we call
   write_copy(). If the list is the last instance of a shared_ptr<T> that
   references the object (determined by shared_ptr::unique()) then we
   erase it from the list, thus deleting the object it points to.  This is lazy
   destruction - the SerializedRCUManager assumes that there will sufficient
   calls to write_copy() to ensure that we do not inadvertently leave objects
   around for excessive periods of time.

   For extremely well defined circumstances (i.e. it is known that there are no
   other writer objects in existence), SerializedRCUManager also provides a
   flush() method that will unconditionally clear out the "dead wood" list. It
   must be used with significant caution, although the use of shared_ptr<T>
   means that no actual objects will be deleted incorrectly if this is misused.
*/
template<class T>
class /*LIBPBD_API*/ SerializedRCUManager : public RCUManager<T>
{
public:

	SerializedRCUManager(T* new_rcu_value)
		: RCUManager<T>(new_rcu_value)
	{
	}

	boost::shared_ptr<T> write_copy ()
	{
		m_lock.lock();

		// clean out any dead wood

		typename std::list<boost::shared_ptr<T> >::iterator i;

		for (i = m_dead_wood.begin(); i != m_dead_wood.end(); ) {
			if ((*i).unique()) {
				i = m_dead_wood.erase (i);
			} else {
				++i;
			}
		}

		/* store the current so that we can do compare and exchange
		   when someone calls update(). Notice that we hold
		   a lock, so this store of m_rcu_value is atomic.
		*/

		current_write_old = RCUManager<T>::x.m_rcu_value;

		boost::shared_ptr<T> new_copy (new T(**current_write_old));

		return new_copy;

		/* notice that the write lock is still held: update() MUST
		   be called or we will cause another writer to stall.
		*/
	}

	bool update (boost::shared_ptr<T> new_value)
	{
		/* we still hold the write lock - other writers are locked out */

		boost::shared_ptr<T>* new_spp = new boost::shared_ptr<T> (new_value);

		/* update, by atomic compare&swap. Only succeeds if the old
		   value has not been changed.

		   XXX but how could it? we hold the freakin' lock!
		*/

		bool ret = g_atomic_pointer_compare_and_exchange (&RCUManager<T>::x.gptr,
								  (gpointer) current_write_old,
								  (gpointer) new_spp);

		if (ret) {

			// successful update : put the old value into dead_wood,

			m_dead_wood.push_back (*current_write_old);

			// now delete it - this gets rid of the shared_ptr<T> but
			// because dead_wood contains another shared_ptr<T> that
			// references the same T, the underlying object lives on

			delete current_write_old;
		}

		/* unlock, allowing other writers to proceed */

		m_lock.unlock();

		return ret;
	}

	void flush () {
		Glib::Threads::Mutex::Lock lm (m_lock);
		m_dead_wood.clear ();
	}

private:
	Glib::Threads::Mutex                      m_lock;
	boost::shared_ptr<T>*            current_write_old;
	std::list<boost::shared_ptr<T> > m_dead_wood;
};

/** RCUWriter is a convenience object that implements write_copy/update via
   lifetime management. Creating the object obtains a writable copy, which can
   be obtained via the get_copy() method; deleting the object will update
   the manager's copy. Code doing a write/update thus looks like:

   {

        RCUWriter writer (object_manager);
        boost::shared_ptr<T> copy = writer.get_copy();
        ... modify copy ...

   } <= writer goes out of scope, update invoked

*/
template<class T>
class /*LIBPBD_API*/ RCUWriter
{
public:

	RCUWriter(RCUManager<T>& manager)
		: m_manager(manager) {
		m_copy = m_manager.write_copy();
	}

	~RCUWriter() {
		if (m_copy.unique()) {
			/* As intended, our copy is the only reference
			   to the object pointed to by m_copy. Update
			   the manager with the (presumed) modified
			   version.
			*/
			m_manager.update(m_copy);
		} else {
			/* This means that some other object is using our copy
			   of the object. This can only happen if the scope in
			   which this RCUWriter exists passed it to a function
			   that created a persistent reference to it, since the
			   copy was private to this particular RCUWriter. Doing
			   so will not actually break anything but it violates
			   the design intention here and so we do not bother to
			   update the manager's copy.

			   XXX should we print a warning about this?
			*/
		}

	}

	boost::shared_ptr<T> get_copy() const { return m_copy; }

private:
	RCUManager<T>& m_manager;
	boost::shared_ptr<T> m_copy;
};

#endif /* __pbd_rcu_h__ */
