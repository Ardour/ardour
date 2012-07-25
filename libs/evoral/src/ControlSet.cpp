/* This file is part of Evoral.
 * Copyright (C) 2008 David Robillard <http://drobilla.net>
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

#include <iostream>
#include <limits>
#include "evoral/ControlSet.hpp"
#include "evoral/ControlList.hpp"
#include "evoral/Control.hpp"
#include "evoral/Event.hpp"

using namespace std;

namespace Evoral {


ControlSet::ControlSet()
{
}

ControlSet::ControlSet (const ControlSet&)
	: noncopyable ()
{
	/* derived class must copy controls */
}

void
ControlSet::add_control(boost::shared_ptr<Control> ac)
{
	_controls[ac->parameter()] = ac;

	ac->ListMarkedDirty.connect_same_thread (_control_connections, boost::bind (&ControlSet::control_list_marked_dirty, this));

	ac->list()->InterpolationChanged.connect_same_thread (
		_list_connections, boost::bind (&ControlSet::control_list_interpolation_changed, this, ac->parameter(), _1)
	                                                      );
}

void
ControlSet::what_has_data (set<Parameter>& s) const
{
	Glib::Threads::Mutex::Lock lm (_control_lock);

	for (Controls::const_iterator li = _controls.begin(); li != _controls.end(); ++li) {
		if (li->second->list() && !li->second->list()->empty()) {
			s.insert (li->first);
		}
	}
}

/** If a control for the given parameter does not exist and \a create_if_missing is true,
 * a control will be created, added to this set, and returned.
 * If \a create_if_missing is false this function may return null.
 */
boost::shared_ptr<Control>
ControlSet::control (const Parameter& parameter, bool create_if_missing)
{
	Controls::iterator i = _controls.find(parameter);

	if (i != _controls.end()) {
		return i->second;

	} else if (create_if_missing) {
		boost::shared_ptr<Control> ac(control_factory(parameter));
		add_control(ac);
		return ac;

	} else {
		return boost::shared_ptr<Control>();
	}
}

bool
ControlSet::find_next_event (double now, double end, ControlEvent& next_event) const
{
	Controls::const_iterator li;

	next_event.when = std::numeric_limits<double>::max();

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

	return next_event.when != std::numeric_limits<double>::max();
}

void
ControlSet::clear_controls ()
{
	Glib::Threads::Mutex::Lock lm (_control_lock);

	_control_connections.drop_connections ();
	_list_connections.drop_connections ();

	for (Controls::iterator li = _controls.begin(); li != _controls.end(); ++li)
		li->second->list()->clear();
}

} // namespace Evoral
