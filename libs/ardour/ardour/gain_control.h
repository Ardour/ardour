/*
    Copyright (C) 2006-2016 Paul Davis

    This program is free software; you can redistribute it and/or modify it
    under the terms of the GNU General Public License as published by the Free
    Software Foundation; either version 2 of the License, or (at your option)
    any later version.

    This program is distributed in the hope that it will be useful, but WITHOUT
    ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
    FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
    for more details.

    You should have received a copy of the GNU General Public License along
    with this program; if not, write to the Free Software Foundation, Inc.,
    675 Mass Ave, Cambridge, MA 02139, USA.
*/

#ifndef __ardour_gain_control_h__
#define __ardour_gain_control_h__

#include <string>
#include <boost/shared_ptr.hpp>

#include "pbd/controllable.h"

#include "evoral/Parameter.hpp"

#include "ardour/automation_control.h"
#include "ardour/libardour_visibility.h"

namespace ARDOUR {

class Session;

class LIBARDOUR_API GainControl : public AutomationControl {
  public:
	GainControl (Session& session, const Evoral::Parameter &param,
	             boost::shared_ptr<AutomationList> al = boost::shared_ptr<AutomationList>());

	void set_value (double val, PBD::Controllable::GroupControlDisposition group_override);
	void set_value_unchecked (double);
	double get_value () const;

	double internal_to_interface (double) const;
	double interface_to_internal (double) const;
	double internal_to_user (double) const;
	double user_to_internal (double) const;
	std::string get_user_string () const;

	double lower_db;
	double range_db;

	boost::shared_ptr<GainControl> master() const { return _master; }
	void set_master (boost::shared_ptr<GainControl>);

  private:
	void _set_value (double val, PBD::Controllable::GroupControlDisposition group_override);
	boost::shared_ptr<GainControl> _master;
};

} /* namespace */

#endif /* __ardour_gain_control_h__ */
