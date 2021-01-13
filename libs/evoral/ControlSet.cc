/*
 * Copyright (C) 2008-2014 David Robillard <d@drobilla.net>
 * Copyright (C) 2010-2018 Paul Davis <paul@linuxaudiosystems.com>
 * Copyright (C) 2010 Carl Hetherington <carl@carlh.net>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#include <iostream>
#include <limits>
#include "evoral/ControlSet.h"
#include "evoral/ControlList.h"
#include "evoral/Control.h"
#include "evoral/Event.h"

using namespace std;

namespace Evoral {


ControlSet::ControlSet ()
{
}

ControlSet::ControlSet (ControlSet const & other)
	: noncopyable ()
{
	/* derived class must copy controls */
}

void
ControlSet::add_control(boost::shared_ptr<Control> ac)
{
	_controls[ac->parameter()] = ac;

	ac->ListMarkedDirty.connect_same_thread (_control_connections, boost::bind (&ControlSet::control_list_marked_dirty, this));

	if (ac->list()) {
		ac->list()->InterpolationChanged.connect_same_thread (
			_list_connections,
			boost::bind (&ControlSet::control_list_interpolation_changed,
			             this, ac->parameter(), _1));
	}
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

void
ControlSet::clear_controls ()
{
	Glib::Threads::Mutex::Lock lm (_control_lock);

	_control_connections.drop_connections ();
	_list_connections.drop_connections ();

	for (Controls::iterator li = _controls.begin(); li != _controls.end(); ++li) {
		if (li->second->list()) {
			li->second->list()->clear();
		}
	}
}

} // namespace Evoral

/* No good place for this so just put it here */

std::ostream&
std::operator<< (std::ostream & str, Evoral::Parameter const & p)
{
	return str << p.type() << '-' << p.id() << '-' << (int) p.channel();
}
