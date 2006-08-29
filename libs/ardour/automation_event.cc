/*
    Copyright (C) 2002 Paul Davis 

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

#include <set>
#include <climits>
#include <float.h>
#include <cmath>
#include <algorithm>
#include <sigc++/bind.h>
#include <ardour/automation_event.h>

#include "i18n.h"

using namespace std;
using namespace ARDOUR;
using namespace sigc;
using namespace PBD;

sigc::signal<void,AutomationList *> AutomationList::AutomationListCreated;

#if 0
static void dumpit (const AutomationList& al, string prefix = "")
{
	cerr << prefix << &al << endl;
	for (AutomationList::const_iterator i = al.const_begin(); i != al.const_end(); ++i) {
		cerr << prefix << '\t' << (*i)->when << ',' << (*i)->value << endl;
	}
	cerr << "\n";
}
#endif

AutomationList::AutomationList (double defval, bool with_state)
{
	_frozen = false;
	changed_when_thawed = false;
	_state = Off;
	_style = Absolute;
	_touching = false;
	no_state = with_state;
	min_yval = FLT_MIN;
	max_yval = FLT_MAX;
	max_xval = 0; // means "no limit" 
	default_value = defval;
	_dirty = false;
	rt_insertion_point = events.end();
	lookup_cache.left = -1;
	lookup_cache.range.first = events.end();

	if (!no_state) {
		save_state (_("initial"));
	}

        AutomationListCreated(this);
}

AutomationList::AutomationList (const AutomationList& other)
{
	_frozen = false;
	changed_when_thawed = false;
	_style = other._style;
	min_yval = other.min_yval;
	max_yval = other.max_yval;
	max_xval = other.max_xval;
	default_value = other.default_value;
	_state = other._state;
	_touching = other._touching;
	_dirty = false;
	rt_insertion_point = events.end();
	no_state = other.no_state;
	lookup_cache.left = -1;
	lookup_cache.range.first = events.end();

	for (const_iterator i = other.events.begin(); i != other.events.end(); ++i) {
		/* we have to use other point_factory() because
		   its virtual and we're in a constructor.
		*/
		events.push_back (other.point_factory (**i));
	}

	mark_dirty ();
        AutomationListCreated(this);
}

AutomationList::AutomationList (const AutomationList& other, double start, double end)
{
	_frozen = false;
	changed_when_thawed = false;
	_style = other._style;
	min_yval = other.min_yval;
	max_yval = other.max_yval;
	max_xval = other.max_xval;
	default_value = other.default_value;
	_state = other._state;
	_touching = other._touching;
	_dirty = false;
	rt_insertion_point = events.end();
	no_state = other.no_state;
	lookup_cache.left = -1;
	lookup_cache.range.first = events.end();

	/* now grab the relevant points, and shift them back if necessary */

	AutomationList* section = const_cast<AutomationList*>(&other)->copy (start, end);

	if (!section->empty()) {
		for (AutomationList::iterator i = section->begin(); i != section->end(); ++i) {
			events.push_back (other.point_factory ((*i)->when, (*i)->value));
		}
	}

	delete section;

	mark_dirty ();
        AutomationListCreated(this);
}

AutomationList::~AutomationList()
{
	std::set<ControlEvent*> all_events;
	AutomationList::State* asp;

	GoingAway ();

	for (AutomationEventList::iterator x = events.begin(); x != events.end(); ++x) {
		all_events.insert (*x);
	}

	for (StateMap::iterator i = states.begin(); i != states.end(); ++i) {

		if ((asp = dynamic_cast<AutomationList::State*> (*i)) != 0) {
			
			for (AutomationEventList::iterator x = asp->events.begin(); x != asp->events.end(); ++x) {
				all_events.insert (*x);
			}
		}
	}

	for (std::set<ControlEvent*>::iterator i = all_events.begin(); i != all_events.end(); ++i) {
		delete (*i);
	}
}

bool
AutomationList::operator== (const AutomationList& other)
{
	return events == other.events;
}

AutomationList&
AutomationList::operator= (const AutomationList& other)
{
	if (this != &other) {
		
		events.clear ();
		
		for (const_iterator i = other.events.begin(); i != other.events.end(); ++i) {
			events.push_back (point_factory (**i));
		}
		
		min_yval = other.min_yval;
		max_yval = other.max_yval;
		max_xval = other.max_xval;
		default_value = other.default_value;
		
		mark_dirty ();
		maybe_signal_changed ();
	}

	return *this;
}

