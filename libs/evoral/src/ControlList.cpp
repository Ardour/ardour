/* This file is part of Evoral.
 * Copyright (C) 2008 Dave Robillard <http://drobilla.net>
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

#include <cmath>
#include <cassert>
#include <utility>
#include <iostream>
#include <evoral/ControlList.hpp>

using namespace std;

namespace Evoral {


inline bool event_time_less_than (ControlEvent* a, ControlEvent* b)
{
	return a->when < b->when;
}


ControlList::ControlList (const Parameter& id)
	: _parameter(id)
	, _interpolation(Linear)
	, _curve(new Curve(*this))
{	
	_frozen = 0;
	_changed_when_thawed = false;
	_min_yval = id.min();
	_max_yval = id.max();
	_max_xval = 0; // means "no limit" 
	_rt_insertion_point = _events.end();
	_lookup_cache.left = -1;
	_lookup_cache.range.first = _events.end();
	_search_cache.left = -1;
	_search_cache.range.first = _events.end();
	_sort_pending = false;
}

ControlList::ControlList (const ControlList& other)
	: _parameter(other._parameter)
	, _interpolation(Linear)
	, _curve(new Curve(*this))
{
	_frozen = 0;
	_changed_when_thawed = false;
	_min_yval = other._min_yval;
	_max_yval = other._max_yval;
	_max_xval = other._max_xval;
	_default_value = other._default_value;
	_rt_insertion_point = _events.end();
	_lookup_cache.range.first = _events.end();
	_search_cache.range.first = _events.end();
	_sort_pending = false;

	for (const_iterator i = other._events.begin(); i != other._events.end(); ++i) {
		_events.push_back (new ControlEvent (**i));
	}

	mark_dirty ();
}

ControlList::ControlList (const ControlList& other, double start, double end)
	: _parameter(other._parameter)
	, _interpolation(Linear)
	, _curve(new Curve(*this))
{
	_frozen = 0;
	_changed_when_thawed = false;
	_min_yval = other._min_yval;
	_max_yval = other._max_yval;
	_max_xval = other._max_xval;
	_default_value = other._default_value;
	_rt_insertion_point = _events.end();
	_lookup_cache.range.first = _events.end();
	_search_cache.range.first = _events.end();
	_sort_pending = false;

	/* now grab the relevant points, and shift them back if necessary */

	boost::shared_ptr<ControlList> section = const_cast<ControlList*>(&other)->copy (start, end);

	if (!section->empty()) {
		for (iterator i = section->begin(); i != section->end(); ++i) {
			_events.push_back (new ControlEvent ((*i)->when, (*i)->value));
		}
	}

	mark_dirty ();
}

ControlList::~ControlList()
{
	for (EventList::iterator x = _events.begin(); x != _events.end(); ++x) {
		delete (*x);
	}
}
	

boost::shared_ptr<ControlList>
ControlList::create(Parameter id)
{
	return boost::shared_ptr<ControlList>(new ControlList(id));
}


bool
ControlList::operator== (const ControlList& other)
{
	return _events == other._events;
}

ControlList&
ControlList::operator= (const ControlList& other)
{
	if (this != &other) {
		
		_events.clear ();
		
		for (const_iterator i = other._events.begin(); i != other._events.end(); ++i) {
			_events.push_back (new ControlEvent (**i));
		}
		
		_min_yval = other._min_yval;
		_max_yval = other._max_yval;
		_max_xval = other._max_xval;
		_default_value = other._default_value;
		
		mark_dirty ();
		maybe_signal_changed ();
	}

	return *this;
}

void
ControlList::maybe_signal_changed ()
{
	mark_dirty ();
	
	if (_frozen)
		_changed_when_thawed = true;
}

void
ControlList::clear ()
{
	{
		Glib::Mutex::Lock lm (_lock);
		_events.clear ();
		mark_dirty ();
	}

	maybe_signal_changed ();
}

void
ControlList::x_scale (double factor)
{
	Glib::Mutex::Lock lm (_lock);
	_x_scale (factor);
}

bool
ControlList::extend_to (double when)
{
	Glib::Mutex::Lock lm (_lock);
	if (_events.empty() || _events.back()->when == when) {
		return false;
	}
	double factor = when / _events.back()->when;
	_x_scale (factor);
	return true;
}

