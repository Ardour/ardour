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

#include <glibmm/thread.h>

#include "pbd/ringbuffer.h"

class Pool 
{
  public:
	Pool (std::string name, unsigned long item_size, unsigned long nitems);
	virtual ~Pool ();

	virtual void *alloc ();
	virtual void release (void *);
	
	std::string name() const { return _name; }

  private:
	RingBuffer<void*>* free_list;
	std::string _name;
	void *block;
};

class SingleAllocMultiReleasePool : public Pool
{
  public:
	SingleAllocMultiReleasePool (std::string name, unsigned long item_size, unsigned long nitems);
	~SingleAllocMultiReleasePool ();

	virtual void *alloc ();
	virtual void release (void *);

  private:
    Glib::Mutex* m_lock;
};


class MultiAllocSingleReleasePool : public Pool
{
  public:
	MultiAllocSingleReleasePool (std::string name, unsigned long item_size, unsigned long nitems);
	~MultiAllocSingleReleasePool ();

	virtual void *alloc ();
	virtual void release (void *);

  private:
    Glib::Mutex* m_lock;
};

#endif // __qm_pool_h__
