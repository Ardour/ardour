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
 
*/

#include <set>
#include <climits>
#include <float.h>
#include <cmath>
#include <sstream>
#include <iomanip>
#include <algorithm>
#include <sigc++/bind.h>
#include <ardour/automation_event.h>
#include <pbd/stacktrace.h>
#include <pbd/localeguard.h>

#include "i18n.h"

using namespace std;
using namespace ARDOUR;
using namespace sigc;
using namespace PBD;

sigc::signal<void,AutomationList *> AutomationList::AutomationListCreated;

static bool sort_events_by_time (ControlEvent* a, ControlEvent* b)
{
	return a->when < b->when;
}

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

AutomationList::AutomationList (double defval)
{
	_frozen = 0;
	changed_when_thawed = false;
	_state = Auto_Off;
	_style = Auto_Absolute;
	g_atomic_int_set (&_touching, 0);
	min_yval = FLT_MIN;
	max_yval = FLT_MAX;
	max_xval = 0; // means "no limit" 
	default_value = defval;
	_dirty = false;
	lookup_cache.left = -1;
	lookup_cache.range.first = events.end();
	sort_pending = false;

        AutomationListCreated(this);
}

AutomationList::AutomationList (const AutomationList& other)
{
	_frozen = 0;
	changed_when_thawed = false;
	_style = other._style;
	min_yval = other.min_yval;
	max_yval = other.max_yval;
	max_xval = other.max_xval;
	default_value = other.default_value;
	_state = other._state;
	g_atomic_int_set (&_touching, 0);
	_dirty = false;
	lookup_cache.left = -1;
	lookup_cache.range.first = events.end();
	sort_pending = false;

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
	_frozen = 0;
	changed_when_thawed = false;
	_style = other._style;
	min_yval = other.min_yval;
	max_yval = other.max_yval;
	max_xval = other.max_xval;
	default_value = other.default_value;
	_state = other._state;
	g_atomic_int_set (&_touching, other.touching());
	_dirty = false;
	lookup_cache.left = -1;
	lookup_cache.range.first = events.end();
	sort_pending = false;

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

AutomationList::AutomationList (const XMLNode& node)
{
	_frozen = 0;
	changed_when_thawed = false;
	g_atomic_int_set (&_touching, 0);
	min_yval = FLT_MIN;
	max_yval = FLT_MAX;
	max_xval = 0; // means "no limit" 
	_dirty = false;
	_state = Auto_Off;
	_style = Auto_Absolute;
	lookup_cache.left = -1;
	lookup_cache.range.first = events.end();
	sort_pending = false;
	
	set_state (node);

        AutomationListCreated(this);
}

AutomationList::~AutomationList()
{
	GoingAway ();
	
	for (AutomationEventList::iterator x = events.begin(); x != events.end(); ++x) {
		delete (*x);
	}

	for (list<NascentInfo*>::iterator n = nascent.begin(); n != nascent.end(); ++n) {
                for (AutomationEventList::iterator x = (*n)->events.begin(); x != (*n)->events.end(); ++x) {
                        delete *x;
                }
		delete (*n);
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

		lookup_cache.range.first = events.end();
	
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
		StateChanged ();
	}
}

void
AutomationList::set_automation_state (AutoState s)
{
	if (s != _state) {
		_state = s;

                if (_state == Auto_Write) {
                        Glib::Mutex::Lock lm (lock);
                        nascent.push_back (new NascentInfo (false));
                }

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
AutomationList::start_touch (double when)
{
        if (_state == Auto_Touch) {
                Glib::Mutex::Lock lm (lock);
                nascent.push_back (new NascentInfo (true, when));
        }

	g_atomic_int_set (&_touching, 1);
}

void
AutomationList::stop_touch (bool mark, double when, double value)
{
	g_atomic_int_set (&_touching, 0);

        if (_state == Auto_Touch) {
                Glib::Mutex::Lock lm (lock);
                
                if (mark) {
                        nascent.back()->end_time = when;
                        
                } else {
                        
                        /* nascent info created in start touch but never used. just get rid of it.
                         */
                        
                        NascentInfo* ninfo = nascent.back ();
                        nascent.erase (nascent.begin());
                        delete ninfo;
                }
        }
        
	//if no automation yet
    if (events.empty()) {
		default_value = value;
	}
}

void
AutomationList::clear ()
{
	{
		Glib::Mutex::Lock lm (lock);
		events.clear ();
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
		(*i)->when *= factor;
	}

	mark_dirty ();
}

void
AutomationList::write_pass_finished (double when)
{
	//if fader is in Write, we need to put an automation point to mark the last place we rolled.
	if ( (_state & Auto_Write) ) {
		if ( !nascent.empty() && !nascent.back()->events.empty() ) {
			rt_add( when, nascent.back()->events.back()->value );
		}
	}

    merge_nascent (when);
}

void
AutomationList::rt_add (double when, double value)
{
	//for now, automation only writes during forward motion
	//if transport goes in reverse, start a new nascent pass and ignore this change
	float last_when = 0;
	if (!nascent.back()->events.empty())
		last_when = nascent.back()->events.back()->when;
	if ( (when < last_when) ) {
		Glib::Mutex::Lock lm (lock);
		nascent.push_back (new NascentInfo (false));
		return;
	}
	
	/* this is for automation recording */

	if ((_state & Auto_Touch) && !touching()) {
                return;
        }

        Glib::Mutex::Lock lm (lock, Glib::TRY_LOCK);

        if (lm.locked()) {
                assert (!nascent.empty());
		/* we don't worry about adding events out of time order as we will
		   sort them in merge_nascent.
		*/
                nascent.back()->events.push_back (point_factory (when, value));
        }
}

struct ControlEventTimeComparator {
    bool operator() (ControlEvent* a, ControlEvent* b) {
            return a->when < b->when;
    }
};

void
AutomationList::merge_nascent (double when)
{
        {
                Glib::Mutex::Lock lm (lock);

                if (nascent.empty()) {
                        return;
                }

                //thin automation data in each nascent packet
                for (list<NascentInfo*>::iterator n = nascent.begin(); n != nascent.end(); ++n) {
                        ControlEvent *next = NULL;
                        ControlEvent *cur = NULL;
                        ControlEvent *prev = NULL;
                        int counter = 0;
                        AutomationEventList delete_list;
                        for (AutomationEventList::iterator x = (*n)->events.begin(); x != (*n)->events.end(); x++) {
                                next = *x;
                                counter++;
                                if (counter > 2) {  //wait for the third iteration so "cur" & "prev" are initialized
	
                                        float area = fabs(
                                                0.5 * (
                                                        prev->when*(cur->value - next->value) + 
                                                        cur->when*(next->value - prev->value) + 
                                                        next->when*(prev->value - cur->value) ) 
                                                );
							 
//printf( "area: %3.16f\n", area);
                                        if (area < ( Config->get_automation_thinning_strength() ) )
                                                delete_list.push_back(cur);
                                }
                                prev = cur;
                                cur = next;
                        }
					
                        for (AutomationEventList::iterator x = delete_list.begin(); x != delete_list.end(); ++x) {
                                (*n)->events.remove(*x);
                                delete *x;
                        }
					
                }
				
                for (list<NascentInfo*>::iterator n = nascent.begin(); n != nascent.end(); ++n) {

                        NascentInfo* ninfo = *n;
                        AutomationEventList& nascent_events (ninfo->events);
                        bool need_adjacent_start_clamp;
                        bool need_adjacent_end_clamp;

                        if (nascent_events.size() < 2) {
                                delete ninfo;
                                continue;
                        }
                        
			nascent_events.sort (ControlEventTimeComparator ());
			
                        if (ninfo->start_time < 0.0) {
                                ninfo->start_time = nascent_events.front()->when;
                        }
                        
                        if (ninfo->end_time < 0.0) {
                                ninfo->end_time = nascent_events.back()->when;
                        }

                        bool preexisting = !events.empty();

                        if (!preexisting) {
                                
                                events = nascent_events;
                                
                        } else {
                                
                                /* find the range that overaps with nascent events,
                                   and insert the contents of nascent events.
                                */
                                
                                iterator i;
                                iterator range_begin = events.end();
                                iterator range_end = events.end();
                                double end_value = unlocked_eval (ninfo->end_time + 1);
                                double start_value = unlocked_eval (ninfo->start_time - 1);

                                need_adjacent_end_clamp = true;
                                need_adjacent_start_clamp = true;

                                for (i = events.begin(); i != events.end(); ++i) {

                                        if ((*i)->when == ninfo->start_time) {
                                                /* existing point at same time, remove it
                                                   and the consider the next point instead.
                                                */
                                                i = events.erase (i);

                                                if (i == events.end()) {
                                                        break;
                                                }

                                                if (range_begin == events.end()) {
                                                        range_begin = i;
                                                        need_adjacent_start_clamp = false;
                                                } else {
                                                        need_adjacent_end_clamp = false;
                                                }
                                                
                                                if ((*i)->when > ninfo->end_time) {
                                                        range_end = i;
                                                        break;
                                                }   

                                        } else if ((*i)->when > ninfo->start_time) {
                                                
                                                if (range_begin == events.end()) {
                                                        range_begin = i;
                                                }
                                                
                                                if ((*i)->when > ninfo->end_time) {
                                                        range_end = i;
                                                        break;
                                                }
                                        }
                                }
 
                                //if you write past the end of existing automation,
                                //then treat it as virgin territory
                                if (range_end == events.end()) {
                                        need_adjacent_end_clamp = false;
                                }

                                /* clamp point before */
                                if (need_adjacent_start_clamp) {
                                        events.insert (range_begin, point_factory (ninfo->start_time-1, start_value));
                                }

                                events.insert (range_begin, nascent_events.begin(), nascent_events.end());

                                /* clamp point after */
                                if (need_adjacent_end_clamp) {
                                        events.insert (range_begin, point_factory (ninfo->end_time+1, end_value));
                                }
						
                                events.erase (range_begin, range_end);
                        }

                        delete ninfo;
                }

                nascent.clear ();

                if (_state == Auto_Write) {
                        nascent.push_back (new NascentInfo (false));
                }
        }

        maybe_signal_changed ();
}

void
AutomationList::fast_simple_add (double when, double value)
{
	/* to be used only for loading pre-sorted data from saved state */
	if ( events.empty() || (when > events.back()->when) ) 	
		events.insert (events.end(), point_factory (when, value));
}

void
AutomationList::add (double when, double value)
{
	/* this is for making changes from some kind of user interface or control surface (GUI, MIDI, OSC etc) */

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
		} 

		mark_dirty ();
	}

	maybe_signal_changed ();
}

void
AutomationList::erase (AutomationList::iterator i)
{
	{
		Glib::Mutex::Lock lm (lock);
		events.erase (i);
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
			erased = true;
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
			if (isnan ((*start)->value)) {
				abort ();
			}
			++start;
		}

		if (!_frozen) {
			events.sort (sort_events_by_time);
		} else {
			sort_pending = true;
		}

		mark_dirty ();
	}

	maybe_signal_changed ();
}

