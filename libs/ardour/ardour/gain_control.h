/*
 * Copyright (C) 2016-2017 Paul Davis <paul@linuxaudiosystems.com>
 * Copyright (C) 2017 Robin Gareus <robin@gareus.org>
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

#pragma once

#include <memory>
#include <string>

#include "pbd/controllable.h"

#include "evoral/Parameter.h"

#include "ardour/slavable_automation_control.h"
#include "ardour/libardour_visibility.h"

namespace ARDOUR {

class Session;

class LIBARDOUR_API GainControl : public SlavableAutomationControl {
  public:
	GainControl (Session& session, const Evoral::Parameter &param,
	             std::shared_ptr<AutomationList> al = std::shared_ptr<AutomationList>());

	void inc_gain (gain_t);

protected:
	void post_add_master (std::shared_ptr<AutomationControl>);
	bool get_masters_curve_locked (samplepos_t, samplepos_t, float*, samplecnt_t) const;
	void actually_set_value (double value, PBD::Controllable::GroupControlDisposition);
};

} /* namespace */

