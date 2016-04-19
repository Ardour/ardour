/*
    Copyright (C) 2016 Paul Davis

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

#ifndef __ardour_mute_control_h__
#define __ardour_mute_control_h__

#include <string>

#include <boost/shared_ptr.hpp>

#include "ardour/slavable_automation_control.h"

#include "ardour/libardour_visibility.h"

namespace ARDOUR {

class Session;
class Muteable;

class LIBARDOUR_API MuteControl : public SlavableAutomationControl
{
  public:
	MuteControl (Session& session, std::string const& name, Muteable&);

	double get_value () const;

	/* Export additional API so that objects that only get access
	 * to a Controllable/AutomationControl can do more fine-grained
	 * operations with respect to mute. Obviously, they would need
	 * to dynamic_cast<MuteControl> first.
	 *
	 * Mute state is not representable by a single scalar value,
	 * so set_value() and get_value() is not enough.
	 *
	 * This means that the Controllable is technically
	 * asymmetric. It is possible to call ::set_value (0.0) to
	 * turn off mute, and then call ::get_value() and get a
	 * return of 1.0 because the control is affected by
	 * upstream/downstream or a master.
	 */

	bool muted () const;
	bool muted_by_self () const;

	bool muted_by_others_soloing () const;
	bool muted_by_others () const;

	void set_mute_points (MuteMaster::MutePoint);
	MuteMaster::MutePoint mute_points () const;

  protected:
	void master_changed (bool, PBD::Controllable::GroupControlDisposition);
	void actually_set_value (double, PBD::Controllable::GroupControlDisposition group_override);

  private:
	Muteable& _muteable;
};

} /* namespace */

#endif /* __libardour_mute_control_h__ */
