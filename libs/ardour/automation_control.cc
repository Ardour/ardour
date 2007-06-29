/*
    Copyright (C) 2007 Paul Davis 
	Author: Dave Robillard

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

#include <iostream>
#include <ardour/automation_control.h>
#include <ardour/session.h>
#include <ardour/automatable.h>

using namespace std;
using namespace ARDOUR;
using namespace PBD;


AutomationControl::AutomationControl(Session& session, boost::shared_ptr<AutomationList> list, string name)
	: Controllable((name == "unnamed controllable") ? list->param_id().to_string() : name)
	, _session(session)
	, _list(list)
	, _user_value(list->default_value())
{
	cerr << "Created AutomationControl " << name << "(" << list->param_id().to_string() << ")" << endl;
}


/** Get the currently effective value (ie the one that corresponds to current output)
 */
float
AutomationControl::get_value() const
{
	if (_list->automation_playback())
		return _list->eval(_session.transport_frame());
	else
		return _user_value;
}


void
AutomationControl::set_value(float value)
{
	_user_value = value;
	
	if (_session.transport_stopped() && _list->automation_write())
		_list->add(_session.transport_frame(), value);

	Changed(); /* EMIT SIGNAL */
}


/** Get the latest user-set value, which may not equal get_value() when automation
 * is playing back, etc.
 *
 * Automation write/touch works by periodically sampling this value and adding it
 * to the AutomationList.
 */
float
AutomationControl::user_value() const
{
	return _user_value;
}
	

void
AutomationControl::set_list(boost::shared_ptr<ARDOUR::AutomationList> list)
{
	_list = list;
	_user_value = list->default_value();
	Changed();  /* EMIT SIGNAL */
}

