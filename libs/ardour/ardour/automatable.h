/*
    Copyright (C) 2000,2007 Paul Davis 

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
#include <ardour/session_object.h>
#include <ardour/automation_event.h>

namespace ARDOUR {

class Session;

class Automatable : public SessionObject
{
public:
	Automatable(Session&, const std::string& name);

	virtual ~Automatable() {}

	virtual AutomationList& automation_list(uint32_t n);

	virtual void automation_snapshot (nframes_t now) {};

	virtual bool find_next_event(nframes_t start, nframes_t end, ControlEvent& ev) const;
	
	virtual string describe_parameter(uint32_t which);
	virtual float default_parameter_value(uint32_t which) { return 1.0f; }

	void what_has_automation(std::set<uint32_t>&) const;
	void what_has_visible_automation(std::set<uint32_t>&) const;
	const std::set<uint32_t>& what_can_be_automated() const { return _can_automate_list; }

	void mark_automation_visible(uint32_t, bool);

protected:

	void can_automate(uint32_t);

	virtual void automation_list_creation_callback(uint32_t, AutomationList&) {}

	int set_automation_state(const XMLNode&);
	XMLNode& get_automation_state();
	
	int load_automation (const std::string& path);
	int old_set_automation_state(const XMLNode&);

	mutable Glib::Mutex _automation_lock;

	// FIXME:  map with int keys is a bit silly.  this could be O(1)
	std::map<uint32_t,AutomationList*> _parameter_automation;
	std::set<uint32_t> _visible_parameter_automation;
	std::set<uint32_t> _can_automate_list;
	
	nframes_t _last_automation_snapshot;
};

} // namespace ARDOUR

#endif /* __ardour_automatable_h__ */
