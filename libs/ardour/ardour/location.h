/*
 * Copyright (C) 2006-2017 Paul Davis <paul@linuxaudiosystems.com>
 * Copyright (C) 2006 Hans Fugal <hans@fugal.net>
 * Copyright (C) 2008-2011 David Robillard <d@drobilla.net>
 * Copyright (C) 2008 Sakari Bergen <sakari.bergen@beatwaves.net>
 * Copyright (C) 2009-2012 Carl Hetherington <carl@carlh.net>
 * Copyright (C) 2015-2016 Nick Mainsbridge <mainsbridge@gmail.com>
 * Copyright (C) 2016-2019 Robin Gareus <robin@gareus.org>
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

#ifndef __ardour_location_h__
#define __ardour_location_h__

#include <string>
#include <list>
#include <iostream>
#include <map>

#include <sys/types.h>

#include <glibmm/threads.h>

#include "pbd/undo.h"
#include "pbd/stateful.h"
#include "pbd/statefuldestructible.h"

#include "ardour/ardour.h"
#include "ardour/scene_change.h"
#include "ardour/session_handle.h"

namespace ARDOUR {

class SceneChange;

/** Location on Timeline - abstract representation for Markers, Loop/Punch Ranges, CD-Markers etc. */
class LIBARDOUR_API Location : public SessionHandleRef, public PBD::StatefulDestructible
{
public:
	enum Flags {
		IsMark = 0x1,
		IsAutoPunch = 0x2,
		IsAutoLoop = 0x4,
		IsHidden = 0x8,
		IsCDMarker = 0x10,
		IsRangeMarker = 0x20,
		IsSessionRange = 0x40,
		IsSkip = 0x80,
		IsSkipping = 0x100, /* skipping is active (or not) */
		IsClockOrigin = 0x200,
		IsXrun = 0x400,
	};

	Location (Session &);
	Location (Session &, Temporal::timepos_t const &, Temporal::timepos_t const &, const std::string &, Flags bits = Flags(0));
	Location (const Location& other);
	Location (Session &, const XMLNode&);
	Location* operator= (const Location& other);

	bool operator==(const Location& other);

	bool locked() const { return _locked; }
	void lock ();
	void unlock ();

	int64_t timestamp() const { return _timestamp; };
	timepos_t start() const { return _start; }
	timepos_t end() const { return _end; }
	timecnt_t length() const { return _start.distance (_end); }

	samplepos_t start_sample() const  { return _start.samples(); }
	samplepos_t end_sample() const { return _end.samples(); }
	samplecnt_t length_samples() const { return _end.samples() - _start.samples(); }

	int set_start (timepos_t const & s, bool force = false);
	int set_end (timepos_t const & e, bool force = false);
	int set (timepos_t const & start, timepos_t const & end);

	int move_to (timepos_t const & pos);

	const std::string& name() const { return _name; }
	void set_name (const std::string &str);

	void set_auto_punch (bool yn, void *src);
	void set_auto_loop (bool yn, void *src);
	void set_hidden (bool yn, void *src);
	void set_cd (bool yn, void *src);
	void set_is_range_marker (bool yn, void* src);
	void set_is_clock_origin (bool yn, void* src);
	void set_skip (bool yn);
	void set_skipping (bool yn);

	bool is_auto_punch () const { return _flags & IsAutoPunch; }
	bool is_auto_loop () const { return _flags & IsAutoLoop; }
	bool is_mark () const { return _flags & IsMark; }
	bool is_hidden () const { return _flags & IsHidden; }
	bool is_cd_marker () const { return _flags & IsCDMarker; }
	bool is_session_range () const { return _flags & IsSessionRange; }
	bool is_range_marker() const { return _flags & IsRangeMarker; }
	bool is_skip() const { return _flags & IsSkip; }
	bool is_clock_origin() const { return _flags & IsClockOrigin; }
	bool is_skipping() const { return (_flags & IsSkip) && (_flags & IsSkipping); }
	bool is_xrun() const { return _flags & IsXrun; }
	bool matches (Flags f) const { return _flags & f; }

	Flags flags () const { return _flags; }

	boost::shared_ptr<SceneChange> scene_change() const { return _scene_change; }
	void set_scene_change (boost::shared_ptr<SceneChange>);

	/* these are static signals for objects that want to listen to all
	 * locations at once.
	 */

	static PBD::Signal1<void,Location*> name_changed;
	static PBD::Signal1<void,Location*> end_changed;
	static PBD::Signal1<void,Location*> start_changed;
	static PBD::Signal1<void,Location*> flags_changed;
	static PBD::Signal1<void,Location*> lock_changed;
	static PBD::Signal1<void,Location*> position_time_domain_changed;

