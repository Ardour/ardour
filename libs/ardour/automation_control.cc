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
#include <ardour/midi_track.h>

using namespace std;
using namespace ARDOUR;
using namespace PBD;


AutomationControl::AutomationControl(
		ARDOUR::Session& session,
		const Evoral::Parameter& parameter,
		boost::shared_ptr<ARDOUR::AutomationList> list,
		const string& name)
	: Controllable((name != "") ? name : EventTypeMap::instance().to_symbol(parameter))
	, Evoral::Control(parameter, list)
	, _session(session)
{
}


float
AutomationControl::get_value() const
{
	bool from_list = _list && ((AutomationList*)_list.get())->automation_playback();
	return Control::get_float(from_list, _session.transport_frame());
}


void
AutomationControl::set_value(float value)
{
	bool to_list = _list && _session.transport_stopped()
		&& ((AutomationList*)_list.get())->automation_playback();
	
	Control::set_float(value, to_list, _session.transport_frame());

	Changed(); /* EMIT SIGNAL */
}


void
AutomationControl::set_list(boost::shared_ptr<Evoral::ControlList> list)
{
	Control::set_list(list);
	Changed();  /* EMIT SIGNAL */
}

