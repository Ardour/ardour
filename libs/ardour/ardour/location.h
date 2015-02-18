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
#include "ardour/scene_change.h"
#include "ardour/session_handle.h"

namespace ARDOUR {

class SceneChange;

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
	void set_name (const std::string &str);

	void set_auto_punch (bool yn, void *src);
	void set_auto_loop (bool yn, void *src);
	void set_hidden (bool yn, void *src);
	void set_cd (bool yn, void *src);
	void set_is_range_marker (bool yn, void* src);
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
	bool is_skipping() const { return (_flags & IsSkip) && (_flags & IsSkipping); }
	bool matches (Flags f) const { return _flags & f; }

	Flags flags () const { return _flags; }

	boost::shared_ptr<SceneChange> scene_change() const { return _scene_change; }
	void set_scene_change (boost::shared_ptr<SceneChange>);

        /* these are static signals for objects that want to listen to all
           locations at once.
        */

	static PBD::Signal1<void,Location*> name_changed;
	static PBD::Signal1<void,Location*> end_changed;
	static PBD::Signal1<void,Location*> start_changed;
	static PBD::Signal1<void,Location*> flags_changed;
        static PBD::Signal1<void,Location*> lock_changed;
	static PBD::Signal1<void,Location*> position_lock_style_changed;

	/* this is sent only when both start and end change at the same time */
	static PBD::Signal1<void,Location*> changed;

        /* these are member signals for objects that care only about
           changes to this object 
        */

	PBD::Signal0<void> NameChanged;
	PBD::Signal0<void> EndChanged;
	PBD::Signal0<void> StartChanged;
	PBD::Signal0<void> Changed;
	PBD::Signal0<void> FlagsChanged;
	PBD::Signal0<void> LockChanged;
	PBD::Signal0<void> PositionLockStyleChanged;
        
	/* CD Track / CD-Text info */

	std::map<std::string, std::string> cd_info;
	XMLNode& cd_info_node (const std::string &, const std::string &);

	XMLNode& get_state (void);
	int set_state (const XMLNode&, int version);

	PositionLockStyle position_lock_style() const { return _position_lock_style; }
	void set_position_lock_style (PositionLockStyle ps);
	void recompute_frames_from_bbt ();

	static PBD::Signal0<void> scene_changed;

  private:
	std::string        _name;
	framepos_t         _start;
	Timecode::BBT_Time _bbt_start;
	framepos_t         _end;
	Timecode::BBT_Time _bbt_end;
	Flags              _flags;
	bool               _locked;
	PositionLockStyle  _position_lock_style;
	boost::shared_ptr<SceneChange> _scene_change;

	void set_mark (bool yn);
	bool set_flag_internal (bool yn, Flags flag);
	void recompute_bbt_from_frames ();
};

class LIBARDOUR_API Locations : public SessionHandleRef, public PBD::StatefulDestructible
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

	Location* mark_at (framepos_t, framecnt_t slop = 0) const;

        framepos_t first_mark_before (framepos_t, bool include_special_ranges = false);
	framepos_t first_mark_after (framepos_t, bool include_special_ranges = false);

	void marks_either_side (framepos_t const, framepos_t &, framepos_t &) const;

	void find_all_between (framepos_t start, framepos_t, LocationList&, Location::Flags);

	PBD::Signal1<void,Location*> current_changed;

        /* Objects that care about individual addition and removal of Locations should connect to added/removed.
           If an object additionally cares about potential mass clearance of Locations, they should connect to changed.
        */

	PBD::Signal1<void,Location*> added;
	PBD::Signal1<void,Location*> removed;
	PBD::Signal0<void> changed; /* emitted when any action that could have added/removed more than 1 location actually removed 1 or more */

	template<class T> void apply (T& obj, void (T::*method)(const LocationList&)) const {
                /* We don't want to hold the lock while the given method runs, so take a copy
                   of the list and pass that instead.
                */
                Locations::LocationList copy;
                {
                        Glib::Threads::Mutex::Lock lm (lock);
                        copy = locations;
                }
		(obj.*method)(copy);
	}

  private:

	LocationList         locations;
	Location            *current_location;
	mutable Glib::Threads::Mutex  lock;

	int set_current_unlocked (Location *);
	void location_changed (Location*);
	void listen_to (Location*);
};

} // namespace ARDOUR

#endif /* __ardour_location_h__ */