void
AutomationList::maybe_signal_changed ()
{
	mark_dirty ();

	if (_frozen) {
		changed_when_thawed = true;
	} else {
		StateChanged (Change (0));
	}
}

void
AutomationList::set_automation_state (AutoState s)
{
	if (s != _state) {
		_state = s;
		automation_state_changed (); /* EMIT SIGNAL */
	}
}

void
AutomationList::set_automation_style (AutoStyle s)
{
	if (s != _style) {
		_style = s;
		automation_style_changed (); /* EMIT SIGNAL */
	}
}

void
AutomationList::start_touch ()
{
	_touching = true;
	_new_touch = true;
}

void
AutomationList::stop_touch ()
{
	_touching = false;
	_new_touch = false;
}

void
AutomationList::clear ()
{
	{
		Glib::Mutex::Lock lm (lock);
		events.clear ();
		if (!no_state) {
			save_state (_("cleared"));
		}
		mark_dirty ();
	}

	maybe_signal_changed ();
}

void
AutomationList::x_scale (double factor)
{
	Glib::Mutex::Lock lm (lock);
	_x_scale (factor);
}

bool
AutomationList::extend_to (double when)
{
	Glib::Mutex::Lock lm (lock);
	if (events.empty() || events.back()->when == when) {
		return false;
	}
	double factor = when / events.back()->when;
	_x_scale (factor);
	return true;
}

void AutomationList::_x_scale (double factor)
{
	for (AutomationList::iterator i = events.begin(); i != events.end(); ++i) {
		(*i)->when = floor ((*i)->when * factor);
	}

	save_state ("x-scaled");
	mark_dirty ();
}

void
AutomationList::reposition_for_rt_add (double when)
{
	rt_insertion_point = events.end();
}

#define last_rt_insertion_point rt_insertion_point

void
AutomationList::rt_add (double when, double value)
{
	/* this is for automation recording */

	if ((_state & Touch) && !_touching) {
		return;
	}

	// cerr << "RT: alist @ " << this << " add " << value << " @ " << when << endl;

	{
		Glib::Mutex::Lock lm (lock);

		iterator where;
		TimeComparator cmp;
		ControlEvent cp (when, 0.0);
		bool done = false;

		if ((last_rt_insertion_point != events.end()) && ((*last_rt_insertion_point)->when < when) ) {

			/* we have a previous insertion point, so we should delete
			   everything between it and the position where we are going
			   to insert this point.
			*/

			iterator after = last_rt_insertion_point;

			if (++after != events.end()) {
				iterator far = after;

				while (far != events.end()) {
					if ((*far)->when > when) {
						break;
					}
					++far;
				}

                                if(_new_touch) {
                                        where = far;
                                        last_rt_insertion_point = where;
                                                                                             
                                        if((*where)->when == when) {
                                                (*where)->value = value;
                                                done = true;
                                        }
                                } else {
                                        where = events.erase (after, far);
                                }

			} else {

				where = after;

			}
			
			iterator previous = last_rt_insertion_point;
                        --previous;
			
			if (last_rt_insertion_point != events.begin() && (*last_rt_insertion_point)->value == value && (*previous)->value == value) {
				(*last_rt_insertion_point)->when = when;
				done = true;
				
			}
			
		} else {

			where = lower_bound (events.begin(), events.end(), &cp, cmp);

			if (where != events.end()) {
				if ((*where)->when == when) {
					(*where)->value = value;
					done = true;
				}
			}
		}
		
		if (!done) {
			last_rt_insertion_point = events.insert (where, point_factory (when, value));
		}
		
		_new_touch = false;
		mark_dirty ();
	}

	maybe_signal_changed ();
}

#undef last_rt_insertion_point

void
AutomationList::add (double when, double value, bool for_loading)
{
	/* this is for graphical editing and loading data from storage */

	{
		Glib::Mutex::Lock lm (lock);
		TimeComparator cmp;
		ControlEvent cp (when, 0.0f);
		bool insert = true;
		iterator insertion_point;

		for (insertion_point = lower_bound (events.begin(), events.end(), &cp, cmp); insertion_point != events.end(); ++insertion_point) {

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

			events.insert (insertion_point, point_factory (when, value));
			reposition_for_rt_add (0);

		} 

		mark_dirty ();

		if (!no_state && !for_loading) {
			save_state (_("added event"));
		}
	}

	if (!for_loading) {
		maybe_signal_changed ();
	}
}

