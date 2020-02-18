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

#include <stdlib.h>
#include <string.h>
#include <cstdio>

#ifndef PLATFORM_WINDOWS
#include <sys/mman.h>
#endif

#include "pbd/reallocpool.h"

#ifdef RAP_WITH_SEGMENT_STATS
#include <assert.h>
#define STATS_segment collect_segment_stats();
#define ASSERT assert
#else
#define STATS_segment
#define ASSERT
#endif

#ifdef RAP_WITH_CALL_STATS
#define STATS_inc(VAR) ++VAR;
#define STATS_if(COND, VAR) if (COND) {++VAR;}
#define STATS_used(DELTA) { _cur_used += (DELTA); if (_cur_used > _max_used) { _max_used = _cur_used; } }
#else
#define STATS_inc(VAR)
#define STATS_if(COND, VAR)
#define STATS_used(DELTA)
#endif

#ifdef RAP_WITH_HISTOGRAM
#define STATS_hist(VAR, SIZE) ++VAR[hist_bin(SIZE)];
#else
#define STATS_hist(VAR, SIZE)
#endif

using namespace PBD;

typedef int poolsize_t;

ReallocPool::ReallocPool (std::string name, size_t bytes)
	: _name (name)
	, _poolsize (bytes)
	, _pool (0)
#ifdef RAP_WITH_SEGMENT_STATS
	, _cur_avail (0)
	, _cur_allocated (0)
	, _max_allocated (0)
	, _seg_cur_count (0)
	, _seg_max_count (0)
	, _seg_max_used (0)
	, _seg_max_avail (0)
#endif
#ifdef RAP_WITH_CALL_STATS
	, _n_alloc (0)
	, _n_grow (0)
	, _n_shrink (0)
	, _n_free (0)
	, _n_noop (0)
	, _n_oom (0)
	, _cur_used (0)
	, _max_used (0)
#endif
{
	_pool = (char*) ::malloc (bytes);

	memset (_pool, 0, bytes); // make resident
#ifndef PLATFORM_WINDOWS
	mlock (_pool, bytes);
#endif

	poolsize_t *in = (poolsize_t*) _pool;
	*in = - (bytes - sizeof (poolsize_t));
	_mru = _pool;

#ifdef RAP_WITH_HISTOGRAM
	for (int i = 0; i < RAP_WITH_HISTOGRAM; ++i) {
		_hist_alloc[i] = _hist_free[i] = _hist_grow[i] = _hist_shrink[i] = 0;
	}
#endif
}

ReallocPool::~ReallocPool ()
{
	STATS_segment;
	printstats ();
	::free (_pool);
	_pool = NULL;
}

// realloc() does it all, malloc(), realloc() and free()
void *
ReallocPool::_realloc (void *ptr, size_t oldsize, size_t newsize) {
	void *rv = NULL;
	ASSERT (!ptr || oldsize <= _asize (ptr));
	oldsize = _asize (ptr); // ignore provided oldsize

	if (ptr == 0 && newsize == 0) {
		STATS_inc(_n_noop);
		return NULL;
	}

	if (ptr == 0) {
		rv = _malloc (newsize);
		STATS_if (!rv, _n_oom);
		STATS_inc(_n_alloc);
		STATS_hist(_hist_alloc, newsize);
		STATS_segment;
		return rv;
	}

	if (newsize == 0) {
		STATS_hist(_hist_free, _asize(ptr));
		STATS_inc(_n_free);
		STATS_segment;
		_free (ptr);
		return NULL;
	}

	if (newsize == oldsize) {
		ASSERT (_asize (ptr) <= newsize);
		STATS_inc(_n_noop);
		return ptr;
	}

	if (newsize > oldsize) {
#ifdef RAP_BLOCKSIZE
		const size_t ns = (newsize + RAP_BLOCKSIZE) & (~RAP_BLOCKSIZE);
		if (ns <= _asize(ptr)) {
			STATS_inc(_n_noop);
			return ptr;
		}
#endif
		if ((rv = _malloc (newsize))) {
			memcpy (rv, ptr, oldsize);
		}
		if (rv) {
			_free (ptr);
		}
		STATS_if(!rv, _n_oom);
		STATS_inc(_n_grow);
		STATS_hist(_hist_grow, newsize);
		STATS_segment;
		return rv;
	}

	if (newsize < oldsize) {
		ASSERT (_asize (ptr) >= newsize);
#if 0 // re-allocate
		if ((rv = _malloc (newsize))) {
			memccpy (rv, ptr, newsize);
		}
		STATS_if(!rv, _n_oom);
		_free (ptr);
#elif 1 // shrink current segment
		const size_t ns = (newsize + RAP_BLOCKSIZE) & (~RAP_BLOCKSIZE);
		_shrink (ptr, ns);
		rv = ptr;
#else // do nothing
		rv = ptr;
#endif
		STATS_inc(_n_shrink);
		STATS_hist (_hist_shrink, newsize);
		STATS_segment;
		return rv;
	}
	return NULL; // not reached
}

