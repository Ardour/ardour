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

#ifndef __ardour_automation_control_h__
#define __ardour_automation_control_h__

#include <boost/shared_ptr.hpp>
#include <pbd/controllable.h>
#include <ardour/parameter.h>

namespace ARDOUR {

class AutomationList;
class Session;
class Automatable;


/** A PBD:Controllable with associated automation data (AutomationList)
 */
class AutomationControl : public PBD::Controllable
{
public:
	AutomationControl(ARDOUR::Session&,
			boost::shared_ptr<ARDOUR::AutomationList>,
			std::string name="unnamed controllable");

	void set_value(float val);
	float get_value() const;
	float user_value() const;

	void set_list(boost::shared_ptr<ARDOUR::AutomationList>);

	boost::shared_ptr<ARDOUR::AutomationList>       list()       { return _list; }
	boost::shared_ptr<const ARDOUR::AutomationList> list() const { return _list; }

	Parameter parameter() const;

protected:
	ARDOUR::Session&                          _session;
	boost::shared_ptr<ARDOUR::AutomationList> _list;
	float                                     _user_value;
};


} // namespace ARDOUR

#endif /* __ardour_automation_control_h__ */
