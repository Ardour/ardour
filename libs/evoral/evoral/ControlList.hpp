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

#ifndef EVORAL_CONTROL_LIST_HPP
#define EVORAL_CONTROL_LIST_HPP

#include <cassert>
#include <list>
#include <boost/pool/pool.hpp>
#include <boost/pool/pool_alloc.hpp>
#include <glibmm/thread.h>
#include "pbd/signals.h"
#include "evoral/types.hpp"
#include "evoral/Range.hpp"
#include "evoral/Parameter.hpp"

namespace Evoral {

class Curve;

/** A single event (time-stamped value) for a control
 */
struct ControlEvent {
	ControlEvent (double w, double v)
		: when (w), value (v), coeff (0)
	{}

	ControlEvent (const ControlEvent& other)
		: when (other.when), value (other.value), coeff (0)
	{
		if (other.coeff) {
			create_coeffs();
			for (size_t i = 0; i < 4; ++i)
				coeff[i] = other.coeff[i];
		}
	}

	~ControlEvent() { if (coeff) delete[] coeff; }

	void create_coeffs() {
		if (!coeff)
			coeff = new double[4];

		coeff[0] = coeff[1] = coeff[2] = coeff[3] = 0.0;
	}

	double  when;
	double  value;
	double* coeff; ///< double[4] allocated by Curve as needed
};


/** Pool allocator for control lists that does not use a lock
 * and allocates 8k blocks of new pointers at a time
 */
typedef boost::fast_pool_allocator<
	ControlEvent*,
	boost::default_user_allocator_new_delete,
	boost::details::pool::null_mutex,
	8192>
ControlEventAllocator;


/** A list (sequence) of time-stamped values for a control
 */
class ControlList
{
public:
	typedef std::list<ControlEvent*,ControlEventAllocator> EventList;
	typedef EventList::iterator iterator;
	typedef EventList::reverse_iterator reverse_iterator;
	typedef EventList::const_iterator const_iterator;

	ControlList (const Parameter& id);
	ControlList (const ControlList&);
	ControlList (const ControlList&, double start, double end);
	virtual ~ControlList();

	virtual boost::shared_ptr<ControlList> create(Parameter id);

	ControlList& operator= (const ControlList&);
	bool operator== (const ControlList&);
        void copy_events (const ControlList&);

	virtual void freeze();
	virtual void thaw ();
	bool frozen() const { return _frozen; }

	const Parameter& parameter() const                 { return _parameter; }
	void             set_parameter(const Parameter& p) { _parameter = p; }

	EventList::size_type size() const { return _events.size(); }
	bool empty() const { return _events.empty(); }

	void reset_default (double val) {
		_default_value = val;
	}

	void clear ();
	void x_scale (double factor);
	bool extend_to (double);
	void slide (iterator before, double distance);
	void shift (double before, double distance);

	virtual bool clamp_value (double& /*when*/, double& /*value*/) const { return true; }

	void rt_add (double when, double value);
	void add (double when, double value);
	void fast_simple_add (double when, double value);
	void merge_nascent (double when);

	void erase_range (double start, double end);
	void erase (iterator);
	void erase (iterator, iterator);
	void erase (double, double);
	bool move_ranges (std::list< RangeMove<double> > const &);
	void modify (iterator, double, double);

        void thin ();

	boost::shared_ptr<ControlList> cut (double, double);
	boost::shared_ptr<ControlList> copy (double, double);
	void clear (double, double);

	bool paste (ControlList&, double position, float times);

	void set_yrange (double min, double max) {
		_min_yval = min;
		_max_yval = max;
	}

	double get_max_y() const { return _max_yval; }
	double get_min_y() const { return _min_yval; }

	void truncate_end (double length);
	void truncate_start (double length);

