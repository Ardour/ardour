/* This file is part of Evoral.
 * Copyright (C) 2008 Dave Robillard <http://drobilla.net>
 * Copyright (C) 2000-2008 Paul Davis
 * 
 * Evoral is free software; you can redistribute it and/or modify it under the
 * terms of the GNU General Public License as published by the Free Software
 * Foundation; either version 2 of the License, or (at your option) any later
 * version.
 * 
 * Evoral is distributed in the hope that it will be useful, but WITHOUT ANY
 * WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE.  See the GNU General Public License for details.
 * 
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin St, Fifth Floor, Boston, MA 02110-1301 USA
 */

#include <limits>
#include <evoral/ControlSet.hpp>
#include <evoral/ControlList.hpp>
#include <evoral/Control.hpp>
#include <evoral/Event.hpp>

using namespace std;

namespace Evoral {

ControlSet::ControlSet()
{
}

void
ControlSet::add_control(boost::shared_ptr<Control> ac)
{
	_controls[ac->parameter()] = ac;
}

void
ControlSet::what_has_data (set<Parameter>& s) const
{
	Glib::Mutex::Lock lm (_control_lock);
	Controls::const_iterator li;
	
	// FIXME: correct semantics?
	for (li = _controls.begin(); li != _controls.end(); ++li) {
		s.insert  ((*li).first);
	}
}

/** If \a create_if_missing is true, a control list will be created and returned
 * if one does not already exists.  Otherwise NULL will be returned if a control list
 * for \a parameter does not exist.
 */
boost::shared_ptr<Control>
ControlSet::control (Parameter parameter, bool create_if_missing)
{
	Controls::iterator i = _controls.find(parameter);

	if (i != _controls.end()) {
		return i->second;

	} else if (create_if_missing) {
		boost::shared_ptr<ControlList> al (control_list_factory(parameter));
		boost::shared_ptr<Control> ac(control_factory(al));
		add_control(ac);
		return ac;

	} else {
		//warning << "ControlList " << parameter.to_string() << " not found for " << _name << endmsg;
		return boost::shared_ptr<Control>();
	}
}

boost::shared_ptr<const Control>
ControlSet::control (Parameter parameter) const
{
	Controls::const_iterator i = _controls.find(parameter);

	if (i != _controls.end()) {
		return i->second;
	} else {
		//warning << "ControlList " << parameter.to_string() << " not found for " << _name << endmsg;
		return boost::shared_ptr<Control>();
	}
}

bool
ControlSet::find_next_event (nframes_t now, nframes_t end, ControlEvent& next_event) const
{
	Controls::const_iterator li;	

	next_event.when = std::numeric_limits<nframes_t>::max();
	
  	for (li = _controls.begin(); li != _controls.end(); ++li) {
		ControlList::const_iterator i;
		boost::shared_ptr<const ControlList> alist (li->second->list());
		ControlEvent cp (now, 0.0f);
		
 		for (i = lower_bound (alist->begin(), alist->end(), &cp, ControlList::time_comparator);
				i != alist->end() && (*i)->when < end; ++i) {
 			if ((*i)->when > now) {
 				break; 
 			}
 		}
 		
 		if (i != alist->end() && (*i)->when < end) {
 			if ((*i)->when < next_event.when) {
 				next_event.when = (*i)->when;
 			}
 		}
 	}

 	return next_event.when != std::numeric_limits<nframes_t>::max();
}

void
ControlSet::clear ()
{
	Glib::Mutex::Lock lm (_control_lock);

	for (Controls::iterator li = _controls.begin(); li != _controls.end(); ++li)
		li->second->list()->clear();
}
	
boost::shared_ptr<Control>
ControlSet::control_factory(boost::shared_ptr<ControlList> list) const
{
	return boost::shared_ptr<Control>(new Control(list));
}

boost::shared_ptr<ControlList>
ControlSet::control_list_factory(const Parameter& param) const
{
	return boost::shared_ptr<ControlList>(new ControlList(param));
}


} // namespace Evoral