void ControlList::_x_scale (double factor)
{
	for (iterator i = _events.begin(); i != _events.end(); ++i) {
		(*i)->when = floor ((*i)->when * factor);
	}

	mark_dirty ();
}

void
ControlList::reposition_for_rt_add (double when)
{
	_rt_insertion_point = _events.end();
}

void
ControlList::rt_add (double when, double value)
{
	// cerr << "RT: alist @ " << this << " add " << value << " @ " << when << endl;

	{
		Glib::Mutex::Lock lm (_lock);

		iterator where;
		ControlEvent cp (when, 0.0);
		bool done = false;

		if ((_rt_insertion_point != _events.end()) && ((*_rt_insertion_point)->when < when) ) {

			/* we have a previous insertion point, so we should delete
			   everything between it and the position where we are going
			   to insert this point.
			*/

			iterator after = _rt_insertion_point;

			if (++after != _events.end()) {
				iterator far = after;

				while (far != _events.end()) {
					if ((*far)->when > when) {
						break;
					}
					++far;
				}

				if (_new_value) {
					where = far;
					_rt_insertion_point = where;

					if ((*where)->when == when) {
						(*where)->value = value;
						done = true;
					}
				} else {
					where = _events.erase (after, far);
				}

			} else {

				where = after;

			}
			
			iterator previous = _rt_insertion_point;
			--previous;
			
			if (_rt_insertion_point != _events.begin() && (*_rt_insertion_point)->value == value && (*previous)->value == value) {
				(*_rt_insertion_point)->when = when;
				done = true;
				
			}
			
		} else {

			where = lower_bound (_events.begin(), _events.end(), &cp, time_comparator);

			if (where != _events.end()) {
				if ((*where)->when == when) {
					(*where)->value = value;
					done = true;
				}
			}
		}

		if (!done) {
			_rt_insertion_point = _events.insert (where, new ControlEvent (when, value));
		}
		
		_new_value = false;
		mark_dirty ();
	}

	maybe_signal_changed ();
}

void
ControlList::fast_simple_add (double when, double value)
{
	/* to be used only for loading pre-sorted data from saved state */
	_events.insert (_events.end(), new ControlEvent (when, value));
	assert(_events.back());
}

void
ControlList::add (double when, double value)
{
	/* this is for graphical editing */

	{
		Glib::Mutex::Lock lm (_lock);
		ControlEvent cp (when, 0.0f);
		bool insert = true;
		iterator insertion_point;

		for (insertion_point = lower_bound (_events.begin(), _events.end(), &cp, time_comparator); insertion_point != _events.end(); ++insertion_point) {

			/* only one point allowed per time point */

			if ((*insertion_point)->when == when) {
				(*insertion_point)->value = value;
				insert = false;
				break;
			} 

			if ((*insertion_point)->when >= when) {
				break;
			}
		}

		if (insert) {
			
			_events.insert (insertion_point, new ControlEvent (when, value));
			reposition_for_rt_add (0);

		} 

		mark_dirty ();
	}

	maybe_signal_changed ();
}

void
ControlList::erase (iterator i)
{
	{
		Glib::Mutex::Lock lm (_lock);
		_events.erase (i);
		reposition_for_rt_add (0);
		mark_dirty ();
	}
	maybe_signal_changed ();
}

void
ControlList::erase (iterator start, iterator end)
{
	{
		Glib::Mutex::Lock lm (_lock);
		_events.erase (start, end);
		reposition_for_rt_add (0);
		mark_dirty ();
	}
	maybe_signal_changed ();
}	

void
ControlList::reset_range (double start, double endt)
{
	bool reset = false;

	{
        Glib::Mutex::Lock lm (_lock);
		ControlEvent cp (start, 0.0f);
		iterator s;
		iterator e;
		
		if ((s = lower_bound (_events.begin(), _events.end(), &cp, time_comparator)) != _events.end()) {

			cp.when = endt;
			e = upper_bound (_events.begin(), _events.end(), &cp, time_comparator);

			for (iterator i = s; i != e; ++i) {
				(*i)->value = _default_value;
			}
			
			reset = true;

			mark_dirty ();
		}
	}

	if (reset) {
		maybe_signal_changed ();
	}
}

