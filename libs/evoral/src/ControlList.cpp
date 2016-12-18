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

#include <cmath>

#ifdef COMPILER_MSVC
#include <float.h>

// 'std::isnan()' is not available in MSVC.
#define isnan_local(val) (bool)_isnan((double)val)
#else
#define isnan_local std::isnan
#endif

#include <cassert>
#include <cmath>
#include <iostream>
#include <utility>

#include "evoral/ControlList.hpp"
#include "evoral/Curve.hpp"
#include "evoral/ParameterDescriptor.hpp"
#include "evoral/TypeMap.hpp"
#include "evoral/types.hpp"

#include "pbd/compose.h"
#include "pbd/debug.h"

using namespace std;
using namespace PBD;

namespace Evoral {

inline bool event_time_less_than (ControlEvent* a, ControlEvent* b)
{
	return a->when < b->when;
}

ControlList::ControlList (const Parameter& id, const ParameterDescriptor& desc)
	: _parameter(id)
	, _desc(desc)
	, _curve(0)
{
	_interpolation = desc.toggled ? Discrete : Linear;
	_frozen = 0;
	_changed_when_thawed = false;
	_min_yval = desc.lower;
	_max_yval = desc.upper;
	_default_value = desc.normal;
	_lookup_cache.left = -1;
	_lookup_cache.range.first = _events.end();
	_lookup_cache.range.second = _events.end();
	_search_cache.left = -1;
	_search_cache.first = _events.end();
	_sort_pending = false;
	new_write_pass = true;
	_in_write_pass = false;
	did_write_during_pass = false;
	insert_position = -1;
	most_recent_insert_iterator = _events.end();
}

ControlList::ControlList (const ControlList& other)
	: _parameter(other._parameter)
	, _desc(other._desc)
	, _interpolation(other._interpolation)
	, _curve(0)
{
	_frozen = 0;
	_changed_when_thawed = false;
	_min_yval = other._min_yval;
	_max_yval = other._max_yval;
	_default_value = other._default_value;
	_lookup_cache.range.first = _events.end();
	_lookup_cache.range.second = _events.end();
	_search_cache.first = _events.end();
	_sort_pending = false;
	new_write_pass = true;
	_in_write_pass = false;
	did_write_during_pass = false;
	insert_position = -1;
	most_recent_insert_iterator = _events.end();

	copy_events (other);

	mark_dirty ();
}

ControlList::ControlList (const ControlList& other, double start, double end)
	: _parameter(other._parameter)
	, _desc(other._desc)
	, _interpolation(other._interpolation)
	, _curve(0)
{
	_frozen = 0;
	_changed_when_thawed = false;
	_min_yval = other._min_yval;
	_max_yval = other._max_yval;
	_default_value = other._default_value;
	_lookup_cache.range.first = _events.end();
	_lookup_cache.range.second = _events.end();
	_search_cache.first = _events.end();
	_sort_pending = false;

	/* now grab the relevant points, and shift them back if necessary */

	boost::shared_ptr<ControlList> section = const_cast<ControlList*>(&other)->copy (start, end);

	if (!section->empty()) {
		copy_events (*(section.get()));
	}

	new_write_pass = false;
	_in_write_pass = false;
	did_write_during_pass = false;
	insert_position = -1;
	most_recent_insert_iterator = _events.end();

	mark_dirty ();
}

ControlList::~ControlList()
{
	for (EventList::iterator x = _events.begin(); x != _events.end(); ++x) {
		delete (*x);
	}
	_events.clear ();

	delete _curve;
}

boost::shared_ptr<ControlList>
ControlList::create(const Parameter& id, const ParameterDescriptor& desc)
{
	return boost::shared_ptr<ControlList>(new ControlList(id, desc));
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

		_min_yval = other._min_yval;
		_max_yval = other._max_yval;


		_interpolation = other._interpolation;
		_default_value = other._default_value;

		copy_events (other);
	}

	return *this;
}

void
ControlList::copy_events (const ControlList& other)
{
	{
		Glib::Threads::RWLock::WriterLock lm (_lock);
		for (EventList::iterator x = _events.begin(); x != _events.end(); ++x) {
			delete (*x);
		}
		_events.clear ();
		for (const_iterator i = other.begin(); i != other.end(); ++i) {
			_events.push_back (new ControlEvent ((*i)->when, (*i)->value));
		}
		unlocked_invalidate_insert_iterator ();
		mark_dirty ();
	}
	maybe_signal_changed ();
}

void
ControlList::create_curve()
{
	_curve = new Curve(*this);
}

void
ControlList::destroy_curve()
{
	delete _curve;
	_curve = NULL;
}

