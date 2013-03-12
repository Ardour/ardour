/*
    Copyright (C) 1998-99 Paul Barton-Davis
 
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

#ifndef __qm_pool_h__
#define __qm_pool_h__

#include <vector>
#include <string>

#include <glibmm/threads.h>

#include "pbd/ringbuffer.h"

/** A pool of data items that can be allocated, read from and written to
 *  without system memory allocation or locking.
 */
class Pool 
{
  public:
	Pool (std::string name, unsigned long item_size, unsigned long nitems);
	virtual ~Pool ();

	virtual void *alloc ();
	virtual void release (void *);
	
	std::string name() const { return _name; }

  protected:
	RingBuffer<void*> free_list; ///< a list of pointers to free items within block
	std::string _name;

  private:
	void *block; ///< data storage area
};

class SingleAllocMultiReleasePool : public Pool
{
  public:
	SingleAllocMultiReleasePool (std::string name, unsigned long item_size, unsigned long nitems);
	~SingleAllocMultiReleasePool ();

	virtual void *alloc ();
	virtual void release (void *);

  private:
        Glib::Threads::Mutex m_lock;
};


class MultiAllocSingleReleasePool : public Pool
{
  public:
	MultiAllocSingleReleasePool (std::string name, unsigned long item_size, unsigned long nitems);
	~MultiAllocSingleReleasePool ();

	virtual void *alloc ();
	virtual void release (void *);

  private:
        Glib::Threads::Mutex m_lock;
};

class PerThreadPool;

/** Management of a per-thread pool of data that is allocated by one thread and
 *  freed by one other thread. Not safe for use when there is more than 1
 *  reader and 1 writer. 
 *
 *  This is basically a wrapper around a thread-local storage instance of a 
 *  ringbuffer, made safe for use in the case where multiple threads allocate
 *  from the ringbuffer and a single thread "frees" the allocations.
 * 
 *  Rather than using locks, each thread has its own ringbuffer (and associated
 *  data), and so it calls alloc(), passes a pointer to the result of the alloc
 *  to another thread, which later calls push() to "free" it. 
 */
class CrossThreadPool : public Pool
{
  public:
	CrossThreadPool (std::string n, unsigned long isize, unsigned long nitems, PerThreadPool *);

	void* alloc ();
	void push (void *);

	PerThreadPool* parent () const {
		return _parent;
	}

	bool empty ();
	
  private:
	RingBuffer<void*> pending;
	PerThreadPool* _parent;
};

/** A class to manage per-thread pools of memory.  One object of this class is instantiated,
 *  and then it is used to create per-thread pools for 1 or more threads as required.
 */
class PerThreadPool
{
  public:
	PerThreadPool ();

        const Glib::Threads::Private<CrossThreadPool>& key() const { return _key; }

	void  create_per_thread_pool (std::string name, unsigned long item_size, unsigned long nitems);
	CrossThreadPool* per_thread_pool ();

	void set_trash (RingBuffer<CrossThreadPool*>* t);
	void add_to_trash (CrossThreadPool *);

  private:
        Glib::Threads::Private<CrossThreadPool> _key;
	std::string _name;
	unsigned long _item_size;
	unsigned long _nitems;

	/** mutex to protect either changes to the _trash variable, or writes to the RingBuffer */
        Glib::Threads::Mutex _trash_mutex;
	RingBuffer<CrossThreadPool*>* _trash;
};

#endif // __qm_pool_h__