#define SEGSIZ (*((poolsize_t*) p))

void
ReallocPool::consolidate_ptr (char *p) {
	const poolsize_t sop = sizeof(poolsize_t);
	if (p - SEGSIZ + sop >= _pool + _poolsize) {
		return; // reached end
	}
	poolsize_t *next = (poolsize_t*)(p - SEGSIZ + sop);
	while (*next < 0) {
		SEGSIZ = SEGSIZ + (*next) - sop;
		if (p - SEGSIZ + sop >= _pool + _poolsize) {
			break;
		}
		next = (poolsize_t*)(p -SEGSIZ + sop);
	}
	_mru = p;
}

void *
ReallocPool::_malloc (size_t s) {
	const poolsize_t sop = sizeof(poolsize_t);
	size_t traversed = 0;
	char *p = _mru;

#ifdef RAP_BLOCKSIZE
	s = (s + RAP_BLOCKSIZE) & (~RAP_BLOCKSIZE); // optional, helps to reduce fragmentation
#endif

	while (1) { // iterates at most once over the available pool
		ASSERT (SEGSIZ != 0);
		while (SEGSIZ > 0) {
			traversed += SEGSIZ + sop;
			if (traversed >= _poolsize) {
				return NULL; // reached last segment. OOM.
			}
			p += SEGSIZ + sop;
			if (p == _pool + _poolsize) {
				p = _pool;
			}
		}

		// found free segment.
		const poolsize_t avail = -SEGSIZ;
		const poolsize_t sp = (poolsize_t)s;
		const poolsize_t ss = sop + s;

		if (sp == avail) {
			// exact match
			SEGSIZ = -SEGSIZ;
			STATS_used (s);
			return (p + sop);
		}

		if (ss < avail) {
			// segment is larger than required space,
			// (we need to fit data + two non-zero poolsize_t)
			SEGSIZ = sp; // mark area as used.
			*((poolsize_t*)(p + ss)) = ss - avail; // mark free space after.
			consolidate_ptr (p + ss);
			_mru = p + ss;
			STATS_used (s);
			return (p + sop);
		}

		// segment is not large enough
		consolidate_ptr (p); // try to consolidate with next segment (if any)

		// check segment (again) and skip over too small free ones
		while (SEGSIZ < 0 && (-SEGSIZ) <= ss && (-SEGSIZ) != sp) {
			traversed += -SEGSIZ + sop;
			if (traversed >= _poolsize) {
				return NULL; // reached last segment. OOM.
			}
			p += (-SEGSIZ) + sop;
			if (p >= _pool + _poolsize) {
				p = _pool;
				if (SEGSIZ < 0) consolidate_ptr (p);
			}
		}
	}
}
#undef SEGSIZ

void
ReallocPool::_free (void *ptr) {
	poolsize_t *in = (poolsize_t*) ptr;
	--in;
	*in = -*in; // mark as free
	//_mru = p + ss;
	STATS_used (*in);
}

void
ReallocPool::_shrink (void *ptr, size_t newsize) {
	poolsize_t *in = (poolsize_t*) ptr;
	--in;
	const poolsize_t avail = *in;
	const poolsize_t ss = newsize + sizeof(poolsize_t);
	if (avail <= ss) {
		return; // can't shrink
	}
	const poolsize_t sp = (poolsize_t)newsize;
	STATS_used (newsize - avail);
	*in = sp; // set new size
	char *p = (char*) in;
	*((poolsize_t*)(p + ss)) = ss - avail; // mark free space after.
	//_mru = p + ss;
}

size_t
ReallocPool::_asize (void *ptr) {
	if (ptr == 0) return 0;
	poolsize_t *in = (poolsize_t*) ptr;
	--in;
	return (*in);
}


/** STATS **/

void
ReallocPool::printstats ()
{
#ifdef RAP_WITH_SEGMENT_STATS
	printf ("ReallocPool '%s': used: %ld (%.1f%%) (max: %ld), free: %ld [bytes]\n"
			"|| segments: cur: %ld (max: %ld), largest-used: %ld, largest-free: %ld\n",
			_name.c_str(),
			_cur_allocated, _cur_allocated * 100.f / _poolsize, _max_allocated, _cur_avail,
			_seg_cur_count, _seg_max_count, _seg_max_used, _seg_max_avail);
#elif defined RAP_WITH_CALL_STATS
	printf ("ReallocPool '%s':\n", _name.c_str());
#endif
#ifdef RAP_WITH_CALL_STATS
		printf("|| malloc(): %ld, free(): %ld, realloc()+:%ld, realloc()-: %ld NOOP:%ld OOM:%ld\n",
			_n_alloc, _n_free, _n_grow, _n_shrink, _n_noop, _n_oom);
		printf("|| used: %ld / %ld, max: %ld (%.1f%%)\n",
				_cur_used, _poolsize,
				_max_used, 100.f *_max_used / _poolsize);
#endif
#ifdef RAP_WITH_HISTOGRAM
		printf("--- malloc()\n");
		print_histogram (_hist_alloc);
		printf("--- realloc()/grow-to\n");
		print_histogram (_hist_grow);
		printf("--- realloc()/shrink-to\n");
		print_histogram (_hist_shrink);
		printf("--- free() histogram\n");
		print_histogram (_hist_free);
		printf("--------------------\n");
#endif
}