void
ControlList::maybe_signal_changed ()
{
	mark_dirty ();

	if (_frozen) {
		_changed_when_thawed = true;
	}
}

void
ControlList::clear ()
{
	{
		Glib::Threads::RWLock::WriterLock lm (_lock);
		for (EventList::iterator x = _events.begin(); x != _events.end(); ++x) {
			delete (*x);
		}
		_events.clear ();
		unlocked_invalidate_insert_iterator ();
		mark_dirty ();
	}

	maybe_signal_changed ();
}

void
ControlList::x_scale (double factor)
{
	Glib::Threads::RWLock::WriterLock lm (_lock);
	_x_scale (factor);
}

bool
ControlList::extend_to (double when)
{
	Glib::Threads::RWLock::WriterLock lm (_lock);
	if (_events.empty() || _events.back()->when == when) {
		return false;
	}
	double factor = when / _events.back()->when;
	_x_scale (factor);
	return true;
}

void
ControlList::_x_scale (double factor)
{
	for (iterator i = _events.begin(); i != _events.end(); ++i) {
		(*i)->when *= factor;
	}

	mark_dirty ();
}

struct ControlEventTimeComparator {
	bool operator() (ControlEvent* a, ControlEvent* b) {
		return a->when < b->when;
	}
};

void
ControlList::thin (double thinning_factor)
{
	if (thinning_factor == 0.0 || _desc.toggled) {
		return;
	}

	bool changed = false;

	{
		Glib::Threads::RWLock::WriterLock lm (_lock);

		ControlEvent* prevprev = 0;
		ControlEvent* cur = 0;
		ControlEvent* prev = 0;
		iterator pprev;
		int counter = 0;

		DEBUG_TRACE (DEBUG::ControlList, string_compose ("@%1 thin from %2 events\n", this, _events.size()));

		for (iterator i = _events.begin(); i != _events.end(); ++i) {

			cur = *i;
			counter++;

			if (counter > 2) {

				/* compute the area of the triangle formed by 3 points
				 */

				double area = fabs ((prevprev->when * (prev->value - cur->value)) +
						    (prev->when * (cur->value - prevprev->value)) +
						    (cur->when * (prevprev->value - prev->value)));

				if (area < thinning_factor) {
					iterator tmp = pprev;

					/* pprev will change to current
					   i is incremented to the next event
					   as we loop.
					*/

					pprev = i;
					_events.erase (tmp);
					changed = true;
					continue;
				}
			}

			prevprev = prev;
			prev = cur;
			pprev = i;
		}

		DEBUG_TRACE (DEBUG::ControlList, string_compose ("@%1 thin => %2 events\n", this, _events.size()));

		if (changed) {
			unlocked_invalidate_insert_iterator ();
			mark_dirty ();
		}
	}

	if (changed) {
		maybe_signal_changed ();
	}
}

void
ControlList::fast_simple_add (double when, double value)
{
	Glib::Threads::RWLock::WriterLock lm (_lock);
	/* to be used only for loading pre-sorted data from saved state */
	_events.insert (_events.end(), new ControlEvent (when, value));

	mark_dirty ();
}

void
ControlList::invalidate_insert_iterator ()
{
	Glib::Threads::RWLock::WriterLock lm (_lock);
	unlocked_invalidate_insert_iterator ();
}

void
ControlList::unlocked_invalidate_insert_iterator ()
{
	most_recent_insert_iterator = _events.end();
}

void
ControlList::start_write_pass (double when)
{
	Glib::Threads::RWLock::WriterLock lm (_lock);

	DEBUG_TRACE (DEBUG::ControlList, string_compose ("%1: setup write pass @ %2\n", this, when));

	new_write_pass = true;
	did_write_during_pass = false;
	insert_position = when;

	/* leave the insert iterator invalid, so that we will do the lookup
	   of where it should be in a "lazy" way - deferring it until
	   we actually add the first point (which may never happen).
	*/

	unlocked_invalidate_insert_iterator ();
}

void
ControlList::write_pass_finished (double /*when*/, double thinning_factor)
{
	DEBUG_TRACE (DEBUG::ControlList, "write pass finished\n");

	if (did_write_during_pass) {
		thin (thinning_factor);
		did_write_during_pass = false;
	}
	new_write_pass = true;
	_in_write_pass = false;
}

void
ControlList::set_in_write_pass (bool yn, bool add_point, double when)
{
	DEBUG_TRACE (DEBUG::ControlList, string_compose ("now in write pass @ %1, add point ? %2\n", when, add_point));

	_in_write_pass = yn;

	if (yn && add_point) {
		add_guard_point (when);
	}
}