void
ControlList::erase_range (double start, double endt)
{
	bool erased = false;

	{
		Glib::Mutex::Lock lm (_lock);
		ControlEvent cp (start, 0.0f);
		iterator s;
		iterator e;

		if ((s = lower_bound (_events.begin(), _events.end(), &cp, time_comparator)) != _events.end()) {
			cp.when = endt;
			e = upper_bound (_events.begin(), _events.end(), &cp, time_comparator);
			_events.erase (s, e);
			reposition_for_rt_add (0);
			erased = true;
			mark_dirty ();
		}
		
	}

	if (erased) {
		maybe_signal_changed ();
	}
}

void
ControlList::move_range (iterator start, iterator end, double xdelta, double ydelta)
{
	/* note: we assume higher level logic is in place to avoid this
	   reordering the time-order of control events in the list. ie. all
	   points after end are later than (end)->when.
	*/

	{
		Glib::Mutex::Lock lm (_lock);

		while (start != end) {
			(*start)->when += xdelta;
			(*start)->value += ydelta;
			if (isnan ((*start)->value)) {
				abort ();
			}
			++start;
		}

		if (!_frozen) {
			_events.sort (event_time_less_than);
		} else {
			_sort_pending = true;
		}

		mark_dirty ();
	}

	maybe_signal_changed ();
}

void
ControlList::slide (iterator before, double distance)
{
	{
		Glib::Mutex::Lock lm (_lock);

		if (before == _events.end()) {
			return;
		}
		
		while (before != _events.end()) {
			(*before)->when += distance;
			++before;
		}
	}

	maybe_signal_changed ();
}

void
ControlList::modify (iterator iter, double when, double val)
{
	/* note: we assume higher level logic is in place to avoid this
	   reordering the time-order of control events in the list. ie. all
	   points after *iter are later than when.
	*/

	{
		Glib::Mutex::Lock lm (_lock);

		(*iter)->when = when;
		(*iter)->value = val;

		if (isnan (val)) {
			abort ();
		}

		if (!_frozen) {
			_events.sort (event_time_less_than);
		} else {
			_sort_pending = true;
		}

		mark_dirty ();
	}

	maybe_signal_changed ();
}

std::pair<ControlList::iterator,ControlList::iterator>
ControlList::control_points_adjacent (double xval)
{
	Glib::Mutex::Lock lm (_lock);
	iterator i;
	ControlEvent cp (xval, 0.0f);
	std::pair<iterator,iterator> ret;

	ret.first = _events.end();
	ret.second = _events.end();

	for (i = lower_bound (_events.begin(), _events.end(), &cp, time_comparator); i != _events.end(); ++i) {
		
		if (ret.first == _events.end()) {
			if ((*i)->when >= xval) {
				if (i != _events.begin()) {
					ret.first = i;
					--ret.first;
				} else {
					return ret;
				}
			}
		} 
		
		if ((*i)->when > xval) {
			ret.second = i;
			break;
		}
	}

	return ret;
}

void
ControlList::set_max_xval (double x)
{
	_max_xval = x;
}

void
ControlList::freeze ()
{
	_frozen++;
}

void
ControlList::thaw ()
{
	assert(_frozen > 0);

	if (--_frozen > 0) {
		return;
	}

	{
		Glib::Mutex::Lock lm (_lock);

		if (_sort_pending) {
			_events.sort (event_time_less_than);
			_sort_pending = false;
		}
	}
}

void 
ControlList::mark_dirty () const
{
	_lookup_cache.left = -1;
	_search_cache.left = -1;
	if (_curve)
		_curve->mark_dirty();
}