	/* this is sent only when both start and end change at the same time */
	static PBD::Signal1<void,Location*> changed;

	/* these are member signals for objects that care only about
	 * changes to this object
	 */

	PBD::Signal0<void> Changed;

	PBD::Signal0<void> NameChanged;
	PBD::Signal0<void> EndChanged;
	PBD::Signal0<void> StartChanged;
	PBD::Signal0<void> FlagsChanged;
	PBD::Signal0<void> LockChanged;
	PBD::Signal0<void> TimeDomainChanged;

	/* CD Track / CD-Text info */

	std::map<std::string, std::string> cd_info;
	XMLNode& cd_info_node (const std::string &, const std::string &);

	XMLNode& get_state (void);
	int set_state (const XMLNode&, int version);

	Temporal::TimeDomain position_time_domain() const { return _start.time_domain(); }
	void set_position_time_domain (Temporal::TimeDomain ps);

	static PBD::Signal0<void> scene_changed; /* for use by backend scene change management, class level */
	PBD::Signal0<void> SceneChangeChanged;   /* for use by objects interested in this object */

private:
	std::string        _name;
	timepos_t          _start;
	timepos_t          _end;
	Flags              _flags;
	bool               _locked;
	boost::shared_ptr<SceneChange> _scene_change;
	int64_t            _timestamp;

	void set_mark (bool yn);
	bool set_flag_internal (bool yn, Flags flag);
};

/** A collection of session locations including unique dedicated locations (loop, punch, etc) */
class LIBARDOUR_API Locations : public SessionHandleRef, public PBD::StatefulDestructible
{
public:
	typedef std::list<Location *> LocationList;

	Locations (Session &);
	~Locations ();

	const LocationList& list () const { return locations; }
	LocationList list () { return locations; }

	void add (Location *, bool make_current = false);

	/** Add new range to the collection
	 *
	 * @param start start position
	 * @param end end position
	 *
	 * @return New location object
	 */
	Location* add_range (samplepos_t start, samplepos_t end);

	void remove (Location *);
	bool clear ();
	bool clear_markers ();
	bool clear_xrun_markers ();
	bool clear_ranges ();

	void ripple (samplepos_t at, samplecnt_t distance, bool include_locked, bool notify);

	XMLNode& get_state (void);
	int set_state (const XMLNode&, int version);
	Location *get_location_by_id(PBD::ID);

	Location* auto_loop_location () const;
	Location* auto_punch_location () const;
	Location* session_range_location() const;
	Location* clock_origin_location() const;

	int next_available_name(std::string& result,std::string base);
	uint32_t num_range_markers() const;

	int set_current (Location *, bool want_lock = true);
	Location *current () const { return current_location; }

	Location* mark_at (samplepos_t, samplecnt_t slop = 0) const;

	void set_clock_origin (Location*, void *src);

	samplepos_t first_mark_before (samplepos_t, bool include_special_ranges = false);
	samplepos_t first_mark_after (samplepos_t, bool include_special_ranges = false);

	void marks_either_side (timepos_t const &, timepos_t &, timepos_t &) const;

	/** Return range with closest start pos to the where argument
	 *
	 * @param pos point to compare with start pos
	 * @param slop area around point to search for start pos
	 * @param incl (optional) look only for ranges that includes 'where' point
	 *
	 * @return Location object or nil
	 */
	Location* range_starts_at(samplepos_t, samplecnt_t slop = 0, bool incl = false) const;

	void find_all_between (timepos_t const & start, timepos_t const & end, LocationList&, Location::Flags);

	PBD::Signal1<void,Location*> current_changed;

	/* Objects that care about individual addition and removal of Locations should connect to added/removed.
	 * If an object additionally cares about potential mass clearance of Locations, they should connect to changed.
	 */

	PBD::Signal1<void,Location*> added;
	PBD::Signal1<void,Location*> removed;
	PBD::Signal0<void> changed; /* emitted when any action that could have added/removed more than 1 location actually removed 1 or more */

	template<class T> void apply (T& obj, void (T::*method)(const LocationList&)) const {
		/* We don't want to hold the lock while the given method runs, so take a copy
		 * of the list and pass that instead.
		 */
		Locations::LocationList copy;
		{
			Glib::Threads::RWLock::ReaderLock lm (_lock);
			copy = locations;
		}
		(obj.*method)(copy);
	}

private:
	LocationList locations;
	Location*    current_location;
	mutable Glib::Threads::RWLock _lock;

	int set_current_unlocked (Location *);
	void location_changed (Location*);
	void listen_to (Location*);
};

} // namespace ARDOUR

#endif /* __ardour_location_h__ */