void
ControlList::add_guard_point (double when)
{
	ControlEvent cp (when, 0.0);
	most_recent_insert_iterator = lower_bound (_events.begin(), _events.end(), &cp, time_comparator);

	double eval_value = unlocked_eval (insert_position);

	if (most_recent_insert_iterator == _events.end()) {

		DEBUG_TRACE (DEBUG::ControlList, string_compose ("@%1 insert iterator at end, adding eval-value there %2\n", this, eval_value));
		_events.push_back (new ControlEvent (when, eval_value));
		/* leave insert iterator at the end */

	} else if ((*most_recent_insert_iterator)->when == when) {

		DEBUG_TRACE (DEBUG::ControlList, string_compose ("@%1 insert iterator at existing point, setting eval-value there %2\n", this, eval_value));

		/* most_recent_insert_iterator points to a control event
		   already at the insert position, so there is
		   nothing to do.

		   ... except ...

		   advance most_recent_insert_iterator so that the "real"
		   insert occurs in the right place, since it
		   points to the control event just inserted.
		*/

		++most_recent_insert_iterator;
	} else {

		/* insert a new control event at the right spot
		 */

		DEBUG_TRACE (DEBUG::ControlList, string_compose ("@%1 insert eval-value %2 just before iterator @ %3\n",
								 this, eval_value, (*most_recent_insert_iterator)->when));

		most_recent_insert_iterator = _events.insert (most_recent_insert_iterator, new ControlEvent (when, eval_value));

		/* advance most_recent_insert_iterator so that the "real"
		 * insert occurs in the right place, since it
		 * points to the control event just inserted.
		 */

		++most_recent_insert_iterator;
	}

	/* don't do this again till the next write pass */

	new_write_pass = false;
}

bool
ControlList::in_write_pass () const
{
	return _in_write_pass;
}

bool
ControlList::editor_add (double when, double value, bool with_guard)
{
	/* this is for making changes from a graphical line editor
	*/

	ControlEvent cp (when, 0.0f);
	iterator i = lower_bound (_events.begin(), _events.end(), &cp, time_comparator);

	if (i != _events.end () && (*i)->when == when) {
		return false;
	}

	if (_events.empty()) {

		/* as long as the point we're adding is not at zero,
		 * add an "anchor" point there.
		 */

		if (when >= 1) {
			_events.insert (_events.end(), new ControlEvent (0, value));
			DEBUG_TRACE (DEBUG::ControlList, string_compose ("@%1 added value %2 at zero\n", this, value));
		}
	}

	insert_position = when;
	if (with_guard) {
		if (when > 64) {
			add_guard_point (when - 64);
		}
		maybe_add_insert_guard (when);
	}

	iterator result;
	DEBUG_TRACE (DEBUG::ControlList, string_compose ("editor_add: actually add when= %1 value= %2\n", when, value));
	result = _events.insert (i, new ControlEvent (when, value));

	if (i == result) {
		return false;
	}

	mark_dirty ();
	maybe_signal_changed ();

	return true;
}

void
ControlList::maybe_add_insert_guard (double when)
{
	if (most_recent_insert_iterator != _events.end()) {
		if ((*most_recent_insert_iterator)->when - when > 64) {
			/* Next control point is some distance from where our new point is
			   going to go, so add a new point to avoid changing the shape of
			   the line too much.  The insert iterator needs to point to the
			   new control point so that our insert will happen correctly. */
			most_recent_insert_iterator = _events.insert (
				most_recent_insert_iterator,
				new ControlEvent (when + 64, (*most_recent_insert_iterator)->value));
			DEBUG_TRACE (DEBUG::ControlList, string_compose ("@%1 added insert guard point @ %2 = %3\n",
			                                                 this, when + 64,
			                                                 (*most_recent_insert_iterator)->value));
		}
	}
}

/** If we would just be adding to a straight line, move the previous point instead. */
bool
ControlList::maybe_insert_straight_line (double when, double value)
{
	if (_events.empty()) {
		return false;
	}

	if (_events.back()->value == value) {
		// Point b at the final point, which we know exists
		EventList::iterator b = _events.end();
		--b;
		if (b == _events.begin()) {
			return false;  // No previous point
		}

		// Check the previous point's value
		--b;
		if ((*b)->value == value) {
			/* At least two points with the exact same value (straight
			   line), just move the final point to the new time. */
			_events.back()->when = when;
			DEBUG_TRACE (DEBUG::ControlList, string_compose ("final value of %1 moved to %2\n", value, when));
			return true;
		}
	}
	return false;
}

ControlList::iterator
ControlList::erase_from_iterator_to (iterator iter, double when)
{
	while (iter != _events.end()) {
		if ((*iter)->when < when) {
			DEBUG_TRACE (DEBUG::ControlList, string_compose ("@%1 erase existing @ %2\n", this, (*iter)->when));
			delete *iter;
			iter = _events.erase (iter);
			continue;
		} else if ((*iter)->when >= when) {
			break;
		}
		++iter;
	}
	return iter;
}

