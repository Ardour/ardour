/*
    Copyright (C) 2011 Paul Davis

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

#ifndef __libardour_pan_controllable_h__
#define __libardour_pan_controllable_h__

#include <string>

#include <boost/shared_ptr.hpp>

#include "evoral/Parameter.hpp"

#include "ardour/automation_control.h"
#include "ardour/automation_list.h"

namespace ARDOUR {

class Session;
class Pannable;

class LIBARDOUR_API PanControllable : public AutomationControl
{
public:
	PanControllable (Session& s, std::string name, Pannable* o, Evoral::Parameter param)
		: AutomationControl (s, param, boost::shared_ptr<AutomationList>(new AutomationList(param)), name)
		, owner (o)
	{}

	double lower () const;
	void set_value (double);

private:
	Pannable* owner;
};

} // namespace

#endif /* __libardour_pan_controllable_h__ */
