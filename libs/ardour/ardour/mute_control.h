/*
 * Copyright (C) 2016-2017 Paul Davis <paul@linuxaudiosystems.com>
 * Copyright (C) 2016-2017 Robin Gareus <robin@gareus.org>
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

#ifndef __ardour_mute_control_h__
#define __ardour_mute_control_h__

#include <string>

#include <boost/shared_ptr.hpp>

#include "ardour/slavable_automation_control.h"

#include "ardour/mute_master.h"
#include "ardour/libardour_visibility.h"

namespace ARDOUR {

class Session;
class Muteable;

class LIBARDOUR_API MuteControl : public SlavableAutomationControl
{
public:
	MuteControl (Session& session, std::string const& name, Muteable&, Temporal::TimeDomain td);

	double get_value () const;
	double get_save_value() const { return muted_by_self(); }

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
	bool muted_by_masters () const;
	bool muted_by_self_or_masters () const {
		return muted_by_self() || muted_by_masters ();
	}

	bool muted_by_others_soloing () const;

	void set_mute_points (MuteMaster::MutePoint);
	MuteMaster::MutePoint mute_points () const;

	void automation_run (samplepos_t start, pframes_t nframes);

protected:
	bool handle_master_change (boost::shared_ptr<AutomationControl>);
	void actually_set_value (double, PBD::Controllable::GroupControlDisposition group_override);

	void pre_remove_master (boost::shared_ptr<AutomationControl>);
	void post_add_master (boost::shared_ptr<AutomationControl>);

private:
	Muteable& _muteable;
};

} /* namespace */

#endif /* __libardour_mute_control_h__ */
