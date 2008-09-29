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

#include <evoral/ControlList.hpp>

namespace ARDOUR {

class AutomationList : public PBD::StatefulDestructible, public Evoral::ControlList
{
  public:
	AutomationList (Evoral::Parameter id);
	AutomationList (const XMLNode&, Evoral::Parameter id);
	~AutomationList();

	virtual boost::shared_ptr<Evoral::ControlList> create(Evoral::Parameter id);

	AutomationList (const AutomationList&);
	AutomationList (const AutomationList&, double start, double end);
	AutomationList& operator= (const AutomationList&);
	bool operator== (const AutomationList&);
	
	void freeze();
	void thaw ();
	void mark_dirty () const;

	void set_automation_state (AutoState);
	AutoState automation_state() const { return _state; }
	sigc::signal<void> automation_state_changed;

	void set_automation_style (AutoStyle m);
	AutoStyle automation_style() const { return _style; }
	sigc::signal<void> automation_style_changed;

	bool automation_playback() const {
		return (_state & Play) || ((_state & Touch) && !_touching);
	}
	bool automation_write () const {
		return (_state & Write) || ((_state & Touch) && _touching);
	}
	
	sigc::signal<void> StateChanged;
	
	static sigc::signal<void, AutomationList*> AutomationListCreated;
	mutable sigc::signal<void> Dirty;

	void start_touch ();
	void stop_touch ();
	bool touching() const { return _touching; }

	XMLNode& get_state(void); 
	int set_state (const XMLNode &s);
	XMLNode& state (bool full);
	XMLNode& serialize_events ();

  private:
	int deserialize_events (const XMLNode&);
	
	void maybe_signal_changed ();
	
	AutoState _state;
	AutoStyle _style;
	bool      _touching;
};

} // namespace

#endif /* __ardour_automation_event_h__ */
