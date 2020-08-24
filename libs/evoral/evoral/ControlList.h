/*
 * Copyright (C) 2008-2012 Carl Hetherington <carl@carlh.net>
 * Copyright (C) 2008-2014 Paul Davis <paul@linuxaudiosystems.com>
 * Copyright (C) 2008-2015 David Robillard <d@drobilla.net>
 * Copyright (C) 2012-2017 Robin Gareus <robin@gareus.org>
 * Copyright (C) 2015 Nick Mainsbridge <mainsbridge@gmail.com>
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

#ifndef EVORAL_CONTROL_LIST_HPP
#define EVORAL_CONTROL_LIST_HPP

#include <cassert>
#include <list>
#include <stdint.h>

#include <boost/pool/pool.hpp>
#include <boost/pool/pool_alloc.hpp>

#include <glibmm/threads.h>

#include "pbd/signals.h"

#include "temporal/timeline.h"
#include "temporal/types.h"
#include "temporal/range.h"

#include "evoral/visibility.h"

#include "evoral/Parameter.h"
#include "evoral/ParameterDescriptor.h"

namespace Evoral {

class Curve;
class TypeMap;

/** A single event (time-stamped value) for a control
 */
class LIBEVORAL_API ControlEvent {
public:

	ControlEvent (Temporal::timepos_t const & w, double v)
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

	Temporal::timepos_t when;
	double  value;
	double* coeff; ///< double[4] allocated by Curve as needed
};


/** A list (sequence) of time-stamped values for a control
 */
class LIBEVORAL_API ControlList
{
public:
	typedef std::list<ControlEvent*> EventList;
	typedef EventList::iterator iterator;
	typedef EventList::reverse_iterator reverse_iterator;
	typedef EventList::const_iterator const_iterator;
	typedef EventList::const_reverse_iterator const_reverse_iterator;

	ControlList (const Parameter& id, const ParameterDescriptor& desc, Temporal::TimeDomain);
	ControlList (const ControlList&, Temporal::timepos_t const & start, Temporal::timepos_t const & end);
	ControlList (const ControlList&);
	virtual ~ControlList();

	Temporal::TimeDomain time_style() const { return _time_style; }

	virtual boost::shared_ptr<ControlList> create(const Parameter& id, const ParameterDescriptor& desc, Temporal::TimeDomain);

	void dump (std::ostream&);

	ControlList& operator= (const ControlList&);
	bool operator== (const ControlList&);
	void copy_events (const ControlList&);

	virtual void freeze();
	virtual void thaw ();
	bool frozen() const { return _frozen; }

	const Parameter& parameter() const                 { return _parameter; }
	void             set_parameter(const Parameter& p) { _parameter = p; }

	const ParameterDescriptor& descriptor() const                           { return _desc; }
	void                       set_descriptor(const ParameterDescriptor& d) { _desc = d; }

	EventList::size_type size() const { return _events.size(); }

	/** @return time-stamp of first or last event in the list */
	Temporal::timepos_t when (bool at_start) const {
		Glib::Threads::RWLock::ReaderLock lm (_lock);
		if (_events.empty()) {
			return std::numeric_limits<Temporal::timepos_t>::min();
		}
		return at_start ? _events.front()->when : _events.back()->when;
	}

	Temporal::timecnt_t length() const {
		Glib::Threads::RWLock::ReaderLock lm (_lock);
		return _events.empty() ? std::numeric_limits<Temporal::timecnt_t>::min() : Temporal::timecnt_t (_events.back()->when, Temporal::timepos_t());
	}
	bool empty() const { return _events.empty(); }

	/** Remove all events from this list. */
	void clear ();
	void x_scale (Temporal::ratio_t const &);
	bool extend_to (Temporal::timepos_t const & );
	void slide (iterator before, Temporal::timecnt_t const &  distance);
	void shift (Temporal::timepos_t const & before, Temporal::timecnt_t const & distance);

	void y_transform (boost::function<double(double)> callback);
	void list_merge (ControlList const& other, boost::function<double(double, double)> callback);

	/** Add an event to this list.
	 *
	 * This method is intended to write automation in realtime. If the transport
	 * is stopped, guard-points will be added regardless of parameter with_guards.
	 *
	 * @param when absolute time in samples
	 * @param value parameter value
	 * @param with_guards if true, add guard-points
	 * @param with_initial if true, add an initial point if the list is empty
	 */
	virtual void add (Temporal::timepos_t const & when, double value, bool with_guards=true, bool with_initial=true);