void
ControlList::truncate_end (double last_coordinate)
{
	{
		Glib::Mutex::Lock lm (_lock);
		ControlEvent cp (last_coordinate, 0);
		ControlList::reverse_iterator i;
		double last_val;

		if (_events.empty()) {
			return;
		}

		if (last_coordinate == _events.back()->when) {
			return;
		}

		if (last_coordinate > _events.back()->when) {
			
			/* extending end:
			*/

			iterator foo = _events.begin();
			bool lessthantwo;

			if (foo == _events.end()) {
				lessthantwo = true;
			} else if (++foo == _events.end()) {
				lessthantwo = true;
			} else {
				lessthantwo = false;
			}

			if (lessthantwo) {
				/* less than 2 points: add a new point */
				_events.push_back (new ControlEvent (last_coordinate, _events.back()->value));
			} else {

				/* more than 2 points: check to see if the last 2 values
				   are equal. if so, just move the position of the
				   last point. otherwise, add a new point.
				*/

				iterator penultimate = _events.end();
				--penultimate; /* points at last point */
				--penultimate; /* points at the penultimate point */
				
				if (_events.back()->value == (*penultimate)->value) {
					_events.back()->when = last_coordinate;
				} else {
					_events.push_back (new ControlEvent (last_coordinate, _events.back()->value));
				}
			}

		} else {

			/* shortening end */

			last_val = unlocked_eval (last_coordinate);
			last_val = max ((double) _min_yval, last_val);
			last_val = min ((double) _max_yval, last_val);
			
			i = _events.rbegin();
			
			/* make i point to the last control point */
			
			++i;
			
			/* now go backwards, removing control points that are
			   beyond the new last coordinate.
			*/

			uint32_t sz = _events.size();
			
			while (i != _events.rend() && sz > 2) {
				ControlList::reverse_iterator tmp;
				
				tmp = i;
				++tmp;
				
				if ((*i)->when < last_coordinate) {
					break;
				}
				
				_events.erase (i.base());
				--sz;

				i = tmp;
			}
			
			_events.back()->when = last_coordinate;
			_events.back()->value = last_val;
		}

		reposition_for_rt_add (0);
		mark_dirty();
	}

	maybe_signal_changed ();
}

void
ControlList::truncate_start (double overall_length)
{
	{
		Glib::Mutex::Lock lm (_lock);
		iterator i;
		double first_legal_value;
		double first_legal_coordinate;

		assert(!_events.empty());
		
		if (overall_length == _events.back()->when) {
			/* no change in overall length */
			return;
		}
		
		if (overall_length > _events.back()->when) {
			
			/* growing at front: duplicate first point. shift all others */

			double shift = overall_length - _events.back()->when;
			uint32_t np;

			for (np = 0, i = _events.begin(); i != _events.end(); ++i, ++np) {
				(*i)->when += shift;
			}

			if (np < 2) {

				/* less than 2 points: add a new point */
				_events.push_front (new ControlEvent (0, _events.front()->value));

			} else {

				/* more than 2 points: check to see if the first 2 values
				   are equal. if so, just move the position of the
				   first point. otherwise, add a new point.
				*/

				iterator second = _events.begin();
				++second; /* points at the second point */
				
				if (_events.front()->value == (*second)->value) {
					/* first segment is flat, just move start point back to zero */
					_events.front()->when = 0;
				} else {
					/* leave non-flat segment in place, add a new leading point. */
					_events.push_front (new ControlEvent (0, _events.front()->value));
				}
			}

		} else {

			/* shrinking at front */
			
			first_legal_coordinate = _events.back()->when - overall_length;
			first_legal_value = unlocked_eval (first_legal_coordinate);
			first_legal_value = max (_min_yval, first_legal_value);
			first_legal_value = min (_max_yval, first_legal_value);

			/* remove all events earlier than the new "front" */

			i = _events.begin();
			
			while (i != _events.end() && !_events.empty()) {
				ControlList::iterator tmp;
				
				tmp = i;
				++tmp;
				
				if ((*i)->when > first_legal_coordinate) {
					break;
				}
				
				_events.erase (i);
				
				i = tmp;
			}
			

			/* shift all remaining points left to keep their same
			   relative position
			*/
			
			for (i = _events.begin(); i != _events.end(); ++i) {
				(*i)->when -= first_legal_coordinate;
			}

			/* add a new point for the interpolated new value */
			
			_events.push_front (new ControlEvent (0, first_legal_value));
		}	    

		reposition_for_rt_add (0);

		mark_dirty();
	}

	maybe_signal_changed ();
}

double
ControlList::unlocked_eval (double x) const
{
	pair<EventList::iterator,EventList::iterator> range;
	int32_t npoints;
	double lpos, upos;
	double lval, uval;
	double fraction;

	npoints = _events.size();

	switch (npoints) {
	case 0:
		return _default_value;

	case 1:
		if (x >= _events.front()->when) {
			return _events.front()->value;
		} else {
			// return _default_value;
			return _events.front()->value;
		} 
		
	case 2:
		if (x >= _events.back()->when) {
			return _events.back()->value;
		} else if (x == _events.front()->when) {
			return _events.front()->value;
 		} else if (x < _events.front()->when) {
			// return _default_value;
			return _events.front()->value;
		}

		lpos = _events.front()->when;
		lval = _events.front()->value;
		upos = _events.back()->when;
		uval = _events.back()->value;
		
		if (_interpolation == Discrete)
			return lval;

		/* linear interpolation betweeen the two points
		*/

		fraction = (double) (x - lpos) / (double) (upos - lpos);
		return lval + (fraction * (uval - lval));

	default:

		if (x >= _events.back()->when) {
			return _events.back()->value;
		} else if (x == _events.front()->when) {
			return _events.front()->value;
 		} else if (x < _events.front()->when) {
			// return _default_value;
			return _events.front()->value;
		}

		return multipoint_eval (x);
		break;
	}

	/*NOTREACHED*/ /* stupid gcc */
	return 0.0;
}