void
AutomationList::erase (AutomationList::iterator i)
{
	{
		Glib::Mutex::Lock lm (lock);
		events.erase (i);
		reposition_for_rt_add (0);
		if (!no_state) {
			save_state (_("removed event"));
		}
		mark_dirty ();
	}
	maybe_signal_changed ();
}

void
AutomationList::erase (AutomationList::iterator start, AutomationList::iterator end)
{
	{
		Glib::Mutex::Lock lm (lock);
		events.erase (start, end);
		reposition_for_rt_add (0);
		if (!no_state) {
			save_state (_("removed multiple events"));
		}
		mark_dirty ();
	}
	maybe_signal_changed ();
}	

void
AutomationList::reset_range (double start, double endt)
{
	bool reset = false;

	{
        Glib::Mutex::Lock lm (lock);
		TimeComparator cmp;
		ControlEvent cp (start, 0.0f);
		iterator s;
		iterator e;
		
		if ((s = lower_bound (events.begin(), events.end(), &cp, cmp)) != events.end()) {

			cp.when = endt;
			e = upper_bound (events.begin(), events.end(), &cp, cmp);

			for (iterator i = s; i != e; ++i) {
				(*i)->value = default_value;
			}
			
			reset = true;

			if (!no_state) {
				save_state (_("removed range"));
			}

			mark_dirty ();
		}
	}

	if (reset) {
		maybe_signal_changed ();
	}
}

void
AutomationList::erase_range (double start, double endt)
{
	bool erased = false;

	{
		Glib::Mutex::Lock lm (lock);
		TimeComparator cmp;
		ControlEvent cp (start, 0.0f);
		iterator s;
		iterator e;

		if ((s = lower_bound (events.begin(), events.end(), &cp, cmp)) != events.end()) {
			cp.when = endt;
			e = upper_bound (events.begin(), events.end(), &cp, cmp);
			events.erase (s, e);
			reposition_for_rt_add (0);
			erased = true;
			if (!no_state) {
				save_state (_("removed range"));
			}
			mark_dirty ();
		}
		
	}

	if (erased) {
		maybe_signal_changed ();
	}
}

void
AutomationList::move_range (iterator start, iterator end, double xdelta, double ydelta)
{
	/* note: we assume higher level logic is in place to avoid this
	   reordering the time-order of control events in the list. ie. all
	   points after end are later than (end)->when.
	*/

	{
		Glib::Mutex::Lock lm (lock);

		while (start != end) {
			(*start)->when += xdelta;
			(*start)->value += ydelta;
			++start;
		}

		if (!no_state) {
			save_state (_("event range adjusted"));
		}

		mark_dirty ();
	}

	maybe_signal_changed ();
}

void
AutomationList::modify (iterator iter, double when, double val)
{
	/* note: we assume higher level logic is in place to avoid this
	   reordering the time-order of control events in the list. ie. all
	   points after *iter are later than when.
	*/

	{
		Glib::Mutex::Lock lm (lock);
		(*iter)->when = when;
		(*iter)->value = val;
		if (!no_state) {
			save_state (_("event adjusted"));
		}

		mark_dirty ();
	}
	
	maybe_signal_changed ();
}