void
ControlList::add (double when, double value, bool with_guards, bool with_initial)
{
	/* this is for making changes from some kind of user interface or
	   control surface (GUI, MIDI, OSC etc)
	*/

	DEBUG_TRACE (DEBUG::ControlList,
	             string_compose ("@%1 add %2 at %3 guards = %4 write pass = %5 (new? %6) at end? %7\n",
	                             this, value, when, with_guards, _in_write_pass, new_write_pass,
	                             (most_recent_insert_iterator == _events.end())));
	{
		Glib::Threads::RWLock::WriterLock lm (_lock);
		ControlEvent cp (when, 0.0f);
		iterator insertion_point;

		if (_events.empty() && with_initial) {

			/* empty: add an "anchor" point if the point we're adding past time 0 */

			if (when >= 1) {
				if (_desc.toggled) {
					const double opp_val = ((value < 0.5) ? 1.0 : 0.0);
					_events.insert (_events.end(), new ControlEvent (0, opp_val));
					DEBUG_TRACE (DEBUG::ControlList, string_compose ("@%1 added toggled value %2 at zero\n", this, opp_val));

				} else {
					_events.insert (_events.end(), new ControlEvent (0, value));
					DEBUG_TRACE (DEBUG::ControlList, string_compose ("@%1 added default value %2 at zero\n", this, _default_value));
				}
			}
		}

		if (_in_write_pass && new_write_pass) {

			/* first write in a write pass: add guard point if requested */

			if (with_guards) {
				add_guard_point (insert_position);
				did_write_during_pass = true;
			} else {
				/* not adding a guard, but we need to set iterator appropriately */
				const ControlEvent cp (when, 0.0);
				most_recent_insert_iterator = lower_bound (_events.begin(), _events.end(), &cp, time_comparator);
			}
			new_write_pass = false;

		} else if (_in_write_pass &&
		           (most_recent_insert_iterator == _events.end() || when > (*most_recent_insert_iterator)->when)) {

			/* in write pass: erase from most recent insert to now */

			if (most_recent_insert_iterator != _events.end()) {
				/* advance to avoid deleting the last inserted point itself. */
				++most_recent_insert_iterator;
			}

			most_recent_insert_iterator = erase_from_iterator_to(most_recent_insert_iterator, when);
			if (with_guards) {
				maybe_add_insert_guard (when);
			}

		} else if (!_in_write_pass) {

			/* not in a write pass: figure out the iterator we should insert in front of */

			DEBUG_TRACE (DEBUG::ControlList, string_compose ("compute(b) MRI for position %1\n", when));
			ControlEvent cp (when, 0.0f);
			most_recent_insert_iterator = lower_bound (_events.begin(), _events.end(), &cp, time_comparator);
		}

		/* OK, now we're really ready to add a new point */

		if (most_recent_insert_iterator == _events.end()) {
			DEBUG_TRACE (DEBUG::ControlList, string_compose ("@%1 appending new point at end\n", this));

			const bool done = maybe_insert_straight_line (when, value);
			if (!done) {
				_events.push_back (new ControlEvent (when, value));
				DEBUG_TRACE (DEBUG::ControlList, string_compose ("\tactually appended, size now %1\n", _events.size()));
			}

			most_recent_insert_iterator = _events.end();
			--most_recent_insert_iterator;

		} else if ((*most_recent_insert_iterator)->when == when) {

			if ((*most_recent_insert_iterator)->value != value) {
				DEBUG_TRACE (DEBUG::ControlList, string_compose ("@%1 reset existing point to new value %2\n", this, value));

				/* only one point allowed per time point, so add a guard point
				 * before it if needed then reset the value of the point.
				 */

				(*most_recent_insert_iterator)->value = value;

				/* if we modified the final value, then its as
				 * if we inserted a new point as far as the
				 * next addition, so make sure we know that.
				 */

				if (_events.back()->when == when) {
					most_recent_insert_iterator = _events.end();
				}

			} else {
				DEBUG_TRACE (DEBUG::ControlList, string_compose ("@%1 same time %2, same value value %3\n", this, when, value));
			}

		} else {
			DEBUG_TRACE (DEBUG::ControlList, string_compose ("@%1 insert new point at %2 at iterator at %3\n", this, when, (*most_recent_insert_iterator)->when));
			bool done = false;
			/* check for possible straight line here until maybe_insert_straight_line () handles the insert iterator properly*/
			if (most_recent_insert_iterator != _events.begin ()) {
				bool have_point2 = false;
				--most_recent_insert_iterator;
				const bool have_point1 = (*most_recent_insert_iterator)->value == value;

				if (most_recent_insert_iterator != _events.begin ()) {
					--most_recent_insert_iterator;
					have_point2 = (*most_recent_insert_iterator)->value == value;
					++most_recent_insert_iterator;
				}

				if (have_point1 && have_point2) {
					(*most_recent_insert_iterator)->when = when;
					done = true;
				} else {
					++most_recent_insert_iterator;
				}
			}
			//done = maybe_insert_straight_line (when, value) || done;
			/* if the transport is stopped, add guard points (?) */
			if (!done && !_in_write_pass && when > 64) {
				add_guard_point (when - 64);
				maybe_add_insert_guard (when);
			}

			if (with_guards) {
				maybe_add_insert_guard (when);
			}

			if (!done) {
				EventList::iterator x = _events.insert (most_recent_insert_iterator, new ControlEvent (when, value));
				DEBUG_TRACE (DEBUG::ControlList, string_compose ("@%1 inserted new value before MRI, size now %2\n", this, _events.size()));
				most_recent_insert_iterator = x;
			}
		}

		mark_dirty ();
	}

	maybe_signal_changed ();
}