	/** Add an event to this list.
	 *
	 * This method is intended for making manual changes from the GUI. An event
	 * will only be created if no other event exists at the given time.
	 *
	 * @param when absolute time in samples
	 * @param value parameter value
	 * @param with_guards if true, add guard-points
	 *
	 * @return true if an event was added.
	 */
	virtual bool editor_add (Temporal::timepos_t const & when, double value, bool with_guard);

	/* to be used only for loading pre-sorted data from saved state */
	void fast_simple_add (Temporal::timepos_t const & when, double value);

	void erase_range (Temporal::timepos_t const & start , Temporal::timepos_t const & end);
	void erase (iterator);
	void erase (iterator, iterator);
	void erase (Temporal::timepos_t const &, double);
	bool move_ranges (std::list<Temporal::RangeMove> const &);
	void modify (iterator, Temporal::timepos_t const &, double);

	/** Thin the number of events in this list.
	 *
	 * The thinning factor corresponds to the area of a triangle computed
	 * between three points in the list (time-difference * value-difference).
	 * If the area is large, it indicates significant non-linearity between
	 * the points.
	 *
	 * Time is measured in samples, value is usually normalized to 0..1.
	 *
	 * During automation recording we thin the recorded points using this
	 * value.  If a point is sufficiently co-linear with its neighbours (as
	 * defined by the area of the triangle formed by three of them), we will
	 * not include it in the list.  The larger the value, the more points are
	 * excluded, so this effectively measures the amount of thinning to be
	 * done.
	 *
	 * @param thinning_factor area-size (default: 20)
	 */
	void thin (double thinning_factor);

	boost::shared_ptr<ControlList> cut (Temporal::timepos_t const &, Temporal::timepos_t const &);
	boost::shared_ptr<ControlList> copy (Temporal::timepos_t const &, Temporal::timepos_t const &);

	/** Remove all events in the given time range from this list.
	 *
	 * @param start start of range (inclusive) in audio samples
	 * @param end end of range (inclusive) in audio samples
	 */
	void clear (Temporal::timepos_t const &, Temporal::timepos_t const &);

	bool paste (const ControlList&, Temporal::timepos_t const &);

	/** Remove all events after the given time from this list.
	 *
	 * @param last_coordinate time in audio samples of the last event to keep
	 */
	void truncate_end (Temporal::timepos_t const & last_coordinate);

	/** Remove all events up to to the given time from this list.
	 *
	 * @param overall_length overall length
	 */
	void truncate_start (Temporal::timecnt_t const & overall_length);

	iterator            begin()       { return _events.begin(); }
	const_iterator      begin() const { return _events.begin(); }
	iterator            end()         { return _events.end(); }
	const_iterator      end()   const { return _events.end(); }
	reverse_iterator            rbegin()       { return _events.rbegin(); }
	const_reverse_iterator      rbegin() const { return _events.rbegin(); }
	reverse_iterator            rend()         { return _events.rend(); }
	const_reverse_iterator      rend()   const { return _events.rend(); }
	ControlEvent*       back()        { return _events.back(); }
	const ControlEvent* back()  const { return _events.back(); }
	ControlEvent*       front()       { return _events.front(); }
	const ControlEvent* front() const { return _events.front(); }

	std::pair<ControlList::iterator,ControlList::iterator> control_points_adjacent (Temporal::timepos_t const & when);

	template<class T> void apply_to_points (T& obj, void (T::*method)(const ControlList&)) {
		Glib::Threads::RWLock::WriterLock lm (_lock);
		(obj.*method)(*this);
	}

	/** Queries the event value at the given time (takes a read-lock, not safe
	 * while writing automation).
	 *
	 * @param where absolute time in samples
	 * @returns parameter value
	 */
	double eval (Temporal::timepos_t const & where) const {
		Glib::Threads::RWLock::ReaderLock lm (_lock);
		return unlocked_eval (where);
	}

	/** Realtime safe version of eval(). This may fail if a read-lock cannot
	 * be taken.
	 *
	 * @param where absolute time in samples
	 * @param ok boolean reference if returned value is valid
	 * @returns parameter value
	 */
	double rt_safe_eval (Temporal::timepos_t const & where, bool& ok) const {

		Glib::Threads::RWLock::ReaderLock lm (_lock, Glib::Threads::TRY_LOCK);

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
		LookupCache() : left (std::numeric_limits<Temporal::timepos_t>::max()) {}
		Temporal::timepos_t left;  /* leftmost x coordinate used when finding "range" */
		std::pair<ControlList::const_iterator,ControlList::const_iterator> range;
	};

	/** Lookup cache for point finding, range contains points after left */
	struct SearchCache {
		SearchCache () : left (std::numeric_limits<Temporal::timepos_t>::max()) {}
		Temporal::timepos_t left;  /* leftmost x coordinate used when finding "first" */
		ControlList::const_iterator first;
	};

