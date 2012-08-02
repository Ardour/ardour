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

    $Id$
*/

#include <cstdlib>
#include <vector>
#include <cstdlib>
#include <cassert>

#include "pbd/pool.h"
#include "pbd/error.h"
#include "pbd/debug.h"
#include "pbd/compose.h"

using namespace std;
using namespace PBD;

Pool::Pool (string n, unsigned long item_size, unsigned long nitems)
	: free_list (nitems)
	, _name (n)
{
	_name = n;

	/* since some overloaded ::operator new() might use this,
	   its important that we use a "lower level" allocator to
	   get more space.  
	*/

	block = malloc (nitems * item_size);

	void **ptrlist = (void **) malloc (sizeof (void *)  * nitems);

	for (unsigned long i = 0; i < nitems; i++) {
		ptrlist[i] = static_cast<void *> (static_cast<char*>(block) + (i * item_size));
	}

	free_list.write (ptrlist, nitems);
	free (ptrlist);
}

Pool::~Pool ()
{
	free (block);
}

/** Allocate an item's worth of memory in the Pool by taking one from the free list.
 *  @return Pointer to free item.
 */
void *
Pool::alloc ()
{
	void *ptr;

	if (free_list.read (&ptr, 1) < 1) {
		fatal << "CRITICAL: " << _name << " POOL OUT OF MEMORY - RECOMPILE WITH LARGER SIZE!!" << endmsg;
		/*NOTREACHED*/
		return 0;
	} else {
		return ptr;
	}
}

/** Release an item's memory by writing its location to the free list */
void		
Pool::release (void *ptr)
{
	free_list.write (&ptr, 1);
}

/*---------------------------------------------*/

MultiAllocSingleReleasePool::MultiAllocSingleReleasePool (string n, unsigned long isize, unsigned long nitems) 
	: Pool (n, isize, nitems)
{
}

MultiAllocSingleReleasePool::~MultiAllocSingleReleasePool ()
{
}

SingleAllocMultiReleasePool::SingleAllocMultiReleasePool (string n, unsigned long isize, unsigned long nitems) 
	: Pool (n, isize, nitems)
{
}

SingleAllocMultiReleasePool::~SingleAllocMultiReleasePool ()
{
}

void*
MultiAllocSingleReleasePool::alloc ()
{
	void *ptr;
	Glib::Threads::Mutex::Lock guard (m_lock);
	ptr = Pool::alloc ();
	return ptr;
}

void
MultiAllocSingleReleasePool::release (void* ptr)
{
	Pool::release (ptr);
}

void*
SingleAllocMultiReleasePool::alloc ()
{
	return Pool::alloc ();
}

void
SingleAllocMultiReleasePool::release (void* ptr)
{
	Glib::Threads::Mutex::Lock guard (m_lock);
	Pool::release (ptr);
}

/*-------------------------------------------------------*/

static void 
free_per_thread_pool (void* ptr)
{
	/* Rather than deleting the CrossThreadPool now, we add it to our trash buffer.
	 * This prevents problems if other threads still require access to this CrossThreadPool.
	 * We assume that some other agent will clean out the trash buffer as required.
	 */
	CrossThreadPool* cp = static_cast<CrossThreadPool*> (ptr);
	assert (cp);

	if (cp->empty()) {
		/* This CrossThreadPool is already empty, and the thread is finishing so nothing
		 * more can be added to it.  We can just delete the pool.
		 */
		delete cp;
	} else {
		/* This CrossThreadPool is not empty, meaning that there's some Events in it
		 * which another thread may yet read, so we can't delete the pool just yet.
		 * Put it in the trash and hope someone deals with it at some stage.
		 */
		cp->parent()->add_to_trash (cp);
	}
}
 
PerThreadPool::PerThreadPool ()
	: _key (free_per_thread_pool)
	, _trash (0)
{
}

/** Create a new CrossThreadPool and set the current thread's private _key to point to it.
 *  @param n Name.
 *  @param isize Size of each item in the pool.
 *  @param nitems Number of items in the pool.
 */
void
PerThreadPool::create_per_thread_pool (string n, unsigned long isize, unsigned long nitems)
{
	_key.set (new CrossThreadPool (n, isize, nitems, this));
}

/** @return CrossThreadPool for the current thread, which must previously have been created by
 *  calling create_per_thread_pool in the current thread.
 */
CrossThreadPool*
PerThreadPool::per_thread_pool ()
{
	CrossThreadPool* p = _key.get();
	if (!p) {
		fatal << "programming error: no per-thread pool \"" << _name << "\" for thread " << pthread_self() << endmsg;
		/*NOTREACHED*/
	}
	return p;
}

void
PerThreadPool::set_trash (RingBuffer<CrossThreadPool*>* t)
{
	Glib::Threads::Mutex::Lock lm (_trash_mutex);
	_trash = t;
}

/** Add a CrossThreadPool to our trash, if we have one.  If not, a warning is emitted. */
void
PerThreadPool::add_to_trash (CrossThreadPool* p)
{
	Glib::Threads::Mutex::Lock lm (_trash_mutex);
	
	if (!_trash) {
		warning << "Pool " << p->name() << " has no trash collector; a memory leak has therefore occurred" << endmsg;
		return;
	}

	/* we have a lock here so that multiple threads can safely call add_to_trash (even though there
	   can only be one writer to the _trash RingBuffer)
	*/
		
	_trash->write (&p, 1);
}

CrossThreadPool::CrossThreadPool  (string n, unsigned long isize, unsigned long nitems, PerThreadPool* p)
	: Pool (n, isize, nitems)
	, pending (nitems)
	, _parent (p)
{
	
}

void*
CrossThreadPool::alloc () 
{
	void* ptr;

	DEBUG_TRACE (DEBUG::Pool, string_compose ("%1 %2 has %3 pending free entries waiting\n", pthread_self(), name(), pending.read_space()));
	while (pending.read (&ptr, 1) == 1) {
		DEBUG_TRACE (DEBUG::Pool, string_compose ("%1 %2 pushes back a pending free list entry before allocating\n", pthread_self(), name()));
		free_list.write (&ptr, 1);
	}
	return Pool::alloc ();
}

void
CrossThreadPool::push (void* t) 
{
	pending.write (&t, 1);
}

/** @return true if there is nothing in this pool */
bool
CrossThreadPool::empty ()
{
	return (free_list.write_space() == pending.read_space());
}

