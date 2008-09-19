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

#include <set>
#include <map>
#include <boost/shared_ptr.hpp>
#include <ardour/session_object.h>
#include <ardour/automation_event.h>
#include <ardour/automation_control.h>
#include <ardour/parameter.h>
#include <evoral/ControlSet.hpp>

namespace ARDOUR {

class Session;
class AutomationControl;

class Automatable : public SessionObject, virtual public Evoral::ControlSet
{
public:
	Automatable(Session&, const std::string& name);

	virtual ~Automatable() {}

	boost::shared_ptr<Evoral::Control> control_factory(boost::shared_ptr<Evoral::ControlList> list) const;
	boost::shared_ptr<Evoral::ControlList> control_list_factory(const Evoral::Parameter& param) const;
	
	virtual void add_control(boost::shared_ptr<Evoral::Control>);
	
	virtual void automation_snapshot(nframes_t now, bool force);
	bool should_snapshot (nframes_t now) {
		return (_last_automation_snapshot > now || (now - _last_automation_snapshot) > _automation_interval);
	}
	virtual void transport_stopped(nframes_t now);

	virtual string describe_parameter(Parameter param);
	
	AutoState get_parameter_automation_state (Parameter param, bool lock = true);
	virtual void set_parameter_automation_state (Parameter param, AutoState);
	
	AutoStyle get_parameter_automation_style (Parameter param);
	void set_parameter_automation_style (Parameter param, AutoStyle);

	void protect_automation ();

	void what_has_visible_data(std::set<Parameter>&) const;
	const std::set<Parameter>& what_can_be_automated() const { return _can_automate_list; }

	void mark_automation_visible(Parameter, bool);
	
	static void set_automation_interval (jack_nframes_t frames) {
		_automation_interval = frames;
	}

	static jack_nframes_t automation_interval() { 
		return _automation_interval;
	}

protected:

	void can_automate(Parameter);

	virtual void auto_state_changed (Parameter which) {}

	int set_automation_state(const XMLNode&, Parameter default_param);
	XMLNode& get_automation_state();
	
	int load_automation (const std::string& path);
	int old_set_automation_state(const XMLNode&);

	std::set<Parameter> _visible_controls;
	std::set<Parameter> _can_automate_list;
	
	nframes_t _last_automation_snapshot;
	static nframes_t _automation_interval;
};

} // namespace ARDOUR

#endif /* __ardour_automatable_h__ */