void
ReallocPool::dumpsegments ()
{
	char *p = _pool;
	const poolsize_t sop = sizeof(poolsize_t);
	poolsize_t *in = (poolsize_t*) p;
	unsigned int traversed = 0;
#ifdef RAP_WITH_CALL_STATS
	size_t used = 0;
#endif
	printf ("<<<<< %s\n", _name.c_str());
	while (1) {
		if ((*in) > 0) {
			printf ("0x%08x used %4d\n", traversed, *in);
			printf ("0x%08x   data %p\n", traversed + sop , p + sop);
			traversed += *in + sop;
			p += *in + sop;
#ifdef RAP_WITH_CALL_STATS
			used += *in;
#endif
		} else if ((*in) < 0) {
			printf ("0x%08x free %4d [+%d]\n", traversed, -*in, sop);
			traversed += -*in + sop;
			p += -*in + sop;
		} else {
			printf ("0x%08x Corrupt!\n", traversed);
			break;
		}
		in = (poolsize_t*) p;
		if (p == _pool + _poolsize) {
			printf ("%08x end\n", traversed);
			break;
		}
		if (p > _pool + _poolsize) {
			printf ("%08x Beyond End!\n", traversed);
			break;
		}
	}
#ifdef RAP_WITH_CALL_STATS
	ASSERT (_cur_used == used);
#endif
	printf (">>>>>\n");
}

#ifdef RAP_WITH_SEGMENT_STATS
void
ReallocPool::collect_segment_stats ()
{
	char *p = _pool;
	poolsize_t *in = (poolsize_t*) p;

	_cur_allocated = _cur_avail = 0;
	_seg_cur_count = _seg_max_avail = _seg_max_used = 0;

	while (1) {
		++_seg_cur_count;
		if ((*in) > 0) {
			_cur_allocated += *in;
			p += *in;
			if (*in > (poolsize_t)_seg_max_used) {
				_seg_max_used = *in;
			}
		} else {
			_cur_avail += -*in;
			p += -*in;
			if (-*in > (poolsize_t)_seg_max_avail) {
				_seg_max_avail = -*in;
			}
		}
		p += sizeof(poolsize_t);
		in = (poolsize_t*) p;
		if (p == _pool + _poolsize) {
			break;
		}
	}
	_seg_cur_count = _seg_cur_count;

	if (_cur_allocated > _max_allocated) {
		_max_allocated = _cur_allocated;
	}
	if (_seg_cur_count > _seg_max_count) {
		_seg_max_count = _seg_cur_count;
	}
}
#endif

#ifdef RAP_WITH_HISTOGRAM
void
ReallocPool::print_histogram (size_t const * const histogram) const
{
	size_t maxhist = 0;
	for (int i = 0; i < RAP_WITH_HISTOGRAM; ++i) {
		if (histogram[i] > maxhist) maxhist = histogram[i];
	}
	const int termwidth = 50;
#ifdef RAP_BLOCKSIZE
	const int fact = RAP_BLOCKSIZE + 1;
#endif
	if (maxhist > 0) for (int i = 0; i < RAP_WITH_HISTOGRAM; ++i) {
		if (histogram[i] == 0) { continue; }
		if (i == RAP_WITH_HISTOGRAM -1 ) {
#ifdef RAP_BLOCKSIZE
			printf("     > %4d: %7lu ", i * fact, histogram[i]);
#else
			printf(">%4d:%7lu ", i * fact, histogram[i]);
#endif
		} else {
#ifdef RAP_BLOCKSIZE
			printf("%4d .. %4d: %7lu ", i * fact, (i + 1) * fact -1, histogram[i]);
#else
			printf("%4d: %7lu ", i * fact, (i + 1) * fact -1 , histogram[i]);
#endif
		}
		int bar_width = (histogram[i] * termwidth ) / maxhist;
		if (bar_width == 0 && histogram[i] > 0) bar_width = 1;
		for (int j = 0; j < bar_width; ++j) printf("#");
		printf("\n");
	}
}

unsigned int
ReallocPool::hist_bin (size_t s) const {
#ifdef RAP_BLOCKSIZE
	s = (s + RAP_BLOCKSIZE) & (~RAP_BLOCKSIZE);
	s /= (RAP_BLOCKSIZE + 1);
#endif
	if (s > RAP_WITH_HISTOGRAM - 1) s = RAP_WITH_HISTOGRAM - 1;
	return s;
}
#endif