std::pair<AutomationList::iterator,AutomationList::iterator>
AutomationList::control_points_adjacent (double xval)
{
	Glib::Mutex::Lock lm (lock);
	iterator i;
	TimeComparator cmp;
	ControlEvent cp (xval, 0.0f);
	std::pair<iterator,iterator> ret;

	ret.first = events.end();
	ret.second = events.end();

	for (i = lower_bound (events.begin(), events.end(), &cp, cmp); i != events.end(); ++i) {
		
		if (ret.first == events.end()) {
			if ((*i)->when >= xval) {
				if (i != events.begin()) {
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
AutomationList::freeze ()
{
	_frozen = true;
}

void
AutomationList::thaw ()
{
	_frozen = false;
	if (changed_when_thawed) {
		 StateChanged(Change(0)); /* EMIT SIGNAL */
	}
}

StateManager::State*
AutomationList::state_factory (std::string why) const
{
	State* state = new State (why);

	for (AutomationEventList::const_iterator x = events.begin(); x != events.end(); ++x) {
		state->events.push_back (point_factory (**x));
	}

	return state;
}

Change
AutomationList::restore_state (StateManager::State& state) 
{
	{
		Glib::Mutex::Lock lm (lock);
		State* lstate = dynamic_cast<State*> (&state);

		events.clear ();
		for (AutomationEventList::const_iterator x = lstate->events.begin(); x != lstate->events.end(); ++x) {
			events.push_back (point_factory (**x));
		}
	}

	return Change (0);
}

UndoAction
AutomationList::get_memento () const
{
  return sigc::bind (mem_fun (*(const_cast<AutomationList*> (this)), &StateManager::use_state), _current_state_id);
}

void
AutomationList::set_max_xval (double x)
{
	max_xval = x;
}

void 
AutomationList::mark_dirty ()
{
	lookup_cache.left = -1;
	_dirty = true;
}

void
AutomationList::truncate_end (double last_coordinate)
{
	{
		Glib::Mutex::Lock lm (lock);
		ControlEvent cp (last_coordinate, 0);
		list<ControlEvent*>::reverse_iterator i;
		double last_val;

		if (events.empty()) {
			fatal << _("programming error:")
			      << "AutomationList::truncate_end() called on an empty list"
			      << endmsg;
			/*NOTREACHED*/
			return;
		}

		if (last_coordinate == events.back()->when) {
			return;
		}

		if (last_coordinate > events.back()->when) {
			
			/* extending end:
			*/

			iterator foo = events.begin();
			bool lessthantwo;

			if (foo == events.end()) {
				lessthantwo = true;
			} else if (++foo == events.end()) {
				lessthantwo = true;
			} else {
				lessthantwo = false;
			}

			if (lessthantwo) {
				/* less than 2 points: add a new point */
				events.push_back (point_factory (last_coordinate, events.back()->value));
			} else {

				/* more than 2 points: check to see if the last 2 values
				   are equal. if so, just move the position of the
				   last point. otherwise, add a new point.
				*/

				iterator penultimate = events.end();
				--penultimate; /* points at last point */
				--penultimate; /* points at the penultimate point */
				
				if (events.back()->value == (*penultimate)->value) {
					events.back()->when = last_coordinate;
				} else {
					events.push_back (point_factory (last_coordinate, events.back()->value));
				}
			}

		} else {

			/* shortening end */

			last_val = unlocked_eval (last_coordinate);
			last_val = max ((double) min_yval, last_val);
			last_val = min ((double) max_yval, last_val);
			
			i = events.rbegin();
			
			/* make i point to the last control point */
			
			++i;
			
			/* now go backwards, removing control points that are
			   beyond the new last coordinate.
			*/

			uint32_t sz = events.size();
			
			while (i != events.rend() && sz > 2) {
				list<ControlEvent*>::reverse_iterator tmp;
				
				tmp = i;
				++tmp;
				
				if ((*i)->when < last_coordinate) {
					break;
				}
				
				events.erase (i.base());
				--sz;

				i = tmp;
			}
			
			events.back()->when = last_coordinate;
			events.back()->value = last_val;
		}

		reposition_for_rt_add (0);
		mark_dirty();
	}

	maybe_signal_changed ();
}

void
AutomationList::truncate_start (double overall_length)
{
	{
		Glib::Mutex::Lock lm (lock);
		AutomationList::iterator i;
		double first_legal_value;
		double first_legal_coordinate;

		if (events.empty()) {
			fatal << _("programming error:")
			      << "AutomationList::truncate_start() called on an empty list"
			      << endmsg;
			/*NOTREACHED*/
			return;
		}
		
		if (overall_length == events.back()->when) {
			/* no change in overall length */
			return;
		}
		
		if (overall_length > events.back()->when) {
			
			/* growing at front: duplicate first point. shift all others */

			double shift = overall_length - events.back()->when;
			uint32_t np;

			for (np = 0, i = events.begin(); i != events.end(); ++i, ++np) {
				(*i)->when += shift;
			}

			if (np < 2) {

				/* less than 2 points: add a new point */
				events.push_front (point_factory (0, events.front()->value));

			} else {

				/* more than 2 points: check to see if the first 2 values
				   are equal. if so, just move the position of the
				   first point. otherwise, add a new point.
				*/

				iterator second = events.begin();
				++second; /* points at the second point */
				
				if (events.front()->value == (*second)->value) {
					/* first segment is flat, just move start point back to zero */
					events.front()->when = 0;
				} else {
					/* leave non-flat segment in place, add a new leading point. */
					events.push_front (point_factory (0, events.front()->value));
				}
			}

		} else {

			/* shrinking at front */
			
			first_legal_coordinate = events.back()->when - overall_length;
			first_legal_value = unlocked_eval (first_legal_coordinate);
			first_legal_value = max (min_yval, first_legal_value);
			first_legal_value = min (max_yval, first_legal_value);

			/* remove all events earlier than the new "front" */

			i = events.begin();
			
			while (i != events.end() && !events.empty()) {
				list<ControlEvent*>::iterator tmp;
				
				tmp = i;
				++tmp;
				
				if ((*i)->when > first_legal_coordinate) {
					break;
				}
				
				events.erase (i);
				
				i = tmp;
			}
			

			/* shift all remaining points left to keep their same
			   relative position
			*/
			
			for (i = events.begin(); i != events.end(); ++i) {
				(*i)->when -= first_legal_coordinate;
			}

			/* add a new point for the interpolated new value */
			
			events.push_front (point_factory (0, first_legal_value));
		}	    

		reposition_for_rt_add (0);

		mark_dirty();
	}

	maybe_signal_changed ();
}

double
AutomationList::unlocked_eval (double x)
{
	return shared_eval (x);
}

double
AutomationList::shared_eval (double x) 
{
	pair<AutomationEventList::iterator,AutomationEventList::iterator> range;
	int32_t npoints;
	double lpos, upos;
	double lval, uval;
	double fraction;

	npoints = events.size();

	switch (npoints) {
	case 0:
		return default_value;

	case 1:
		if (x >= events.front()->when) {
			return events.front()->value;
		} else {
			// return default_value;
			return events.front()->value;
		} 
		
	case 2:
		if (x >= events.back()->when) {
			return events.back()->value;
		} else if (x == events.front()->when) {
			return events.front()->value;
 		} else if (x < events.front()->when) {
			// return default_value;
			return events.front()->value;
		}

		lpos = events.front()->when;
		lval = events.front()->value;
		upos = events.back()->when;
		uval = events.back()->value;
		
		/* linear interpolation betweeen the two points
		*/

		fraction = (double) (x - lpos) / (double) (upos - lpos);
		return lval + (fraction * (uval - lval));

	default:

		if (x >= events.back()->when) {
			return events.back()->value;
		} else if (x == events.front()->when) {
			return events.front()->value;
 		} else if (x < events.front()->when) {
			// return default_value;
			return events.front()->value;
		}

		return multipoint_eval (x);
		break;
	}
}

double
AutomationList::multipoint_eval (double x) 
{
	pair<AutomationList::iterator,AutomationList::iterator> range;
	double upos, lpos;
	double uval, lval;
	double fraction;

	/* only do the range lookup if x is in a different range than last time
	   this was called (or if the lookup cache has been marked "dirty" (left<0)
	*/

	if ((lookup_cache.left < 0) ||
	    ((lookup_cache.left > x) || 
	     (lookup_cache.range.first == events.end()) || 
	     ((*lookup_cache.range.second)->when < x))) {

		ControlEvent cp (x, 0);
		TimeComparator cmp;
		
		lookup_cache.range = equal_range (events.begin(), events.end(), &cp, cmp);
	}
	
	range = lookup_cache.range;

	if (range.first == range.second) {

		/* x does not exist within the list as a control point */

		lookup_cache.left = x;

		if (range.first != events.begin()) {
			--range.first;
			lpos = (*range.first)->when;
			lval = (*range.first)->value;
		}  else {
			/* we're before the first point */
			// return default_value;
			return events.front()->value;
		}
		
		if (range.second == events.end()) {
			/* we're after the last point */
			return events.back()->value;
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
	lookup_cache.left = -1;
	return (*range.first)->value;
}

AutomationList*
AutomationList::cut (iterator start, iterator end)
{
	AutomationList* nal = new AutomationList (default_value);

	{
		Glib::Mutex::Lock lm (lock);

		for (iterator x = start; x != end; ) {
			iterator tmp;
			
			tmp = x;
			++tmp;
			
			nal->events.push_back (point_factory (**x));
			events.erase (x);
			
			reposition_for_rt_add (0);

			x = tmp;
		}

		mark_dirty ();
	}

	maybe_signal_changed ();

	return nal;
}

AutomationList*
AutomationList::cut_copy_clear (double start, double end, int op)
{
	AutomationList* nal = new AutomationList (default_value);
	iterator s, e;
	ControlEvent cp (start, 0.0);
	TimeComparator cmp;
	bool changed = false;
	
	{
		Glib::Mutex::Lock lm (lock);

		if ((s = lower_bound (events.begin(), events.end(), &cp, cmp)) == events.end()) {
			return nal;
		}

		cp.when = end;
		e = upper_bound (events.begin(), events.end(), &cp, cmp);

		if (op != 2 && (*s)->when != start) {
			nal->events.push_back (point_factory (0, unlocked_eval (start)));
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
				nal->events.push_back (point_factory ((*x)->when - start, (*x)->value));
			}

			if (op != 1) {
				events.erase (x);
			}
			
			x = tmp;
		}

		if (op != 2 && nal->events.back()->when != end - start) {
			nal->events.push_back (point_factory (end - start, unlocked_eval (end)));
		}

		if (changed) {
			reposition_for_rt_add (0);
			if (!no_state) {
				save_state (_("cut/copy/clear"));
			}
		}

		mark_dirty ();
	}

	maybe_signal_changed ();

	return nal;

}

AutomationList*
AutomationList::copy (iterator start, iterator end)
{
	AutomationList* nal = new AutomationList (default_value);

	{
		Glib::Mutex::Lock lm (lock);
		
		for (iterator x = start; x != end; ) {
			iterator tmp;
			
			tmp = x;
			++tmp;
			
			nal->events.push_back (point_factory (**x));
			
			x = tmp;
		}

		if (!no_state) {
			save_state (_("copy"));
		}
	}

	return nal;
}

AutomationList*
AutomationList::cut (double start, double end)
{
	return cut_copy_clear (start, end, 0);
}

AutomationList*
AutomationList::copy (double start, double end)
{
	return cut_copy_clear (start, end, 1);
}

void
AutomationList::clear (double start, double end)
{
	(void) cut_copy_clear (start, end, 2);
}

bool
AutomationList::paste (AutomationList& alist, double pos, float times)
{
	if (alist.events.empty()) {
		return false;
	}

	{
		Glib::Mutex::Lock lm (lock);
		iterator where;
		iterator prev;
		double end = 0;
		ControlEvent cp (pos, 0.0);
		TimeComparator cmp;

		where = upper_bound (events.begin(), events.end(), &cp, cmp);

		for (iterator i = alist.begin();i != alist.end(); ++i) {
			events.insert (where, point_factory( (*i)->when+pos,( *i)->value));
			end = (*i)->when + pos;
		}
	
	
		/* move all  points after the insertion along the timeline by 
		   the correct amount.
		*/

		while (where != events.end()) {
			iterator tmp;
			if ((*where)->when <= end) {
				tmp = where;
				++tmp;
				events.erase(where);
				where = tmp;

			} else {
				break;
			}
		}

		reposition_for_rt_add (0);

		if (!no_state) {
			save_state (_("paste"));
		}

		mark_dirty ();
	}

	maybe_signal_changed ();
	return true;
}

ControlEvent*
AutomationList::point_factory (double when, double val) const
{
	return new ControlEvent (when, val);
}

ControlEvent*
AutomationList::point_factory (const ControlEvent& other) const
{
	return new ControlEvent (other);
}

void
AutomationList::store_state (XMLNode& node) const
{
	LocaleGuard lg (X_("POSIX"));

	for (const_iterator i = const_begin(); i != const_end(); ++i) {
		char buf[64];
		
		XMLNode *pointnode = new XMLNode ("point");
		
		snprintf (buf, sizeof (buf), "%" PRIu32, (jack_nframes_t) floor ((*i)->when));
		pointnode->add_property ("x", buf);
		snprintf (buf, sizeof (buf), "%.12g", (*i)->value);
		pointnode->add_property ("y", buf);

		node.add_child_nocopy (*pointnode);
	}
}

void
AutomationList::load_state (const XMLNode& node)
{
	const XMLNodeList& elist = node.children();
	XMLNodeConstIterator i;
	XMLProperty* prop;
	jack_nframes_t x;
	double y;

	clear ();
	
	for (i = elist.begin(); i != elist.end(); ++i) {
		
		if ((prop = (*i)->property ("x")) == 0) {
			error << _("automation list: no x-coordinate stored for control point (point ignored)") << endmsg;
			continue;
		}
		x = atoi (prop->value().c_str());
		
		if ((prop = (*i)->property ("y")) == 0) {
			error << _("automation list: no y-coordinate stored for control point (point ignored)") << endmsg;
			continue;
		}
		y = atof (prop->value().c_str());
		
		add (x, y);
	}
}

XMLNode &AutomationList::get_state ()
{
    XMLNode *node = new XMLNode("AutomationList");
    store_state(*node);
    return *node;
}

int AutomationList::set_state(const XMLNode &s)
{
    load_state(s);
    return 0;
}