	iterator            begin()       { return _events.begin(); }
	const_iterator      begin() const { return _events.begin(); }
	iterator            end()         { return _events.end(); }
	const_iterator      end()   const { return _events.end(); }
	ControlEvent*       back()        { return _events.back(); }
	const ControlEvent* back()  const { return _events.back(); }
	ControlEvent*       front()       { return _events.front(); }
	const ControlEvent* front() const { return _events.front(); }

	std::pair<ControlList::iterator,ControlList::iterator> control_points_adjacent (double when);

	template<class T> void apply_to_points (T& obj, void (T::*method)(const ControlList&)) {
		Glib::Mutex::Lock lm (_lock);
		(obj.*method)(*this);
	}

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
		std::pair<ControlList::const_iterator,ControlList::const_iterator> range;
	};

	/** Lookup cache for point finding, range contains points after left */
	struct SearchCache {
		SearchCache () : left(-1) {}
		double left;  /* leftmost x coordinate used when finding "first" */
		ControlList::const_iterator first;
	};

	const EventList& events() const { return _events; }
	double default_value() const { return _parameter.normal(); }

	// FIXME: const violations for Curve
	Glib::Mutex& lock()         const { return _lock; }
	LookupCache& lookup_cache() const { return _lookup_cache; }
	SearchCache& search_cache() const { return _search_cache; }

	/** Called by locked entry point and various private
	 * locations where we already hold the lock.
	 *
	 * FIXME: Should this be private?  Curve needs it..
	 */
	double unlocked_eval (double x) const;

	bool rt_safe_earliest_event (double start, double& x, double& y, bool start_inclusive=false) const;
	bool rt_safe_earliest_event_unlocked (double start, double& x, double& y, bool start_inclusive=false) const;
	bool rt_safe_earliest_event_linear_unlocked (double start, double& x, double& y, bool inclusive) const;
	bool rt_safe_earliest_event_discrete_unlocked (double start, double& x, double& y, bool inclusive) const;

	void create_curve();
	void destroy_curve();

	Curve&       curve()       { assert(_curve); return *_curve; }
	const Curve& curve() const { assert(_curve); return *_curve; }

	void mark_dirty () const;

	enum InterpolationStyle {
		Discrete,
		Linear,
		Curved
	};

	InterpolationStyle interpolation() const { return _interpolation; }
	void set_interpolation (InterpolationStyle);

	virtual bool touching() const { return false; }
	virtual bool writing() const { return false; }
	virtual bool touch_enabled() const { return false; }
	void write_pass_finished (double when);

	/** Emitted when mark_dirty() is called on this object */
	mutable PBD::Signal0<void> Dirty;
	/** Emitted when our interpolation style changes */
	PBD::Signal1<void, InterpolationStyle> InterpolationChanged;

        static void set_thinning_factor (double d);
        static double thinning_factor() { return _thinning_factor; }

protected:

	/** Called by unlocked_eval() to handle cases of 3 or more control points. */
	double multipoint_eval (double x) const;

	void build_search_cache_if_necessary (double start) const;

	boost::shared_ptr<ControlList> cut_copy_clear (double, double, int op);
	bool erase_range_internal (double start, double end, EventList &);

	virtual void maybe_signal_changed ();

	void _x_scale (double factor);

	mutable LookupCache   _lookup_cache;
	mutable SearchCache   _search_cache;

	Parameter             _parameter;
	InterpolationStyle    _interpolation;
	EventList             _events;
	mutable Glib::Mutex   _lock;
	int8_t                _frozen;
	bool                  _changed_when_thawed;
	double                _min_yval;
	double                _max_yval;
	double                _default_value;
	bool                  _sort_pending;

	Curve* _curve;

	struct NascentInfo {
		EventList events;
		double start_time;
		double end_time;

		NascentInfo (double start = -1.0)
			: start_time (start)
			, end_time (-1.0)
		{}
	};

	std::list<NascentInfo*> nascent;
        static double _thinning_factor;
};

} // namespace Evoral

#endif // EVORAL_CONTROL_LIST_HPP