double
ControlList::multipoint_eval (double x) const
{
	double upos, lpos;
	double uval, lval;
	double fraction;
	
	/* "Stepped" lookup (no interpolation) */
	/* FIXME: no cache.  significant? */
	if (_interpolation == Discrete) {
		const ControlEvent cp (x, 0);
		EventList::const_iterator i = lower_bound (_events.begin(), _events.end(), &cp, time_comparator);

		// shouldn't have made it to multipoint_eval
		assert(i != _events.end());

		if (i == _events.begin() || (*i)->when == x)
			return (*i)->value;
		else
			return (*(--i))->value;
	}

	/* Only do the range lookup if x is in a different range than last time
	 * this was called (or if the lookup cache has been marked "dirty" (left<0) */
	if ((_lookup_cache.left < 0) ||
	    ((_lookup_cache.left > x) || 
	     (_lookup_cache.range.first == _events.end()) || 
	     ((*_lookup_cache.range.second)->when < x))) {

		const ControlEvent cp (x, 0);
		
		_lookup_cache.range = equal_range (_events.begin(), _events.end(), &cp, time_comparator);
	}
	
	pair<const_iterator,const_iterator> range = _lookup_cache.range;

	if (range.first == range.second) {

		/* x does not exist within the list as a control point */

		_lookup_cache.left = x;

		if (range.first != _events.begin()) {
			--range.first;
			lpos = (*range.first)->when;
			lval = (*range.first)->value;
		}  else {
			/* we're before the first point */
			// return _default_value;
			return _events.front()->value;
		}
		
		if (range.second == _events.end()) {
			/* we're after the last point */
			return _events.back()->value;
		}

		upos = (*range.second)->when;
		uval = (*range.second)->value;
		
		/* linear interpolation betweeen the two points
		   on either side of x
		*/

		fraction = (double) (x - lpos) / (double) (upos - lpos);
		return lval + (fraction * (uval - lval));

	} 

	/* x is a control point in the data */
	_lookup_cache.left = -1;
	return (*range.first)->value;
}

void
ControlList::build_search_cache_if_necessary(double start, double end) const
{
	/* Only do the range lookup if x is in a different range than last time
	 * this was called (or if the search cache has been marked "dirty" (left<0) */
	if (!_events.empty() && ((_search_cache.left < 0) ||
			((_search_cache.left > start) ||
			 (_search_cache.right < end)))) {

		const ControlEvent start_point (start, 0);
		const ControlEvent end_point (end, 0);

		//cerr << "REBUILD: (" << _search_cache.left << ".." << _search_cache.right << ") := ("
		//	<< start << ".." << end << ")" << endl;

		_search_cache.range.first = lower_bound (_events.begin(), _events.end(), &start_point, time_comparator);
		_search_cache.range.second = upper_bound (_events.begin(), _events.end(), &end_point, time_comparator);

		_search_cache.left = start;
		_search_cache.right = end;
	}
}

/** Get the earliest event between \a start and \a end, using the current interpolation style.
 *
 * If an event is found, \a x and \a y are set to its coordinates.
 *
 * \param inclusive Include events with timestamp exactly equal to \a start
 * \return true if event is found (and \a x and \a y are valid).
 */
bool
ControlList::rt_safe_earliest_event(double start, double end, double& x, double& y, bool inclusive) const
{
	// FIXME: It would be nice if this was unnecessary..
	Glib::Mutex::Lock lm(_lock, Glib::TRY_LOCK);
	if (!lm.locked()) {
		return false;
	}

	return rt_safe_earliest_event_unlocked(start, end, x, y, inclusive);
} 


