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

#ifndef __ardour_automation_event_h__
#define __ardour_automation_event_h__

#include <stdint.h>
#include <list>
#include <cmath>

#include <sigc++/signal.h>
#include <glibmm/thread.h>

#include <pbd/undo.h>
#include <pbd/xml++.h>
#include <pbd/statefuldestructible.h> 

#include <ardour/ardour.h>
#include <ardour/parameter.h>

namespace ARDOUR {

class Curve;

struct ControlEvent {

    ControlEvent (double w, double v)
	    : when (w), value (v) { 
	    coeff[0] = coeff[1] = coeff[2] = coeff[3] = 0.0;
	}

    ControlEvent (const ControlEvent& other) 
	    : when (other.when), value (other.value) {
	    coeff[0] = coeff[1] = coeff[2] = coeff[3] = 0.0;
	}
    
    double when;
    double value;
    double coeff[4]; ///< Used by Curve
};


class AutomationList : public PBD::StatefulDestructible
{
  public:
	typedef std::list<ControlEvent*> EventList;
	typedef EventList::iterator iterator;
	typedef EventList::const_iterator const_iterator;

	AutomationList (Parameter id, double min_val, double max_val, double default_val);
	AutomationList (const XMLNode&, Parameter id);
	~AutomationList();

	AutomationList (const AutomationList&);
	AutomationList (const AutomationList&, double start, double end);
	AutomationList& operator= (const AutomationList&);
	bool operator== (const AutomationList&);

	Parameter parameter() const          { return _parameter; }
	void      set_parameter(Parameter p) { _parameter = p; }

	void freeze();
	void thaw ();

	EventList::size_type size() const { return _events.size(); }
	bool empty() const { return _events.empty(); }

	void reset_default (double val) {
		_default_value = val;
	}

	void clear ();
	void x_scale (double factor);
	bool extend_to (double);
	void slide (iterator before, double distance);
	
	void reposition_for_rt_add (double when);
	void rt_add (double when, double value);
	void add (double when, double value);
	/* this should be private but old-school automation loading needs it in IO/IOProcessor */
	void fast_simple_add (double when, double value);

	void reset_range (double start, double end);
	void erase_range (double start, double end);
	void erase (iterator);
	void erase (iterator, iterator);
	void move_range (iterator start, iterator end, double, double);
	void modify (iterator, double, double);

	AutomationList* cut (double, double);
	AutomationList* copy (double, double);
	void clear (double, double);

	AutomationList* cut (iterator, iterator);
	AutomationList* copy (iterator, iterator);
	void clear (iterator, iterator);

	bool paste (AutomationList&, double position, float times);

	void set_automation_state (AutoState);
	AutoState automation_state() const { return _state; }
	sigc::signal<void> automation_style_changed;

	void set_automation_style (AutoStyle m);
	AutoStyle automation_style() const { return _style; }
	sigc::signal<void> automation_state_changed;

	bool automation_playback() const {
		return (_state & Play) || ((_state & Touch) && !_touching);
	}
	bool automation_write () const {
		return (_state & Write) || ((_state & Touch) && _touching);
	}

	void start_touch ();
	void stop_touch ();
	bool touching() const { return _touching; }

	void set_yrange (double min, double max) {
		_min_yval = min;
		_max_yval = max;
	}

	double get_max_y() const { return _max_yval; }
	double get_min_y() const { return _min_yval; }

	void truncate_end (double length);
	void truncate_start (double length);
	
	iterator begin() { return _events.begin(); }
	iterator end() { return _events.end(); }

	ControlEvent* back() { return _events.back(); }
	ControlEvent* front() { return _events.front(); }

	const_iterator const_begin() const { return _events.begin(); }
	const_iterator const_end() const { return _events.end(); }

	std::pair<AutomationList::iterator,AutomationList::iterator> control_points_adjacent (double when);