void
ControlList::erase (iterator i)
{
	{
		Glib::Threads::RWLock::WriterLock lm (_lock);
		if (most_recent_insert_iterator == i) {
			unlocked_invalidate_insert_iterator ();
		}
		_events.erase (i);
		mark_dirty ();
	}
	maybe_signal_changed ();
}

void
ControlList::erase (iterator start, iterator end)
{
	{
		Glib::Threads::RWLock::WriterLock lm (_lock);
		_events.erase (start, end);
		unlocked_invalidate_insert_iterator ();
		mark_dirty ();
	}
	maybe_signal_changed ();
}

/** Erase the first event which matches the given time and value */
void
ControlList::erase (double when, double value)
{
	{
		Glib::Threads::RWLock::WriterLock lm (_lock);

		iterator i = begin ();
		while (i != end() && ((*i)->when != when || (*i)->value != value)) {
			++i;
		}

		if (i != end ()) {
			_events.erase (i);
			if (most_recent_insert_iterator == i) {
				unlocked_invalidate_insert_iterator ();
			}
		}

		mark_dirty ();
	}

	maybe_signal_changed ();
}

void
ControlList::erase_range (double start, double endt)
{
	bool erased = false;

	{
		Glib::Threads::RWLock::WriterLock lm (_lock);
		erased = erase_range_internal (start, endt, _events);

		if (erased) {
			mark_dirty ();
		}

	}

	if (erased) {
		maybe_signal_changed ();
	}
}

bool
ControlList::erase_range_internal (double start, double endt, EventList & events)
{
	bool erased = false;
	ControlEvent cp (start, 0.0f);
	iterator s;
	iterator e;

	if ((s = lower_bound (events.begin(), events.end(), &cp, time_comparator)) != events.end()) {
		cp.when = endt;
		e = upper_bound (events.begin(), events.end(), &cp, time_comparator);
		events.erase (s, e);
		if (s != e) {
			unlocked_invalidate_insert_iterator ();
			erased = true;
		}
	}

	return erased;
}

void
ControlList::slide (iterator before, double distance)
{
	{
		Glib::Threads::RWLock::WriterLock lm (_lock);

		if (before == _events.end()) {
			return;
		}

		while (before != _events.end()) {
			(*before)->when += distance;
			++before;
		}

		mark_dirty ();
	}

	maybe_signal_changed ();
}

