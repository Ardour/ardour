/* This file is part of Evoral.
 * Copyright (C) 2008 David Robillard <http://drobilla.net>
 * Copyright (C) 2000-2008 Paul Davis
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

#ifndef EVORAL_RANGE_HPP
#define EVORAL_RANGE_HPP

#include <list>
#include <assert.h>
#include <iostream>
#include "evoral/visibility.h"



namespace Evoral {

enum /*LIBEVORAL_API*/ OverlapType {
	OverlapNone,      // no overlap
	OverlapInternal,  // the overlap is 100% within the object
	OverlapStart,     // overlap covers start, but ends within
	OverlapEnd,       // overlap begins within and covers end
	OverlapExternal   // overlap extends to (at least) begin+end
};

template<typename T>
/*LIBEVORAL_API*/ OverlapType coverage (T sa, T ea, T sb, T eb) {
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
		std::cerr << "a - start after end: " << sa << ", " << ea << std::endl;
		return OverlapNone;
	}

	if (sb > eb) {
		// seems we are sometimes called with negative length ranges
		std::cerr << "b - start after end: " << sb << ", " << eb << std::endl;
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

/** Type to describe a time range */
template<typename T>
struct /*LIBEVORAL_API*/ Range {
	Range (T f, T t) : from (f), to (t) {}
	T from; ///< start of the range
	T to;   ///< end of the range (inclusive: to lies inside the range)
	bool empty() const { return from == to; }
};

template<typename T>	
bool operator== (Range<T> a, Range<T> b) {
	return a.from == b.from && a.to == b.to;
}

template<typename T>
class /*LIBEVORAL_API*/ RangeList {
public:
	RangeList () : _dirty (false) {}
	
	typedef std::list<Range<T> > List;

	List const & get () {
		coalesce ();
		return _list;
	}

	void add (Range<T> const & range) {
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

				if (coverage (i->from, i->to, j->from, j->to) != OverlapNone) {
					i->from = std::min (i->from, j->from);
					i->to = std::max (i->to, j->to);
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
template<typename T>
struct /*LIBEVORAL_API*/ RangeMove {
	RangeMove (T f, double l, T t) : from (f), length (l), to (t) {}
	T         from;   ///< start of the range
	double    length; ///< length of the range
	T         to;     ///< new start of the range
};

/** Subtract the ranges in `sub' from that in `range',
 *  returning the result.
 */
template<typename T>
RangeList<T> subtract (Range<T> range, RangeList<T> sub)
{
	/* Start with the input range */
	RangeList<T> result;
	result.add (range);

	if (sub.empty () || range.empty()) {
		return result;
	}

	typename RangeList<T>::List s = sub.get ();

	/* The basic idea here is to keep a list of the result ranges, and subtract
	   the bits of `sub' from them one by one.
	*/
	
	for (typename RangeList<T>::List::const_iterator i = s.begin(); i != s.end(); ++i) {

		/* Here's where we'll put the new current result after subtracting *i from it */
		RangeList<T> new_result;

		typename RangeList<T>::List r = result.get ();

		/* Work on all parts of the current result using this range *i */
		for (typename RangeList<T>::List::const_iterator j = r.begin(); j != r.end(); ++j) {

			switch (coverage (j->from, j->to, i->from, i->to)) {
			case OverlapNone:
				/* The thing we're subtracting (*i) does not overlap this bit of the result (*j),
				   so pass it through.
				*/
				new_result.add (*j);
				break;
			case OverlapInternal:
				/* Internal overlap of the thing we're subtracting (*i) from this bit of the result,
				   so we should end up with two bits of (*j) left over, from the start of (*j) to
				   the start of (*i), and from the end of (*i) to the end of (*j).
				*/
				assert (j->from < i->from);
				assert (j->to > i->to);
				new_result.add (Range<T> (j->from, i->from - 1));
				new_result.add (Range<T> (i->to + 1, j->to));
				break;
			case OverlapStart:
				/* The bit we're subtracting (*i) overlaps the start of the bit of the result (*j),
				 * so we keep only the part of of (*j) from after the end of (*i)
				 */
				assert (i->to < j->to);
				new_result.add (Range<T> (i->to + 1, j->to));
				break;
			case OverlapEnd:
				/* The bit we're subtracting (*i) overlaps the end of the bit of the result (*j),
				 * so we keep only the part of of (*j) from before the start of (*i)
				 */
				assert (j->from < i->from);
				new_result.add (Range<T> (j->from, i->from - 1));
				break;
			case OverlapExternal:
				/* total overlap of the bit we're subtracting with the result bit, so the
				   result bit is completely removed; do nothing */
				break;
			}
		}

		new_result.coalesce ();
		result = new_result;
	}

	return result;
}

}

#endif
