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
#include "evoral/Control.hpp"
#include "evoral/ControlList.hpp"

namespace Evoral {

Parameter::TypeMetadata Parameter::_type_metadata;

Control::Control(const Parameter& parameter, boost::shared_ptr<ControlList> list)
	: _parameter(parameter)
	, _user_value(list ? list->default_value() : parameter.normal())
{
	set_list (list);
}


/** Get the currently effective value (ie the one that corresponds to current output)
 */
double
Control::get_double (bool from_list, double frame) const
{
	if (from_list) {
		return _list->eval(frame);
	} else {
		return _user_value;
	}
}


void
Control::set_double (double value, double frame, bool to_list)
{
	_user_value = value;

	if (to_list) {
		_list->add (frame, value);
	}
}


void
Control::set_list(boost::shared_ptr<ControlList> list)
{
	_list_marked_dirty_connection.disconnect ();
	
	_list = list;

	if (_list) {
		_list->Dirty.connect_same_thread (_list_marked_dirty_connection, boost::bind (&Control::list_marked_dirty, this));
	}
}

void
Control::list_marked_dirty ()
{
	ListMarkedDirty (); /* EMIT SIGNAL */
}

} // namespace Evoral