/** Get the earliest event between \a start and \a end, using the current interpolation style.
 *
 * If an event is found, \a x and \a y are set to its coordinates.
 *
 * \param inclusive Include events with timestamp exactly equal to \a start
 * \return true if event is found (and \a x and \a y are valid).
 */
bool
ControlList::rt_safe_earliest_event_unlocked(double start, double end, double& x, double& y, bool inclusive) const
{
	if (_interpolation == Discrete)
		return rt_safe_earliest_event_discrete_unlocked(start, end, x, y, inclusive);
	else
		return rt_safe_earliest_event_linear_unlocked(start, end, x, y, inclusive);
} 


/** Get the earliest event between \a start and \a end (Discrete (lack of) interpolation)
 *
 * If an event is found, \a x and \a y are set to its coordinates.
 *
 * \param inclusive Include events with timestamp exactly equal to \a start
 * \return true if event is found (and \a x and \a y are valid).
 */
bool
ControlList::rt_safe_earliest_event_discrete_unlocked (double start, double end, double& x, double& y, bool inclusive) const
{
	build_search_cache_if_necessary(start, end);

	const pair<const_iterator,const_iterator>& range = _search_cache.range;

	if (range.first != _events.end()) {
		const ControlEvent* const first = *range.first;

		const bool past_start = (inclusive ? first->when >= start : first->when > start);

		/* Earliest points is in range, return it */
		if (past_start >= start && first->when < end) {

			x = first->when;
			y = first->value;

			/* Move left of cache to this point
			 * (Optimize for immediate call this cycle within range) */
			_search_cache.left = x;
			++_search_cache.range.first;

			assert(x >= start);
			assert(x < end);
			return true;

		} else {
			return false;
		}
	
	/* No points in range */
	} else {
		return false;
	}
}

/** Get the earliest time the line crosses an integer (Linear interpolation).
 *
 * If an event is found, \a x and \a y are set to its coordinates.
 *
 * \param inclusive Include events with timestamp exactly equal to \a start
 * \return true if event is found (and \a x and \a y are valid).
 */
bool
ControlList::rt_safe_earliest_event_linear_unlocked (double start, double end, double& x, double& y, bool inclusive) const
{
	//cerr << "earliest_event(" << start << ", " << end << ", " << x << ", " << y << ", " << inclusive << endl;

	if (_events.size() == 0)
		return false;
	else if (_events.size() == 1)
		return rt_safe_earliest_event_discrete_unlocked(start, end, x, y, inclusive);

	// Hack to avoid infinitely repeating the same event
	build_search_cache_if_necessary(start, end);
	
	pair<const_iterator,const_iterator> range = _search_cache.range;

	if (range.first != _events.end()) {

		const ControlEvent* first = NULL;
		const ControlEvent* next = NULL;

		/* Step is after first */
		if (range.first == _events.begin() || (*range.first)->when == start) {
			first = *range.first;
			next = *(++range.first);
			++_search_cache.range.first;

		/* Step is before first */
		} else {
			const_iterator prev = range.first;
			--prev;
			first = *prev;
			next = *range.first;
		}
		
		if (inclusive && first->when == start) {
			x = first->when;
			y = first->value;
			/* Move left of cache to this point
			 * (Optimize for immediate call this cycle within range) */
			_search_cache.left = x;
			//++_search_cache.range.first;
			return true;
		}
			
		if (abs(first->value - next->value) <= 1) {
			if (next->when <= end && (!inclusive || next->when > start)) {
				x = next->when;
				y = next->value;
				/* Move left of cache to this point
				 * (Optimize for immediate call this cycle within range) */
				_search_cache.left = x;
				//++_search_cache.range.first;
				return true;
			} else {
				return false;
			}
		}

		const double slope = (next->value - first->value) / (double)(next->when - first->when);
		//cerr << "start y: " << start_y << endl;

		//y = first->value + (slope * fabs(start - first->when));
		y = first->value;

		if (first->value < next->value) // ramping up
			y = ceil(y);
		else // ramping down
			y = floor(y);

		x = first->when + (y - first->value) / (double)slope;
		
		while ((inclusive && x < start) || (x <= start && y != next->value)) {
			
			if (first->value < next->value) // ramping up
				y += 1.0;
			else // ramping down
				y -= 1.0;

			x = first->when + (y - first->value) / (double)slope;
		}

		/*cerr << first->value << " @ " << first->when << " ... "
				<< next->value << " @ " << next->when
				<< " = " << y << " @ " << x << endl;*/

		assert(    (y >= first->value && y <= next->value)
				|| (y <= first->value && y >= next->value) );

		
		const bool past_start = (inclusive ? x >= start : x > start);
		if (past_start && x < end) {
			/* Move left of cache to this point
			 * (Optimize for immediate call this cycle within range) */
			_search_cache.left = x;

			return true;

		} else {
			return false;
		}
	
	/* No points in the future, so no steps (towards them) in the future */
	} else {
		return false;
	}
}

