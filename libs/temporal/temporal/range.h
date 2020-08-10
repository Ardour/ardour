/*
 * Copyright (C) 2008 David Robillard <http://drobilla.net>
 * Copyright (C) 2000-2017 Paul Davis
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

#ifndef __libtemporal_range_hpp__
#define __libtemporal_range_hpp__

#include <list>
#include <assert.h>
#include <iostream>

#include "temporal/visibility.h"
#include "temporal/timeline.h"

namespace Temporal {

enum /*LIBTEMPORAL_API*/ OverlapType {
	OverlapNone,      // no overlap
	OverlapInternal,  // the overlap is 100% within the object
	OverlapStart,     // overlap covers start, but ends within
	OverlapEnd,       // overlap begins within and covers end
	OverlapExternal   // overlap extends to (at least) begin+end
};

/** end position arguments are inclusive */
template<typename T>
/*LIBTEMPORAL_API*/ OverlapType coverage_inclusive_ends (T sa, T ea, T sb, T eb) {
	/* OverlapType returned reflects how the second (B)
	 * range overlaps the first (A).
	 *
	 * The diagram shows the OverlapType of each possible relative
	 * placement of A and B.
	 *
	 * Notes:
	 *    Internal: the start and end points cannot coincide
	 *    External: the start and end points can coincide
	 *    Start: end points can coincide
	 *    End: start points can coincide
	 *
	 * Internal disallows start and end point equality, and thus implies
	 * that there are two disjoint portions of A which do not overlap B.
	 *
	 * A:    |---|
	 * B starts before A
	 * B: |-|          None
	 * B: |--|         Start
	 * B: |----|       Start
	 * B: |------|     External
	 * B: |--------|   External
	 * B starts equal to A
	 * B:    |-|       Start
	 * B:    |---|     External
	 * B:    |----|    External
	 * B starts inside A
	 * B:     |-|      Internal
	 * B:     |--|     End
	 * B:     |---|    End
	 * B starts at end of A
	 * B:        |--|  End
	 * B starts after A
	 * B:         |-|  None
	 * A:    |---|
	 */

	if (sa > ea) {
		// seems we are sometimes called with negative length ranges
		return OverlapNone;
	}

	if (sb > eb) {
		// seems we are sometimes called with negative length ranges
		return OverlapNone;
	}

	if (sb < sa) {  // B starts before A
		if (eb < sa) {
			return OverlapNone;
		} else if (eb == sa) {
			return OverlapStart;
		} else { // eb > sa
			if (eb < ea) {
				return OverlapStart;
			} else if (eb == ea) {
				return OverlapExternal;
			} else {
				return OverlapExternal;
			}
		}
	} else if (sb == sa) { // B starts equal to A
		if (eb < ea) {
			return OverlapStart;
		} else if (eb == ea) {
			return OverlapExternal;
		} else { // eb > ea
			return OverlapExternal;
		}
	} else { // sb > sa
		if (eb < ea) {
			return OverlapInternal;
		} else if (eb == ea) {
			return OverlapEnd;
		} else { // eb > ea
			if (sb < ea) { // B starts inside A
				return OverlapEnd;
			} else if (sb == ea) { // B starts at end of A
				return OverlapEnd;
			} else { // sb > ea, B starts after A
				return OverlapNone;
			}
		}
	}

	std::cerr << "unknown overlap type!" << sa << ", " << ea << "; " << sb << ", " << eb << std::endl;
	assert(!"unknown overlap type!");
	return OverlapNone;
}

/** end position arguments are inclusive */
template<typename T>
/*LIBTEMPORAL_API*/ OverlapType coverage_exclusive_ends (T sa, T eaE, T sb, T ebE)
{
	/* convert end positions to inclusive */
	return coverage_inclusive_ends (sa, eaE.decrement(), sb, ebE.decrement());
}


class RangeList;

class LIBTEMPORAL_API Range {
  public:
	/* exclusive end semantics
	 *
	 * |--------------------------------------|
	 * ^                                      ^
	 * start                                  end
	 * 0                                      10 => length = 10, last position inside range = 9
	 * 1                                      11 => length = 10, last position inside range = 10
	 * 0                                      1  => length = 1,  last position inside range = 0
	 * 0                                      2  => length = 2,  last position inside range = 1
	 * 32                                     48 => length = 16, last position inside range = 47
	 */

	Range (timepos_t const & s, timepos_t const & e) : _start (s), _end (e) {}
	bool empty() const { return _start == _end; }
	timecnt_t length() const { return _start.distance (_end); }

	RangeList subtract (RangeList &) const;

	void set_start (timepos_t s) { _start = s; }
	void set_end (timepos_t e) { _end = e; }

	timepos_t start() const { return _start; }
	timepos_t end() const   { return _end; }

	bool operator== (Range const & other) const {
		return other._start == _start && other._end == _end;
	}

	/** for a T, return a mapping of it into the range (used for
	 * looping). If the argument is earlier than or equal to the end of
	 * this range, do nothing.
	 */
	timepos_t squish (timepos_t t) const {
		if (t >= _end) {
			t = _start + (_start.distance (t) % length());
		}
		return t;
	}

	/* end is exclusive */
	OverlapType coverage (timepos_t const & s, timepos_t const & e) const {
		return Temporal::coverage_exclusive_ends (_start, _end, s, e);
	}

  private:
	timepos_t _start; ///< start of the range
	timepos_t _end;   ///< end of the range (exclusive, see above)
};

typedef Range TimeRange;

class LIBTEMPORAL_API RangeList {
public:
	RangeList () : _dirty (false) {}

	typedef std::list<Range> List;

	List const & get () {
		coalesce ();
		return _list;
	}

	void add (Range const & range) {
		_dirty = true;
		_list.push_back (range);
	}

	bool empty () const {
		return _list.empty ();
	}

	void coalesce () {
		if (!_dirty) {
			return;
		}

	restart:
		for (typename List::iterator i = _list.begin(); i != _list.end(); ++i) {
			for (typename List::iterator j = _list.begin(); j != _list.end(); ++j) {

				if (i == j) {
					continue;
				}

				if (coverage_exclusive_ends (i->start(), i->end(), j->start(), j->end()) != OverlapNone) {
					i->set_start (std::min (i->start(), j->start()));
					i->set_end (std::max (i->end(), j->end()));
					_list.erase (j);
					goto restart;
				}
			}
		}

		_dirty = false;
	}

private:

	List _list;
	bool _dirty;
};

/** Type to describe the movement of a time range */
struct LIBTEMPORAL_API RangeMove {
	RangeMove (timepos_t f, timecnt_t l, timepos_t t) : from (f), length (l), to (t) {}
	timepos_t from;   ///< start of the range
	timecnt_t length; ///< length of the range
	timepos_t to;     ///< new start of the range
};

}

#endif /* __libtemporal_range_hpp__ */