	template<class T> void apply_to_points (T& obj, void (T::*method)(const AutomationList&)) {
		Glib::Mutex::Lock lm (_lock);
		(obj.*method)(*this);
	}

	sigc::signal<void> StateChanged;

	XMLNode& get_state(void); 
	int set_state (const XMLNode &s);
	XMLNode& state (bool full);
	XMLNode& serialize_events ();

	void set_max_xval (double);
	double get_max_xval() const { return _max_xval; }

	double eval (double where) {
		Glib::Mutex::Lock lm (_lock);
		return unlocked_eval (where);
	}

	double rt_safe_eval (double where, bool& ok) {

		Glib::Mutex::Lock lm (_lock, Glib::TRY_LOCK);

		if ((ok = lm.locked())) {
			return unlocked_eval (where);
		} else {
			return 0.0;
		}
	}

	static inline bool time_comparator (const ControlEvent* a, const ControlEvent* b) { 
		return a->when < b->when;
	}
	
	/** Lookup cache for eval functions, range contains equivalent values */
	struct LookupCache {
		LookupCache() : left(-1) {}
	    double left;  /* leftmost x coordinate used when finding "range" */
	    std::pair<AutomationList::const_iterator,AutomationList::const_iterator> range;
	};
	
	/** Lookup cache for point finding, range contains points between left and right */
	struct SearchCache {
		SearchCache() : left(-1), right(-1) {}
	    double left;  /* leftmost x coordinate used when finding "range" */
		double right; /* rightmost x coordinate used when finding "range" */
	    std::pair<AutomationList::const_iterator,AutomationList::const_iterator> range;
	};

	static sigc::signal<void, AutomationList*> AutomationListCreated;

	const EventList& events() const { return _events; }
	double default_value() const { return _default_value; }

	// teeny const violations for Curve
	mutable sigc::signal<void> Dirty;
	Glib::Mutex& lock() const { return _lock; }
	LookupCache& lookup_cache() const { return _lookup_cache; }
	SearchCache& search_cache() const { return _search_cache; }
	
	/** Called by locked entry point and various private
	 * locations where we already hold the lock.
	 * 
	 * FIXME: Should this be private?  Curve needs it..
	 */
	double unlocked_eval (double x) const;
	
	bool rt_safe_earliest_event (double start, double end, double& x, double& y) const;

	Curve&       curve()       { return *_curve; }
	const Curve& curve() const { return *_curve; }

	enum InterpolationStyle {
		Discrete,
		Linear,
		Curved
	};

	InterpolationStyle interpolation() const { return _interpolation; }
	void set_interpolation(InterpolationStyle style) { _interpolation = style; }

  private:

	/** Called by unlocked_eval() to handle cases of 3 or more control points.
	 */
	double multipoint_eval (double x) const; 

	void build_search_cache_if_necessary(double start, double end) const;
	
	bool rt_safe_earliest_event_discrete (double start, double end, double& x, double& y) const;
	bool rt_safe_earliest_event_linear (double start, double end, double& x, double& y) const;

	AutomationList* cut_copy_clear (double, double, int op);

	int deserialize_events (const XMLNode&);
	
	void maybe_signal_changed ();
	void mark_dirty ();
	void _x_scale (double factor);

	mutable LookupCache _lookup_cache;
	mutable SearchCache _search_cache;
	
	Parameter           _parameter;
	InterpolationStyle  _interpolation;
	EventList           _events;
	mutable Glib::Mutex _lock;
	int8_t              _frozen;
	bool                _changed_when_thawed;
	AutoState           _state;
	AutoStyle           _style;
	bool                _touching;
	bool                _new_touch;
	double              _max_xval;
	double              _min_yval;
	double              _max_yval;
	double              _default_value;
	bool                _sort_pending;
	iterator            _rt_insertion_point;
	double              _rt_pos;

	Curve* _curve;
};

} // namespace

#endif /* __ardour_automation_event_h__ */
