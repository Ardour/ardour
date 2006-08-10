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

    $Id$
*/

#ifndef __ardour_location_h__
#define __ardour_location_h__

#include <string>
#include <list>
#include <iostream>
#include <map>

#include <sys/types.h>
#include <sigc++/signal.h>

#include <glibmm/thread.h>

#include <pbd/undo.h>
#include <pbd/stateful.h> 

#include <ardour/ardour.h>
#include <ardour/state_manager.h>

using std::string;

namespace ARDOUR {

class Location : public Stateful, public sigc::trackable
{
  public:
	enum Flags {
		IsMark = 0x1,
		IsAutoPunch = 0x2,
		IsAutoLoop = 0x4,
		IsHidden = 0x8,
		IsCDMarker = 0x10,
		IsEnd = 0x20,
		IsRangeMarker = 0x40,
		IsStart = 0x80
	};

	Location (jack_nframes_t sample_start,
		  jack_nframes_t sample_end,
		  const string &name,
		  Flags bits = Flags(0))		
		
		: _name (name),
		_start (sample_start),
		_end (sample_end),
		_flags (bits) { }
	
	Location () {
		_start = 0;
		_end = 0;
		_flags = 0;	
	}

	Location (const Location& other);
	Location* operator= (const Location& other);

	jack_nframes_t start() { return _start; }
	jack_nframes_t end() { return _end; }
	jack_nframes_t length() { return _end - _start; }

	int set_start (jack_nframes_t s);
	int set_end (jack_nframes_t e);
	int set (jack_nframes_t start, jack_nframes_t end);

	const string& name() { return _name; }
	void set_name (const string &str) { _name = str; name_changed(this); }

	void set_auto_punch (bool yn, void *src);
	void set_auto_loop (bool yn, void *src);
	void set_hidden (bool yn, void *src);
	void set_cd (bool yn, void *src);
	void set_is_end (bool yn, void* src);
	void set_is_start (bool yn, void* src);

	bool is_auto_punch ()  { return _flags & IsAutoPunch; }
	bool is_auto_loop () { return _flags & IsAutoLoop; }
	bool is_mark () { return _flags & IsMark; }
	bool is_hidden () { return _flags & IsHidden; }
	bool is_cd_marker () { return _flags & IsCDMarker; }
	bool is_end() { return _flags & IsEnd; }
	bool is_start() { return _flags & IsStart; }
	bool is_range_marker() { return _flags & IsRangeMarker; }

	sigc::signal<void,Location*> name_changed;
	sigc::signal<void,Location*> end_changed;
	sigc::signal<void,Location*> start_changed;

	sigc::signal<void,Location*,void*> FlagsChanged;

	/* this is sent only when both start&end change at the same time */

	sigc::signal<void,Location*> changed;
   
	/* CD Track / CD-Text info */

	std::map<string, string> cd_info;
	XMLNode& cd_info_node (const string &, const string &);

	XMLNode& get_state (void);
	int set_state (const XMLNode&);

        PBD::ID id() { return _id; }

  private:
        PBD::ID _id;
	string        _name;
	jack_nframes_t     _start;
	jack_nframes_t     _end;
	uint32_t _flags;

	void set_mark (bool yn);
	bool set_flag_internal (bool yn, Flags flag);
};

class Locations : public Stateful, public StateManager
{
  public:
	typedef std::list<Location *> LocationList;

	Locations ();
	~Locations ();

	void add (Location *, bool make_current = false);
	void remove (Location *);
	void clear ();
	void clear_markers ();
	void clear_ranges ();

	XMLNode& get_state (void);
	int set_state (const XMLNode&);
        PBD::ID id() { return _id; }

	Location* auto_loop_location () const;
	Location* auto_punch_location () const;
	Location* end_location() const;
	Location* start_location() const;

	uint32_t num_range_markers() const;

	int set_current (Location *, bool want_lock = true);
	Location *current () const { return current_location; }

	Location *first_location_before (jack_nframes_t);
	Location *first_location_after (jack_nframes_t);

	jack_nframes_t first_mark_before (jack_nframes_t);
	jack_nframes_t first_mark_after (jack_nframes_t);

	sigc::signal<void,Location*> current_changed;
	sigc::signal<void>           changed;
	sigc::signal<void,Location*> added;
	sigc::signal<void,Location*> removed;

	template<class T> void apply (T& obj, void (T::*method)(LocationList&)) {
		Glib::Mutex::Lock lm (lock);
		(obj.*method)(locations);
	}

	template<class T1, class T2> void apply (T1& obj, void (T1::*method)(LocationList&, T2& arg), T2& arg) {
		Glib::Mutex::Lock lm (lock);
		(obj.*method)(locations, arg);
	}

	UndoAction get_memento () const;

  private:

	struct State : public ARDOUR::StateManager::State {
	    LocationList locations;
	    LocationList states;

	    State (std::string why) : ARDOUR::StateManager::State (why) {}
	};

	LocationList       locations;
	Location          *current_location;
	mutable Glib::Mutex  lock;

	int set_current_unlocked (Location *);
	void location_changed (Location*);

	Change   restore_state (StateManager::State&);
	StateManager::State* state_factory (std::string why) const;

        PBD::ID _id;
};

} // namespace ARDOUR

#endif /* __ardour_location_h__ */
