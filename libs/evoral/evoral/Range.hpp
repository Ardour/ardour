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

#include "evoral/visibility.h"

namespace Evoral {

enum LIBEVORAL_API OverlapType {
	OverlapNone,      // no overlap
	OverlapInternal,  // the overlap is 100% with the object
	OverlapStart,     // overlap covers start, but ends within
	OverlapEnd,       // overlap begins within and covers end
	OverlapExternal   // overlap extends to (at least) begin+end
};

template<typename T>
LIBEVORAL_API OverlapType coverage (T sa, T ea, T sb, T eb) {
	/* OverlapType returned reflects how the second (B)
	   range overlaps the first (A).

	   The diagrams show various relative placements
	   of A and B for each OverlapType.

	   Notes:
	      Internal: the start points cannot coincide
	      External: the start and end points can coincide
	      Start: end points can coincide
	      End: start points can coincide

	   XXX Logically, Internal should disallow end
	   point equality.
	*/

	/*
	     |--------------------|   A
	          |------|            B
	        |-----------------|   B


             "B is internal to A"

	*/

	if ((sb > sa) && (eb <= ea)) {
		return OverlapInternal;
	}

	/*
	     |--------------------|   A
	   ----|                      B
           -----------------------|   B
	   --|                        B

	     "B overlaps the start of A"

	*/

	if ((eb >= sa) && (eb <= ea)) {
		return OverlapStart;
	}
	/*
	     |---------------------|  A
                   |----------------- B
	     |----------------------- B
                                   |- B

            "B overlaps the end of A"

	*/
	if ((sb > sa) && (sb <= ea)) {
		return OverlapEnd;
	}
	/*
	     |--------------------|     A
	   --------------------------  B
	     |-----------------------  B
	    ----------------------|    B
             |--------------------|    B


           "B overlaps all of A"
	*/
	if ((sa >= sb) && (sa <= eb) && (ea <= eb)) {
		return OverlapExternal;
	}

	return OverlapNone;
}

/** Type to describe a time range */
template<typename T>
struct LIBEVORAL_API Range {
	Range (T f, T t) : from (f), to (t) {}
	T from; ///< start of the range
	T to;   ///< end of the range
};

template<typename T>	
bool operator== (Range<T> a, Range<T> b) {
	return a.from == b.from && a.to == b.to;
}

template<typename T>
class LIBEVORAL_API RangeList {
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
struct LIBEVORAL_API RangeMove {
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

	if (sub.empty ()) {
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
				/* The thing we're subtracting does not overlap this bit of the result,
				   so pass it through.
				*/
				new_result.add (*j);
				break;
			case OverlapInternal:
				/* Internal overlap of the thing we're subtracting from this bit of the result,
				   so we might end up with two bits left over.
				*/
				if (j->from < (i->from - 1)) {
					new_result.add (Range<T> (j->from, i->from - 1));
				}
				if (j->to != i->to) {
					new_result.add (Range<T> (i->to, j->to));
				}
				break;
			case OverlapStart:
				/* The bit we're subtracting overlaps the start of the bit of the result */
				new_result.add (Range<T> (i->to, j->to - 1));
				break;
			case OverlapEnd:
				/* The bit we're subtracting overlaps the end of the bit of the result */
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