boost::shared_ptr<ControlList>
ControlList::cut (iterator start, iterator end)
{
	boost::shared_ptr<ControlList> nal = create (_parameter);

	{
		Glib::Mutex::Lock lm (_lock);

		for (iterator x = start; x != end; ) {
			iterator tmp;
			
			tmp = x;
			++tmp;
			
			nal->_events.push_back (new ControlEvent (**x));
			_events.erase (x);
			
			reposition_for_rt_add (0);

			x = tmp;
		}

		mark_dirty ();
	}

	maybe_signal_changed ();

	return nal;
}

boost::shared_ptr<ControlList>
ControlList::cut_copy_clear (double start, double end, int op)
{
	boost::shared_ptr<ControlList> nal = create (_parameter);
	iterator s, e;
	ControlEvent cp (start, 0.0);
	bool changed = false;
	
	{
		Glib::Mutex::Lock lm (_lock);

		if ((s = lower_bound (_events.begin(), _events.end(), &cp, time_comparator)) == _events.end()) {
			return nal;
		}

		cp.when = end;
		e = upper_bound (_events.begin(), _events.end(), &cp, time_comparator);

		if (op != 2 && (*s)->when != start) {
			nal->_events.push_back (new ControlEvent (0, unlocked_eval (start)));
		}

		for (iterator x = s; x != e; ) {
			iterator tmp;
			
			tmp = x;
			++tmp;

			changed = true;
			
			/* adjust new points to be relative to start, which
			   has been set to zero.
			*/
			
			if (op != 2) {
				nal->_events.push_back (new ControlEvent ((*x)->when - start, (*x)->value));
			}

			if (op != 1) {
				_events.erase (x);
			}
			
			x = tmp;
		}

		if (op != 2 && nal->_events.back()->when != end - start) {
			nal->_events.push_back (new ControlEvent (end - start, unlocked_eval (end)));
		}

		if (changed) {
			reposition_for_rt_add (0);
		}

		mark_dirty ();
	}

	maybe_signal_changed ();

	return nal;

}

boost::shared_ptr<ControlList>
ControlList::copy (iterator start, iterator end)
{
	boost::shared_ptr<ControlList> nal = create (_parameter);

	{
		Glib::Mutex::Lock lm (_lock);
		
		for (iterator x = start; x != end; ) {
			iterator tmp;
			
			tmp = x;
			++tmp;
			
			nal->_events.push_back (new ControlEvent (**x));
			
			x = tmp;
		}
	}

	return nal;
}

boost::shared_ptr<ControlList>
ControlList::cut (double start, double end)
{
	return cut_copy_clear (start, end, 0);
}

boost::shared_ptr<ControlList>
ControlList::copy (double start, double end)
{
	return cut_copy_clear (start, end, 1);
}

void
ControlList::clear (double start, double end)
{
	(void) cut_copy_clear (start, end, 2);
}

bool
ControlList::paste (ControlList& alist, double pos, float times)
{
	if (alist._events.empty()) {
		return false;
	}

	{
		Glib::Mutex::Lock lm (_lock);
		iterator where;
		iterator prev;
		double end = 0;
		ControlEvent cp (pos, 0.0);

		where = upper_bound (_events.begin(), _events.end(), &cp, time_comparator);

		for (iterator i = alist.begin();i != alist.end(); ++i) {
			_events.insert (where, new ControlEvent( (*i)->when+pos,( *i)->value));
			end = (*i)->when + pos;
		}
	
	
		/* move all  points after the insertion along the timeline by 
		   the correct amount.
		*/

		while (where != _events.end()) {
			iterator tmp;
			if ((*where)->when <= end) {
				tmp = where;
				++tmp;
				_events.erase(where);
				where = tmp;

			} else {
				break;
			}
		}

		reposition_for_rt_add (0);
		mark_dirty ();
	}

	maybe_signal_changed ();
	return true;
}

} // namespace Evoral

