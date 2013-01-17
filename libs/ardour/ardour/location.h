/*
    Copyright (C) 2000 Paul Davis

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
#include "ardour/session_handle.h"

namespace ARDOUR {

class Location : public SessionHandleRef, public PBD::StatefulDestructible
{
  public:
	enum Flags {
		IsMark = 0x1,
		IsAutoPunch = 0x2,
		IsAutoLoop = 0x4,
		IsHidden = 0x8,
		IsCDMarker = 0x10,
		IsRangeMarker = 0x20,
		IsSessionRange = 0x40
	};

	Location (Session &);
	Location (Session &, framepos_t, framepos_t, const std::string &, Flags bits = Flags(0));
	Location (const Location& other);
	Location (Session &, const XMLNode&);
	Location* operator= (const Location& other);
    
        bool operator==(const Location& other);

	bool locked() const { return _locked; }
	void lock ();
	void unlock ();

	framepos_t start() const  { return _start; }
	framepos_t end() const { return _end; }
	framecnt_t length() const { return _end - _start; }

	int set_start (framepos_t s, bool force = false, bool allow_bbt_recompute = true);
	int set_end (framepos_t e, bool force = false, bool allow_bbt_recompute = true);
	int set (framepos_t start, framepos_t end, bool allow_bbt_recompute = true);

	int move_to (framepos_t pos);

	const std::string& name() const { return _name; }
	void set_name (const std::string &str) { _name = str; name_changed(this); }

	void set_auto_punch (bool yn, void *src);
	void set_auto_loop (bool yn, void *src);
	void set_hidden (bool yn, void *src);
	void set_cd (bool yn, void *src);
	void set_is_range_marker (bool yn, void* src);

	bool is_auto_punch () const { return _flags & IsAutoPunch; }
	bool is_auto_loop () const { return _flags & IsAutoLoop; }
	bool is_mark () const { return _flags & IsMark; }
	bool is_hidden () const { return _flags & IsHidden; }
	bool is_cd_marker () const { return _flags & IsCDMarker; }
	bool is_session_range () const { return _flags & IsSessionRange; }
	bool is_range_marker() const { return _flags & IsRangeMarker; }
	bool matches (Flags f) const { return _flags & f; }

	Flags flags () const { return _flags; }

	PBD::Signal1<void,Location*> name_changed;
	PBD::Signal1<void,Location*> end_changed;
	PBD::Signal1<void,Location*> start_changed;

	PBD::Signal1<void,Location*> LockChanged;
	PBD::Signal2<void,Location*,void*> FlagsChanged;
	PBD::Signal1<void,Location*> PositionLockStyleChanged;

	/* this is sent only when both start and end change at the same time */
	PBD::Signal1<void,Location*> changed;

	/* CD Track / CD-Text info */

	std::map<std::string, std::string> cd_info;
	XMLNode& cd_info_node (const std::string &, const std::string &);

	XMLNode& get_state (void);
	int set_state (const XMLNode&, int version);

	PositionLockStyle position_lock_style() const { return _position_lock_style; }
	void set_position_lock_style (PositionLockStyle ps);
	void recompute_frames_from_bbt ();

  private:
	std::string        _name;
	framepos_t         _start;
	Timecode::BBT_Time _bbt_start;
	framepos_t         _end;
	Timecode::BBT_Time _bbt_end;
	Flags              _flags;
	bool               _locked;
	PositionLockStyle  _position_lock_style;

	void set_mark (bool yn);
	bool set_flag_internal (bool yn, Flags flag);
	void recompute_bbt_from_frames ();
};

class Locations : public SessionHandleRef, public PBD::StatefulDestructible
{
  public:
	typedef std::list<Location *> LocationList;

	Locations (Session &);
	~Locations ();

	const LocationList& list() { return locations; }

	void add (Location *, bool make_current = false);
	void remove (Location *);
	void clear ();
	void clear_markers ();
	void clear_ranges ();

	XMLNode& get_state (void);
	int set_state (const XMLNode&, int version);
	Location *get_location_by_id(PBD::ID);

	Location* auto_loop_location () const;
	Location* auto_punch_location () const;
	Location* session_range_location() const;

	int next_available_name(std::string& result,std::string base);
	uint32_t num_range_markers() const;

	int set_current (Location *, bool want_lock = true);
	Location *current () const { return current_location; }

        framepos_t first_mark_before (framepos_t, bool include_special_ranges = false);
	framepos_t first_mark_after (framepos_t, bool include_special_ranges = false);

	void marks_either_side (framepos_t const, framepos_t &, framepos_t &) const;

	void find_all_between (framepos_t start, framepos_t, LocationList&, Location::Flags);

	enum Change {
		ADDITION, ///< a location was added, but nothing else changed
		REMOVAL, ///< a location was removed, but nothing else changed
		OTHER ///< something more complicated happened
	};

	PBD::Signal1<void,Location*> current_changed;
	/** something changed about the location list; the parameter gives some idea as to what */
	PBD::Signal1<void,Change>    changed;
	/** a location has been added to the end of the list */
	PBD::Signal1<void,Location*> added;
	PBD::Signal1<void,Location*> removed;
	PBD::Signal1<void,const PBD::PropertyChange&>    StateChanged;

	template<class T> void apply (T& obj, void (T::*method)(LocationList&)) {
		Glib::Threads::Mutex::Lock lm (lock);
		(obj.*method)(locations);
	}

	template<class T1, class T2> void apply (T1& obj, void (T1::*method)(LocationList&, T2& arg), T2& arg) {
		Glib::Threads::Mutex::Lock lm (lock);
		(obj.*method)(locations, arg);
	}

  private:

	LocationList         locations;
	Location            *current_location;
	mutable Glib::Threads::Mutex  lock;

	int set_current_unlocked (Location *);
	void location_changed (Location*);
};

} // namespace ARDOUR

#endif /* __ardour_location_h__ */
