/* This file is part of Evoral.
 * Copyright (C) 2016 Paul Davis
 *
 * Evoral is free software; you can redistribute it and/or modify it under the
 * terms of the GNU General Public License as published by the Free Software
 * Foundation; either version 2 of the License, or (at your option) any later
 * version.
 *
 * Evoral is distributed in the hope that it will be useful, but WITHOUT ANY
 * WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE.  See the GNU General Public License for details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin St, Fifth Floor, Boston, MA 02110-1301 USA
 */

#ifndef EVORAL_EVENTPOOL_HPP
#define EVORAL_EVENTPOOL_HPP

#include <cstdlib>
#include <iostream>
#include <string>
#include <unistd.h>
#include <vector>

#include <boost/atomic.hpp>
#include <boost/utility.hpp>

#include "evoral/visibility.h"

#define DEBUG_EVENT_POOL 1

namespace Evoral {

class LIBEVORAL_API EventPool
{
  private:
	struct FreeList : public std::vector<void*>, boost::noncopyable {
		size_t item_size;
		void*  block;
		void*  end;

		FreeList (size_t sz, size_t num_items) {

			/* ensure that size is correctly set to give alignment
			   for each item.
			*/
			item_size = aligned_size (sz);

			/* allocate block and save its end to allow to us to
			 * quickly identify which if a pointer was allocated
			 * from this freelist (all pointers will be between
			 * block and end).
			 */
			block = malloc (item_size * num_items);
			end = (char*) block + (item_size * num_items);

			/* reserve space to make the initial setup slightly more efficient */
			reserve (num_items);

			/* store address of each chunk of size "sz" in our list */
			for (size_t n = 0; n < num_items; ++n) {
				push_back (((char*) block) + (n * item_size));
			}
		}

		~FreeList () {
			::free (block);
		}

		bool owns (void *ptr) const { return ptr >= block && ptr < end; }
		size_t num_items() const { return (((char*)end - (char*)block) / item_size); }
	};

	typedef std::vector<FreeList*> FreeLists;

  public:
	typedef std::pair<size_t,size_t> SizePair;
	typedef std::vector<SizePair> SizePairs;

	EventPool (std::string const & str, SizePairs const & sp) : _name (str) {
		add (sp);
	}
	EventPool (std::string const & str) : _name (str)  { /* no pools allocated */ }

	void add (SizePairs const & sp) {
		_freelists.reserve (_freelists.size() + sp.size());
		for (SizePairs::const_iterator s = sp.begin(); s != sp.end(); ++s) {
			_freelists.push_back (new FreeList (s->first, s->second));
		}
	}

	std::string name() const { return _name; }

	static size_t aligned_size (size_t sz) {
		const int align_size = 8; /* XXX probably a better value for this */
		return  (sz + align_size - 1) & ~(align_size - 1);
	}

	void* alloc (size_t sz) {

		sz = aligned_size (sz);

	  repeat:
		for (FreeLists::iterator x = _freelists.begin(); x != _freelists.end(); ++x) {
			if ((*x)->item_size >= sz) {
				if ((*x)->empty()) {
					/* out of memory - grab a new block */
					std::cerr << "Out of memory in pool for size " << sz << " allocate a new block\n";
					SizePairs spp;
					spp.push_back (std::make_pair<size_t,size_t> (sz, (*x)->num_items()));
					add (spp);
					goto repeat;
				}

				void* ret = (*x)->back();
				(*x)->pop_back ();
#ifdef DEBUG_EVENT_POOL
				std::cerr << name() << ": alloc size[" << (*x)->item_size << "] free = "
				          << (*x)->size()
				          << " alloc = " << ((char*)(*x)->end - (char*)(*x)->block) / sizeof ((*x)->item_size)
				          << std::endl;
#endif

				return ret;
			}
		}
		std::cerr << "Size too large\n";
		return 0;
	}

	void release (void* p) {
		for (FreeLists::iterator x = _freelists.begin(); x != _freelists.end(); ++x) {
			if ((*x)->owns (p)) {
				(*x)->push_back (p);
#ifdef DEBUG_EVENT_POOL
				std::cerr << name() << ": release size[" << (*x)->item_size << "] free = "
				          << (*x)->size()
				          << " alloc = " << ((char*)(*x)->end - (char*)(*x)->block) / sizeof ((*x)->item_size)
				          << std::endl;
#endif
				return;
			}
		}
	}

  private:
	FreeLists _freelists;
	std::string _name;
};

class LIBEVORAL_API PoolAllocated
{
  public:
	PoolAllocated (EventPool* p) : pool (p) {}

	void operator delete (void *ptr) {
		PoolAllocated* pa = reinterpret_cast<PoolAllocated*>(ptr);
		if (pa && pa->pool) {
			pa->pool->release (ptr);
		}
	}

  private:
	EventPool* pool;
};

} // namespace Evoral

#endif /* EVORAL_EVENTPOOL_HPP */