	/** @return the list of events */
	const EventList& events() const { return _events; }

	// FIXME: const violations for Curve
	Glib::Threads::RWLock& lock()       const { return _lock; }
	LookupCache& lookup_cache() const { return _lookup_cache; }
	SearchCache& search_cache() const { return _search_cache; }

	/** Called by locked entry point and various private
	 * locations where we already hold the lock.
	 *
	 * FIXME: Should this be private?  Curve needs it..
	 */
	double unlocked_eval (Temporal::timepos_t const & x) const;

	bool rt_safe_earliest_event_discrete_unlocked (Temporal::timepos_t const & start, Temporal::timepos_t & x, double& y, bool inclusive) const;
	bool rt_safe_earliest_event_linear_unlocked (Temporal::timepos_t const & start, Temporal::timepos_t & x, double& y, bool inclusive, Temporal::timepos_t min_x_delta = Temporal::timepos_t ()) const;

	void create_curve();
	void destroy_curve();

	Curve&       curve()       { assert(_curve); return *_curve; }
	const Curve& curve() const { assert(_curve); return *_curve; }

	void mark_dirty () const;

	enum InterpolationStyle {
		Discrete,
		Linear,
		Curved, // spline, used for x-fades
		Logarithmic,
		Exponential // fader, gain
	};

	/** query interpolation style of the automation data
	 * @returns Interpolation Style
	 */
	InterpolationStyle interpolation() const { return _interpolation; }

	/** query default interpolation for parameter-descriptor */
	virtual InterpolationStyle default_interpolation() const;

	/** Sets the interpolation style of the automation data.
	 *
	 * This will fail when asking for Logarithmic scale and min,max crosses 0
	 * or Exponential scale with min != 0.
	 *
	 * @param is interpolation style
	 * @returns true if style change was successful
	 */
	bool set_interpolation (InterpolationStyle is);

	virtual bool touching() const { return false; }
	virtual bool writing() const { return false; }
	virtual bool touch_enabled() const { return false; }
	void start_write_pass (Temporal::timepos_t const &);
	void write_pass_finished (Temporal::timepos_t const &, double thinning_factor=0.0);
	void set_in_write_pass (bool, bool add_point = false, Temporal::timepos_t = std::numeric_limits<Temporal::timepos_t>::min());
	/** @return true if transport is running and this list is in write mode */
	bool in_write_pass () const;
	bool in_new_write_pass () { return new_write_pass; }

	PBD::Signal0<void> WritePassStarted;
	/** Emitted when mark_dirty() is called on this object */
	mutable PBD::Signal0<void> Dirty;
	/** Emitted when our interpolation style changes */
	PBD::Signal1<void, InterpolationStyle> InterpolationChanged;

	bool operator!= (ControlList const &) const;

	void invalidate_insert_iterator ();

  protected:

	/** Called by unlocked_eval() to handle cases of 3 or more control points. */
	double multipoint_eval (Temporal::timepos_t const & x) const;

	void build_search_cache_if_necessary (Temporal::timepos_t const & start) const;

	boost::shared_ptr<ControlList> cut_copy_clear (Temporal::timepos_t const &, Temporal::timepos_t const &, int op);
	bool erase_range_internal (Temporal::timepos_t const & start, Temporal::timepos_t const & end, EventList &);

	void     maybe_add_insert_guard (Temporal::timepos_t const & when);
	iterator erase_from_iterator_to (iterator iter, Temporal::timepos_t const & when);
	bool     maybe_insert_straight_line (Temporal::timepos_t const & when, double value);

	virtual void maybe_signal_changed ();

	void _x_scale (Temporal::ratio_t const &);

	mutable LookupCache   _lookup_cache;
	mutable SearchCache   _search_cache;

	mutable Glib::Threads::RWLock _lock;

	Parameter             _parameter;
	ParameterDescriptor   _desc;
	InterpolationStyle    _interpolation;
	EventList             _events;
	int8_t                _frozen;
	bool                  _changed_when_thawed;
	bool                  _sort_pending;
	Temporal::TimeDomain   _time_style;

	Curve* _curve;

  private:
	iterator   most_recent_insert_iterator;
	Temporal::timepos_t insert_position;
	bool       new_write_pass;
	bool       did_write_during_pass;
	bool       _in_write_pass;

	void unlocked_remove_duplicates ();
	void unlocked_invalidate_insert_iterator ();
	void add_guard_point (Temporal::timepos_t const & when, Temporal::timecnt_t const & offset);

	bool is_sorted () const;

};

} // namespace Evoral

#endif // EVORAL_CONTROL_LIST_HPP