void
ControlList::shift (double pos, double frames)
{
	{
		Glib::Threads::RWLock::WriterLock lm (_lock);

		for (iterator i = _events.begin(); i != _events.end(); ++i) {
			if ((*i)->when >= pos) {
				(*i)->when += frames;
			}
		}

		mark_dirty ();
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
		Glib::Threads::RWLock::WriterLock lm (_lock);

		(*iter)->when = when;
		(*iter)->value = val;
		if (isnan_local (val)) {
			abort ();
		}

		if (!_frozen) {
			_events.sort (event_time_less_than);
			unlocked_invalidate_insert_iterator ();
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
	Glib::Threads::RWLock::ReaderLock lm (_lock);
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
		Glib::Threads::RWLock::WriterLock lm (_lock);

		if (_sort_pending) {
			_events.sort (event_time_less_than);
			unlocked_invalidate_insert_iterator ();
			_sort_pending = false;
		}
	}
}

void
ControlList::mark_dirty () const
{
	_lookup_cache.left = -1;
	_lookup_cache.range.first = _events.end();
	_lookup_cache.range.second = _events.end();
	_search_cache.left = -1;
	_search_cache.first = _events.end();

	if (_curve) {
		_curve->mark_dirty();
	}

	Dirty (); /* EMIT SIGNAL */
}

void
ControlList::truncate_end (double last_coordinate)
{
	{
		Glib::Threads::RWLock::WriterLock lm (_lock);
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

			// FIXME: SLOW! (size() == O(n))

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

		unlocked_invalidate_insert_iterator ();
		mark_dirty();
	}

	maybe_signal_changed ();
}

void
ControlList::truncate_start (double overall_length)
{
	{
		Glib::Threads::RWLock::WriterLock lm (_lock);
		iterator i;
		double first_legal_value;
		double first_legal_coordinate;

		if (_events.empty()) {
			/* nothing to truncate */
			return;
		} else if (overall_length == _events.back()->when) {
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

		unlocked_invalidate_insert_iterator ();
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

	const_iterator length_check_iter = _events.begin();
	for (npoints = 0; npoints < 4; ++npoints, ++length_check_iter) {
		if (length_check_iter == _events.end()) {
			break;
		}
	}

	switch (npoints) {
	case 0:
		return _default_value;

	case 1:
		return _events.front()->value;

	case 2:
		if (x >= _events.back()->when) {
			return _events.back()->value;
		} else if (x <= _events.front()->when) {
			return _events.front()->value;
		}

		lpos = _events.front()->when;
		lval = _events.front()->value;
		upos = _events.back()->when;
		uval = _events.back()->value;

		if (_interpolation == Discrete) {
			return lval;
		}

		/* linear interpolation between the two points */
		fraction = (double) (x - lpos) / (double) (upos - lpos);
		return lval + (fraction * (uval - lval));

	default:
		if (x >= _events.back()->when) {
			return _events.back()->value;
		} else if (x <= _events.front()->when) {
			return _events.front()->value;
		}

		return multipoint_eval (x);
	}

	abort(); /*NOTREACHED*/ /* stupid gcc */
	return _default_value;
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
ControlList::build_search_cache_if_necessary (double start) const
{
	if (_events.empty()) {
		/* Empty, nothing to cache, move to end. */
		_search_cache.first = _events.end();
		_search_cache.left = 0;
		return;
	} else if ((_search_cache.left < 0) || (_search_cache.left > start)) {
		/* Marked dirty (left < 0), or we're too far forward, re-search. */

		const ControlEvent start_point (start, 0);

		_search_cache.first = lower_bound (_events.begin(), _events.end(), &start_point, time_comparator);
		_search_cache.left = start;
	}

	/* We now have a search cache that is not too far right, but it may be too
	   far left and need to be advanced. */

	while (_search_cache.first != end() && (*_search_cache.first)->when < start) {
		++_search_cache.first;
	}
	_search_cache.left = start;
}

/** Get the earliest event after \a start using the current interpolation style.
 *
 * If an event is found, \a x and \a y are set to its coordinates.
 *
 * \param inclusive Include events with timestamp exactly equal to \a start
 * \return true if event is found (and \a x and \a y are valid).
 */
bool
ControlList::rt_safe_earliest_event (double start, double& x, double& y, bool inclusive) const
{
	// FIXME: It would be nice if this was unnecessary..
	Glib::Threads::RWLock::ReaderLock lm(_lock, Glib::Threads::TRY_LOCK);
	if (!lm.locked()) {
		return false;
	}

	return rt_safe_earliest_event_unlocked (start, x, y, inclusive);
}


/** Get the earliest event after \a start using the current interpolation style.
 *
 * If an event is found, \a x and \a y are set to its coordinates.
 *
 * \param inclusive Include events with timestamp exactly equal to \a start
 * \return true if event is found (and \a x and \a y are valid).
 */
bool
ControlList::rt_safe_earliest_event_unlocked (double start, double& x, double& y, bool inclusive) const
{
	if (_interpolation == Discrete) {
		return rt_safe_earliest_event_discrete_unlocked(start, x, y, inclusive);
	} else {
		return rt_safe_earliest_event_linear_unlocked(start, x, y, inclusive);
	}
}


/** Get the earliest event after \a start without interpolation.
 *
 * If an event is found, \a x and \a y are set to its coordinates.
 *
 * \param inclusive Include events with timestamp exactly equal to \a start
 * \return true if event is found (and \a x and \a y are valid).
 */
bool
ControlList::rt_safe_earliest_event_discrete_unlocked (double start, double& x, double& y, bool inclusive) const
{
	build_search_cache_if_necessary (start);

	if (_search_cache.first != _events.end()) {
		const ControlEvent* const first = *_search_cache.first;

		const bool past_start = (inclusive ? first->when >= start : first->when > start);

		/* Earliest points is in range, return it */
		if (past_start) {

			x = first->when;
			y = first->value;

			/* Move left of cache to this point
			 * (Optimize for immediate call this cycle within range) */
			_search_cache.left = x;
			++_search_cache.first;

			assert(x >= start);
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
ControlList::rt_safe_earliest_event_linear_unlocked (double start, double& x, double& y, bool inclusive) const
{
	// cout << "earliest_event(start: " << start << ", x: " << x << ", y: " << y << ", inclusive: " << inclusive <<  ")" << endl;

	const_iterator length_check_iter = _events.begin();
	if (_events.empty()) { // 0 events
		return false;
	} else if (_events.end() == ++length_check_iter) { // 1 event
		return rt_safe_earliest_event_discrete_unlocked (start, x, y, inclusive);
	}

	// Hack to avoid infinitely repeating the same event
	build_search_cache_if_necessary (start);

	if (_search_cache.first != _events.end()) {

		const ControlEvent* first = NULL;
		const ControlEvent* next = NULL;

		if (_search_cache.first == _events.begin() || (*_search_cache.first)->when <= start) {
			/* Step is after first */
			first = *_search_cache.first;
			++_search_cache.first;
			if (_search_cache.first == _events.end()) {
				return false;
			}
			next = *_search_cache.first;

		} else {
			/* Step is before first */
			const_iterator prev = _search_cache.first;
			--prev;
			first = *prev;
			next = *_search_cache.first;
		}

		if (inclusive && first->when == start) {
			x = first->when;
			y = first->value;
			/* Move left of cache to this point
			 * (Optimize for immediate call this cycle within range) */
			_search_cache.left = x;
			return true;
		} else if (next->when < start || (!inclusive && next->when == start)) {
			/* "Next" is before the start, no points left. */
			return false;
		}

		if (fabs(first->value - next->value) <= 1) {
			if (next->when > start) {
				x = next->when;
				y = next->value;
				/* Move left of cache to this point
				 * (Optimize for immediate call this cycle within range) */
				_search_cache.left = x;
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
		if (past_start) {
			/* Move left of cache to this point
			 * (Optimize for immediate call this cycle within range) */
			_search_cache.left = x;
			assert(inclusive ? x >= start : x > start);
			return true;
		} else {
			if (inclusive) {
				x = next->when;
			} else {
				x = start;
			}
			_search_cache.left = x;
			return true;
		}

	} else {
		/* No points in the future, so no steps (towards them) in the future */
		return false;
	}
}


/** @param start Start position in model coordinates.
 *  @param end End position in model coordinates.
 *  @param op 0 = cut, 1 = copy, 2 = clear.
 */
boost::shared_ptr<ControlList>
ControlList::cut_copy_clear (double start, double end, int op)
{
	boost::shared_ptr<ControlList> nal = create (_parameter, _desc);
	iterator s, e;
	ControlEvent cp (start, 0.0);

	{
		Glib::Threads::RWLock::WriterLock lm (_lock);

		/* first, determine s & e, two iterators that define the range of points
		   affected by this operation
		*/

		if ((s = lower_bound (_events.begin(), _events.end(), &cp, time_comparator)) == _events.end()) {
			return nal;
		}

		/* and the last that is at or after `end' */
		cp.when = end;
		e = upper_bound (_events.begin(), _events.end(), &cp, time_comparator);


		/* if "start" isn't the location of an existing point,
		   evaluate the curve to get a value for the start. Add a point to
		   both the existing event list, and if its not a "clear" operation,
		   to the copy ("nal") as well.

		   Note that the time positions of the points in each list are different
		   because we want the copy ("nal") to have a zero time reference.
		*/


		/* before we begin any cut/clear operations, get the value of the curve
		   at "end".
		*/

		double end_value = unlocked_eval (end);

		if ((*s)->when != start) {

			double val = unlocked_eval (start);

			if (op == 0) { // cut
				if (start > _events.front()->when) {
					_events.insert (s, (new ControlEvent (start, val)));
				}
			}

			if (op != 2) { // ! clear
				nal->_events.push_back (new ControlEvent (0, val));
			}
		}

		for (iterator x = s; x != e; ) {

			/* adjust new points to be relative to start, which
			   has been set to zero.
			*/

			if (op != 2) {
				nal->_events.push_back (new ControlEvent ((*x)->when - start, (*x)->value));
			}

			if (op != 1) {
				x = _events.erase (x);
			} else {
				++x;
			}
		}

		if (e == _events.end() || (*e)->when != end) {

			/* only add a boundary point if there is a point after "end"
			 */

			if (op == 0 && (e != _events.end() && end < (*e)->when)) { // cut
				_events.insert (e, new ControlEvent (end, end_value));
			}

			if (op != 2 && (e != _events.end() && end < (*e)->when)) { // cut/copy
				nal->_events.push_back (new ControlEvent (end - start, end_value));
			}
		}

		unlocked_invalidate_insert_iterator ();
		mark_dirty ();
	}

	if (op != 1) {
		maybe_signal_changed ();
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
	cut_copy_clear (start, end, 2);
}

/** @param pos Position in model coordinates */
bool
ControlList::paste (const ControlList& alist, double pos, float /*times*/)
{
	if (alist._events.empty()) {
		return false;
	}

	{
		Glib::Threads::RWLock::WriterLock lm (_lock);
		iterator where;
		iterator prev;
		double end = 0;
		ControlEvent cp (pos, 0.0);

		where = upper_bound (_events.begin(), _events.end(), &cp, time_comparator);

		for (const_iterator i = alist.begin();i != alist.end(); ++i) {
			double value = (*i)->value;
			if (alist.parameter() != parameter()) {
				const ParameterDescriptor& src_desc = alist.descriptor();

				// This does not work for logscale and will probably also not do
				// the right thing for integer_step and sr_dependent parameters.
				//
				// TODO various flags from from ARDOUR::ParameterDescriptor
				// to Evoral::ParameterDescriptor

				value -= src_desc.lower;  // translate to 0-relative
				value /= (src_desc.upper - src_desc.lower);  // normalize range
				value *= (_desc.upper - _desc.lower);  // scale to our range
				value += _desc.lower;  // translate to our offset
				if (_desc.toggled) {
					value = (value < 0.5) ? 0.0 : 1.0;
				}
			}
			_events.insert (where, new ControlEvent((*i)->when + pos, value));
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

		unlocked_invalidate_insert_iterator ();
		mark_dirty ();
	}

	maybe_signal_changed ();
	return true;
}

/** Move automation around according to a list of region movements.
 *  @param return true if anything was changed, otherwise false (ie nothing needed changing)
 */
bool
ControlList::move_ranges (const list< RangeMove<double> >& movements)
{
	typedef list< RangeMove<double> > RangeMoveList;

	{
		Glib::Threads::RWLock::WriterLock lm (_lock);

		/* a copy of the events list before we started moving stuff around */
		EventList old_events = _events;

		/* clear the source and destination ranges in the new list */
		bool things_erased = false;
		for (RangeMoveList::const_iterator i = movements.begin (); i != movements.end (); ++i) {

			if (erase_range_internal (i->from, i->from + i->length, _events)) {
				things_erased = true;
			}

			if (erase_range_internal (i->to, i->to + i->length, _events)) {
				things_erased = true;
			}
		}

		/* if nothing was erased, there is nothing to do */
		if (!things_erased) {
			return false;
		}

		/* copy the events into the new list */
		for (RangeMoveList::const_iterator i = movements.begin (); i != movements.end (); ++i) {
			iterator j = old_events.begin ();
			const double limit = i->from + i->length;
			const double dx    = i->to - i->from;
			while (j != old_events.end () && (*j)->when <= limit) {
				if ((*j)->when >= i->from) {
					ControlEvent* ev = new ControlEvent (**j);
					ev->when += dx;
					_events.push_back (ev);
				}
				++j;
			}
		}

		if (!_frozen) {
			_events.sort (event_time_less_than);
			unlocked_invalidate_insert_iterator ();
		} else {
			_sort_pending = true;
		}

		mark_dirty ();
	}

	maybe_signal_changed ();
	return true;
}

void
ControlList::set_interpolation (InterpolationStyle s)
{
	if (_interpolation == s) {
		return;
	}

	_interpolation = s;
	InterpolationChanged (s); /* EMIT SIGNAL */
}

bool
ControlList::operator!= (ControlList const & other) const
{
	if (_events.size() != other._events.size()) {
		return true;
	}

	EventList::const_iterator i = _events.begin ();
	EventList::const_iterator j = other._events.begin ();

	while (i != _events.end() && (*i)->when == (*j)->when && (*i)->value == (*j)->value) {
	        ++i;
		++j;
	}

	if (i != _events.end ()) {
		return true;
	}

	return (
		_parameter != other._parameter ||
		_interpolation != other._interpolation ||
		_min_yval != other._min_yval ||
		_max_yval != other._max_yval ||
		_default_value != other._default_value
		);
}

void
ControlList::dump (ostream& o)
{
	/* NOT LOCKED ... for debugging only */

	for (EventList::iterator x = _events.begin(); x != _events.end(); ++x) {
		o << (*x)->value << " @ " << (uint64_t) (*x)->when << endl;
	}
}

} // namespace Evoral

