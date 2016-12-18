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
#include <cstdlib>
#include <list>
#include <cmath>

#include <glibmm/threads.h>

#include "evoral/ControlList.hpp"
#include "evoral/Parameter.hpp"

#include "pbd/undo.h"
#include "pbd/xml++.h"
#include "pbd/statefuldestructible.h"
#include "pbd/properties.h"

#include "ardour/ardour.h"

namespace ARDOUR {

class AutomationList;

/** A SharedStatefulProperty for AutomationLists */
class LIBARDOUR_API AutomationListProperty : public PBD::SharedStatefulProperty<AutomationList>
{
public:
	AutomationListProperty (PBD::PropertyDescriptor<boost::shared_ptr<AutomationList> > d, Ptr p)
		: PBD::SharedStatefulProperty<AutomationList> (d.property_id, p)
	{}

	AutomationListProperty (PBD::PropertyDescriptor<boost::shared_ptr<AutomationList> > d, Ptr o, Ptr c)
		: PBD::SharedStatefulProperty<AutomationList> (d.property_id, o, c)
	{}

	PBD::PropertyBase* clone () const;

private:
	/* No copy-construction nor assignment */
	AutomationListProperty (AutomationListProperty const &);
	AutomationListProperty& operator= (AutomationListProperty const &);
};

/** AutomationList is a stateful wrapper around Evoral::ControlList.
 * It includes session-specifics (such as automation state), control logic (e.g. touch, signals)
 * and acts as proxy to the underlying ControlList which holds the actual data.
 */
class LIBARDOUR_API AutomationList : public PBD::StatefulDestructible, public Evoral::ControlList
{
  public:
	AutomationList (const Evoral::Parameter& id, const Evoral::ParameterDescriptor& desc);
	AutomationList (const Evoral::Parameter& id);
	AutomationList (const XMLNode&, Evoral::Parameter id);
	AutomationList (const AutomationList&);
	AutomationList (const AutomationList&, double start, double end);
	~AutomationList();

	virtual boost::shared_ptr<ControlList> create(const Evoral::Parameter&           id,
	                                              const Evoral::ParameterDescriptor& desc);

	AutomationList& operator= (const AutomationList&);

	void thaw ();

	void set_automation_state (AutoState);
	AutoState automation_state() const { return _state; }
	PBD::Signal1<void, AutoState> automation_state_changed;

	void set_automation_style (AutoStyle m);
	AutoStyle automation_style() const { return _style; }
	PBD::Signal0<void> automation_style_changed;

	bool automation_playback() const {
		return (_state & Play) || ((_state & Touch) && !touching());
	}
	bool automation_write () const {
		return ((_state & Write) || ((_state & Touch) && touching()));
	}

	PBD::Signal0<void> StateChanged;

	static PBD::Signal1<void,AutomationList*> AutomationListCreated;

	void start_write_pass (double when);
	void write_pass_finished (double when, double thinning_factor=0.0);

	void start_touch (double when);
	void stop_touch (bool mark, double when);
	bool touching() const { return g_atomic_int_get (const_cast<gint*>(&_touching)); }
	bool writing() const { return _state == Write; }
	bool touch_enabled() const { return _state == Touch; }

	XMLNode& get_state ();
	int set_state (const XMLNode &, int version);
	XMLNode& state (bool full);
	XMLNode& serialize_events ();

	Command* memento_command (XMLNode* before, XMLNode* after);

	bool operator!= (const AutomationList &) const;

	XMLNode* before () { XMLNode* rv = _before; _before = 0; return rv; }
	void clear_history ();
  private:
	void create_curve_if_necessary ();
	int deserialize_events (const XMLNode&);

	void maybe_signal_changed ();

	AutoState    _state;
	AutoStyle    _style;
	gint         _touching;

	bool operator== (const AutomationList&) const { /* not called */ abort(); return false; }
	XMLNode* _before; //used for undo of touch start/stop pairs.

};

} // namespace

#endif /* __ardour_automation_event_h__ */