void
AutomationList::slide (iterator before, double distance)
{
	{
		Glib::Mutex::Lock lm (lock);

		if (before == events.end()) {
			return;
		}
		
		while (before != events.end()) {
			(*before)->when += distance;
			++before;
		}
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

		if (isnan (val)) {
			abort ();
		}

		if (!_frozen) {
			events.sort (sort_events_by_time);
		} else {
			sort_pending = true;
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
	_frozen++;
}

void
AutomationList::thaw ()
{
	if (_frozen == 0) {
		PBD::stacktrace (cerr);
		fatal << string_compose (_("programming error: %1"), X_("AutomationList::thaw() called while not frozen")) << endmsg;
		/*NOTREACHED*/
	}

	if (--_frozen > 0) {
		return;
	}

	{
		Glib::Mutex::Lock lm (lock);

		if (sort_pending) {
			events.sort (sort_events_by_time);
			sort_pending = false;
		}
	}

	if (changed_when_thawed) {
		StateChanged(); /* EMIT SIGNAL */
	}
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
		AutomationList::reverse_iterator i;
		double last_val;

		if (events.empty()) {
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
				AutomationList::reverse_iterator tmp;
				
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
			cerr  << _("programming error:")
			      << "AutomationList::truncate_start() called on an empty list"
			      << endmsg;			
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
				AutomationList::iterator tmp;
				
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
		return events.front()->value;
		
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

		return AutomationList::multipoint_eval (x);  //switch to plain linear for now.
		break;
	}

	/*NOTREACHED*/ /* stupid gcc */
	return 0.0;
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
AutomationList::cut_copy_clear (double start, double end, int op)
{
	AutomationList* nal = new AutomationList (default_value);
	iterator s, e;
	ControlEvent cp (start, 0.0);
	TimeComparator cmp;

	{
		Glib::Mutex::Lock lm (lock);

                /* first, determine s & e, two iterators that define the range of points
                   affected by this operation
                */

		if ((s = lower_bound (events.begin(), events.end(), &cp, cmp)) == events.end()) {
			return nal;
		}

		cp.when = end;
		e = upper_bound (events.begin(), events.end(), &cp, cmp);

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
				if (start > events.front()->when) {
					events.insert (s, (point_factory (start, val)));
				}
			}
                        
                        if (op != 2) { // ! clear
                                nal->events.push_back (point_factory (0, val));
                        }
                }

		for (iterator x = s; x != e; ) {

			/* adjust new points to be relative to start, which
			   has been set to zero.
			*/
			
			if (op != 2) {
				nal->events.push_back (point_factory ((*x)->when - start, (*x)->value));
			}

			if (op != 1) {
				x = events.erase (x);
			} else {
                                ++x;
                        }
		}
                
                if (e == events.end() || (*e)->when != end) {

                        /* only add a boundary point if there is a point after "end"
                         */

                        if (op == 0 && (e != events.end() && end < (*e)->when)) { // cut
                                events.insert (e, point_factory (end, end_value));
                        }

                        if (op != 2 && (e != events.end() && end < (*e)->when)) { // cut/copy
                                nal->events.push_back (point_factory (end - start, end_value));
                        }
		}

                mark_dirty ();
	}

        if (op != 1) {
                maybe_signal_changed ();
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

XMLNode&
AutomationList::get_state ()
{
	return state (true);
}

XMLNode&
AutomationList::state (bool full)
{
	XMLNode* root = new XMLNode (X_("AutomationList"));
	char buf[64];
	LocaleGuard lg (X_("POSIX"));

	root->add_property ("id", _id.to_s());

	snprintf (buf, sizeof (buf), "%.12g", default_value);
	root->add_property ("default", buf);
	snprintf (buf, sizeof (buf), "%.12g", min_yval);
	root->add_property ("min_yval", buf);
	snprintf (buf, sizeof (buf), "%.12g", max_yval);
	root->add_property ("max_yval", buf);
	snprintf (buf, sizeof (buf), "%.12g", max_xval);
	root->add_property ("max_xval", buf);

	if (full) {
                /* never serialize state with Write enabled - too dangerous 
                   for the user's data
                */
                if (_state != Auto_Write) {
                        root->add_property ("state", auto_state_to_string (_state));
                } else {
                        root->add_property ("state", auto_state_to_string (Auto_Off));
                }
	} else {
		/* never save anything but Off for automation state to a template */
		root->add_property ("state", auto_state_to_string (Auto_Off));
	}

	root->add_property ("style", auto_style_to_string (_style));

	if (!events.empty()) {
		root->add_child_nocopy (serialize_events());
	}

	return *root;
}

XMLNode&
AutomationList::serialize_events ()
{
	XMLNode* node = new XMLNode (X_("events"));
	stringstream str;
	
	str.precision(15);  //10 digits is enough digits for 24 hours at 96kHz

	for (iterator xx = events.begin(); xx != events.end(); ++xx) {
		str << (double) (*xx)->when;
		str << ' ';
		str <<(double) (*xx)->value;
		str << '\n';
	}

	/* XML is a bit wierd */

	XMLNode* content_node = new XMLNode (X_("foo")); /* it gets renamed by libxml when we set content */
	content_node->set_content (str.str());

	node->add_child_nocopy (*content_node);

	return *node;
}

int
AutomationList::deserialize_events (const XMLNode& node)
{
	if (node.children().empty()) {
		return -1;
	}

	XMLNode* content_node = node.children().front();

	if (content_node->content().empty()) {
		return -1;
	}

	freeze ();
	clear ();
	
	stringstream str (content_node->content());
	
	double x;
	double y;
	bool ok = true;
	
	while (str) {
		str >> x;
		if (!str) {
			break;
		}
		str >> y;
		if (!str) {
			ok = false;
			break;
		}
		fast_simple_add (x, y);
	}
	
	if (!ok) {
		clear ();
		error << _("automation list: cannot load coordinates from XML, all points ignored") << endmsg;
	} else {
		mark_dirty ();
		maybe_signal_changed ();
	}

	thaw ();

	return 0;
}

int
AutomationList::set_state (const XMLNode& node)
{
	XMLNodeList nlist = node.children();
	XMLNode* nsos;
	XMLNodeIterator niter;
	const XMLProperty* prop;

	if (node.name() == X_("events")) {
		/* partial state setting*/
		return deserialize_events (node);
	}
	
	if (node.name() == X_("Envelope") || node.name() == X_("FadeOut") || node.name() == X_("FadeIn")) {

		if ((nsos = node.child (X_("AutomationList")))) {
			/* new school in old school clothing */
			return set_state (*nsos);
		}

		/* old school */

		const XMLNodeList& elist = node.children();
		XMLNodeConstIterator i;
		XMLProperty* prop;
		jack_nframes_t x;
		double y;
		
		freeze ();
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
			
			fast_simple_add (x, y);
		}
		
		thaw ();

		return 0;
	}

	if (node.name() != X_("AutomationList") ) {
		error << string_compose (_("AutomationList: passed XML node called %1, not \"AutomationList\" - ignored"), node.name()) << endmsg;
		return -1;
	}
	
	if ((prop = node.property ("id")) != 0) {
		_id = prop->value ();
		/* update session AL list */
		AutomationListCreated(this);
	}
	
	if ((prop = node.property (X_("default"))) != 0){ 
		default_value = atof (prop->value());
	} else {
		default_value = 0.0;
	}

	if ((prop = node.property (X_("style"))) != 0) {
		_style = string_to_auto_style (prop->value());
	} else {
		_style = Auto_Absolute;
	}

	if ((prop = node.property (X_("state"))) != 0) {
		_state = string_to_auto_state (prop->value());
                if (_state == Auto_Write) {
                        _state = Auto_Off;
                }
	} else {
		_state = Auto_Off;
	}

	if ((prop = node.property (X_("min_yval"))) != 0) {
		min_yval = atof (prop->value ());
	} else {
		min_yval = FLT_MIN;
	}

	if ((prop = node.property (X_("max_yval"))) != 0) {
		max_yval = atof (prop->value ());
	} else {
		max_yval = FLT_MAX;
	}

	if ((prop = node.property (X_("max_xval"))) != 0) {
		max_xval = atof (prop->value ());
	} else {
		max_xval = 0; // means "no limit ;
	}

	for (niter = nlist.begin(); niter != nlist.end(); ++niter) {
		if ((*niter)->name() == X_("events")) {
			deserialize_events (*(*niter));
		}
	}

	return 0;
}

void
AutomationList::shift (nframes64_t pos, nframes64_t frames)
{
	{
		Glib::Mutex::Lock lm (lock);

		for (iterator i = begin (); i != end (); ++i) {
			if ((*i)->when >= pos) {
				(*i)->when += frames;
			}
		}

		mark_dirty ();
	}

	maybe_signal_changed ();
}
