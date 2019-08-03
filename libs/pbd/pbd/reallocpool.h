/*
 * Copyright (C) 2016 Robin Gareus <robin@gareus.org>
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
#ifndef _reallocpool_h_
#define _reallocpool_h_

#ifndef NDEBUG
//#define RAP_WITH_CALL_STATS // collect statistics on calls counts (light)
//#define RAP_WITH_HISTOGRAM 513 // collect statistic about allocation size (not bad)
//#define RAP_WITH_SEGMENT_STATS // collect statistics (expensive)
#endif

#ifndef RAP_BLOCKSIZE
#define RAP_BLOCKSIZE 7 // [bytes] power-of-two minus one (optional)
#endif

#ifdef RAP_WITH_SEGMENT_STATS
#define RAP_WITH_CALL_STATS
#endif

#include <string>

#ifndef LIBPBD_API
#include "pbd/libpbd_visibility.h"
#endif

namespace PBD {

class LIBPBD_API ReallocPool
{
public:
	ReallocPool (std::string name, size_t bytes);
	~ReallocPool ();

	void set_name (const std::string& n) { _name = n; }

	static void * lalloc (void* pool, void *ptr, size_t oldsize, size_t newsize) {
		return static_cast<ReallocPool*>(pool)->_realloc (ptr, oldsize, newsize);
	}

	void * malloc (size_t size) {
		return _realloc (NULL, 0, size);
	}

	void free (void *ptr) {
		if (ptr) _realloc (ptr, 0, 0);
	}

	void * realloc (void *ptr, size_t newsize) {
		return _realloc (ptr, _asize(ptr), newsize);
	}

	void printstats ();
	void dumpsegments ();

#ifdef RAP_WITH_CALL_STATS
	size_t mem_used () const { return _cur_used; }
#endif

private:
	std::string _name;
	size_t _poolsize;
	char *_pool;
	char *_mru;

#ifdef RAP_WITH_SEGMENT_STATS
	size_t _cur_avail;
	size_t _cur_allocated;
	size_t _max_allocated;
	size_t _seg_cur_count;
	size_t _seg_max_count;
	size_t _seg_max_used;
	size_t _seg_max_avail;
	void collect_segment_stats ();
#endif
#ifdef RAP_WITH_CALL_STATS
	size_t _n_alloc;
	size_t _n_grow;
	size_t _n_shrink;
	size_t _n_free;
	size_t _n_noop;
	size_t _n_oom;
	size_t _cur_used; // cheaper _cur_allocated
	size_t _max_used; // cheaper _max_allocated
#endif
#ifdef RAP_WITH_HISTOGRAM
	size_t _hist_alloc [RAP_WITH_HISTOGRAM];
	size_t _hist_free [RAP_WITH_HISTOGRAM];
	size_t _hist_grow [RAP_WITH_HISTOGRAM];
	size_t _hist_shrink [RAP_WITH_HISTOGRAM];

	unsigned int hist_bin (size_t s) const;
	void print_histogram (size_t const * const histogram) const;
#endif

	void *_realloc (void *ptr, size_t oldsize, size_t newsize);
	void *_malloc (size_t);
	void _free (void *ptr);
	void _shrink (void *, size_t);
	size_t _asize (void *);
	void consolidate_ptr (char *);
};

} /* namespace */
#endif // _reallocpool_h_
