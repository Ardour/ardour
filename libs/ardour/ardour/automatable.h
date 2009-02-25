/*
    Copyright (C) 2000-2007 Paul Davis 

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

#ifndef __ardour_automatable_h__
#define __ardour_automatable_h__

#include <map>
#include <set>
#include <string>
#include <boost/shared_ptr.hpp>
#include "evoral/ControlSet.hpp"
#include "ardour/types.h"

class XMLNode;

namespace ARDOUR {

class Session;
class AutomationControl;


/** Note this class is abstract, actual objects must either be
 * an AutomatableControls or an AutomatableSequence
 */
class Automatable : virtual public Evoral::ControlSet
{
public:
	Automatable(Session&);
	Automatable();

	virtual ~Automatable() {}

	boost::shared_ptr<Evoral::Control>
	control_factory(const Evoral::Parameter& id);

	boost::shared_ptr<AutomationControl>
	automation_control (const Evoral::Parameter& id, bool create_if_missing=false);
	
	boost::shared_ptr<const AutomationControl>
	automation_control (const Evoral::Parameter& id) const;

	virtual void add_control(boost::shared_ptr<Evoral::Control>);
	
	virtual void automation_snapshot(nframes_t now, bool force);
	virtual void transport_stopped(nframes_t now);

	virtual std::string describe_parameter(Evoral::Parameter param);
	
	AutoState get_parameter_automation_state (Evoral::Parameter param, bool lock = true);
	virtual void set_parameter_automation_state (Evoral::Parameter param, AutoState);
	
	AutoStyle get_parameter_automation_style (Evoral::Parameter param);
	void set_parameter_automation_style (Evoral::Parameter param, AutoStyle);

	void protect_automation ();

	void what_has_visible_data(std::set<Evoral::Parameter>&) const;
	const std::set<Evoral::Parameter>& what_can_be_automated() const { return _can_automate_list; }

	void mark_automation_visible(Evoral::Parameter, bool);
	
	inline bool should_snapshot (nframes_t now) {
		return (_last_automation_snapshot > now
				|| (now - _last_automation_snapshot) > _automation_interval);
	}
	
	static void set_automation_interval (jack_nframes_t frames) {
		_automation_interval = frames;
	}

	static jack_nframes_t automation_interval() { 
		return _automation_interval;
	}
	
	typedef Evoral::ControlSet::Controls Controls;
	
	Evoral::ControlSet&       data()       { return *this; }
	const Evoral::ControlSet& data() const { return *this; }

	int set_automation_state (const XMLNode&, Evoral::Parameter default_param);
	XMLNode& get_automation_state();

  protected:
	Session& _a_session;

	void can_automate(Evoral::Parameter);

	virtual void auto_state_changed (Evoral::Parameter which) {}

	
	int load_automation (const std::string& path);
	int old_set_automation_state(const XMLNode&);

	std::set<Evoral::Parameter> _visible_controls;
	std::set<Evoral::Parameter> _can_automate_list;
	
	nframes_t        _last_automation_snapshot;
	static nframes_t _automation_interval;
};


} // namespace ARDOUR

#endif /* __ardour_automatable_h__ */
